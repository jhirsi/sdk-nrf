# Debug Tips for DECT RPC Android Shell

## 1. Viewing Logs

### From WSL Command Line

```bash
# View all logs for the app
/mnt/c/Users/jahi/AppData/Local/Android/Sdk/platform-tools/adb.exe logcat | grep "com.nordicsemi.dect_rpc_shell"

# View only errors and fatal issues
/mnt/c/Users/jahi/AppData/Local/Android/Sdk/platform-tools/adb.exe logcat *:E *:F

# View logs with specific tags
/mnt/c/Users/jahi/AppData/Local/Android/Sdk/platform-tools/adb.exe logcat -s DectRpcClient DectRpcShell CloudAuthService

# Clear log buffer and start fresh
/mnt/c/Users/jahi/AppData/Local/Android/Sdk/platform-tools/adb.exe logcat -c
/mnt/c/Users/jahi/AppData/Local/Android/Sdk/platform-tools/adb.exe logcat | grep "com.nordicsemi.dect_rpc_shell"
```

### From Android Studio

1. Open **Logcat** window (View → Tool Windows → Logcat)
2. Filter by package: `package:com.nordicsemi.dect_rpc_shell`
3. Filter by log level: Select level (Verbose, Debug, Info, Warn, Error)
4. Search for specific tags: `tag:DectRpcClient`

## 2. Common Crash Scenarios

### Theme Issues

**Symptom**: App crashes on startup with `ThemeUtils` error

**Solution**: Ensure `styles.xml` uses AppCompat theme:
```xml
<style name="AppTheme" parent="Theme.AppCompat.Light.DarkActionBar">
```

### Null Pointer Exceptions

**Symptom**: `NullPointerException` in logs

**Debug Steps**:
1. Check logcat for the exact line number
2. Look for null checks before accessing objects
3. Use safe call operator (`?.`) in Kotlin

### MQTT Connection Issues

**Symptom**: Cannot connect to nRF Cloud

**Debug Steps**:
1. Check internet connectivity
2. Verify API key is correct
3. Check MQTT broker URI: `CloudAuthService.getMqttBrokerUri()`
4. Look for MQTT connection errors in logcat

### CBOR Decoding Errors

**Symptom**: `IllegalStateException` or `IllegalArgumentException` in `CborSerializer`

**Debug Steps**:
1. Check the raw CBOR data being decoded
2. Verify the data structure matches expected format
3. Add logging before decoding to see what data is received

## 3. Adding Debug Logging

### In Kotlin Code

```kotlin
import android.util.Log

// Debug log
Log.d("TAG", "Debug message: $variable")

// Error log with exception
Log.e("TAG", "Error occurred", exception)

// Info log
Log.i("TAG", "Info message")

// Warning log
Log.w("TAG", "Warning message")
```

### Viewing Your Logs

```bash
# Filter by your tag
/mnt/c/Users/jahi/AppData/Local/Android/Sdk/platform-tools/adb.exe logcat -s TAG
```

## 4. Testing MQTT Connection

### Check if MQTT is Connected

```bash
# Look for MQTT connection logs
/mnt/c/Users/jahi/AppData/Local/Android/Sdk/platform-tools/adb.exe logcat | grep -i mqtt
```

### Test MQTT Manually

You can test MQTT connection using the nRF Cloud web terminal or MQTT client tools.

## 5. Testing RPC Commands

### Enable Verbose Logging

Add this to your code temporarily:
```kotlin
Log.v("DectRpcClient", "Sending command: $cmdId, payload: ${payload.contentToString()}")
Log.v("DectRpcClient", "Received response: ${response.contentToString()}")
```

### Check RPC Protocol

```bash
# View RPC-related logs
/mnt/c/Users/jahi/AppData/Local/Android/Sdk/platform-tools/adb.exe logcat | grep -i "rpc\|dect"
```

## 6. Network Debugging

### Check Network Permissions

```bash
# Verify permissions are granted
/mnt/c/Users/jahi/AppData/Local/Android/Sdk/platform-tools/adb.exe shell dumpsys package com.nordicsemi.dect_rpc_shell | grep permission
```

### Test Network Connectivity

```bash
# Check if device has internet
/mnt/c/Users/jahi/AppData/Local/Android/Sdk/platform-tools/adb.exe shell ping -c 3 8.8.8.8
```

## 7. App State Inspection

### Check App Process

```bash
# See if app is running
/mnt/c/Users/jahi/AppData/Local/Android/Sdk/platform-tools/adb.exe shell ps | grep dect_rpc_shell

# Check app memory usage
/mnt/c/Users/jahi/AppData/Local/Android/Sdk/platform-tools/adb.exe shell dumpsys meminfo com.nordicsemi.dect_rpc_shell
```

### Force Stop and Restart

```bash
# Force stop the app
/mnt/c/Users/jahi/AppData/Local/Android/Sdk/platform-tools/adb.exe shell am force-stop com.nordicsemi.dect_rpc_shell

# Start the app
/mnt/c/Users/jahi/AppData/Local/Android/Sdk/platform-tools/adb.exe shell am start -n com.nordicsemi.dect_rpc_shell/.activities.LoginActivity
```

## 8. APK Inspection

### Check APK Info

```bash
# Get APK path
APK_PATH=$(find app/build/outputs/apk -name "*.apk" | head -1)

# Check APK info
/mnt/c/Users/jahi/AppData/Local/Android/Sdk/build-tools/*/aapt dump badging "$APK_PATH"
```

### Verify Signing

```bash
# Check if APK is signed
/mnt/c/Users/jahi/AppData/Local/Android/Sdk/build-tools/*/apksigner verify --print-certs "$APK_PATH"
```

## 9. Common Issues and Solutions

### Issue: App crashes immediately on launch

**Debug**:
1. Check logcat for stack trace
2. Verify theme is correct (AppCompat)
3. Check if all activities are declared in AndroidManifest.xml
4. Verify all dependencies are included

### Issue: Cannot connect to nRF Cloud

**Debug**:
1. Check API key is valid
2. Verify device has internet connection
3. Check MQTT broker URI
4. Look for SSL/TLS certificate errors

### Issue: RPC commands fail

**Debug**:
1. Check if MQTT is connected
2. Verify RPC packet format
3. Check CBOR encoding/decoding
4. Look for timeout errors

### Issue: UI not updating

**Debug**:
1. Check if running on main thread (UI updates must be on main thread)
2. Verify coroutines are using `Dispatchers.Main` for UI updates
3. Check for exceptions in background threads

## 10. Performance Debugging

### Check CPU Usage

```bash
/mnt/c/Users/jahi/AppData/Local/Android/Sdk/platform-tools/adb.exe shell top | grep dect_rpc_shell
```

### Monitor Memory

```bash
# Watch memory usage
watch -n 1 '/mnt/c/Users/jahi/AppData/Local/Android/Sdk/platform-tools/adb.exe shell dumpsys meminfo com.nordicsemi.dect_rpc_shell | grep -E "TOTAL|Native Heap"'
```

## 11. Quick Debug Commands

```bash
# Clear app data and cache
/mnt/c/Users/jahi/AppData/Local/Android/Sdk/platform-tools/adb.exe shell pm clear com.nordicsemi.dect_rpc_shell

# Uninstall app
/mnt/c/Users/jahi/AppData/Local/Android/Sdk/platform-tools/adb.exe uninstall com.nordicsemi.dect_rpc_shell

# Reinstall and launch
cd /home/jani/ncs/nrf/samples/dect/dect_mac/dect_rpc_android_shell
./build-and-deploy.sh Debug
```

## 12. Useful Logcat Filters

```bash
# All app logs
adb logcat | grep "com.nordicsemi.dect_rpc_shell"

# Errors only
adb logcat *:E | grep "com.nordicsemi.dect_rpc_shell"

# Specific tags
adb logcat -s DectRpcClient:D DectRpcShell:D CloudAuthService:D

# Time-based (last 5 minutes)
adb logcat -t "$(date -d '5 minutes ago' +%m-%d\ %H:%M:%S.000)"
```

