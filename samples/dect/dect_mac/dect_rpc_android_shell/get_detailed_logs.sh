#!/bin/bash
# Script to capture detailed Android logs including full JSON structures

echo "=== Detailed Android Log Capture ==="
echo ""

# Check if adb is available
ADB_CMD=""
if command -v adb &> /dev/null; then
    ADB_CMD="adb"
elif [ -f "/mnt/c/Users/jahi/AppData/Local/Android/Sdk/platform-tools/adb.exe" ]; then
    ADB_CMD="/mnt/c/Users/jahi/AppData/Local/Android/Sdk/platform-tools/adb.exe"
elif [ -f "/mnt/c/Users/$USER/AppData/Local/Android/Sdk/platform-tools/adb.exe" ]; then
    ADB_CMD="/mnt/c/Users/$USER/AppData/Local/Android/Sdk/platform-tools/adb.exe"
else
    echo "ERROR: adb not found!"
    exit 1
fi

# Check if device is connected
DEVICES=$($ADB_CMD devices | grep -v "List" | grep "device$" | wc -l)
if [ "$DEVICES" -eq 0 ]; then
    echo "ERROR: No Android device found!"
    $ADB_CMD devices
    exit 1
fi

echo "Found $DEVICES device(s)"
echo ""

# Clear old logs
echo "Clearing old logs..."
$ADB_CMD logcat -c

echo ""
echo "=== Starting detailed log capture ==="
echo "This will capture ALL CloudRestService logs including full JSON"
echo "Press Ctrl+C to stop"
echo ""
echo "Now:"
echo "1. Open the DECT RPC Android Shell app"
echo "2. Login with your API key"
echo "3. Wait for devices to load"
echo "4. Press Ctrl+C to stop capturing"
echo ""

# Capture with verbose logging (no filter level, show all Debug logs)
$ADB_CMD logcat -s CloudRestService:* | tee device_status_logs_detailed.txt
