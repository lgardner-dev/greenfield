#!/usr/bin/env bash
set -euo pipefail

script_directory=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd -P)
allowlist_file="${script_directory}/retained-paths.txt"
large_blob_threshold_bytes=1048576

usage() {
    printf 'Usage: %s FILTERED_REPOSITORY\n' "$(basename -- "$0")" >&2
}

if (( $# != 1 )); then
    usage
    exit 2
fi

repository=$1
[[ -f "$allowlist_file" ]] || {
    printf 'audit-filtered-history: allowlist is missing: %s\n' "$allowlist_file" >&2
    exit 1
}

git -C "$repository" rev-parse --git-dir >/dev/null 2>&1 || {
    printf 'audit-filtered-history: not a Git repository: %s\n' "$repository" >&2
    exit 1
}
head_commit=$(git -C "$repository" rev-parse --verify HEAD 2>/dev/null) || {
    printf 'audit-filtered-history: repository has no HEAD commit\n' >&2
    exit 1
}

declare -a retained_paths=()
while IFS= read -r retained_path || [[ -n "$retained_path" ]]; do
    [[ -n "$retained_path" ]] || continue
    retained_paths+=("$retained_path")
done < "$allowlist_file"

is_retained_path() {
    local candidate=$1
    local retained_path
    for retained_path in "${retained_paths[@]}"; do
        if [[ "$retained_path" == */ ]]; then
            [[ "$candidate" == "${retained_path}"* ]] && return 0
        else
            [[ "$candidate" == "$retained_path" ]] && return 0
        fi
    done
    return 1
}

declare -A all_files=()
declare -A path_violations=()
declare -A credential_files=()
declare -A env_files=()
declare -A private_key_files=()
declare -A token_files=()
declare -A large_files=()
declare -A binary_files=()
declare -A scanned_blobs=()
declare -A missing_retained_paths=()

scan_directory=$(mktemp -d "${TMPDIR:-/tmp}/software-factory-audit.XXXXXX")
cleanup() {
    rm -rf -- "$scan_directory"
}
trap cleanup EXIT

mapfile -t commits < <(git -C "$repository" rev-list --all "$head_commit")
if (( ${#commits[@]} == 0 )); then
    printf 'audit-filtered-history: no reachable commits\n' >&2
    exit 1
fi

for commit in "${commits[@]}"; do
    while IFS= read -r -d '' path; do
        if ! is_retained_path "$path"; then
            path_violations["$path"]=1
            continue
        fi

        all_files["$path"]=1
        object_id=$(git -C "$repository" rev-parse "${commit}:${path}")
        if [[ -n "${scanned_blobs[$object_id]+present}" ]]; then
            continue
        fi
        scanned_blobs["$object_id"]=1

        blob_file="${scan_directory}/${object_id}"
        git -C "$repository" cat-file blob "$object_id" > "$blob_file"
        blob_size=$(git -C "$repository" cat-file -s "$object_id")
        if (( blob_size > large_blob_threshold_bytes )); then
            large_files["$path"]=1
        fi

        base_name=${path##*/}
        case "$base_name" in
            .env|.env.*)
                env_files["$path"]=1
                ;;
            credentials|credentials.*|*credential*|secret|secret.*|*secret*|*.pem|*.key|*.p12|*.pfx|id_rsa|id_ed25519)
                credential_files["$path"]=1
                ;;
        esac

        if (( blob_size > 0 )); then
            set +o pipefail
            LC_ALL=C grep -Iq . "$blob_file"
            is_text=$?
            set -o pipefail
            if (( is_text != 0 )); then
                binary_files["$path"]=1
            fi
        fi

        if LC_ALL=C grep -Eq -- '-----BEGIN ([A-Z0-9 ]+ )?PRIVATE KEY-----' "$blob_file"; then
            private_key_files["$path"]=1
        fi
        if LC_ALL=C grep -Eq -- '(gh[pousr]_[A-Za-z0-9_]{20,}|github_pat_[A-Za-z0-9_]{20,}|xox[baprs]-[A-Za-z0-9-]{20,}|AKIA[0-9A-Z]{16}|AIza[0-9A-Za-z_-]{30,}|sk-[A-Za-z0-9]{20,})' "$blob_file"; then
            token_files["$path"]=1
        fi
    done < <(git -C "$repository" ls-tree -r --name-only -z "$commit")
done

head_files=()
while IFS= read -r -d '' path; do
    head_files+=("$path")
done < <(git -C "$repository" ls-tree -r --name-only -z HEAD)

for retained_path in "${retained_paths[@]}"; do
    found=1
    for path in "${head_files[@]}"; do
        if [[ "$retained_path" == */ ]]; then
            [[ "$path" == "${retained_path}"* ]] && found=0 && break
        else
            [[ "$path" == "$retained_path" ]] && found=0 && break
        fi
    done
    if (( found != 0 )); then
        missing_retained_paths["$retained_path"]=1
    fi
done

print_map() {
    local label=$1
    local map_name=$2
    declare -n map_reference="$map_name"
    if (( ${#map_reference[@]} == 0 )); then
        printf '%s_COUNT=0\n' "$label"
        return
    fi
    printf '%s_COUNT=%s\n' "$label" "${#map_reference[@]}"
    while IFS= read -r path; do
        printf '%s=%s\n' "$label" "$path"
    done < <(printf '%s\n' "${!map_reference[@]}" | LC_ALL=C sort)
}

printf 'FILTERED_HEAD=%s\n' "$head_commit"
printf 'REF_COUNT=%s\n' "$(git -C "$repository" for-each-ref --format='%(refname)' | wc -l | tr -d ' ')"
while IFS= read -r ref; do
    [[ -n "$ref" ]] || continue
    printf 'REF=%s\n' "$ref"
done < <(git -C "$repository" for-each-ref --format='%(refname) %(objectname)')
printf 'ROOT_COUNT=%s\n' "$(git -C "$repository" rev-list --all "$head_commit" --max-parents=0 | wc -l | tr -d ' ')"
while IFS= read -r root; do
    [[ -n "$root" ]] || continue
    printf 'ROOT=%s\n' "$root"
done < <(git -C "$repository" rev-list --all "$head_commit" --max-parents=0 | LC_ALL=C sort)
printf 'COMMIT_COUNT=%s\n' "${#commits[@]}"
while IFS= read -r author; do
    [[ -n "$author" ]] || continue
    printf 'AUTHOR=%s\n' "$author"
done < <(git -C "$repository" log --all "$head_commit" --format='%aN <%aE>' | LC_ALL=C sort -u)
dates=$(git -C "$repository" log --all "$head_commit" --format='%aI' | LC_ALL=C sort)
printf 'DATE_MIN=%s\n' "$(printf '%s\n' "$dates" | sed -n '1p')"
printf 'DATE_MAX=%s\n' "$(printf '%s\n' "$dates" | sed -n '$p')"
printf 'TAG_COUNT=%s\n' "$(git -C "$repository" tag --list | wc -l | tr -d ' ')"
while IFS= read -r tag; do
    [[ -n "$tag" ]] || continue
    printf 'TAG=%s\n' "$tag"
done < <(git -C "$repository" tag --list | LC_ALL=C sort)
printf 'BRANCH_COUNT=%s\n' "$(git -C "$repository" for-each-ref --format='%(refname:strip=2)' refs/heads/ | wc -l | tr -d ' ')"
while IFS= read -r branch; do
    [[ -n "$branch" ]] || continue
    printf 'BRANCH=%s\n' "$branch"
done < <(git -C "$repository" for-each-ref --format='%(refname:strip=2)' refs/heads/ | LC_ALL=C sort)
printf 'RETAINED_FILE_COUNT=%s\n' "${#head_files[@]}"
while IFS= read -r path; do
    [[ -n "$path" ]] || continue
    printf 'RETAINED_FILE=%s\n' "$path"
done < <(printf '%s\n' "${head_files[@]}" | LC_ALL=C sort)

print_map PATH_VIOLATION path_violations
print_map MISSING_RETAINED_PATH missing_retained_paths
print_map CREDENTIAL_FILE credential_files
print_map ENV_FILE env_files
print_map PRIVATE_KEY private_key_files
print_map LIKELY_TOKEN token_files
print_map LARGE_BLOB large_files
print_map UNEXPECTED_BINARY binary_files

printf 'REMOTE_COUNT=%s\n' "$(git -C "$repository" remote | wc -l | tr -d ' ')"
while IFS= read -r remote; do
    [[ -n "$remote" ]] || continue
    printf 'REMOTE=%s\n' "$remote"
done < <(git -C "$repository" remote)

failure=0
if (( ${#path_violations[@]} != 0 || ${#missing_retained_paths[@]} != 0 )); then
    failure=1
fi
if (( ${#env_files[@]} != 0 || ${#private_key_files[@]} != 0 || ${#token_files[@]} != 0 )); then
    failure=1
fi
if [[ -n "$(git -C "$repository" remote)" ]]; then
    failure=1
fi
if (( failure != 0 )); then
    exit 1
fi
