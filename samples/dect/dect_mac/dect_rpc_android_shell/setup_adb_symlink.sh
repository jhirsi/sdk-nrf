#!/bin/bash
# Script to create a symbolic link for ADB pointing to Windows ADB

echo "=== Setting up ADB Symbolic Link ==="
echo ""

# Windows ADB path
WINDOWS_ADB="/mnt/c/Users/jahi/AppData/Local/Android/Sdk/platform-tools/adb.exe"

# Check if Windows ADB exists
if [ ! -f "$WINDOWS_ADB" ]; then
    echo "ERROR: Windows ADB not found at: $WINDOWS_ADB"
    echo ""
    echo "Please check the path and update this script if needed."
    exit 1
fi

echo "Found Windows ADB at: $WINDOWS_ADB"
echo ""

# Target location for symlink (in /usr/local/bin which should be in PATH)
SYMLINK_TARGET="/usr/local/bin/adb"

# Check if symlink already exists
if [ -L "$SYMLINK_TARGET" ]; then
    echo "Symbolic link already exists: $SYMLINK_TARGET"
    echo "Current target: $(readlink $SYMLINK_TARGET)"
    read -p "Do you want to replace it? (y/n): " replace
    if [ "$replace" != "y" ] && [ "$replace" != "Y" ]; then
        echo "Aborted."
        exit 0
    fi
    sudo rm "$SYMLINK_TARGET"
fi

# Check if file exists (not symlink)
if [ -f "$SYMLINK_TARGET" ]; then
    echo "WARNING: $SYMLINK_TARGET exists as a regular file (not a symlink)"
    read -p "Do you want to replace it? (y/n): " replace
    if [ "$replace" != "y" ] && [ "$replace" != "Y" ]; then
        echo "Aborted."
        exit 0
    fi
    sudo rm "$SYMLINK_TARGET"
fi

# Create the symbolic link
echo "Creating symbolic link..."
echo "  $SYMLINK_TARGET -> $WINDOWS_ADB"
sudo ln -s "$WINDOWS_ADB" "$SYMLINK_TARGET"

if [ $? -eq 0 ]; then
    echo ""
    echo "✓ Symbolic link created successfully!"
    echo ""
    echo "Verifying..."
    if command -v adb &> /dev/null; then
        echo "✓ 'adb' command is now available"
        echo ""
        echo "ADB version:"
        adb version
        echo ""
        echo "You can now use 'adb' directly from WSL!"
        echo ""
        echo "Test it with:"
        echo "  adb devices"
        echo "  adb logcat"
    else
        echo "⚠ 'adb' command not found in PATH"
        echo "Make sure /usr/local/bin is in your PATH"
        echo "Add to ~/.bashrc: export PATH=\$PATH:/usr/local/bin"
    fi
else
    echo "ERROR: Failed to create symbolic link"
    exit 1
fi
