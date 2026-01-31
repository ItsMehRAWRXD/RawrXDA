#!/bin/bash
# BlueStacks Integration Script

echo "Starting BlueStacks integration..."

# Check if BlueStacks is installed
if [ -d "/c/Program Files/BlueStacks_nxt" ]; then
    BLUESTACKS_PATH="/c/Program Files/BlueStacks_nxt"
elif [ -d "/c/Program Files (x86)/BlueStacks_nxt" ]; then
    BLUESTACKS_PATH="/c/Program Files (x86)/BlueStacks_nxt"
else
    echo "BlueStacks not found. Please install BlueStacks first."
    exit 1
fi

echo "BlueStacks found at: $BLUESTACKS_PATH"

# Start BlueStacks
echo "Starting BlueStacks..."
"$BLUESTACKS_PATH/HD-Player.exe" &

# Wait for BlueStacks to start
echo "Waiting for BlueStacks to start..."
sleep 30

# Connect to BlueStacks via ADB
echo "Connecting to BlueStacks via ADB..."
adb connect 127.0.0.1:5555

# Check connection
echo "Checking ADB connection..."
adb devices

echo "BlueStacks integration complete!"
echo "You can now develop Android apps with BlueStacks as the emulator."
