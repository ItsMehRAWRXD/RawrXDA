#!/bin/bash
# RawrXD Linux Launcher — Wine-based Bootable Space
# Runs RawrXD-Win32IDE.exe without code changes

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
WINE_PREFIX="$SCRIPT_DIR/.wine_rawrxd"
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
    if [ "$BACKEND_ONLY" = "1" ]; then
        # Check Python backend dependencies only
        if ! command -v python3 &> /dev/null; then
            error "Python3 required for backend-only mode"
            exit 1
        fi
        return
    fi
    
    if ! command -v wine &> /dev/null; then
        error "Wine not installed. Install with:"
        echo "  Ubuntu/Debian: sudo apt install wine64"
        echo "  Fedora: sudo dnf install wine"
        echo "  Arch: sudo pacman -S wine"
        exit 1
    fi
    
    # Check Vulkan support for GPU acceleration
    if command -v vulkaninfo &> /dev/null; then
        log "Vulkan detected — GPU acceleration available"
    else
        warn "Vulkan not detected — falling back to CPU"
    fi
}

setup_wine_prefix() {
    if [ ! -d "$WINE_PREFIX" ]; then
        log "Initializing Wine prefix..."
        WINEARCH=win64 WINEPREFIX="$WINE_PREFIX" winecfg /v win10 &> /dev/null
        # Install dependencies via winetricks if available
        if command -v winetricks &> /dev/null; then
            log "Installing VC++ runtime..."
            WINEPREFIX="$WINE_PREFIX" winetricks -q vcrun2022 &> /dev/null || true
        fi
    fi
}

launch_backend() {
    log "Starting RawrEngine backend (Linux native)..."
    cd "$SCRIPT_DIR/.."
    
    # Use system Python to run RawrEngine directly
    export RAWRXD_BACKEND_ONLY=1
    export RAWRXD_HOST=0.0.0.0
    export RAWRXD_PORT=23959
    
    if [ -f "backend/rawr_engine.py" ]; then
        exec python3 backend/rawr_engine.py
    elif [ -f "RawrEngine.py" ]; then
        exec python3 RawrEngine.py
    elif [ -f "backend/rawrxd_backend.py" ]; then
        exec python3 backend/rawrxd_backend.py
    else
        error "RawrEngine not found. Expected backend/rawr_engine.py"
        exit 1
    fi
}

launch_ide() {
    if [ ! -f "$IDE_PATH" ]; then
        error "IDE not found at $IDE_PATH"
        exit 1
    fi
    
    log "Launching RawrXD IDE via Wine..."
    export WINEPREFIX="$WINE_PREFIX"
    export WINEARCH=win64
    
    # Optimize Wine for performance
    export WINEDEBUG=-all  # Disable debug output
    export __GL_THREADED_OPTIMIZATIONS=1
    export MESA_GLTHREAD=true
    
    # Launch IDE
    wine "$IDE_PATH" "$@" 2>&1 | tee "$SCRIPT_DIR/rawrxd_wine.log"
}

main() {
    log "RawrXD Universal Access — Linux Wrapper v1.0"
    
    # Parse args
    while [[ $# -gt 0 ]]; do
        case $1 in
            --backend-only)
                BACKEND_ONLY=1
                shift
                ;;
            --help|-h)
                echo "Usage: $0 [options]"
                echo "  --backend-only  Run only the HTTP backend (no GUI)"
                echo "  --help          Show this help"
                exit 0
                ;;
            *)
                break
                ;;
        esac
    done
    
    check_dependencies
    
    if [ "$BACKEND_ONLY" = "1" ]; then
        launch_backend
    else
        setup_wine_prefix
        launch_ide "$@"
    fi
}

main "$@"
