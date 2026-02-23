#!/usr/bin/env bash
# RawrXD — run backend only (no Wine). Builds and runs RawrEngine on Linux/macOS.
set -e
REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${RAWRXD_BUILD_DIR:-$REPO_ROOT/build}"
cd "$REPO_ROOT"
if [ ! -x "$BUILD_DIR/RawrEngine" ] && [ ! -x "$BUILD_DIR/RawrEngine.exe" ]; then
  echo "Building RawrEngine..."
  mkdir -p "$BUILD_DIR"
  (cd "$BUILD_DIR" && cmake .. -DCMAKE_BUILD_TYPE=Release && cmake --build . --target RawrEngine -j"$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)")
fi
EXE="$BUILD_DIR/RawrEngine"
[ -x "$EXE" ] || EXE="$BUILD_DIR/RawrEngine.exe"
[ -x "$EXE" ] || { echo "RawrEngine not found." >&2; exit 1; }
exec "$EXE" "$@"
