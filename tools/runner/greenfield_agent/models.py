"""Persistent task model and atomic serialization helpers."""

from __future__ import annotations

import json
import os
import stat
import tempfile
from dataclasses import asdict, dataclass
from enum import Enum
from pathlib import Path
from typing import Any


class TaskState(str, Enum):
    """Every durable state understood by the initial runner."""

    READY = "ready"
    RUNNING = "running"
    REVIEW = "review"
    BLOCKED_CODEX = "blocked-codex"
    BLOCKED_VALIDATION = "blocked-validation"
    BLOCKED_RUNNER = "blocked-runner"


TERMINAL_STATES = frozenset(
    {
        TaskState.REVIEW,
        TaskState.BLOCKED_CODEX,
        TaskState.BLOCKED_VALIDATION,
        TaskState.BLOCKED_RUNNER,
    }
)

VALID_TRANSITIONS = {
    TaskState.READY: frozenset({TaskState.RUNNING}),
    TaskState.RUNNING: TERMINAL_STATES,
}


@dataclass
class TaskMetadata:
    """Durable task metadata stored in ``metadata.json``."""

    task_id: str
    state: TaskState
    expected_ref: str
    worktree_path: str
    report_path: str
    created_at: str
    updated_at: str | None = None
    expected_commit: str | None = None
    started_at: str | None = None
    finished_at: str | None = None
    duration_seconds: float | None = None
    runner_pid: int | None = None
    runner_exit_code: int | None = None
    codex_exit_code: int | None = None
    codex_timed_out: bool = False
    validation_exit_code: int | None = None
    validation_timed_out: bool = False
    current_head: str | None = None
    unexpected_commit_count: int | None = None
    runner_error: str | None = None

    def to_dict(self) -> dict[str, Any]:
        data = asdict(self)
        data["state"] = self.state.value
        return data

    @classmethod
    def from_dict(cls, data: dict[str, Any]) -> "TaskMetadata":
        values = dict(data)
        values["state"] = TaskState(values["state"])
        return cls(**values)


def transition_state(current: TaskState, requested: TaskState) -> TaskState:
    """Validate and return one state transition."""

    if requested not in VALID_TRANSITIONS.get(current, frozenset()):
        raise ValueError(f"invalid task state transition: {current.value} -> {requested.value}")
    return requested


def atomic_write_text(path: Path, content: str, mode: int = 0o644) -> None:
    """Replace a text file atomically and flush the replacement to disk."""

    atomic_write_bytes(path, content.encode("utf-8"), mode)


def atomic_write_bytes(path: Path, content: bytes, mode: int = 0o644) -> None:
    """Replace a binary file atomically and flush the replacement to disk."""

    path.parent.mkdir(parents=True, exist_ok=True)
    descriptor, temporary_name = tempfile.mkstemp(prefix=f".{path.name}.", dir=path.parent)
    temporary_path = Path(temporary_name)
    try:
        with os.fdopen(descriptor, "wb") as stream:
            os.fchmod(stream.fileno(), mode)
            stream.write(content)
            stream.flush()
            os.fsync(stream.fileno())
        os.replace(temporary_path, path)
        _flush_directory(path.parent)
    except BaseException:
        temporary_path.unlink(missing_ok=True)
        raise


def atomic_write_json(path: Path, data: dict[str, Any]) -> None:
    """Serialize JSON deterministically through atomic file replacement."""

    atomic_write_text(path, json.dumps(data, indent=2, sort_keys=True) + "\n")


def read_json(path: Path) -> dict[str, Any]:
    """Load a JSON object from a non-symlink regular file."""

    content = read_text_file(path)
    value = json.loads(content)
    if not isinstance(value, dict):
        raise ValueError(f"expected a JSON object in {path}")
    return value


def read_text_file(path: Path) -> str:
    """Read a regular file without following a final-component symlink."""

    flags = os.O_RDONLY
    if hasattr(os, "O_NOFOLLOW"):
        flags |= os.O_NOFOLLOW
    if hasattr(os, "O_NONBLOCK"):
        flags |= os.O_NONBLOCK
    descriptor = os.open(path, flags)
    if not stat.S_ISREG(os.fstat(descriptor).st_mode):
        os.close(descriptor)
        raise OSError(f"not a regular file: {path}")
    with os.fdopen(descriptor, "r", encoding="utf-8") as stream:
        return stream.read()


def _flush_directory(directory: Path) -> None:
    flags = os.O_RDONLY
    if hasattr(os, "O_DIRECTORY"):
        flags |= os.O_DIRECTORY
    descriptor = os.open(directory, flags)
    try:
        os.fsync(descriptor)
    finally:
        os.close(descriptor)
