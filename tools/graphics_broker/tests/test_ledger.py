from __future__ import annotations

import hashlib
import os
import tempfile
import unittest
from pathlib import Path

from greenfield_graphics_broker import ErrorCode, IdempotencyLedger, LaunchRequest, ProtocolError


REQUEST_DATA = {
    "protocol_version": 1,
    "operation": "launch-visual-review",
    "request_id": "123e4567-e89b-12d3-a456-426614174000",
    "idempotency_key": "task-12-attempt-1",
    "task_id": "Task-12",
    "review_id": "123e4567-e89b-12d3-a456-426614174001",
    "profile_id": "control-room-webgpu",
    "workspace_id": "runner-task-worktree",
    "capture_plan_id": "control-room-1280x720",
    "deadline_seconds": 180,
}


class LedgerTests(unittest.TestCase):
    def setUp(self) -> None:
        self.temporary_directory = tempfile.TemporaryDirectory()
        self.root = Path(self.temporary_directory.name) / "ledger"
        self.request = LaunchRequest.from_dict(REQUEST_DATA)

    def tearDown(self) -> None:
        self.temporary_directory.cleanup()

    def test_exact_replay_returns_original_result(self) -> None:
        ledger = IdempotencyLedger(self.root)
        first = ledger.accept(self.request, {"status": "accepted", "attempt": 1})
        replay = ledger.accept(self.request, {"status": "different"})
        self.assertEqual(replay, first)
        self.assertEqual(replay.result, {"status": "accepted", "attempt": 1})

    def test_conflicting_request_and_idempotency_replays_are_rejected(self) -> None:
        ledger = IdempotencyLedger(self.root)
        ledger.accept(self.request)
        changed_request = LaunchRequest.from_dict({**REQUEST_DATA, "deadline_seconds": 181})
        with self.assertRaises(ProtocolError) as context:
            ledger.accept(changed_request)
        self.assertEqual(context.exception.code, ErrorCode.DUPLICATE_CONFLICT)

        changed_request_id = LaunchRequest.from_dict(
            {**REQUEST_DATA, "request_id": "123e4567-e89b-12d3-a456-426614174002"}
        )
        with self.assertRaises(ProtocolError) as context:
            ledger.accept(changed_request_id)
        self.assertEqual(context.exception.code, ErrorCode.DUPLICATE_CONFLICT)

    def test_persistence_survives_ledger_reconstruction(self) -> None:
        first = IdempotencyLedger(self.root).accept(self.request, {"accepted": True})
        second = IdempotencyLedger(self.root).accept(self.request)
        self.assertEqual(second, first)
        self.assertEqual(
            IdempotencyLedger(self.root).lookup(self.request.idempotency_key), first
        )

    def test_identifiers_are_hash_derived_and_not_path_components(self) -> None:
        ledger = IdempotencyLedger(self.root)
        ledger.accept(self.request)
        records = list((self.root / "records").iterdir())
        self.assertEqual(len(records), 2)
        for path in records:
            self.assertEqual(path.name, f"{path.stem}.json")
            self.assertEqual(len(path.stem), 64)
            self.assertNotIn("Task-12", str(path))
        traversal_identifier = "../../outside"
        self.assertIsNone(ledger.lookup(traversal_identifier))
        expected_path = self.root / "records" / (
            hashlib.sha256(traversal_identifier.encode()).hexdigest() + ".json"
        )
        self.assertFalse(expected_path.exists())

    def test_symlink_record_is_rejected_without_following_it(self) -> None:
        ledger = IdempotencyLedger(self.root)
        identifier_hash = hashlib.sha256(self.request.request_id.encode()).hexdigest()
        record_path = self.root / "records" / f"{identifier_hash}.json"
        target = self.root / "outside.json"
        target.write_text("{}", encoding="utf-8")
        record_path.symlink_to(target)
        with self.assertRaises(ProtocolError) as context:
            ledger.accept(self.request)
        self.assertEqual(context.exception.code, ErrorCode.LEDGER_ERROR)
        self.assertEqual(target.read_text(encoding="utf-8"), "{}")

    def test_symlinked_root_is_rejected(self) -> None:
        real_root = Path(self.temporary_directory.name) / "real"
        real_root.mkdir()
        symlink_root = Path(self.temporary_directory.name) / "linked"
        symlink_root.symlink_to(real_root, target_is_directory=True)
        with self.assertRaises(ProtocolError) as context:
            IdempotencyLedger(symlink_root)
        self.assertEqual(context.exception.code, ErrorCode.LEDGER_ERROR)

    def test_same_identifier_is_one_record(self) -> None:
        same_identifier = {**REQUEST_DATA, "idempotency_key": REQUEST_DATA["request_id"]}
        request = LaunchRequest.from_dict(same_identifier)
        IdempotencyLedger(self.root).accept(request)
        self.assertEqual(len(list((self.root / "records").iterdir())), 1)

    def test_ledger_never_creates_execution_artifacts(self) -> None:
        IdempotencyLedger(self.root).accept(self.request)
        names = {path.name for path in self.root.rglob("*")}
        self.assertNotIn("pid", names)
        self.assertNotIn("command", names)
        self.assertNotIn("artifacts", names)


if __name__ == "__main__":
    unittest.main()
