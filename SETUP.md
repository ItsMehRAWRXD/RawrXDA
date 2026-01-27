# RawrXD Model Loader - Setup & Installation Guide

## Prerequisites Installation

Before building RawrXD Model Loader, ensure you have all required tools installed.

### 1. Visual Studio 2022 Installation

**Download**: https://visualstudio.microsoft.com/vs/

**Installation Steps**:
1. Run the Visual Studio installer
2. Select **Desktop development with C++** workload
3. In the Installation details panel, verify:
   - ✓ MSVC v143 - VS 2022 C++ x64/x86 build tools (latest)
   - ✓ CMake tools for Windows
   - ✓ C++ Clang tools for Windows
4. Complete installation (~20-30 GB)

**Verify Installation**:
```powershell
# Open Developer Command Prompt for VS 2022
# Test compilation:
cl.exe /?
```

### 2. CMake Installation

**Download**: https://cmake.org/download/

**Installation Steps**:
1. Download Windows x64 installer (`.msi`)
2. Run installer
3. Check **Add CMake to system PATH**
4. Complete installation

**Verify Installation**:
```powershell
cmake --version
# Output: cmake version 3.20+ (or higher)
```

### 3. Vulkan SDK Installation

**Download**: https://vulkan.lunarg.com/sdk/home

**Installation Steps**:
1. Download Latest Vulkan SDK for Windows
2. Run installer
3. Follow defaults for installation location
4. **Important**: Check **Environment Variables** during installation
5. Installer should set `VULKAN_SDK` environment variable automatically
6. Restart PowerShell/CMD for environment changes to take effect

**Verify Installation**:
```powershell
# Check environment variable
$env:VULKAN_SDK

# Should output: C:\VulkanSDK\<version>

# Test glslc compiler
& "$env:VULKAN_SDK\bin\glslc.exe" --version
# Output: shaderc version 2024.0.1 (or higher)

# Test vulkaninfo
& "$env:VULKAN_SDK\bin\vulkaninfo.exe" | head -20
```

## Building RawrXD Model Loader

### Step 1: Verify All Prerequisites

```powershell
# Open new PowerShell and verify all tools
$checks = @(
    @{Name="Visual Studio 2022"; Cmd={cl.exe /?} },
    @{Name="CMake"; Cmd={cmake --version} },
    @{Name="Vulkan SDK"; Cmd={& "$env:VULKAN_SDK\bin\vulkaninfo.exe" | Select-Object -First 1} },
    @{Name="GLSLC"; Cmd={& "$env:VULKAN_SDK\bin\glslc.exe" --version} }
)

foreach ($check in $checks) {
    try {
        & $check.Cmd | Out-Null
        Write-Host "✓ $($check.Name)" -ForegroundColor Green
    } catch {
        Write-Host "✗ $($check.Name)" -ForegroundColor Red
    }
}
```

### Step 2: Run Build Script

```powershell
# Navigate to project directory
cd "c:\Users\HiH8e\OneDrive\Desktop\Powershield\RawrXD-ModelLoader"

# Run build script
.\build.ps1

# Or with options:
.\build.ps1 -Configuration Release -CleanBuild
```

### Step 3: Build Manually (If Script Fails)

```powershell
# Create build directory
mkdir build
cd build

# Configure CMake
cmake .. -G "Visual Studio 17 2022" -DCMAKE_BUILD_TYPE=Release

# Compile shaders first
cd ../shaders
$env:VULKAN_SDK\bin\glslc.exe matmul.glsl -o matmul.spv
$env:VULKAN_SDK\bin\glslc.exe attention.glsl -o attention.spv
$env:VULKAN_SDK\bin\glslc.exe rope.glsl -o rope.spv
$env:VULKAN_SDK\bin\glslc.exe rmsnorm.glsl -o rmsnorm.spv
$env:VULKAN_SDK\bin\glslc.exe softmax.glsl -o softmax.spv
$env:VULKAN_SDK\bin\glslc.exe silu.glsl -o silu.spv
$env:VULKAN_SDK\bin\glslc.exe dequant.glsl -o dequant.spv

# Return to build directory and compile
cd ../build
cmake --build . --config Release --verbose

# Output executable
# .\bin\Release\RawrXD-ModelLoader.exe
```

## Troubleshooting Installation

### Issue: VULKAN_SDK Not Found

**Solution**:
```powershell
# Set manually
$env:VULKAN_SDK = "C:\VulkanSDK\1.3.290"  # Adjust version number

# Add to system environment (permanent)
[Environment]::SetEnvironmentVariable("VULKAN_SDK", "C:\VulkanSDK\1.3.290", "User")

# Verify
$env:VULKAN_SDK
```

### Issue: CMake Not Found

**Solution**:
```powershell
# Check if in PATH
where cmake

# If not found, add manually
$cmake_path = "C:\Program Files\CMake\bin"
$env:PATH = "$cmake_path;$env:PATH"

# Add to system PATH (permanent)
[Environment]::SetEnvironmentVariable("PATH", "$env:PATH;$cmake_path", "User")
```

### Issue: Visual Studio Not Found

**Solution**:
```powershell
# Find VS installation
$vs_path = "C:\Program Files\Microsoft Visual Studio\2022"
Get-ChildItem $vs_path  # Should show: Community, Professional, Enterprise

# Use Community if available
cd "$vs_path\Community\VC\Auxiliary\Build"
.\vcvars64.bat  # Initialize build environment

# Verify compiler
cl.exe /?
```

### Issue: glslc Shader Compilation Fails

**Solution**:
```powershell
# Test glslc directly
cd "c:\Users\HiH8e\OneDrive\Desktop\Powershield\RawrXD-ModelLoader\shaders"

# Try compiling one shader
& "$env:VULKAN_SDK\bin\glslc.exe" -fshader-stage=compute matmul.glsl -o matmul.spv -v

# Check errors in detail
if ($LASTEXITCODE -ne 0) {
    Write-Host "Compilation failed with error code: $LASTEXITCODE"
}
```

### Issue: CMake Configuration Fails

**Solution**:
```powershell
# Clear CMake cache and reconfigure
cd c:\Users\HiH8e\OneDrive\Desktop\Powershield\RawrXD-ModelLoader\build
rm -r CMakeFiles CMakeCache.txt

# Try configuration again with verbose output
cmake .. -G "Visual Studio 17 2022" -DCMAKE_BUILD_TYPE=Release --debug-output

# Check detailed error messages
cmake --build . --config Release -- /v:detailed
```

## Running the Application

### First Run

```powershell
# Navigate to executable
cd "c:\Users\HiH8e\OneDrive\Desktop\Powershield\RawrXD-ModelLoader\build\bin\Release"

# Run application
.\RawrXD-ModelLoader.exe

# Should output:
# RawrXD Model Loader v1.0
# Pure Custom Implementation - No Ollama, No llama.cpp
# 
# === Initializing Application ===
# Initializing GPU context...
# ✓ GPU context initialized
#   Device: AMD Radeon RX 7800 XT (or similar)
#   AMD Device: Yes
#   Compute Queue Family: 0
# ...
```

### Verify GPU Detection

```powershell
# Check if AMD 7800XT is detected
.\RawrXD-ModelLoader.exe | Select-String -Pattern "AMD", "Device"

# Expected output includes:
# Device: AMD Radeon RX 7800 XT
# AMD Device: Yes
```

### Testing API Server

```powershell
# In another PowerShell window, test API endpoints
curl -X GET http://localhost:11434/api/tags

# Should return:
# {"models":[]}

# Or with detailed output:
Invoke-RestMethod -Uri "http://localhost:11434/api/tags" -Method GET | ConvertTo-Json
```

## Next Steps

1. **Load a Model**:
   ```powershell
   # Place GGUF file in models directory
   $models_dir = "$env:USERPROFILE\RawrXD\models"
   # Copy BigDaddyG-Q2_K-PRUNED-16GB.gguf to this directory
   
   # Or download via API:
   Invoke-RestMethod -Uri "http://localhost:11434/api/pull" `
     -Method POST `
     -Body @{name="TheBloke/BigDaddyG-7B-Q2_K-GGUF"} -ContentType "application/json"
   ```

2. **Run Inference**:
   ```powershell
   $prompt = "Once upon a time"
   Invoke-RestMethod -Uri "http://localhost:11434/api/generate" `
     -Method POST `
     -Body @{model="bigdaddyg-q2k"; prompt=$prompt} -ContentType "application/json"
   ```

3. **Integrate with RawrXD IDE**:
   - Update RawrXD.ps1 to use localhost:11434
   - Query models via `/api/tags`
   - Send chat messages to `/v1/chat/completions`

## Development Build for Debugging

```powershell
# Configure for Debug build
cd c:\Users\HiH8e\OneDrive\Desktop\Powershield\RawrXD-ModelLoader\build
cmake .. -G "Visual Studio 17 2022" -DCMAKE_BUILD_TYPE=Debug

# Build with debug symbols
cmake --build . --config Debug

# Run in Visual Studio debugger
"c:\Users\HiH8e\OneDrive\Desktop\Powershield\RawrXD-ModelLoader\build\bin\Debug\RawrXD-ModelLoader.exe"
```

## Performance Optimization

For optimal performance on AMD 7800XT:

1. **Release Build**: Always use Release configuration in production
2. **Shader Optimization**: Use `-O` flag with glslc
   ```powershell
   & "$env:VULKAN_SDK\bin\glslc.exe" -O matmul.glsl -o matmul.spv
   ```
3. **Wave Size**: AMD RDNA3 optimized for wave64 (default in our shaders)
4. **Local Work Size**: Keep 16x16 or 32x1 for optimal occupancy

## Support

If you encounter issues:

1. Check the troubleshooting section above
2. Verify all prerequisites are installed with correct versions
3. Check error messages in detailed output (`--verbose` flags)
4. Ensure GPU drivers are up to date
5. Check Vulkan validation layers for GPU compatibility

## References

- Vulkan SDK: https://vulkan.lunarg.com/
- CMake: https://cmake.org/
- Visual Studio: https://visualstudio.microsoft.com/
- Vulkan Documentation: https://www.khronos.org/vulkan/
- AMD GPU Documentation: https://gpuopen.com/
