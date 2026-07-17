# Restricted graphics broker protocol

This package is the Phase 2B1 protocol-only foundation. It defines a strict
version-one `launch-visual-review` request, bounded UTF-8 JSON framing, Linux
peer-credential extraction and UID authorization primitives, structured errors,
and a durable acceptance-only idempotency ledger.

It does not bind or listen on a socket, register a graphics session, launch a
process, capture graphics, or integrate with Slack or Hermes. It is independent
of the C++ engine, renderers, SDL, WebGPU, and the existing Python runner.

Version one uses canonical JSON with these bounds: identifier strings are
ASCII-safe and 128 characters or fewer for task/workspace/idempotency values;
profile and capture identifiers are 64 characters or fewer and allowlisted;
UUIDs are lowercase canonical 36-character values; and deadlines are integer
seconds from 1 through 3,600. The framed UTF-8 payload is at most 64 KiB.

## Security invariants

- Requests have a closed schema with canonical UUIDs, runner-safe task IDs,
  bounded strings and deadlines, and allowlisted profile/capture identifiers.
- Requests cannot carry executable paths, unrestricted arguments, environment
  maps, display variables, library paths, shell fragments, or extra fields.
- Framing is one four-byte unsigned big-endian length followed by a non-empty
  UTF-8 JSON payload no larger than 64 KiB.
- Peer authorization uses kernel-reported `SO_PEERCRED` values; a UID in JSON
  is never consulted.
- Ledger filenames are SHA-256 hashes of identifiers. Records are created with
  exclusive no-follow opens, flushed atomically, and protected by a file lock.
- The package performs no process creation, shell parsing, systemd mutation,
  graphics launch, screenshot capture, Fast2D work, or display connection.

## Tests

From any working directory:

```sh
tools/graphics_broker/run-tests
```

The suite uses only Python’s standard library and temporary caller-provided
ledger roots. Phase 2B2 may add the broker daemon and its Unix-domain socket
integration after the protocol and security policy receive the required review.
