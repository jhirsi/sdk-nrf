# Building and Deploying from WSL

Complete guide for building and deploying the DECT RPC Android Shell application from WSL (Windows Subsystem for Linux).

## Prerequisites

1. **Android SDK** installed on Windows (accessible from WSL)
2. **Gradle** - Either:
   - Local `gradlew` wrapper in project (recommended)
   - Or use reference project's `gradlew` at `/mnt/c/Users/jahi/code_wa/StudioProjects/nRFCloud_remote/gradlew.bat`
3. **ADB** - Android Debug Bridge (comes with Android SDK)
4. **USB Debugging** enabled on your Android device
5. **Device connected** via USB

## Quick Start

### Build Only
```bash
cd /home/jani/ncs/nrf/samples/dect/dect_mac/dect_rpc_android_shell
./build.sh Debug
```

### Deploy Only (after building)
```bash
./deploy.sh Debug
```

### Build and Deploy in One Step
```bash
./build-and-deploy.sh Debug
```

For Release builds, replace `Debug` with `Release`:
```bash
./build-and-deploy.sh Release
```

## Setup Gradle Wrapper

If `gradlew` doesn't exist, create it:

**Option 1: Copy from reference project**
```bash
cd /home/jani/ncs/nrf/samples/dect/dect_mac/dect_rpc_android_shell
cp /mnt/c/Users/jahi/code_wa/StudioProjects/nRFCloud_remote/gradlew ./
cp /mnt/c/Users/jahi/code_wa/StudioProjects/nRFCloud_remote/gradlew.bat ./
cp -r /mnt/c/Users/jahi/code_wa/StudioProjects/nRFCloud_remote/gradle ./
chmod +x gradlew
```

**Option 2: Generate new wrapper (if Gradle is installed)**
```bash
# Install Gradle in WSL if needed
sudo apt-get update
sudo apt-get install -y gradle

# Generate wrapper
gradle wrapper --gradle-version 8.2
```

## Scripts

### `build.sh`
Builds the Android APK using Gradle from WSL.

**Usage:**
```bash
./build.sh [Debug|Release]
```

**What it does:**
1. Cleans previous build artifacts
2. Builds the project using Gradle
3. Shows the APK location on success

**Output:**
- Debug APK: `app/build/outputs/apk/debug/app-debug.apk`
- Release APK: `app/build/outputs/apk/release/app-release.apk`

### `deploy.sh`
Deploys the built APK to a connected Android device.

**Usage:**
```bash
./deploy.sh [Debug|Release]
```

**What it does:**
1. Checks if APK exists
2. Verifies device connection
3. Uninstalls old version (if present)
4. Installs new APK

**Requirements:**
- APK must be built first
- Android device must be connected via USB
- USB debugging must be enabled

### `build-and-deploy.sh`
Combines build and deploy in one command.

**Usage:**
```bash
./build-and-deploy.sh [Debug|Release]
```

## Manual Commands

If you prefer to run commands manually:

### Build
```bash
# Using gradlew wrapper
./gradlew assembleDebug

# Or using Windows gradlew from reference project
/mnt/c/Users/jahi/code_wa/StudioProjects/nRFCloud_remote/gradlew.bat assembleDebug
```

### Deploy
```bash
# Windows ADB path
ADB="/mnt/c/Users/jahi/AppData/Local/Android/Sdk/platform-tools/adb.exe"

# Check device
"$ADB" devices

# Uninstall old version
"$ADB" uninstall com.nordicsemi.dect_rpc_shell

# Install new APK
"$ADB" install -r app/build/outputs/apk/debug/app-debug.apk
```

## Troubleshooting

### "gradlew: command not found"
The scripts use the local `gradlew` wrapper. If it doesn't exist:
1. Copy from reference project (see Setup Gradle Wrapper above)
2. Or generate a new wrapper with `gradle wrapper`

### "adb: command not found"
The scripts use Windows ADB from `/mnt/c/Users/jahi/AppData/Local/Android/Sdk/platform-tools/adb.exe`. If your Android SDK is in a different location, update the `ADB` variable in `deploy.sh`.

### "No Android device connected"
1. Connect your phone via USB
2. Enable USB debugging: **Settings → Developer Options → USB Debugging**
3. On your phone, allow USB debugging when prompted
4. Run: `"/mnt/c/Users/jahi/AppData/Local/Android/Sdk/platform-tools/adb.exe" devices`
5. You should see your device listed

### Build errors
If you encounter build errors:
1. Clean the build: `./gradlew clean`
2. Check that Android SDK is properly configured
3. Verify `local.properties` exists with `sdk.dir` set (or set `ANDROID_HOME`)

### "SDK location not found"
Create `local.properties` in the project root:
```properties
sdk.dir=C\:\\Users\\jahi\\AppData\\Local\\Android\\Sdk
```

Or set environment variable:
```bash
export ANDROID_HOME=/mnt/c/Users/jahi/AppData/Local/Android/Sdk
```

## Project Structure

The converted project follows standard Android Gradle structure:

```
dect_rpc_android_shell/
├── app/
│   ├── build.gradle.kts          # App-level build configuration
│   ├── src/
│   │   └── main/
│   │       ├── java/com/nordicsemi/dect_rpc_shell/
│   │       │   ├── activities/   # Activity classes
│   │       │   ├── services/     # Service classes
│   │       │   ├── models/       # Data models
│   │       │   └── utils/        # Utility classes
│   │       ├── res/              # Android resources
│   │       └── AndroidManifest.xml
│   └── proguard-rules.pro
├── build.gradle.kts              # Root build file
├── settings.gradle.kts            # Project settings
├── gradle.properties             # Gradle properties
├── gradlew                       # Gradle wrapper (Unix)
├── gradlew.bat                   # Gradle wrapper (Windows)
├── build.sh                      # Build script
├── deploy.sh                     # Deploy script
└── build-and-deploy.sh           # Combined script
```

## Workflow Example

```bash
# Navigate to project
cd /home/jani/ncs/nrf/samples/dect/dect_mac/dect_rpc_android_shell

# Build Debug version
./build.sh Debug

# Connect your phone via USB
# Enable USB debugging on phone

# Deploy
./deploy.sh Debug

# Or do both at once
./build-and-deploy.sh Debug
```

## Notes

- The scripts use Windows executables (`gradlew.bat`, `adb.exe`) from WSL when needed
- Paths are automatically converted between WSL and Windows formats
- The APK is signed automatically by the build process (debug signing for Debug builds)
- For Release builds, you may need to configure signing in `app/build.gradle.kts`

## Next Steps

1. **Complete DECT RPC Library Conversion**: The `DectRpcShell.kt` currently has placeholder implementations. You need to:
   - Convert the C# DECT RPC library to Kotlin/Java, OR
   - Create a JNI wrapper for the C# library, OR
   - Implement the DECT RPC protocol directly in Kotlin

2. **Test Build**: Run `./build.sh Debug` to verify the project builds

3. **Fix Compilation Errors**: Address any missing dependencies or API mismatches

4. **Test on Device**: Deploy and test the app functionality
