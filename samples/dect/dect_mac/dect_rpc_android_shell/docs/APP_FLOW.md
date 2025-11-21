# DECT RPC Android Shell - Application Flow

## Overview

The application now follows a three-screen flow:

1. **Login Screen** - Authenticate to nRF Cloud
2. **Device List Screen** - Display all devices as a list
3. **Terminal Screen** - DECT RPC shell for selected device

## Flow Description

### 1. LoginActivity (Main Launcher)

**Purpose**: Authenticate user with nRF Cloud API key

**Features**:
- API key input field
- API key validation via REST API
- Saves API key to SharedPreferences for future use
- Auto-navigates to DeviceListActivity on successful authentication
- If API key is already saved, skips login and goes directly to device list

**Navigation**:
- Success → DeviceListActivity
- Failure → Stays on LoginActivity with error message

### 2. DeviceListActivity

**Purpose**: Display all nRF Cloud devices in a scrollable list

**Features**:
- Fetches device list from nRF Cloud REST API (`/v1/devices`)
- Displays devices with:
  - Device name (or ID if name not available)
  - Online/Offline status
  - Last seen timestamp
- Refresh button to reload device list
- Logout button to clear saved API key and return to login
- Click on device → Shows selection dialog to choose shell type

**Navigation**:
- Device click → Selection dialog (RPC Shell or Direct Shell)
  - RPC Shell → TerminalActivity (with device ID, API key, and device name)
  - Direct Shell → DirectShellActivity (with device ID, API key, and device name)
- Logout → LoginActivity

### 3. TerminalActivity

**Purpose**: DECT RPC shell interface for controlling DECT stack

**Features**:
- Auto-connects to selected device using device ID and API key from intent
- Terminal interface for executing DECT RPC commands
- All existing shell commands available:
  - `activate [iface_index]`
  - `deactivate [iface_index]`
  - `status [iface_index]`
  - `settings read [iface_index]`
  - `events subscribe [iface_index] [mask]`
  - `help`
  - `disconnect`
  - `exit`

**Navigation**:
- Back button → Returns to DeviceListActivity

## Implementation Details

### New Files

1. **CloudRestService.cs**
   - REST API client for nRF Cloud
   - `GetDevicesAsync()` - Fetches device list
   - `ValidateApiKeyAsync()` - Validates API key

2. **LoginActivity.cs**
   - Login screen with API key input
   - Validates API key before proceeding
   - Saves API key to SharedPreferences

3. **DeviceListActivity.cs**
   - Device list screen using RecyclerView
   - DeviceAdapter and DeviceViewHolder for list items
   - Fetches and displays devices from nRF Cloud
   - Shows selection dialog when device is clicked

4. **DirectShellClient.cs** (New)
   - MQTT client for sending direct DECT shell commands
   - Sends JSON: `{"appId":"DECT_SHELL", "data":"<command>"}`
   - Publishes to `c2d` topic, subscribes to `d2c` topic

5. **DirectShellActivity.cs** (New)
   - Terminal interface for direct DECT shell commands
   - Auto-connects to device
   - Sends commands as JSON via DirectShellClient

### Modified Files

1. **TerminalActivity.cs**
   - Removed `MainLauncher = true`
   - Added auto-connect functionality
   - Receives device ID, API key, and device name from intent
   - Auto-connects on startup if device ID is available

2. **AndroidManifest.xml**
   - Added LoginActivity as main launcher
   - Added DeviceListActivity
   - Updated TerminalActivity (removed main launcher)
   - Set parent activities for proper back navigation

3. **DectRpcAndroidShell.csproj**
   - Added `Xamarin.AndroidX.RecyclerView` package
   - Added `System.Text.Json` package

### Data Persistence

- **SharedPreferences** (`DectRpcPrefs`):
  - `ApiKey` - Saved API key for auto-login
  - `LastDeviceId` - Last used device ID (optional, for fallback)

### API Integration

**nRF Cloud REST API**:
- Base URL: `https://api.nrfcloud.com/v1`
- Authentication: Bearer token (API key)
- Endpoint: `GET /devices`
- Response format: `{ "items": [ { "id": "...", "name": "...", "state": { "online": true, "lastSeen": "..." } }, ... ] }`

## Usage

1. **First Launch**:
   - App opens to LoginActivity
   - Enter nRF Cloud API key
   - Click "Login"
   - App validates API key and navigates to DeviceListActivity

2. **Device Selection**:
   - DeviceListActivity shows all devices
   - Tap on a device to see selection dialog
   - Choose "RPC Shell" for DECT RPC protocol (binary packets)
   - Choose "Direct Shell" for direct dect_shell commands (JSON format)
   - Selected activity auto-connects to device

3. **Subsequent Launches**:
   - If API key is saved, LoginActivity auto-navigates to DeviceListActivity
   - User can select a device and use the terminal

4. **Logout**:
   - Click "Logout" button in DeviceListActivity
   - Clears saved API key
   - Returns to LoginActivity

## Shell Types

### RPC Shell (TerminalActivity)
- Uses DECT RPC protocol with binary packets
- Structured commands: `activate`, `deactivate`, `status`, `settings read/write`, `events subscribe`
- Responses are structured CBOR-encoded data
- Best for: Programmatic control, structured data access

### Direct Shell (DirectShellActivity)
- Uses direct dect_shell commands via JSON
- Commands sent as: `{"appId":"DECT_SHELL", "data":"<command>"}`
- Supports all dect_shell commands (e.g., `version`, `dect status 0`, `dect activate 0`)
- Responses are plain text from the shell
- Best for: Interactive shell usage, testing, debugging

## Notes

- API key is stored in SharedPreferences (not encrypted - consider encryption for production)
- Device list is fetched on each visit to DeviceListActivity
- Both shell types auto-connect but can still use `connect` command manually if needed
- Back navigation follows: TerminalActivity/DirectShellActivity → DeviceListActivity → LoginActivity
- MQTT topics:
  - Commands: `<device_id>/c2d` (cloud-to-device)
  - Responses: `<device_id>/d2c` (device-to-cloud)

