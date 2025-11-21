#!/bin/bash
# Script to capture Android logs for DECT RPC Android Shell debugging

echo "=== Android Log Capture Script ==="
echo ""

# Check if adb is available (try WSL adb first, then Windows ADB)
ADB_CMD=""
if command -v adb &> /dev/null; then
    ADB_CMD="adb"
elif [ -f "/mnt/c/Users/jahi/AppData/Local/Android/Sdk/platform-tools/adb.exe" ]; then
    ADB_CMD="/mnt/c/Users/jahi/AppData/Local/Android/Sdk/platform-tools/adb.exe"
    echo "Using Windows ADB from: $ADB_CMD"
elif [ -f "/mnt/c/Users/$USER/AppData/Local/Android/Sdk/platform-tools/adb.exe" ]; then
    ADB_CMD="/mnt/c/Users/$USER/AppData/Local/Android/Sdk/platform-tools/adb.exe"
    echo "Using Windows ADB from: $ADB_CMD"
else
    echo "ERROR: adb not found!"
    echo ""
    echo "Options:"
    echo "1. Install ADB in WSL: sudo apt-get install android-tools-adb"
    echo "2. Use Windows ADB: Create alias in ~/.bashrc:"
    echo "   alias adb='/mnt/c/Users/<your-user>/AppData/Local/Android/Sdk/platform-tools/adb.exe'"
    echo ""
    exit 1
fi

# Check if device is connected
echo "Checking for connected devices..."
DEVICES=$($ADB_CMD devices | grep -v "List" | grep "device$" | wc -l)

if [ "$DEVICES" -eq 0 ]; then
    echo "ERROR: No Android device found!"
    echo ""
    echo "Troubleshooting:"
    echo "1. Connect your Android device via USB"
    echo "2. Enable USB debugging on your device"
    echo "3. If using WSL with Windows ADB, try:"
    echo "   $ADB_CMD kill-server"
    echo "   $ADB_CMD start-server"
    echo "   $ADB_CMD devices"
    echo ""
    echo "Current devices:"
    $ADB_CMD devices
    exit 1
fi

echo "Found $DEVICES device(s)"
$ADB_CMD devices
echo ""

# Ask user what to do
echo "Choose an option:"
echo "1. View logs in real-time (filtered for CloudRestService)"
echo "2. Save logs to file (device_status_logs.txt)"
echo "3. Clear logs and start fresh capture"
read -p "Enter choice (1-3): " choice

case $choice in
    1)
        echo ""
        echo "=== Viewing logs in real-time ==="
        echo "Press Ctrl+C to stop"
        echo ""
        $ADB_CMD logcat -s CloudRestService:D
        ;;
    2)
        echo ""
        echo "=== Saving logs to device_status_logs.txt ==="
        echo "Press Ctrl+C to stop capturing"
        echo ""
        $ADB_CMD logcat -s CloudRestService:D | tee device_status_logs.txt
        ;;
    3)
        echo ""
        echo "=== Clearing old logs ==="
        $ADB_CMD logcat -c
        echo "Logs cleared!"
        echo ""
        echo "Now start the app and use it, then run this script again with option 1 or 2"
        ;;
    *)
        echo "Invalid choice"
        exit 1
        ;;
esac
