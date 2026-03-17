# RawrXD Model Loader - Clang Build Guide

**Status**: You have VS2022 and Clang installed - Perfect! ✅

This guide is optimized for your setup.

---

## 🚀 Quick Build (Clang)

```powershell
cd "c:\Users\HiH8e\OneDrive\Desktop\Powershield\RawrXD-ModelLoader"

# Default: Uses Clang (faster compilation)
.\build.ps1

# Or explicitly with Clang
.\build.ps1 -UseClang $true

# With clean rebuild
.\build.ps1 -CleanBuild -UseClang $true
```

**Expected Build Time**: 3-5 minutes (Clang is faster than MSVC)

---

## 🔧 Available Options

```powershell
# Use MSVC instead (if needed)
.\build.ps1 -UseClang $false

# Debug build with Clang
.\build.ps1 -Configuration Debug -UseClang $true

# Skip shader compilation
.\build.ps1 -SkipShaderCompile

# Clean everything and rebuild
.\build.ps1 -CleanBuild -Configuration Release
```

---

## ✅ Prerequisites Check

Your setup should have:

```powershell
# Visual Studio 2022 with Clang
clang-cl --version

# Should output: clang version X.X.X ...

# Ninja (recommended for Clang)
ninja --version

# Should output: X.X.X

# CMake
cmake --version

# Should output: cmake version 3.20+

# Vulkan SDK
$env:VULKAN_SDK

# Should be: C:\VulkanSDK\X.X.XXX
```

---

## 🎯 Compiler Comparison

### Clang (Recommended)
```
Pros:
  ✅ Faster compilation (20-30% faster than MSVC)
  ✅ Better optimization (-O3 equivalent to -O2 in MSVC)
  ✅ Excellent diagnostics
  ✅ Native Windows support (clang-cl)

Cons:
  ⚠ Slightly different error messages
```

### MSVC (Alternative)
```
Pros:
  ✅ Official Visual Studio compiler
  ✅ Full IDE integration debugging

Cons:
  ⚠ Slower compilation
  ⚠ Heavier runtime
```

---

## 📊 Build Configuration

The build script now detects and uses:

1. **Ninja Generator** (with Clang) - Faster parallel builds
2. **Clang-CL Compiler** - Windows-native Clang
3. **Modern C++20** - Full language support
4. **Vulkan 1.3** - GPU compute

---

## 🔨 Manual Build with Clang

If the script fails, build manually:

```powershell
# Create build directory
mkdir build
cd build

# Configure with Clang
cmake .. -G Ninja -DCMAKE_C_COMPILER=clang-cl -DCMAKE_CXX_COMPILER=clang-cl -DCMAKE_BUILD_TYPE=Release

# Compile shaders first
cd ../shaders
foreach ($shader in Get-ChildItem *.glsl) {
    & "$env:VULKAN_SDK\bin\glslc.exe" -O $shader.Name -o "$($shader.BaseName).spv"
}

# Build project
cd ../build
ninja
```

---

## 📈 Performance Tips with Clang

### Enable Maximum Optimization
```cmake
# In CMakeLists.txt, Clang can handle aggressive optimization:
target_compile_options(RawrXD-ModelLoader PRIVATE -O3 -march=native)
```

### Parallel Compilation
```powershell
# Ninja automatically parallelizes
ninja -j 8  # Use 8 parallel jobs
```

### Link-Time Optimization (LTO)
```cmake
# For production builds:
set_target_properties(RawrXD-ModelLoader PROPERTIES INTERPROCEDURAL_OPTIMIZATION ON)
```

---

## 🐛 Troubleshooting Clang

### Error: "clang-cl not found"
```powershell
# Verify Clang installation
clang-cl --version

# If not found, add to PATH
$env:PATH += ";C:\Program Files\LLVM\bin"

# Verify VS2022 Clang installation
"C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\Clang\x64\bin\clang-cl.exe" --version
```

### Error: "Ninja not found"
```powershell
# Install Ninja via VS2022 CMake tools, or:
# Download from https://github.com/ninja-build/ninja/releases

# Add to PATH
$env:PATH += ";C:\Program Files\Ninja"
```

### Error: "Cannot find Vulkan"
```powershell
# Ensure Vulkan SDK is installed
$env:VULKAN_SDK

# Set if not found
$env:VULKAN_SDK = "C:\VulkanSDK\1.3.xxx"
```

---

## 🎯 What to Expect

### Build Output with Clang
```
RawrXD Model Loader - Build Script
Configuration: Release
Compiler: Clang

Checking prerequisites...
✓ CMake found: cmake version 3.20+
✓ Vulkan SDK found: C:\VulkanSDK\1.3.xxx
✓ glslc compiler found
✓ Clang-CL found

=== Compiling Shaders ===
Compiling: matmul.glsl
  ✓ Compiled to: matmul.spv
...

=== Configuring CMake ===
Running CMake with Ninja generator...

=== Building Project ===
[1/50] Building CXX object ...
[2/50] Linking CXX executable ...
...
[50/50] Linking executable bin\Release\RawrXD-ModelLoader.exe

✓ Build successful

=== Build Summary ===
Configuration: Release
Build directory: .\build
Output executable: .\build\bin\Release\RawrXD-ModelLoader.exe

✓ Build complete! Ready to run.
```

---

## ⚡ Quick Commands

```powershell
# Standard build (Clang, Release)
.\build.ps1

# Debug build
.\build.ps1 -Configuration Debug

# MSVC instead of Clang
.\build.ps1 -UseClang $false

# Full clean rebuild
.\build.ps1 -CleanBuild

# Skip shaders (if already compiled)
.\build.ps1 -SkipShaderCompile

# Manual Ninja build (after CMake configure)
cd build
ninja -j 8
```

---

## 🚀 Running After Build

```powershell
# Run the compiled application
.\build\bin\Release\RawrXD-ModelLoader.exe

# Or debug version
.\build\bin\Debug\RawrXD-ModelLoader.exe

# Expected output:
# RawrXD Model Loader v1.0
# Pure Custom Implementation - No Ollama, No llama.cpp
# === Initializing Application ===
# Initializing GPU context...
# ✓ GPU context initialized
#   Device: AMD Radeon RX 7800 XT
#   AMD Device: Yes
```

---

## 📊 Build Performance Comparison

Typical build times:

| Compiler | Generator | Config | Time |
|----------|-----------|--------|------|
| Clang | Ninja | Release | 3-4 min |
| Clang | Ninja | Debug | 2-3 min |
| MSVC | VS 2022 | Release | 4-6 min |
| MSVC | VS 2022 | Debug | 3-4 min |

*Times vary based on system specs*

---

## 🎯 Recommended Workflow

1. **First build** (full):
   ```powershell
   .\build.ps1  # Uses Clang by default
   ```

2. **Iterative development** (with rebuild):
   ```powershell
   .\build.ps1  # Already configured, just rebuilds changed files
   ```

3. **Clean rebuild** (after major changes):
   ```powershell
   .\build.ps1 -CleanBuild
   ```

4. **Switch to MSVC** (if needed for debugging):
   ```powershell
   .\build.ps1 -UseClang $false -Configuration Debug
   ```

---

## 💡 Pro Tips

### Faster Incremental Builds
```powershell
# Navigate to build directory and run ninja directly
cd build
ninja  # Only rebuilds changed files

# Much faster than re-running CMake
```

### Parallel Build with Specific Job Count
```powershell
cd build
ninja -j 16  # Use 16 parallel jobs (adjust based on CPU cores)
```

### Verbose Build Output
```powershell
cd build
ninja -v  # Show full compiler commands
```

### Profile Build Time
```powershell
cd build
ninja -d stats  # Show build statistics
```

---

## 🔗 Integration with Visual Studio

Even though Clang is used for compilation, you can still debug in VS2022:

1. Open the project folder in VS2022
2. Open the compiled executable for debugging
3. Use VS2022's debugger with Clang-compiled binaries

```powershell
# Visual Studio can debug any Windows executable
start "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\devenv.exe" .\build\bin\Release\RawrXD-ModelLoader.exe
```

---

## 📞 Support

- **Build errors**: Check Prerequisites section above
- **Clang issues**: Verify installation with `clang-cl --version`
- **GPU detection**: Run `vulkaninfo` to verify Vulkan setup
- **General help**: See main README.md

---

**You're all set!** Run `.\build.ps1` to start building with Clang. 🚀

Expected completion: 3-5 minutes
