#!/usr/bin/env bash
# RawrXD IDE — macOS launcher (bootable space). Requires Wine and a Windows build.
set -e
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
[ -f "$SCRIPT_DIR/rawrxd-space.env" ] && set -a && source "$SCRIPT_DIR/rawrxd-space.env" && set +a
RAWRXD_SPACE_ROOT="${RAWRXD_SPACE_ROOT:-$HOME/Library/Application Support/RawrXD/rawrxd-space}"
export WINEPREFIX="${WINEPREFIX:-$RAWRXD_SPACE_ROOT}"
WINE="${WINE:-}"
for c in wine wine64; do [ -x "$(command -v $c 2>/dev/null)" ] && WINE=$c && break; done
[ -z "$WINE" ] && [ -d "/Applications/CrossOver.app" ] && WINE="/Applications/CrossOver.app/Contents/SharedSupport/CrossOver/bin/wine"
IDE_EXE="${RAWRXD_IDE_EXE:-}"
[ -z "$IDE_EXE" ] && [ -f "$REPO_ROOT/build_ide/bin/RawrXD-Win32IDE.exe" ] && IDE_EXE="$REPO_ROOT/build_ide/bin/RawrXD-Win32IDE.exe"
[ -z "$IDE_EXE" ] && [ -f "$REPO_ROOT/build/bin/RawrXD-Win32IDE.exe" ] && IDE_EXE="$REPO_ROOT/build/bin/RawrXD-Win32IDE.exe"
if [ -z "$IDE_EXE" ] || [ ! -f "$IDE_EXE" ]; then
  echo "Set RAWRXD_IDE_EXE or copy RawrXD-Win32IDE.exe. See README.md." >&2
  exit 1
fi
[ ! -d "$RAWRXD_SPACE_ROOT/drive_c" ] && mkdir -p "$RAWRXD_SPACE_ROOT" && $WINE wineboot --init 2>/dev/null || true
exec $WINE "$IDE_EXE" "$@"
