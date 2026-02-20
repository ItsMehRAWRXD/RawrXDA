#!/bin/bash
# RawrXD macOS Launcher - Universal Binary wrapper
# Supports Intel (Wine) and Apple Silicon (Rosetta + Wine/CrossOver).

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
WINE_PREFIX="${WINE_PREFIX:-$SCRIPT_DIR/.wine_rawrxd}"
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
warn() { echo -e "${YELLOW}[Warning]${NC} $1"; }
info() { echo -e "${BLUE}[Info]${NC} $1"; }
error() { echo -e "${RED}[Error]${NC} $1"; }

check_rosetta() {
    if [[ "$ARCH" == "arm64" ]]; then
        if ! pgrep -q oahd >/dev/null 2>&1; then
            log "Rosetta 2 not detected. Installing ..."
            /usr/sbin/softwareupdate --install-rosetta --agree-to-license
        fi
        info "Apple Silicon detected - running through Rosetta-compatible Wine."
    fi
}

check_wine() {
    if [[ -d "/Applications/CrossOver.app" ]]; then
        WINE_BIN="/Applications/CrossOver.app/Contents/SharedSupport/CrossOver/bin/wine"
        info "Using CrossOver for better compatibility."
    elif command -v wine64 >/dev/null 2>&1; then
        WINE_BIN="wine64"
    elif command -v wine >/dev/null 2>&1; then
        WINE_BIN="wine"
    else
        error "Wine was not found."
        echo "Install options:"
        echo "  1. brew install --cask wine-stable"
        echo "  2. brew install --cask crossover (recommended)"
        exit 1
    fi
}

setup_environment() {
    export WINEPREFIX="$WINE_PREFIX"
    export WINEDEBUG=-all
    export WINEHIGHDPI=1
    export __GL_THREADED_OPTIMIZATIONS=1
    export DYLD_FALLBACK_LIBRARY_PATH="/usr/lib:/opt/X11/lib:${DYLD_FALLBACK_LIBRARY_PATH:-}"
}

create_app_bundle() {
    local app_bundle="$SCRIPT_DIR/RawrXD.app"
    if [[ -d "$app_bundle" ]]; then
        return
    fi

    log "Creating macOS app bundle ..."
    mkdir -p "$app_bundle/Contents/MacOS" "$app_bundle/Contents/Resources"

    cat > "$app_bundle/Contents/Info.plist" <<'EOF'
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

    cat > "$app_bundle/Contents/MacOS/rawrxd-launcher" <<EOF
#!/bin/bash
cd "$SCRIPT_DIR"
exec "$SCRIPT_DIR/launch-macos.sh" "\$@"
EOF
    chmod +x "$app_bundle/Contents/MacOS/rawrxd-launcher"

    if [[ -f "$SCRIPT_DIR/rawrxd.icns" ]]; then
        cp "$SCRIPT_DIR/rawrxd.icns" "$app_bundle/Contents/Resources/"
    fi

    log "App bundle created at $app_bundle"
}

backend_only() {
    log "Starting RawrEngine backend (macOS native Python) ..."
    cd "$SCRIPT_DIR/.."

    if [[ -f "RawrEngine.py" ]]; then
        exec python3 RawrEngine.py
    elif [[ -f "Ship/chat_server.py" ]]; then
        exec python3 Ship/chat_server.py
    else
        error "Backend entrypoint not found."
        exit 1
    fi
}

launch() {
    local ide_args=("$@")
    if [[ ! -f "$IDE_PATH" ]]; then
        error "IDE executable not found at $IDE_PATH"
        exit 1
    fi

    log "Starting RawrXD on macOS ($ARCH) ..."
    create_app_bundle
    "$WINE_BIN" "$IDE_PATH" "${ide_args[@]}"
}

main() {
    if [[ "${1:-}" == "--backend-only" ]]; then
        backend_only
        exit 0
    fi

    check_rosetta
    check_wine
    setup_environment
    launch "$@"
}

main "$@"
