"""Durable Greenfield task creation, execution, diagnostics, and reporting."""

from __future__ import annotations

import fcntl
import json
import math
import os
import re
import shutil
import signal
import stat
import subprocess
import sys
import tempfile
import time
from contextlib import contextmanager
from datetime import datetime, timezone
from pathlib import Path
from typing import BinaryIO, Iterator, Sequence, TextIO

from .models import (
    TERMINAL_STATES,
    TaskMetadata,
    TaskState,
    atomic_write_bytes,
    atomic_write_json,
    atomic_write_text,
    read_json,
    read_text_file,
    transition_state,
)
from .paths import RunnerConfig, TaskPaths
from .process import CommandResult, TimedProcessResult, run_command, run_timed_process


TASK_ID_PATTERN = re.compile(r"^[A-Za-z0-9][A-Za-z0-9._-]*$")
DEFAULT_TIMEOUT_SECONDS = 14400
DEFAULT_REASONING_EFFORT = "high"


class RunnerError(RuntimeError):
    """A user-facing task runner error."""


class LockContentionError(RunnerError):
    """Another task currently owns the global runner lock."""


class RunnerInterrupted(RunnerError):
    """A catchable request to stop the runner and its active child process group."""

    def __init__(self, signal_number: int):
        signal_name = signal.Signals(signal_number).name
        super().__init__(f"interrupted by {signal_name}")
        self.exit_code = 128 + signal_number


def validate_task_id(task_id: str) -> str:
    """Reject task identifiers that are unsafe as directory and ref components."""

    if (
        not TASK_ID_PATTERN.fullmatch(task_id)
        or ".." in task_id
        or task_id.endswith(".")
        or task_id.endswith(".lock")
    ):
        raise RunnerError(
            "invalid task ID; expected a safe directory name and Git branch component"
        )
    return task_id


class GreenfieldRunner:
    """Coordinates one durable task at a time."""

    def __init__(self, config: RunnerConfig):
        self.config = config

    def create_task(
        self,
        task_id: str,
        expected_ref: str,
        prompt_file: Path,
        validation_file: Path | None = None,
    ) -> TaskMetadata:
        validate_task_id(task_id)
        normalized_ref = expected_ref.strip()
        if not normalized_ref:
            raise RunnerError("expected ref must not be empty")
        self._require_input_file(prompt_file, "prompt")
        if validation_file is not None:
            self._require_input_file(validation_file, "validation")

        paths = self.config.task_paths(task_id)
        self.config.runs.mkdir(parents=True, exist_ok=True)
        self.config.worktrees.mkdir(parents=True, exist_ok=True)
        self.config.state.mkdir(parents=True, exist_ok=True)
        if os.path.lexists(paths.run):
            raise RunnerError(f"task run directory already exists: {paths.run}")

        paths.run.mkdir(mode=0o750)
        paths.logs.mkdir(mode=0o750)
        paths.git_before.mkdir(parents=True, mode=0o750)
        paths.git_after.mkdir(parents=True, mode=0o750)
        self._copy_input_file(prompt_file, paths.prompt, 0o640)
        if validation_file is not None:
            self._copy_input_file(validation_file, paths.validation, 0o750)
        atomic_write_text(paths.expected_ref, normalized_ref + "\n", 0o640)
        atomic_write_text(paths.timeout, f"{DEFAULT_TIMEOUT_SECONDS}\n", 0o640)
        atomic_write_text(paths.reasoning_effort, DEFAULT_REASONING_EFFORT + "\n", 0o640)
        atomic_write_text(paths.state, TaskState.READY.value + "\n", 0o640)

        metadata = TaskMetadata(
            task_id=task_id,
            state=TaskState.READY,
            expected_ref=normalized_ref,
            worktree_path=str(paths.worktree),
            report_path=str(paths.report),
            created_at=_timestamp_now(),
        )
        self._write_metadata(paths, metadata)
        return metadata

    def run_task(self, task_id: str) -> TaskMetadata:
        validate_task_id(task_id)
        with self._exclusive_lock():
            paths = self._existing_task_paths(task_id)
            self._require_task_directories(paths)
            metadata = self._load_metadata(paths)
            current_state = self._load_state(paths)
            if current_state is not TaskState.READY:
                raise RunnerError(
                    f"task {task_id} is {current_state.value}; only ready tasks may run"
                )
            metadata.state = current_state

            started_at = _timestamp_now()
            started_monotonic = time.monotonic()
            metadata.started_at = started_at
            metadata.finished_at = None
            metadata.runner_error = None
            metadata.runner_pid = os.getpid()
            metadata.runner_exit_code = None

            with self._interruption_guard():
                try:
                    self._transition(paths, metadata, TaskState.RUNNING)
                    codex_result = self._prepare_and_run_codex(paths, metadata)
                    validation_result = None
                    if codex_result.returncode == 0 and not codex_result.timed_out:
                        validation_result = self._run_validation_if_present(paths, metadata)
                    self._collect_git_diagnostics(paths, metadata)
                    final_state = self._outcome_state(codex_result, validation_result)
                    self._finish(paths, metadata, final_state, started_monotonic)
                except BaseException as error:
                    self._ignore_lifecycle_signals()
                    durable_state = self._durable_state(paths, metadata.state)
                    if durable_state in TERMINAL_STATES:
                        metadata.state = durable_state
                        self._write_metadata(paths, metadata)
                    elif durable_state is TaskState.RUNNING:
                        metadata.state = TaskState.RUNNING
                        self._preserve_diagnostics_after_error(paths, metadata)
                        self._record_runner_failure(
                            paths, metadata, error, started_monotonic
                        )
                    else:
                        raise
            return metadata

    def status(self, task_id: str, output: TextIO) -> bool:
        validate_task_id(task_id)
        paths = self._existing_task_paths(task_id)
        try:
            metadata = self._load_metadata(paths)
            metadata.state = self._load_state(paths)
        except RunnerError as error:
            self._print_rows(
                output,
                [
                    ("Task ID", task_id),
                    ("State", "invalid"),
                    ("Runner error", str(error)),
                ],
            )
            return False
        current_head = metadata.current_head
        if not paths.worktree.is_symlink() and paths.worktree.is_dir():
            try:
                current_head = self._git(paths.worktree, ["rev-parse", "HEAD"]).stdout.strip()
            except RunnerError:
                pass
        runner_process = self._runner_process_status(metadata)
        report = metadata.report_path
        if not os.path.lexists(paths.report):
            report += " (not present)"
        rows = [
            ("Task ID", metadata.task_id),
            ("State", metadata.state.value),
            ("Created", metadata.created_at),
            ("Updated", metadata.updated_at or "-"),
            ("Started", metadata.started_at or "-"),
            ("Finished", metadata.finished_at or "-"),
            ("Duration", _format_duration(metadata.duration_seconds)),
            ("Runner process", runner_process),
            ("Runner exit code", _format_optional(metadata.runner_exit_code)),
            ("Expected ref", metadata.expected_ref),
            ("Expected commit", metadata.expected_commit or "-"),
            ("Current worktree HEAD", current_head or "-"),
            ("Codex exit code", _format_optional(metadata.codex_exit_code)),
            ("Validation exit code", _format_optional(metadata.validation_exit_code)),
            ("Unexpected commits", _format_optional(metadata.unexpected_commit_count)),
            ("Worktree", metadata.worktree_path),
            ("Report", report),
            ("Runner error", metadata.runner_error or "-"),
        ]
        self._print_rows(output, rows)
        return True

    def list_tasks(self, output: TextIO) -> None:
        if not self.config.runs.exists():
            print("No tasks.", file=output)
            return
        task_rows: list[tuple[str, str]] = []
        for entry in sorted(self.config.runs.iterdir(), key=lambda path: path.name):
            if entry.is_symlink() or not entry.is_dir() or not TASK_ID_PATTERN.fullmatch(entry.name):
                continue
            try:
                paths = self.config.task_paths(entry.name)
                durable_state = self._load_state(paths)
                state = durable_state.value
                if durable_state is TaskState.RUNNING:
                    metadata = self._load_metadata(paths)
                    if self._runner_process_is_stale(metadata):
                        state = "running-stale"
            except (OSError, ValueError, RunnerError):
                state = "invalid"
            task_rows.append((entry.name, state))
        if not task_rows:
            print("No tasks.", file=output)
            return
        width = max(len(task_id) for task_id, _ in task_rows)
        for task_id, state in task_rows:
            print(f"{task_id:<{width}}  {state}", file=output)

    def doctor(self, output: TextIO) -> bool:
        self._print_installation_status(output)
        checks: list[tuple[str, bool, str]] = []
        python_supported = sys.version_info >= (3, 10)
        checks.append(
            (
                "Python version",
                python_supported,
                f"{sys.version_info.major}.{sys.version_info.minor}.{sys.version_info.micro}",
            )
        )
        checks.append(("Configured root", True, str(self.config.root)))
        checks.append(("Configured lock", True, str(self.config.lock)))

        resolved_commands: dict[str, str] = {}
        for name, command in (
            ("git", self.config.git_command),
            ("codex", self.config.codex_command),
            ("bash", self.config.bash_command),
        ):
            resolved = shutil.which(command)
            checks.append((f"Command: {name}", resolved is not None, resolved or "not found"))
            if resolved is not None:
                resolved_commands[name] = resolved

        repository_exists = self.config.repository.is_dir()
        checks.append(("Repository exists", repository_exists, str(self.config.repository)))
        repository_valid = False
        repository_detail = "repository is unavailable"
        if repository_exists and "git" in resolved_commands:
            result, command_error = self._doctor_command(
                [resolved_commands["git"], "-C", str(self.config.repository), "rev-parse", "--is-inside-work-tree"],
            )
            if result is not None:
                repository_valid = result.returncode == 0 and result.stdout.strip() == "true"
                repository_detail = result.stdout.strip() or result.stderr.strip() or f"exit {result.returncode}"
            else:
                repository_detail = command_error
        checks.append(("Repository is Git", repository_valid, repository_detail))

        for label, directory in (
            ("Runs directory writable", self.config.runs),
            ("Worktrees directory writable", self.config.worktrees),
            ("State directory writable", self.config.state),
        ):
            writable, detail = self._check_directory_writable(directory)
            checks.append((label, writable, detail))

        if "codex" in resolved_commands:
            version, version_error = self._doctor_command([resolved_commands["codex"], "--version"])
            version_ok = version is not None and version.returncode == 0
            version_detail = version_error
            if version is not None:
                version_detail = version.stdout.strip() or version.stderr.strip() or f"exit {version.returncode}"
            checks.append(("Codex version", version_ok, version_detail))
            profile, profile_error = self._doctor_command(
                [resolved_commands["codex"], "--profile", "greenfield-agent", "exec", "--help"],
            )
            profile_ok = profile is not None and profile.returncode == 0
            profile_detail = profile_error
            if profile is not None:
                profile_detail = profile.stdout.strip().splitlines()[0] if profile.stdout.strip() else (
                    profile.stderr.strip() or f"exit {profile.returncode}"
                )
            checks.append(("Codex profile parse", profile_ok, profile_detail))
        else:
            checks.append(("Codex version", False, "codex not found"))
            checks.append(("Codex profile parse", False, "codex not found"))

        for label, passed, detail in checks:
            print(f"{'PASS' if passed else 'FAIL'}  {label}: {detail}", file=output)
        return all(passed for _, passed, _ in checks)

    def _print_installation_status(self, output: TextIO) -> None:
        package_path = Path(__file__).resolve().parent
        release_path = package_path.parent
        version_path = release_path / "VERSION"
        metadata_path = release_path / "INSTALL-METADATA"
        installed_release = version_path.is_file() and metadata_path.is_file()

        if installed_release:
            version = version_path.read_text(encoding="utf-8").strip()
            print(f"PASS  Installed version: {version or 'empty VERSION'}", file=output)
            print(f"PASS  Installation metadata: {metadata_path}", file=output)
        else:
            print(f"INFO  Runner package: source checkout at {package_path}", file=output)

        wrapper = os.environ.get("GREENFIELD_AGENT_WRAPPER") or shutil.which("greenfield-agent")
        if wrapper:
            print(f"INFO  Wrapper path: {wrapper}", file=output)
        else:
            print("INFO  Wrapper path: not found in the environment", file=output)

        unit_directory = Path(
            os.environ.get("GREENFIELD_AGENT_SYSTEMD_UNIT_DIR", "/etc/systemd/user")
        )
        unit_path = unit_directory / "greenfield-task@.service"
        if unit_path.is_file():
            print(f"INFO  Service unit: present at {unit_path}", file=output)
        else:
            print(
                f"INFO  Service unit: not present at {unit_path} (optional outside systemd)",
                file=output,
            )

    def _prepare_and_run_codex(
        self, paths: TaskPaths, metadata: TaskMetadata
    ) -> TimedProcessResult:
        self._verify_manager_checkout()
        self._git(self.config.repository, ["fetch", "--prune", "origin"])
        expected_ref = self._read_required_text(paths.expected_ref, "expected ref")
        resolved = self._git(
            self.config.repository,
            ["rev-parse", "--verify", "--end-of-options", f"{expected_ref}^{{commit}}"],
        ).stdout.strip()
        if not re.fullmatch(r"[0-9a-fA-F]{40,64}", resolved):
            raise RunnerError(f"expected ref did not resolve to a commit: {expected_ref}")
        metadata.expected_commit = resolved
        self._write_metadata(paths, metadata)

        if os.path.lexists(paths.worktree):
            raise RunnerError(f"task worktree already exists: {paths.worktree}")
        branch_ref = f"refs/heads/agent/{paths.task_id}"
        branch_check = self._git_allow_status(
            self.config.repository,
            ["show-ref", "--verify", "--quiet", branch_ref],
            {0, 1},
        )
        if branch_check.returncode == 0:
            raise RunnerError(f"task branch already exists: agent/{paths.task_id}")
        self._git(
            self.config.repository,
            ["worktree", "add", "-b", f"agent/{paths.task_id}", str(paths.worktree), resolved],
        )
        self._collect_git_snapshot(paths.worktree, paths.git_before, resolved)
        return self._run_codex(paths, metadata)

    def _run_codex(self, paths: TaskPaths, metadata: TaskMetadata) -> TimedProcessResult:
        timeout_seconds = self._read_positive_timeout(paths.timeout, "Codex timeout")
        reasoning_effort = self._read_required_text(paths.reasoning_effort, "reasoning effort")
        if reasoning_effort not in {"minimal", "low", "medium", "high", "xhigh"}:
            raise RunnerError(f"unsupported reasoning effort: {reasoning_effort}")
        network_allowed = self._network_allowed(paths)
        atomic_write_text(paths.report, "", 0o640)
        arguments = [
            self.config.codex_command,
            "--profile",
            "greenfield-agent",
            "exec",
            "--sandbox",
            "workspace-write",
            "--config",
            'approval_policy="never"',
            "--config",
            f"sandbox_workspace_write.network_access={str(network_allowed).lower()}",
            "--config",
            f'model_reasoning_effort="{reasoning_effort}"',
            "--json",
            "--output-last-message",
            str(paths.report),
            "-",
        ]
        with self._open_regular_input(paths.prompt) as prompt_stream:
            with self._open_output(paths.logs / "codex-events.jsonl") as events_stream:
                with self._open_output(paths.logs / "codex-stderr.log") as error_stream:
                    result = run_timed_process(
                        arguments,
                        cwd=paths.worktree,
                        stdin=prompt_stream,
                        stdout=events_stream,
                        stderr=error_stream,
                        timeout_seconds=timeout_seconds,
                        terminate_grace_seconds=self.config.terminate_grace_seconds,
                    )
        metadata.codex_exit_code = result.returncode
        metadata.codex_timed_out = result.timed_out
        self._require_regular_file(paths.report, "final report")
        atomic_write_text(paths.logs / "codex-exit-code", f"{result.returncode}\n")
        self._write_timing(paths.logs / "codex-timing.json", result)
        self._write_metadata(paths, metadata)
        return result

    def _run_validation_if_present(
        self, paths: TaskPaths, metadata: TaskMetadata
    ) -> TimedProcessResult | None:
        if not os.path.lexists(paths.validation):
            return None
        self._require_regular_file(paths.validation, "validation script")
        timeout_seconds = float(self.config.validation_timeout_seconds)
        validation_timeout_file = paths.run / "validation-timeout-seconds"
        if os.path.lexists(validation_timeout_file):
            timeout_seconds = self._read_positive_timeout(
                validation_timeout_file, "validation timeout"
            )
        with self._open_regular_input(paths.validation) as validation_stream:
            with self._open_output(paths.logs / "validation-stdout.log") as output_stream:
                with self._open_output(paths.logs / "validation-stderr.log") as error_stream:
                    result = run_timed_process(
                        [self.config.bash_command, "-s", "--"],
                        cwd=paths.worktree,
                        stdin=validation_stream,
                        stdout=output_stream,
                        stderr=error_stream,
                        timeout_seconds=timeout_seconds,
                        terminate_grace_seconds=self.config.terminate_grace_seconds,
                    )
        metadata.validation_exit_code = result.returncode
        metadata.validation_timed_out = result.timed_out
        atomic_write_text(paths.logs / "validation-exit-code", f"{result.returncode}\n")
        self._write_timing(paths.logs / "validation-timing.json", result)
        self._write_metadata(paths, metadata)
        return result

    def _collect_git_diagnostics(self, paths: TaskPaths, metadata: TaskMetadata) -> None:
        if metadata.expected_commit is None:
            raise RunnerError("cannot collect Git diagnostics without the expected commit")
        self._collect_git_snapshot(paths.worktree, paths.git_after, metadata.expected_commit)
        head = self._git(paths.worktree, ["rev-parse", "HEAD"]).stdout.strip()
        count_text = self._git(
            paths.worktree,
            ["rev-list", "--count", f"{metadata.expected_commit}..HEAD"],
        ).stdout.strip()
        metadata.current_head = head
        metadata.unexpected_commit_count = int(count_text)
        self._write_metadata(paths, metadata)

    def _collect_git_snapshot(self, worktree: Path, destination: Path, expected_commit: str) -> None:
        self._require_directory(worktree, "task worktree")
        self._require_directory(destination, "Git diagnostics directory")
        commands: tuple[tuple[str, Sequence[str]], ...] = (
            ("status.txt", ["status", "--short", "--branch", "--untracked-files=all"]),
            ("staged.diff", ["diff", "--cached", "--binary", "--no-ext-diff"]),
            ("unstaged.diff", ["diff", "--binary", "--no-ext-diff"]),
            ("expected.diff", ["diff", "--binary", "--no-ext-diff", expected_commit]),
            ("diff-stat.txt", ["diff", "--stat", expected_commit]),
            ("head.txt", ["rev-parse", "HEAD"]),
            ("commits-added.txt", ["log", "--format=fuller", f"{expected_commit}..HEAD"]),
        )
        for filename, arguments in commands:
            result = self._git(worktree, arguments)
            atomic_write_text(destination / filename, result.stdout)
        untracked = self._git(
            worktree, ["ls-files", "--others", "--exclude-standard", "-z"]
        ).stdout
        inventory = [name for name in untracked.split("\0") if name]
        atomic_write_json(destination / "untracked-files.json", {"files": inventory})
        atomic_write_text(destination / "untracked-files.txt", "".join(f"{name}\n" for name in inventory))

    def _verify_manager_checkout(self) -> None:
        if not self.config.repository.is_dir():
            raise RunnerError(f"manager repository does not exist: {self.config.repository}")
        inside = self._git(self.config.repository, ["rev-parse", "--is-inside-work-tree"])
        if inside.stdout.strip() != "true":
            raise RunnerError(f"manager repository is not a Git worktree: {self.config.repository}")
        status_result = self._git(
            self.config.repository, ["status", "--porcelain=v1", "--untracked-files=all"]
        )
        if status_result.stdout:
            raise RunnerError("manager checkout is not clean")

    def _outcome_state(
        self,
        codex_result: TimedProcessResult,
        validation_result: TimedProcessResult | None,
    ) -> TaskState:
        if codex_result.timed_out or codex_result.returncode != 0:
            return TaskState.BLOCKED_CODEX
        if validation_result is not None and (
            validation_result.timed_out or validation_result.returncode != 0
        ):
            return TaskState.BLOCKED_VALIDATION
        return TaskState.REVIEW

    def _finish(
        self,
        paths: TaskPaths,
        metadata: TaskMetadata,
        state: TaskState,
        started_monotonic: float,
    ) -> None:
        metadata.finished_at = _timestamp_now()
        metadata.duration_seconds = round(time.monotonic() - started_monotonic, 6)
        metadata.runner_exit_code = 0 if state is TaskState.REVIEW else 1
        self._transition(paths, metadata, state)

    def _record_runner_failure(
        self,
        paths: TaskPaths,
        metadata: TaskMetadata,
        error: BaseException,
        started_monotonic: float,
    ) -> None:
        durable_state = self._durable_state(paths, metadata.state)
        if durable_state in TERMINAL_STATES:
            metadata.state = durable_state
            self._write_metadata(paths, metadata)
            return
        if durable_state is not TaskState.RUNNING:
            raise RunnerError("cannot record a runner failure before the task is running")

        summary = _error_summary(error)
        metadata.runner_error = summary
        metadata.finished_at = _timestamp_now()
        metadata.duration_seconds = round(time.monotonic() - started_monotonic, 6)
        metadata.runner_exit_code = (
            error.exit_code if isinstance(error, RunnerInterrupted) else 1
        )
        previous = ""
        if os.path.lexists(paths.runner_errors):
            try:
                previous = read_text_file(paths.runner_errors)
            except OSError:
                previous = ""
        atomic_write_text(
            paths.runner_errors,
            previous + f"{metadata.finished_at} {summary}\n",
            0o640,
        )
        self._transition(paths, metadata, TaskState.BLOCKED_RUNNER)

    def _preserve_diagnostics_after_error(
        self, paths: TaskPaths, metadata: TaskMetadata
    ) -> None:
        if (
            paths.worktree.is_symlink()
            or not paths.worktree.is_dir()
            or metadata.expected_commit is None
        ):
            return
        try:
            self._collect_git_diagnostics(paths, metadata)
        except Exception as diagnostics_error:
            try:
                atomic_write_text(
                    paths.git_after / "collection-error.txt",
                    _error_summary(diagnostics_error) + "\n",
                )
            except OSError:
                pass

    def _transition(
        self, paths: TaskPaths, metadata: TaskMetadata, requested: TaskState
    ) -> None:
        next_state = transition_state(metadata.state, requested)
        atomic_write_text(paths.state, next_state.value + "\n", 0o640)
        metadata.state = next_state
        self._write_metadata(paths, metadata)

    def _load_metadata(self, paths: TaskPaths) -> TaskMetadata:
        try:
            metadata = TaskMetadata.from_dict(read_json(paths.metadata))
        except (OSError, ValueError, TypeError, KeyError, json.JSONDecodeError) as error:
            raise RunnerError(f"invalid task metadata for {paths.task_id}: {error}") from error
        if metadata.task_id != paths.task_id:
            raise RunnerError(f"task metadata ID does not match directory: {paths.task_id}")
        return metadata

    def _write_metadata(self, paths: TaskPaths, metadata: TaskMetadata) -> None:
        metadata.updated_at = _timestamp_now()
        atomic_write_json(paths.metadata, metadata.to_dict())

    def _load_state(self, paths: TaskPaths) -> TaskState:
        try:
            return TaskState(read_text_file(paths.state).strip())
        except (OSError, ValueError) as error:
            raise RunnerError(f"invalid task state for {paths.task_id}: {error}") from error

    def _existing_task_paths(self, task_id: str) -> TaskPaths:
        paths = self.config.task_paths(task_id)
        if not os.path.lexists(paths.run):
            raise RunnerError(f"task does not exist: {task_id}")
        if paths.run.is_symlink() or not paths.run.is_dir():
            raise RunnerError(f"task run path is not a regular directory: {paths.run}")
        return paths

    def _read_required_text(self, path: Path, description: str) -> str:
        self._require_regular_file(path, description)
        value = read_text_file(path).strip()
        if not value:
            raise RunnerError(f"{description} must not be empty: {path}")
        return value

    def _read_positive_timeout(self, path: Path, description: str) -> float:
        value = self._read_required_text(path, description)
        try:
            timeout = float(value)
        except ValueError as error:
            raise RunnerError(f"{description} is not numeric: {path}") from error
        if not math.isfinite(timeout) or timeout <= 0:
            raise RunnerError(f"{description} must be greater than zero: {path}")
        return timeout

    def _network_allowed(self, paths: TaskPaths) -> bool:
        if not os.path.lexists(paths.allow_network):
            return False
        self._require_regular_file(paths.allow_network, "allow-network marker")
        return True

    def _require_input_file(self, path: Path, description: str) -> None:
        if not os.path.lexists(path):
            raise RunnerError(f"{description} file does not exist: {path}")
        self._require_regular_file(path, f"{description} file")

    def _require_regular_file(self, path: Path, description: str) -> None:
        try:
            file_status = path.lstat()
        except OSError as error:
            raise RunnerError(f"cannot inspect {description}: {path}: {error}") from error
        if stat.S_ISLNK(file_status.st_mode) or not stat.S_ISREG(file_status.st_mode):
            raise RunnerError(f"{description} is not a regular non-symlink file: {path}")

    def _copy_input_file(self, source: Path, destination: Path, mode: int) -> None:
        with self._open_regular_input(source) as source_stream:
            data = source_stream.read()
        atomic_write_bytes(destination, data, mode)

    def _git(self, cwd: Path, arguments: Sequence[str]) -> CommandResult:
        result = self._git_allow_status(cwd, arguments, {0})
        return result

    def _git_allow_status(
        self, cwd: Path, arguments: Sequence[str], allowed_statuses: set[int]
    ) -> CommandResult:
        try:
            result = run_command(
                [self.config.git_command, "-C", str(cwd), *arguments],
                timeout_seconds=120,
            )
        except (OSError, subprocess.TimeoutExpired) as error:
            raise RunnerError(f"failed to execute Git: {error}") from error
        if result.returncode not in allowed_statuses:
            detail = result.stderr.strip() or result.stdout.strip() or "no diagnostic output"
            raise RunnerError(
                f"Git command failed ({result.returncode}): {' '.join(arguments)}: {detail}"
            )
        return result

    def _write_timing(self, path: Path, result: TimedProcessResult) -> None:
        atomic_write_json(
            path,
            {
                "duration_seconds": round(result.duration_seconds, 6),
                "timed_out": result.timed_out,
            },
        )

    def _open_regular_input(self, path: Path) -> BinaryIO:
        self._require_regular_file(path, "input")
        flags = os.O_RDONLY
        if hasattr(os, "O_NOFOLLOW"):
            flags |= os.O_NOFOLLOW
        if hasattr(os, "O_NONBLOCK"):
            flags |= os.O_NONBLOCK
        descriptor = os.open(path, flags)
        if not stat.S_ISREG(os.fstat(descriptor).st_mode):
            os.close(descriptor)
            raise RunnerError(f"input is not a regular file: {path}")
        return os.fdopen(descriptor, "rb")

    def _open_output(self, path: Path) -> BinaryIO:
        flags = os.O_WRONLY | os.O_CREAT | os.O_TRUNC
        if hasattr(os, "O_NOFOLLOW"):
            flags |= os.O_NOFOLLOW
        if hasattr(os, "O_NONBLOCK"):
            flags |= os.O_NONBLOCK
        descriptor = os.open(path, flags, 0o640)
        if not stat.S_ISREG(os.fstat(descriptor).st_mode):
            os.close(descriptor)
            raise RunnerError(f"output is not a regular file: {path}")
        os.fchmod(descriptor, 0o640)
        return os.fdopen(descriptor, "wb")

    @contextmanager
    def _exclusive_lock(self) -> Iterator[None]:
        self.config.lock.parent.mkdir(parents=True, exist_ok=True)
        flags = os.O_RDWR | os.O_CREAT
        if hasattr(os, "O_NOFOLLOW"):
            flags |= os.O_NOFOLLOW
        descriptor = os.open(self.config.lock, flags, 0o640)
        if not stat.S_ISREG(os.fstat(descriptor).st_mode):
            os.close(descriptor)
            raise RunnerError(f"runner lock is not a regular file: {self.config.lock}")
        try:
            try:
                fcntl.flock(descriptor, fcntl.LOCK_EX | fcntl.LOCK_NB)
            except BlockingIOError as error:
                raise LockContentionError("another Greenfield task is already running") from error
            yield
        finally:
            os.close(descriptor)

    @contextmanager
    def _interruption_guard(self) -> Iterator[None]:
        handled_signals = (signal.SIGINT, signal.SIGTERM)
        previous_handlers = {
            signal_number: signal.getsignal(signal_number)
            for signal_number in handled_signals
        }

        def handle_interruption(signal_number: int, _frame: object) -> None:
            self._ignore_lifecycle_signals()
            raise RunnerInterrupted(signal_number)

        try:
            for signal_number in handled_signals:
                signal.signal(signal_number, handle_interruption)
            yield
        finally:
            for signal_number, previous_handler in previous_handlers.items():
                signal.signal(signal_number, previous_handler)

    def _ignore_lifecycle_signals(self) -> None:
        signal.signal(signal.SIGINT, signal.SIG_IGN)
        signal.signal(signal.SIGTERM, signal.SIG_IGN)

    def _durable_state(self, paths: TaskPaths, fallback: TaskState) -> TaskState:
        try:
            return self._load_state(paths)
        except RunnerError:
            return fallback

    def _require_task_directories(self, paths: TaskPaths) -> None:
        for directory, description in (
            (paths.logs, "task logs directory"),
            (paths.git, "task Git diagnostics directory"),
            (paths.git_before, "before-run Git diagnostics directory"),
            (paths.git_after, "after-run Git diagnostics directory"),
        ):
            self._require_directory(directory, description)

    def _require_directory(self, path: Path, description: str) -> None:
        try:
            path_status = path.lstat()
        except OSError as error:
            raise RunnerError(f"cannot inspect {description}: {path}: {error}") from error
        if stat.S_ISLNK(path_status.st_mode) or not stat.S_ISDIR(path_status.st_mode):
            raise RunnerError(f"{description} is not a regular non-symlink directory: {path}")

    def _runner_process_status(self, metadata: TaskMetadata) -> str:
        if metadata.runner_pid is None:
            return "-" if metadata.state is not TaskState.RUNNING else "PID not recorded; stale status unknown"
        if not isinstance(metadata.runner_pid, int) or metadata.runner_pid <= 0:
            return f"invalid recorded PID: {metadata.runner_pid}"
        if metadata.state is not TaskState.RUNNING:
            return f"PID {metadata.runner_pid} (finished)"
        if self._runner_process_is_stale(metadata):
            return f"PID {metadata.runner_pid} is not alive; task is stale"
        return f"PID {metadata.runner_pid} is alive"

    def _runner_process_is_stale(self, metadata: TaskMetadata) -> bool:
        runner_pid = metadata.runner_pid
        return (
            metadata.state is TaskState.RUNNING
            and isinstance(runner_pid, int)
            and runner_pid > 0
            and not _process_is_alive(runner_pid)
        )

    def _print_rows(self, output: TextIO, rows: Sequence[tuple[str, object]]) -> None:
        width = max(len(label) for label, _ in rows)
        for label, value in rows:
            print(f"{label:<{width}} : {value}", file=output)

    def _check_directory_writable(self, directory: Path) -> tuple[bool, str]:
        try:
            if os.path.lexists(directory) and directory.is_symlink():
                return False, f"refusing symlink: {directory}"
            directory.mkdir(parents=True, exist_ok=True)
            with tempfile.NamedTemporaryFile(dir=directory, prefix=".doctor-", delete=True):
                pass
            return True, str(directory)
        except OSError as error:
            return False, f"{directory}: {error}"

    def _doctor_command(self, arguments: Sequence[str]) -> tuple[CommandResult | None, str]:
        try:
            return run_command(arguments, timeout_seconds=10), ""
        except (OSError, subprocess.TimeoutExpired) as error:
            return None, str(error)


def _timestamp_now() -> str:
    return datetime.now(timezone.utc).isoformat().replace("+00:00", "Z")


def _error_summary(error: BaseException) -> str:
    detail = " ".join(str(error).split()) or "no detail"
    summary = f"{type(error).__name__}: {detail}"
    maximum_length = 500
    if len(summary) <= maximum_length:
        return summary
    return summary[: maximum_length - 3] + "..."


def _process_is_alive(process_id: int) -> bool:
    try:
        os.kill(process_id, 0)
    except ProcessLookupError:
        return False
    except PermissionError:
        return True
    return True


def _format_optional(value: object | None) -> str:
    return "-" if value is None else str(value)


def _format_duration(duration_seconds: float | None) -> str:
    if duration_seconds is None:
        return "-"
    return f"{duration_seconds:.3f} seconds"
