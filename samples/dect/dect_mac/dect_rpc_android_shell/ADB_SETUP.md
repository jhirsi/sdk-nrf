# ADB Setup for WSL

This guide shows how to set up ADB in WSL using a symbolic link to the Windows ADB executable.

## Quick Setup (One Command)

```bash
sudo ln -s /mnt/c/Users/jahi/AppData/Local/Android/Sdk/platform-tools/adb.exe /usr/local/bin/adb
```

## Step-by-Step Setup

### Step 1: Create the Symbolic Link

```bash
sudo ln -s /mnt/c/Users/jahi/AppData/Local/Android/Sdk/platform-tools/adb.exe /usr/local/bin/adb
```

**Note:** If the symlink already exists, remove it first:
```bash
sudo rm /usr/local/bin/adb
sudo ln -s /mnt/c/Users/jahi/AppData/Local/Android/Sdk/platform-tools/adb.exe /usr/local/bin/adb
```

### Step 2: Verify the Setup

```bash
# Check if adb is now in PATH
which adb

# Check ADB version
adb version
```

Expected output:
```
/usr/local/bin/adb
Android Debug Bridge version 1.0.41
Version 36.0.0-13206524
```

### Step 3: Test Connection

```bash
# List connected devices
adb devices

# If no devices, restart ADB server
adb kill-server
adb start-server
adb devices
```

## Using ADB Commands

Once set up, you can use `adb` directly from WSL:

```bash
# View logs (filtered for CloudRestService)
adb logcat -s CloudRestService:D

# Save logs to file
adb logcat -s CloudRestService:D | tee device_status_logs.txt

# Clear logs
adb logcat -c

# Restart ADB server (if connection issues)
adb kill-server
adb start-server
```

## Using the Helper Scripts

After setting up the symlink, you can use the helper scripts:

```bash
cd /home/jani/ncs/nrf/samples/dect/dect_mac/dect_rpc_android_shell

# Setup and verify ADB connection
./setup_adb_wsl.sh

# Capture logs
./get_logs.sh
```

## Troubleshooting

### "adb: command not found"
- Make sure `/usr/local/bin` is in your PATH
- Check: `echo $PATH | grep /usr/local/bin`
- If not, add to `~/.bashrc`: `export PATH=$PATH:/usr/local/bin`

### "No devices found"
- Connect your Android device via USB
- Enable USB debugging on your device
- Try: `adb kill-server && adb start-server`
- If using WSL, you may need to use ADB over network (see `setup_adb_wsl.sh`)

### "Permission denied"
- Make sure you have sudo access
- The symlink needs to be created in `/usr/local/bin` which requires root

## Alternative: Use Alias Instead

If you prefer an alias over a symlink, add to `~/.bashrc`:

```bash
alias adb='/mnt/c/Users/jahi/AppData/Local/Android/Sdk/platform-tools/adb.exe'
```

Then reload: `source ~/.bashrc`
