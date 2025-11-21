# Usage Guide for DECT RPC Android Shell

Complete guide for using the Android application to control DECT NR+ devices via nRF Cloud.

## Getting Started

### First Launch

1. **Install the application** on your Android device (see [BUILD_INSTRUCTIONS.md](BUILD_INSTRUCTIONS.md))
2. **Launch the application** - you'll see the login screen
3. **Enter your nRF Cloud API key**
   - Get your API key from: https://nrfcloud.com → Account Settings → API Keys
   - Click **Login**
4. **Wait for authentication** - the app validates your API key
5. **Device list appears** - showing all devices in your nRF Cloud account

### Subsequent Launches

- If API key is saved, you'll automatically go to the device list
- If you need to change accounts, use the **Logout** button

## Application Flow

### 1. Login Screen

**Purpose**: Authenticate with nRF Cloud

**Steps**:
1. Enter your nRF Cloud API key
2. Click **Login**
3. App validates the API key
4. On success, navigates to device list

**Note**: API key is saved for future use (stored in SharedPreferences)

### 2. Device List Screen

**Purpose**: Browse and select devices

**Features**:
- Lists all devices in your nRF Cloud account
- Shows device name (or ID if name not available)
- Shows online/offline status
- Shows last seen timestamp
- **Refresh** button to reload device list
- **Logout** button to clear credentials

**Actions**:
- **Tap a device** → Selection dialog appears
- **Refresh** → Reload device list from nRF Cloud
- **Logout** → Clear API key and return to login

### 3. Shell Selection Dialog

**Purpose**: Choose how to interact with the device

**Options**:
- **RPC Shell**: Structured DECT RPC protocol (binary packets)
- **Direct Shell**: Direct dect_shell commands (JSON format)
- **Cancel**: Return to device list

### 4. Terminal Screen

**Purpose**: Execute commands and view responses

**Features**:
- Auto-connects to selected device
- Terminal interface with command input
- Scrollable output area
- Real-time responses
- Command history (if supported by keyboard)

## Shell Types

### RPC Shell (TerminalActivity)

Uses the DECT RPC protocol with structured binary packets.

#### Available Commands

**Connection Commands**:
- `connect <device_id> <api_key>` - Connect to nRF Cloud (usually auto-connects)
- `disconnect` - Disconnect from nRF Cloud

**DECT Control Commands**:
- `activate [iface_index]` - Activate DECT interface (default: 0)
- `deactivate [iface_index]` - Deactivate DECT interface (default: 0)

**Status Commands**:
- `status [iface_index]` - Get DECT status information (default: 0)
  - Returns structured data: modem status, cluster info, network status, etc.

**Settings Commands**:
- `settings read [iface_index]` - Read DECT settings (default: 0)
- `settings write [iface_index]` - Write DECT settings (not yet implemented)

**Event Commands**:
- `events subscribe [iface_index] [mask]` - Subscribe to DECT events
  - `iface_index`: Interface index (default: 0)
  - `mask`: Event mask in hex (default: 0xFFFFFFFF = all events)
- `events unsubscribe` - Unsubscribe from events (not yet implemented)

**Utility Commands**:
- `help` - Show help message
- `exit` or `quit` - Exit the application

#### RPC Shell Examples

```
> activate 0
Activating DECT interface 0...
DECT interface 0 activated successfully

> status 0
Getting status for interface 0...
Status for interface 0:
  Modem activated: True
  Cluster running: True
  Cluster channel: 5
  Network beacon running: False
  Parent count: 0
  Child count: 2
  Device type: Ft
  Firmware version: 1.2.3

> events subscribe 0 0xFFFFFFFF
Subscribing to events for interface 0 (mask: 0xFFFFFFFF)...
Event subscription successful

> settings read 0
Reading settings for interface 0...
Settings:
  Channel: 5
  Power: 10 dBm
  ...
```

### Direct Shell (DirectShellActivity)

Uses direct dect_shell commands sent as JSON: `{"appId":"DECT_SHELL", "data":"<command>"}`

#### Available Commands

**All dect_shell commands are supported**, for example:

- `version` - Show version information
- `dect status 0` - Get DECT status
- `dect activate 0` - Activate DECT interface
- `dect deactivate 0` - Deactivate DECT interface
- `dect settings read 0` - Read DECT settings
- `dect settings write 0 <file>` - Write DECT settings
- `help` - Show help (shows all available commands)
- Any other dect_shell command

**Utility Commands**:
- `disconnect` - Disconnect from device
- `exit` or `quit` - Exit application
- `help` - Show help message

#### Direct Shell Examples

```
> version
Nordic DECT NR+ Shell v1.0.0
Build: 2025-01-15

> dect status 0
DECT Interface 0:
  Status: Active
  Channel: 5
  Cluster: Running
  ...

> dect activate 0
Activating DECT interface 0...
DECT interface 0 activated

> dect settings read 0
Reading DECT settings...
[Settings output]
```

## Command Examples

### Basic RPC Shell Usage

```
> activate 0
Activating DECT interface 0...
DECT interface 0 activated successfully

> status 0
Getting status for interface 0...
Status for interface 0:
  Modem activated: True
  Cluster running: True
  ...

> events subscribe 0 0xFFFFFFFF
Subscribing to events for interface 0 (mask: 0xFFFFFFFF)...
Event subscription successful
```

### Basic Direct Shell Usage

```
> version
Nordic DECT NR+ Shell v1.0.0

> dect status 0
DECT Interface 0:
  Status: Active
  ...

> dect activate 0
Activating DECT interface 0...
DECT interface 0 activated
```

### Event Subscription (RPC Shell)

After subscribing to events, you'll receive real-time notifications:

```
Event: ActivateDone on interface 0
Event: AssociationChanged on interface 0
Event: NetworkStatus on interface 0
```

## Tips and Best Practices

### Choosing Shell Type

**Use RPC Shell when**:
- You need structured data (e.g., status as objects)
- You're building automated scripts
- You need programmatic control
- You want type-safe responses

**Use Direct Shell when**:
- You want interactive shell experience
- You're testing or debugging
- You need all dect_shell commands
- You prefer text-based responses

### Command Tips

1. **Interface Index**: Most commands default to interface index 0. Specify a different index if you have multiple DECT interfaces.

2. **Event Mask** (RPC Shell): Use hex format for event masks:
   - `0xFFFFFFFF` - All events
   - `0x00000001` - Only activate events
   - `0x00000002` - Only deactivate events

3. **Command History**: Use up/down arrow keys to navigate command history (if supported by your keyboard).

4. **Connection State**: The connection state is displayed in brackets:
   - `[Connected]` - Connected to nRF Cloud
   - `[Disconnected]` - Not connected

5. **Auto-connect**: Both shell types auto-connect when you select a device. You can still use `connect` command manually if needed.

## Troubleshooting

### Connection Issues

**"Connection error"**:
- Check your network connection
- Verify device ID and API key are correct
- Check nRF Cloud service status

**"Not connected"**:
- Use `connect` command before executing DECT commands
- Check that device is online in nRF Cloud portal

**"Timeout"**:
- Ensure the device is online and reachable via nRF Cloud
- Check firewall/network restrictions
- Verify MQTT broker connectivity: `mqtt.nrfcloud.com:8883`

### Command Issues

**"Unknown command"**:
- Check command spelling
- Use `help` for available commands
- Verify you're using the correct shell type

**"Invalid device ID"**:
- Device ID should be a valid UUID or device identifier
- Check device ID in nRF Cloud portal

**"Invalid API key"**:
- API key should be at least 8 characters
- Verify API key in nRF Cloud portal → Account Settings

### Event Issues

**"No events received"**:
- Verify event subscription was successful
- Check that device is sending events
- Verify event mask includes desired events

**"Events not displaying"**:
- Check that events are enabled on the device side
- Verify event subscription was successful

### Device List Issues

**"No devices found"**:
- Verify you have devices in your nRF Cloud account
- Check API key has access to devices
- Try refreshing the device list

**"Device appears offline"**:
- Device may be disconnected from nRF Cloud
- Check device connectivity
- Verify device is powered on

## Security Notes

- **API Keys**: Never share your nRF Cloud API keys
- **Device IDs**: Device IDs are typically UUIDs and can be shared
- **TLS**: All MQTT connections use TLS encryption
- **Credentials**: API keys are stored in SharedPreferences (consider using Android Keystore for production)
- **Network**: Ensure you're on a secure network when using the application

## Advanced Usage

### Multiple Devices

You can switch between devices:
1. Press **Back** button to return to device list
2. Select a different device
3. Choose shell type
4. New terminal session opens

### Command Scripting

For RPC Shell, you can build scripts using the structured responses:
- Parse status responses programmatically
- Build automation workflows
- Integrate with other applications

For Direct Shell, you can use standard shell commands:
- Chain commands
- Use shell features
- Redirect output (if supported)

## Next Steps

- See [APP_FLOW.md](APP_FLOW.md) for detailed application flow
- See [BUILD_INSTRUCTIONS.md](BUILD_INSTRUCTIONS.md) for build and setup
- See [README.md](README.md) for overview and architecture

## License

Copyright (c) 2025 Nordic Semiconductor ASA

SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
