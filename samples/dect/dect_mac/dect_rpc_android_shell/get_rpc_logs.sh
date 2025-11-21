#!/bin/bash
# Script to capture RPC client debug logs

echo "=== DECT RPC Client Debug Logs ==="
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
    echo ""
    echo "Options:"
    echo "1. Install ADB in WSL: sudo apt-get install android-tools-adb"
    echo "2. Use Windows ADB: Create alias in ~/.bashrc"
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

# Ask user what to do
echo "Choose an option:"
echo "1. View logs in real-time (filtered for DectRpcClient)"
echo "2. Save logs to file (rpc_client_logs.txt)"
echo "3. Clear logs and start fresh capture"
read -p "Enter choice (1-3): " choice

case $choice in
    1)
        echo ""
        echo "=== Viewing RPC client logs in real-time ==="
        echo "Press Ctrl+C to stop"
        echo ""
        $ADB_CMD logcat -s DectRpcClient:D DectRpcShell:D TerminalActivity:D
        ;;
    2)
        echo ""
        echo "=== Saving logs to rpc_client_logs.txt ==="
        echo "Press Ctrl+C to stop capturing"
        echo ""
        $ADB_CMD logcat -s DectRpcClient:D DectRpcShell:D TerminalActivity:D | tee rpc_client_logs.txt
        ;;
    3)
        echo ""
        echo "=== Clearing old logs ==="
        $ADB_CMD logcat -c
        echo "Logs cleared!"
        echo ""
        echo "Now start the app, use RPC Shell, and run this script again with option 1 or 2"
        ;;
    *)
        echo "Invalid choice"
        exit 1
        ;;
esac

