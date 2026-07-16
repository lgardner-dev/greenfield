from __future__ import annotations

import fcntl
import os
import shutil
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path


class InstallerTests(unittest.TestCase):
    def setUp(self) -> None:
        self.temporary_directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary_directory.cleanup)
        self.base = Path(self.temporary_directory.name)
        self.source_root = self.base / "source"
        runner_source = Path(__file__).resolve().parents[1]
        shutil.copytree(
            runner_source,
            self.source_root / "tools" / "runner",
            ignore=shutil.ignore_patterns("__pycache__", "*.pyc"),
        )
        self.installer = self.source_root / "tools" / "runner" / "install.sh"
        self._create_git_checkout()
        self.prefix = self.base / "prefix"
        self.unit_directory = self.base / "systemd-user"

    def _create_git_checkout(self) -> None:
        self._run(["git", "init", "-q", str(self.source_root)])
        self._run(["git", "-C", str(self.source_root), "config", "user.name", "Installer Tests"])
        self._run(
            [
                "git",
                "-C",
                str(self.source_root),
                "config",
                "user.email",
                "installer-tests@example.invalid",
            ]
        )
        self._run(["git", "-C", str(self.source_root), "add", "tools/runner"])
        self._run(
            [
                "git",
                "-C",
                str(self.source_root),
                "commit",
                "-qm",
                "installer fixture",
            ]
        )

    def _run(
        self,
        arguments: list[str],
        *,
        cwd: Path | None = None,
        environment: dict[str, str] | None = None,
    ) -> subprocess.CompletedProcess[str]:
        return subprocess.run(
            arguments,
            cwd=cwd,
            env=environment,
            stdin=subprocess.DEVNULL,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            encoding="utf-8",
            check=False,
        )

    def _install(
        self,
        version: str | None,
        *,
        allow_dirty: bool = False,
        systemd_directory: Path | None = None,
    ) -> subprocess.CompletedProcess[str]:
        arguments = [
            str(self.installer),
            "--prefix",
            str(self.prefix),
            "--systemd-user-dir",
            str(systemd_directory or self.unit_directory),
            "--skip-owner-change",
        ]
        if version is not None:
            arguments[5:5] = ["--version", version]
        if allow_dirty:
            arguments.append("--allow-dirty")
        return self._run(arguments, cwd=self.base)

    def _current(self) -> Path:
        return self.prefix / "lib" / "greenfield-runner" / "current"

    def _isolated_runner_environment(self) -> dict[str, str]:
        root = self.base / "test-runner"
        state = root / "state"
        return {
            **os.environ,
            "GREENFIELD_ROOT": str(root),
            "GREENFIELD_REPOSITORY": str(self.source_root),
            "GREENFIELD_RUNS": str(root / "runs"),
            "GREENFIELD_WORKTREES": str(root / "worktrees"),
            "GREENFIELD_STATE": str(state),
            "GREENFIELD_LOCK": str(state / "agent.lock"),
        }

    def _run_wrapper(
        self,
        wrapper: Path,
        arguments: list[str],
        *,
        cwd: Path = Path("/tmp"),
    ) -> subprocess.CompletedProcess[str]:
        return self._run(
            [str(wrapper), *arguments],
            cwd=cwd,
            environment=self._isolated_runner_environment(),
        )

    def test_fresh_install_has_importable_release_and_rendered_unit(self) -> None:
        completed = self._install("test-one")
        self.assertEqual(completed.returncode, 0, completed.stderr)

        current = self._current()
        self.assertTrue(current.is_symlink())
        self.assertEqual(os.readlink(current), "releases/test-one")
        self.assertEqual((current / "VERSION").read_text(encoding="utf-8").strip(), "test-one")
        metadata = (current / "INSTALL-METADATA").read_text(encoding="utf-8")
        self.assertIn("source_commit=", metadata)
        self.assertIn("installed_at=", metadata)
        self.assertIn("installer_format=1", metadata)
        self.assertIn(f"source_repository={self.source_root}", metadata)
        self.assertIn("dirty_install_allowed=false", metadata)

        wrapper = self.prefix / "bin" / "greenfield-agent"
        self.assertTrue(wrapper.is_file())
        self.assertTrue(os.access(wrapper, os.X_OK))
        help_result = self._run_wrapper(wrapper, ["--help"])
        self.assertEqual(help_result.returncode, 0, help_result.stderr)
        self.assertIn("greenfield-agent", help_result.stdout)

        import_environment = os.environ.copy()
        import_environment.pop("PYTHONPATH", None)
        import_environment["PYTHONPATH"] = str(current)
        import_result = self._run(
            [sys.executable, "-c", "import greenfield_agent; print(greenfield_agent.__file__)"],
            cwd=Path("/tmp"),
            environment=import_environment,
        )
        self.assertEqual(import_result.returncode, 0, import_result.stderr)
        self.assertIn("greenfield_agent/__init__.py", import_result.stdout)

        unit = self.unit_directory / "greenfield-task@.service"
        self.assertTrue(unit.is_file())
        rendered = unit.read_text(encoding="utf-8")
        self.assertIn(f"WorkingDirectory={self.prefix}", rendered)
        self.assertIn(f"ExecStart={wrapper} run %i", rendered)
        self.assertIn("KillMode=mixed", rendered)
        self.assertNotIn("User=", rendered)
        self.assertNotIn("Group=", rendered)

        doctor = self._run_wrapper(wrapper, ["doctor"])
        self.assertIn("PASS  Installed version: test-one", doctor.stdout)
        self.assertIn("INFO  Service unit: present", doctor.stdout)

    def test_default_version_uses_the_git_commit(self) -> None:
        completed = self._install(None)
        self.assertEqual(completed.returncode, 0, completed.stderr)
        expected = self._run(
            ["git", "-C", str(self.source_root), "rev-parse", "--short=12", "HEAD"]
        ).stdout.strip()
        self.assertEqual((self._current() / "VERSION").read_text(encoding="utf-8").strip(), expected)

    def test_reinstall_and_upgrade_preserve_releases(self) -> None:
        first = self._install("test-one")
        self.assertEqual(first.returncode, 0, first.stderr)
        same = self._install("test-one")
        self.assertEqual(same.returncode, 0, same.stderr)
        self.assertTrue((self.prefix / "lib" / "greenfield-runner" / "releases" / "test-one").is_dir())

        second = self._install("test-two")
        self.assertEqual(second.returncode, 0, second.stderr)
        self.assertEqual(os.readlink(self._current()), "releases/test-two")
        self.assertTrue((self.prefix / "lib" / "greenfield-runner" / "releases" / "test-one").is_dir())
        self.assertTrue((self.prefix / "lib" / "greenfield-runner" / "releases" / "test-two").is_dir())

    def test_failed_install_does_not_move_current(self) -> None:
        first = self._install("test-one")
        self.assertEqual(first.returncode, 0, first.stderr)
        invalid_unit_directory = self.base / "unit-file"
        invalid_unit_directory.write_text("not a directory\n", encoding="utf-8")

        failed = self._install("test-two", systemd_directory=invalid_unit_directory)
        self.assertNotEqual(failed.returncode, 0)
        self.assertEqual(os.readlink(self._current()), "releases/test-one")

    def test_current_external_symlink_is_replaced_without_following_it(self) -> None:
        first = self._install("test-one")
        self.assertEqual(first.returncode, 0, first.stderr)
        external = self.base / "external"
        external.mkdir()
        current = self._current()
        current.unlink()
        current.symlink_to(external)

        second = self._install("test-two")
        self.assertEqual(second.returncode, 0, second.stderr)
        self.assertEqual(os.readlink(current), "releases/test-two")
        self.assertFalse((external / "greenfield_agent").exists())

    def test_option_validation_and_dirty_checkout_behavior(self) -> None:
        for arguments in (
            ["--unknown"],
            ["--prefix"],
            ["--version"],
            ["--systemd-user-dir"],
            ["--prefix", ""],
            ["--prefix", "/"],
        ):
            completed = self._run([str(self.installer), *arguments], cwd=self.base)
            self.assertNotEqual(completed.returncode, 0, arguments)

        dirty_marker = self.source_root / "tools" / "runner" / "DIRTY-MARKER"
        dirty_marker.write_text("dirty\n", encoding="utf-8")
        refused = self._install("dirty-version")
        self.assertNotEqual(refused.returncode, 0)
        self.assertIn("dirty", refused.stderr)

        allowed = self._install("dirty-version", allow_dirty=True)
        self.assertEqual(allowed.returncode, 0, allowed.stderr)
        metadata = (self._current() / "INSTALL-METADATA").read_text(encoding="utf-8")
        self.assertIn("dirty_install_allowed=true", metadata)

    def test_wrapper_propagates_runner_exit_code_from_unrelated_directory(self) -> None:
        completed = self._install("test-one")
        self.assertEqual(completed.returncode, 0, completed.stderr)
        wrapper = self.prefix / "bin" / "greenfield-agent"
        result = self._run_wrapper(wrapper, ["run", "missing-task"])
        self.assertEqual(result.returncode, 1)
        self.assertIn("task does not exist", result.stderr)

    def test_wrapper_uses_isolated_lock_while_outer_runner_lock_is_held(self) -> None:
        completed = self._install("test-one")
        self.assertEqual(completed.returncode, 0, completed.stderr)
        wrapper = self.prefix / "bin" / "greenfield-agent"

        outer_lock = self.base / "outer-runner" / "state" / "agent.lock"
        outer_lock.parent.mkdir(parents=True)
        with outer_lock.open("a+") as lock_file:
            fcntl.flock(lock_file.fileno(), fcntl.LOCK_EX | fcntl.LOCK_NB)
            environment = self._isolated_runner_environment()
            self.assertNotEqual(Path(environment["GREENFIELD_LOCK"]), outer_lock)
            result = self._run(
                [str(wrapper), "run", "missing-task"],
                cwd=Path("/tmp"),
                environment=environment,
            )

        self.assertEqual(result.returncode, 1)
        self.assertIn("task does not exist", result.stderr)
        self.assertNotIn("another Greenfield task is already running", result.stderr)
        self.assertTrue(Path(environment["GREENFIELD_LOCK"]).is_file())


if __name__ == "__main__":
    unittest.main()
