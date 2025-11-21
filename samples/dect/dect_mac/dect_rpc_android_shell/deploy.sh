#!/bin/bash
# Deploy script for DECT RPC Android Shell from WSL
# Uses Windows ADB from WSL

set -e

# Get the script directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

# Auto-detect ADB - try multiple locations
ADB=""
if command -v adb &> /dev/null; then
    # ADB is in PATH (e.g., via symlink)
    ADB="adb"
elif [ -f "/usr/local/bin/adb" ]; then
    # ADB symlink exists
    ADB="/usr/local/bin/adb"
elif [ -f "/mnt/c/Users/jahi/AppData/Local/Android/Sdk/platform-tools/adb.exe" ]; then
    # Windows ADB in default location
    ADB="/mnt/c/Users/jahi/AppData/Local/Android/Sdk/platform-tools/adb.exe"
elif [ -f "$HOME/AppData/Local/Android/Sdk/platform-tools/adb.exe" ]; then
    # Windows ADB in user home
    ADB="$HOME/AppData/Local/Android/Sdk/platform-tools/adb.exe"
else
    echo "Error: ADB not found. Please:"
    echo "  1. Install Android SDK Platform Tools"
    echo "  2. Or set up ADB symlink (see ADB_SETUP.md)"
    echo "  3. Or set ADB path in this script"
    exit 1
fi

echo "Using ADB: $ADB"

# Configuration (Debug or Release)
CONFIG="${1:-Debug}"

if [ "$CONFIG" = "Release" ]; then
    APK_PATH="app/build/outputs/apk/release/app-release.apk"
else
    APK_PATH="app/build/outputs/apk/debug/app-debug.apk"
fi

echo "=========================================="
echo "Deploying DECT RPC Android Shell"
echo "Configuration: $CONFIG"
echo "=========================================="

# Check if APK exists
if [ ! -f "$APK_PATH" ]; then
    echo "Error: APK not found at: $APK_PATH"
    echo "Please build first using: ./build.sh $CONFIG"
    exit 1
fi

# Check if device is connected
echo "Checking for connected devices..."
DEVICE_OUTPUT=$("$ADB" devices 2>&1)
echo "$DEVICE_OUTPUT"
# Count devices (lines containing "device" or "unauthorized" but not "List")
# Convert Windows line endings and filter
DEVICES=$(echo "$DEVICE_OUTPUT" | tr -d '\r' | grep -v "^List" | grep -E "[[:space:]]device$|[[:space:]]unauthorized" | wc -l)
if [ "$DEVICES" -eq 0 ]; then
    echo "Error: No Android device connected"
    echo "Please:"
    echo "  1. Connect your phone via USB"
    echo "  2. Enable USB debugging"
    echo "  3. Run: $ADB devices"
    exit 1
fi

# Check for unauthorized devices
UNAUTHORIZED=$(echo "$DEVICE_OUTPUT" | tr -d '\r' | grep -c "unauthorized" || true)
if [ "$UNAUTHORIZED" -gt 0 ]; then
    echo "Warning: Unauthorized device(s) detected. Please authorize USB debugging on your device."
    exit 1
fi

echo "Found $DEVICES device(s)"

# Uninstall old version (ignore errors if not installed)
echo "Uninstalling old version (if installed)..."
"$ADB" uninstall com.nordicsemi.dect_rpc_shell 2>/dev/null || true

# Install new APK
echo "Installing APK..."
"$ADB" install -r "$APK_PATH"

if [ $? -eq 0 ]; then
    echo ""
    echo "=========================================="
    echo "Deployment successful!"
    echo "=========================================="
    echo "The app should now be available on your device."
    echo "Look for 'DECT NR+ Shell' in your app drawer."
else
    echo ""
    echo "=========================================="
    echo "Deployment failed!"
    echo "=========================================="
    exit 1
fi
