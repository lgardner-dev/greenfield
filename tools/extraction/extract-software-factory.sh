#!/usr/bin/env bash
set -euo pipefail

script_directory=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd -P)
allowlist_file="${script_directory}/retained-paths.txt"

usage() {
    printf 'Usage: %s SOURCE_REPOSITORY OUTPUT_DIRECTORY\n' "$(basename -- "$0")" >&2
}

fail() {
    printf 'extract-software-factory: %s\n' "$1" >&2
    exit 1
}

if (( $# != 2 )); then
    usage
    exit 2
fi

source_repository=$1
output_directory=$2

if [[ -z "$source_repository" || -z "$output_directory" ]]; then
    usage
    exit 2
fi
if [[ "$source_repository" == -* || "$output_directory" == -* ]]; then
    usage
    exit 2
fi
if [[ ! -f "$allowlist_file" ]]; then
    fail "allowlist is missing: ${allowlist_file}"
fi
if [[ -e "$output_directory" ]]; then
    fail "output directory already exists; choose a new disposable path"
fi

source_top_level=$(git -C "$source_repository" rev-parse --show-toplevel 2>/dev/null) \
    || fail "source is not a non-bare Git repository: ${source_repository}"
source_top_level=$(CDPATH= cd -- "$source_top_level" && pwd -P)
source_commit=$(git -C "$source_top_level" rev-parse --verify HEAD 2>/dev/null) \
    || fail "source repository has no HEAD commit"
source_branch=$(git -C "$source_top_level" symbolic-ref --quiet --short HEAD 2>/dev/null) \
    || fail "source repository must be checked out on an explicit branch"
git check-ref-format --branch "$source_branch" >/dev/null 2>&1 \
    || fail "source repository branch is invalid: ${source_branch}"

output_parent=$(dirname -- "$output_directory")
output_name=$(basename -- "$output_directory")
[[ -d "$output_parent" ]] \
    || fail "output parent directory does not exist: ${output_parent}"
output_parent=$(CDPATH= cd -- "$output_parent" && pwd -P)
output_directory="${output_parent}/${output_name}"

case "${output_directory}/" in
    "${source_top_level}/"*)
        fail "output directory must not be inside the supplied source repository"
        ;;
esac

filter_repo_version=$(git-filter-repo --version 2>/dev/null) \
    || fail "git-filter-repo is required"

staging_parent=$(mktemp -d "${output_parent}/.software-factory-extract.XXXXXX")
staging_repository="${staging_parent}/repository"
cleanup() {
    rm -rf -- "$staging_parent"
}
trap cleanup EXIT

export GIT_CONFIG_NOSYSTEM=1
export GIT_TERMINAL_PROMPT=0

# --no-local and --no-hardlinks force a new object database rather than a
# hard-link or alternate-object relationship with the source checkout.
#
# --single-branch prevents temporary task/agent branches from entering the
# standalone repository. Only the explicitly checked-out source branch is
# eligible for extraction.
git clone \
    --no-local \
    --no-hardlinks \
    --single-branch \
    --branch "$source_branch" \
    "$source_top_level" \
    "$staging_repository" \
    || fail "could not make an independent single-branch local clone"

if [[ -e "${staging_repository}/.git/objects/info/alternates" ]]; then
    fail "clone unexpectedly depends on a source alternate object store"
fi

git -C "$staging_repository" filter-repo \
    --force \
    --paths-from-file "$allowlist_file" \
    --prune-empty always \
    --prune-degenerate always

while IFS= read -r remote_name; do
    [[ -n "$remote_name" ]] || continue
    git -C "$staging_repository" remote remove "$remote_name"
done < <(git -C "$staging_repository" remote)

while IFS= read -r remote_ref; do
    [[ -n "$remote_ref" ]] || continue
    git -C "$staging_repository" update-ref -d "$remote_ref"
done < <(git -C "$staging_repository" for-each-ref --format='%(refname)' refs/remotes/)

if [[ -e "${staging_repository}/.git/objects/info/alternates" ]]; then
    fail "filtered repository still depends on an alternate object store"
fi
if [[ -n "$(git -C "$staging_repository" remote)" ]]; then
    fail "filtered repository still has a configured remote"
fi

git -C "$staging_repository" fsck --full --no-progress >/dev/null \
    || fail "filtered repository failed object integrity validation"

mv -- "$staging_repository" "$output_directory"

filtered_head=$(git -C "$output_directory" rev-parse --verify HEAD 2>/dev/null) \
    || fail "filtered repository has no HEAD commit"
commit_count=$(git -C "$output_directory" rev-list --all "$filtered_head" --count)
retained_file_count=$(git -C "$output_directory" ls-tree -r --name-only HEAD | wc -l | tr -d ' ')
remote_summary=$(git -C "$output_directory" remote | paste -sd ',' -)

printf 'SOURCE_REPOSITORY=%s\n' "$source_top_level"
printf 'SOURCE_COMMIT=%s\n' "$source_commit"
printf 'SOURCE_BRANCH=%s\n' "$source_branch"
printf 'FILTER_REPO_VERSION=%s\n' "$filter_repo_version"
printf 'FILTERED_HEAD=%s\n' "$filtered_head"
printf 'COMMIT_COUNT=%s\n' "$commit_count"
printf 'RETAINED_FILE_COUNT=%s\n' "$retained_file_count"
printf 'REMOTES=%s\n' "${remote_summary:-none}"
while IFS= read -r ref_name; do
    [[ -n "$ref_name" ]] || continue
    printf 'REF=%s\n' "$ref_name"
done < <(git -C "$output_directory" for-each-ref --format='%(refname) %(objectname)')
while IFS= read -r retained_file; do
    [[ -n "$retained_file" ]] || continue
    printf 'RETAINED_FILE=%s\n' "$retained_file"
done < <(git -C "$output_directory" ls-tree -r --name-only HEAD)
