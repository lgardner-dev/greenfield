# Greenfield Task Runner

`greenfield-agent` is the initial in-repository Python task runner for unattended,
single-task Codex work. It creates durable run records, prepares an isolated Git
worktree and task branch, invokes Codex with a constrained configuration, runs an
optional validation script, and leaves the result for human review.

The runner uses Python 3.10 or newer and only the Python standard library. The
repository also contains a repeatable release installer and a systemd user
service template for durable task execution.

## Threat Model

The runner treats task IDs, copied task files, task state, and filesystem paths as
untrusted inputs. Task IDs have a narrow allowlist, subprocesses always receive
argument vectors, final-component symlinks are rejected for run-controlled input
files where practical, and state and JSON metadata use atomic replacement. The
global lock is exclusive and nonblocking, so two task processes cannot execute at
the same time.

Codex runs with `workspace-write`, approval policy `never`, and sandbox network
access disabled. Creating a regular file named `allow-network` in a task's run
directory explicitly enables sandbox network access for that task. The runner
does not protect against a user who can replace arbitrary parent directories or
modify the runner's own Python source. Filesystem permissions and service-account
isolation remain deployment responsibilities.

The manager checkout must be clean before every run. The runner never commits,
stages, resets, cleans, pushes, merges, removes a worktree, or deletes a task
branch. Codex or a validation script can still modify the task worktree, so a
human must inspect every review result.

## Layout

The source tree is:

```text
tools/runner/
  greenfield_agent/       Python package
  tests/                  standard-library unittest suite
  greenfield-agent        location-independent executable wrapper
  install.sh              release and systemd user-service installer
  systemd/                 service template
  README.md
```

The default durable data layout is:

```text
/srv/greenfield/
  runs/TASK_ID/
    metadata.json
    state
    prompt.md
    expected-ref
    timeout-seconds
    reasoning-effort
    validation.sh         optional
    allow-network         optional, created explicitly after task creation
    final-report.md
    logs/
    git/before/
    git/after/
  worktrees/TASK_ID/
  state/agent.lock
```

The installed immutable runtime is separate from that mutable data:

```text
/srv/greenfield/bin/greenfield-agent
/srv/greenfield/lib/greenfield-runner/
  releases/<version>/greenfield_agent/
  releases/<version>/VERSION
  releases/<version>/INSTALL-METADATA
  current -> releases/<version>
```

Git snapshots contain status, staged and unstaged diffs, the diff and diff stat
against the resolved expected commit, HEAD, commits added, and JSON and text
inventories of untracked files. The copied prompt is `prompt.md`; durable state
is `state` and metadata is `metadata.json`. Logs contain Codex JSONL events,
Codex stderr, exit codes, timing, optional validation stdout/stderr, and durable
runner errors. The final report is `final-report.md`; patch and worktree
evidence is retained in the Git diagnostics and the task worktree rather than
being deleted after execution.

## Installation

Run the installer from a clean checkout:

```bash
sudo tools/runner/install.sh
```

It installs the wrapper at `/srv/greenfield/bin/greenfield-agent`, immutable
releases under `/srv/greenfield/lib/greenfield-runner/releases/`, and the user
unit at `/etc/systemd/user/greenfield-task@.service`. The `current` symlink is
updated atomically. Older releases and all mutable task data remain in place.
For unprivileged temporary tests, use `--prefix`, `--systemd-user-dir`,
`--skip-owner-change`, and, only when needed, `--allow-dirty` or `--no-systemd`.

As `greenfield-agent`, reload and inspect the unit:

```bash
systemctl --user daemon-reload
greenfield-agent doctor
systemctl --user cat greenfield-task@.service
```

## Upgrade

Update the checkout and rerun the installer. Verify the active release; no
mutable task data is deleted:

```bash
git pull --ff-only
sudo tools/runner/install.sh
readlink -f /srv/greenfield/lib/greenfield-runner/current
```

## Service execution

The unit is not enabled, does not discover tasks, and does not retry failures.
One explicit user-manager start runs one task:

```bash
systemctl --user start greenfield-task@TASK_ID.service
systemctl --user status greenfield-task@TASK_ID.service
journalctl --user -u greenfield-task@TASK_ID.service -f
```

`KillMode=mixed` lets systemd stop reach the runner's existing SIGTERM handler;
the runner then terminates its Codex or validation process group and records
`blocked-runner`. A failed task returns nonzero to systemd. Durable state and
task logs remain authoritative over journal output.

## Commands

Run the wrapper directly from any current working directory:

```bash
/path/to/greenfield/tools/runner/greenfield-agent create TASK_ID EXPECTED_REF PROMPT_FILE [VALIDATION_FILE]
/path/to/greenfield/tools/runner/greenfield-agent run TASK_ID
/path/to/greenfield/tools/runner/greenfield-agent status TASK_ID
/path/to/greenfield/tools/runner/greenfield-agent list
/path/to/greenfield/tools/runner/greenfield-agent doctor
```

Task IDs must match `^[A-Za-z0-9][A-Za-z0-9._-]*$` and must also be valid as the
final component of the runner's `agent/TASK_ID` Git branch. In particular, they
must not contain `..`, end with `.`, or end with `.lock`. `create` copies its inputs;
subsequent edits to the original prompt or validation file do not affect the task.
The initial Codex timeout is 14,400 seconds. The initial reasoning effort is
`high`. The optional validation script runs through `bash` from the task worktree
after Codex exits successfully. Its default independent timeout is 3,600 seconds.
An operator may place a positive number in `validation-timeout-seconds` inside the
run directory to override that task's validation timeout.

`run` returns success only for a task that reaches `review`. A blocked outcome is
durable and returns nonzero. A task is never rerun unless its current durable
state is `ready`; this initial version has no reset or retry command.

## State Model

The exact states are:

- `ready`: created and eligible to run
- `running`: currently owns the run lifecycle
- `review`: Codex and optional validation succeeded
- `blocked-codex`: Codex failed or timed out
- `blocked-validation`: validation failed or timed out
- `blocked-runner`: a Git, filesystem, process-launch, or other infrastructure step failed

The only valid transitions are `ready` to `running`, then `running` to one of the
four terminal states. Once `running` is durable, runner-side exceptions and
handled SIGINT/SIGTERM interruptions transition the task to `blocked-runner`,
record a concise error, the runner exit code, and finish/update timestamps. The
recorded runner PID makes a stale `running` task visible in `status` and as
`running-stale` in `list` when that PID is no longer alive.

SIGKILL, host power loss, and similar abrupt process death cannot run in-process
finalization. Such a task can remain `running`; `status` reports it as stale once
the recorded PID is no longer alive. PID reuse can temporarily make this basic
liveness check appear alive. This initial runner deliberately provides no reset,
retry, branch deletion, or worktree cleanup command.

## Configuration

Environment variables provide deployment configuration:

| Variable | Default |
| --- | --- |
| `GREENFIELD_ROOT` | `/srv/greenfield` |
| `GREENFIELD_REPOSITORY` | `/srv/greenfield/repos/greenfield` |
| `GREENFIELD_RUNS` | `$GREENFIELD_ROOT/runs` |
| `GREENFIELD_WORKTREES` | `$GREENFIELD_ROOT/worktrees` |
| `GREENFIELD_STATE` | `$GREENFIELD_ROOT/state` |
| `GREENFIELD_LOCK` | `$GREENFIELD_STATE/agent.lock` |
| `GREENFIELD_GIT` | `git` |
| `GREENFIELD_CODEX` | `codex` |
| `GREENFIELD_BASH` | `bash` |
| `GREENFIELD_VALIDATION_TIMEOUT_SECONDS` | `3600` |
| `GREENFIELD_TERMINATE_GRACE_SECONDS` | `5` |

Tests construct `RunnerConfig` directly and use temporary directories and fake
executables. They never invoke the real Codex CLI or modify the real checkout.

## Recovery

Task state and logs survive an SSH disconnect because the user service manager
owns the process. Running processes cannot survive a machine reboot. After a
reboot, inspect stale or interrupted work with `status`, `list`, and `doctor`.
Tasks are never silently resumed or retried; a human decides whether to create
a new recovery task.

## Phase 1 durability validation templates

After review and real installation, replace `EXPECTED_REF` with the reviewed
branch or ref. Task A is a non-graphical repository audit suitable for an SSH
disconnect test. Task B repeats the same validation after reboot:

```bash
cat >/tmp/phase1-durable-a.prompt.md <<'EOF'
Audit the Greenfield Python runner installation and durable systemd execution
contract in this checkout. Make only narrow, necessary corrections. Do not
commit, push, merge, use GitHub, run graphical applications, or modify the
engine. Run the repository's non-graphical runner and CMake headless tests as
appropriate, and leave a clear final report describing the audit and results.
EOF
cat >/tmp/phase1-durable-a.validation.sh <<'EOF'
#!/bin/bash
set -euo pipefail
tools/runner/run-tests
cmake --preset headless-fast2d
cmake --build --preset headless-fast2d
ctest --preset headless-fast2d --output-on-failure
git diff --check
EOF
chmod 700 /tmp/phase1-durable-a.validation.sh
greenfield-agent create phase1-durable-a EXPECTED_REF \
  /tmp/phase1-durable-a.prompt.md /tmp/phase1-durable-a.validation.sh
systemctl --user start greenfield-task@phase1-durable-a.service
systemctl --user status greenfield-task@phase1-durable-a.service
```

Disconnect SSH while Task A is running, reconnect, and inspect its durable
state and artifacts:

```bash
greenfield-agent status phase1-durable-a
greenfield-agent list
greenfield-agent doctor
find /srv/greenfield/runs/phase1-durable-a -maxdepth 3 -type f -print | sort
find /srv/greenfield/worktrees/phase1-durable-a -maxdepth 1 -print
```

After Task A completes, reboot and verify the installed wrapper, completed task,
unit, and lingering user manager. The completed unit itself need not remain
active after reboot:

```bash
sudo reboot
# reconnect as greenfield-agent
greenfield-agent --help
greenfield-agent doctor
greenfield-agent list
greenfield-agent status phase1-durable-a
test -f /srv/greenfield/runs/phase1-durable-a/final-report.md
test -d /srv/greenfield/worktrees/phase1-durable-a
systemctl --user cat greenfield-task@.service
loginctl show-user greenfield-agent -p Linger
```

Create and start Task B after reboot:

```bash
cat >/tmp/phase1-durable-b.prompt.md <<'EOF'
Perform a post-reboot non-graphical Greenfield runner health audit. Verify the
installed runner, durable task layout, and repository test entry points. Do not
commit, push, merge, use GitHub, or run graphical applications. Leave a concise
final report and run the supplied validation script.
EOF
cat >/tmp/phase1-durable-b.validation.sh <<'EOF'
#!/bin/bash
set -euo pipefail
tools/runner/run-tests
cmake --preset headless-fast2d
cmake --build --preset headless-fast2d
ctest --preset headless-fast2d --output-on-failure
git diff --check
EOF
chmod 700 /tmp/phase1-durable-b.validation.sh
greenfield-agent create phase1-durable-b EXPECTED_REF \
  /tmp/phase1-durable-b.prompt.md /tmp/phase1-durable-b.validation.sh
systemctl --user start greenfield-task@phase1-durable-b.service
systemctl --user status greenfield-task@phase1-durable-b.service
greenfield-agent status phase1-durable-b
find /srv/greenfield/runs/phase1-durable-b -maxdepth 3 -type f -print | sort
```

## Codex Invocation

The command shape keeps the named profile in Codex's global option position:

```text
codex --profile greenfield-agent exec \
  --sandbox workspace-write \
  --config approval_policy="never" \
  --config sandbox_workspace_write.network_access=false \
  --config model_reasoning_effort="high" \
  --json --output-last-message RUN_DIR/final-report.md -
```

The copied prompt is sent to standard input. JSONL events and stderr are separate
files. A timeout first sends `SIGTERM` to the Codex or validation process group,
waits for the configured grace period, and then uses `SIGKILL` if needed.
SIGINT or SIGTERM sent to the runner uses the same child-process-group shutdown,
preserves available Git diagnostics, and records `blocked-runner`. If a terminal
`review`, `blocked-codex`, or `blocked-validation` state is already durable, late
interruption/finalization handling does not overwrite it. Worktrees and branches
are always left in place.

## Tests

From the repository root, run the focused suite without configuring the C++
build or setting `PYTHONPATH` manually:

```bash
tools/runner/run-tests
```

The location-independent script supplies the in-tree package path and invokes
standard-library unittest discovery. The same suite is registered as
`greenfield_python_runner_tests` in CTest and is included in the existing
repository test flow.

The initial runner intentionally has no GitHub or pull-request polling, PR
creation, Hermes integration, graphical session access, automatic pushes,
automatic cleanup, retry/reset commands, parallel task execution, worktree
deletion, branch deletion, repository installation, or service installation. It
does not access the graphical `greenfield-gfx` account and does not require root.
