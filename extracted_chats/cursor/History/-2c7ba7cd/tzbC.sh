#!/usr/bin/env bash
# ULTRA Cursor Trial Reset Script (Unix/macOS/Linux)
# Completely resets Cursor trial period and clears all tracking data

set -e

echo "🚀 ULTRA Cursor Trial Reset Starting..."
echo "⚠️  This will completely reset your Cursor trial period"
echo ""

# Kill all Cursor processes
echo "🔪 Terminating Cursor processes..."
killall Cursor 2>/dev/null || true
killall "Cursor Helper" 2>/dev/null || true
killall "Cursor Helper (Renderer)" 2>/dev/null || true
killall "Cursor Helper (GPU)" 2>/dev/null || true
killall "Cursor Helper (Plugin)" 2>/dev/null || true

# Wait for processes to fully terminate
sleep 2

# ULTRA storage locations to clear
STORAGE_LOCATIONS=(
    "$HOME/.config/Cursor"
    "$HOME/.cache/Cursor"
    "$HOME/.local/share/Cursor"
    "$HOME/Library/Application Support/Cursor"
    "$HOME/Library/Caches/Cursor"
    "$HOME/Library/Preferences/com.cursor.Cursor.plist"
    "$HOME/Library/Logs/Cursor"
    "$HOME/Library/Saved Application State/com.cursor.Cursor.savedState"
)

# Clear all storage locations
echo "🗑️  Clearing Cursor data..."
for location in "${STORAGE_LOCATIONS[@]}"; do
    if [ -e "$location" ]; then
        echo "   Clearing: $location"
        rm -rf "$location" 2>/dev/null || true
    fi
done

# Create new storage with ULTRA IDs
echo "🆔 Generating new machine IDs..."
mkdir -p "$HOME/.config/Cursor/User/globalStorage"
cat > "$HOME/.config/Cursor/User/globalStorage/storage.json" <<EOF
{
    "telemetry.machineId": "$(uuidgen)",
    "telemetry.devDeviceId": "$(uuidgen)",
    "telemetry.macMachineId": "$(uuidgen | tr -d -)",
    "telemetry.sessionId": "$(uuidgen)",
    "telemetry.installId": "$(uuidgen)",
    "telemetry.userId": "$(uuidgen)",
    "telemetry.anonymousId": "$(uuidgen)",
    "telemetry.rotated": true,
    "telemetry.resetCount": $(($(cat "$HOME/.config/Cursor/User/globalStorage/storage.json" 2>/dev/null | grep -o '"resetCount":[0-9]*' | grep -o '[0-9]*' || echo 0) + 1)),
    "telemetry.lastReset": "$(date -u +%Y-%m-%dT%H:%M:%SZ)",
    "telemetry.ultraReset": true,
    "telemetry.version": "2.0.0"
}
EOF

# Clear keychain entries (macOS)
if command -v security >/dev/null 2>&1; then
    echo "🔐 Clearing keychain entries..."
    security delete-generic-password -a "Cursor" -s "Cursor" 2>/dev/null || true
    security delete-generic-password -a "cursor" -s "cursor" 2>/dev/null || true
    security delete-generic-password -a "com.cursor.Cursor" 2>/dev/null || true
fi

# Clear browser data
echo "🌐 Clearing browser data..."
rm -rf "$HOME/Library/Application Support/Cursor" 2>/dev/null || true
rm -rf "$HOME/.config/Cursor/User/workspaceStorage" 2>/dev/null || true

# Clear logs
echo "📝 Clearing logs..."
rm -rf "$HOME/.config/Cursor/logs" 2>/dev/null || true
rm -rf "$HOME/Library/Logs/Cursor" 2>/dev/null || true

# Clear cache
echo "💾 Clearing cache..."
rm -rf "$HOME/.cache/Cursor" 2>/dev/null || true
rm -rf "$HOME/Library/Caches/Cursor" 2>/dev/null || true

echo ""
echo "✅ ULTRA Trial Reset Complete!"
echo "🎯 Fresh trial period activated"
echo "🔄 Restart Cursor for maximum effect"
echo ""
echo "📊 Reset Summary:"
echo "   • Machine ID: $(cat "$HOME/.config/Cursor/User/globalStorage/storage.json" | grep -o '"machineId":"[^"]*"' | cut -d'"' -f4 | cut -c1-16)..."
echo "   • Reset Count: $(cat "$HOME/.config/Cursor/User/globalStorage/storage.json" | grep -o '"resetCount":[0-9]*' | grep -o '[0-9]*')"
echo "   • Timestamp: $(date)"
