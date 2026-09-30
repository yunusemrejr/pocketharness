#!/bin/sh
# Exercise the exact package installer in isolated directories, including spaces.
set -eu
root=$(mktemp -d)
trap 'rm -rf "$root"' EXIT HUP INT TERM
./scripts/package-release.sh "$root/artifacts"
(cd "$root/artifacts" && sha256sum -c ./*.sha256)
tar -xzf "$root"/artifacts/*.tar.gz -C "$root"
package_dir=$(find "$root" -maxdepth 1 -type d -name 'pocketharness-*')
mkdir -p "$root/home/.config/pocketharness/skills/personal" "$root/home/.local/share/pocketharness/skills/retired"
printf 'private configuration\n' > "$root/home/.config/pocketharness/config.json"
printf 'personal skill\n' > "$root/home/.config/pocketharness/skills/personal/SKILL.md"
env HOME="$root/home" PREFIX="$root/custom prefix" "$package_dir/install.sh"
installed="$root/custom prefix/bin/pocket"
test "$("$installed" --version)" = "$(./pocket --version)"
cmp pocket "$installed"
test -f "$root/home/.config/pocketharness/skills/personal/SKILL.md"
test "$(cat "$root/home/.config/pocketharness/config.json")" = 'private configuration'
test ! -e "$root/home/.local/share/pocketharness/skills/retired"
test -f "$root/home/.local/share/pocketharness/skills/project-workflows/SKILL.md"
mkdir -p "$root/blocked prefix/bin/pocket"
printf 'previous bundle\n' > "$root/home/.local/share/pocketharness/skills/installation-marker"
if env HOME="$root/home" PREFIX="$root/blocked prefix" "$package_dir/install.sh"; then
    echo 'installer accepted a directory as its binary destination' >&2
    exit 1
fi
test -d "$root/blocked prefix/bin/pocket"
test "$(cat "$root/home/.local/share/pocketharness/skills/installation-marker")" = 'previous bundle'
rm "$root/home/.local/share/pocketharness/skills/installation-marker"
env HOME="$root/home" "$installed" --workflow "Build and deploy a PHP 8 website through Git and SSH on Namecheap" > "$root/workflow.json"
python3 - "$root/workflow.json" <<'PYTHON'
import json,sys
report=json.load(open(sys.argv[1]))
assert report['planning_brief'] and report['ui']
assert 'shared-hosting-deployment' in report['skills']
assert 'php-application-engineering' in report['skills']
PYTHON
"$installed" kit sfx "$root/installed.wav" chime --duration .1
"$installed" kit audio "$root/installed.wav"
printf 'release package/install smoke passed\n'
