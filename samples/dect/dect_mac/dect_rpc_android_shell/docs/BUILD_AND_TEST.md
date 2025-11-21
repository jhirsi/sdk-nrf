# Build and Test Guide

Complete guide for building and testing the integrated DECT RPC Android Shell application.

## Prerequisites

- ✅ Android SDK installed
- ✅ Gradle wrapper (`gradlew`) in project
- ✅ Android device connected (for testing)
- ✅ USB debugging enabled on device
- ✅ nRF Cloud account with API key

## Quick Start

### 1. Build the Application

```bash
cd /home/jani/ncs/nrf/samples/dect/dect_mac/dect_rpc_android_shell
./build.sh Debug
```

**Expected Output:**
```
==========================================
Building DECT RPC Android Shell
Configuration: Debug
==========================================
...
Build successful!
APK location: app/build/outputs/apk/debug/app-debug.apk
```

### 2. Deploy to Device

```bash
./deploy.sh Debug
```

**Expected Output:**
```
==========================================
Deploying DECT RPC Android Shell
Configuration: Debug
==========================================
Found 1 device(s)
Installing APK...
Deployment successful!
```

### 3. Test the Application

1. **Open the app** on your device (look for "DECT NR+ Shell")
2. **Login** with your nRF Cloud API key
3. **Select a device** from the list
4. **Choose "RPC Shell"** when prompted
5. **Test commands**:
   - `help` - Show available commands
   - `status` - Get DECT status
   - `activate` - Activate DECT stack
   - `deactivate` - Deactivate DECT stack

## Troubleshooting

### Build Errors

#### "gradlew: command not found"
```bash
# Copy gradlew from reference project or generate new one
cp /mnt/c/Users/jahi/code_wa/StudioProjects/nRFCloud_remote/gradlew ./
chmod +x gradlew
```

#### "SDK location not found"
Create `local.properties`:
```properties
sdk.dir=C\:\\Users\\jahi\\AppData\\Local\\Android\\Sdk
```

#### Compilation Errors
```bash
# Clean and rebuild
./gradlew clean
./build.sh Debug
```

### Runtime Errors

#### App Crashes on Launch
1. Check logcat: `adb logcat | grep -i "dect\|error\|fatal"`
2. Verify all dependencies are included
3. Check AndroidManifest.xml for missing permissions

#### MQTT Connection Fails
1. Verify API key is correct
2. Check device ID format (should be UUID)
3. Verify network connectivity
4. Check logcat for MQTT errors

#### RPC Commands Fail
1. Verify device is connected to nRF Cloud
2. Check logcat for RPC protocol errors
3. Verify CBOR encoding/decoding
4. Test with `status` command first (simplest)

### Debug Logging

Enable verbose logging:
```bash
# View all logs
adb logcat | grep -i "dect"

# View specific component
adb logcat | grep -i "DectRpcClient"
adb logcat | grep -i "DectRpcShell"
adb logcat | grep -i "CborSerializer"
```

## Testing Checklist

### Basic Functionality
- [ ] App launches without crashing
- [ ] Login screen appears
- [ ] Can enter API key
- [ ] Login succeeds
- [ ] Device list loads
- [ ] Can select device
- [ ] Shell selection dialog appears

### RPC Shell
- [ ] Can connect to device
- [ ] `help` command works
- [ ] `status` command returns data
- [ ] `activate` command executes
- [ ] `deactivate` command executes
- [ ] Connection state updates correctly
- [ ] Events are received (if any)

### Error Handling
- [ ] Invalid API key shows error
- [ ] Network errors are handled
- [ ] Timeout errors are handled
- [ ] Invalid commands show error message

## Manual Testing Steps

### Test 1: Login Flow
1. Open app
2. Enter invalid API key → Should show error
3. Enter valid API key → Should navigate to device list

### Test 2: Device List
1. After login, verify devices are listed
2. Check online/offline status
3. Tap on device → Should show shell selection

### Test 3: RPC Shell Connection
1. Select "RPC Shell"
2. Wait for "Connecting..." message
3. Verify "Connected to nRF Cloud" appears
4. Verify connection state is updated

### Test 4: RPC Commands
1. Type `status` → Should return DECT status info
2. Type `activate` → Should activate DECT (if not already active)
3. Type `status` again → Should show updated status
4. Type `deactivate` → Should deactivate DECT

### Test 5: Error Handling
1. Disconnect device from nRF Cloud
2. Try `status` command → Should show error
3. Reconnect device
4. Try command again → Should work

## Performance Testing

### Connection Time
- Measure time from "Connecting..." to "Connected"
- Should be < 5 seconds on good network

### Command Response Time
- Measure time from command sent to response received
- Should be < 2 seconds for simple commands
- May be longer for complex operations

### Memory Usage
```bash
# Monitor memory usage
adb shell dumpsys meminfo com.nordicsemi.dect_rpc_shell
```

## Integration Verification

### Verify Library Integration
1. Check that `DectRpcClient` is instantiated
2. Verify MQTT connection is established
3. Check that RPC packets are encoded correctly
4. Verify responses are decoded correctly

### Verify CBOR Encoding
1. Capture MQTT messages (if possible)
2. Verify CBOR format matches C server expectations
3. Check boolean encoding (should be BooleanDataItem)
4. Verify integer encoding (signed/unsigned)

## Next Steps After Testing

1. **Fix Issues**: Address any bugs found during testing
2. **Optimize**: Improve performance if needed
3. **Add Features**: Implement additional RPC commands
4. **Documentation**: Update docs based on findings
5. **Release**: Prepare for production use

## Support

For issues or questions:
1. Check logcat output
2. Review documentation in `docs/` folder
3. Check GitHub issues (if applicable)
4. Contact development team

