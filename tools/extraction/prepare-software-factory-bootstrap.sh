#!/usr/bin/env bash
set -euo pipefail

script_directory=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd -P)
extract_script="${script_directory}/extract-software-factory.sh"
audit_script="${script_directory}/audit-filtered-history.sh"
retained_allowlist="${script_directory}/retained-paths.txt"
generated_allowlist="${script_directory}/bootstrap-generated-paths.txt"
provenance_directory='docs/provenance'
provenance_commit_message='Record software factory extraction provenance'
provenance_identity_name='Greenfield Extraction'
provenance_identity_email='greenfield-extraction@greenfield.invalid'

usage() {
    printf 'Usage: %s SOURCE_REPOSITORY OUTPUT_DIRECTORY SOURCE_REPOSITORY_ID EXTRACTION_TIMESTAMP_UTC\n' "$(basename -- "$0")" >&2
}

fail() {
    printf 'prepare-software-factory-bootstrap: %s\n' "$1" >&2
    exit 1
}

if (( $# != 4 )); then
    usage
    exit 2
fi

source_repository=$1
requested_output_directory=$2
source_repository_id=$3
extraction_timestamp_utc=$4

for argument in "$source_repository" "$requested_output_directory" "$source_repository_id" "$extraction_timestamp_utc"; do
    [[ -n "$argument" && "$argument" != -* ]] || {
        usage
        exit 2
    }
done

if [[ ! "$source_repository_id" =~ ^[A-Za-z0-9][A-Za-z0-9._-]{0,63}/[A-Za-z0-9][A-Za-z0-9._-]{0,95}$ ]]; then
    fail 'source repository ID must be a bounded owner/name identifier'
fi

if [[ ! "$extraction_timestamp_utc" =~ ^[0-9]{4}-[0-9]{2}-[0-9]{2}T[0-9]{2}:[0-9]{2}:[0-9]{2}Z$ ]]; then
    fail 'extraction timestamp must be canonical UTC in YYYY-MM-DDTHH:MM:SSZ form'
fi
normalized_timestamp=$(date -u -d "$extraction_timestamp_utc" '+%Y-%m-%dT%H:%M:%SZ' 2>/dev/null) \
    || fail 'extraction timestamp is not a valid UTC instant'
[[ "$normalized_timestamp" == "$extraction_timestamp_utc" ]] \
    || fail 'extraction timestamp is not canonical UTC'

[[ -f "$extract_script" ]] || fail "extraction script is missing: ${extract_script}"
[[ -f "$audit_script" ]] || fail "audit script is missing: ${audit_script}"
[[ -f "$retained_allowlist" ]] || fail "retained allowlist is missing: ${retained_allowlist}"
[[ -f "$generated_allowlist" ]] || fail "generated-path allowlist is missing: ${generated_allowlist}"

source_top_level=$(git -C "$source_repository" rev-parse --show-toplevel 2>/dev/null) \
    || fail "source is not a non-bare Git repository: ${source_repository}"
source_top_level=$(CDPATH= cd -- "$source_top_level" && pwd -P)
source_commit=$(git -C "$source_top_level" rev-parse --verify HEAD 2>/dev/null) \
    || fail 'source repository has no HEAD commit'
source_branch=$(git -C "$source_top_level" symbolic-ref --quiet --short HEAD 2>/dev/null) \
    || fail 'source repository must be checked out on an explicit branch'
git check-ref-format --branch "$source_branch" >/dev/null 2>&1 \
    || fail "source repository branch is invalid: ${source_branch}"

output_parent=$(dirname -- "$requested_output_directory")
output_name=$(basename -- "$requested_output_directory")
[[ -d "$output_parent" ]] || fail "output parent directory does not exist: ${output_parent}"
output_parent=$(CDPATH= cd -- "$output_parent" && pwd -P)
output_directory="${output_parent}/${output_name}"
[[ ! -e "$output_directory" ]] || fail 'output directory already exists; choose a new disposable path'

case "${output_directory}/" in
    "${source_top_level}/"*)
        fail 'output directory must not be inside the supplied source repository'
        ;;
esac

snapshot_source_state() {
    local destination=$1
    {
        printf 'STATUS\n'
        git -C "$source_top_level" status --porcelain=v1
        printf 'REFS\n'
        git -C "$source_top_level" for-each-ref --format='%(refname) %(objectname)'
        printf 'REMOTES\n'
        git -C "$source_top_level" remote -v
    } > "$destination"
}

sha256_file() {
    sha256sum "$1" | awk '{print $1}'
}

audit_value() {
    local audit_file=$1
    local key=$2
    sed -n "s/^${key}=//p" "$audit_file" | sed -n '1p'
}

write_manifest() {
    local manifest_file=$1
    local audit_hash=$2
    local audit_file=$3
    python3 - "$manifest_file" "$audit_file" "$source_repository_id" "$source_commit" "$source_branch" "$extraction_timestamp_utc" \
        "$filter_repo_version" "$extract_script_hash" "$audit_script_hash" "$retained_allowlist_hash" \
        "$generated_allowlist_hash" "$pre_provenance_head" "$filtered_commit_count" "$commit_map_hash" \
        "$retained_inventory_hash" "$audit_hash" "$architecture_source_commit" <<'PY'
import json
import sys
from pathlib import Path

(
    manifest_path,
    audit_path,
    source_repository_id,
    source_commit,
    source_branch,
    extraction_timestamp,
    filter_repo_version,
    extraction_script_hash,
    audit_script_hash,
    retained_allowlist_hash,
    generated_allowlist_hash,
    pre_provenance_head,
    filtered_commit_count,
    commit_map_hash,
    retained_inventory_hash,
    audit_hash,
    architecture_source_commit,
) = sys.argv[1:]

records = {}
for line in Path(audit_path).read_text(encoding="utf-8").splitlines():
    if "=" not in line:
        continue
    key, value = line.split("=", 1)
    records.setdefault(key, []).append(value)

def values(key):
    return records.get(key, [])

manifest = {
    "manifest_schema": "software-factory-bootstrap-manifest-v1",
    "source_repository_id": source_repository_id,
    "source_commit": source_commit,
    "source_branch": source_branch,
    "extraction_timestamp_utc": extraction_timestamp,
    "git_filter_repo_version": filter_repo_version,
    "extraction_script_sha256": extraction_script_hash,
    "audit_script_sha256": audit_script_hash,
    "retained_allowlist_sha256": retained_allowlist_hash,
    "generated_path_allowlist_sha256": generated_allowlist_hash,
    "pre_provenance_filtered_head": pre_provenance_head,
    # A literal final commit ID here would make the manifest hash itself a
    # cryptographic fixed-point problem. The summary reports the actual ID;
    # this committed field records the stable ref it describes.
    "final_bootstrap_head": "HEAD",
    "filtered_commit_count": int(filtered_commit_count),
    "original_to_filtered_commit_map_path": "docs/provenance/original-to-filtered-commit-map.txt",
    "original_to_filtered_commit_map_sha256": commit_map_hash,
    "retained_file_inventory_path": "docs/provenance/retained-files.txt",
    "retained_file_inventory_sha256": retained_inventory_hash,
    "audit_result_path": "docs/provenance/filtered-history-audit.txt",
    "audit_result": "PASS",
    "audit_result_sha256": audit_hash,
    "branches": sorted(values("BRANCH")),
    "tags": sorted(values("TAG")),
    "roots": sorted(values("ROOT")),
    "authors": sorted(values("AUTHOR")),
    "date_range": {
        "min": values("DATE_MIN")[0] if values("DATE_MIN") else "",
        "max": values("DATE_MAX")[0] if values("DATE_MAX") else "",
    },
    "generated_provenance_paths": [
        "docs/provenance/README.md",
        "docs/provenance/manifest.json",
        "docs/provenance/original-to-filtered-commit-map.txt",
        "docs/provenance/retained-files.txt",
        "docs/provenance/filtered-history-audit.txt",
    ],
    "provenance_commit_message": "Record software factory extraction provenance",
    "provenance_commit_author": "Greenfield Extraction <greenfield-extraction@greenfield.invalid>",
    "provenance_commit_timestamp_utc": extraction_timestamp,
    "destination_remote_contacted": False,
    "destination_remote_statement": "The destination remote was not contacted.",
    "no_remote_configured": True,
    "no_remote_statement": "No remote is configured in the candidate.",
    "greenfield_extraction_architecture_source_commit": architecture_source_commit,
}

Path(manifest_path).write_text(
    json.dumps(manifest, indent=2, sort_keys=True) + "\n",
    encoding="utf-8",
)
PY
}

export GIT_CONFIG_NOSYSTEM=1
export GIT_TERMINAL_PROMPT=0

source_state_before=$(mktemp "${output_parent}/.software-factory-source-before.XXXXXX")
source_state_after=$(mktemp "${output_parent}/.software-factory-source-after.XXXXXX")
working_parent=$(mktemp -d "${output_parent}/.software-factory-bootstrap.XXXXXX")
cleanup() {
    rm -f -- "$source_state_before" "$source_state_after"
    rm -rf -- "$working_parent"
}
trap cleanup EXIT

snapshot_source_state "$source_state_before"

filter_repo_version=$(git-filter-repo --version 2>/dev/null) \
    || fail 'git-filter-repo is required'
extract_script_hash=$(sha256_file "$extract_script")
audit_script_hash=$(sha256_file "$audit_script")
retained_allowlist_hash=$(sha256_file "$retained_allowlist")
generated_allowlist_hash=$(sha256_file "$generated_allowlist")
architecture_source_commit=$(git -C "$source_top_level" log -1 --format='%H' "$source_commit" -- docs/software-factory-extraction.md)
[[ -n "$architecture_source_commit" ]] || fail 'source does not contain the extraction architecture document'

pre_extract_summary="${working_parent}/extract-summary.txt"
"$extract_script" "$source_top_level" "$output_directory" > "$pre_extract_summary"

pre_audit_file="${working_parent}/pre-provenance-audit.txt"
"$audit_script" "$output_directory" > "$pre_audit_file"
pre_provenance_head=$(audit_value "$pre_audit_file" FILTERED_HEAD)
filtered_commit_count=$(audit_value "$pre_audit_file" COMMIT_COUNT)
[[ "$pre_provenance_head" =~ ^[0-9a-f]{40}$ ]] || fail 'pre-provenance audit did not report a commit ID'
[[ "$filtered_commit_count" =~ ^[0-9]+$ ]] || fail 'pre-provenance audit did not report a commit count'

filter_repo_map=$(git -C "$output_directory" rev-parse --git-path filter-repo/commit-map)
[[ "$filter_repo_map" == /* ]] || filter_repo_map="${output_directory}/${filter_repo_map}"
[[ -f "$filter_repo_map" ]] || fail 'git-filter-repo original-to-filtered commit map is missing'
[[ -s "$filter_repo_map" ]] || fail 'git-filter-repo original-to-filtered commit map is empty'

mkdir -p "${output_directory}/${provenance_directory}"
cp -- "$filter_repo_map" "${output_directory}/${provenance_directory}/original-to-filtered-commit-map.txt"
git -C "$output_directory" ls-tree -r --name-only HEAD | LC_ALL=C sort > \
    "${output_directory}/${provenance_directory}/retained-files.txt"
cat > "${output_directory}/${provenance_directory}/README.md" <<'EOF'
# Software Factory Bootstrap Provenance

This directory contains deterministic evidence for the reviewed local
software-factory bootstrap candidate. The evidence was generated after the
approved retained-path extraction and is limited to the exact paths declared
by `tools/extraction/bootstrap-generated-paths.txt` in the Greenfield source.

The candidate has one provenance commit, no configured remotes, and has not
contacted the future destination repository. The original-to-filtered commit
map is preserved byte-for-byte from `git-filter-repo` metadata.
EOF

commit_map_hash=$(sha256_file "${output_directory}/${provenance_directory}/original-to-filtered-commit-map.txt")
retained_inventory_hash=$(sha256_file "${output_directory}/${provenance_directory}/retained-files.txt")

preflight_repository="${working_parent}/preflight-repository"
git clone --no-local --no-hardlinks "$output_directory" "$preflight_repository" >/dev/null 2>&1 \
    || fail 'could not make a local preflight copy'
while IFS= read -r remote_name; do
    [[ -n "$remote_name" ]] || continue
    git -C "$preflight_repository" remote remove "$remote_name"
done < <(git -C "$preflight_repository" remote)
preflight_filter_repo_map=$(git -C "$preflight_repository" rev-parse --git-path filter-repo/commit-map)
[[ "$preflight_filter_repo_map" == /* ]] || preflight_filter_repo_map="${preflight_repository}/${preflight_filter_repo_map}"
mkdir -p "$(dirname -- "$preflight_filter_repo_map")"
cp -- "$filter_repo_map" "$preflight_filter_repo_map"
mkdir -p "${preflight_repository}/${provenance_directory}"
cp -- "${output_directory}/${provenance_directory}/README.md" "${preflight_repository}/${provenance_directory}/README.md"
cp -- "${output_directory}/${provenance_directory}/original-to-filtered-commit-map.txt" \
    "${preflight_repository}/${provenance_directory}/original-to-filtered-commit-map.txt"
cp -- "${output_directory}/${provenance_directory}/retained-files.txt" \
    "${preflight_repository}/${provenance_directory}/retained-files.txt"

preflight_manifest="${preflight_repository}/${provenance_directory}/manifest.json"
preflight_audit="${preflight_repository}/${provenance_directory}/filtered-history-audit.txt"
printf 'PENDING\n' > "$preflight_audit"
write_manifest "$preflight_manifest" \
    '0000000000000000000000000000000000000000000000000000000000000000' \
    "$preflight_audit"

git -C "$preflight_repository" add "$provenance_directory"
GIT_AUTHOR_NAME="$provenance_identity_name" \
GIT_AUTHOR_EMAIL="$provenance_identity_email" \
GIT_AUTHOR_DATE="$extraction_timestamp_utc" \
GIT_COMMITTER_NAME="$provenance_identity_name" \
GIT_COMMITTER_EMAIL="$provenance_identity_email" \
GIT_COMMITTER_DATE="$extraction_timestamp_utc" \
    git -C "$preflight_repository" \
        -c user.name="$provenance_identity_name" \
        -c user.email="$provenance_identity_email" \
        -c core.hooksPath=/dev/null \
        commit --no-verify -m "$provenance_commit_message" >/dev/null

expected_audit="${working_parent}/expected-audit.txt"
"$audit_script" "$preflight_repository" "$generated_allowlist" > "$expected_audit" \
    || fail 'preflight provenance audit failed'
expected_audit_hash=$(sha256_file "$expected_audit")

final_manifest="${output_directory}/${provenance_directory}/manifest.json"
final_audit="${output_directory}/${provenance_directory}/filtered-history-audit.txt"
cp -- "$expected_audit" "$final_audit"
write_manifest "$final_manifest" "$expected_audit_hash" "$expected_audit"

git -C "$output_directory" add "$provenance_directory"
GIT_AUTHOR_NAME="$provenance_identity_name" \
GIT_AUTHOR_EMAIL="$provenance_identity_email" \
GIT_AUTHOR_DATE="$extraction_timestamp_utc" \
GIT_COMMITTER_NAME="$provenance_identity_name" \
GIT_COMMITTER_EMAIL="$provenance_identity_email" \
GIT_COMMITTER_DATE="$extraction_timestamp_utc" \
    git -C "$output_directory" \
        -c user.name="$provenance_identity_name" \
        -c user.email="$provenance_identity_email" \
        -c core.hooksPath=/dev/null \
        commit --no-verify -m "$provenance_commit_message" >/dev/null

actual_audit="${working_parent}/actual-audit.txt"
"$audit_script" "$output_directory" "$generated_allowlist" > "$actual_audit" \
    || fail 'post-provenance audit failed'
cmp -s "$expected_audit" "$actual_audit" \
    || fail 'post-provenance audit bytes differ from committed audit evidence'
cmp -s "$final_audit" "$actual_audit" \
    || fail 'committed audit evidence does not match the post-provenance audit'

[[ -z "$(git -C "$output_directory" remote)" ]] || fail 'candidate has a configured remote'
[[ ! -e "${output_directory}/.git/objects/info/alternates" ]] || fail 'candidate depends on an alternate object store'
git -C "$output_directory" fsck --full --no-progress >/dev/null \
    || fail 'candidate failed object integrity validation'

snapshot_source_state "$source_state_after"
cmp -s "$source_state_before" "$source_state_after" \
    || fail 'source repository changed during bootstrap preparation'

final_head=$(git -C "$output_directory" rev-parse --verify HEAD)
provenance_commit_count=$(git -C "$output_directory" log --all --format='%s' | grep -Fx "$provenance_commit_message" | wc -l | tr -d ' ')
[[ "$provenance_commit_count" == 1 ]] || fail 'candidate does not contain exactly one provenance commit'
branch_count=$(git -C "$output_directory" for-each-ref --format='%(refname)' refs/heads/ | wc -l | tr -d ' ')
[[ "$branch_count" == 1 ]] || fail 'candidate contains more than the selected source branch'
[[ "$(git -C "$output_directory" symbolic-ref --short HEAD)" == "$source_branch" ]] \
    || fail 'candidate branch does not match the selected source branch'

printf 'BOOTSTRAP_STATUS=success\n'
printf 'SOURCE_REPOSITORY_ID=%s\n' "$source_repository_id"
printf 'SOURCE_COMMIT=%s\n' "$source_commit"
printf 'SOURCE_BRANCH=%s\n' "$source_branch"
printf 'PRE_PROVENANCE_FILTERED_HEAD=%s\n' "$pre_provenance_head"
printf 'FINAL_BOOTSTRAP_HEAD=%s\n' "$final_head"
printf 'PROVENANCE_COMMIT_COUNT=%s\n' "$provenance_commit_count"
printf 'FILTERED_COMMIT_COUNT=%s\n' "$filtered_commit_count"
printf 'GENERATED_AUDIT_SHA256=%s\n' "$expected_audit_hash"
printf 'REMOTES=none\n'
printf 'SOURCE_IMMUTABLE=true\n'
printf 'NETWORK_CONTACTED=false\n'
printf 'PUSHED=false\n'
