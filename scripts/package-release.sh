#!/bin/sh
# Assemble the already tested native binary and its bundled program data.
set -eu
version=$(./pocket --version | awk '{print $2}')
case "$version" in *[!0-9.]*|'') echo "invalid version" >&2; exit 1;; esac
if [ -n "${RELEASE_TAG:-}" ] && [ "$RELEASE_TAG" != "v$version" ]; then
  echo "tag $RELEASE_TAG differs from binary v$version" >&2; exit 1
fi
out=${1:-build/release}
mkdir -p "$out"
stage=$(mktemp -d)
trap 'rm -rf "$stage"' EXIT HUP INT TERM
name="pocketharness-$version-linux-$(uname -m)"
mkdir -p "$stage/$name"
cp pocket LICENSE README.md CHANGELOG.md mascot.png "$stage/$name/"
cp -R docs "$stage/$name/docs"
cp -R examples "$stage/$name/examples"
cp -R skills "$stage/$name/skills"
cp scripts/install-built.sh "$stage/$name/install-built.sh"
cat > "$stage/$name/install.sh" <<'INSTALL'
#!/bin/sh
set -eu
cd "$(dirname "$0")"
exec ./install-built.sh .
INSTALL
chmod 0755 "$stage/$name/install.sh"
# Fixed metadata makes packages reproducible for the same binary+skills.
tar --sort=name --mtime=@0 --owner=0 --group=0 --numeric-owner -C "$stage" -czf "$out/$name.tar.gz" "$name"
(cd "$out" && sha256sum "$name.tar.gz" > "$name.sha256")
printf '%s\n' "$out/$name.tar.gz"
