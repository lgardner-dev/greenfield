"""Command-line interface for the Greenfield task runner."""

from __future__ import annotations

import argparse
import sys
from pathlib import Path
from typing import Sequence

from .paths import RunnerConfig
from .runner import GreenfieldRunner, RunnerError, RunnerInterrupted


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(prog="greenfield-agent")
    subparsers = parser.add_subparsers(dest="command", required=True)

    create = subparsers.add_parser("create", help="create a durable ready task")
    create.add_argument("task_id")
    create.add_argument("expected_ref")
    create.add_argument("prompt_file", type=Path)
    create.add_argument("validation_file", type=Path, nargs="?")

    run = subparsers.add_parser("run", help="run one ready task")
    run.add_argument("task_id")

    status = subparsers.add_parser("status", help="show one task")
    status.add_argument("task_id")

    subparsers.add_parser("list", help="list durable tasks")
    subparsers.add_parser("doctor", help="check runner prerequisites")
    return parser


def main(arguments: Sequence[str] | None = None) -> int:
    parser = build_parser()
    options = parser.parse_args(arguments)
    try:
        runner = GreenfieldRunner(RunnerConfig.from_environment())
        if options.command == "create":
            metadata = runner.create_task(
                options.task_id,
                options.expected_ref,
                options.prompt_file,
                options.validation_file,
            )
            print(f"Created {metadata.task_id}: {metadata.state.value}")
            return 0
        if options.command == "run":
            metadata = runner.run_task(options.task_id)
            print(f"{metadata.task_id}: {metadata.state.value}")
            return metadata.runner_exit_code if metadata.runner_exit_code is not None else 1
        if options.command == "status":
            return 0 if runner.status(options.task_id, sys.stdout) else 1
        if options.command == "list":
            runner.list_tasks(sys.stdout)
            return 0
        if options.command == "doctor":
            return 0 if runner.doctor(sys.stdout) else 1
        parser.error(f"unsupported command: {options.command}")
    except RunnerInterrupted as error:
        print(f"greenfield-agent: {error}", file=sys.stderr)
        return error.exit_code
    except (RunnerError, OSError, ValueError) as error:
        print(f"greenfield-agent: {error}", file=sys.stderr)
        return 1
    return 1
