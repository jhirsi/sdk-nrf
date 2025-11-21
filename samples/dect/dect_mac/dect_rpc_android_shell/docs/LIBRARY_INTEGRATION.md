# Integrating DectNrpRpcNetCloudLib in Android Studio

Complete guide for integrating the `DectNrpRpcNetCloudLib` library into your Android Studio project.

## Prerequisites

- **Android Studio** (latest version) - See [BUILD_INSTRUCTIONS.md](BUILD_INSTRUCTIONS.md)
- **.NET SDK 10.0 or later** (required - project targets `net10.0-android` for Android 16)
- **The `DectNrpRpcNetCloudLib` library source code** located at:
  ```
  nrf/subsys/net/lib/dect_nrp_rpc_net_cloud_lib/
  ```

## Library Location

The library source files are located at:
```
nrf/subsys/net/lib/dect_nrp_rpc_net_cloud_lib/
├── DectNrpRpcNetCloudLib.csproj  (project file)
├── DectRpcClient.cs
├── DectRpcIds.cs
├── DectDataStructures.cs
├── CborSerializer.cs
├── NrfRpcProtocol.cs
└── ... (other .cs files)
```

**Note**: The library includes a `.csproj` file, so you can use **Method 2 (Project Reference)** which is already configured in the Android project.

## Integration Methods

There are three ways to integrate the library. **Method 2 (Project Reference) is recommended** as it's already configured in the project.

Choose the method that best fits your workflow:

### Method 1: Add Library as Source Files (Recommended for Development)

This method includes the library source files directly in your Android project. Best for development and debugging.

#### Steps in Android Studio:

1. **Open your Android project** in Android Studio
   - File → Open → Select `dect_rpc_android_shell` directory

2. **Create library directory** in your project:
   ```bash
   cd /path/to/dect_rpc_android_shell
   mkdir -p DectNrpRpcNetCloudLib
   ```

3. **Copy library source files**:
   ```bash
   # From project root
   cp ../../../../subsys/net/lib/dect_nrp_rpc_net_cloud_lib/*.cs DectNrpRpcNetCloudLib/
   ```

4. **Add files to project in Android Studio**:
   - Right-click on project root → **New → Folder → Source Folder**
   - Name: `DectNrpRpcNetCloudLib`
   - Or: Drag and drop the `.cs` files into the project
   - Android Studio will automatically detect and include them

5. **Verify files are included**:
   - Check that `.cs` files appear in **Project** view
   - Files should be under `DectNrpRpcNetCloudLib/` folder

6. **Update namespace** (if needed):
   - Ensure the namespace `DectNrpRpcNetCloudLib` is used consistently
   - Check `using` statements in your code:
     ```csharp
     using DectNrpRpcNetCloudLib;
     ```

7. **Add required NuGet packages**:
   - Right-click project → **Manage NuGet Packages**
   - Or: Edit `DectRpcAndroidShell.csproj` directly
   - Required packages:
     - `MQTTnet` (>= 4.3.3.952)
     - `PeterO.Cbor` (>= 4.5.1)
     - `System.Text.Json` (>= 9.0.0) - Note: 8.0.0 has known vulnerabilities

8. **Restore NuGet packages**:
   ```bash
   dotnet restore
   ```
   Or in Android Studio: Right-click project → **Restore NuGet Packages**

9. **Build the project**:
   - **Build → Rebuild Project**
   - Or: `dotnet build`

#### Advantages:
- ✅ Easy to debug (source code available)
- ✅ Can modify library code if needed
- ✅ No external dependencies
- ✅ Fast iteration during development

#### Disadvantages:
- ❌ Library code is duplicated in project
- ❌ Updates require manual copy

### Method 2: Add as Project Reference (Recommended)

This method references the library as a separate project. Best for when you want to keep the library separate and maintain a clean project structure.

#### Steps in Android Studio:

1. **Verify library project file exists**:
   ```
   nrf/subsys/net/lib/dect_nrp_rpc_net_cloud_lib/DectNrpRpcNetCloudLib.csproj
   ```

2. **The project reference is already configured** in `DectRpcAndroidShell.csproj`:
   ```xml
   <ItemGroup>
     <ProjectReference Include="../../../../subsys/net/lib/dect_nrp_rpc_net_cloud_lib/DectNrpRpcNetCloudLib.csproj" />
   </ItemGroup>
   ```

3. **Verify path is correct**:
   - Path should be relative to `DectRpcAndroidShell.csproj`
   - Adjust path if your project structure differs

4. **Restore and build**:
   ```bash
   dotnet restore
   dotnet build
   ```

#### Advantages:
- ✅ Library stays in original location
- ✅ Easy to update (just pull latest changes)
- ✅ No code duplication
- ✅ Standard .NET project reference
- ✅ Clean project structure

#### Disadvantages:
- ❌ Requires correct relative path
- ❌ Library project must be accessible

### Method 3: Create and Use NuGet Package (Advanced)

This method requires creating a `.csproj` file for the library first, then packaging it. Best for distribution and production use.

#### Prerequisites:
- You need to create a `.csproj` file for the library first
- This is an advanced option and not recommended for initial setup

#### Steps:

1. **Create a `.csproj` file** for the library:
   ```xml
   <Project Sdk="Microsoft.NET.Sdk">
     <PropertyGroup>
       <TargetFramework>netstandard2.1</TargetFramework>
       <RootNamespace>DectNrpRpcNetCloudLib</RootNamespace>
     </PropertyGroup>
     <ItemGroup>
       <PackageReference Include="MQTTnet" Version="4.3.3.952" />
       <PackageReference Include="PeterO.Cbor" Version="4.5.1" />
       <PackageReference Include="System.Text.Json" Version="9.0.0" />
     </ItemGroup>
   </Project>
   ```

2. **Create NuGet package**:
   ```bash
   cd nrf/subsys/net/lib/dect_nrp_rpc_net_cloud_lib
   dotnet pack -c Release
   ```

3. **Use the package** (see Method 1 for simpler approach)

**Note**: For most users, **Method 1** (copy files) is recommended and simpler.

## Verifying Integration

### Check 1: Files are Included

In Android Studio:
1. Open **Project** view
2. Navigate to `DectNrpRpcNetCloudLib/`
3. Verify `.cs` files are present:
   - `DectRpcClient.cs`
   - `DectRpcIds.cs`
   - `DectDataStructures.cs`
   - `CborSerializer.cs`
   - `NrfRpcProtocol.cs`

### Check 2: Namespace is Accessible

In your code (e.g., `TerminalActivity.cs`):
```csharp
using DectNrpRpcNetCloudLib;

// Should compile without errors
var client = new DectRpcClient();
```

### Check 3: Build Succeeds

```bash
dotnet build
```

Should complete without errors related to missing types or namespaces.

### Check 4: NuGet Packages are Installed

In Android Studio:
1. **View → Tool Windows → NuGet Package Manager**
2. Verify packages are listed:
   - `MQTTnet`
   - `PeterO.Cbor`
   - `System.Text.Json`

## Troubleshooting

### "Namespace 'DectNrpRpcNetCloudLib' not found"

**Solution**:
1. Verify library files are in the project
2. Check namespace in library files matches `DectNrpRpcNetCloudLib`
3. Verify `using DectNrpRpcNetCloudLib;` is in your code
4. Rebuild project: **Build → Rebuild Project**

### "Type 'DectRpcClient' not found"

**Solution**:
1. Verify `DectRpcClient.cs` is included in project
2. Check file is not excluded from build
3. Verify namespace is correct
4. Restore NuGet packages: `dotnet restore`

### "Package 'MQTTnet' not found"

**Solution**:
1. Add NuGet package reference in `.csproj`:
   ```xml
   <PackageReference Include="MQTTnet" Version="4.3.3.952" />
   ```
2. Restore packages: `dotnet restore`
3. Or: Right-click project → **Manage NuGet Packages** → Install `MQTTnet`

### "Project reference path not found" or "DectNrpRpcNetCloudLib.csproj not found"

**Solution**:
1. Verify the library `.csproj` file exists:
   ```
   nrf/subsys/net/lib/dect_nrp_rpc_net_cloud_lib/DectNrpRpcNetCloudLib.csproj
   ```
2. Check the relative path in `DectRpcAndroidShell.csproj` is correct
3. The path should be relative to your Android project's `.csproj` file
4. If the path is incorrect, adjust it in the `<ProjectReference>` entry
5. Alternatively, use **Method 1** (copy library files) if project reference doesn't work

### Build Errors After Integration

**Solution**:
1. **Clean solution**: **Build → Clean Solution**
2. **Restore packages**: `dotnet restore`
3. **Rebuild**: **Build → Rebuild Solution**
4. Check **Build** output window for specific errors

## Android Studio Specific Notes

### File Organization

Android Studio may organize files differently than Visual Studio:
- Files are shown in **Project** view
- **Solution Explorer** equivalent is **Project** view
- Use **File → Project Structure** to manage project settings

### NuGet Package Management

In Android Studio:
1. **Right-click project** → **Manage NuGet Packages**
2. Or: Edit `.csproj` file directly
3. **Restore**: Right-click project → **Restore NuGet Packages**

### Build Configuration

1. **File → Project Structure** → **SDKs**
2. Verify Android SDK and JDK are configured
3. **Build → Make Project** to build
4. Check **Build** output for errors

## Next Steps

After successful integration:
1. See [BUILD_INSTRUCTIONS.md](BUILD_INSTRUCTIONS.md) for build steps
2. See [USAGE.md](USAGE.md) for usage guide
3. Test with a DECT NR+ device connected to nRF Cloud

## License

Copyright (c) 2025 Nordic Semiconductor ASA

SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
