from __future__ import annotations

import contextlib
import io
import os
import subprocess
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

from greenfield_agent.cli import main


class CliTests(unittest.TestCase):
    def test_wrapper_works_outside_the_repository(self) -> None:
        wrapper = Path(__file__).resolve().parents[1] / "greenfield-agent"
        with tempfile.TemporaryDirectory() as temporary_directory:
            completed = subprocess.run(
                [str(wrapper), "--help"],
                cwd=temporary_directory,
                stdin=subprocess.DEVNULL,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                text=True,
                encoding="utf-8",
                check=False,
            )
        self.assertEqual(completed.returncode, 0, completed.stderr)
        self.assertIn("greenfield-agent", completed.stdout)

    def test_direct_test_command_is_location_independent(self) -> None:
        test_command = Path(__file__).resolve().parents[1] / "run-tests"
        with tempfile.TemporaryDirectory() as temporary_directory:
            completed = subprocess.run(
                [str(test_command), "--help"],
                cwd=temporary_directory,
                stdin=subprocess.DEVNULL,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                text=True,
                encoding="utf-8",
                check=False,
            )
        self.assertEqual(completed.returncode, 0, completed.stderr)
        self.assertIn("unittest", completed.stdout)

    def test_create_status_and_list_use_environment_configuration(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            repository = root / "repository"
            repository.mkdir()
            prompt = root / "prompt.md"
            prompt.write_text("Do the narrow task.\n", encoding="utf-8")
            environment = {
                "GREENFIELD_ROOT": str(root / "greenfield"),
                "GREENFIELD_REPOSITORY": str(repository),
            }
            output = io.StringIO()
            with patch.dict(os.environ, environment, clear=False):
                with contextlib.redirect_stdout(output):
                    self.assertEqual(main(["create", "cli-task", "main", str(prompt)]), 0)
                    self.assertEqual(main(["status", "cli-task"]), 0)
                    self.assertEqual(main(["list"]), 0)
            rendered = output.getvalue()
            self.assertIn("Created cli-task: ready", rendered)
            self.assertIn("Task ID", rendered)
            self.assertIn("cli-task", rendered)
            self.assertIn("ready", rendered)

    def test_cli_reports_invalid_create_input(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            environment = {
                "GREENFIELD_ROOT": str(root / "greenfield"),
                "GREENFIELD_REPOSITORY": str(root / "repository"),
            }
            error_output = io.StringIO()
            with patch.dict(os.environ, environment, clear=False):
                with contextlib.redirect_stderr(error_output):
                    result = main(["create", "bad/id", "main", str(root / "missing")])
            self.assertEqual(result, 1)
            self.assertIn("invalid task ID", error_output.getvalue())


if __name__ == "__main__":
    unittest.main()
