# Java (OpenJDK) Installation for WSL

## Required Java Version

Based on the project configuration:
- **Minimum**: Java 11 (OpenJDK 11)
- **Recommended**: Java 17 (OpenJDK 17) - LTS version
- **Also works**: Java 21 (OpenJDK 21) - Latest LTS

The project is configured with:
```kotlin
sourceCompatibility = JavaVersion.VERSION_11
targetCompatibility = JavaVersion.VERSION_11
jvmTarget = "11"
```

## Installation Options

### Option 1: OpenJDK 17 (Recommended) ⭐

**Why Java 17?**
- LTS (Long Term Support) version
- Works perfectly with Java 11 target
- Better performance and features
- Recommended by Android Gradle Plugin

**Install on Ubuntu/Debian WSL:**
```bash
# Update package list
sudo apt update

# Install OpenJDK 17
sudo apt install openjdk-17-jdk -y

# Verify installation
java -version
# Should show: openjdk version "17.x.x"

# Set JAVA_HOME (add to ~/.bashrc or ~/.zshrc)
echo 'export JAVA_HOME=/usr/lib/jvm/java-17-openjdk-amd64' >> ~/.bashrc
echo 'export PATH=$JAVA_HOME/bin:$PATH' >> ~/.bashrc
source ~/.bashrc

# Verify JAVA_HOME
echo $JAVA_HOME
```

### Option 2: OpenJDK 11 (Minimum Required)

**Install on Ubuntu/Debian WSL:**
```bash
sudo apt update
sudo apt install openjdk-11-jdk -y

# Verify
java -version

# Set JAVA_HOME
echo 'export JAVA_HOME=/usr/lib/jvm/java-11-openjdk-amd64' >> ~/.bashrc
echo 'export PATH=$JAVA_HOME/bin:$PATH' >> ~/.bashrc
source ~/.bashrc
```

### Option 3: OpenJDK 21 (Latest LTS)

**Install on Ubuntu/Debian WSL:**
```bash
sudo apt update
sudo apt install openjdk-21-jdk -y

# Verify
java -version

# Set JAVA_HOME
echo 'export JAVA_HOME=/usr/lib/jvm/java-21-openjdk-amd64' >> ~/.bashrc
echo 'export PATH=$JAVA_HOME/bin:$PATH' >> ~/.bashrc
source ~/.bashrc
```

## Verify Installation

After installation, verify everything works:

```bash
# Check Java version
java -version

# Check Java compiler
javac -version

# Check JAVA_HOME
echo $JAVA_HOME

# Test Gradle can find Java
cd /home/jani/ncs/nrf/samples/dect/dect_mac/dect_rpc_android_shell
./gradlew --version
# Should show Java version in output
```

## Multiple Java Versions (Optional)

If you need multiple Java versions, use `update-alternatives`:

```bash
# Install multiple versions
sudo apt install openjdk-11-jdk openjdk-17-jdk -y

# Configure alternatives
sudo update-alternatives --config java
sudo update-alternatives --config javac

# Select the version you want (usually 17)
```

## Gradle Java Version

Gradle will use the Java version specified by:
1. `JAVA_HOME` environment variable (highest priority)
2. `java` command in PATH
3. Gradle wrapper's Java toolchain (if configured)

## Troubleshooting

### "java: command not found"
```bash
# Install Java
sudo apt install openjdk-17-jdk -y

# Add to PATH
export PATH=/usr/lib/jvm/java-17-openjdk-amd64/bin:$PATH
```

### "JAVA_HOME not set"
```bash
# Find Java installation
sudo update-alternatives --list java
# Output: /usr/lib/jvm/java-17-openjdk-amd64/bin/java

# Set JAVA_HOME (use the directory without /bin/java)
export JAVA_HOME=/usr/lib/jvm/java-17-openjdk-amd64
echo 'export JAVA_HOME=/usr/lib/jvm/java-17-openjdk-amd64' >> ~/.bashrc
```

### Gradle uses wrong Java version
```bash
# Check what Java Gradle sees
./gradlew --version

# Set JAVA_HOME explicitly
export JAVA_HOME=/usr/lib/jvm/java-17-openjdk-amd64
./gradlew --version
```

### "Unsupported class file major version"
This means you're using a Java version that's too new for Gradle, or Gradle is using a Java version that's too old. Use Java 17.

## Quick Install Script

Copy and paste this to install OpenJDK 17:

```bash
#!/bin/bash
sudo apt update
sudo apt install openjdk-17-jdk -y
export JAVA_HOME=/usr/lib/jvm/java-17-openjdk-amd64
export PATH=$JAVA_HOME/bin:$PATH
echo 'export JAVA_HOME=/usr/lib/jvm/java-17-openjdk-amd64' >> ~/.bashrc
echo 'export PATH=$JAVA_HOME/bin:$PATH' >> ~/.bashrc
java -version
```

## Summary

**Recommended**: Install **OpenJDK 17** (LTS, works great with Android development)

```bash
sudo apt update && sudo apt install openjdk-17-jdk -y
export JAVA_HOME=/usr/lib/jvm/java-17-openjdk-amd64
export PATH=$JAVA_HOME/bin:$PATH
```

Then verify:
```bash
java -version
cd /home/jani/ncs/nrf/samples/dect/dect_mac/dect_rpc_android_shell
./gradlew --version
```

