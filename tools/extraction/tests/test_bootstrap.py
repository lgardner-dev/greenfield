import hashlib
import json
import os
import subprocess
import tempfile
import unittest
from pathlib import Path


SCRIPT_DIRECTORY = Path(__file__).resolve().parents[1]
BOOTSTRAP_SCRIPT = SCRIPT_DIRECTORY / "prepare-software-factory-bootstrap.sh"
AUDIT_SCRIPT = SCRIPT_DIRECTORY / "audit-filtered-history.sh"
GENERATED_ALLOWLIST = SCRIPT_DIRECTORY / "bootstrap-generated-paths.txt"
PROVENANCE_PREFIX = "docs/provenance/"
TIMESTAMP = "2024-06-15T12:34:56Z"
REPOSITORY_ID = "lgardner-dev/greenfield"
PROVENANCE_MESSAGE = "Record software factory extraction provenance"


def run_command(*arguments, cwd=None, check=True, extra_environment=None):
    environment = {
        **os.environ,
        "GIT_CONFIG_NOSYSTEM": "1",
        "GIT_TERMINAL_PROMPT": "0",
    }
    if extra_environment:
        environment.update(extra_environment)
    return subprocess.run(
        [str(argument) for argument in arguments],
        cwd=cwd,
        check=check,
        text=True,
        capture_output=True,
        env=environment,
    )


def git(repository, *arguments, check=True):
    return run_command("git", "-C", repository, *arguments, check=check)


def sha256(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


class BootstrapTests(unittest.TestCase):
    def setUp(self):
        self.temporary_directory = tempfile.TemporaryDirectory(prefix="software-factory-e2-")
        self.root = Path(self.temporary_directory.name)
        self.source = self.root / "source"
        self._create_source_repository()

    def tearDown(self):
        self.temporary_directory.cleanup()

    def _create_source_repository(self):
        self.source.mkdir()
        run_command("git", "init", "-b", "main", self.source)
        git(self.source, "config", "user.name", "E2 Fixture Author")
        git(self.source, "config", "user.email", "e2-fixture@example.invalid")
        git(self.source, "remote", "add", "origin", "https://example.invalid/greenfield.git")

        self._write_source_files(
            {
                "tools/runner/README.md": "runner history one\n",
                "tools/graphics_broker/README.md": "broker history one\n",
                "docs/phase2-restricted-graphics-launcher.md": "phase two history one\n",
                "docs/software-factory-extraction.md": "extraction history one\n",
                "src/application.cpp": "excluded application history\n",
            }
        )
        self._commit("Add initial runner, broker, docs, and excluded application", "2023-01-01T00:00:00Z")
        self._write_source_files(
            {
                "tools/runner/README.md": "runner history two\n",
                "tools/graphics_broker/README.md": "broker history two\n",
                "src/application.cpp": "excluded application history two\n",
            }
        )
        self._commit("Update retained history and excluded application", "2023-02-01T00:00:00Z")
        self._write_source_files({"tools/runner/branch-only.txt": "branch history\n"})
        self._commit("Add branch-only retained file", "2023-03-01T00:00:00Z")
        git(self.source, "branch", "feature/e2")
        git(self.source, "tag", "e2-fixture")
        git(self.source, "checkout", "main")

        self.source_head = git(self.source, "rev-parse", "HEAD").stdout.strip()
        self.source_status = git(self.source, "status", "--porcelain=v1").stdout
        self.source_refs = git(self.source, "for-each-ref", "--format=%(refname) %(objectname)").stdout
        self.source_remotes = git(self.source, "remote", "-v").stdout

    def _write_source_files(self, files):
        for relative_path, contents in files.items():
            path = self.source / relative_path
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text(contents, encoding="utf-8")

    def _commit(self, message, timestamp):
        git(self.source, "add", ".")
        run_command(
            "git",
            "-C",
            self.source,
            "commit",
            "-m",
            message,
            cwd=self.root,
            check=True,
            extra_environment={
                "GIT_AUTHOR_DATE": timestamp,
                "GIT_COMMITTER_DATE": timestamp,
            },
        )

    def _bootstrap(self, output, check=True):
        return run_command(
            BOOTSTRAP_SCRIPT,
            self.source,
            output,
            REPOSITORY_ID,
            TIMESTAMP,
            cwd=self.root,
            check=check,
        )

    def _audit(self, repository, check=True):
        return run_command(
            AUDIT_SCRIPT,
            repository,
            GENERATED_ALLOWLIST,
            cwd=self.root,
            check=check,
        )

    def _summary(self, result):
        return {
            line.split("=", 1)[0]: line.split("=", 1)[1]
            for line in result.stdout.splitlines()
            if "=" in line
        }

    def _create_candidate_clone(self, source_candidate, name):
        candidate = self.root / name
        run_command("git", "clone", "--no-local", "--no-hardlinks", source_candidate, candidate)
        git(candidate, "remote", "remove", "origin")
        git(candidate, "config", "user.name", "E2 Fixture Author")
        git(candidate, "config", "user.email", "e2-fixture@example.invalid")
        return candidate

    def test_two_candidates_are_byte_and_history_identical(self):
        first_result = self._bootstrap(self.root / "first-candidate")
        second_result = self._bootstrap(self.root / "second-candidate")
        first = self.root / "first-candidate"
        second = self.root / "second-candidate"
        first_summary = self._summary(first_result)
        second_summary = self._summary(second_result)

        self.assertEqual(first_result.returncode, 0, first_result.stderr)
        self.assertEqual(second_result.returncode, 0, second_result.stderr)
        self.assertEqual(first_summary["PRE_PROVENANCE_FILTERED_HEAD"], second_summary["PRE_PROVENANCE_FILTERED_HEAD"])
        self.assertEqual(first_summary["FINAL_BOOTSTRAP_HEAD"], second_summary["FINAL_BOOTSTRAP_HEAD"])
        self.assertEqual(git(first, "rev-parse", "HEAD").stdout, git(second, "rev-parse", "HEAD").stdout)
        self.assertEqual(
            git(first, "for-each-ref", "--format=%(refname) %(objectname)").stdout,
            git(second, "for-each-ref", "--format=%(refname) %(objectname)").stdout,
        )
        self.assertEqual(git(first, "rev-list", "--all", "--objects").stdout, git(second, "rev-list", "--all", "--objects").stdout)
        self.assertEqual(git(first, "ls-tree", "-r", "--full-tree", "--name-only", "HEAD").stdout, git(second, "ls-tree", "-r", "--full-tree", "--name-only", "HEAD").stdout)
        self.assertEqual(git(first, "log", "--all", "--format=%H|%P|%an|%ae|%cn|%ce|%aI|%cI|%s").stdout, git(second, "log", "--all", "--format=%H|%P|%an|%ae|%cn|%ce|%aI|%cI|%s").stdout)

        generated_paths = [line for line in GENERATED_ALLOWLIST.read_text().splitlines() if line]
        first_paths = sorted(path[len(PROVENANCE_PREFIX):] for path in git(first, "ls-tree", "-r", "--name-only", "HEAD").stdout.splitlines() if path.startswith(PROVENANCE_PREFIX))
        self.assertEqual(first_paths, sorted(path[len(PROVENANCE_PREFIX):] for path in generated_paths))
        for relative_path in generated_paths:
            self.assertEqual((first / relative_path).read_bytes(), (second / relative_path).read_bytes())
        self.assertEqual(git(first, "remote").stdout, "")
        self.assertEqual(git(second, "remote").stdout, "")
        self.assertEqual(first_result.stderr.count("push"), 0)

    def test_manifest_hashes_identity_and_single_provenance_commit(self):
        output = self.root / "candidate"
        result = self._bootstrap(output)
        self.assertEqual(result.returncode, 0, result.stderr)
        summary = self._summary(result)
        manifest_path = output / "docs/provenance/manifest.json"
        manifest = json.loads(manifest_path.read_text(encoding="utf-8"))

        self.assertEqual(manifest["manifest_schema"], "software-factory-bootstrap-manifest-v1")
        self.assertEqual(manifest["source_repository_id"], REPOSITORY_ID)
        self.assertEqual(manifest["source_commit"], self.source_head)
        self.assertEqual(manifest["source_branch"], "main")
        self.assertEqual(manifest["extraction_timestamp_utc"], TIMESTAMP)
        self.assertEqual(manifest["final_bootstrap_head"], "HEAD")
        self.assertEqual(summary["FINAL_BOOTSTRAP_HEAD"], git(output, "rev-parse", "HEAD").stdout.strip())
        self.assertEqual(manifest["audit_result_sha256"], sha256(output / manifest["audit_result_path"]))
        self.assertEqual(manifest["original_to_filtered_commit_map_sha256"], sha256(output / manifest["original_to_filtered_commit_map_path"]))
        self.assertEqual(manifest["retained_file_inventory_sha256"], sha256(output / manifest["retained_file_inventory_path"]))
        self.assertEqual(manifest["filtered_commit_count"] + 1, int(git(output, "rev-list", "--all", "--count").stdout))
        self.assertEqual(git(output, "branch", "--format=%(refname:short)").stdout, "main\n")
        self.assertEqual(git(output, "tag").stdout, "e2-fixture\n")
        self.assertEqual(git(output, "log", "--all", "--format=%s").stdout.splitlines().count(PROVENANCE_MESSAGE), 1)

        identity = git(output, "show", "-s", "--format=%an|%ae|%cn|%ce|%aI|%cI|%s", "HEAD").stdout.strip()
        self.assertEqual(
            identity,
            "Greenfield Extraction|greenfield-extraction@greenfield.invalid|"
            "Greenfield Extraction|greenfield-extraction@greenfield.invalid|"
            f"{TIMESTAMP}|{TIMESTAMP}|{PROVENANCE_MESSAGE}",
        )
        self.assertEqual(git(output, "remote").stdout, "")
        self.assertFalse((output / ".git/objects/info/alternates").exists())
        self.assertNotIn(self.source.as_posix(), manifest_path.read_text(encoding="utf-8"))
        self.assertNotIn("ghp_", manifest_path.read_text(encoding="utf-8"))

    def test_invalid_ids_timestamps_and_existing_outputs_are_rejected(self):
        for repository_id in ("https://github.com/example/repo", "../repo", "owner", "owner/repo/extra", "owner/repo with spaces"):
            result = run_command(BOOTSTRAP_SCRIPT, self.source, self.root / "invalid-id-output", repository_id, TIMESTAMP, cwd=self.root, check=False)
            self.assertNotEqual(result.returncode, 0, repository_id)
            self.assertFalse((self.root / "invalid-id-output").exists())
        for timestamp in ("2024-06-15T12:34:56+00:00", "2024-02-30T12:34:56Z", "2024-06-15 12:34:56Z"):
            result = run_command(BOOTSTRAP_SCRIPT, self.source, self.root / "invalid-time-output", REPOSITORY_ID, timestamp, cwd=self.root, check=False)
            self.assertNotEqual(result.returncode, 0, timestamp)
            self.assertFalse((self.root / "invalid-time-output").exists())

        existing = self.root / "existing"
        existing.mkdir()
        result = self._bootstrap(existing, check=False)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("already exists", result.stderr)

    def test_missing_or_altered_commit_map_and_undeclared_files_fail_audit(self):
        output = self.root / "candidate"
        self._bootstrap(output)

        missing_map = self._create_candidate_clone(output, "missing-map")
        (missing_map / "docs/provenance/original-to-filtered-commit-map.txt").unlink()
        git(missing_map, "add", "-u")
        git(missing_map, "commit", "-m", "Remove map evidence")
        missing_result = self._audit(missing_map, check=False)
        self.assertNotEqual(missing_result.returncode, 0)
        self.assertIn("GENERATED_PATH_VIOLATION=docs/provenance/original-to-filtered-commit-map.txt", missing_result.stdout)

        altered_map = self._create_candidate_clone(output, "altered-map")
        map_path = altered_map / "docs/provenance/original-to-filtered-commit-map.txt"
        map_path.write_text(map_path.read_text(encoding="utf-8") + "altered\n", encoding="utf-8")
        git(altered_map, "add", map_path.relative_to(altered_map))
        git(altered_map, "commit", "-m", "Alter map evidence")
        altered_result = self._audit(altered_map, check=False)
        self.assertNotEqual(altered_result.returncode, 0)
        self.assertIn("GENERATED_EVIDENCE_MISMATCH=docs/provenance/original-to-filtered-commit-map.txt", altered_result.stdout)

        undeclared = self._create_candidate_clone(output, "undeclared")
        undeclared_path = undeclared / "docs/provenance/undeclared.txt"
        undeclared_path.write_text("not declared\n", encoding="utf-8")
        git(undeclared, "add", undeclared_path.relative_to(undeclared))
        git(undeclared, "commit", "-m", "Add undeclared evidence")
        undeclared_result = self._audit(undeclared, check=False)
        self.assertNotEqual(undeclared_result.returncode, 0)
        self.assertIn("PATH_VIOLATION=docs/provenance/undeclared.txt", undeclared_result.stdout)

    def test_source_is_unchanged_and_tests_run_outside_checkout(self):
        result = self._bootstrap(self.root / "candidate")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(git(self.source, "rev-parse", "HEAD").stdout.strip(), self.source_head)
        self.assertEqual(git(self.source, "status", "--porcelain=v1").stdout, self.source_status)
        self.assertEqual(git(self.source, "for-each-ref", "--format=%(refname) %(objectname)").stdout, self.source_refs)
        self.assertEqual(git(self.source, "remote", "-v").stdout, self.source_remotes)
        self.assertEqual(result.args[0], str(BOOTSTRAP_SCRIPT))
        self.assertNotEqual(self.root, SCRIPT_DIRECTORY)


if __name__ == "__main__":
    unittest.main()
