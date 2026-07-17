from __future__ import annotations

import io
import json
import struct
import unittest
import uuid

from greenfield_graphics_broker import (
    ErrorCode,
    LaunchRequest,
    MAX_PAYLOAD_SIZE,
    PeerCredentials,
    ProtocolError,
    authorize_peer,
    decode_frame,
    encode_frame,
    extract_peer_credentials,
    parse_request,
    read_frame,
)


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


class ProtocolRequestTests(unittest.TestCase):
    def test_valid_request_round_trip_is_deterministic(self) -> None:
        request = LaunchRequest.from_dict(REQUEST_DATA)
        serialized = request.to_json()
        self.assertEqual(serialized, request.to_json())
        self.assertEqual(parse_request(serialized), request)
        self.assertEqual(
            serialized,
            '{"capture_plan_id":"control-room-1280x720","deadline_seconds":180,'
            '"idempotency_key":"task-12-attempt-1","operation":"launch-visual-review",'
            '"profile_id":"control-room-webgpu","protocol_version":1,'
            '"request_id":"123e4567-e89b-12d3-a456-426614174000",'
            '"review_id":"123e4567-e89b-12d3-a456-426614174001",'
            '"task_id":"Task-12","workspace_id":"runner-task-worktree"}',
        )

    def test_missing_and_unknown_fields_are_rejected(self) -> None:
        missing = dict(REQUEST_DATA)
        del missing["review_id"]
        self.assert_code(ErrorCode.MISSING_FIELD, LaunchRequest.from_dict, missing)

        unknown = dict(REQUEST_DATA, executable_path="/bin/anything")
        self.assert_code(ErrorCode.UNKNOWN_FIELD, LaunchRequest.from_dict, unknown)

    def test_protocol_and_operation_values_are_rejected(self) -> None:
        unsupported = dict(REQUEST_DATA, protocol_version=2)
        self.assert_code(ErrorCode.UNSUPPORTED_VERSION, LaunchRequest.from_dict, unsupported)
        invalid_operation = dict(REQUEST_DATA, operation="exec")
        self.assert_code(ErrorCode.INVALID_OPERATION, LaunchRequest.from_dict, invalid_operation)
        invalid_profile = dict(REQUEST_DATA, profile_id="arbitrary-profile")
        self.assert_code(ErrorCode.INVALID_REQUEST, LaunchRequest.from_dict, invalid_profile)
        invalid_capture = dict(REQUEST_DATA, capture_plan_id="arbitrary-plan")
        self.assert_code(ErrorCode.INVALID_REQUEST, LaunchRequest.from_dict, invalid_capture)

    def test_uuid_and_task_id_rules_are_rejected(self) -> None:
        for field in ("request_id", "review_id"):
            for value in ("not-a-uuid", str(uuid.uuid4()).upper()):
                with self.subTest(field=field, value=value):
                    self.assert_code(
                        ErrorCode.INVALID_REQUEST,
                        LaunchRequest.from_dict,
                        {**REQUEST_DATA, field: value},
                    )
        for task_id in ("", "-bad", ".bad", "has space", "a/../b", "ends.", "bad.lock"):
            with self.subTest(task_id=task_id):
                self.assert_code(
                    ErrorCode.INVALID_REQUEST,
                    LaunchRequest.from_dict,
                    {**REQUEST_DATA, "task_id": task_id},
                )

    def test_string_and_numeric_bounds_and_types_are_rejected(self) -> None:
        for field in (
            "idempotency_key",
            "task_id",
            "profile_id",
            "workspace_id",
            "capture_plan_id",
        ):
            with self.subTest(field=field):
                oversized = dict(REQUEST_DATA, **{field: "a" * 129})
                self.assert_code(ErrorCode.INVALID_REQUEST, LaunchRequest.from_dict, oversized)
        for deadline in (0, 3601, 1.5, True):
            with self.subTest(deadline=deadline):
                self.assert_code(
                    ErrorCode.INVALID_REQUEST,
                    LaunchRequest.from_dict,
                    {**REQUEST_DATA, "deadline_seconds": deadline},
                )
        self.assert_code(
            ErrorCode.INVALID_REQUEST,
            LaunchRequest.from_dict,
            {**REQUEST_DATA, "workspace_id": "\ud800"},
        )

    def test_malformed_utf8_json_and_duplicate_fields_are_rejected(self) -> None:
        self.assert_code(ErrorCode.INVALID_REQUEST, parse_request, b"{\xff")
        self.assert_code(ErrorCode.INVALID_REQUEST, parse_request, b"{not-json")
        duplicate = json.dumps(REQUEST_DATA)[:-1] + ',"operation":"exec"}'
        self.assert_code(ErrorCode.INVALID_REQUEST, parse_request, duplicate)
        self.assert_code(ErrorCode.INVALID_REQUEST, parse_request, "[]")

    @staticmethod
    def assert_code(code: ErrorCode, function, *args) -> None:
        with unittest.TestCase().assertRaises(ProtocolError) as context:
            function(*args)
        unittest.TestCase().assertEqual(context.exception.code, code)


class FramingTests(unittest.TestCase):
    def test_round_trip_and_exact_stream_reads(self) -> None:
        payload = json.dumps(REQUEST_DATA, separators=(",", ":")).encode("utf-8")
        frame = encode_frame(payload)
        self.assertEqual(decode_frame(frame), payload)
        self.assertEqual(read_frame(io.BytesIO(frame)), payload)

        class OneByteReader(io.BytesIO):
            def read(self, size=-1):
                return super().read(1 if size else size)

        self.assertEqual(read_frame(OneByteReader(frame)), payload)

    def test_empty_truncated_oversized_and_trailing_frames(self) -> None:
        self.assert_code(ErrorCode.MALFORMED_FRAME, decode_frame, b"")
        self.assert_code(ErrorCode.TRUNCATED_FRAME, decode_frame, b"\x00\x00")
        self.assert_code(ErrorCode.MALFORMED_FRAME, decode_frame, b"\x00\x00\x00\x00")
        oversized = struct.pack(">I", MAX_PAYLOAD_SIZE + 1) + b"{}"
        self.assert_code(ErrorCode.FRAME_TOO_LARGE, decode_frame, oversized)
        frame = encode_frame("{}")
        self.assert_code(ErrorCode.MALFORMED_FRAME, decode_frame, frame + b"extra")
        self.assert_code(ErrorCode.TRUNCATED_FRAME, read_frame, io.BytesIO(b"\x00"))
        self.assert_code(ErrorCode.MALFORMED_FRAME, read_frame, io.BytesIO(b""))
        self.assert_code(ErrorCode.MALFORMED_FRAME, decode_frame, struct.pack(">I", 1) + b"\xff")
        self.assert_code(ErrorCode.MALFORMED_FRAME, decode_frame, struct.pack(">I", 1) + b"x")

    def test_frame_size_is_measured_in_utf8_bytes(self) -> None:
        payload = '{"value":"' + ("é" * (MAX_PAYLOAD_SIZE // 2)) + '"}'
        self.assert_code(ErrorCode.FRAME_TOO_LARGE, encode_frame, payload)

    @staticmethod
    def assert_code(code: ErrorCode, function, *args) -> None:
        with unittest.TestCase().assertRaises(ProtocolError) as context:
            function(*args)
        unittest.TestCase().assertEqual(context.exception.code, code)


class CredentialTests(unittest.TestCase):
    def test_authorized_and_unauthorized_peers(self) -> None:
        peer = PeerCredentials(pid=42, uid=1001, gid=1001)
        authorize_peer(peer, 1001)
        with self.assertRaises(ProtocolError) as context:
            authorize_peer(peer, 1002)
        self.assertEqual(context.exception.code, ErrorCode.UNAUTHORIZED_PEER)

    def test_credentials_are_extracted_from_socket_option(self) -> None:
        class FakeSocket:
            def getsockopt(self, level, option, size):
                self.arguments = (level, option, size)
                return struct.pack("3i", 42, 1001, 1002)

        credentials = extract_peer_credentials(FakeSocket())
        self.assertEqual(credentials, PeerCredentials(pid=42, uid=1001, gid=1002))


if __name__ == "__main__":
    unittest.main()
