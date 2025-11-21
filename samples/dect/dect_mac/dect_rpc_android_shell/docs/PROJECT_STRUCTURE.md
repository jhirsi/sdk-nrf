# Project Structure

This document describes the organization of the DECT RPC Android Shell project.

## Directory Structure

```
dect_rpc_android_shell/
├── src/                    # Source code organized by functionality
│   ├── Activities/         # Android Activity classes
│   │   ├── LoginActivity.cs
│   │   ├── DeviceListActivity.cs
│   │   ├── TerminalActivity.cs
│   │   └── DirectShellActivity.cs
│   ├── Services/          # Service classes
│   │   ├── CloudAuthService.cs
│   │   ├── CloudRestService.cs
│   │   └── DirectShellClient.cs
│   ├── Models/           # Data models (currently empty)
│   └── Utils/            # Utility classes
│       └── DectRpcShell.cs
├── Resources/            # Android resources (layouts, drawables, etc.)
├── docs/                # Documentation
├── AndroidManifest.xml  # Android application manifest
├── DectRpcAndroidShell.csproj  # .NET project file
└── DectRpcAndroidShell.sln     # Visual Studio solution file
```

## Source Code Organization

### Activities (`src/Activities/`)
Android Activity classes that represent the UI screens:
- **LoginActivity**: nRF Cloud API key authentication
- **DeviceListActivity**: Lists available nRF Cloud devices
- **TerminalActivity**: RPC-based shell interface
- **DirectShellActivity**: Direct DECT shell interface (JSON format)

### Services (`src/Services/`)
Service classes for external communication:
- **CloudAuthService**: nRF Cloud MQTT authentication
- **CloudRestService**: nRF Cloud REST API client
- **DirectShellClient**: Direct shell command client (JSON over MQTT)

### Utils (`src/Utils/`)
Utility classes:
- **DectRpcShell**: RPC shell command handler

### Models (`src/Models/`)
Data model classes (currently empty, can be used for device info, settings, etc.)

## Build System

This is a .NET Android project using:
- **Target Framework**: `net10.0-android`
- **Build System**: MSBuild / .NET SDK
- **Solution File**: `DectRpcAndroidShell.sln` (for Visual Studio / IDE support)

## Resources

Android resources are stored in the `Resources/` directory following standard Android resource organization:
- `Resources/values/`: Strings, styles, themes
- `Resources/drawable/`: Drawable resources
- `Resources/layout/`: Layout XML files (if any)

## Documentation

All documentation is in the `docs/` directory:
- `README.md`: Main project documentation
- `BUILD_INSTRUCTIONS.md`: How to build the project
- `DEPLOYMENT.md`: How to deploy to Android devices
- `APP_FLOW.md`: Application flow and user guide
- `LIBRARY_INTEGRATION.md`: How to integrate the DECT RPC library
- `PROJECT_STRUCTURE.md`: This file


