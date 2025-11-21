# Build Fixes

## Common Build Issues and Solutions

### Issue: SDK location not found

**Error:**
```
SDK location not found. Define a valid SDK location with an ANDROID_HOME environment variable or by setting the sdk.dir path in your project's local properties file.
```

**Solution:**

The `local.properties` file needs to use WSL path format, not Windows path format.

**For WSL:**
```properties
sdk.dir=/mnt/c/Users/jahi/AppData/Local/Android/Sdk
```

**NOT:**
```properties
sdk.dir=C\:\\Users\\jahi\\AppData\\Local\\Android\\Sdk
```

**Alternative: Set ANDROID_HOME environment variable:**
```bash
export ANDROID_HOME=/mnt/c/Users/jahi/AppData/Local/Android/Sdk
export PATH=$ANDROID_HOME/tools:$PATH
export PATH=$ANDROID_HOME/platform-tools:$PATH

# Make permanent
echo 'export ANDROID_HOME=/mnt/c/Users/jahi/AppData/Local/Android/Sdk' >> ~/.bashrc
echo 'export PATH=$ANDROID_HOME/tools:$PATH' >> ~/.bashrc
echo 'export PATH=$ANDROID_HOME/platform-tools:$PATH' >> ~/.bashrc
```

### Issue: Java version not found

**Error:**
```
Could not find or load main class org.gradle.wrapper.GradleWrapperMain
```

**Solution:**
Install Java (OpenJDK 17 recommended):
```bash
sudo apt update
sudo apt install openjdk-17-jdk -y
export JAVA_HOME=/usr/lib/jvm/java-17-openjdk-amd64
export PATH=$JAVA_HOME/bin:$PATH
```

### Issue: Gradle wrapper not found

**Error:**
```
./gradlew: No such file or directory
```

**Solution:**
Copy gradlew from reference project or generate new one:
```bash
# Copy from reference project
cp /mnt/c/Users/jahi/code_wa/StudioProjects/nRFCloud_remote/gradlew ./
cp /mnt/c/Users/jahi/code_wa/StudioProjects/nRFCloud_remote/gradlew.bat ./
cp -r /mnt/c/Users/jahi/code_wa/StudioProjects/nRFCloud_remote/gradle ./
chmod +x gradlew
```

### Issue: Permission denied

**Error:**
```
Permission denied: ./gradlew
```

**Solution:**
```bash
chmod +x gradlew
```

### Issue: Build tools not found

**Error:**
```
SDK Build Tools revision X.X.X is missing
```

**Solution:**
Install required build tools via Android Studio SDK Manager or:
```bash
# Using sdkmanager (if available)
$ANDROID_HOME/cmdline-tools/latest/bin/sdkmanager "build-tools;34.0.0"
```

### Issue: Compilation errors

**Error:**
```
Unresolved reference: ...
```

**Solution:**
1. Clean build:
   ```bash
   ./gradlew clean
   ```

2. Sync dependencies:
   ```bash
   ./gradlew --refresh-dependencies
   ```

3. Rebuild:
   ```bash
   ./build.sh Debug
   ```

## Quick Fix Checklist

1. ✅ **SDK Path**: Update `local.properties` with WSL path format
2. ✅ **Java**: Install OpenJDK 17 and set JAVA_HOME
3. ✅ **Gradle**: Ensure gradlew exists and is executable
4. ✅ **Permissions**: Make scripts executable (`chmod +x *.sh`)

## Verify Setup

```bash
# Check Java
java -version
# Should show: openjdk version "17.x.x"

# Check Android SDK
echo $ANDROID_HOME
# Should show: /mnt/c/Users/jahi/AppData/Local/Android/Sdk

# Check Gradle
./gradlew --version
# Should show Gradle version and Java version

# Test build
./build.sh Debug
```

