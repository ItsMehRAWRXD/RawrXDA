#!/bin/bash
# RawrXD Linux Launcher - Wine-based Bootable Space
# Runs RawrXD-Win32IDE.exe without code changes.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
WINE_PREFIX="${WINE_PREFIX:-$SCRIPT_DIR/.wine_rawrxd}"
IDE_PATH="${IDE_PATH:-$SCRIPT_DIR/../RawrXD-Win32IDE.exe}"
BACKEND_ONLY="${BACKEND_ONLY:-0}"

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

log() { echo -e "${GREEN}[RawrXD]${NC} $1"; }
warn() { echo -e "${YELLOW}[Warning]${NC} $1"; }
error() { echo -e "${RED}[Error]${NC} $1"; }

check_dependencies() {
    if [[ "$BACKEND_ONLY" == "1" ]]; then
        if ! command -v python3 >/dev/null 2>&1; then
            error "Python3 is required for backend-only mode."
            exit 1
        fi
        return
    fi

    if ! command -v wine >/dev/null 2>&1 && ! command -v wine64 >/dev/null 2>&1; then
        error "Wine is not installed. Install with:"
        echo "  Ubuntu/Debian: sudo apt install wine64"
        echo "  Fedora: sudo dnf install wine"
        echo "  Arch: sudo pacman -S wine"
        exit 1
    fi

    if command -v vulkaninfo >/dev/null 2>&1; then
        log "Vulkan detected - GPU acceleration available."
    else
        warn "Vulkan not detected - falling back to CPU rendering."
    fi
}

setup_wine_prefix() {
    if [[ ! -d "$WINE_PREFIX" ]]; then
        log "Initializing Wine prefix at $WINE_PREFIX ..."
        mkdir -p "$WINE_PREFIX"
        WINEARCH=win64 WINEPREFIX="$WINE_PREFIX" winecfg /v win10 >/dev/null 2>&1 || true

        if command -v winetricks >/dev/null 2>&1; then
            log "Installing VC++ runtime via winetricks ..."
            WINEPREFIX="$WINE_PREFIX" winetricks -q vcrun2022 >/dev/null 2>&1 || warn "winetricks vcrun2022 failed; continuing."
        fi
    fi
}

launch_backend() {
    log "Starting RawrEngine backend (Linux native) ..."
    cd "$SCRIPT_DIR/.."

    export RAWRXD_BACKEND_ONLY=1
    export RAWRXD_HOST="${RAWRXD_HOST:-0.0.0.0}"
    export RAWRXD_PORT="${RAWRXD_PORT:-23959}"

    if [[ -f "RawrEngine.py" ]]; then
        exec python3 RawrEngine.py
    elif [[ -f "backend/rawr_engine.py" ]]; then
        exec python3 backend/rawr_engine.py
    elif [[ -f "Ship/chat_server.py" ]]; then
        exec python3 Ship/chat_server.py
    else
        error "RawrEngine backend entrypoint not found."
        exit 1
    fi
}

launch_ide() {
    if [[ ! -f "$IDE_PATH" ]]; then
        error "IDE executable not found at: $IDE_PATH"
        exit 1
    fi

    log "Launching RawrXD IDE via Wine ..."
    export WINEPREFIX="$WINE_PREFIX"
    export WINEARCH=win64
    export WINEDEBUG=-all
    export __GL_THREADED_OPTIMIZATIONS=1
    export MESA_GLTHREAD=true

    wine "$IDE_PATH" "$@" 2>&1 | tee "$SCRIPT_DIR/rawrxd_wine.log"
}

main() {
    log "RawrXD Universal Access - Linux Wrapper v1.0"

    local ide_args=()
    while [[ $# -gt 0 ]]; do
        case "$1" in
            --backend-only)
                BACKEND_ONLY=1
                shift
                ;;
            --help|-h)
                echo "Usage: $0 [options] [-- <ide args>]"
                echo "  --backend-only  Run only the HTTP backend (no GUI)"
                echo "  --help          Show this help"
                exit 0
                ;;
            --)
                shift
                ide_args+=("$@")
                break
                ;;
            *)
                ide_args+=("$1")
                shift
                ;;
        esac
    done

    check_dependencies

    if [[ "$BACKEND_ONLY" == "1" ]]; then
        launch_backend
    else
        setup_wine_prefix
        launch_ide "${ide_args[@]}"
    fi
}

main "$@"
