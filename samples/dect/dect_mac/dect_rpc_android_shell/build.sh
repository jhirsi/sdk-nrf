#!/bin/bash
# Build script for DECT RPC Android Shell from WSL
# Uses Windows Gradle from WSL

set -e

# Get the script directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

# Use local gradlew if available, otherwise use Windows gradlew from reference project
if [ -f "./gradlew" ]; then
    GRADLEW="./gradlew"
elif [ -f "./gradlew.bat" ]; then
    GRADLEW="./gradlew.bat"
else
    # Fallback to reference project's gradlew
    GRADLEW="/mnt/c/Users/jahi/code_wa/StudioProjects/nRFCloud_remote/gradlew.bat"
    if [ ! -f "$GRADLEW" ]; then
        echo "Error: gradlew not found. Please run 'gradle wrapper' to create gradlew"
        exit 1
    fi
fi

# Check if clean option is requested
if [ "$1" = "clean" ]; then
    echo "=========================================="
    echo "Cleaning DECT RPC Android Shell"
    echo "=========================================="
    
    # Run Gradle clean
    echo "Running Gradle clean..."
    "$GRADLEW" clean || true
    
    # Remove build directories
    echo "Removing build directories..."
    rm -rf app/build
    rm -rf dect-rpc-lib/build
    rm -rf build
    rm -rf .gradle
    rm -rf .idea
    rm -rf bin
    rm -rf obj
    
    # Remove generated files
    echo "Removing generated files..."
    find . -type f -name "*.apk" -delete
    find . -type f -name "*.aab" -delete
    find . -type f -name "*.dex" -delete
    find . -type f -name "*.class" -delete
    find . -type f -name "*.log" -delete
    find . -type f -name "*.tmp" -delete
    find . -type f -name "*.temp" -delete
    find . -type f -name "*.bak" -delete
    find . -type f -name "*.swp" -delete
    find . -type f -name "*.swo" -delete
    find . -type f -name "*~" -delete
    
    # Remove Android Studio cache
    echo "Removing Android Studio cache..."
    rm -rf .idea/caches
    rm -rf .idea/libraries
    rm -rf .idea/modules.xml
    rm -rf .idea/workspace.xml
    rm -rf .idea/tasks.xml
    rm -rf .idea/gradle.xml
    rm -rf .idea/*.iml
    find . -type f -name "*.iml" -delete
    
    # Remove local.properties if it exists (contains local paths)
    if [ -f "local.properties" ]; then
        echo "Note: local.properties exists but is not removed (contains local SDK paths)"
    fi
    
    echo ""
    echo "=========================================="
    echo "Clean completed successfully!"
    echo "=========================================="
    exit 0
fi

# Configuration (Debug or Release)
CONFIG="${1:-Debug}"

echo "=========================================="
echo "Building DECT RPC Android Shell"
echo "Configuration: $CONFIG"
echo "=========================================="

# First, create gradlew if it doesn't exist
if [ ! -f "./gradlew" ]; then
    echo "Creating gradlew wrapper..."
    # Copy from reference project or create new
    if [ -f "$GRADLEW" ]; then
        cp "$GRADLEW" ./gradlew.bat 2>/dev/null || true
    fi
    # We'll use the Windows gradlew.bat from WSL
fi

# Clean previous build
echo "Cleaning previous build..."
"$GRADLEW" clean || true

# Build
echo "Building..."
if [ "$CONFIG" = "Release" ]; then
    "$GRADLEW" assembleRelease
    APK_PATH="app/build/outputs/apk/release/app-release.apk"
else
    "$GRADLEW" assembleDebug
    APK_PATH="app/build/outputs/apk/debug/app-debug.apk"
fi

# Show APK location
if [ -f "$APK_PATH" ]; then
    echo ""
    echo "=========================================="
    echo "Build successful!"
    echo "APK location: $APK_PATH"
    echo "=========================================="
    # Convert to Windows path for easier copying
    WIN_APK_PATH=$(echo "$APK_PATH" | sed 's|/mnt/c/|C:/|' | sed 's|/|\\|g')
    echo "Windows path: $WIN_APK_PATH"
else
    echo ""
    echo "=========================================="
    echo "Build completed, but APK not found at:"
    echo "$APK_PATH"
    echo "=========================================="
    exit 1
fi
