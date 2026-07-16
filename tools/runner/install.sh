#!/bin/sh
set -eu

INSTALLER_FORMAT="1"
SCRIPT_DIRECTORY=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
SOURCE_ROOT=$(CDPATH= cd -- "$SCRIPT_DIRECTORY/../.." && pwd)
PACKAGE_SOURCE="$SCRIPT_DIRECTORY/greenfield_agent"
SERVICE_TEMPLATE="$SCRIPT_DIRECTORY/systemd/greenfield-task@.service.in"

PREFIX=/srv/greenfield
SYSTEMD_USER_DIR=/etc/systemd/user
VERSION=
OWNER=root
GROUP=root
ALLOW_DIRTY=0
SKIP_OWNER_CHANGE=0
NO_SYSTEMD=0

temporary_release=
temporary_current=
temporary_wrapper=
temporary_unit=

fail() {
    printf 'greenfield-agent installer: %s\n' "$1" >&2
    exit 1
}

usage() {
    cat <<'EOF'
Usage: tools/runner/install.sh [options]

Options:
  --prefix PATH              Installation prefix (default: /srv/greenfield)
  --systemd-user-dir PATH    User unit directory (default: /etc/systemd/user)
  --version VALUE            Release directory name (default: Git commit)
  --owner USER               Installed-file owner (default: root)
  --group GROUP              Installed-file group (default: root)
  --skip-owner-change        Do not call chown (for unprivileged tests)
  --allow-dirty              Permit an explicitly identified dirty checkout
  --no-systemd               Do not install the user-service template
  --help                     Show this help
EOF
}

require_value() {
    option=$1
    shift
    if [ "$#" -eq 0 ] || [ -z "$1" ]; then
        fail "missing value for $option"
    fi
}

while [ "$#" -gt 0 ]; do
    case "$1" in
        --prefix)
            require_value "$1" "$@"
            shift
            PREFIX=$1
            ;;
        --prefix=*)
            PREFIX=${1#--prefix=}
            [ -n "$PREFIX" ] || fail "missing value for --prefix"
            ;;
        --systemd-user-dir)
            require_value "$1" "$@"
            shift
            SYSTEMD_USER_DIR=$1
            ;;
        --systemd-user-dir=*)
            SYSTEMD_USER_DIR=${1#--systemd-user-dir=}
            [ -n "$SYSTEMD_USER_DIR" ] || fail "missing value for --systemd-user-dir"
            ;;
        --version)
            require_value "$1" "$@"
            shift
            VERSION=$1
            ;;
        --version=*)
            VERSION=${1#--version=}
            [ -n "$VERSION" ] || fail "missing value for --version"
            ;;
        --owner)
            require_value "$1" "$@"
            shift
            OWNER=$1
            ;;
        --owner=*)
            OWNER=${1#--owner=}
            [ -n "$OWNER" ] || fail "missing value for --owner"
            ;;
        --group)
            require_value "$1" "$@"
            shift
            GROUP=$1
            ;;
        --group=*)
            GROUP=${1#--group=}
            [ -n "$GROUP" ] || fail "missing value for --group"
            ;;
        --skip-owner-change)
            SKIP_OWNER_CHANGE=1
            ;;
        --allow-dirty)
            ALLOW_DIRTY=1
            ;;
        --no-systemd)
            NO_SYSTEMD=1
            ;;
        --help|-h)
            usage
            exit 0
            ;;
        *)
            fail "unknown option: $1"
            ;;
    esac
    shift
done

validate_path() {
    path=$1
    description=$2
    case "$path" in
        ""|/)
            fail "$description must not be empty or /"
            ;;
        /*)
            ;;
        *)
            fail "$description must be an absolute path"
            ;;
    esac
    case "$path" in
        *[!A-Za-z0-9_./-]*)
            fail "$description contains unsafe characters"
            ;;
    esac
    case "/$path/" in
        */../*|*/./*)
            fail "$description must not contain . or .. path components"
            ;;
    esac
}

validate_path "$PREFIX" "prefix"
validate_path "$SYSTEMD_USER_DIR" "systemd user directory"

case "$VERSION" in
    "")
        ;;
    *[!A-Za-z0-9._+-]*)
        fail "version contains unsafe characters"
        ;;
esac
case "$OWNER" in
    ""|*[!A-Za-z0-9_.-]*)
        fail "owner contains unsafe characters"
        ;;
esac
case "$GROUP" in
    ""|*[!A-Za-z0-9_.-]*)
        fail "group contains unsafe characters"
        ;;
esac

[ -d "$PACKAGE_SOURCE" ] || fail "runtime package is missing: $PACKAGE_SOURCE"
[ -f "$PACKAGE_SOURCE/__init__.py" ] || fail "runtime package entry point is missing"
[ -f "$PACKAGE_SOURCE/__main__.py" ] || fail "runtime module entry point is missing"
[ -f "$SERVICE_TEMPLATE" ] || fail "systemd service template is missing: $SERVICE_TEMPLATE"

package_symlink=$(find "$PACKAGE_SOURCE" -type l -print -quit)
[ -z "$package_symlink" ] || fail "runtime package contains a symlink: $package_symlink"

git_root=$(git -C "$SOURCE_ROOT" rev-parse --show-toplevel 2>/dev/null) \
    || fail "source directory is not a Git checkout: $SOURCE_ROOT"
[ "$git_root" = "$SOURCE_ROOT" ] || fail "source Git root is unexpected: $git_root"

source_commit=$(git -C "$SOURCE_ROOT" rev-parse HEAD 2>/dev/null) \
    || fail "cannot determine the source commit"
if [ -z "$VERSION" ]; then
    VERSION=$(git -C "$SOURCE_ROOT" rev-parse --short=12 HEAD 2>/dev/null) \
        || fail "cannot determine the release version"
fi
case "$VERSION" in
    ""|*[!A-Za-z0-9._+-]*)
        fail "version contains unsafe characters"
        ;;
esac

dirty_status=$(git -C "$SOURCE_ROOT" status --porcelain --untracked-files=all)
if [ "$ALLOW_DIRTY" -eq 0 ] && [ -n "$dirty_status" ]; then
    fail "source checkout is dirty; use --allow-dirty only for an explicit local test"
fi

runner_library="$PREFIX/lib/greenfield-runner"
release_root="$runner_library/releases"
release_directory="$release_root/$VERSION"
wrapper_directory="$PREFIX/bin"
wrapper_path="$wrapper_directory/greenfield-agent"
unit_path="$SYSTEMD_USER_DIR/greenfield-task@.service"

mkdir -p "$release_root" "$wrapper_directory"
chmod 755 "$runner_library" "$release_root" "$wrapper_directory"
if [ "$SKIP_OWNER_CHANGE" -eq 0 ]; then
    chown "$OWNER:$GROUP" "$runner_library" "$release_root" "$wrapper_directory"
fi

if [ -L "$release_directory" ]; then
    fail "release path is a symlink: $release_directory"
fi

cleanup() {
    [ -z "$temporary_release" ] || rm -rf "$temporary_release"
    [ -z "$temporary_current" ] || rm -f "$temporary_current"
    [ -z "$temporary_wrapper" ] || rm -f "$temporary_wrapper"
    [ -z "$temporary_unit" ] || rm -f "$temporary_unit"
}
trap cleanup EXIT HUP INT TERM

temporary_release=$(mktemp -d "$release_root/.install-$VERSION.XXXXXX") \
    || fail "cannot create a temporary release directory"
mkdir "$temporary_release/greenfield_agent"

for source_file in "$PACKAGE_SOURCE"/*.py; do
    [ -f "$source_file" ] || fail "runtime package has no Python modules"
    cp -p "$source_file" "$temporary_release/greenfield_agent/${source_file##*/}"
done

installation_timestamp=$(date -u '+%Y-%m-%dT%H:%M:%SZ') \
    || fail "cannot determine installation timestamp"
printf '%s\n' "$VERSION" >"$temporary_release/VERSION"
printf '%s\n' \
    "source_commit=$source_commit" \
    "installed_at=$installation_timestamp" \
    "installer_format=$INSTALLER_FORMAT" \
    "source_repository=$SOURCE_ROOT" \
    "dirty_install_allowed=$([ "$ALLOW_DIRTY" -eq 1 ] && printf true || printf false)" \
    "requested_version=$VERSION" >"$temporary_release/INSTALL-METADATA"

find "$temporary_release" -type d -exec chmod 755 {} \;
find "$temporary_release" -type f -exec chmod 644 {} \;
if [ "$SKIP_OWNER_CHANGE" -eq 0 ]; then
    chown -R "$OWNER:$GROUP" "$temporary_release"
fi

if [ -e "$release_directory" ]; then
    [ -d "$release_directory" ] || fail "release path is not a directory: $release_directory"
    [ "$(cat "$release_directory/VERSION" 2>/dev/null || true)" = "$VERSION" ] \
        || fail "existing release is missing matching VERSION: $release_directory"
    [ -f "$release_directory/greenfield_agent/__main__.py" ] \
        || fail "existing release is incomplete: $release_directory"
    if [ "$SKIP_OWNER_CHANGE" -eq 0 ]; then
        chown -R "$OWNER:$GROUP" "$release_directory"
    fi
    find "$release_directory" -type d -exec chmod 755 {} \;
    find "$release_directory" -type f -exec chmod 644 {} \;
    rm -rf "$temporary_release"
    temporary_release=
else
    mv "$temporary_release" "$release_directory"
    temporary_release=
fi

if [ -L "$wrapper_path" ] || [ -d "$wrapper_path" ]; then
    fail "wrapper destination is not a replaceable regular file: $wrapper_path"
fi
temporary_wrapper="$wrapper_directory/.greenfield-agent.$$.tmp"
cat >"$temporary_wrapper" <<EOF
#!/bin/sh
set -eu

install_prefix='$PREFIX'
package_directory="\$install_prefix/lib/greenfield-runner/current"
if [ ! -d "\$package_directory/greenfield_agent" ] || [ ! -f "\$package_directory/greenfield_agent/__main__.py" ]; then
    printf '%s\\n' "greenfield-agent: installed package is missing: \$package_directory" >&2
    exit 1
fi

if [ -z "\${GREENFIELD_ROOT+x}" ]; then
    export GREENFIELD_ROOT="\$install_prefix"
fi
if [ -z "\${GREENFIELD_REPOSITORY+x}" ]; then
    export GREENFIELD_REPOSITORY="\$install_prefix/repos/greenfield"
fi
export GREENFIELD_AGENT_INSTALL_PREFIX="\$install_prefix"
export GREENFIELD_AGENT_PACKAGE="\$package_directory"
export GREENFIELD_AGENT_WRAPPER="\$0"
export GREENFIELD_AGENT_SYSTEMD_UNIT_DIR='$SYSTEMD_USER_DIR'
if [ -n "\${PYTHONPATH:-}" ]; then
    export PYTHONPATH="\$package_directory:\$PYTHONPATH"
else
    export PYTHONPATH="\$package_directory"
fi

exec python3 -m greenfield_agent "\$@"
EOF
chmod 755 "$temporary_wrapper"
if [ "$SKIP_OWNER_CHANGE" -eq 0 ]; then
    chown "$OWNER:$GROUP" "$temporary_wrapper"
fi
mv -f "$temporary_wrapper" "$wrapper_path"
temporary_wrapper=

if [ "$NO_SYSTEMD" -eq 0 ]; then
    mkdir -p "$SYSTEMD_USER_DIR"
    if [ -L "$unit_path" ] || [ -d "$unit_path" ]; then
        fail "systemd unit destination is not a replaceable regular file: $unit_path"
    fi
    temporary_unit="$SYSTEMD_USER_DIR/.greenfield-task@.$$.tmp"
    sed \
        -e "s|@PREFIX@|$PREFIX|g" \
        -e "s|@SYSTEMD_USER_DIR@|$SYSTEMD_USER_DIR|g" \
        "$SERVICE_TEMPLATE" >"$temporary_unit"
    chmod 644 "$temporary_unit"
    if [ "$SKIP_OWNER_CHANGE" -eq 0 ]; then
        chown "$OWNER:$GROUP" "$temporary_unit"
    fi
    mv -f "$temporary_unit" "$unit_path"
    temporary_unit=
fi

temporary_current="$runner_library/.current.$$.tmp"
ln -s "releases/$VERSION" "$temporary_current"
mv -Tf "$temporary_current" "$runner_library/current"
temporary_current=

printf 'Installed Greenfield runner release %s\n' "$VERSION"
printf '  Package: %s\n' "$runner_library/current"
printf '  Wrapper: %s\n' "$wrapper_path"
if [ "$NO_SYSTEMD" -eq 0 ]; then
    printf '  Service: %s\n' "$unit_path"
fi
printf '\nAs greenfield-agent, run:\n'
printf '  systemctl --user daemon-reload\n'
printf '  systemctl --user cat greenfield-task@.service\n'
