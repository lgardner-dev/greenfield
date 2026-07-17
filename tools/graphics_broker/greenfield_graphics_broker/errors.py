"""Stable, transport-neutral errors for the restricted graphics protocol."""

from __future__ import annotations

import json
from dataclasses import dataclass
from enum import Enum


class ErrorCode(str, Enum):
    """Machine-readable protocol and security failure codes."""

    UNSUPPORTED_VERSION = "unsupported-version"
    INVALID_OPERATION = "invalid-operation"
    INVALID_REQUEST = "invalid-request"
    MISSING_FIELD = "missing-field"
    UNKNOWN_FIELD = "unknown-field"
    MALFORMED_FRAME = "malformed-frame"
    FRAME_TOO_LARGE = "frame-too-large"
    TRUNCATED_FRAME = "truncated-frame"
    UNAUTHORIZED_PEER = "unauthorized-peer"
    DUPLICATE_CONFLICT = "duplicate-conflict"
    LEDGER_ERROR = "ledger-error"
    CREDENTIAL_ERROR = "credential-error"


MAX_ERROR_DETAIL_LENGTH = 256


def _safe_detail(detail: str | None) -> str | None:
    if detail is None:
        return None
    if not isinstance(detail, str):
        detail = str(detail)
    detail = " ".join(detail.split())
    detail = detail.encode("utf-8", "replace").decode("utf-8")
    if len(detail) > MAX_ERROR_DETAIL_LENGTH:
        return detail[:MAX_ERROR_DETAIL_LENGTH]
    return detail


@dataclass(frozen=True)
class ProtocolError(Exception):
    """An error that can be safely represented on a future broker transport."""

    code: ErrorCode
    detail: str | None = None

    def __post_init__(self) -> None:
        if not isinstance(self.code, ErrorCode):
            object.__setattr__(self, "code", ErrorCode(self.code))
        object.__setattr__(self, "detail", _safe_detail(self.detail))
        Exception.__init__(self, self.message)

    @property
    def message(self) -> str:
        if self.detail:
            return f"{self.code.value}: {self.detail}"
        return self.code.value

    def to_dict(self) -> dict[str, str]:
        data = {"code": self.code.value}
        if self.detail:
            data["detail"] = self.detail
        return data

    def to_json(self) -> str:
        """Return a deterministic, bounded JSON error object."""

        return json.dumps(self.to_dict(), sort_keys=True, separators=(",", ":"))

    def __str__(self) -> str:
        return self.message
