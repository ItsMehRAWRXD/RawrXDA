#!/usr/bin/env bash
# RawrXD bootable space: Mac/Linux wrapper to run RawrXD IDE under Wine.
# Usage: ./scripts/rawrxd-space.sh [--cli] [-- BUILD_DIR]
# Env: RAWRXD_SPACE_DIR (Wine prefix), RAWRXD_BUILD_DIR, LAUNCH_CLI=1, WINE

set -e
RAWRXD_SPACE_DIR="${RAWRXD_SPACE_DIR:-$HOME/.rawrxd/rawrxd-space}"
RAWRXD_BUILD_DIR="${RAWRXD_BUILD_DIR:-}"
LAUNCH_CLI="${LAUNCH_CLI:-0}"
WINE="${WINE:-wine}"

while [ $# -gt 0 ]; do
  case "$1" in
    --cli) LAUNCH_CLI=1 ;;
    --) shift; break ;;
    *) break ;;
  esac
  shift
done
[ -n "$1" ] && RAWRXD_BUILD_DIR="$1"

if [ -z "$RAWRXD_BUILD_DIR" ]; then
  SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
  REPO="$(cd "$SCRIPT_DIR/.." && pwd)"
  for d in "$REPO/build_ide/bin" "$REPO/build_ide/Release" "$REPO/build_ide"; do
    [ -f "$d/RawrXD-Win32IDE.exe" ] && RAWRXD_BUILD_DIR="$d" && break
  done
fi

[ -z "$RAWRXD_BUILD_DIR" ] || [ ! -d "$RAWRXD_BUILD_DIR" ] && { echo "Set RAWRXD_BUILD_DIR or pass build dir"; exit 1; }
mkdir -p "$RAWRXD_SPACE_DIR"
export WINEPREFIX="$RAWRXD_SPACE_DIR"

if [ "$LAUNCH_CLI" = "1" ]; then EXE="RawrXD_CLI.exe"; else EXE="RawrXD-Win32IDE.exe"; fi
[ ! -f "$RAWRXD_BUILD_DIR/$EXE" ] && { echo "Not found: $RAWRXD_BUILD_DIR/$EXE"; exit 1; }
cd "$RAWRXD_BUILD_DIR"
exec "$WINE" "$EXE" "$@"
