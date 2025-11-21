#!/bin/bash
# Script to set up ADB in WSL for Android debugging

echo "=== ADB Setup for WSL ==="
echo ""

# Check for ADB (WSL or Windows)
ADB_CMD=""
if command -v adb &> /dev/null; then
    ADB_CMD="adb"
    echo "✓ Found WSL ADB"
    $ADB_CMD version
    echo ""
elif [ -f "/mnt/c/Users/jahi/AppData/Local/Android/Sdk/platform-tools/adb.exe" ]; then
    ADB_CMD="/mnt/c/Users/jahi/AppData/Local/Android/Sdk/platform-tools/adb.exe"
    echo "✓ Found Windows ADB: $ADB_CMD"
    $ADB_CMD version
    echo ""
elif [ -f "/mnt/c/Users/$USER/AppData/Local/Android/Sdk/platform-tools/adb.exe" ]; then
    ADB_CMD="/mnt/c/Users/$USER/AppData/Local/Android/Sdk/platform-tools/adb.exe"
    echo "✓ Found Windows ADB: $ADB_CMD"
    $ADB_CMD version
    echo ""
else
    echo "ADB not found. Installing..."
    echo ""
    echo "Run these commands to install ADB:"
    echo "  sudo apt-get update"
    echo "  sudo apt-get install -y android-tools-adb android-tools-fastboot"
    echo ""
    read -p "Do you want to install ADB now? (y/n): " install_choice
    if [ "$install_choice" = "y" ] || [ "$install_choice" = "Y" ]; then
        sudo apt-get update
        sudo apt-get install -y android-tools-adb android-tools-fastboot
        ADB_CMD="adb"
    else
        echo "Please install ADB manually and run this script again"
        exit 1
    fi
fi

echo ""
echo "=== Setting up ADB connection ==="
echo ""

# Kill existing adb server
echo "Restarting ADB server..."
$ADB_CMD kill-server 2>/dev/null
sleep 1

# Start adb server
$ADB_CMD start-server

# Check for devices
echo ""
echo "Checking for connected devices..."
DEVICES=$($ADB_CMD devices | grep -v "List" | grep "device$" | wc -l)

if [ "$DEVICES" -eq 0 ]; then
    echo ""
    echo "⚠ No devices found!"
    echo ""
    echo "If your device is connected via USB on Windows, you have two options:"
    echo ""
    echo "Option 1: Use USB over IP (usbipd-win)"
    echo "  On Windows PowerShell (as Admin):"
    echo "    1. Install: winget install usbipd"
    echo "    2. List devices: usbipd list"
    echo "    3. Bind your Android device: usbipd bind -b <busid>"
    echo "  On WSL:"
    echo "    1. sudo modprobe usbip-core"
    echo "    2. sudo modprobe usbip-host"
    echo "    3. sudo usbip attach -r <windows-ip> -b <busid>"
    echo ""
    echo "Option 2: Use ADB over network"
    echo "  On Windows:"
    echo "    1. Connect device via USB"
    echo "    2. Run: adb tcpip 5555"
    echo "    3. Find device IP: adb shell ip addr"
    echo "  On WSL:"
    echo "    1. adb connect <device-ip>:5555"
    echo ""
    echo "Option 3: Use Windows ADB from WSL"
    echo "  Create alias in ~/.bashrc:"
    echo "    alias adb='/mnt/c/Users/<your-user>/AppData/Local/Android/Sdk/platform-tools/adb.exe'"
    echo ""
    read -p "Press Enter to continue..."
else
    echo "✓ Found $DEVICES device(s):"
    $ADB_CMD devices
    echo ""
    echo "Setup complete! You can now use ./get_logs.sh to capture logs"
fi
