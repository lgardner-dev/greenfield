"""Environment-backed runner configuration and task paths."""

from __future__ import annotations

import os
from dataclasses import dataclass
from pathlib import Path


DEFAULT_ROOT = Path("/srv/greenfield")
DEFAULT_REPOSITORY = Path("/srv/greenfield/repos/greenfield")


@dataclass(frozen=True)
class RunnerConfig:
    """All filesystem and executable dependencies used by the runner."""

    root: Path
    repository: Path
    runs: Path
    worktrees: Path
    state: Path
    lock: Path
    git_command: str = "git"
    codex_command: str = "codex"
    bash_command: str = "bash"
    validation_timeout_seconds: int = 3600
    terminate_grace_seconds: float = 5.0

    @classmethod
    def from_environment(cls) -> "RunnerConfig":
        root = Path(os.environ.get("GREENFIELD_ROOT", str(DEFAULT_ROOT)))
        repository = Path(os.environ.get("GREENFIELD_REPOSITORY", str(DEFAULT_REPOSITORY)))
        state = Path(os.environ.get("GREENFIELD_STATE", str(root / "state")))
        return cls(
            root=root,
            repository=repository,
            runs=Path(os.environ.get("GREENFIELD_RUNS", str(root / "runs"))),
            worktrees=Path(os.environ.get("GREENFIELD_WORKTREES", str(root / "worktrees"))),
            state=state,
            lock=Path(os.environ.get("GREENFIELD_LOCK", str(state / "agent.lock"))),
            git_command=os.environ.get("GREENFIELD_GIT", "git"),
            codex_command=os.environ.get("GREENFIELD_CODEX", "codex"),
            bash_command=os.environ.get("GREENFIELD_BASH", "bash"),
            validation_timeout_seconds=_positive_integer_environment(
                "GREENFIELD_VALIDATION_TIMEOUT_SECONDS", 3600
            ),
            terminate_grace_seconds=_positive_float_environment(
                "GREENFIELD_TERMINATE_GRACE_SECONDS", 5.0
            ),
        )

    def task_paths(self, task_id: str) -> "TaskPaths":
        return TaskPaths(
            task_id=task_id,
            run=self.runs / task_id,
            worktree=self.worktrees / task_id,
        )


@dataclass(frozen=True)
class TaskPaths:
    """Filesystem layout for one task."""

    task_id: str
    run: Path
    worktree: Path

    @property
    def metadata(self) -> Path:
        return self.run / "metadata.json"

    @property
    def state(self) -> Path:
        return self.run / "state"

    @property
    def prompt(self) -> Path:
        return self.run / "prompt.md"

    @property
    def validation(self) -> Path:
        return self.run / "validation.sh"

    @property
    def expected_ref(self) -> Path:
        return self.run / "expected-ref"

    @property
    def timeout(self) -> Path:
        return self.run / "timeout-seconds"

    @property
    def reasoning_effort(self) -> Path:
        return self.run / "reasoning-effort"

    @property
    def allow_network(self) -> Path:
        return self.run / "allow-network"

    @property
    def logs(self) -> Path:
        return self.run / "logs"

    @property
    def git(self) -> Path:
        return self.run / "git"

    @property
    def git_before(self) -> Path:
        return self.git / "before"

    @property
    def git_after(self) -> Path:
        return self.git / "after"

    @property
    def report(self) -> Path:
        return self.run / "final-report.md"

    @property
    def runner_errors(self) -> Path:
        return self.logs / "runner-errors.log"


def _positive_integer_environment(name: str, default: int) -> int:
    value = int(os.environ.get(name, str(default)))
    if value <= 0:
        raise ValueError(f"{name} must be greater than zero")
    return value


def _positive_float_environment(name: str, default: float) -> float:
    value = float(os.environ.get(name, str(default)))
    if value <= 0:
        raise ValueError(f"{name} must be greater than zero")
    return value
