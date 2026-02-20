#!/bin/bash
# RawrXD Local Universal Launcher (Linux/macOS)
# Starts backend + web interface on localhost.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$SCRIPT_DIR/.."

if ! command -v python3 >/dev/null 2>&1; then
    echo "python3 is required but was not found." >&2
    exit 1
fi

exec python3 "$ROOT_DIR/scripts/launch_local_universal.py" "$@"
