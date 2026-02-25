#!/bin/bash
set -e
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"
export SOAK_LOG_DIR="${SOAK_LOG_DIR:-$ROOT/soak_logs}"
mkdir -p "$SOAK_LOG_DIR"
exec python3 scripts/soak_cross_platform.py "$@"
