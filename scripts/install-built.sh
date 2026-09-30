#!/bin/sh
# Install a tested binary and bundled skills; never touch user config/sessions.
set -eu
source_dir=${1:-.}
bindir="${PREFIX:-$HOME/.local}/bin"
skilldir="$HOME/.local/share/pocketharness/skills"
mkdir -p "$bindir" "$(dirname "$skilldir")"
if [ -d "$bindir/pocket" ]; then
    printf 'cannot install over directory %s\n' "$bindir/pocket" >&2
    exit 1
fi
staged_bin=$(mktemp "$bindir/.pocket-install.XXXXXX")
staged_skills=$(mktemp -d "$(dirname "$skilldir")/.pocket-skills.XXXXXX")
backup_skills="$staged_skills.previous"
cleanup() {
    rm -f "$staged_bin"
    rm -rf "$staged_skills"
    if [ -d "$backup_skills" ]; then
        if [ ! -e "$skilldir" ]; then mv "$backup_skills" "$skilldir"; fi
    fi
}
trap cleanup EXIT HUP INT TERM
install -m 0755 "$source_dir/pocket" "$staged_bin"
cp -R "$source_dir/skills/." "$staged_skills/"
"$staged_bin" --version
if [ -e "$skilldir" ]; then mv "$skilldir" "$backup_skills"; fi
mv "$staged_skills" "$skilldir"
if ! mv -fT "$staged_bin" "$bindir/pocket"; then
    rm -rf "$skilldir"
    exit 1
fi
rm -rf "$backup_skills"
printf 'installed %s and bundled skills\n' "$bindir/pocket"
