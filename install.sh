#!/bin/sh
# PocketHarness installer: builds from source and installs to ~/.local/bin.
# No root needed. Requires: g++, make, curl.
set -eu

cd "$(dirname "$0")"

for t in g++ make curl; do
  if ! command -v "$t" >/dev/null 2>&1; then
    echo "install.sh: required tool missing: $t" >&2
    exit 1
  fi
done

make
make test
make install "PREFIX=${PREFIX:-$HOME/.local}"

BIN="${PREFIX:-$HOME/.local}/bin/pocket"
echo "installed $BIN"
"$BIN" --version

case ":$PATH:" in
  *":$(dirname "$BIN"):"*) ;;
  *) echo "NOTE: $(dirname "$BIN") is not on PATH. Add this to ~/.profile:" >&2
     echo "  export PATH=\"\$HOME/.local/bin:\$PATH\"" >&2 ;;
esac
