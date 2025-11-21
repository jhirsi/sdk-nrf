# How to Deploy to Your Phone

This guide shows you how to deploy the DECT RPC Android Shell app to your Android phone.

## Prerequisites

1. **Enable USB Debugging on your phone:**
   - Go to **Settings** → **About phone**
   - Tap **Build number** 7 times to enable Developer options
   - Go back to **Settings** → **Developer options**
   - Enable **USB debugging**
   - Enable **Install via USB** (if available)

2. **Connect your phone:**
   - Connect your phone to your computer via USB
   - On your phone, when prompted, allow USB debugging and check "Always allow from this computer"

3. **Verify ADB connection:**
   ```bash
   adb devices
   ```
   You should see your device listed. If not, see troubleshooting below.

## Method 1: Quick Deploy (Recommended)

### From WSL (Linux)

```bash
cd /home/jani/ncs/nrf/samples/dect/dect_mac/dect_rpc_android_shell

# Build and deploy in one command
./build-and-deploy.sh Debug
```

This will:
1. Build the Debug APK
2. Check for connected devices
3. Uninstall old version (if exists)
4. Install the new APK

### From Windows Command Prompt

```powershell
cd C:\Users\jahi\ncs\nrf\samples\dect\dect_mac\dect_rpc_android_shell

# Build
.\gradlew.bat assembleDebug

# Deploy
adb install -r app\build\outputs\apk\debug\app-debug.apk
```

## Method 2: Step-by-Step Deployment

### Step 1: Build the APK

**From WSL:**
```bash
./build.sh Debug
```

**From Windows:**
```powershell
.\gradlew.bat assembleDebug
```

**From Android Studio:**
- Click **Build** → **Make Project** (Ctrl+F9)
- Or click **Run** → **Run 'app'** (Shift+F10) - this builds and deploys automatically

### Step 2: Deploy the APK

**From WSL:**
```bash
./deploy.sh Debug
```

**From Windows:**
```powershell
adb install -r app\build\outputs\apk\debug\app-debug.apk
```

**From Android Studio:**
- Click **Run** button (green play icon) or press **Shift+F10**
- Select your device from the list
- The app will build, install, and launch automatically

## Method 3: Manual APK Installation

If you prefer to install the APK manually:

1. **Build the APK** (see Step 1 above)
2. **Copy APK to your phone:**
   - **Via USB:** Copy `app/build/outputs/apk/debug/app-debug.apk` to your phone
   - **Via ADB:**
     ```bash
     adb push app/build/outputs/apk/debug/app-debug.apk /sdcard/Download/
     ```
3. **Install on phone:**
   - Open **Files** app on your phone
   - Navigate to **Downloads**
   - Tap on `app-debug.apk`
   - Tap **Install**
   - Allow installation from unknown sources if prompted

## Method 4: Using Android Studio (Easiest)

1. **Open project in Android Studio:**
   - File → Open → Select the project directory

2. **Connect your phone:**
   - Connect via USB
   - Enable USB debugging (see Prerequisites)

3. **Run the app:**
   - Click the green **Run** button (▶️)
   - Or press **Shift+F10**
   - Select your device from the dropdown
   - The app will build, install, and launch automatically

## Troubleshooting

### "No devices found" or "device offline"

1. **Check USB connection:**
   ```bash
   adb devices
   ```

2. **Restart ADB server:**
   ```bash
   adb kill-server
   adb start-server
   adb devices
   ```

3. **Check USB debugging:**
   - On your phone: Settings → Developer options → USB debugging (should be ON)
   - Try unplugging and replugging the USB cable
   - On your phone, when the "Allow USB debugging?" prompt appears, check "Always allow" and tap OK

4. **For WSL users:**
   - Make sure ADB is set up (see `ADB_SETUP.md`)
   - You may need to use ADB over network:
     ```bash
     # On Windows, find your phone's IP and port
     # Then in WSL:
     adb connect <phone-ip>:5555
     ```

### "Installation failed" or "INSTALL_FAILED"

1. **Uninstall old version first:**
   ```bash
   adb uninstall com.nordicsemi.dect_rpc_shell
   ```

2. **Try installing with flags:**
   ```bash
   adb install -r -d app/build/outputs/apk/debug/app-debug.apk
   ```
   - `-r` = Replace existing app
   - `-d` = Allow version downgrade

3. **Check if device has enough storage**

### "adb: command not found" (WSL)

Set up ADB symlink (see `ADB_SETUP.md`):
```bash
sudo ln -s /mnt/c/Users/jahi/AppData/Local/Android/Sdk/platform-tools/adb.exe /usr/local/bin/adb
```

Or use the full path in deploy.sh:
```bash
/mnt/c/Users/jahi/AppData/Local/Android/Sdk/platform-tools/adb.exe install -r app/build/outputs/apk/debug/app-debug.apk
```

### "Permission denied" when running deploy.sh

Make the script executable:
```bash
chmod +x deploy.sh
chmod +x build.sh
chmod +x build-and-deploy.sh
```

### Build fails

1. **Clean and rebuild:**
   ```bash
   ./build.sh clean
   ./build.sh Debug
   ```

2. **Check Android SDK path:**
   - Verify `local.properties` has correct `sdk.dir` path
   - Or set it: `echo "sdk.dir=/mnt/c/Users/jahi/AppData/Local/Android/Sdk" > local.properties`

3. **Sync Gradle in Android Studio:**
   - File → Sync Project with Gradle Files

## Verifying Installation

After deployment, verify the app is installed:

```bash
# List installed packages
adb shell pm list packages | grep dect

# Should show: package:com.nordicsemi.dect_rpc_shell
```

Or simply look for "Dect Shell" in your app drawer on your phone.

## Launching the App

**From ADB:**
```bash
adb shell am start -n com.nordicsemi.dect_rpc_shell/.activities.LoginActivity
```

**From phone:**
- Open app drawer
- Find "Dect Shell" app
- Tap to launch

## Updating the App

To update an existing installation:

1. **Build new version:**
   ```bash
   ./build.sh Debug
   ```

2. **Deploy (will automatically replace old version):**
   ```bash
   ./deploy.sh Debug
   ```

The `-r` flag in the deploy script automatically replaces the existing app.

## Building Release Version

For a release build (signed APK):

```bash
# Build Release
./build.sh Release

# Deploy Release
./deploy.sh Release
```

**Note:** Release builds require signing. See Android documentation for setting up signing keys.

## Additional Commands

**View app logs:**
```bash
adb logcat -s DirectShellClient:D DirectShellActivity:D
```

**Uninstall app:**
```bash
adb uninstall com.nordicsemi.dect_rpc_shell
```

**Clear app data:**
```bash
adb shell pm clear com.nordicsemi.dect_rpc_shell
```

