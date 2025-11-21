# Build Instructions for DECT RPC Android Shell

Complete guide for setting up and building the Android application in Android Studio.

## Prerequisites

### 1. Android Studio

**Download and Install**:
- Download from: https://developer.android.com/studio
- Install Android Studio with all default components
- Minimum version: Android Studio Hedgehog (2023.1.1) or later

**Required Components** (installed via Android Studio SDK Manager):
- Android SDK Platform-Tools
- Android SDK Build-Tools (latest)
- Android SDK Platform (API Level 36) - Android 16
- Android SDK Platform (API Level 21) - minimum support

### 2. .NET SDK

**Download and Install**:
- Download from: https://dotnet.microsoft.com/download
- **Required**: .NET SDK 10.0 or later (project targets `net10.0-android`)
- **Recommended**: .NET SDK 10.0 (supports Android 16 / API 36)

**Why .NET SDK 10.0 is required**:
- The project targets `net10.0-android` framework
- .NET 10.0 includes support for Android 16 (API 36)
- This is the correct framework version for Android 16 development

**Verify Installation**:
```bash
dotnet --version
# Should show 10.0.x or higher (e.g., 10.0.100, 10.0.200)
```

### 3. Java Development Kit (JDK)

Android Studio includes a bundled JDK, but you can also use:
- OpenJDK 11 or later
- Oracle JDK 11 or later

**Verify Installation**:
```bash
java -version
```

### 4. Android Device or Emulator

**Option A: Physical Device**
- Android 5.0 (API 21) or later
- USB debugging enabled
- Developer options enabled

**Option B: Android Emulator**
- Create via Android Studio AVD Manager
- Recommended: API Level 36 (Android 16), x86_64 or arm64-v8a

## Project Setup in Android Studio

### Step 1: Open Project

1. **Launch Android Studio**
2. **File → Open** (or **Open** from welcome screen)
3. **Navigate to project directory**:
   ```
   /path/to/ncs/nrf/samples/dect/dect_mac/dect_rpc_android_shell
   ```
4. **Select the directory** and click **OK**
5. Android Studio will detect it as a .NET project

### Step 2: Integrate DECT RPC Library

**IMPORTANT**: The `DectNrpRpcNetCloudLib` library must be integrated before building.

#### Option A: Copy Library Files (Recommended for Development)

1. **Create library directory** in the project:
   ```bash
   cd /path/to/dect_rpc_android_shell
   mkdir -p DectNrpRpcNetCloudLib
   ```

2. **Copy library source files**:
   ```bash
   cp ../../../../subsys/net/lib/dect_nrp_rpc_net_cloud_lib/*.cs DectNrpRpcNetCloudLib/
   ```

3. **Add files to Android Studio project**:

   **Method A: Auto-detection (Recommended)**
   - After copying files, Android Studio should automatically detect them
   - If files appear in the **Project** view under `DectNrpRpcNetCloudLib/`, you're done!
   - If not visible, refresh: Right-click project → **Synchronize** (or press `Ctrl+F5`)

   **Method B: Manual refresh**
   - Right-click on project root in **Project** view
   - Select **Synchronize** (or press `Ctrl+F5` / `Cmd+F5` on macOS)
   - Android Studio will scan for new files and add them

   **Method C: Drag and drop**
   - Open file explorer (outside Android Studio)
   - Navigate to `DectNrpRpcNetCloudLib/` folder
   - Select all `.cs` files
   - Drag and drop them into the `DectNrpRpcNetCloudLib/` folder in Android Studio's **Project** view
   - Android Studio will prompt: "Copy files?" → Select **OK**

   **Method D: Verify in .csproj (if needed)**
   - Open `DectRpcAndroidShell.csproj` in a text editor
   - Ensure files are included (usually auto-detected, but you can manually add):
     ```xml
     <ItemGroup>
       <Compile Include="DectNrpRpcNetCloudLib\**\*.cs" />
     </ItemGroup>
     ```
   - Save and reload project in Android Studio

#### Option B: Add as Project Reference (Recommended)

1. **Verify** that `DectNrpRpcNetCloudLib.csproj` exists in the library directory:
   ```
   nrf/subsys/net/lib/dect_nrp_rpc_net_cloud_lib/DectNrpRpcNetCloudLib.csproj
   ```

2. **The project reference is already configured** in `DectRpcAndroidShell.csproj`:
   ```xml
   <ItemGroup>
     <ProjectReference Include="../../../../subsys/net/lib/dect_nrp_rpc_net_cloud_lib/DectNrpRpcNetCloudLib.csproj" />
   </ItemGroup>
   ```

3. **Restore and build**:
   ```bash
   dotnet restore
   dotnet build
   ```

**Advantages**:
- ✅ Library stays in original location
- ✅ Easy to update (just pull latest changes)
- ✅ No code duplication
- ✅ Standard .NET project reference

See [LIBRARY_INTEGRATION.md](LIBRARY_INTEGRATION.md) for more details.

### Step 3: Configure NuGet Packages

The project requires these NuGet packages (already listed in `.csproj`):

- **MQTTnet** (>= 4.3.3.952) - MQTT client library
- **PeterO.Cbor** (>= 4.5.1) - CBOR encoding/decoding
- **System.Text.Json** (>= 9.0.0) - JSON serialization (8.0.0 has known vulnerabilities)
- **Xamarin.AndroidX.AppCompat** (>= 1.6.0.1) - Android compatibility
- **Xamarin.AndroidX.RecyclerView** (>= 1.3.0.1) - RecyclerView support

**Restore Packages**:

**Method 1: Command Line**
```bash
cd /path/to/dect_rpc_android_shell
dotnet restore
```

**Method 2: Android Studio**
1. Right-click project → **Restore NuGet Packages**
2. Or: **Build → Restore NuGet Packages**

### Step 4: Configure Android SDK

**IMPORTANT**: The Android SDK path must be configured for .NET Android builds.

#### Option A: Configure in Android Studio (Recommended)

1. **File → Project Structure** (or **Ctrl+Alt+Shift+S**)
2. **SDK Location** tab:
   - Verify **Android SDK location** is set (e.g., `C:\Users\YourName\AppData\Local\Android\Sdk` on Windows, `~/Android/Sdk` on Linux/Mac)
   - Verify **JDK location** is set
3. **SDKs** tab:
   - Ensure **Android SDK Platform 36** (Android 16) is installed
   - Ensure **Android SDK Platform 21** is installed

#### Option B: Set Android SDK Path in Project File

If Android Studio doesn't detect the SDK automatically, you can set it in the project file:

1. **Find your Android SDK path**:
   - Windows: Usually `C:\Users\<YourName>\AppData\Local\Android\Sdk`
   - Linux/Mac: Usually `~/Android/Sdk` or `/home/<username>/Android/Sdk`
   - Or check in Android Studio: **File → Settings → Appearance & Behavior → System Settings → Android SDK**

2. **Add to `DectRpcAndroidShell.csproj`**:
   ```xml
   <PropertyGroup>
     <AndroidSdkDirectory>C:\Users\YourName\AppData\Local\Android\Sdk</AndroidSdkDirectory>
   </PropertyGroup>
   ```
   Replace the path with your actual Android SDK location.

#### Option C: Set Environment Variable

You can also set the `ANDROID_HOME` or `ANDROID_SDK_ROOT` environment variable:

**Windows**:
```cmd
setx ANDROID_HOME "C:\Users\YourName\AppData\Local\Android\Sdk"
```

**Linux/Mac**:
```bash
export ANDROID_HOME=$HOME/Android/Sdk
# Add to ~/.bashrc or ~/.zshrc for persistence
echo 'export ANDROID_HOME=$HOME/Android/Sdk' >> ~/.bashrc
```

### Step 5: Configure Build Settings

1. **File → Settings** (or **Android Studio → Preferences** on macOS)
2. **Build, Execution, Deployment → Build Tools → Gradle**:
   - Use Gradle from: **'gradle-wrapper.properties' file**
   - Gradle JVM: **jbr-17** (or compatible JDK)

## Building the Project

**Important**: If you previously built with an older .NET version, clean build artifacts first:
```bash
cd /path/to/dect_rpc_android_shell
dotnet clean
rm -rf obj/ bin/  # Remove generated build artifacts with old framework references
```

**After changing target framework**, you must restore packages:
```bash
dotnet restore
```

**If you get NETSDK1005 error** (assets file doesn't have target for net10.0-android):
1. Clean build artifacts: `dotnet clean && rm -rf obj/ bin/`
2. Restore packages: `dotnet restore`
3. Install Android workload (if needed): `dotnet workload install android`
4. Verify .NET SDK 10.0 is installed: `dotnet --list-sdks`
5. Build: `dotnet build`

### Method 1: Android Studio GUI

1. **Build → Clean Project** (recommended after framework change)
2. **Build → Make Project** (or **Ctrl+F9** / **Cmd+F9**)
3. Wait for build to complete
4. Check **Build** output window for errors

### Method 2: Command Line

```bash
cd /path/to/dect_rpc_android_shell
dotnet clean  # Clean old build artifacts
rm -rf obj/ bin/  # Remove generated files
dotnet restore  # Restore packages for net10.0-android
dotnet workload install android  # Install Android workload if not already installed
dotnet build -c Release
```

### Method 3: Gradle (if configured)

```bash
cd /path/to/dect_rpc_android_shell
./gradlew build
```

## Running the Application

### On Physical Device

1. **Enable USB Debugging** on Android device:
   - Settings → About Phone → Tap "Build Number" 7 times
   - Settings → Developer Options → Enable "USB Debugging"

2. **Connect device via USB**

3. **Verify device is detected**:
   ```bash
   adb devices
   ```

4. **In Android Studio**:
   - Click **Run** button (green play icon)
   - Or: **Run → Run 'app'** (or **Shift+F10** / **Ctrl+R**)
   - Select your device from the device list
   - Click **OK**

### On Android Emulator

1. **Create AVD** (Android Virtual Device):
   - **Tools → Device Manager**
   - Click **Create Device**
   - Select device (e.g., Pixel 5)
   - Select system image (API 36 - Android 16, x86_64)
   - Click **Finish**

2. **Start Emulator**:
   - Click **Play** icon next to AVD
   - Wait for emulator to boot

3. **Run Application**:
   - Click **Run** button in Android Studio
   - Select emulator from device list

### Install APK Manually

1. **Build APK**:
   ```bash
   dotnet build -c Release
   ```

2. **Find APK**:
   ```
   bin/Release/net10.0-android/com.nordicsemi.dect_rpc_shell.apk
   ```

3. **Install via ADB**:
   ```bash
   adb install -r bin/Release/net10.0-android/com.nordicsemi.dect_rpc_shell.apk
   ```

## Troubleshooting

### Build Errors

#### "Missing NuGet packages"
```bash
dotnet restore
```

#### "SDK version mismatch"
- Update to .NET SDK 10.0 or later
- Verify in `DectRpcAndroidShell.csproj`: `<TargetFramework>net10.0-android</TargetFramework>`

#### "NETSDK1005: Assets file doesn't have a target for 'net10.0-android'"
- This error occurs when restore hasn't run after changing target framework
- **Solution**:
  1. Clean build artifacts: `dotnet clean && rm -rf obj/ bin/`
  2. Restore packages: `dotnet restore`
  3. Install Android workload (if needed): `dotnet workload install android`
  4. Build: `dotnet build`
- **Note**: Ensure .NET SDK 10.0 is installed and Android workload is available

#### "NU1903: Package 'System.Text.Json' 8.0.0 has a known high severity vulnerability"
- **Solution**: Update `System.Text.Json` to version 9.0.0 or later
- The project file already uses version 9.0.0
- After updating, run: `dotnet restore && dotnet build`
- **Security**: Always keep packages updated to latest secure versions
- **Vulnerability**: https://github.com/advisories/GHSA-hh2w-p6rv-4g7w

#### "Android SDK missing"
1. **File → Settings → Appearance & Behavior → System Settings → Android SDK**
2. Install missing SDK components
3. Verify **SDK Location** is correct

#### "XA5300: The Android SDK directory could not be found"
- This error occurs when .NET build tools cannot locate the Android SDK
- **Solution Option 1** (Recommended): Set in project file
  1. Find your Android SDK path (check Android Studio: **File → Settings → Android SDK**)
  2. Add to `DectRpcAndroidShell.csproj`:
     ```xml
     <PropertyGroup>
       <AndroidSdkDirectory>C:\Users\YourName\AppData\Local\Android\Sdk</AndroidSdkDirectory>
     </PropertyGroup>
     ```
     Replace with your actual SDK path
  3. Restore and build: `dotnet restore && dotnet build`

- **Solution Option 2**: Set environment variable
  - **Windows**: `setx ANDROID_HOME "C:\Users\YourName\AppData\Local\Android\Sdk"`
  - **Linux/Mac**: `export ANDROID_HOME=$HOME/Android/Sdk`
  - Restart terminal/IDE after setting

- **Solution Option 3**: Install Android SDK via .NET workload
  ```bash
  dotnet workload install android
  ```

#### "Library not found"
- Ensure `DectNrpRpcNetCloudLib` files are copied to project
- Verify files are included in `.csproj`:
  ```xml
  <ItemGroup>
    <Compile Include="DectNrpRpcNetCloudLib/**/*.cs" />
  </ItemGroup>
  ```

#### "Namespace not found"
- Verify library files are in correct namespace: `DectNrpRpcNetCloudLib`
- Check `using` statements in source files

### Runtime Errors

#### "Connection failed"
- Check device ID and API key
- Verify network connectivity
- Check nRF Cloud service status

#### "MQTT connection timeout"
- Verify device is online in nRF Cloud portal
- Check firewall/network restrictions
- Verify MQTT broker: `mqtt.nrfcloud.com:8883`

#### "Permission denied"
- Check `AndroidManifest.xml` has required permissions:
  ```xml
  <uses-permission android:name="android.permission.INTERNET" />
  <uses-permission android:name="android.permission.ACCESS_NETWORK_STATE" />
  ```

### Debugging

#### Enable Debug Logging

Add debug statements in code:
```csharp
System.Diagnostics.Debug.WriteLine("Debug message");
```

#### View Logs

**Via ADB**:
```bash
adb logcat | grep DectRpcShell
```

**Via Android Studio**:
1. **View → Tool Windows → Logcat**
2. Filter by package: `com.nordicsemi.dect_rpc_shell`

#### Debug Configuration

1. **Run → Edit Configurations**
2. Select **app** configuration
3. **Debugger**: Select **.NET**
4. Set breakpoints in code
5. **Run → Debug 'app'** (or **Shift+F9**)

## Project Configuration

### AndroidManifest.xml

Key settings:
- **Package**: `com.nordicsemi.dect_rpc_shell`
- **Min SDK**: 21 (Android 5.0)
- **Target SDK**: 36 (Android 16)
- **Permissions**: INTERNET, ACCESS_NETWORK_STATE

### DectRpcAndroidShell.csproj

Key settings:
- **Target Framework**: `net10.0-android`
- **Min SDK Version**: 21
- **Target SDK Version**: 36 (Android 16)
- **Supported ABIs**: armeabi-v7a, arm64-v8a, x86, x86_64

## Next Steps

After building successfully:
1. See [USAGE.md](USAGE.md) for usage instructions
2. See [APP_FLOW.md](APP_FLOW.md) for application flow details
3. Test with a DECT NR+ device connected to nRF Cloud

## License

Copyright (c) 2025 Nordic Semiconductor ASA

SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
