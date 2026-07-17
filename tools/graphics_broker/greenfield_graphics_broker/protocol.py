"""Strict version-one request model for the restricted graphics broker."""

from __future__ import annotations

import json
import re
import uuid
from dataclasses import dataclass
from typing import Any, Mapping

from .errors import ErrorCode, ProtocolError


PROTOCOL_VERSION = 1
OPERATION_LAUNCH_VISUAL_REVIEW = "launch-visual-review"
PROTOCOL_FIELDS = frozenset(
    {
        "protocol_version",
        "operation",
        "request_id",
        "idempotency_key",
        "task_id",
        "review_id",
        "profile_id",
        "workspace_id",
        "capture_plan_id",
        "deadline_seconds",
    }
)

MAX_IDEMPOTENCY_KEY_LENGTH = 128
MAX_TASK_ID_LENGTH = 128
MAX_PROFILE_ID_LENGTH = 64
MAX_WORKSPACE_ID_LENGTH = 128
MAX_CAPTURE_PLAN_ID_LENGTH = 64
MIN_DEADLINE_SECONDS = 1
MAX_DEADLINE_SECONDS = 3600

ALLOWED_PROFILE_IDS = frozenset(
    {
        "control-room-webgpu",
        "sandbox-webgpu",
        "renderer-startup-probe-hardware",
        "renderer-startup-probe-swiftshader-x11",
    }
)
ALLOWED_CAPTURE_PLAN_IDS = frozenset(
    {
        "control-room-1280x720",
        "sandbox-1280x720",
        "startup-probe-1280x720",
    }
)

_TASK_ID_PATTERN = re.compile(r"^[A-Za-z0-9][A-Za-z0-9._-]*$")
_IDENTIFIER_PATTERN = re.compile(r"^[A-Za-z0-9][A-Za-z0-9._-]*$")


def _invalid(detail: str) -> ProtocolError:
    return ProtocolError(ErrorCode.INVALID_REQUEST, detail)


def _require_string(data: Mapping[str, Any], field: str, maximum: int) -> str:
    value = data.get(field)
    if not isinstance(value, str) or not value:
        raise _invalid(f"{field} must be a non-empty string")
    try:
        value.encode("utf-8", "strict")
    except UnicodeEncodeError as error:
        raise _invalid(f"{field} must be valid UTF-8") from error
    if len(value) > maximum:
        raise _invalid(f"{field} exceeds its length bound")
    if any(ord(character) < 0x20 for character in value):
        raise _invalid(f"{field} contains a control character")
    return value


def _require_identifier(data: Mapping[str, Any], field: str, maximum: int) -> str:
    value = _require_string(data, field, maximum)
    if not _IDENTIFIER_PATTERN.fullmatch(value):
        raise _invalid(f"{field} has an invalid identifier")
    if ".." in value or value.endswith(".") or value.endswith(".lock"):
        raise _invalid(f"{field} has an unsafe identifier")
    return value


def _require_task_id(data: Mapping[str, Any]) -> str:
    value = _require_string(data, "task_id", MAX_TASK_ID_LENGTH)
    if (
        not _TASK_ID_PATTERN.fullmatch(value)
        or ".." in value
        or value.endswith(".")
        or value.endswith(".lock")
    ):
        raise _invalid("task_id is not safe for a runner directory or branch component")
    return value


def _require_uuid(data: Mapping[str, Any], field: str) -> str:
    value = _require_string(data, field, 36)
    try:
        parsed = uuid.UUID(value)
    except (ValueError, AttributeError) as error:
        raise _invalid(f"{field} must be a canonical UUID") from error
    if str(parsed) != value:
        raise _invalid(f"{field} must be a canonical UUID")
    return value


def _reject_duplicate_keys(pairs: list[tuple[str, Any]]) -> dict[str, Any]:
    result: dict[str, Any] = {}
    for key, value in pairs:
        if key in result:
            raise ValueError("duplicate JSON field")
        result[key] = value
    return result


def _parse_json_object(payload: str) -> dict[str, Any]:
    try:
        value = json.loads(
            payload,
            object_pairs_hook=_reject_duplicate_keys,
            parse_constant=lambda value: (_ for _ in ()).throw(ValueError(value)),
        )
    except (json.JSONDecodeError, UnicodeDecodeError, ValueError, TypeError) as error:
        raise _invalid("request is not valid JSON") from error
    if not isinstance(value, dict):
        raise _invalid("request must be a JSON object")
    return value


@dataclass(frozen=True)
class LaunchRequest:
    """The complete closed request schema accepted by protocol version one."""

    protocol_version: int
    operation: str
    request_id: str
    idempotency_key: str
    task_id: str
    review_id: str
    profile_id: str
    workspace_id: str
    capture_plan_id: str
    deadline_seconds: int

    def __post_init__(self) -> None:
        if isinstance(self.protocol_version, bool) or not isinstance(self.protocol_version, int):
            raise _invalid("protocol_version must be an integer")
        if self.protocol_version != PROTOCOL_VERSION:
            raise ProtocolError(ErrorCode.UNSUPPORTED_VERSION, "unsupported protocol version")
        if self.operation != OPERATION_LAUNCH_VISUAL_REVIEW:
            raise ProtocolError(ErrorCode.INVALID_OPERATION, "unsupported operation")
        _require_uuid({"request_id": self.request_id}, "request_id")
        _require_identifier(
            {"idempotency_key": self.idempotency_key},
            "idempotency_key",
            MAX_IDEMPOTENCY_KEY_LENGTH,
        )
        _require_task_id({"task_id": self.task_id})
        _require_uuid({"review_id": self.review_id}, "review_id")
        profile_id = _require_identifier(
            {"profile_id": self.profile_id}, "profile_id", MAX_PROFILE_ID_LENGTH
        )
        if profile_id not in ALLOWED_PROFILE_IDS:
            raise _invalid("profile_id is not an allowed protocol enum value")
        _require_identifier(
            {"workspace_id": self.workspace_id}, "workspace_id", MAX_WORKSPACE_ID_LENGTH
        )
        capture_plan_id = _require_identifier(
            {"capture_plan_id": self.capture_plan_id},
            "capture_plan_id",
            MAX_CAPTURE_PLAN_ID_LENGTH,
        )
        if capture_plan_id not in ALLOWED_CAPTURE_PLAN_IDS:
            raise _invalid("capture_plan_id is not an allowed protocol enum value")
        if (
            isinstance(self.deadline_seconds, bool)
            or not isinstance(self.deadline_seconds, int)
            or not MIN_DEADLINE_SECONDS <= self.deadline_seconds <= MAX_DEADLINE_SECONDS
        ):
            raise _invalid("deadline_seconds is outside its positive bounded range")

    @classmethod
    def from_dict(cls, data: Mapping[str, Any]) -> "LaunchRequest":
        if not isinstance(data, Mapping):
            raise _invalid("request must be a JSON object")
        unknown_fields = set(data) - PROTOCOL_FIELDS
        if unknown_fields:
            field = sorted(str(value) for value in unknown_fields)[0]
            raise ProtocolError(ErrorCode.UNKNOWN_FIELD, f"unknown field: {field}")
        missing_fields = PROTOCOL_FIELDS - set(data)
        if missing_fields:
            field = sorted(missing_fields)[0]
            raise ProtocolError(ErrorCode.MISSING_FIELD, f"missing field: {field}")

        protocol_version = data["protocol_version"]
        if isinstance(protocol_version, bool) or not isinstance(protocol_version, int):
            raise _invalid("protocol_version must be an integer")
        if protocol_version != PROTOCOL_VERSION:
            raise ProtocolError(ErrorCode.UNSUPPORTED_VERSION, "unsupported protocol version")

        operation = data["operation"]
        if operation != OPERATION_LAUNCH_VISUAL_REVIEW:
            raise ProtocolError(ErrorCode.INVALID_OPERATION, "unsupported operation")

        profile_id = _require_identifier(data, "profile_id", MAX_PROFILE_ID_LENGTH)
        if profile_id not in ALLOWED_PROFILE_IDS:
            raise _invalid("profile_id is not an allowed protocol enum value")
        capture_plan_id = _require_identifier(
            data, "capture_plan_id", MAX_CAPTURE_PLAN_ID_LENGTH
        )
        if capture_plan_id not in ALLOWED_CAPTURE_PLAN_IDS:
            raise _invalid("capture_plan_id is not an allowed protocol enum value")

        deadline_seconds = data["deadline_seconds"]
        if (
            isinstance(deadline_seconds, bool)
            or not isinstance(deadline_seconds, int)
            or not MIN_DEADLINE_SECONDS <= deadline_seconds <= MAX_DEADLINE_SECONDS
        ):
            raise _invalid("deadline_seconds is outside its positive bounded range")

        return cls(
            protocol_version=protocol_version,
            operation=operation,
            request_id=_require_uuid(data, "request_id"),
            idempotency_key=_require_identifier(
                data, "idempotency_key", MAX_IDEMPOTENCY_KEY_LENGTH
            ),
            task_id=_require_task_id(data),
            review_id=_require_uuid(data, "review_id"),
            profile_id=profile_id,
            workspace_id=_require_identifier(data, "workspace_id", MAX_WORKSPACE_ID_LENGTH),
            capture_plan_id=capture_plan_id,
            deadline_seconds=deadline_seconds,
        )

    @classmethod
    def from_json(cls, payload: str | bytes) -> "LaunchRequest":
        if isinstance(payload, bytes):
            try:
                payload = payload.decode("utf-8", "strict")
            except UnicodeDecodeError as error:
                raise _invalid("request is not valid UTF-8") from error
        if not isinstance(payload, str) or not payload:
            raise _invalid("request JSON must be a non-empty UTF-8 string")
        return cls.from_dict(_parse_json_object(payload))

    def to_dict(self) -> dict[str, Any]:
        return {
            "protocol_version": self.protocol_version,
            "operation": self.operation,
            "request_id": self.request_id,
            "idempotency_key": self.idempotency_key,
            "task_id": self.task_id,
            "review_id": self.review_id,
            "profile_id": self.profile_id,
            "workspace_id": self.workspace_id,
            "capture_plan_id": self.capture_plan_id,
            "deadline_seconds": self.deadline_seconds,
        }

    def to_json(self) -> str:
        try:
            return json.dumps(
                self.to_dict(), ensure_ascii=False, sort_keys=True, separators=(",", ":")
            )
        except (TypeError, UnicodeEncodeError) as error:
            raise _invalid("request cannot be serialized") from error


def serialize_request(request: LaunchRequest) -> bytes:
    """Return the deterministic UTF-8 JSON representation of a request."""

    if not isinstance(request, LaunchRequest):
        raise _invalid("expected a LaunchRequest")
    try:
        return request.to_json().encode("utf-8", "strict")
    except UnicodeEncodeError as error:
        raise _invalid("request is not valid UTF-8") from error


def parse_request(payload: str | bytes) -> LaunchRequest:
    """Parse and validate a version-one request without accepting extensions."""

    return LaunchRequest.from_json(payload)
