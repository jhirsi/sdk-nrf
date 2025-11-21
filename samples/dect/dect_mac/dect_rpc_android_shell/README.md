# DECT RPC Android Shell

Android application for controlling DECT NR+ devices via nRF Cloud MQTT using RPC (Remote Procedure Call) or direct shell commands.

## Project Status

✅ **Converted to Kotlin/Java (Gradle-based)** - Similar structure to `nRFCloud_remote` reference project

## Quick Start

### Build from WSL

```bash
cd /home/jani/ncs/nrf/samples/dect/dect_mac/dect_rpc_android_shell

# Build Debug APK
./build.sh Debug

# Deploy to connected device
./deploy.sh Debug

# Or both at once
./build-and-deploy.sh Debug
```

### Build from Windows

```powershell
cd C:\Users\jahi\ncs\nrf\samples\dect\dect_mac\dect_rpc_android_shell

# Build Debug APK
.\gradlew.bat assembleDebug

# Deploy to connected device
adb install -r app\build\outputs\apk\debug\app-debug.apk
```

### Build from Android Studio

1. Open project in Android Studio
2. Wait for Gradle sync to complete
3. Click **Run** or press `Shift+F10`

## Project Structure

```
dect_rpc_android_shell/
├── app/
│   ├── build.gradle.kts          # App-level build configuration
│   ├── src/main/
│   │   ├── java/com/nordicsemi/dect_rpc_shell/
│   │   │   ├── activities/       # Android Activities
│   │   │   ├── services/        # Service classes
│   │   │   ├── models/          # Data models
│   │   │   └── utils/           # Utility classes
│   │   ├── res/                 # Android resources
│   │   └── AndroidManifest.xml
├── build.gradle.kts              # Root build file
├── settings.gradle.kts           # Project settings
├── gradle.properties            # Gradle properties
├── local.properties             # Android SDK location
├── build.sh                     # WSL build script
├── deploy.sh                    # WSL deploy script
└── build-and-deploy.sh          # Combined script
```

## Features

- **nRF Cloud Authentication** - Login with API key
- **Device List** - Browse available nRF Cloud devices in a grid layout with detailed information:
  - Device name and online/offline status
  - Connection protocol (MQTT, CoAP, etc.)
  - Connection status
  - Firmware version
  - Battery voltage
  - Last seen timestamp
- **RPC Shell** - Send RPC commands to DECT devices via nRF RPC protocol
- **Direct Shell** - Terminal-style interface for sending direct DECT shell commands:
  - Terminal/console look and feel with black background
  - Shell prompt "desh:~$" (green text)
  - Monospace font for authentic terminal experience
  - JSON formatting hidden from user - just type commands like "dect status"
  - Automatic JSON parsing of responses

## Requirements

- **Android**: API Level 21 (Android 5.0) or higher
- **Target SDK**: API Level 36 (Android 16)
- **Kotlin**: 1.9.20+
- **Gradle**: 8.2.0+

## Dependencies

- **AndroidX**: Core, AppCompat, RecyclerView, Material
- **MQTT**: Eclipse Paho MQTT Client
- **HTTP**: OkHttp for REST API
- **JSON**: Gson
- **CBOR**: co.nstant.in:cbor for DECT RPC encoding
- **Coroutines**: Kotlin Coroutines for async operations

## Building

### From WSL

See `docs/WSL_BUILD_DEPLOY.md` for complete instructions.

**Quick commands:**
```bash
./build.sh Debug          # Build Debug APK
./deploy.sh Debug         # Deploy to device
./build-and-deploy.sh Debug  # Build and deploy
```

### From Windows Command Line

```powershell
.\gradlew.bat assembleDebug
.\gradlew.bat assembleRelease
```

### From Android Studio

1. **File → Open** → Select project directory
2. Wait for Gradle sync
3. **Build → Make Project** (Ctrl+F9)
4. **Run → Run 'app'** (Shift+F10)

## Deployment

### Via ADB (Command Line)

```bash
# WSL
./deploy.sh Debug

# Windows
adb install -r app\build\outputs\apk\debug\app-debug.apk
```

### Via Android Studio

1. Connect device via USB
2. Enable USB debugging
3. Click **Run** button
4. Select your device

## Application Details

- **Application Name**: "Dect Shell" (displayed under icon)
- **Application Icon**: Blue background with "NR+" text
- **Minimum SDK**: API Level 21 (Android 5.0)
- **Target SDK**: API Level 36 (Android 16)

## User Interface

### Login Screen
- Enter nRF Cloud API key to authenticate
- API key is validated before proceeding

### Device List Screen
- Grid layout (2 columns) showing device cards
- Each card displays:
  - Device icon (first letter in colored circle - green if online, gray if offline)
  - Device name
  - Status line: Online/Offline • Protocol • Connection status • Last seen time
  - Info line: Firmware version • Battery voltage
- Click on a device to select shell type (RPC Shell or Direct Shell)
- MQTT connection status is checked before showing shell options
- Refresh button to reload device list
- Logout button to return to login screen

### RPC Shell Screen
- Traditional RPC command interface
- Send structured RPC commands to DECT devices

### Direct Shell Screen
- Terminal/console interface with black background
- Green shell prompt: `desh:~$`
- White monospace text on black background
- Type commands directly (e.g., `dect status`, `dect activate`)
- JSON formatting is hidden - commands are automatically wrapped in JSON format
- Responses are automatically parsed and displayed
- Enter key sends commands (no Send button needed)
- Commands available:
  - `help` - Show available commands
  - `connect <device_id> <api_key>` - Connect to device
  - `disconnect` - Disconnect from device
  - `<dect_command>` - Send any DECT shell command

## Documentation

- **`docs/WSL_BUILD_DEPLOY.md`** - Complete WSL build/deploy guide
- **`docs/KOTLIN_CONVERSION.md`** - Conversion details and examples
- **`docs/CONVERSION_SUMMARY.md`** - Conversion summary and status
- **`docs/PROJECT_STRUCTURE.md`** - Project organization

## License

Copyright (c) 2025 Nordic Semiconductor ASA

SPDX-License-Identifier: LicenseRef-Nordic-5-Clause


