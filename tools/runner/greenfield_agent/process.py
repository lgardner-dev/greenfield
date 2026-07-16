"""Small subprocess helpers with explicit timeout termination."""

from __future__ import annotations

import os
import signal
import subprocess
import time
from dataclasses import dataclass
from pathlib import Path
from typing import BinaryIO, Mapping, Sequence


@dataclass(frozen=True)
class CommandResult:
    returncode: int
    stdout: str
    stderr: str


@dataclass(frozen=True)
class TimedProcessResult:
    returncode: int
    timed_out: bool
    started_monotonic: float
    finished_monotonic: float

    @property
    def duration_seconds(self) -> float:
        return self.finished_monotonic - self.started_monotonic


def run_command(
    arguments: Sequence[str],
    *,
    cwd: Path | None = None,
    timeout_seconds: float | None = None,
    environment: Mapping[str, str] | None = None,
) -> CommandResult:
    """Run one captured process group without a shell."""

    process = subprocess.Popen(
        list(arguments),
        cwd=cwd,
        env=environment,
        stdin=subprocess.DEVNULL,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
        encoding="utf-8",
        errors="replace",
        shell=False,
        start_new_session=True,
    )
    try:
        stdout, stderr = process.communicate(timeout=timeout_seconds)
    except subprocess.TimeoutExpired:
        _terminate_process_group(process, 1.0)
        raise
    except BaseException:
        _terminate_process_group(process, 1.0)
        raise
    return CommandResult(process.returncode, stdout, stderr)


def run_timed_process(
    arguments: Sequence[str],
    *,
    cwd: Path,
    stdin: BinaryIO,
    stdout: BinaryIO,
    stderr: BinaryIO,
    timeout_seconds: float,
    terminate_grace_seconds: float,
    environment: Mapping[str, str] | None = None,
) -> TimedProcessResult:
    """Run a process group and terminate it cleanly when its timeout expires."""

    started = time.monotonic()
    process: subprocess.Popen[bytes] | None = None
    try:
        process = subprocess.Popen(
            list(arguments),
            cwd=cwd,
            env=environment,
            stdin=stdin,
            stdout=stdout,
            stderr=stderr,
            shell=False,
            start_new_session=True,
        )
        returncode = process.wait(timeout=timeout_seconds)
        timed_out = False
    except subprocess.TimeoutExpired:
        assert process is not None
        timed_out = True
        _terminate_process_group(process, terminate_grace_seconds)
        returncode = 124
    except BaseException:
        if process is not None:
            _terminate_process_group(process, terminate_grace_seconds)
        raise
    finished = time.monotonic()
    return TimedProcessResult(returncode, timed_out, started, finished)


def _terminate_process_group(process: subprocess.Popen[object], grace_seconds: float) -> None:
    termination_started = time.monotonic()
    try:
        os.killpg(process.pid, signal.SIGTERM)
    except ProcessLookupError:
        return

    if process.poll() is None:
        try:
            process.wait(timeout=grace_seconds)
        except subprocess.TimeoutExpired:
            pass

    remaining_grace = grace_seconds - (time.monotonic() - termination_started)
    if remaining_grace > 0 and _process_group_exists(process.pid):
        time.sleep(remaining_grace)
    try:
        os.killpg(process.pid, signal.SIGKILL)
    except ProcessLookupError:
        return
    if process.poll() is None:
        process.wait()


def _process_group_exists(process_group_id: int) -> bool:
    try:
        os.killpg(process_group_id, 0)
    except ProcessLookupError:
        return False
    except PermissionError:
        return True
    return True
