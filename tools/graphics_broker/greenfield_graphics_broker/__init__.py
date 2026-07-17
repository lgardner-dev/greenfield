"""Dependency-light Phase 2B1 restricted graphics broker primitives."""

from .credentials import PeerCredentials, authorize_peer, extract_peer_credentials
from .errors import ErrorCode, ProtocolError
from .framing import MAX_PAYLOAD_SIZE, decode_frame, encode_frame, read_frame, write_frame
from .ledger import IdempotencyLedger, LedgerRecord
from .protocol import (
    ALLOWED_CAPTURE_PLAN_IDS,
    ALLOWED_PROFILE_IDS,
    LaunchRequest,
    parse_request,
    serialize_request,
)

__all__ = [
    "ALLOWED_CAPTURE_PLAN_IDS",
    "ALLOWED_PROFILE_IDS",
    "ErrorCode",
    "IdempotencyLedger",
    "LaunchRequest",
    "LedgerRecord",
    "MAX_PAYLOAD_SIZE",
    "PeerCredentials",
    "ProtocolError",
    "authorize_peer",
    "decode_frame",
    "encode_frame",
    "extract_peer_credentials",
    "parse_request",
    "read_frame",
    "serialize_request",
    "write_frame",
]
