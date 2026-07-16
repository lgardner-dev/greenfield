from __future__ import annotations

import dataclasses
import fcntl
import io
import json
import os
import shutil
import subprocess
import tempfile
import time
import unittest
from pathlib import Path
from unittest.mock import patch

from greenfield_agent.models import TaskState, atomic_write_json, atomic_write_text
from greenfield_agent.paths import RunnerConfig
from greenfield_agent.runner import GreenfieldRunner, LockContentionError, RunnerError


FAKE_CODEX = r'''#!/usr/bin/env python3
import json
import os
import subprocess
import sys
import time
from pathlib import Path

if "--version" in sys.argv:
    print("codex-fake 1.0")
    raise SystemExit(0)
if "--help" in sys.argv:
    print("fake Codex exec help")
    raise SystemExit(0)

arguments = sys.argv[1:]
Path("codex-arguments.json").write_text(json.dumps(arguments), encoding="utf-8")
prompt = sys.stdin.read()
Path("received-prompt.txt").write_text(prompt, encoding="utf-8")
behavior = os.environ.get("FAKE_CODEX_BEHAVIOR", "success")
if behavior == "timeout":
    time.sleep(30)
if behavior == "failure":
    print("controlled Codex failure", file=sys.stderr)
    raise SystemExit(7)

Path("generated.txt").write_text("generated\n", encoding="utf-8")
if behavior == "commit":
    Path("committed.txt").write_text("unexpected commit\n", encoding="utf-8")
    subprocess.run(["git", "add", "committed.txt"], check=True)
    subprocess.run(["git", "commit", "-m", "unexpected agent commit"], check=True,
                   stdout=subprocess.DEVNULL)
    Path("untracked.txt").write_text("untracked\n", encoding="utf-8")

report_index = arguments.index("--output-last-message") + 1
Path(arguments[report_index]).write_text("Fake final report.\n", encoding="utf-8")
print(json.dumps({"type": "result", "status": "ok"}))
'''


def run_git(arguments: list[str], cwd: Path | None = None) -> str:
    completed = subprocess.run(
        ["git", *arguments],
        cwd=cwd,
        stdin=subprocess.DEVNULL,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
        encoding="utf-8",
        check=False,
    )
    if completed.returncode != 0:
        raise AssertionError(f"Git failed: {arguments}: {completed.stderr}")
    return completed.stdout


class RunnerTestCase(unittest.TestCase):
    def setUp(self) -> None:
        self.temporary_directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary_directory.cleanup)
        self.base = Path(self.temporary_directory.name)
        self.origin = self.base / "origin.git"
        self.seed = self.base / "seed"
        self.repository = self.base / "manager"
        self.root = self.base / "greenfield"
        self.fake_codex = self.base / "fake-codex"
        self.fake_codex.write_text(FAKE_CODEX, encoding="utf-8")
        self.fake_codex.chmod(0o755)
        self._create_repository()
        self.config = RunnerConfig(
            root=self.root,
            repository=self.repository,
            runs=self.root / "runs",
            worktrees=self.root / "worktrees",
            state=self.root / "state",
            lock=self.root / "state" / "agent.lock",
            git_command=shutil.which("git") or "git",
            codex_command=str(self.fake_codex),
            bash_command=shutil.which("bash") or "bash",
            validation_timeout_seconds=2,
            terminate_grace_seconds=0.1,
        )
        self.runner = GreenfieldRunner(self.config)
        self.prompt = self.base / "prompt.md"
        self.prompt.write_text("Implement exactly one fake change.\n", encoding="utf-8")

    def _create_repository(self) -> None:
        run_git(["init", "--bare", str(self.origin)])
        run_git(["init", str(self.seed)])
        run_git(["config", "user.name", "Greenfield Tests"], self.seed)
        run_git(["config", "user.email", "tests@example.invalid"], self.seed)
        (self.seed / "tracked.txt").write_text("initial\n", encoding="utf-8")
        run_git(["add", "tracked.txt"], self.seed)
        run_git(["commit", "-m", "initial"], self.seed)
        run_git(["branch", "-M", "main"], self.seed)
        run_git(["remote", "add", "origin", str(self.origin)], self.seed)
        run_git(["push", "-u", "origin", "main"], self.seed)
        run_git(["symbolic-ref", "HEAD", "refs/heads/main"], self.origin)
        run_git(["clone", str(self.origin), str(self.repository)])
        run_git(["config", "user.name", "Greenfield Tests"], self.repository)
        run_git(["config", "user.email", "tests@example.invalid"], self.repository)

    def create_task(self, task_id: str = "task-1", validation: Path | None = None) -> None:
        self.runner.create_task(task_id, "origin/main", self.prompt, validation)

    def run_with_behavior(self, behavior: str, task_id: str = "task-1"):
        with patch.dict(os.environ, {"FAKE_CODEX_BEHAVIOR": behavior}, clear=False):
            return self.runner.run_task(task_id)


class CreationTests(RunnerTestCase):
    def test_create_success_and_duplicate_rejection(self) -> None:
        validation = self.base / "validation.sh"
        validation.write_text("#!/bin/bash\nexit 0\n", encoding="utf-8")
        metadata = self.runner.create_task(
            "Create_1.0", " origin/main ", self.prompt, validation
        )
        paths = self.config.task_paths("Create_1.0")
        self.assertIs(metadata.state, TaskState.READY)
        self.assertEqual(paths.prompt.read_text(encoding="utf-8"), self.prompt.read_text(encoding="utf-8"))
        self.assertEqual(paths.expected_ref.read_text(encoding="utf-8"), "origin/main\n")
        self.assertEqual(paths.timeout.read_text(encoding="utf-8"), "14400\n")
        self.assertEqual(paths.reasoning_effort.read_text(encoding="utf-8"), "high\n")
        self.assertTrue(paths.validation.is_file())
        self.assertTrue(paths.git_before.is_dir())
        self.assertTrue(paths.git_after.is_dir())
        with self.assertRaises(RunnerError):
            self.runner.create_task("Create_1.0", "origin/main", self.prompt)

    def test_invalid_prompt_and_validation_paths(self) -> None:
        with self.assertRaises(RunnerError):
            self.runner.create_task("missing-prompt", "origin/main", self.base / "missing")
        with self.assertRaises(RunnerError):
            self.runner.create_task(
                "missing-validation", "origin/main", self.prompt, self.base / "missing"
            )
        prompt_link = self.base / "prompt-link"
        prompt_link.symlink_to(self.prompt)
        with self.assertRaises(RunnerError):
            self.runner.create_task("linked-prompt", "origin/main", prompt_link)


class ExecutionTests(RunnerTestCase):
    def test_successful_fake_codex_run_and_rerun_rejection(self) -> None:
        self.create_task()
        metadata = self.run_with_behavior("success")
        paths = self.config.task_paths("task-1")
        self.assertIs(metadata.state, TaskState.REVIEW)
        self.assertEqual(metadata.codex_exit_code, 0)
        self.assertFalse(metadata.codex_timed_out)
        self.assertEqual(paths.report.read_text(encoding="utf-8"), "Fake final report.\n")
        self.assertIn('"status": "ok"', (paths.logs / "codex-events.jsonl").read_text(encoding="utf-8"))
        arguments = json.loads((paths.worktree / "codex-arguments.json").read_text(encoding="utf-8"))
        self.assertEqual(arguments[:3], ["--profile", "greenfield-agent", "exec"])
        self.assertIn("workspace-write", arguments)
        self.assertIn('approval_policy="never"', arguments)
        self.assertIn("sandbox_workspace_write.network_access=false", arguments)
        self.assertEqual(
            (paths.worktree / "received-prompt.txt").read_text(encoding="utf-8"),
            self.prompt.read_text(encoding="utf-8"),
        )
        with self.assertRaises(RunnerError):
            self.runner.run_task("task-1")

    def test_fake_codex_failure(self) -> None:
        validation = self.base / "must-not-run.sh"
        validation.write_text("#!/bin/bash\ntouch validation-ran\n", encoding="utf-8")
        self.create_task(validation=validation)
        metadata = self.run_with_behavior("failure")
        self.assertIs(metadata.state, TaskState.BLOCKED_CODEX)
        self.assertEqual(metadata.codex_exit_code, 7)
        self.assertIsNone(metadata.validation_exit_code)
        self.assertFalse((self.config.task_paths("task-1").worktree / "validation-ran").exists())

    def test_allow_network_marker_changes_codex_sandbox_configuration(self) -> None:
        self.create_task()
        paths = self.config.task_paths("task-1")
        paths.allow_network.touch()
        metadata = self.run_with_behavior("success")
        arguments = json.loads(
            (paths.worktree / "codex-arguments.json").read_text(encoding="utf-8")
        )
        self.assertIs(metadata.state, TaskState.REVIEW)
        self.assertIn("sandbox_workspace_write.network_access=true", arguments)

    def test_fake_codex_timeout(self) -> None:
        self.create_task()
        paths = self.config.task_paths("task-1")
        paths.timeout.write_text("0.1\n", encoding="utf-8")
        metadata = self.run_with_behavior("timeout")
        self.assertIs(metadata.state, TaskState.BLOCKED_CODEX)
        self.assertEqual(metadata.codex_exit_code, 124)
        self.assertTrue(metadata.codex_timed_out)

    def test_validation_success_and_failure(self) -> None:
        successful_validation = self.base / "validate-success.sh"
        successful_validation.write_text(
            "#!/bin/bash\nset -eu\ntest -f generated.txt\n", encoding="utf-8"
        )
        self.create_task("validation-ok", successful_validation)
        successful = self.run_with_behavior("success", "validation-ok")
        self.assertIs(successful.state, TaskState.REVIEW)
        self.assertEqual(successful.validation_exit_code, 0)

        failing_validation = self.base / "validate-failure.sh"
        failing_validation.write_text("#!/bin/bash\nexit 9\n", encoding="utf-8")
        self.create_task("validation-fails", failing_validation)
        failed = self.run_with_behavior("success", "validation-fails")
        self.assertIs(failed.state, TaskState.BLOCKED_VALIDATION)
        self.assertEqual(failed.validation_exit_code, 9)

    def test_validation_timeout(self) -> None:
        validation = self.base / "validate-timeout.sh"
        validation.write_text("#!/bin/bash\nsleep 30\n", encoding="utf-8")
        self.create_task(validation=validation)
        paths = self.config.task_paths("task-1")
        (paths.run / "validation-timeout-seconds").write_text("0.1\n", encoding="utf-8")
        metadata = self.run_with_behavior("success")
        self.assertIs(metadata.state, TaskState.BLOCKED_VALIDATION)
        self.assertEqual(metadata.validation_exit_code, 124)
        self.assertTrue(metadata.validation_timed_out)

    def test_infrastructure_failure_becomes_blocked_runner(self) -> None:
        self.create_task()
        (self.repository / "dirty.txt").write_text("dirty\n", encoding="utf-8")
        metadata = self.run_with_behavior("success")
        self.assertIs(metadata.state, TaskState.BLOCKED_RUNNER)
        self.assertIn("manager checkout is not clean", metadata.runner_error or "")
        self.assertTrue(self.config.task_paths("task-1").runner_errors.is_file())

    def test_unexpected_exception_after_running_becomes_blocked_runner(self) -> None:
        self.create_task()
        with patch.object(
            self.runner,
            "_prepare_and_run_codex",
            side_effect=RuntimeError("unexpected test failure"),
        ):
            metadata = self.runner.run_task("task-1")
        paths = self.config.task_paths("task-1")
        self.assertIs(metadata.state, TaskState.BLOCKED_RUNNER)
        self.assertEqual(metadata.runner_exit_code, 1)
        self.assertEqual(metadata.runner_pid, os.getpid())
        self.assertIsNotNone(metadata.finished_at)
        self.assertIsNotNone(metadata.updated_at)
        self.assertEqual(read_state(paths), TaskState.BLOCKED_RUNNER.value)
        self.assertIn("RuntimeError: unexpected test failure", metadata.runner_error or "")

    def test_expected_ref_resolution_failure_becomes_blocked_runner(self) -> None:
        self.runner.create_task("bad-ref", "origin/ref-does-not-exist", self.prompt)
        metadata = self.run_with_behavior("success", "bad-ref")
        self.assertIs(metadata.state, TaskState.BLOCKED_RUNNER)
        self.assertEqual(metadata.runner_exit_code, 1)
        self.assertIn("Git command failed", metadata.runner_error or "")
        self.assertIsNotNone(metadata.finished_at)

    def test_terminal_state_is_not_overwritten_by_late_exception(self) -> None:
        self.create_task("review-task")
        self.create_task("codex-task")
        validation = self.base / "late-failure-validation.sh"
        validation.write_text("#!/bin/bash\nexit 9\n", encoding="utf-8")
        self.create_task("validation-task", validation)
        original_finish = self.runner._finish

        def finish_then_raise(*arguments, **keywords) -> None:
            original_finish(*arguments, **keywords)
            raise RuntimeError("failure after terminal transition")

        with patch.object(self.runner, "_finish", side_effect=finish_then_raise):
            review = self.run_with_behavior("success", "review-task")
            codex = self.run_with_behavior("failure", "codex-task")
            validation_result = self.run_with_behavior("success", "validation-task")
        for task_id, metadata, expected_state in (
            ("review-task", review, TaskState.REVIEW),
            ("codex-task", codex, TaskState.BLOCKED_CODEX),
            ("validation-task", validation_result, TaskState.BLOCKED_VALIDATION),
        ):
            with self.subTest(task_id=task_id):
                self.assertIs(metadata.state, expected_state)
                self.assertIsNone(metadata.runner_error)
                self.assertEqual(
                    read_state(self.config.task_paths(task_id)), expected_state.value
                )

    def test_sigterm_blocks_running_task_and_preserves_worktree(self) -> None:
        self.create_task()
        paths = self.config.task_paths("task-1")
        environment = os.environ.copy()
        environment.update(
            {
                "FAKE_CODEX_BEHAVIOR": "timeout",
                "GREENFIELD_ROOT": str(self.config.root),
                "GREENFIELD_REPOSITORY": str(self.config.repository),
                "GREENFIELD_GIT": self.config.git_command,
                "GREENFIELD_CODEX": self.config.codex_command,
                "GREENFIELD_BASH": self.config.bash_command,
                "GREENFIELD_TERMINATE_GRACE_SECONDS": "0.1",
            }
        )
        wrapper = Path(__file__).resolve().parents[1] / "greenfield-agent"
        process = subprocess.Popen(
            [str(wrapper), "run", "task-1"],
            cwd=self.base,
            env=environment,
            stdin=subprocess.DEVNULL,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            encoding="utf-8",
        )
        self.addCleanup(terminate_process, process)
        wait_for_path(paths.worktree / "codex-arguments.json", process)
        process.terminate()
        stdout, stderr = process.communicate(timeout=10)
        metadata = json.loads(paths.metadata.read_text(encoding="utf-8"))
        self.assertEqual(process.returncode, 143, stderr)
        self.assertIn("blocked-runner", stdout)
        self.assertEqual(metadata["state"], TaskState.BLOCKED_RUNNER.value)
        self.assertEqual(metadata["runner_exit_code"], 143)
        self.assertIn("interrupted by SIGTERM", metadata["runner_error"])
        self.assertIsNotNone(metadata["finished_at"])
        self.assertTrue(paths.worktree.is_dir())
        self.assertTrue((paths.git_after / "status.txt").is_file())

    def test_lock_contention_leaves_task_ready(self) -> None:
        self.create_task()
        self.config.state.mkdir(parents=True, exist_ok=True)
        descriptor = os.open(self.config.lock, os.O_RDWR | os.O_CREAT, 0o640)
        self.addCleanup(os.close, descriptor)
        fcntl.flock(descriptor, fcntl.LOCK_EX | fcntl.LOCK_NB)
        with self.assertRaises(LockContentionError):
            self.runner.run_task("task-1")
        self.assertEqual(
            self.config.task_paths("task-1").state.read_text(encoding="utf-8").strip(),
            "ready",
        )

    def test_git_diagnostics_include_untracked_files_and_unexpected_commit(self) -> None:
        self.create_task()
        metadata = self.run_with_behavior("commit")
        paths = self.config.task_paths("task-1")
        inventory = json.loads(
            (paths.git_after / "untracked-files.json").read_text(encoding="utf-8")
        )["files"]
        self.assertIs(metadata.state, TaskState.REVIEW)
        self.assertEqual(metadata.unexpected_commit_count, 1)
        self.assertIn("untracked.txt", inventory)
        self.assertIn("unexpected agent commit", (paths.git_after / "commits-added.txt").read_text(encoding="utf-8"))
        self.assertIn("committed.txt", (paths.git_after / "expected.diff").read_text(encoding="utf-8"))


class ReportingTests(RunnerTestCase):
    def test_status_and_list_output(self) -> None:
        empty = io.StringIO()
        self.runner.list_tasks(empty)
        self.assertEqual(empty.getvalue(), "No tasks.\n")
        self.create_task()
        self.run_with_behavior("success")
        status_output = io.StringIO()
        self.runner.status("task-1", status_output)
        rendered_status = status_output.getvalue()
        self.assertIn("Task ID", rendered_status)
        self.assertIn("review", rendered_status)
        self.assertIn("Expected commit", rendered_status)
        self.assertIn("Current worktree HEAD", rendered_status)
        self.assertIn("Codex exit code", rendered_status)
        list_output = io.StringIO()
        self.runner.list_tasks(list_output)
        self.assertIn("task-1", list_output.getvalue())
        self.assertIn("review", list_output.getvalue())

    def test_stale_running_task_is_visible_in_status_and_list(self) -> None:
        self.create_task()
        paths = self.config.task_paths("task-1")
        dead_process = subprocess.Popen(["true"])
        dead_process.wait(timeout=5)
        metadata = json.loads(paths.metadata.read_text(encoding="utf-8"))
        metadata["state"] = TaskState.RUNNING.value
        metadata["runner_pid"] = dead_process.pid
        atomic_write_json(paths.metadata, metadata)
        atomic_write_text(paths.state, TaskState.RUNNING.value + "\n")

        status_output = io.StringIO()
        self.assertTrue(self.runner.status("task-1", status_output))
        self.assertIn("task is stale", status_output.getvalue())
        list_output = io.StringIO()
        self.runner.list_tasks(list_output)
        self.assertIn("running-stale", list_output.getvalue())

    def test_partial_task_directory_has_controlled_status_and_list_output(self) -> None:
        partial = self.config.runs / "partial-task"
        partial.mkdir(parents=True)
        status_output = io.StringIO()
        self.assertFalse(self.runner.status("partial-task", status_output))
        self.assertIn("State", status_output.getvalue())
        self.assertIn("invalid", status_output.getvalue())
        list_output = io.StringIO()
        self.runner.list_tasks(list_output)
        self.assertIn("partial-task", list_output.getvalue())
        self.assertIn("invalid", list_output.getvalue())

    def test_ready_status_reports_absent_optional_report(self) -> None:
        self.create_task()
        status_output = io.StringIO()
        self.assertTrue(self.runner.status("task-1", status_output))
        self.assertIn("final-report.md (not present)", status_output.getvalue())

    def test_doctor_with_injected_command_paths(self) -> None:
        output = io.StringIO()
        self.assertTrue(self.runner.doctor(output))
        rendered = output.getvalue()
        self.assertIn("PASS  Python version", rendered)
        self.assertIn("PASS  Command: git", rendered)
        self.assertIn("PASS  Codex version: codex-fake 1.0", rendered)
        self.assertIn("PASS  Codex profile parse", rendered)

        missing_codex_config = dataclasses.replace(
            self.config, codex_command=str(self.base / "missing-codex")
        )
        failing_output = io.StringIO()
        self.assertFalse(GreenfieldRunner(missing_codex_config).doctor(failing_output))
        self.assertIn("FAIL  Command: codex", failing_output.getvalue())


def read_state(paths) -> str:
    return paths.state.read_text(encoding="utf-8").strip()


def wait_for_path(path: Path, process: subprocess.Popen[str]) -> None:
    deadline = time.monotonic() + 10
    while time.monotonic() < deadline:
        if path.exists():
            return
        if process.poll() is not None:
            stdout, stderr = process.communicate()
            raise AssertionError(
                f"runner exited before creating {path}: {stdout}\n{stderr}"
            )
        time.sleep(0.02)
    raise AssertionError(f"timed out waiting for {path}")


def terminate_process(process: subprocess.Popen[str]) -> None:
    if process.poll() is None:
        process.kill()
        process.wait()


if __name__ == "__main__":
    unittest.main()
