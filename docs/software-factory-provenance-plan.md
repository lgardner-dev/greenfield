# Software Factory E1 Provenance Plan

Status: E1 preparation only. This plan does not create a GitHub repository,
push history, install software, alter services, or change production
execution.

## Source and extraction record

The E1 record must identify the exact source checkout used for the extraction,
including its canonical absolute repository path, Git common directory, source
`HEAD`, active branch or detached-HEAD state, and the capture time in UTC. The
source commit is the immutable input to every dry run. The operator must also
record the source worktree status, complete refs, and configured remotes
before and after each run.

The retained-path authority is the four-line file
[`tools/extraction/retained-paths.txt`](../tools/extraction/retained-paths.txt):

- `tools/runner/` contains the reusable durable runner implementation, tests,
  launcher, and installation templates that are the historical factory core.
- `tools/graphics_broker/` contains the renderer-independent Phase 2B1 broker
  protocol primitives, ledger, credentials policy, and tests.
- `docs/phase2-restricted-graphics-launcher.md` records the security and
  review boundary for the broker-adjacent graphics contract.
- `docs/software-factory-extraction.md` is the architecture authority and
  preserves the extraction, ownership, migration, and fallback decisions.

No other path is approved. A genuinely necessary addition requires a reviewed
allowlist change, a rationale in this plan, and a new human approval before
running E2. E1 tooling and this plan are preparation artifacts in Greenfield;
they are intentionally not retained by the extraction.

The repeatable procedure is:

1. Refuse an existing output path and validate the explicit source repository.
2. Clone the source locally with `git clone --no-local --no-hardlinks` into a
   disposable staging directory. This avoids hard links and alternate object
   stores; the procedure performs no network operation.
3. Require the source checkout to be on an explicit reviewed branch and
   clone only that branch with `--single-branch --branch`. Temporary task,
   feature, and agent branches are not imported.
4. Run `git filter-repo` with `--paths-from-file` pointing to the exact
   allowlist, plus deterministic empty/degenerate commit pruning.
5. Remove every configured remote and every remote-tracking ref, verify there
   is no alternate object store, and run `git fsck --full`.
6. Move the independent filtered repository into the new output path and
   report the source commit, filter version, filtered `HEAD`, refs, count,
   retained inventory, and remotes.

The installed `git-filter-repo` version must be recorded from
`git-filter-repo --version`. The command line, allowlist checksum, source
commit, and script checksum belong in the E2 operator record. A failed staging
run is disposable and must not leave a partial requested output directory.

## History and provenance fields

The E2 provenance manifest must contain at least:

```text
manifest_schema
source_repository
source_git_common_dir
source_commit
source_branch_or_detached_head
source_status
source_refs_digest
source_remotes_digest
extraction_timestamp_utc
extraction_script_sha256
allowlist_path
allowlist_sha256
git_filter_repo_version
git_filter_repo_command
filtered_head
filtered_refs
filtered_commit_count
filtered_roots
original_to_filtered_commit_map_path
original_to_filtered_commit_map_sha256
retained_file_inventory
retained_file_inventory_sha256
authors
author_date_min
author_date_max
tags
branches
audit_tool_sha256
audit_result
secret_scan_result
unrelated_content_result
large_blob_result
binary_file_result
remote_result
clean_extraction_fallback_status
reviewer
approval_timestamp_utc
```

`git-filter-repo` writes an original-to-filtered commit map in the filtered
repository metadata. Preserve that map as evidence and record its path and
hash in the E2/E2 bootstrap manifest before any later package restructuring.
The map must be reviewed for deleted, rewritten, and unchanged commits; a
filtered commit with no retained tree content may disappear under the
approved prune policy.

Authors, author dates, commit messages, and retained commit parentage are
preserved by `git-filter-repo` unless the documented filter operation removes
an empty or degenerate commit. Committer dates and identities are also
retained by the tool and should be checked in the manifest review.

The branch policy is to preserve only the explicitly reviewed checked-out
source branch, normally `main`. Temporary task, feature, and agent branches are
not product history and must not enter the filtered repository. Source tags are
preserved when their target survives; every surviving tag requires review.
Remote-tracking refs are removed. No new branch, tag, or commit may be created
during E1.

## Audit and review process

Run `tools/extraction/audit-filtered-history.sh` against every independent
filtered output. The audit emits stable line-oriented records for refs, roots,
commit count, authors, date range, tags, branches, and the complete `HEAD`
file inventory. It walks every reachable filtered commit and rejects any file
outside the allowlist or any missing allowlisted path at `HEAD`.

The audit checks, without printing content values, for private-key markers,
likely provider tokens, `.env` files, credential-like filenames, unusually
large blobs, and unexpected binary files. High-confidence secret findings,
unsafe remotes, and path violations are blocking failures. Large and binary
findings are review findings even when they are not automatically blocking.
The reviewer must inspect every finding and the complete filtered tree before
approving E2.

The destination requirement for E2 is a private, completely empty GitHub
repository named `software-factory`, created only after owner approval. It
must have no README, license, `.gitignore`, initial commit, tags, branch
protection side effects, or other generated content. E1 does not create or
contact that destination.

Before the first push, the reviewer must confirm:

- source `HEAD`, status, refs, and remotes are unchanged;
- both independent dry runs have identical filtered `HEAD`, refs, commit
  counts, trees, and inventories;
- the destination is private and completely empty;
- the allowlist and filter-repo version match the approved record;
- the commit map, authors, dates, branches, tags, roots, and retained files
  have been reviewed;
- the audit has no high-confidence secrets, path violations, unsafe remotes,
  or unexplained binary/large-blob findings;
- no E1 tooling, Greenfield application code, build files, sandbox files,
  services, state, artifacts, or installed releases entered the history; and
- the first push is performed once, from the reviewed disposable output, with
  no force-push or subsequent history rewrite planned.

## Rollback and unsafe-history fallback

During E1, rollback is simply to delete the disposable output directories and
retain the unchanged Greenfield checkout. No source history or production data
is rewritten. If a future empty remote exists and approval is withdrawn before
the first push, delete or abandon that empty remote; do not alter Greenfield.

If the filtered-history audit finds a secret, excessive unrelated history,
unreviewable binary content, broken authorship/date provenance, or an unsafe
ref result, do not push the filtered history. Preserve the audit report as
review evidence, discard the disposable output, and use a clean-extraction
baseline: create a new empty repository, copy only the approved current files,
add this provenance manifest and the original source-commit mapping as
documentation, and make one reviewed initial commit. The clean baseline is a
separate approved migration choice, not an automatic E1 behavior.
