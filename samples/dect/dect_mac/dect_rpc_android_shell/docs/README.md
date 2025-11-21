# DECT RPC Android Shell Application

A terminal/shell application for Android that provides command-line access to DECT NR+ devices via nRF Cloud MQTT. The application supports both DECT RPC protocol (binary) and direct DECT shell commands (JSON format).

## Overview

This Android application provides a user-friendly interface for remotely controlling and monitoring DECT NR+ devices. It connects to nRF Cloud, authenticates with an API key, lists all available devices, and allows you to interact with devices using either:

- **RPC Shell**: Structured DECT RPC protocol with binary packets (best for programmatic control)
- **Direct Shell**: Direct dect_shell commands via JSON format (best for interactive shell usage)

## Features

- **nRF Cloud Authentication**: Secure login with API key
- **Device List**: Browse all your nRF Cloud devices with online/offline status
- **Dual Shell Modes**: Choose between RPC Shell and Direct Shell for each device
- **RPC Shell**: Full support for DECT RPC operations with structured commands
- **Direct Shell**: Direct access to dect_shell commands via JSON
- **Real-time Events**: Receive and display DECT events
- **Auto-connect**: Automatically connects to selected device
- **Command History**: Navigate through command history
- **Offline Support**: View device list even when devices are offline

## Application Flow

1. **Login Screen**: Authenticate with nRF Cloud API key
2. **Device List**: Browse all devices in your nRF Cloud account
3. **Shell Selection**: Choose RPC Shell or Direct Shell when selecting a device
4. **Terminal Interface**: Execute commands and view responses

See [APP_FLOW.md](APP_FLOW.md) for detailed flow documentation.

## Architecture

```
┌─────────────────────────────────────────┐
│     Android Application (C#/.NET)        │
│  ┌───────────────────────────────────┐  │
│  │   LoginActivity                   │  │
│  │   - API Key Input                 │  │
│  │   - Authentication                │  │
│  └──────────────┬────────────────────┘  │
│                 │                        │
│  ┌──────────────▼────────────────────┐  │
│  │   DeviceListActivity              │  │
│  │   - Device List (RecyclerView)    │  │
│  │   - Shell Selection Dialog        │  │
│  └──────────────┬────────────────────┘  │
│                 │                        │
│      ┌──────────┴──────────┐            │
│      │                      │            │
│  ┌───▼──────┐      ┌───────▼──────┐    │
│  │ RPC Shell│      │ Direct Shell  │    │
│  │ Terminal │      │ Terminal      │    │
│  └────┬─────┘      └───────┬───────┘    │
│       │                     │            │
│  ┌────▼─────────────────────▼───────┐  │
│  │   DECT RPC Client / Direct Shell  │  │
│  │   Client (MQTT)                    │  │
│  └──────────────┬────────────────────┘  │
│                 │                        │
│  ┌──────────────▼────────────────────┐  │
│  │   nRF Cloud MQTT                  │  │
│  └───────────────────────────────────┘  │
└─────────────────────────────────────────┘
```

## Project Structure

```
dect_rpc_android_shell/
├── README.md
├── BUILD_INSTRUCTIONS.md
├── USAGE.md
├── APP_FLOW.md
├── LIBRARY_INTEGRATION.md
├── AndroidManifest.xml
├── DectRpcAndroidShell.csproj
├── LoginActivity.cs              # Login screen
├── DeviceListActivity.cs        # Device list with selection
├── TerminalActivity.cs          # RPC Shell terminal
├── DirectShellActivity.cs       # Direct Shell terminal
├── CloudAuthService.cs          # nRF Cloud authentication
├── CloudRestService.cs          # nRF Cloud REST API
├── DectRpcShell.cs              # RPC command parser
├── DirectShellClient.cs         # Direct shell MQTT client
└── Resources/
    ├── layout/
    └── values/
```

## Shell Types

### RPC Shell (TerminalActivity)

Uses the DECT RPC protocol with binary packets over MQTT.

**Supported Commands**:
- `activate [iface_index]` - Activate DECT interface
- `deactivate [iface_index]` - Deactivate DECT interface
- `status [iface_index]` - Get DECT status (structured data)
- `settings read [iface_index]` - Read DECT settings
- `settings write [iface_index]` - Write DECT settings
- `events subscribe [iface_index] [mask]` - Subscribe to events
- `disconnect` - Disconnect from device
- `help` - Show help
- `exit` - Exit application

**Best for**: Programmatic control, structured data access, automated scripts

### Direct Shell (DirectShellActivity)

Uses direct dect_shell commands sent as JSON: `{"appId":"DECT_SHELL", "data":"<command>"}`

**Supported Commands**: All dect_shell commands, for example:
- `version` - Show version information
- `dect status 0` - Get DECT status
- `dect activate 0` - Activate DECT interface
- `dect settings read 0` - Read DECT settings
- `dect settings write 0 <file>` - Write DECT settings
- Any other dect_shell command

**Best for**: Interactive shell usage, testing, debugging, manual operations

## Getting Started

### Prerequisites

1. **Android Studio** with .NET support (see [BUILD_INSTRUCTIONS.md](BUILD_INSTRUCTIONS.md))
2. **.NET SDK 10.0 or later** (required - project targets `net10.0-android` for Android 16)
3. **Android SDK** (API level 21+)
4. **nRF Cloud Account** with API key
5. **DECT NR+ Device** connected to nRF Cloud

### Quick Start

1. **Build the application** (see [BUILD_INSTRUCTIONS.md](BUILD_INSTRUCTIONS.md))
2. **Install on Android device**
3. **Launch application**
4. **Enter nRF Cloud API key** on login screen
5. **Select a device** from the device list
6. **Choose shell type** (RPC Shell or Direct Shell)
7. **Start using commands**

See [USAGE.md](USAGE.md) for detailed usage instructions.

## Library Integration

**Important**: Before building, you need to integrate the `DectNrpRpcNetCloudLib` library.

See [LIBRARY_INTEGRATION.md](LIBRARY_INTEGRATION.md) for detailed instructions on how to:
- Add the library as source files (recommended)
- Add the library as a project reference
- Create and use a NuGet package

**Quick setup** (copy library files):
```bash
cd /path/to/dect_rpc_android_shell
mkdir -p DectNrpRpcNetCloudLib
cp ../../../../subsys/net/lib/dect_nrp_rpc_net_cloud_lib/*.cs DectNrpRpcNetCloudLib/
```

Then add NuGet packages: `MQTTnet` (>= 4.3.3.952), `PeterO.Cbor` (>= 4.5.1), `System.Text.Json` (>= 9.0.0), and `Xamarin.AndroidX.RecyclerView` (>= 1.3.0.1)

## Building

See [BUILD_INSTRUCTIONS.md](BUILD_INSTRUCTIONS.md) for detailed build instructions, including:
- Android Studio setup
- Library integration
- NuGet package management
- Deployment to device/emulator

## Usage

See [USAGE.md](USAGE.md) for detailed usage instructions, including:
- Application flow
- Command reference
- Examples
- Troubleshooting

## Documentation

- **[APP_FLOW.md](APP_FLOW.md)**: Detailed application flow and architecture
- **[BUILD_INSTRUCTIONS.md](BUILD_INSTRUCTIONS.md)**: Build and setup instructions
- **[USAGE.md](USAGE.md)**: Usage guide and command reference
- **[LIBRARY_INTEGRATION.md](LIBRARY_INTEGRATION.md)**: Library integration guide

## Deployment

See [DEPLOYMENT.md](DEPLOYMENT.md) for detailed instructions on how to install and run the application on your Android device.

Quick start:
```bash
# Build and install via ADB
dotnet build -c Debug
# IMPORTANT: Use the -Signed.apk file (signed APK)
adb install -r bin/Debug/net10.0-android/com.nordicsemi.dect_rpc_shell-Signed.apk
```

## License

Copyright (c) 2025 Nordic Semiconductor ASA

SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
