"""Linux peer credentials and the separate UID authorization policy."""

from __future__ import annotations

import socket
import struct
from dataclasses import dataclass
from typing import Any

from .errors import ErrorCode, ProtocolError


@dataclass(frozen=True)
class PeerCredentials:
    """Credentials returned by Linux ``SO_PEERCRED``."""

    pid: int
    uid: int
    gid: int

    def __post_init__(self) -> None:
        for name in ("pid", "uid", "gid"):
            value = getattr(self, name)
            if isinstance(value, bool) or not isinstance(value, int) or value < 0:
                raise ValueError(f"{name} must be a non-negative integer")


def extract_peer_credentials(sock: Any) -> PeerCredentials:
    """Read typed credentials from an already-connected Linux socket."""

    option = getattr(socket, "SO_PEERCRED", None)
    if option is None:
        raise ProtocolError(ErrorCode.CREDENTIAL_ERROR, "SO_PEERCRED is unavailable")
    try:
        raw_credentials = sock.getsockopt(
            socket.SOL_SOCKET, option, struct.calcsize("3i")
        )
        pid, uid, gid = struct.unpack("3i", raw_credentials)
    except (AttributeError, OSError, struct.error) as error:
        raise ProtocolError(
            ErrorCode.CREDENTIAL_ERROR, "could not extract peer credentials"
        ) from error
    try:
        return PeerCredentials(pid=pid, uid=uid, gid=gid)
    except ValueError as error:
        raise ProtocolError(ErrorCode.CREDENTIAL_ERROR, "peer credentials were invalid") from error


def authorize_peer(peer: PeerCredentials, allowed_uid: int) -> None:
    """Apply the configured UID policy independently of request contents."""

    if not isinstance(peer, PeerCredentials):
        raise ProtocolError(ErrorCode.UNAUTHORIZED_PEER, "peer credentials are not typed")
    if isinstance(allowed_uid, bool) or not isinstance(allowed_uid, int) or allowed_uid < 0:
        raise ValueError("allowed_uid must be a non-negative integer")
    if peer.uid != allowed_uid:
        raise ProtocolError(ErrorCode.UNAUTHORIZED_PEER, "peer UID is not authorized")
