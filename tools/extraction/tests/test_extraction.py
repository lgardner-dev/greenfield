import os
import subprocess
import tempfile
import unittest
from pathlib import Path


SCRIPT_DIRECTORY = Path(__file__).resolve().parents[1]
EXTRACT_SCRIPT = SCRIPT_DIRECTORY / "extract-software-factory.sh"
AUDIT_SCRIPT = SCRIPT_DIRECTORY / "audit-filtered-history.sh"


def run_command(*arguments, cwd=None, check=True):
    return subprocess.run(
        [str(argument) for argument in arguments],
        cwd=cwd,
        check=check,
        text=True,
        capture_output=True,
        env={**os.environ, "GIT_CONFIG_NOSYSTEM": "1", "GIT_TERMINAL_PROMPT": "0"},
    )


def git(repository, *arguments, check=True):
    return run_command("git", "-C", repository, *arguments, check=check)


class ExtractionTests(unittest.TestCase):
    def setUp(self):
        self.temporary_directory = tempfile.TemporaryDirectory(prefix="software-factory-e1-")
        self.root = Path(self.temporary_directory.name)
        self.source = self.root / "source"
        self._create_source_repository()

    def tearDown(self):
        self.temporary_directory.cleanup()

    def _create_source_repository(self):
        self.source.mkdir()
        run_command("git", "init", "-b", "main", self.source)
        git(self.source, "config", "user.name", "E1 Fixture Author")
        git(self.source, "config", "user.email", "e1-fixture@example.invalid")
        git(self.source, "remote", "add", "origin", "https://example.invalid/greenfield.git")

        self._write_files(
            {
                "tools/runner/README.md": "runner history one\n",
                "tools/graphics_broker/README.md": "broker history one\n",
                "docs/phase2-restricted-graphics-launcher.md": "phase two history one\n",
                "docs/software-factory-extraction.md": "extraction history one\n",
                "src/application.cpp": "excluded application history\n",
            }
        )
        self._commit("Add initial runner, broker, docs, and excluded application")
        self._write_files(
            {
                "tools/runner/README.md": "runner history two\n",
                "tools/graphics_broker/README.md": "broker history two\n",
                "src/application.cpp": "excluded application history two\n",
            }
        )
        self._commit("Update retained history and excluded application")
        self._write_files({"tools/runner/branch-only.txt": "branch history\n"})
        self._commit("Add branch-only retained file")
        git(self.source, "branch", "feature/e1")
        git(self.source, "tag", "e1-fixture")
        git(self.source, "checkout", "main")
        self.source_commit = git(self.source, "rev-parse", "HEAD").stdout.strip()

        self.source_status = git(self.source, "status", "--porcelain=v1").stdout
        self.source_refs = git(self.source, "show-ref").stdout
        self.source_remotes = git(self.source, "remote", "-v").stdout

    def _write_files(self, files):
        for relative_path, contents in files.items():
            path = self.source / relative_path
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text(contents, encoding="utf-8")

    def _commit(self, message):
        git(self.source, "add", ".")
        git(self.source, "commit", "-m", message)

    def _extract(self, output_directory, check=True):
        return run_command(
            EXTRACT_SCRIPT,
            self.source,
            output_directory,
            cwd=self.root,
            check=check,
        )

    def _audit(self, repository, check=True):
        return run_command(AUDIT_SCRIPT, repository, cwd=self.root, check=check)

    def test_two_independent_extractions_are_identical_and_source_is_unchanged(self):
        first_output = self.root / "first-output"
        second_output = self.root / "second-output"

        first_result = self._extract(first_output)
        second_result = self._extract(second_output)
        first_audit = self._audit(first_output)
        second_audit = self._audit(second_output)

        self.assertEqual(first_result.returncode, 0, first_result.stderr)
        self.assertEqual(second_result.returncode, 0, second_result.stderr)
        self.assertEqual(first_audit.returncode, 0, first_audit.stderr)
        self.assertEqual(second_audit.returncode, 0, second_audit.stderr)

        for output in (first_output, second_output):
            self.assertEqual(git(output, "remote").stdout, "")
            self.assertFalse((output / ".git/objects/info/alternates").exists())

        self.assertEqual(
            git(first_output, "rev-parse", "HEAD").stdout,
            git(second_output, "rev-parse", "HEAD").stdout,
        )
        self.assertEqual(
            git(first_output, "for-each-ref", "--format=%(refname) %(objectname)").stdout,
            git(second_output, "for-each-ref", "--format=%(refname) %(objectname)").stdout,
        )
        self.assertEqual(
            git(first_output, "rev-list", "--all", "--count").stdout,
            git(second_output, "rev-list", "--all", "--count").stdout,
        )
        self.assertEqual(
            git(first_output, "ls-tree", "-r", "--name-only", "HEAD").stdout,
            git(second_output, "ls-tree", "-r", "--name-only", "HEAD").stdout,
        )
        self.assertNotIn(
            "src/application.cpp",
            git(first_output, "ls-tree", "-r", "--name-only", "HEAD").stdout,
        )
        self.assertIn("TAG=e1-fixture", first_audit.stdout)
        self.assertIn("BRANCH_COUNT=1", first_audit.stdout)
        self.assertIn("BRANCH=main", first_audit.stdout)
        self.assertNotIn("BRANCH=feature/e1", first_audit.stdout)

        self.assertEqual(git(self.source, "rev-parse", "HEAD").stdout.strip(), self.source_commit)
        self.assertEqual(git(self.source, "status", "--porcelain=v1").stdout, self.source_status)
        self.assertEqual(git(self.source, "show-ref").stdout, self.source_refs)
        self.assertEqual(git(self.source, "remote", "-v").stdout, self.source_remotes)

    def test_output_directory_refusal_and_invalid_source(self):
        output_directory = self.root / "output"
        self._extract(output_directory)

        existing_result = self._extract(output_directory, check=False)
        self.assertNotEqual(existing_result.returncode, 0)
        self.assertIn("already exists", existing_result.stderr)

        empty_output = self.root / "empty-output"
        empty_output.mkdir()
        empty_result = self._extract(empty_output, check=False)
        self.assertNotEqual(empty_result.returncode, 0)
        self.assertIn("already exists", empty_result.stderr)

        invalid_result = run_command(
            EXTRACT_SCRIPT,
            self.root / "does-not-exist",
            self.root / "invalid-output",
            cwd=self.root,
            check=False,
        )
        self.assertNotEqual(invalid_result.returncode, 0)
        self.assertIn("not a non-bare Git repository", invalid_result.stderr)
        self.assertFalse((self.root / "invalid-output").exists())

        unsupported_result = run_command(
            EXTRACT_SCRIPT,
            "--source",
            self.source,
            cwd=self.root,
            check=False,
        )
        self.assertEqual(unsupported_result.returncode, 2)

    def test_audit_detects_disallowed_fixture_path(self):
        output_directory = self.root / "output"
        self._extract(output_directory)

        malicious = self.root / "malicious"
        run_command("git", "clone", "--no-local", "--no-hardlinks", output_directory, malicious)
        git(malicious, "config", "user.name", "E1 Fixture Author")
        git(malicious, "config", "user.email", "e1-fixture@example.invalid")
        (malicious / "unapproved.txt").write_text("deliberately disallowed\n", encoding="utf-8")
        git(malicious, "add", "unapproved.txt")
        git(malicious, "commit", "-m", "Add disallowed fixture path")
        git(malicious, "remote", "remove", "origin")

        audit_result = self._audit(malicious, check=False)
        self.assertNotEqual(audit_result.returncode, 0)
        self.assertIn("PATH_VIOLATION=unapproved.txt", audit_result.stdout)

    def test_audit_rejects_configured_remote(self):
        output_directory = self.root / "output"
        self._extract(output_directory)
        git(output_directory, "remote", "add", "unsafe", "https://example.invalid/unsafe.git")

        audit_result = self._audit(output_directory, check=False)
        self.assertNotEqual(audit_result.returncode, 0)
        self.assertIn("REMOTE_COUNT=1", audit_result.stdout)
        self.assertIn("REMOTE=unsafe", audit_result.stdout)

    def test_audit_detects_high_confidence_secret_without_printing_value(self):
        output_directory = self.root / "output"
        self._extract(output_directory)

        malicious = self.root / "secret-fixture"
        run_command("git", "clone", "--no-local", "--no-hardlinks", output_directory, malicious)
        git(malicious, "config", "user.name", "E1 Fixture Author")
        git(malicious, "config", "user.email", "e1-fixture@example.invalid")
        secret_value = "ghp_123456789012345678901234567890123456"
        secret_path = malicious / "tools/runner/secret-fixture.txt"
        secret_path.write_text(f"token={secret_value}\n", encoding="utf-8")
        git(malicious, "add", secret_path.relative_to(malicious))
        git(malicious, "commit", "-m", "Add secret fixture")
        git(malicious, "remote", "remove", "origin")

        audit_result = self._audit(malicious, check=False)
        self.assertNotEqual(audit_result.returncode, 0)
        self.assertIn("LIKELY_TOKEN=tools/runner/secret-fixture.txt", audit_result.stdout)
        self.assertNotIn(secret_value, audit_result.stdout)
        self.assertNotIn(secret_value, audit_result.stderr)


if __name__ == "__main__":
    unittest.main()
