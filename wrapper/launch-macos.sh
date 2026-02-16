#!/bin/bash
# RawrXD macOS Launcher — Universal Binary wrapper
# Supports both Intel (Wine) and Apple Silicon (Rosetta + Wine)

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
WINE_PREFIX="$SCRIPT_DIR/.wine_rawrxd"
IDE_PATH="${IDE_PATH:-$SCRIPT_DIR/../RawrXD-Win32IDE.exe}"
ARCH="$(uname -m)"
WINE_BIN=""

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

log() { echo -e "${GREEN}[RawrXD]${NC} $1"; }
info() { echo -e "${BLUE}[Info]${NC} $1"; }
warn() { echo -e "${YELLOW}[Warning]${NC} $1"; }

check_rosetta() {
    if [ "$ARCH" = "arm64" ]; then
        if ! /usr/bin/pgrep -q "oahd"; then
            log "Installing Rosetta 2..."
            /usr/sbin/softwareupdate --install-rosetta --agree-to-license
        fi
        info "Apple Silicon detected — using Rosetta 2"
    fi
}

check_wine() {
    # Check for CrossOver (commercial, better performance)
    if [ -d "/Applications/CrossOver.app" ]; then
        WINE_BIN="/Applications/CrossOver.app/Contents/SharedSupport/CrossOver/bin/wine"
        info "Using CrossOver for better performance"
    elif command -v wine64 &> /dev/null; then
        WINE_BIN="wine64"
    elif command -v wine &> /dev/null; then
        WINE_BIN="wine"
    else
        echo -e "${RED}Wine not found.${NC}"
        echo "Install options:"
        echo "  1. brew install --cask wine-stable"
        echo "  2. brew install --cask crossover (recommended)"
        exit 1
    fi
}

setup_environment() {
    export WINEPREFIX="$WINE_PREFIX"
    export DYLD_FALLBACK_LIBRARY_PATH="/usr/lib:/opt/X11/lib:${DYLD_FALLBACK_LIBRARY_PATH:-}"

    # macOS-specific optimizations
    export WINEDEBUG=-all
    export __GL_THREADED_OPTIMIZATIONS=1

    # HiDPI support
    export WINEHIGHDPI=1
}

create_app_bundle() {
    local APP_BUNDLE="$SCRIPT_DIR/RawrXD.app"
    log "Creating macOS App Bundle..."
    mkdir -p "$APP_BUNDLE/Contents/"{MacOS,Resources}

    cat > "$APP_BUNDLE/Contents/Info.plist" << 'EOF'
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>CFBundleExecutable</key>
    <string>rawrxd-launcher</string>
    <key>CFBundleIdentifier</key>
    <string>com.rawrxd.ide</string>
    <key>CFBundleName</key>
    <string>RawrXD IDE</string>
    <key>CFBundlePackageType</key>
    <string>APPL</string>
    <key>CFBundleShortVersionString</key>
    <string>1.0</string>
    <key>LSMinimumSystemVersion</key>
    <string>10.14</string>
    <key>NSHighResolutionCapable</key>
    <true/>
</dict>
</plist>
EOF

    cat > "$APP_BUNDLE/Contents/MacOS/rawrxd-launcher" << EOF
#!/bin/bash
cd "$SCRIPT_DIR"
exec "$SCRIPT_DIR/launch-macos.sh" "\$@"
EOF
    chmod +x "$APP_BUNDLE/Contents/MacOS/rawrxd-launcher"

    # Icon (optional)
    if [ -f "$SCRIPT_DIR/rawrxd.icns" ]; then
        cp "$SCRIPT_DIR/rawrxd.icns" "$APP_BUNDLE/Contents/Resources/"
    fi

    log "App bundle created at $APP_BUNDLE"
}

launch() {
    if [ ! -f "$IDE_PATH" ]; then
        echo -e "${RED}IDE not found at $IDE_PATH${NC}"
        exit 1
    fi

    log "Starting RawrXD on macOS ($ARCH)..."

    local APP_BUNDLE="$SCRIPT_DIR/RawrXD.app"
    if [ ! -d "$APP_BUNDLE" ]; then
        create_app_bundle
    fi

    "$WINE_BIN" "$IDE_PATH" "$@"
}

backend_only() {
    log "Starting RawrEngine backend (macOS native)..."
    cd "$SCRIPT_DIR/.."

    export RAWRXD_HOST="${RAWRXD_HOST:-0.0.0.0}"
    export RAWRXD_PORT="${RAWRXD_PORT:-23959}"

    if [ -x "./RawrEngine" ]; then
        exec ./RawrEngine --port "$RAWRXD_PORT"
    fi

    if [ -f "RawrEngine.py" ]; then
        exec python3 RawrEngine.py
    elif [ -f "Ship/RawrEngine.py" ]; then
        exec python3 Ship/RawrEngine.py
    elif [ -f "Ship/chat_server.py" ]; then
        exec python3 Ship/chat_server.py --port "$RAWRXD_PORT"
    else
        echo -e "${RED}Backend not found${NC}"
        exit 1
    fi
}

if [ "${1:-}" = "--backend-only" ]; then
    backend_only
else
    check_rosetta
    check_wine
    setup_environment
    launch "$@"
fi

