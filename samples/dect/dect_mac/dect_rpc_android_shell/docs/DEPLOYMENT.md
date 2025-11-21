# Deploying DECT RPC Android Shell to Your Phone

Complete guide for installing and running the application on your Android device.

## Prerequisites

1. **Android Device**:
   - Android 5.0 (API 21) or later
   - USB debugging enabled
   - Developer options enabled

2. **Computer**:
   - USB cable to connect phone
   - ADB (Android Debug Bridge) installed (comes with Android SDK)
   - **Windows**: ADB is typically at `C:\Users\<YourName>\AppData\Local\Android\Sdk\platform-tools\adb.exe`

## Method 1: Direct Deployment via dotnet CLI (Recommended)

### Step 1: Enable USB Debugging on Your Phone

1. **Enable Developer Options**:
   - Go to **Settings → About Phone**
   - Tap **Build Number** 7 times
   - You'll see "You are now a developer!"

2. **Enable USB Debugging**:
   - Go to **Settings → Developer Options**
   - Enable **USB Debugging**
   - (Optional) Enable **Stay Awake** (keeps screen on while charging)

3. **Connect Phone to Computer**:
   - Connect via USB cable
   - On your phone, you may see a prompt: **"Allow USB debugging?"**
   - Check **"Always allow from this computer"** and tap **OK**

### Step 2: Verify Device Connection

**Windows PowerShell/Command Prompt**:

**Option A: Use full path to ADB** (if not in PATH):
```powershell
# Replace <YourName> with your Windows username
C:\Users\<YourName>\AppData\Local\Android\Sdk\platform-tools\adb.exe devices
```

**Option B: Add ADB to PATH** (recommended, no admin rights required):
1. Find your Android SDK path (usually `C:\Users\<YourName>\AppData\Local\Android\Sdk`)
2. Add `platform-tools` folder to **User PATH** (doesn't require admin rights):
   
   **Method 1: Via Settings UI** (Windows 11):
   - Press **Win + I** to open Settings
   - Go to **System** → **About** → **Advanced system settings** (at the bottom)
   - Click **Environment Variables** button
   - Under **User variables** (top section), find and select **Path** → Click **Edit**
   - Click **New** → Add: `C:\Users\<YourName>\AppData\Local\Android\Sdk\platform-tools`
   - Click **OK** on all dialogs
   - **Close and reopen PowerShell/Command Prompt** for changes to take effect
   
   **Method 2: Via PowerShell** (quick method, no admin rights):
   ```powershell
   # Replace <YourName> with your Windows username
   $userPath = [Environment]::GetEnvironmentVariable("Path", "User")
   $adbPath = "C:\Users\<YourName>\AppData\Local\Android\Sdk\platform-tools"
   if ($userPath -notlike "*$adbPath*") {
       [Environment]::SetEnvironmentVariable("Path", "$userPath;$adbPath", "User")
       Write-Host "ADB added to PATH. Please restart PowerShell."
   } else {
       Write-Host "ADB is already in PATH."
   }
   ```
   - **Close and reopen PowerShell** after running this command
   
3. Then run:
   ```powershell
   adb devices
   ```

**Linux/Mac**:
```bash
adb devices
```

You should see your device listed:
```
List of devices attached
ABC123XYZ    device
```

If you see "unauthorized", check your phone for the USB debugging authorization prompt.

### Step 3: Build and Deploy

Navigate to the project directory:

```bash
cd /path/to/ncs/nrf/samples/dect/dect_mac/dect_rpc_android_shell
```

**Option A: Build and Install Debug APK** (Recommended - works reliably):
```powershell
# Windows (use full path if ADB not in PATH)
dotnet build -c Debug
# IMPORTANT: Use the -Signed.apk file (signed APK), not the unsigned one
C:\Users\<YourName>\AppData\Local\Android\Sdk\platform-tools\adb.exe install -r bin/Debug/net10.0-android/com.nordicsemi.dect_rpc_shell-Signed.apk
```

**Option B: Build Release APK** (may have debug info extraction errors, but APK still works):
```powershell
# Windows (use full path if ADB not in PATH)
# Note: Release builds may show "Failed to extract debug info" errors
# These are harmless - the APK will still be created and work correctly
dotnet build -c Release 2>&1 | Out-String
# IMPORTANT: Use the -Signed.apk file (signed APK)
# The APK will be at: bin/Release/net10.0-android/com.nordicsemi.dect_rpc_shell-Signed.apk
C:\Users\<YourName>\AppData\Local\Android\Sdk\platform-tools\adb.exe install -r bin/Release/net10.0-android/com.nordicsemi.dect_rpc_shell-Signed.apk
```

**Note**: 
- If you added ADB to PATH, you can use `adb` instead of the full path.
- **Debug builds are recommended** as they work without errors and are easier to debug if issues occur.
- Release builds may show "Failed to extract debug info" errors, but the APK is still created and functional.

**Option C: Build and Install via dotnet** (if supported):
```bash
dotnet build -c Debug
dotnet android install
```

### Step 4: Run the App

1. On your phone, find the **"DECT RPC Shell"** app in the app drawer
2. Tap to open it
3. You should see the login screen

## Method 2: Manual APK Installation

### Step 1: Build APK

```bash
cd /path/to/ncs/nrf/samples/dect/dect_mac/dect_rpc_android_shell
dotnet build -c Release
```

### Step 2: Find the APK

The APK will be located at:
```
bin/Release/net10.0-android/com.nordicsemi.dect_rpc_shell-Signed.apk
```

Or for Debug builds:
```
bin/Debug/net10.0-android/com.nordicsemi.dect_rpc_shell-Signed.apk
```

**Important**: Always use the **-Signed.apk** file for installation. The unsigned `.apk` file (without `-Signed`) cannot be installed on Android devices as it lacks the required signature.

### Step 3: Transfer APK to Phone

**Option A: Via USB (File Transfer Mode)**:
1. Connect phone via USB
2. On phone, select **File Transfer** mode when prompted
3. Copy APK to phone's Downloads folder
4. On phone, open **Files** app → **Downloads**
5. Tap the APK file
6. Tap **Install** (you may need to allow "Install from Unknown Sources")

**Option B: Via ADB**:
```powershell
# Windows (use full path if ADB not in PATH)
# IMPORTANT: Use the -Signed.apk file
C:\Users\<YourName>\AppData\Local\Android\Sdk\platform-tools\adb.exe install -r bin/Release/net10.0-android/com.nordicsemi.dect_rpc_shell-Signed.apk
```

Or for Debug builds:
```powershell
C:\Users\<YourName>\AppData\Local\Android\Sdk\platform-tools\adb.exe install -r bin/Debug/net10.0-android/com.nordicsemi.dect_rpc_shell-Signed.apk
```

**Option C: Via Email/Cloud**:
1. Email the APK to yourself
2. Open email on phone
3. Download and install the APK

## Method 3: Using Android Studio

### Step 1: Open Project in Android Studio

1. Launch **Android Studio**
2. **File → Open**
3. Navigate to project directory
4. Select the directory and click **OK**

### Step 2: Connect Device

1. Connect phone via USB
2. Enable USB debugging (see Method 1, Step 1)
3. In Android Studio, you should see your device in the device dropdown

### Step 3: Run Application

1. Click the **Run** button (green play icon) in the toolbar
2. Or: **Run → Run 'app'** (or press **Shift+F10** / **Ctrl+R**)
3. Select your device from the list
4. Click **OK**
5. Android Studio will build, install, and launch the app automatically

## Troubleshooting

### "Device not found" or "adb: no devices/emulators found"

**Solutions**:
1. Check USB cable (try a different cable)
2. Check USB connection mode on phone (should be "File Transfer" or "MTP")
3. Restart ADB:
   ```powershell
   # Windows (use full path if ADB not in PATH)
   C:\Users\<YourName>\AppData\Local\Android\Sdk\platform-tools\adb.exe kill-server
   C:\Users\<YourName>\AppData\Local\Android\Sdk\platform-tools\adb.exe start-server
   C:\Users\<YourName>\AppData\Local\Android\Sdk\platform-tools\adb.exe devices
   ```
4. Check phone for USB debugging authorization prompt
5. Try different USB port on computer

### "Installation failed: INSTALL_FAILED_INSUFFICIENT_STORAGE"

**Solution**: Free up space on your phone

### "Installation failed: INSTALL_FAILED_UPDATE_INCOMPATIBLE"

**Solution**: Uninstall the existing app first:
```powershell
# Windows (use full path if ADB not in PATH)
C:\Users\<YourName>\AppData\Local\Android\Sdk\platform-tools\adb.exe uninstall com.nordicsemi.dect_rpc_shell
```
Then install again.

### "Installation failed: INSTALL_PARSE_FAILED_NO_CERTIFICATES"

**Solution**: You're trying to install the unsigned APK. Always use the **-Signed.apk** file:
```powershell
# Wrong (unsigned APK):
adb install -r bin/Debug/net10.0-android/com.nordicsemi.dect_rpc_shell.apk

# Correct (signed APK):
adb install -r bin/Debug/net10.0-android/com.nordicsemi.dect_rpc_shell-Signed.apk
```

The build process creates two APK files:
- `com.nordicsemi.dect_rpc_shell.apk` - **unsigned** (cannot be installed)
- `com.nordicsemi.dect_rpc_shell-Signed.apk` - **signed** (use this one)

If you still get this error with the signed APK, try:
```bash
dotnet clean
dotnet build -c Debug
```

### "App crashes on launch"

**Check logs**:
```powershell
# Windows PowerShell (use full path if ADB not in PATH)
C:\Users\<YourName>\AppData\Local\Android\Sdk\platform-tools\adb.exe logcat | Select-String -Pattern "dect|fatal|exception"
```

**Linux/Mac**:
```bash
adb logcat | grep -i "dect\|fatal\|exception"
```

Or view logs in Android Studio:
1. **View → Tool Windows → Logcat**
2. Filter by package: `com.nordicsemi.dect_rpc_shell`

### "USB debugging authorization keeps appearing"

**Solution**: 
1. Revoke USB debugging authorizations: **Settings → Developer Options → Revoke USB debugging authorizations**
2. Disconnect and reconnect USB
3. Authorize again and check "Always allow from this computer"

## Verifying Installation

After installation, verify the app is installed:

```powershell
# Windows PowerShell (use full path if ADB not in PATH)
C:\Users\<YourName>\AppData\Local\Android\Sdk\platform-tools\adb.exe shell pm list packages | Select-String dect
```

**Linux/Mac**:
```bash
adb shell pm list packages | grep dect
```

You should see:
```
package:com.nordicsemi.dect_rpc_shell
```

## Uninstalling the App

**Via ADB**:
```powershell
# Windows (use full path if ADB not in PATH)
C:\Users\<YourName>\AppData\Local\Android\Sdk\platform-tools\adb.exe uninstall com.nordicsemi.dect_rpc_shell
```

**Linux/Mac**:
```bash
adb uninstall com.nordicsemi.dect_rpc_shell
```

**Via Phone**:
1. **Settings → Apps → DECT RPC Shell**
2. Tap **Uninstall**

## Next Steps

After successful installation:
1. See [USAGE.md](USAGE.md) for how to use the application
2. See [APP_FLOW.md](APP_FLOW.md) for application flow details
3. Test with a DECT NR+ device connected to nRF Cloud

## License

Copyright (c) 2025 Nordic Semiconductor ASA

SPDX-License-Identifier: LicenseRef-Nordic-5-Clause

