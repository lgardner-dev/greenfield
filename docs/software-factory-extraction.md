# Greenfield Automation Platform Extraction

Status: proposed architecture and migration plan. This document does not move
files, create a repository, install a package, change a service, or change
runner behavior. It records the design to approve before implementation.

The recommended direction is to extract the automation platform now, after
Phase 2B1. Greenfield is the first reference consumer and the first complete
end-to-end test project. The extracted factory is reusable by Greenfield, APG,
apgv2, and future projects, while each project retains authority over its own
build, validation, prompt, graphics, and review policy.

## 1. Product boundary

The product is a standalone software factory: a durable execution service and
library that turns an approved task request into an isolated run, executes one
or more approved agents and validations, retains evidence, and exposes a
reviewable result. It is not a project build system, an application launcher
with arbitrary command execution, a chat system, a source-of-truth database
for external services, or an all-purpose orchestration framework.

The boundary has three ownership zones:

1. The factory repository owns reusable execution mechanics and their durable
   contracts.
2. A consuming project owns declarative policy and project-specific adapters.
3. Host administration owns installation, service accounts, secrets,
   capacity, and host-level security.

### Ownership matrix

| Area | Standalone factory core | Consuming project | Host administration |
| --- | --- | --- | --- |
| Task records and state transitions | Owns schema, atomic writes, recovery, and state machine | Supplies task inputs and policy references | Owns storage ownership and backup policy |
| Git and worktrees | Owns safe Git operations, worktree lifecycle, snapshots, and isolation | Supplies repository identity, branch policy, expected-ref rules, and protected paths | Owns repository checkout placement and credentials |
| Processes | Owns fixed-argv spawning, process groups, timeouts, signals, logs, and cleanup | Selects an allowlisted agent, validation, or graphics profile | Owns service account, limits, and OS supervision |
| Agents | Owns adapter interface and generic Codex/agent implementations | Selects profiles, prompts, model policy, and allowed capabilities | Owns installed agent binaries and host credentials |
| Validation | Owns lifecycle, timeout, result capture, and report linkage | Defines CMake, tests, linters, smoke checks, and success criteria | Provides toolchain and dependency installation |
| Artifacts and review packages | Owns immutable manifests, hashing, retention hooks, and durable paths | Defines required evidence and reviewer gates | Owns permissions, storage capacity, and backup |
| Graphics broker | Owns protocol, peer authorization primitives, request ledger, and broker interface | Defines approved launch profiles, screenshots, readiness, and review rules | Owns graphics account, session, socket, display, and service installation |
| GitHub | Owns adapter interface, bounded request mapping, and idempotency | Selects repository, labels, checks, and approval policy | Supplies scoped token/app credentials |
| Slack | Owns transport adapter interface and delivery/retry behavior | Selects channels, thread routing, and message policy | Supplies scoped webhook/app credentials |
| Hermes | Owns a thin plan/intention integration boundary | Defines project plan policy and allowed factory operations | Supplies deployment and identity configuration |
| Installation | Owns release format, entry points, compatibility metadata, and upgrade protocol | No project-specific installer logic | Owns prefixes, users, units, permissions, and rollback execution |

The factory core must remain usable without GitHub, Slack, Hermes, or a live
graphics session. Those integrations are optional adapters around the durable
local factory lifecycle.

### Explicit boundaries

The standalone factory owns:

- request validation and a versioned task model;
- project registration and configuration loading;
- task state transitions and durable metadata;
- project-scoped locks and bounded host capacity;
- Git repository verification, branch creation, worktree creation, and Git
  diagnostics;
- fixed argument-vector process execution, process groups, deadlines, and
  cleanup;
- agent adapter invocation and validation-profile invocation;
- durable stdout, stderr, event, timing, metadata, report, log, and artifact
  handling;
- visual-review request protocol and graphics-broker primitives;
- immutable review packages, manifests, hashes, and local review state;
- compatibility, doctor, bootstrap, upgrade, and recovery commands; and
- stable interfaces for queue, notification, review, and orchestration
  adapters.

The consuming project owns:

- project identity and repository location;
- repository cleanliness, branch, expected-ref, commit, and push policy;
- executable IDs and fixed argument arrays for its approved build and test
  profiles;
- agent profile selection, prompt guidance, model and network policy;
- graphics profile selection, display/readiness policy, capture sizes, and
  prohibited launch modes;
- evidence requirements and human review gates;
- source architecture, build system, renderer, application, sandbox, and
  project-specific terminology; and
- the mapping from project events to GitHub, Slack, and Hermes adapters.

The host administrator owns:

- the factory service account and any separate graphics account;
- immutable factory releases and the `current` release pointer;
- systemd units, process limits, network policy, filesystem ownership, and
  backups;
- per-project credentials and secrets; and
- the host's capacity and graphical-session lifecycle.

The factory must not import a consuming project's Python package to discover
policy. Project policy is data loaded through a validated schema. A future
extension point may add a separately reviewed factory adapter, but arbitrary
project code is outside this extraction and must not be part of the initial
contract.

## 2. Proposed standalone repository

The proposed repository name is `software-factory`. The Python distribution
name is `software-factory`, the import package is `software_factory`, and the
primary executable is `factory`. The old `greenfield-agent` name is a
temporary compatibility entry point only.

### Repository layout

```text
software-factory/
  pyproject.toml
  src/software_factory/
    __init__.py
    __main__.py
    cli.py
    version.py
    config/
      loader.py
      schema.py
      validation.py
      migrations.py
    identity/
      project_ids.py
      task_ids.py
      namespaces.py
    runner/
      models.py
      state.py
      lifecycle.py
      recovery.py
      locking.py
    git/
      repository.py
      worktrees.py
      diagnostics.py
    process/
      argv.py
      lifecycle.py
      limits.py
    agents/
      interface.py
      codex.py
      registry.py
    validation/
      interface.py
      profiles.py
      runner.py
    artifacts/
      store.py
      manifests.py
      review_packages.py
      retention.py
    graphics_broker/
      protocol.py
      framing.py
      credentials.py
      ledger.py
      client.py
    adapters/
      queue.py
      github.py
      slack.py
      review.py
      hermes.py
    installation/
      release.py
      compatibility.py
      doctor.py
  bin/
    factory
    greenfield-agent-compat
  systemd/
    factory-task@.service.in
    factory-broker.service.in
  installer/
    install.sh
    upgrade.sh
    uninstall.sh
  schemas/
    project-config-v1.schema.json
    task-v1.schema.json
    run-v1.schema.json
    artifact-manifest-v1.schema.json
    compatibility-v1.schema.json
  tests/
    unit/
    integration/
    security/
    installation/
    protocol/
    fixtures/
  examples/
    minimal-project/
    greenfield-reference/
    multi-project-host/
  docs/
    architecture.md
    project-contract.md
    installation.md
    adapter-authoring.md
    operations.md
    provenance.md
    compatibility.md
    release-process.md
```

The first extraction can preserve the current Python standard-library posture.
If schema validation needs a third-party dependency, it must be explicit in
`pyproject.toml`, included in the immutable release, and covered by the
installer and offline/self-hosting tests. The factory must not require the
consuming project's C++ dependencies, SDL, Dawn/WebGPU, FreeType, or a
graphics desktop to run its core tests.

### Packaging and entry points

Use a normal `pyproject.toml` package with a `src/` layout. At minimum it
should declare:

```toml
[project]
name = "software-factory"
requires-python = ">=3.10"
dynamic = ["version"]

[project.scripts]
factory = "software_factory.cli:main"
greenfield-agent = "software_factory.compat.greenfield_agent:main"
```

The exact build backend is deferred until E1, but the distribution must be
buildable without copying source files into an installation by hand. A source
checkout may continue to provide `python -m software_factory` for tests.

Use Semantic Versioning for the factory distribution. Version compatibility is
not inferred only from the package version: every release also publishes
explicit compatibility declarations for the factory API, project-config
schema, task/run state schema, artifact schema, graphics protocol, and
compatibility wrapper. A patch release may fix behavior without changing
schemas; a minor release may add backward-compatible fields and profiles; a
major release may remove a contract only after the declared migration window.

Schema versions are independent integers or semantic identifiers and are
stored in every durable record. A release must declare both the schemas it
writes and the older schemas it can read.

## 3. Project contract

Each consuming repository has a project-owned `.factory/` directory at its
repository root. The factory loads it from the registered repository at a
specific commit or from an explicitly registered host configuration copy;
that choice is recorded in the run. The configuration is declarative and
validated before a task can run.

The recommended starter layout is:

```text
.factory/
  project.json
  repository.json
  compatibility.json
  adapters.json
  review.json
  prompts/
    default.md
    visual-review.md
  agents/
    codex-default.json
  validation/
    headless.json
    smoke.json
  graphics/
    control-room-webgpu.json
  schemas/
    README.md
```

The factory owns the schema definitions and validates unknown fields, type
errors, path escapes, unsafe identifiers, unsupported commands, and
incompatible versions. A project may add descriptive metadata, but it may not
replace required security fields or weaken host policy.

### Required configuration records

The following records are recommended for version one. The examples are
illustrative shapes, not an implementation commitment to a particular JSON
library.

`project.json` identifies the project and its owning repository:

```json
{
  "schema": "factory.project.v1",
  "project_id": "greenfield",
  "display_name": "Greenfield",
  "repository": "greenfield",
  "owner": "greenfield-maintainers",
  "default_agent_profile": "codex-default",
  "default_validation_profile": "headless",
  "default_review_policy": "greenfield-ui",
  "enabled": true
}
```

`repository.json` describes safe repository operations. It contains a
registered repository key or canonical path, an expected default ref policy,
branch prefix, allowed worktree root, clean-checkout requirement, and whether
the agent may create commits. It does not allow the task prompt to override
those values.

```json
{
  "schema": "factory.repository.v1",
  "repository_key": "greenfield",
  "manager_checkout": "/srv/software-factory/projects/greenfield/repository",
  "allowed_worktree_root": "/srv/software-factory/projects/greenfield/worktrees",
  "branch_prefix": "factory/greenfield/",
  "require_clean_manager_checkout": true,
  "allow_agent_commit": false,
  "allow_agent_push": false,
  "expected_ref_source": "request-or-registered-default"
}
```

Paths in project configuration are either registered path IDs or canonical
paths under the project's approved roots. They are not accepted as arbitrary
task-controlled paths.

`validation/<name>.json` defines an allowlisted validation profile. It records
an executable ID, a fixed argument array, a working-directory root, timeout,
environment names from a small factory-owned allowlist, expected outputs, and
the result policy. It must not contain a shell string, command substitution,
unbounded environment map, or arbitrary dynamic arguments.

```json
{
  "schema": "factory.validation.v1",
  "profile_id": "headless",
  "executable_id": "bash",
  "arguments": ["tools/runner/run-tests"],
  "working_directory": "worktree",
  "timeout_seconds": 3600,
  "network": "disabled",
  "required_artifacts": ["logs/validation-stdout.log", "logs/validation-stderr.log"],
  "success_exit_codes": [0]
}
```

The Greenfield reference adapter will use fixed profile data for CMake
configure/build/CTest and for the existing runner and graphics-broker tests.
The generic factory only knows that a profile is a bounded fixed-argv action.

`agents/<name>.json` selects an agent adapter and its bounded policy:

```json
{
  "schema": "factory.agent.v1",
  "profile_id": "codex-default",
  "adapter": "codex",
  "executable_id": "codex",
  "fixed_arguments": ["--json"],
  "sandbox": "workspace-write",
  "approval_policy": "never",
  "network": "disabled",
  "timeout_seconds": 14400,
  "reasoning_effort": "high",
  "prompt_file": "prompts/default.md"
}
```

The adapter, executable, and fixed arguments are selected from host-registered
allowlists. A project may choose among approved profiles, but may not add an
arbitrary executable path, shell, dynamic library path, or environment
variable through this file.

`graphics/<name>.json` defines a graphics policy, not a free-form launch:
profile ID, approved application target, workspace/build mapping, renderer
policy, graphics-session requirement, readiness method, fixed capture plan,
dimensions, stabilization interval, timeouts, output manifest, and prohibited
modes. It references the factory graphics broker protocol and does not embed
display credentials.

`review.json` defines required review gates and evidence:

```json
{
  "schema": "factory.review.v1",
  "policy_id": "greenfield-ui",
  "required_gates": ["validation", "human-code-review", "human-visual-review"],
  "required_artifact_kinds": ["final-report", "git-diagnostics", "screenshots"],
  "approval_is_external": true,
  "failure_is_terminal": true
}
```

`prompts/*.md` contains project guidance as data. The factory combines a
factory-owned system contract, project-owned guidance, and task content into a
durable prompt snapshot. The task content cannot remove the factory safety
instructions or project review requirements.

`adapters.json` routes logical events to optional adapters:

```json
{
  "schema": "factory.adapters.v1",
  "queue": {"adapter": "local", "enabled": true},
  "github": {"adapter": "github", "enabled": false, "repository": "greenfield/greenfield"},
  "slack": {"adapter": "slack", "enabled": false, "channel": "greenfield-factory"},
  "hermes": {"adapter": "hermes", "enabled": false}
}
```

Disabled adapters must not be contacted. Adapter configuration maps local
project IDs and durable event IDs to external records; it does not make an
external service authoritative for task state.

`compatibility.json` declares the minimum and maximum factory contracts the
project accepts:

```json
{
  "schema": "factory.compatibility.v1",
  "factory_api": {"minimum": "1.0", "maximum": "1.x"},
  "project_config": "factory.project.v1",
  "task_state": ["factory.task.v1"],
  "artifact_manifest": ["factory.artifact.v1"],
  "graphics_protocol": ["factory.graphics.v1"]
}
```

The factory validates compatibility before registering a project and records a
configuration digest and release compatibility declaration in every run.

### Configuration ownership and reload

Project owners may edit project policy in the repository, subject to review.
Host administrators own the registered project record, service account,
credential bindings, executable allowlist, resource ceilings, and security
policy. The factory takes an immutable configuration snapshot at task creation
or run start, records its digest, and never changes the meaning of an active
run because a file was edited later.

Configuration reload is explicit (`factory project register` or a future
`factory config reload`) and validates the complete project before activation.
An invalid new configuration leaves the last known-good configuration active.

## 4. Multi-project identity and paths

`project_id` is a required factory identity, not a display name. It is a
lowercase ASCII slug matching `^[a-z][a-z0-9-]{0,63}$`, is never inferred
from a repository directory, and is immutable after registration. The
registered project record maps it to exactly one approved repository and one
service-account/secret scope. Renaming a project creates an explicit migration
or a new registration; it does not silently rebind paths.

`task_id` remains a project-local safe identifier. A recommended form is
`^[A-Za-z0-9][A-Za-z0-9._-]*$` with the current restrictions against `..`, a
trailing `.`, and a trailing `.lock`. The factory addresses a task as the
tuple `(project_id, task_id)`, even if the CLI permits a compact spelling.

Every durable record contains both fields and a schema version. No record is
looked up by `task_id` alone.

### Namespacing rules

| Object | Required scope and identity |
| --- | --- |
| Task record | `(project_id, task_id)`; task IDs need only be unique within a project |
| Lock | A project lock or task lock includes `project_id`; host admission locks have a separate factory namespace |
| Worktree | `/projects/<project_id>/worktrees/<task_id>`; canonicalized and checked beneath the project root |
| Run | `/projects/<project_id>/runs/<task_id>/<run_id>`; retries never reuse a run directory |
| Artifact | `/projects/<project_id>/artifacts/<task_id>/<run_id>/<artifact_id>`; immutable manifest includes both IDs |
| Review ID | Factory-generated globally unique ID, with project/task/run fields in the review record |
| GitHub record | External ID plus `(project_id, task_id, run_id)` mapping; one project credential and repository scope |
| Slack thread | External channel/thread ID plus local mapping keyed by project/task/run; Slack is transport only |
| Hermes plan | External plan ID plus local mapping; plan cannot own a worktree, process, or authoritative state |
| Idempotency key | Hash or canonical record containing project, operation, task, run/request nonce, and adapter scope |

External IDs are never used as local path components without validation and a
factory-generated mapping record. Names and labels sent to external services
may contain project display names, but local identity always uses `project_id`.

### Recommended host layout

```text
/srv/software-factory/
  bin/
    factory
    greenfield-agent
  lib/software-factory/
    releases/<factory-version>/
    current -> releases/<factory-version>
  projects/
    greenfield/
      repository/          # approved manager checkout or registered checkout
      runs/
      worktrees/
      artifacts/
      logs/
      state/
        project.json
        locks/
      cache/
    apg/
      repository/
      runs/
      worktrees/
      artifacts/
      logs/
      state/
      cache/
  registry/
    projects/<project_id>.json
    factory-capacity.json
  shared/
    protocol-ledgers/
    cache/                  # only where explicitly safe to share
  backups/
```

Host-owned configuration and secrets should be separate from mutable task
data:

```text
/etc/software-factory/
  factory.json
  projects/<project_id>/
    registration.json
    secrets-bindings.json
    .factory/              # optional host-pinned copy of project policy
  systemd/
```

The project service account can read its own project data and approved
configuration, but not another project's repository, worktrees, runs,
artifacts, or secret bindings. Root-owned installation files and host policy
are not writable by the project service account. A single shared `state` area
must not become a cross-project data directory.

### Locking model

The current runner's exclusive nonblocking lock proves that two task
lifecycles cannot execute concurrently. The standalone factory preserves that
primitive while making its scope explicit:

- a short-lived registry lock protects project registration and host metadata;
- a project scheduler lock protects one project's queue/state index;
- a task/run lock prevents duplicate execution of one `(project_id, task_id,
  run_id)`;
- a graphics resource lock protects a particular graphics account/session or
  profile-defined scarce resource; and
- an optional host capacity semaphore limits total work without holding a
  single global lock for an entire task.

The Greenfield compatibility profile may retain the old single global
execution lock while old tasks are active. New project profiles must not use a
long-held cross-project lock: a blocked Greenfield task must not prevent an APG
task from creating metadata, running non-graphics validation, or acquiring
its own project resources. Lock files are created with exclusive/no-follow
semantics, contain owner metadata, and are released by process exit or
explicit stale-owner diagnosis; stale locks never cause an unrelated project
to be treated as the owner.

## 5. Security boundaries

The factory's security posture is defense in depth. Project configuration is
trusted policy input only after validation; task prompts, queue payloads,
external events, and files in a worktree are untrusted.

### Filesystem and path isolation

- Resolve every registered root and reject symlinked or path-escaped final
  components where the operation requires a regular file or directory.
- Require project data directories to be owned by the expected service account
  and have approved modes before execution.
- Verify that repository, worktree, run, artifact, and log paths remain beneath
  the registered project root after canonicalization.
- Reject `..`, absolute task-controlled paths, alternate separators, device
  paths, symlink escapes, and unexpected file types.
- Never accept a path from a task request for an output artifact, executable,
  display endpoint, or secret.
- Keep artifact manifests and hashes in the project artifact root; do not use a
  shared temporary directory as a cross-project exchange mechanism.

### Process and configuration safety

- Spawn fixed argument arrays with `shell=False`, a clean factory-built
  environment, a project-approved working directory, a new process group, and
  bounded wall-clock/resource limits.
- Do not support shell fragments, command substitution, pipelines, arbitrary
  executable paths, arbitrary environment maps, `LD_PRELOAD`, dynamic library
  injection, or task-selected renderer/display variables.
- Project profiles reference factory/host-registered executable IDs and fixed
  argument arrays. Any variable substitution is from a closed set of typed
  values such as the canonical worktree path, task ID, or approved artifact
  directory, with escaping and root checks.
- Prompts are data. They cannot alter approval policy, network policy,
  executable selection, timeout ceilings, credential bindings, or review
  requirements.
- Configuration is schema-validated and versioned. Security-sensitive host
  policy is root/admin-owned and cannot be weakened by a repository commit.

### Credentials and external services

- Each project has separate Git credentials or SSH identities, scoped to its
  registered repository and account. The factory must not copy one project's
  credential environment into another project's process.
- Secrets are injected by host binding into the adapter that needs them, not
  written into task prompts, metadata, logs, artifacts, Slack messages, or
  worktrees. Secret values are never returned by `status` or `doctor`.
- Graphics credentials and display environment belong to the dedicated
  graphics account/session. A project request may select an approved graphics
  profile, but cannot provide `DISPLAY`, `WAYLAND_DISPLAY`, `XDG_RUNTIME_DIR`,
  DBus, Xauthority, or library-path values.
- GitHub authorization is per project and repository. The GitHub adapter can
  create or update only records allowed by that project's registration.
- Slack routing is per project/channel policy. A Slack message is a delivery
  attempt and review link, never the durable state source.
- Hermes receives or submits bounded plans/intents through a project-scoped
  adapter. It cannot access another project's configuration, process, or
  worktree and cannot directly write factory state.

### Availability and failure isolation

- Project queues, state indexes, locks, and artifact roots are separate.
- A malformed project configuration fails registration for that project only.
- Adapter retries use per-project idempotency keys and bounded backoff; a
  failed Slack or GitHub endpoint cannot block local task finalization.
- Graphics-account or session failure blocks only tasks that require that
  resource; non-graphics tasks in other projects remain runnable.
- Capacity limits are explicit and observable. They are not implemented as a
  shared unbounded queue or a lock held by an external adapter.
- Review packages are append-only/immutable after sealing. A project cannot
  replace another project's package by guessing an artifact or review ID.

## 6. Greenfield adapter

Greenfield remains a project repository and first reference consumer. Its
project adapter should contain policy, not copies of factory lifecycle code.
The adapter is implemented as `.factory/` data plus a small set of reviewed
host registrations if a policy cannot safely live in the repository.

### Greenfield-owned policy

The Greenfield adapter must define:

- CMake configure, build, and CTest commands for the `developer` and
  `headless-fast2d` profiles;
- the existing dependency-light `tools/runner/run-tests` and
  `tools/graphics_broker/run-tests` commands as fixed validation actions;
- expected repository cleanliness, expected-ref, branch, and no-push/no-merge
  rules;
- the Control Room and single sandbox targets, their approved working/build
  directory mappings, and the permitted `--renderer=webgpu` selection;
- hardware WebGPU launch profiles and bounded readiness/capture plans;
- the fixed screenshot sizes currently used for review, including
  1280x720, 1600x900, and 1920x1080 where a profile requires them;
- application-owned readiness markers and profile-defined stabilization rules;
- the prohibition on Fast2D visual review and the prohibition on native
  Wayland SwiftShader;
- any explicitly approved dedicated X11 SwiftShader startup-probe profile,
  if a future owner approves that infrastructure;
- Greenfield architecture guidance covering SDK-first boundaries, renderer
  neutrality, UI/platform/backend separation, and prohibited coupling; and
- the UI and architecture review policy, including required human visual review
  for visible changes and the required review package contents.

The adapter may describe a Fast2D headless/diagnostic validation command, but
it must not mark Fast2D as an allowed visual-review renderer. This preserves
the current project policy: WebGPU is the interactive visual path; Fast2D is
an opt-in diagnostic/legacy path, not a visual approval path.

The adapter must keep the native Wayland SwiftShader prohibition explicit in
the graphics profile rather than relying on a prompt or an operator convention.
The profile must reject caller-provided renderer flags and display variables.

### Factory-owned Greenfield integration

The factory consumes the adapter through the same interfaces as APG and
apgv2:

1. Register `project_id=greenfield` with its repository and host roots.
2. Validate and snapshot `.factory/` against the installed factory release.
3. Accept a project-scoped task request.
4. Create the Greenfield run, branch, and worktree through generic Git
   mechanics.
5. Invoke the selected Codex/agent profile with Greenfield prompt guidance.
6. Run Greenfield's fixed validation profile.
7. If required, submit a typed visual-review request to the graphics broker.
8. Seal the factory-owned review package and expose it to the human reviewer.
9. Optionally publish bounded status links through GitHub, Slack, or Hermes.

The Greenfield adapter must not own worktree creation, process groups,
artifact sealing, task state transitions, Slack truth, or Hermes process
control. The current `greenfield-agent` command becomes a compatibility wrapper
that selects this registered project and maps legacy arguments/environment to
the generic `factory` command.

## 7. Migration plan

The stages are deliberately narrow. Each stage has an explicit approval point;
no stage should silently combine extraction with new graphics execution,
GitHub, Slack, or Hermes behavior.

### E0 — Contract freeze

**Scope:** Approve this boundary, naming, identity model, path layout,
configuration contract, security invariants, artifact/review ownership, and
compatibility policy. Record the current Phase 1 and Phase 2B1 behavior as
the baseline.

**Tests:** Documentation/schema review; inventory current runner and broker
tests; map every current durable file and state transition to a target
contract; produce a state/config compatibility matrix.

**Compatibility:** No runtime change. Existing `greenfield-agent`, installed
releases, task IDs, state strings, paths, artifacts, and broker protocol remain
the baseline.

**Human approval:** Greenfield owners approve the adapter boundary; factory
owners approve the generic contract; host administrators approve the security
and installation assumptions.

**Rollback:** No code or data changed. Reject the design or revise the
document.

### E1 — Extraction preparation and empty remote

**Scope:** Define the exact retained Greenfield paths and history, create a
repeatable `git filter-repo` script, prepare the provenance manifest, and audit
the planned result for secrets and unrelated project content. Create an empty
private `software-factory` remote without an initial commit. Do not yet add
package metadata, restructure files, install software, or change runtime
behavior.

**Tests:** Run the extraction repeatedly against disposable local clones and
verify identical resulting refs and trees. Inventory every retained file and
commit, scan retained history for secrets and unrelated application content,
and prove the source Greenfield checkout and remote remain unchanged.

**Compatibility:** No replacement package or installed release exists yet.
The Greenfield checkout and installed runner remain the only execution source
of truth.

**Human approval:** Review the retained-path allowlist, extraction script,
complete filtered tree, secret-audit result, and provenance plan before any
history is pushed.

**Rollback:** Delete the disposable clone or empty remote. No Greenfield code,
history, installation, or durable state has changed.

### E2 — Filtered history bootstrap and package extraction

**Scope:** Run the approved one-time history extraction in a temporary clone,
audit the complete result, add the provenance manifest, and push the filtered
history as the initial history of the empty `software-factory` repository.
Only after that history is established, add package metadata, move the durable
runner and Phase 2B1 primitives into generic package locations, rename public
symbols and paths, and add schemas and release documentation in normal
follow-up commits. Keep Greenfield compatibility names in a dedicated
compatibility module. Do not alter the Greenfield checkout.

**Tests:** Verify the filtered remote contains only approved history and files.
Then port the complete runner and graphics-broker suites and run import, CLI,
installer, process-tree, lock-contention, atomic-write, task-state, artifact,
framing, peer-credential, and ledger tests from the standalone repository.
Compare fixtures and serialized metadata with the baseline.

**Compatibility:** The old package remains the source of truth for installed
Greenfield execution. The extracted package must be able to read the declared
legacy records before it is allowed to write a new format.

**Human approval:** First approve the filtered history and provenance commit;
then separately approve package restructuring, symbol mapping, release
metadata, and behavioral parity. No import is accepted without confirming that
no secret or unrelated application code entered the factory.

**Rollback:** Delete or archive the standalone remote before adoption, or reset
it to the approved filtered-history baseline. Do not rewrite or delete the
Greenfield repository history or existing `/srv/greenfield` data.

### E3 — Project configuration loader

**Scope:** Implement `.factory/` discovery, schema validation, registration,
configuration digests, compatibility checks, path/command policy, and the
starter project contract. The loader must remain declarative; no arbitrary
Python plugin loading.

**Tests:** Valid/invalid schema fixtures; unknown-field rejection; project and
task ID validation; symlink/path escape tests; fixed-argv and environment
allowlist tests; config snapshot and reload failure tests; two-project
isolation tests.

**Compatibility:** Legacy Greenfield environment variables and task records
remain readable through the compatibility wrapper. New registrations must not
change old data.

**Human approval:** Project owners approve the `.factory/` policy files;
security owners approve command, environment, and path restrictions.

**Rollback:** Disable the loader for runtime use and return to the legacy
wrapper. Configuration files are inert data and can remain for a later retry.

### E4 — Greenfield adapter

**Scope:** Add Greenfield `.factory/` files and registration mapping for
repository, validation, agent, graphics, prompt, and review policy. Move no
source files and add no new graphics capabilities.

**Tests:** Greenfield adapter contract tests; CMake profile command-shape
tests; headless runner/broker tests; graphics profile rejection tests for
Fast2D visual review, native Wayland SwiftShader, arbitrary renderer flags,
and arbitrary display environment; screenshot-size/readiness fixture tests.

**Compatibility:** Existing Greenfield task creation and installed execution
continue unchanged. The adapter is first exercised in validation-only or
dry-run mode.

**Human approval:** Greenfield maintainers approve project policy and visual
review requirements; graphics owners approve profile and session assumptions.

**Rollback:** Unregister Greenfield's new project config and keep using the
legacy Greenfield wrapper and paths.

### E5 — Dual-run parity

**Scope:** Run equivalent clean and controlled failure tasks through the legacy
runner and standalone factory against isolated fixture repositories. Compare
state, worktree, logs, metadata, reports, Git diagnostics, artifacts, and
exit codes. Do not run both against the same task or worktree.

**Tests:** Full Phase 1 proof matrix, Phase 2B1 protocol tests, process-group
termination, lock contention, stale `running` detection, manager-checkout
cleanliness, unexpected commit diagnostics, validation timeout, and artifact
hash comparison. Include SSH disconnect and reboot interruption tests in the
host environment where the baseline was proven.

**Compatibility:** The extracted factory must read and report existing
Greenfield records without taking ownership of them. New runs use a distinct
project/run namespace until parity is approved.

**Human approval:** Greenfield and factory owners sign the parity report;
operations approves the survival/reboot evidence.

**Rollback:** Stop standalone scheduling and leave the legacy service active.
No task data is merged or converted automatically.

### E6 — Installation migration

**Scope:** Install immutable factory releases under
`/srv/software-factory/`, render generic systemd templates, register
Greenfield, and enable the compatibility wrapper. Preserve old Greenfield
installation and releases during the migration window.

**Tests:** Fresh install, reinstall, upgrade, failed upgrade, current-symlink
atomicity, wrapper behavior outside the repository, systemd unit rendering,
permissions, release metadata, `factory doctor`, and rollback to the previous
release. Run a clean smoke task after installation.

**Compatibility:** Old installed releases remain executable against old
records. The new release declares the legacy reader and wrapper compatibility
window. The generic unit must not assume a Greenfield repository path.

**Human approval:** Host administrator approves service accounts, ownership,
unit limits, credentials, and rollback procedure; project owner approves the
Greenfield cutover.

**Rollback:** Atomically point `current` back to the prior release, restore
the legacy unit/wrapper, and leave all run/worktree/artifact directories in
place. Do not run a destructive state migration as part of rollback.

### E7 — Multi-project hardening

**Scope:** Register APG and apgv2 as separate projects, prove isolation, add
project-scoped queue/adapter routing, and exercise capacity and failure
boundaries. Keep external adapters optional and thin.

**Tests:** Concurrent projects with separate locks and roots; credential and
secret isolation; malformed-config isolation; one project's blocked graphics
session not blocking another's headless task; GitHub/Slack/Hermes idempotency
fixtures; artifact permission checks; crash/reboot recovery per project;
cross-project path and record lookup fuzz cases.

**Compatibility:** Greenfield remains the reference consumer and retains its
legacy aliases. Existing tasks remain addressable by `(greenfield, task_id)`.
Project registrations must declare the factory version range they accept.

**Human approval:** Each project owner approves its config and adapter routes;
security/operations approve host isolation and credential tests.

**Rollback:** Disable the new project registration independently. A project
must not require removing the factory release or stopping other projects.

### E8 — Resume graphics/GitHub/Slack/Hermes development externally

**Scope:** Continue Phase 2B2 graphics daemon/session work and later GitHub,
Slack, and Hermes adapters in the standalone repository. Integrate them only
through the approved interfaces and project configuration. Hermes remains a
thin planner; it never owns processes or worktrees.

**Tests:** Protocol compatibility, broker security, graphics lifecycle and
review-package tests, external adapter contract and idempotency tests, and
end-to-end Greenfield visual-review tests with the existing WebGPU policy.

**Compatibility:** Protocol and artifact schema compatibility are declared per
release. Slack/GitHub outages do not invalidate local durable results.

**Human approval:** Factory, project, graphics, and operations owners approve
each adapter's security review and production enablement separately.

**Rollback:** Disable the adapter or profile, keep local factory execution and
review packages active, and revert the adapter release atomically.

## 8. History strategy

### Clean extraction with provenance documentation

Create a new repository from an intentional baseline, copy only the approved
implementation and tests, and add `docs/provenance.md` mapping source paths,
source commits, behavior baselines, and excluded Greenfield-specific files.

Advantages are a small readable history, a clean product identity, no
accidental Greenfield application history, and a clear opportunity to rename
the package and remove project-specific assumptions. The cost is that line-
level `git blame` across the boundary stops at the extraction commit; the
provenance document and release manifest must carry that audit trail.

### `git filter-repo` history extraction

Use a temporary clone to retain selected runner, broker, test, and documentation
paths, then rename and clean them in the standalone repository.

This preserves useful commit-level provenance and can retain the evolution of
durability/security decisions. It also retains unrelated parent history,
Greenfield names, coupled commits, and potentially sensitive content unless
the path and commit selection are audited carefully. It requires a reliable
repeatable extraction script and a post-filter review of every retained tree.

### Subtree or submodule

A subtree keeps the factory inside or synchronized with Greenfield; a
submodule keeps a pointer from Greenfield to an external repository. Both can
be useful for a temporary bridge, but they preserve a source-layout and
release-ownership coupling that this extraction is intended to end. They also
make independent project configuration, installation, and versioning less
obvious.

### Recommendation

Use a one-time `git filter-repo` extraction in a temporary clone, followed by a
manual cleanup and a committed provenance manifest in the standalone
repository. This retains the most valuable durability and security history
without making Greenfield a permanent parent or submodule. If the source
history audit finds that filtering would retain secrets or too much unrelated
application history, use the clean extraction baseline with the same
provenance manifest. Do not use subtree or submodule as the production
architecture.

The extraction operation is not part of this documentation-only change and
must be reviewed before it is run.

## 9. Compatibility and migration

### Temporary Greenfield compatibility wrapper

Keep an executable named `greenfield-agent` for the migration window. It must
be a thin wrapper that:

- invokes the installed `factory` package/release;
- selects `project_id=greenfield`;
- maps `create`, `run`, `status`, `list`, and `doctor` to their generic
  equivalents;
- maps the current `GREENFIELD_*` environment variables to registered
  Greenfield paths only when explicit generic variables are absent;
- preserves current task ID and expected-ref arguments;
- reports the same terminal states and meaningful exit-code behavior; and
- never forks a second implementation of lifecycle or persistence logic.

The wrapper is temporary and must emit a deprecation/compatibility indicator
in `doctor` or release documentation without making existing scripts fail.
The future command is simply:

```text
factory task create <project_id> <task_id> ...
factory task run <project_id> <task_id>
factory task status <project_id> <task_id>
factory task list [<project_id>]
factory doctor [<project_id>]
```

### Existing durable runs and installed releases

Existing `/srv/greenfield/runs/<task_id>` directories, worktrees, logs,
reports, Git snapshots, metadata, and states remain in place. During the
compatibility window the Greenfield adapter reads them as legacy records with
`project_id=greenfield`, without renaming or deleting paths. A legacy run is
not silently retried or converted. Any additive migration record stores the
legacy path and original hash.

An old installed release remains usable with its original wrapper and state
layout. A new factory release may read a declared set of old records, but an
old release must not be expected to read new records unless its compatibility
declaration explicitly says so. `factory doctor` reports the active release,
supported legacy schemas, config digest, and migration status.

### Schema migration and release declarations

Every release publishes a machine-readable declaration such as:

```json
{
  "factory_version": "1.0.0",
  "reads": {
    "project_config": ["factory.project.v1"],
    "task_state": ["greenfield.task.v1", "factory.task.v1"],
    "artifacts": ["greenfield.review.v1", "factory.artifact.v1"],
    "graphics_protocol": ["factory.graphics.v1"]
  },
  "writes": {
    "project_config": "factory.project.v1",
    "task_state": "factory.task.v1",
    "artifacts": "factory.artifact.v1"
  },
  "compatibility_wrapper": "greenfield-agent.v1"
}
```

Schema migrations must support a read-only dry run that reports planned
changes, missing fields, ownership/mode failures, and hashes. The dry run must
not create worktrees, start processes, contact adapters, or rewrite state.
Writes, if approved later, are additive and atomic; a migration writes a new
record beside the old record and verifies it before changing an index pointer.
There is no destructive in-place migration during E0-E6.

### Atomic upgrades and rollback

Install each release into a new immutable directory, verify its manifest and
tests, and update `current` with an atomic symlink replacement. Render service
assets to temporary regular files and replace them atomically. Preserve old
releases until the retention/rollback policy allows removal. A failed install
must not move `current`; a failed runtime upgrade must not mutate task data.

Rollback selects the prior release and compatible service asset. If a release
has written a new schema, rollback is allowed only when the old release's
declaration can read it; otherwise the old release remains available for
legacy data and the new release must provide a forward-compatible recovery
mode. This rule is checked before enabling a release.

## 10. Project bootstrap

The bootstrap commands are factory-owned and operate on a registered project
identity, not on arbitrary paths.

### `factory project init`

`factory project init --project-id <id> --repository <path>` creates a minimal
`.factory/` starter layout in the selected repository, refusing to overwrite
existing files unless an explicit dry-run/force policy is later approved. It
does not register credentials, install services, or enable adapters.

### `factory project register`

`factory project register --project-id <id> --repository <path>` validates the
repository, `.factory/` contract, canonical roots, compatibility range, and
host registration. It writes only the host-owned registration after all checks
pass. A duplicate ID, repository mismatch, unsafe root, or invalid config
leaves the existing registration unchanged.

### `factory project doctor`

`factory project doctor <project-id>` checks configuration/schema validity,
factory compatibility, repository cleanliness and Git availability, root
ownership/modes, worktree/run/artifact writability, executable allowlists,
credential bindings without printing secrets, enabled adapter configuration,
graphics-session prerequisites when requested, and legacy migration status.
It reports project-scoped failures without probing unrelated projects.

### Minimal generated starter

```text
.factory/
  project.json
  repository.json
  compatibility.json
  adapters.json
  review.json
  prompts/
    default.md
  agents/
    codex-default.json
  validation/
    headless.json
  graphics/
    disabled.json
```

The generated `graphics/disabled.json` is intentionally safe: it cannot
launch a graphics process until a project owner supplies an approved profile
and host registration. Generated validation and agent files contain examples
that fail closed until executable IDs are registered.

## 11. Parity acceptance criteria

The extracted factory is accepted only when it repeats the established Phase 1
proofs and preserves Phase 2B1 protocol behavior. Tests must use temporary
roots and fixture repositories where possible; host survival tests use a
dedicated test installation and do not run graphics unless a later graphics
stage explicitly requires it.

### Phase 1 durability proofs

- `create`, `run`, `status`, `list`, and `doctor` work through `factory` and
  through the temporary `greenfield-agent` wrapper.
- Installation from an immutable release succeeds from a clean checkout,
  records version/source metadata, and runs from outside the source tree.
- A failed install or upgrade leaves the prior `current` release active.
- Worktree isolation creates the expected branch and worktree beneath the
  project root, rejects an existing worktree/branch, and retains Git before
  and after diagnostics.
- Locking prevents duplicate execution, records contention without changing a
  task from `ready`, and isolates project locks.
- An SSH disconnect does not interrupt a systemd-owned task; durable state,
  logs, report, metadata, and artifacts remain inspectable.
- A reboot interruption leaves a diagnosable non-running/stale record and does
  not cause an unsafe silent retry; a post-reboot doctor and status pass.
- Self-hosting validation runs the factory's own package, unit, security, and
  documentation tests through a registered validation profile.
- Test isolation proves factory tests do not use or modify a real project
  checkout, real credentials, or another project's durable root.
- A clean smoke task reaches the expected successful review state.
- Logs, metadata, reports, Git diagnostics, artifact manifests, and hashes are
  durable and retained on success and failure.
- Process timeouts and interruption terminate the complete process group and
  preserve failure evidence.
- Manager-checkout cleanliness, unexpected commits, untracked files, stale
  `running` records, validation failure, and Codex/agent failure remain
  diagnosable outcomes.

### Phase 2B1 protocol proofs

The extracted `graphics_broker` package must preserve the current tests and
security invariants:

- strict version-one visual-review request schema and bounded identifiers;
- canonical UUID and deadline validation;
- bounded UTF-8 length-prefixed framing;
- kernel peer-credential extraction and UID authorization primitives;
- structured errors and rejection of extra/request-controlled executable,
  argument, environment, display, or library fields; and
- acceptance-only idempotency ledger with SHA-256 names, exclusive no-follow
  creation, atomic flush behavior, and file locking.

Phase 2B1 parity does not mean adding a daemon, socket listener, graphics
session, screenshot capture, Slack, or Hermes in the extraction slice. Those
remain later adapter/broker stages.

### Multi-project acceptance

Before APG or apgv2 is enabled, the factory must also prove:

- identical task IDs in different projects produce disjoint records, locks,
  worktrees, logs, artifacts, external mappings, and idempotency scopes;
- one project cannot read or write another project's repository, config,
  secrets, credentials, or artifact package;
- one project's malformed configuration, blocked task, graphics outage, or
  adapter outage does not block another project's local execution;
- each project can independently register, doctor, disable, roll back, and
  migrate; and
- external notifications can be lost or duplicated without changing local
  durable state.

## 12. Open decisions

### Recommended defaults

- Extract immediately after Phase 2B1; do not wait for graphics-session,
  GitHub, Slack, or Hermes coupling.
- Use `software-factory` / `software_factory` / `factory` as the generic
  names, with `greenfield-agent` only as a temporary wrapper.
- Keep the factory core Python and dependency-light; package with a standard
  `pyproject.toml` and immutable releases.
- Use declarative `.factory/` configuration and fixed argument arrays; do not
  load arbitrary project Python plugins.
- Make `project_id` explicit, validated, immutable, and present in every
  durable and external mapping record.
- Use project-scoped durable roots and locks, with only short-lived host
  coordination and explicit capacity limits.
- Keep screenshots and review packages factory-owned, immutable, hashed, and
  durable; project policy specifies what evidence is required.
- Treat Slack as a transport and Hermes as a thin plan/intention adapter; the
  local factory record remains authoritative.
- Keep GitHub, Slack, Hermes, and live graphics optional to the core package.
- Use a one-time filtered history extraction with a provenance manifest, with a
  clean extraction fallback if the history audit is not safe.

### Deferred decisions

- The exact packaging backend and whether JSON Schema validation is bundled or
  implemented with a small standard-library validator.
- Whether project policy is loaded only from the repository or may be pinned
  by an administrator under `/etc/software-factory/projects/<id>/`.
- The exact queue adapter and whether the first queue remains explicit CLI plus
  systemd rather than a daemon.
- The graphics broker daemon/socket implementation and graphics-account
  session lifecycle after Phase 2B1.
- The GitHub App versus scoped token implementation.
- Slack app/webhook transport and message threading details.
- Hermes wire protocol, plan format, and event subscription mechanism.
- Artifact retention duration, backup medium, and large-object storage.
- Whether a future project adapter needs a reviewed plugin ABI; arbitrary
  Python plugins are not allowed in the initial contract.

### Requires project-owner approval

- Each project's `project_id`, repository registration, service account, and
  credential scope.
- Each project's commands, timeout ceilings, network policy, agent profiles,
  validation success criteria, and review gates.
- Greenfield's exact screenshot sizes/readiness marker and any future approved
  dedicated X11 SwiftShader probe.
- Which Greenfield tasks may use visible graphics review and the human
  reviewers who can approve them.
- APG and apgv2 artifact/review requirements and external routing.
- The migration cutover date, legacy wrapper deprecation window, and retention
  period for old installed releases.
- Host-wide concurrency limits, graphics-account capacity, and backup/restore
  objectives.

## 13. Decision summary

Approve E0 now and begin E1 only after the contract and security invariants
receive owner approval. The first implementation should extract the existing
durable runner and Phase 2B1 protocol primitives, make their names and paths
generic, and prove parity in a standalone repository. Greenfield should then
become a declarative reference consumer through `.factory/`, while all future
queue, graphics-session, GitHub, Slack, and Hermes work proceeds against the
factory interfaces rather than increasing coupling inside the Greenfield
repository.
