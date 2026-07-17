"""Bounded four-byte-length framing for a future Unix-domain socket."""

from __future__ import annotations

import json
import struct
from typing import BinaryIO

from .errors import ErrorCode, ProtocolError


MAX_PAYLOAD_SIZE = 64 * 1024
_HEADER = struct.Struct(">I")


def _validate_payload(payload: bytes) -> bytes:
    if not payload:
        raise ProtocolError(ErrorCode.MALFORMED_FRAME, "frame payload is empty")
    try:
        text = payload.decode("utf-8", "strict")
    except UnicodeDecodeError as error:
        raise ProtocolError(ErrorCode.MALFORMED_FRAME, "frame payload is not UTF-8") from error
    try:
        json.loads(text, parse_constant=lambda value: (_ for _ in ()).throw(ValueError(value)))
    except (json.JSONDecodeError, ValueError) as error:
        raise ProtocolError(ErrorCode.MALFORMED_FRAME, "frame payload is not valid JSON") from error
    return payload


def encode_frame(payload: bytes | str) -> bytes:
    """Encode one non-empty, valid UTF-8 JSON payload with a bounded length."""

    if isinstance(payload, str):
        try:
            payload = payload.encode("utf-8", "strict")
        except UnicodeEncodeError as error:
            raise ProtocolError(ErrorCode.MALFORMED_FRAME, "frame payload is not UTF-8") from error
    if not isinstance(payload, bytes):
        raise ProtocolError(ErrorCode.MALFORMED_FRAME, "frame payload must be bytes or text")
    if len(payload) > MAX_PAYLOAD_SIZE:
        raise ProtocolError(ErrorCode.FRAME_TOO_LARGE, "frame payload exceeds 64 KiB")
    _validate_payload(payload)
    return _HEADER.pack(len(payload)) + payload


def _read_exact(stream: BinaryIO, size: int) -> bytes:
    chunks: list[bytes] = []
    remaining = size
    while remaining:
        chunk = stream.read(remaining)
        if not chunk:
            raise ProtocolError(ErrorCode.TRUNCATED_FRAME, "frame ended before its declared length")
        if not isinstance(chunk, bytes):
            raise ProtocolError(ErrorCode.MALFORMED_FRAME, "frame stream returned non-bytes")
        if len(chunk) > remaining:
            raise ProtocolError(ErrorCode.MALFORMED_FRAME, "frame stream returned extra bytes")
        chunks.append(chunk)
        remaining -= len(chunk)
    return b"".join(chunks)


def read_frame(stream: BinaryIO) -> bytes:
    """Read exactly one bounded frame from a binary stream.

    A stream may contain a subsequent frame. Use :func:`decode_frame` when the
    caller has a complete message buffer and trailing bytes must be rejected.
    """

    first_header_chunk = stream.read(_HEADER.size)
    if not first_header_chunk:
        raise ProtocolError(ErrorCode.MALFORMED_FRAME, "frame is empty")
    if not isinstance(first_header_chunk, bytes):
        raise ProtocolError(ErrorCode.MALFORMED_FRAME, "frame stream returned non-bytes")
    if len(first_header_chunk) > _HEADER.size:
        raise ProtocolError(ErrorCode.MALFORMED_FRAME, "frame stream returned extra bytes")
    if len(first_header_chunk) < _HEADER.size:
        try:
            header = first_header_chunk + _read_exact(
                stream, _HEADER.size - len(first_header_chunk)
            )
        except ProtocolError as error:
            if error.code is ErrorCode.TRUNCATED_FRAME:
                raise ProtocolError(
                    ErrorCode.TRUNCATED_FRAME, "frame header is incomplete"
                ) from error
            raise
    else:
        header = first_header_chunk
    payload_size = _HEADER.unpack(header)[0]
    if payload_size == 0:
        raise ProtocolError(ErrorCode.MALFORMED_FRAME, "frame payload is empty")
    if payload_size > MAX_PAYLOAD_SIZE:
        raise ProtocolError(ErrorCode.FRAME_TOO_LARGE, "frame payload exceeds 64 KiB")
    return _validate_payload(_read_exact(stream, payload_size))


def decode_frame(frame: bytes) -> bytes:
    """Decode one complete frame and reject missing, extra, or invalid bytes."""

    if not isinstance(frame, bytes):
        raise ProtocolError(ErrorCode.MALFORMED_FRAME, "frame must be bytes")
    if not frame:
        raise ProtocolError(ErrorCode.MALFORMED_FRAME, "frame is empty")
    if len(frame) < _HEADER.size:
        raise ProtocolError(ErrorCode.TRUNCATED_FRAME, "frame header is incomplete")
    payload_size = _HEADER.unpack(frame[: _HEADER.size])[0]
    if payload_size == 0:
        raise ProtocolError(ErrorCode.MALFORMED_FRAME, "frame payload is empty")
    if payload_size > MAX_PAYLOAD_SIZE:
        raise ProtocolError(ErrorCode.FRAME_TOO_LARGE, "frame payload exceeds 64 KiB")
    expected_size = _HEADER.size + payload_size
    if len(frame) < expected_size:
        raise ProtocolError(ErrorCode.TRUNCATED_FRAME, "frame ended before its declared length")
    if len(frame) > expected_size:
        raise ProtocolError(ErrorCode.MALFORMED_FRAME, "trailing bytes follow the frame")
    return _validate_payload(frame[_HEADER.size :])


def write_frame(stream: BinaryIO, payload: bytes | str) -> None:
    """Write one complete frame without opening or owning the stream."""

    frame = encode_frame(payload)
    stream.write(frame)
    flush = getattr(stream, "flush", None)
    if flush is not None:
        flush()
