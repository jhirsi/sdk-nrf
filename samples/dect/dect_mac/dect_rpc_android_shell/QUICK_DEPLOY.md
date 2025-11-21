# Quick Deploy Guide

## Fastest Way to Deploy

### 1. Prepare Your Phone
- Enable **USB debugging** (Settings → Developer options)
- Connect phone via USB
- Allow USB debugging when prompted

### 2. Deploy (One Command)

**From WSL:**
```bash
cd /home/jani/ncs/nrf/samples/dect/dect_mac/dect_rpc_android_shell
./build-and-deploy.sh Debug
```

**From Windows:**
```powershell
cd C:\Users\jahi\ncs\nrf\samples\dect\dect_mac\dect_rpc_android_shell
.\gradlew.bat assembleDebug
adb install -r app\build\outputs\apk\debug\app-debug.apk
```

**From Android Studio:**
- Click **Run** button (▶️) or press **Shift+F10**

### 3. Verify
- Look for "Dect Shell" app on your phone
- Or run: `adb shell pm list packages | grep dect`

## Troubleshooting

**No device found?**
```bash
adb devices
adb kill-server && adb start-server
```

**Installation failed?**
```bash
adb uninstall com.nordicsemi.dect_rpc_shell
./deploy.sh Debug
```

**ADB not found (WSL)?**
```bash
sudo ln -s /mnt/c/Users/jahi/AppData/Local/Android/Sdk/platform-tools/adb.exe /usr/local/bin/adb
```

For detailed instructions, see [DEPLOYMENT.md](DEPLOYMENT.md)

