# Kotlin Conversion Guide

This document describes the conversion from C# .NET Android to Kotlin/Java Android.

## Project Structure

The project has been converted to a standard Android Gradle project:

```
dect_rpc_android_shell/
├── app/
│   ├── build.gradle.kts          # App-level build configuration
│   ├── src/
│   │   └── main/
│   │       ├── java/com/nordicsemi/dect_rpc_shell/
│   │       │   ├── activities/   # Activity classes
│   │       │   ├── services/     # Service classes
│   │       │   ├── models/       # Data models
│   │       │   └── utils/        # Utility classes
│   │       ├── res/              # Android resources
│   │       └── AndroidManifest.xml
│   └── proguard-rules.pro
├── build.gradle.kts              # Root build file
├── settings.gradle.kts            # Project settings
└── gradle.properties             # Gradle properties
```

## Key Changes

### Build System
- **From**: .NET SDK (`DectRpcAndroidShell.csproj`)
- **To**: Gradle (`build.gradle.kts`)

### Language
- **From**: C# (.cs files)
- **To**: Kotlin (.kt files)

### Package Structure
- **From**: `DectRpcAndroidShell` namespace
- **To**: `com.nordicsemi.dect_rpc_shell` package

### Dependencies

#### C# Dependencies (Removed)
- `MQTTnet` → Replaced with Eclipse Paho MQTT
- `PeterO.Cbor` → Replaced with `co.nstant.in:cbor`
- `System.Text.Json` → Replaced with Gson
- `Xamarin.AndroidX.*` → Replaced with standard AndroidX libraries

#### Kotlin Dependencies (Added)
- `org.eclipse.paho:org.eclipse.paho.client.mqttv3` - MQTT client
- `co.nstant.in:cbor` - CBOR encoding/decoding
- `com.google.code.gson:gson` - JSON parsing
- `com.squareup.okhttp3:okhttp` - HTTP client for REST API
- Standard AndroidX libraries

## Code Conversion Examples

### Activity Conversion

**C#:**
```csharp
public class LoginActivity : Activity
{
    protected override void OnCreate(Bundle savedInstanceState)
    {
        base.OnCreate(savedInstanceState);
        // ...
    }
}
```

**Kotlin:**
```kotlin
class LoginActivity : AppCompatActivity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        // ...
    }
}
```

### Service Conversion

**C#:**
```csharp
public static class CloudAuthService
{
    public static MqttClientOptions CreateMqttOptions(string deviceId, string apiKey)
    {
        // ...
    }
}
```

**Kotlin:**
```kotlin
object CloudAuthService {
    fun createMqttOptions(deviceId: String, apiKey: String): MqttConnectOptions {
        // ...
    }
}
```

### Async/Await Conversion

**C#:**
```csharp
public static async Task<List<CloudDevice>> GetDevicesAsync(string apiKey)
{
    var response = await _httpClient.SendAsync(request);
    // ...
}
```

**Kotlin:**
```kotlin
suspend fun getDevices(apiKey: String): List<CloudDevice> = withContext(Dispatchers.IO) {
    val response = httpClient.newCall(request).execute()
    // ...
}
```

## Remaining Work

The following files still need to be converted:

1. **Activities:**
   - `LoginActivity.cs` → `LoginActivity.kt`
   - `DeviceListActivity.cs` → `DeviceListActivity.kt`
   - `TerminalActivity.cs` → `TerminalActivity.kt`
   - `DirectShellActivity.cs` → `DirectShellActivity.kt`

2. **Services:**
   - `DirectShellClient.cs` → `DirectShellClient.kt`

3. **Utils:**
   - `DectRpcShell.cs` → `DectRpcShell.kt`

4. **DECT RPC Library:**
   - The C# DECT RPC library needs to be either:
     - Converted to Kotlin/Java, OR
     - Wrapped using JNI, OR
     - Replaced with a Kotlin implementation

5. **Resources:**
   - Convert XML resources if needed
   - Update AndroidManifest.xml package references

## Next Steps

1. Complete conversion of remaining Activity classes
2. Convert DECT RPC client library to Kotlin
3. Update AndroidManifest.xml with correct package names
4. Test build and fix any compilation errors
5. Test on device

## Building

```bash
# Build debug APK
./gradlew assembleDebug

# Build release APK
./gradlew assembleRelease

# Install on connected device
./gradlew installDebug
```


