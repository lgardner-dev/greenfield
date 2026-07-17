"""A durable, acceptance-only idempotency ledger for validated requests."""

from __future__ import annotations

import fcntl
import hashlib
import json
import os
import tempfile
from dataclasses import dataclass
from pathlib import Path
from typing import Any, Mapping

from .errors import ErrorCode, ProtocolError
from .protocol import LaunchRequest


LEDGER_SCHEMA_VERSION = 1
MAX_STORED_RESULT_BYTES = 16 * 1024
MAX_RECORD_BYTES = 64 * 1024


@dataclass(frozen=True)
class LedgerRecord:
    """The only durable result of ledger acceptance."""

    fingerprint: str
    request: LaunchRequest
    result: dict[str, Any] | None

    def to_dict(self) -> dict[str, Any]:
        data: dict[str, Any] = {
            "schema_version": LEDGER_SCHEMA_VERSION,
            "fingerprint": self.fingerprint,
            "request": self.request.to_dict(),
        }
        if self.result is not None:
            data["result"] = self.result
        return data


class IdempotencyLedger:
    """Persist request acceptance using hash-derived, exclusive record names.

    The ledger never starts work and has no process, socket, or graphics
    dependencies. A caller supplies the root so production policy can choose
    its storage location and tests can use a temporary directory.
    """

    def __init__(self, root: Path | str) -> None:
        self._root = Path(root)
        self._records = self._root / "records"
        self._lock_path = self._root / ".ledger.lock"
        self._prepare_root()

    def accept(
        self, request: LaunchRequest, result: Mapping[str, Any] | None = None
    ) -> LedgerRecord:
        """Accept a request, replay its record, or reject a conflicting reuse."""

        if not isinstance(request, LaunchRequest):
            raise ProtocolError(ErrorCode.INVALID_REQUEST, "ledger requires a LaunchRequest")
        stored_result = _validate_result(result)
        record = LedgerRecord(
            fingerprint=_fingerprint(request), request=request, result=stored_result
        )
        serialized_record = _serialize_record(record)
        with self._exclusive_lock():
            existing_records = self._load_existing_records(request)
            for existing in existing_records:
                if existing.fingerprint != record.fingerprint:
                    raise ProtocolError(
                        ErrorCode.DUPLICATE_CONFLICT,
                        "request or idempotency key was reused with different contents",
                    )
            if existing_records:
                existing = existing_records[0]
                self._ensure_aliases(request, serialized_record)
                return existing
            identifiers = tuple(dict.fromkeys((request.request_id, request.idempotency_key)))
            for identifier in identifiers:
                self._create_record(identifier, serialized_record)
        return record

    def lookup(self, identifier: str) -> LedgerRecord | None:
        """Look up by request ID or idempotency key without using it as a path."""

        record_path = self._record_path(identifier)
        if not record_path.is_symlink() and not record_path.exists():
            return None
        return self._read_record(record_path)

    def _prepare_root(self) -> None:
        try:
            self._root.mkdir(parents=True, exist_ok=True)
            if self._root.is_symlink() or not self._root.is_dir():
                raise OSError("ledger root is not a directory")
            self._records.mkdir(exist_ok=True)
            if self._records.is_symlink() or not self._records.is_dir():
                raise OSError("ledger records path is not a directory")
        except OSError as error:
            raise ProtocolError(
                ErrorCode.LEDGER_ERROR, "could not prepare ledger storage"
            ) from error

    def _record_path(self, identifier: str) -> Path:
        if not isinstance(identifier, str):
            raise ProtocolError(ErrorCode.LEDGER_ERROR, "ledger identifier must be text")
        try:
            identifier_bytes = identifier.encode("utf-8", "strict")
        except UnicodeEncodeError as error:
            raise ProtocolError(
                ErrorCode.LEDGER_ERROR, "ledger identifier is not valid UTF-8"
            ) from error
        identifier_hash = hashlib.sha256(identifier_bytes).hexdigest()
        return self._records / f"{identifier_hash}.json"

    def _load_existing_records(self, request: LaunchRequest) -> list[LedgerRecord]:
        records: list[LedgerRecord] = []
        seen_paths: set[Path] = set()
        identifiers = tuple(dict.fromkeys((request.request_id, request.idempotency_key)))
        for identifier in identifiers:
            path = self._record_path(identifier)
            if path in seen_paths:
                continue
            seen_paths.add(path)
            if path.is_symlink() or path.exists():
                records.append(self._read_record(path))
        return records

    def _read_record(self, path: Path) -> LedgerRecord:
        try:
            descriptor = os.open(path, os.O_RDONLY | _no_follow())
            if os.fstat(descriptor).st_size > MAX_RECORD_BYTES:
                os.close(descriptor)
                raise OSError("ledger record exceeds its size bound")
            with os.fdopen(descriptor, "r", encoding="utf-8") as stream:
                data = json.load(stream)
        except (OSError, UnicodeDecodeError, json.JSONDecodeError, TypeError) as error:
            raise ProtocolError(
                ErrorCode.LEDGER_ERROR, "ledger record could not be read"
            ) from error
        if not isinstance(data, dict):
            raise ProtocolError(ErrorCode.LEDGER_ERROR, "ledger record is not an object")
        try:
            if data.get("schema_version") != LEDGER_SCHEMA_VERSION:
                raise ValueError("unsupported ledger schema")
            fingerprint = data["fingerprint"]
            request_data = data["request"]
            result = data.get("result")
            if not isinstance(fingerprint, str) or not isinstance(request_data, dict):
                raise ValueError("invalid ledger record fields")
            request = LaunchRequest.from_dict(request_data)
            if result is not None and not isinstance(result, dict):
                raise ValueError("invalid ledger result")
            if fingerprint != _fingerprint(request):
                raise ValueError("ledger fingerprint does not match request")
            result = _validate_result(result)
        except (KeyError, TypeError, ValueError, ProtocolError) as error:
            raise ProtocolError(ErrorCode.LEDGER_ERROR, "ledger record is invalid") from error
        return LedgerRecord(fingerprint=fingerprint, request=request, result=result)

    def _create_record(self, identifier: str, serialized_record: bytes) -> None:
        path = self._record_path(identifier)
        temporary_path: Path | None = None
        try:
            descriptor, temporary_name = tempfile.mkstemp(
                prefix=".record-", suffix=".tmp", dir=self._records
            )
            temporary_path = Path(temporary_name)
            os.fchmod(descriptor, 0o600)
            with os.fdopen(descriptor, "wb") as stream:
                stream.write(serialized_record)
                stream.flush()
                os.fsync(stream.fileno())
            os.link(temporary_path, path)
            temporary_path.unlink()
            temporary_path = None
            _flush_directory(self._records)
        except FileExistsError:
            if temporary_path is not None:
                temporary_path.unlink(missing_ok=True)
            existing = self._read_record(path)
            raise ProtocolError(
                ErrorCode.DUPLICATE_CONFLICT,
                "ledger alias appeared concurrently",
            ) from existing
        except OSError as error:
            if temporary_path is not None:
                temporary_path.unlink(missing_ok=True)
            raise ProtocolError(ErrorCode.LEDGER_ERROR, "could not create ledger record") from error

    def _ensure_aliases(self, request: LaunchRequest, serialized_record: bytes) -> None:
        for identifier in (request.request_id, request.idempotency_key):
            path = self._record_path(identifier)
            if not path.is_symlink() and not path.exists():
                self._create_record(identifier, serialized_record)

    def _exclusive_lock(self):
        return _ExclusiveFileLock(self._lock_path)


class _ExclusiveFileLock:
    def __init__(self, path: Path) -> None:
        self._path = path
        self._stream = None

    def __enter__(self) -> "_ExclusiveFileLock":
        try:
            descriptor = os.open(
                self._path,
                os.O_RDWR | os.O_CREAT | _no_follow(),
                0o600,
            )
            self._stream = os.fdopen(descriptor, "r+b")
            fcntl.flock(self._stream.fileno(), fcntl.LOCK_EX)
        except OSError as error:
            if self._stream is not None:
                self._stream.close()
            raise ProtocolError(ErrorCode.LEDGER_ERROR, "could not lock ledger storage") from error
        return self

    def __exit__(self, exception_type, exception, traceback) -> None:
        if self._stream is None:
            return
        try:
            fcntl.flock(self._stream.fileno(), fcntl.LOCK_UN)
        finally:
            self._stream.close()


def _no_follow() -> int:
    return getattr(os, "O_NOFOLLOW", 0)


def _fingerprint(request: LaunchRequest) -> str:
    return hashlib.sha256(request.to_json().encode("utf-8", "strict")).hexdigest()


def _validate_result(result: Mapping[str, Any] | None) -> dict[str, Any] | None:
    if result is None:
        return None
    if not isinstance(result, Mapping):
        raise ProtocolError(ErrorCode.INVALID_REQUEST, "stored result must be an object")
    try:
        result_copy = dict(result)
        serialized = json.dumps(
            result_copy,
            ensure_ascii=False,
            sort_keys=True,
            separators=(",", ":"),
            allow_nan=False,
        ).encode("utf-8", "strict")
    except (TypeError, ValueError, UnicodeEncodeError) as error:
        raise ProtocolError(ErrorCode.INVALID_REQUEST, "stored result is not safe JSON") from error
    if len(serialized) > MAX_STORED_RESULT_BYTES:
        raise ProtocolError(ErrorCode.INVALID_REQUEST, "stored result exceeds its size bound")
    return result_copy


def _serialize_record(record: LedgerRecord) -> bytes:
    try:
        return (
            json.dumps(
                record.to_dict(),
                ensure_ascii=False,
                sort_keys=True,
                separators=(",", ":"),
                allow_nan=False,
            )
            + "\n"
        ).encode("utf-8", "strict")
    except (TypeError, ValueError, UnicodeEncodeError) as error:
        raise ProtocolError(ErrorCode.LEDGER_ERROR, "ledger record is not serializable") from error


def _flush_directory(directory: Path) -> None:
    flags = os.O_RDONLY | getattr(os, "O_DIRECTORY", 0) | _no_follow()
    try:
        descriptor = os.open(directory, flags)
    except OSError as error:
        raise ProtocolError(ErrorCode.LEDGER_ERROR, "could not open ledger directory") from error
    try:
        os.fsync(descriptor)
    except OSError as error:
        raise ProtocolError(ErrorCode.LEDGER_ERROR, "could not flush ledger directory") from error
    finally:
        os.close(descriptor)
