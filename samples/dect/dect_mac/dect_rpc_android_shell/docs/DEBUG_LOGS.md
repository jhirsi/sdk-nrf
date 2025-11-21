# How to Get Debug Logs

This guide explains how to capture Android logs to diagnose device status issues.

## Prerequisites

- Android device connected via USB or ADB over network
- ADB (Android Debug Bridge) installed and in your PATH

## Quick Start (WSL)

### Step 1: Install ADB in WSL

```bash
sudo apt-get update
sudo apt-get install -y android-tools-adb android-tools-fastboot
```

### Step 2: Set Up Connection

If your device is connected via USB on Windows, you have several options:

**Option A: Use Windows ADB from WSL (Easiest)**

Add to your `~/.bashrc`:
```bash
alias adb='/mnt/c/Users/<YOUR_USERNAME>/AppData/Local/Android/Sdk/platform-tools/adb.exe'
```

Or if Android Studio is installed:
```bash
alias adb='/mnt/c/Users/<YOUR_USERNAME>/AppData/Local/Android/Sdk/platform-tools/adb.exe'
```

Then reload: `source ~/.bashrc`

**Option B: Use ADB over Network**

1. On Windows (with device connected via USB):
   ```cmd
   adb tcpip 5555
   adb shell ip addr show wlan0  # Get device IP
   ```

2. On WSL:
   ```bash
   adb connect <DEVICE_IP>:5555
   ```

**Option C: Use USB over IP (usbipd-win)**

See detailed instructions in `setup_adb_wsl.sh` script.

### Step 3: Use the Helper Script

```bash
cd /home/jani/ncs/nrf/samples/dect/dect_mac/dect_rpc_android_shell
./setup_adb_wsl.sh    # First time setup
./get_logs.sh         # Capture logs
```

## Method 1: Using ADB Logcat (Recommended)

### Step 1: Connect Your Device

```bash
# Check if device is connected
adb devices
```

You should see your device listed. If not, enable USB debugging on your device.

### Step 2: Filter Logs for CloudRestService

```bash
# Filter logs for device status parsing
adb logcat -s CloudRestService:D

# Or save to file
adb logcat -s CloudRestService:D > device_status_logs.txt
```

### Step 3: Use the App

1. Open the DECT RPC Android Shell app
2. Login with your API key
3. Wait for devices to load
4. The logs will show detailed information about how device status is being determined

### Step 4: View the Logs

The logs will show:
- Full device JSON structure
- `state.online` value and how it's parsed
- `connection.status` value from multiple locations
- Final `isOnline` determination

Example log output:
```
D/CloudRestService: === Parsing devices JSON ===
D/CloudRestService: Found 2 devices
D/CloudRestService: === Device: abc123 ===
D/CloudRestService: Full device JSON: {...}
D/CloudRestService: Device abc123 - state object: {...}
D/CloudRestService: Device abc123 - state.online value: true, parsed: true
D/CloudRestService: Device abc123 - reported.connection.status: connected
D/CloudRestService: Device abc123: stateOnline=true, connectionStatus=connected, isOnline=true
```

## Method 2: Using Android Studio

1. Open Android Studio
2. Connect your device
3. Go to **View → Tool Windows → Logcat**
4. Filter by tag: `CloudRestService`
5. Use the app and watch the logs in real-time

## Method 3: Save All Logs to File

```bash
# Save all logs (can be large)
adb logcat > all_logs.txt

# Then filter for CloudRestService
grep "CloudRestService" all_logs.txt > device_status_logs.txt
```

## What to Look For

When checking logs, look for:

1. **Device JSON structure** - Shows the actual API response format
2. **state.online value** - Should be `true` for online devices
3. **connection.status value** - Should be `"connected"` for connected devices
4. **Final isOnline determination** - Shows which condition made the device online/offline

## Common Issues

### Device shows offline but logs show state.online=true
- The logic might need adjustment
- Check if there's a different field we should be using

### state.online field not found
- The API response structure might be different
- Check the full device JSON to see where online status is stored

### connection.status is null
- Device might not have connection status reported yet
- Check if there's another field indicating connection

## Sharing Logs

When reporting issues, please share:
1. The log output from `CloudRestService` tag
2. What the nRF Cloud web portal shows for the device status
3. The device ID that's showing incorrectly

