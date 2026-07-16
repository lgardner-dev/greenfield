# Phase 2: Restricted Graphics Launcher and Durable Visual Review

Status: architecture proposal. This document defines boundaries and contracts;
it does not implement a broker, launcher, service, socket, Slack adapter, Hermes
integration, screenshot capture, or graphical automation.

## 1. Goals and non-goals

### Goals

Phase 2 should provide a narrow capability for unattended graphics validation:

- allow the unprivileged `greenfield-agent` runner to request a named graphics
  launch in the dedicated `greenfield-gfx` session;
- capture screenshots, logs, renderer/session metadata, and other artifacts;
- make every visible UI change stop at an explicit human visual-approval gate;
- preserve durable execution state in the Python runner;
- provide an immutable, transport-neutral review bundle for filesystem review,
  a future Slack adapter, a future web dashboard, and Hermes;
- leave room for authenticated Slack decisions and Hermes follow-up tasks without
  changing the runner's process, worktree, artifact, or state ownership; and
- keep the graphics capability constrained enough that it is not a general
  remote desktop, shell, sudo bridge, or arbitrary command runner.

The primary native renderer is hardware WebGPU. SwiftShader is a fallback only
for explicitly approved infrastructure and profiles. Native Wayland SwiftShader
execution is prohibited, especially during unattended graphics validation on the
primary workstation. Fast2D remains in the repository temporarily as a deprecated,
opt-in diagnostic/legacy path, but it is excluded from Phase 2 launch profiles,
screenshot capture, visual review, and graphical validation.

### Non-goals

Phase 2 does not provide:

- a general-purpose remote desktop or screen-control service;
- arbitrary shell or Python execution as `greenfield-gfx`;
- caller-selected executable paths, unrestricted command arguments, or caller
  supplied environment variables;
- general sudo privileges for either service account;
- native Wayland SwiftShader experiments;
- graphical automation that clicks, types, or otherwise drives arbitrary UI;
- a second sandbox application or a change to the existing one-sandbox renderer
  selection model;
- changes to C++ engine or renderer code;
- Slack transport or Hermes implementation in this architecture slice; or
- automatic acceptance, merge, publication, or deployment of visible changes.

## 2. Existing ownership and Phase 2 extension

The current Python runner is the durable source of truth. It creates task records,
owns task worktrees and task branches, validates the manager checkout, runs
process groups with timeouts, captures Codex and validation logs, records Git
snapshots, and leaves worktrees and evidence in place for human review. Its
current states are `ready`, `running`, `review`, `blocked-codex`,
`blocked-validation`, and `blocked-runner`.

Phase 2 adds a graphics sub-lifecycle to a task rather than moving those
responsibilities into a broker or Hermes. The runner persists the request,
launch result, review bundle location, graphics state, decisions, and audit
events, and remains the lifecycle authority for timeout, cancellation, and
recovery. A broker is a cross-account execution delegate: it is the temporary
OS-level custodian of a `greenfield-gfx` process group, but it is not a task
database, durable process owner, or orchestration layer.

The existing `tools/runner/systemd/greenfield-task@.service.in` remains the
durable entry point for Python task execution. A future graphics broker may have
its own root-installed service or socket activation, but this document does not
authorize changing systemd state.

## 3. Trust boundaries and threat model

### Principals and protected resources

| Boundary | Trust assumption | Protected resource and consequence |
| --- | --- | --- |
| `greenfield-agent` | Unprivileged, task-running account; task prompts, worktrees, and build outputs are not trusted input | Must not gain sudo, general shell access as `greenfield-gfx`, or control of unrelated sessions |
| `greenfield-gfx` | Dedicated unprivileged graphical-session owner | Owns the display connection and launched graphics process groups; must not gain root or access to unrelated agent data beyond approved inputs |
| Root-installed configuration | Administratively trusted and immutable to both accounts | Defines socket ownership, profiles, approved roots, limits, and policy; neither account may rewrite it |
| Graphical session | Dedicated and disposable validation surface | Display and D-Bus credentials must not be borrowed from a stale or another user's session |
| Repository worktrees | Writable by the runner and therefore potentially adversarial from the broker's perspective | A graphics process may execute code from an approved build output; this must never imply broader account privileges |
| Artifact storage | Append-only handoff between broker and runner, then immutable review package | Screenshots, logs, metadata, and hashes must not be silently substituted after review |
| Future Slack service | External transport and identity provider; not an execution authority | Network input can request only an authenticated, allowed review decision |
| Future Hermes service | Orchestration and policy layer; not a process or worktree owner | Must not become a shell tunnel or bypass runner state transitions |

The highest-risk capability is intentional: launching a built graphics target as
`greenfield-gfx`. A malicious or compromised `greenfield-agent` can try to make
that target behave like an arbitrary program. The design therefore limits the
target and arguments, keeps the graphics account unprivileged and isolated, and
records the exact request and result. If stronger isolation is required, the
approved profile must launch inside a separate disposable sandbox or dedicated
graphics host; the broker must not pretend that a user-account boundary alone
turns untrusted binaries into trusted code.

### Threats and controls

| Threat | Required control |
| --- | --- |
| Command injection through task data, profile names, or feedback | Parse typed fields; use an argument vector; never invoke a shell; never interpret Slack text as commands |
| Arbitrary executable selection | Profile ID maps to a root-owned executable rule; no executable path in a request |
| Argument injection or resource exhaustion | Profiles own fixed arguments and bounded typed parameters such as one of a fixed set of capture sizes; reject unknown fields and length/count overflow |
| Environment injection | Construct a minimal environment from root-owned profile values and validated session values; do not merge caller environment |
| Path traversal | Allow only validated task IDs and named workspaces; canonicalize beneath approved roots; reject `..`, unexpected symlinks, non-regular files, and path escapes |
| Symlink and substitution attacks | Root-owned configuration directories; no-follow file opens where applicable; validate ownership/mode and immutable manifest/hash before launch; use a new request ID and exclusive artifact directory |
| Stale session variables | Never accept display variables from the request; bind them to a live registered session lease, boot ID, session ID, and server sockets; refuse on logout, restart, missing socket, or lease expiry |
| Cross-session or display theft | Dedicated `greenfield-gfx` session only; peer/session identity checks; profile display policy; no fallback to the caller's `DISPLAY` or Wayland variables |
| Forged review responses | Authenticate the transport and decision actor; allow only authorized reviewers; bind decision to task, review ID, and bundle digest; record the actor and raw provider identifiers |
| Replayed Slack events | Verify provider signature and freshness, persist provider event ID before applying a transition, and make the `(provider, event_id)` operation idempotent |
| Artifact substitution | Broker seals capture artifacts with hashes; runner verifies before import; bundle IDs are immutable; adapters publish by digest and never rebuild a review from mutable paths |
| Process escape or orphaning | Start one dedicated process group per request; enforce wall-clock and capture timeouts; terminate the group gracefully, then forcefully; record descendants that could not be confirmed gone |
| Denial of service | One request at a time per graphics session, bounded queue and artifact sizes, request deadlines, rate limits, and explicit rejection while another launch owns the session |
| Information leakage | Keep logs and screenshots under restricted task/artifact permissions; redact secrets from structured metadata; do not expose environment dumps or arbitrary filesystem paths to Slack |
| Broker compromise or configuration tampering | Root-owned installation/configuration, least-privilege service hardening, integrity checks, auditable profile changes, and human approval for profile additions |

This model does not protect against root, a compromised kernel, or a malicious
administrator. It also does not make code in an agent-owned worktree trustworthy;
the purpose of the boundary is to contain its privileges and make execution
explicit and auditable.

## 4. Recommended cross-account launch architecture

### Recommendation: typed Unix-domain-socket broker

Use a small broker running as `greenfield-gfx`, installed and configured by root,
with a root-owned Unix-domain-socket directory. The agent runner connects to the
socket and sends one length-bounded, versioned request per launch. The broker
verifies the peer credentials, validates the complete schema, resolves the
profile from root-owned configuration, starts the fixed profile process in the
dedicated session, and returns one structured response. The runner remains
responsible for the durable process lifecycle: it chooses the request deadline,
records the broker-owned process-group identity, requests cancellation, waits
for the structured result, and decides whether the task is running, blocked, or
ready for visual review.

The socket must not be a generic RPC endpoint. It exposes only operations such as
`register-session` (from the graphics-session owner), `launch-visual-review`,
`get-result` for an existing request ID, and `cancel` for the request owner.
There is no `exec`, `shell`, `set-environment`, `attach-display`, or arbitrary
file-read operation.

The socket directory and socket permissions must allow the intended agent peer
to connect while excluding other users. The broker must verify `SO_PEERCRED` (or
the platform-equivalent peer credential) and require the expected UID,
optionally also requiring a configured group. Peer credentials are an
authorization input, not a substitute for request validation.

### Typed request protocol

The wire representation may be canonical JSON initially, or a small binary
encoding later, but its semantics must be typed and versioned. A request has a
shape equivalent to:

```json
{
  "protocol_version": 1,
  "operation": "launch-visual-review",
  "request_id": "uuid",
  "idempotency_key": "task-id-attempt",
  "task_id": "safe-runner-task-id",
  "review_id": "uuid",
  "profile_id": "control-room-webgpu",
  "workspace_id": "runner-task-worktree",
  "capture_plan_id": "control-room-1280x720",
  "deadline_seconds": 180
}
```

The exact schema is a later implementation decision, but these rules are not:

- reject unknown protocol versions, operations, fields, and enum values;
- require bounded strings, UUID-shaped request/review IDs, and the runner's safe
  task-ID rules;
- require a unique request ID and an idempotency key scoped to the task/review;
- map `workspace_id` to an approved worktree/build root; never accept a raw
  executable path or a path outside that mapping;
- allow only profile-defined capture plans and bounded deadlines;
- reject duplicate idempotency keys unless the stored result is returned exactly;
- make request processing idempotent across reconnects and broker restarts; and
- return a structured result with status, exit reason, timestamps, artifact
  references, hashes, renderer/session metadata, and safe diagnostic details.

The request must not contain arbitrary arguments, an environment map, a shell
fragment, a display variable, a dynamic library path, a loader option, or a
secret. The broker constructs all of those values from policy and the live
session registration.

### Alternatives considered

| Alternative | Advantages | Problems |
| --- | --- | --- |
| Unix-domain-socket broker | Strong local peer identity, typed request/response semantics, explicit lifecycle and idempotency, natural fit for a long-running graphics owner | Requires a small daemon and careful socket/configuration permissions |
| Filesystem queue | Easy inspection and durable handoff; useful as an artifact/result spool | Weak notification semantics, races and symlink hazards, awkward peer identity, duplicate handling, and difficult cancellation; requires careful atomic claim protocol |
| systemd-mediated launch | Strong process cgroups, resource limits, logging, and lifecycle controls | A direct agent-to-systemd interface risks exposing unit/property/argument injection; user-systemd ownership does not naturally cross into the dedicated graphics session |
| Restricted helper | Small attack surface if it accepts fixed argv and policy IDs; can be socket or file activated | A setuid/root helper would be high risk and is unnecessary; a non-root helper still needs a broker/session boundary and must not become a generic launcher |

The Unix-domain-socket broker is preferred because it combines peer identity,
request/response semantics, bounded authorization, and a clean future adapter
boundary. Use systemd cgroups and a restricted helper internally if they improve
process isolation, but do not expose systemd or a helper as the public protocol.

### Broker result contract

The response must identify the same `request_id`, `review_id`, and idempotency
key, and include at least:

- accepted/rejected/running/completed status and a machine-readable reason;
- profile ID and resolved immutable profile version;
- start, readiness, capture, graceful-stop, forced-stop, and finish timestamps;
- process-group outcome, exit code, signal, timeout flags, and crash indication;
- artifact bundle reference and per-file content hashes;
- renderer/backend, display server, display/session identity, resolution, and
  adapter metadata; and
- a bounded human-readable diagnostic that contains no shell command or secret.

The result must never claim `approved`. Approval is a separate durable human
decision recorded by the runner.

## 5. Graphics-session ownership

`greenfield-gfx` obtains its session context from a dedicated graphical-session
bootstrap and registers that context with the broker. The agent never supplies
or overrides it. The bootstrap may be a future root-installed user service or
session startup contract; it is not implemented here.

Only the variables required by the selected profile are exposed to a launch:

- `XDG_RUNTIME_DIR` for the graphics user's runtime directory;
- `WAYLAND_DISPLAY` for a native Wayland profile;
- `DISPLAY` for an X11 profile;
- `DBUS_SESSION_BUS_ADDRESS` when the application requires the dedicated session
  bus; and
- `XAUTHORITY` for an X11 profile when authentication is required.

The broker must construct a clean environment, not inherit the requester's
environment. It must reject variables such as `LD_PRELOAD`, `LD_LIBRARY_PATH`,
`PYTHONPATH`, `PATH` overrides, `GDK_BACKEND`, `QT_QPA_PLATFORM`, arbitrary
proxy variables, or arbitrary `WEBGPU`/Dawn configuration unless a future
profile explicitly owns a fixed value for them.

### Registration and stale-state checks

A session registration should contain a broker-generated session/lease ID,
graphics UID, boot ID, login/session ID, registration time, lease expiry, and
the validated socket endpoints. The broker should validate:

1. `XDG_RUNTIME_DIR` exists, is a directory owned by `greenfield-gfx`, and has
   the expected restrictive permissions.
2. The Wayland socket or X11 display endpoint exists and is reachable by the
   graphics account, and matches the registered session rather than a caller's
   environment.
3. `DBUS_SESSION_BUS_ADDRESS` points to the dedicated session bus when required,
   with endpoints beneath the expected runtime directory where applicable.
4. `XAUTHORITY`, when used, is a regular file owned by `greenfield-gfx` with
   restrictive permissions and is not a symlink.
5. The session/login identity and boot ID are still live, the lease has not
   expired, and the display server has not been replaced since registration.

On logout, display-server restart, boot change, missing endpoint, ownership or
permission change, lease expiry, or failed readiness probe, the broker invalidates
the registration, refuses new launches, and marks active work `blocked-graphics`
after process cleanup. It must not guess a replacement display or reuse stale
environment files. A new session must explicitly register and pass the same
checks.

Native Wayland SwiftShader is not a valid profile. A future
`renderer-startup-probe-swiftshader-x11` profile may run only with explicit
owner approval on dedicated X11 infrastructure and must encode that policy in
the profile itself. It cannot be selected by changing a request field or
environment variable.

## 6. Typed launch profiles

A launch profile is a root-owned, versioned policy record. It maps a profile ID
to a fixed executable rule, fixed argument vector, approved workspace/build
mapping, renderer policy, session/display policy, resource limits, readiness
method, capture plan, and timeout policy. Profile IDs are an enum-like allowlist,
not arbitrary filenames.

Illustrative profiles are:

| Profile ID | Intended policy |
| --- | --- |
| `control-room-webgpu` | Existing Control Room target, hardware WebGPU, dedicated native session, fixed review capture sizes |
| `sandbox-webgpu` | Existing single sandbox app, hardware WebGPU, fixed renderer selection and no caller-defined options |
| `renderer-startup-probe-hardware` | Narrow startup/adapter probe using the hardware WebGPU path and a bounded diagnostic capture |
| `renderer-startup-probe-swiftshader-x11` | Explicit fallback probe on dedicated X11 infrastructure only; never native Wayland and never the primary unattended workstation |

The profile set must explicitly exclude:

- native Wayland SwiftShader;
- arbitrary executable paths or dynamically selected binaries;
- arbitrary shell commands, shell fragments, or command substitution;
- unrestricted arguments, including arbitrary renderer flags;
- unrestricted environment injection or dynamic library injection;
- launches outside approved worktrees and build outputs;
- launches from symlinked or path-escaped workspace/build directories; and
- hidden profile aliases that bypass renderer, display, timeout, or capture policy.

The broker should resolve an approved `workspace_id` to a canonical worktree
owned by the runner's configured root. It should validate the expected commit,
profile-specific output manifest, file type, ownership/mode policy, and optional
content digest before execution. Because the runner can modify its worktree, the
profile's account isolation and resource policy remain necessary even after
these checks.

## 7. Visual capture lifecycle

The runner owns the logical launch lifecycle for one graphics request; the
broker executes the cross-account spawn and is the temporary OS-level custodian
of the graphics process group. A future profile implementation should follow
this sequence:

1. **Validate.** Verify peer credentials, request schema, idempotency, live
   graphics-session lease, profile version, workspace mapping, and output
   manifest. Reject before spawning if any check fails.
2. **Prepare.** Create a new request/artifact staging directory with exclusive
   creation, record `launch-request.json`, and construct the profile-owned clean
   environment and fixed argv.
3. **Start.** Launch the fixed application with a new session/process group and
   capture stdout/stderr. Do not invoke a shell. Record the child PID, process
   group ID, and start time.
4. **Detect readiness.** Wait for a profile-defined readiness signal, such as an
   application readiness marker tied to the request nonce plus a bounded display
   or surface check. A mere process-alive check is insufficient. Refuse capture
   if readiness is not reached before the startup deadline.
5. **Stabilize.** Allow the fixed profile settling interval or frame boundary to
   complete. Do not poll arbitrary UI or synthesize input.
6. **Capture.** Capture one or more fixed-size screenshots specified by the
   profile. Capture plans may include a first stable frame, a second state after
   a deterministic application-owned transition, and a diagnostic frame. The
   request cannot choose arbitrary dimensions, output formats, or paths.
7. **Optional future recording.** A profile may later define bounded video or
   animation capture with a fixed duration, frame rate, and size. This is
   intentionally deferred and must not become general screen recording.
8. **Stop.** Request graceful application shutdown through the profile contract.
   If it does not exit by the deadline, send termination to the entire process
   group, wait the configured grace period, then force-kill the group.
9. **Finalize.** Record exit/crash/timeout information, close logs, hash every
   artifact, write `launch-result.json`, and seal the staging result. Preserve
   partial screenshots and logs on failure.
10. **Handoff.** Return the sealed result to the runner. The runner verifies the
    hashes and creates or imports the immutable review bundle before persisting
    the visual-review state.

All startup, capture, graceful-shutdown, forced-shutdown, total wall-clock, log,
and artifact-size limits are profile-owned and bounded. A crash, readiness
timeout, capture failure, invalid session, or forced termination is a diagnostic
outcome and cannot become visual approval.

## 8. Durable visual review package

Each visual review receives a new immutable bundle ID and a directory that is
never reused. The logical package contains at least:

```text
review-bundle/<review-id>/
  review.json
  summary.md
  screenshots/
    001-<capture-name>.png
    002-<capture-name>.png
  stdout.log
  stderr.log
  launch-request.json
  launch-result.json
  task.json
  git/
    expected-ref
    expected-commit
    worktree-head
    status.txt
    diff-stat.txt
    expected.diff
    untracked-files.json
  metadata/
    renderer.json
    display.json
    resolution.json
    adapter.json
  manifest.json
```

The existing runner's `metadata.json`, `state`, `final-report.md`, logs, and
`git/before`/`git/after` snapshots remain authoritative task evidence. The
visual bundle references those records and copies only the graphics-specific
artifacts needed by reviewers. `task.json` must include task ID, review ID,
expected ref/commit, current worktree HEAD, branch, timestamps, and runner
state. Renderer/session metadata must distinguish hardware WebGPU, approved
X11 SwiftShader fallback, and deprecated Fast2D; it must include display server,
resolution, adapter/device identity, and profile version.

`review.json` is the bundle index and audit record. It includes schema version,
task/review/request IDs, immutable profile and workspace identifiers, current
visual state, artifact manifest, content hashes, review decisions, and links to
the runner record. `summary.md` is concise human context, not an executable
instruction channel.

The broker seals its staging result with a manifest containing a cryptographic
hash and byte length for every file. The runner verifies that manifest while
importing into its task directory, refuses symlinks and unexpected files, and
uses atomic publication of the final bundle index. After publication, an
adapter may add delivery receipts in a separate audit record, but it must not
modify the captured package. This supports:

- manual filesystem review;
- a Slack adapter that uploads the exact sealed screenshots and summary;
- a future web dashboard that reads by bundle ID and digest; and
- Hermes consuming structured evidence without owning the files or process.

## 9. Visual review state machine

The runner should persist a graphics state alongside the existing task state.
The graphics state is the source of truth for visual work; the broker and
transport adapters report events but do not invent state.

```text
graphics-requested -> graphics-running -> visual-review
        |                    |                |
        v                    v                +--> approved
 blocked-graphics       blocked-graphics     +--> changes-requested
                                             +--> rejected
                                             +--> paused
```

Recommended meanings and transitions:

- `graphics-requested`: runner durably recorded an explicit profile request and
  has not yet received a successful launch result. It may retry the same
  idempotent request only according to a human-approved retry policy.
- `graphics-running`: the broker accepted the request and owns a live process
  group or capture lifecycle.
- `visual-review`: the immutable review package is complete and awaits a human
  visual decision.
- `approved`: an authenticated authorized reviewer approved this exact bundle
  digest. This is visual approval only; it is not merge or deployment approval.
- `changes-requested`: an authenticated reviewer requested changes. Freeform
  feedback is retained separately and may be used by Hermes to create a new
  explicit runner task.
- `rejected`: an authenticated reviewer rejected the visual result. This is
  terminal until a human creates a new explicit task/review.
- `paused`: a human deliberately paused review or execution. Resume requires a
  separate authorized action and a still-valid bundle/session policy.
- `blocked-graphics`: launch, session, capture, artifact, or infrastructure
  failure prevented a valid review. It is never equivalent to rejection or
  approval.

There must be no automatic `visual-review -> approved` transition, no approval
based on process exit code, and no approval based on a successful screenshot.
The runner may automatically persist `graphics-requested`,
`graphics-running`, `visual-review`, or `blocked-graphics` from trusted broker
results, but only an authenticated human decision may produce `approved`,
`changes-requested`, `rejected`, or human-directed `paused`.

The existing task state remains correlated but separate. A task can have a
successful code/validation `review` result and still be waiting in
`visual-review`; a visual `approved` decision does not silently change Git,
merge, or publication state. Any future runner-state extension must retain
backward-compatible audit history and explicit human transitions.

## 10. Future Slack workflow

Slack is a transport adapter over the immutable bundle and runner decision API.
It is not a launch path. The adapter should be split into four independently
auditable responsibilities:

### Outbound notifications

For each review, post one Slack thread with a concise summary and upload the
exact screenshots from the sealed bundle. Include task ID, review ID, commit,
renderer/profile, platform/display server, resolution, and a link or reference
to the durable review. Record the channel, root message timestamp, thread
timestamp, uploaded file/message IDs, bundle digest, and delivery timestamps.

Retries must be idempotent: a review has one logical thread, and a retry updates
or completes the recorded outbox operation rather than creating an uncontrolled
set of threads.

### Inbound structured decisions

Use Slack interactive controls or another structured action to represent
Approve, Request Changes, Reject, and Pause. The inbound adapter must:

- authenticate Slack signatures/tokens and enforce a freshness window;
- authenticate and authorize the responding Slack user under a project-owner
  policy;
- validate the review ID, thread ID, bundle digest, and currently allowed
  transition;
- persist Slack event ID before applying the transition, with a unique
  provider/event key to reject replay;
- record event ID, event timestamp, user ID/name as permitted, channel, root and
  response message/thread identifiers, received time, decision, and resulting
  runner state; and
- call a narrow runner decision boundary rather than writing state files or
  invoking commands directly.

An already-applied event must return its recorded result. A stale event, event
for another thread/review, unauthorized user, invalid transition, or mismatched
bundle digest must be rejected and audited without changing runner state.

### Freeform feedback ingestion

Store thread replies and reviewer comments as immutable feedback records linked
to the review and message identifiers. Treat the text as untrusted prose. Never
convert it directly to shell commands, executable arguments, environment
variables, or a launch profile.

### Hermes follow-up task creation

Hermes may interpret approved policy plus freeform feedback and propose a new
explicit durable task. The new task must have a new task ID, copied prompt,
validation contract, expected ref/commit policy, parent review ID, and audit link.
The runner must create and own it through its normal task lifecycle. A Slack
message or Hermes interpretation must never mutate an existing worktree or
silently restart a review.

These four responsibilities should have separate credentials, queues or
outboxes, failure handling, and audit records. A future Slack outage must not
change a local review state or erase the review package.

## 11. Hermes boundary

Hermes is an orchestration and policy layer. It can select or propose explicit
tasks, sequence approved workflow steps, consume structured runner results, and
route human feedback. It must not own the execution primitives.

The Python runner retains ownership of:

- repository worktrees, task branches, and Git diagnostics;
- process launch requests, deadlines, cancellation, and the durable lifecycle
  of every process. The broker is only the delegated OS-level process-group
  custodian required to run as `greenfield-gfx`;
- code and graphics validation;
- graphics request IDs, profiles, broker calls, and artifact imports;
- review bundles and content hashes;
- task and graphics state transitions; and
- human-review decisions and their complete audit trail.

Hermes communicates through typed runner operations and immutable result
records. It never receives a shell, a raw executable path, a mutable worktree
handle, or permission to write state files. If Hermes is unavailable, the local
runner can still complete a graphics request and preserve a filesystem review.

## 12. Narrow implementation roadmap after approval

Each slice below must remain independently reviewable. No slice authorizes
launching graphics on the primary workstation before its human approval point.

| Slice | Scope | Required tests/security checks | Human approval point |
| --- | --- | --- | --- |
| 1. Broker skeleton and protocol | Versioned schema, bounded framing, peer credentials, request IDs, idempotency, structured errors; no process launch | Parser fuzz/property tests, unknown-field rejection, size limits, peer-UID tests, replay/idempotency tests, socket permission review | Approve protocol and threat-model review before any executable launch |
| 2. Graphics-session registration | Dedicated `greenfield-gfx` lease, required-variable validation, stale-session invalidation | Ownership/mode tests, socket endpoint tests, logout/restart/boot-ID tests, stale lease tests, no caller-environment tests | Owner approves session bootstrap and primary-workstation policy |
| 3. Allowlisted launch profiles | Root-owned profile records, fixed argv/environment, workspace/build mapping, renderer/display policy | Profile schema tests, path/symlink traversal tests, forbidden-argument/env tests, native Wayland SwiftShader rejection, profile integrity checks | Owner approves each profile and its executable/build trust model |
| 4. Process lifecycle | New process group, startup/readiness deadlines, graceful and forced shutdown, crash classification | Fake-process lifecycle tests, orphan/child tests, timeout escalation, cgroup/resource-limit inspection | Approve evidence that no process survives cancellation unexpectedly |
| 5. Screenshot capture | Fixed capture plans/sizes, readiness stabilization, log/artifact staging | Deterministic capture fixtures, size/format limits, partial-failure preservation, no arbitrary screen/input access | Human inspects first captures on dedicated graphics infrastructure |
| 6. Review package generation | Immutable bundle, manifest/hashes, runner import, task/Git/renderer metadata | Hash tamper tests, no-follow/symlink tests, atomic publication, duplicate bundle tests, permission review | Approve package contents and retention policy |
| 7. Runner integration | Durable graphics states, broker request/result persistence, blocked/paused handling, no auto-approval | Transition matrix, crash/restart recovery, idempotent reconnect, stale result rejection, existing runner regression suite | Owner approves the human gate and terminal-state policy |
| 8. Slack outbound adapter | One thread per review, summary/screenshots, delivery outbox and receipts | Mock Slack contract tests, retry/idempotency tests, bundle digest verification, redaction checks | Approve channel, retention, and notification policy |
| 9. Slack inbound decisions | Structured controls, authenticated users, replay rejection, decision audit | Signature/freshness tests, event dedupe, authorization/transition tests, thread/review binding | Owner approves reviewer authorization and decision semantics |
| 10. Hermes integration | Typed result consumption and explicit follow-up task creation | Contract tests, prompt/task provenance tests, no-shell/no-state-write tests, outage isolation | Owner approves policy for translating feedback into new tasks |

Every implementation slice should include documentation updates, negative
security tests, durable recovery tests, and a manual review of captured logs and
artifacts. The existing runner suite and non-graphical validation must remain
green. No implementation slice should add a second sandbox app, alter C++
renderer boundaries, launch or capture Fast2D, perform Fast2D visual review, or
exercise native Wayland SwiftShader.

## 13. Open questions and recommended defaults

### Recommended now

- Use a root-installed Unix-domain-socket broker running unprivileged as
  `greenfield-gfx`; verify the connecting UID and keep the socket directory
  root-owned.
- Make the request protocol versioned, typed, length-bounded, and strict about
  unknown fields.
- Use profile IDs, workspace IDs, capture-plan IDs, request IDs, and idempotency
  keys; never accept executable paths, arbitrary argv, or arbitrary env.
- Keep the dedicated graphics session registered by `greenfield-gfx`, not
  supplied by `greenfield-agent`.
- Make hardware WebGPU the only primary native profile; prohibit native Wayland
  SwiftShader; reserve SwiftShader for an explicit X11/dedicated-infrastructure
  fallback profile.
- Keep Fast2D deprecated and exclude it from Phase 2 launch profiles,
  screenshot capture, graphical validation, and visual review.
- Make review bundles append-only and hash-sealed, with runner-owned durable
  state and a transport-neutral package layout.
- Require explicit authenticated human decisions for every visible review.
- Keep Slack and Hermes outside the process-launch and state-ownership boundary.

### Intentionally deferred

- JSON versus a binary protocol encoding, provided the typed semantics and
  strict validation remain unchanged;
- exact socket activation/service hardening directives;
- the display capture mechanism and readiness signal for each approved app;
- profile output-manifest and executable-digest policy;
- artifact retention, encryption, and remote storage details;
- Slack channel/thread formatting, bot identity, and dashboard URL scheme;
- reviewer roster and organization-wide authorization policy;
- whether a future dedicated host or VM is required for untrusted graphics
  binaries; and
- bounded future video/animation capture.

### Requires project-owner approval

- granting the broker permission to execute agent-produced build outputs as
  `greenfield-gfx`;
- the root-owned profile list and every profile's executable, renderer, display,
  resource, and capture policy;
- the dedicated graphics-session bootstrap and whether the primary workstation
  may run any unattended graphics at all;
- any SwiftShader X11 infrastructure and its isolation/retention policy;
- reviewer identities and which Slack actions can produce `approved`,
  `changes-requested`, `rejected`, or `paused`;
- artifact retention and whether screenshots may leave the host; and
- the policy allowing Hermes to create follow-up tasks from freeform review
  feedback.

Until these decisions are approved, the safe behavior is to reject graphics
requests and leave the runner in a durable blocked or awaiting-review state.
