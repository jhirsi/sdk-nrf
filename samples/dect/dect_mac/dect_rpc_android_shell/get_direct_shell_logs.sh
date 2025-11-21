#!/bin/bash
# Script to capture Direct Shell connection debug logs

echo "=== Direct Shell Connection Debug Logs ==="
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
echo "=== Starting log capture ==="
echo "Now:"
echo "1. Open the DECT RPC Android Shell app"
echo "2. Go to device list"
echo "3. Select a device"
echo "4. Choose 'Direct Shell'"
echo "5. Watch for connection logs"
echo "6. Press Ctrl+C to stop"
echo ""

# Capture with verbose logging
$ADB_CMD logcat -s DirectShellClient:D DirectShellActivity:D | tee direct_shell_connection_logs.txt
