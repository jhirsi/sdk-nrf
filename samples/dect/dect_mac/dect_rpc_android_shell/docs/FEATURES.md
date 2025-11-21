# DECT RPC Android Shell - Features

This document describes all implemented features of the DECT RPC Android Shell application.

## Overview

The DECT RPC Android Shell is a Kotlin-based Android application that provides a user interface for controlling DECT NR+ devices via nRF Cloud MQTT. It supports both RPC (Remote Procedure Call) and direct shell command interfaces.

## Core Features

### 1. nRF Cloud Authentication

- **Login Screen**: Simple API key entry interface
- **API Key Validation**: Validates API key before proceeding to device list
- **Persistent Storage**: API key can be saved (optional, via SharedPreferences)
- **Error Handling**: Clear error messages for invalid credentials

### 2. Device List Management

#### Device Information Display
The device list shows comprehensive information similar to the `nRFCloud_remote` reference project:

- **Device Name**: Full device name from nRF Cloud
- **Online Status**: Visual indicator (green circle = online, gray = offline)
- **Connection Protocol**: MQTT, CoAP, etc. (from `state.reported.device.connectionInfo.protocol`)
- **Connection Status**: connected, disconnected, etc. (from `state.reported.connection.status`)
- **Firmware Information**:
  - App name and version (from `firmware.app.name` and `firmware.app.version`)
  - Modem version (from `firmware.modem`)
- **Battery Voltage**: Displayed in volts (from `state.reported.device.deviceInfo.batteryVoltage`, converted from mV)
- **Last Seen**: Time since device was last seen (just now, X min ago, X hours ago, X days ago)

#### UI Layout
- **Grid Layout**: 2-column grid layout for device cards
- **Card Design**: Each device is displayed in a card with:
  - Circular icon with device's first letter
  - Device name (bold)
  - Status line with connection info
  - Info line with firmware and battery
- **Alignment Fix**: Bottom padding and margin added to prevent buttons from being hidden
- **Refresh Functionality**: Manual refresh button to reload device list

#### Device Selection
- Click on any device card to open shell selection dialog
- MQTT connection status is checked before showing options
- Two shell types available:
  1. **RPC Shell**: Traditional RPC command interface
  2. **Direct Shell**: Terminal-style direct command interface

### 3. RPC Shell

- **Purpose**: Send structured RPC commands to DECT devices
- **Protocol**: Uses nRF RPC protocol over MQTT
- **Commands**: Standard DECT RPC commands (activate, deactivate, status, settings, etc.)

### 4. Direct Shell

#### Terminal Interface
- **Look and Feel**: Authentic terminal/console appearance
  - Black background throughout
  - Monospace font for all text
  - Green shell prompt: `desh:~$`
  - White text on black background
  - No Send button - uses Enter key only

#### Command Interface
- **User-Friendly**: JSON formatting is completely hidden from the user
- **Natural Commands**: Type commands as if using a real shell:
  ```
  desh:~$ dect status
  desh:~$ dect activate
  ```
- **Automatic JSON Wrapping**: Commands are automatically wrapped in JSON format (hidden from user):
  ```json
  {"appId":"DECT_SHELL", "data":"dect status"}
  ```
- **Response Parsing**: Responses are automatically parsed from JSON and displayed cleanly:
  - Automatically extracts content from JSON `{"data":"..."}` format
  - Handles nested JSON structures recursively
  - Falls back to plain text if response is not JSON
  - Never displays raw JSON structure to user
  - Skips empty responses

#### Available Commands
- `help` - Show available commands and examples
- `connect <device_id> <api_key>` - Connect to a device
- `disconnect` - Disconnect from current device
- `<dect_command>` - Any DECT shell command (e.g., `dect status`, `dect activate`)

#### Auto-Connect
- If device ID and API key are provided via intent, automatically attempts connection
- Shows connection status messages

## Technical Implementation

### Device Data Parsing

The `CloudRestService` parses the full nRF Cloud device response, extracting:

```kotlin
// From state.reported.device.connectionInfo
protocol: String?  // "MQTT", "CoAP", etc.
method: String?    // Connection method

// From state.reported.connection
connectionStatus: String?  // "connected", "disconnected", etc.

// From state.reported.device.deviceInfo
batteryVoltage: Double?  // Battery voltage in volts (converted from mV)

// From firmware
firmware: FirmwareInfo(
    appName: String?,
    appVersion: String?,
    modemVersion: String?
)
```

### UI Components

#### DeviceListActivity
- Uses `RecyclerView` with `GridLayoutManager` (2 columns)
- Custom `DeviceAdapter` and `DeviceViewHolder` for card layout
- MQTT connection check before shell selection
- AlertDialog for shell type selection

#### DirectShellActivity
- Custom terminal-style UI with black backgrounds
- `ScrollView` for output area
- `EditText` with monospace font for input
- Automatic JSON parsing in `onMessageReceived()`
- `writePrompt()` and `writeOutput()` helpers for consistent display

## Application Branding

- **Application Name**: "Dect Shell" (defined in `strings.xml`)
- **Application Icon**: Blue background (`#2196F3`) with "NR+" text (defined in `ic_launcher.xml`)
- **Activity Labels**:
  - Login: Uses app name
  - Device List: "Devices"
  - RPC Shell: "DECT RPC Shell"
  - Direct Shell: "DECT Shell"

## Build and Deployment

See `BUILD_INSTRUCTIONS.md` for complete build and deployment instructions.

## Future Enhancements

Potential improvements:
- Command history (up/down arrow keys)
- Tab completion for commands
- Syntax highlighting in terminal
- Device filtering and search
- Device grouping by type or status
- Push notifications for device events

