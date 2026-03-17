# 🔍 Smoke Test Results - RawrXD-AgenticIDE

## Test Status: ⚠️ RUNTIME INITIALIZATION FAILURE

**Date**: December 16, 2025  
**Executable**: `D:\temp\RawrXD-agentic-ide-production\RawrXD-ModelLoader\build\bin\Release\RawrXD-AgenticIDE.exe`  
**Exit Code**: `-1073741701` (0xC000007B - STATUS_INVALID_IMAGE_FORMAT)

---

## Error Analysis

### Error Code: 0xC000007B
**Meaning**: STATUS_INVALID_IMAGE_FORMAT  
**Common Causes**:
1. Missing DLL dependencies
2. 32-bit/64-bit architecture mismatch
3. Corrupted or incompatible DLL versions
4. Missing Visual C++ Redistributables
5. Missing Vulkan runtime DLL (vulkan-1.dll)

---

## Dependencies Status

### ✅ Qt 6.7.3 Libraries (Present)
- Qt6Core.dll
- Qt6Gui.dll
- Qt6Widgets.dll
- Qt6Network.dll
- Qt6Sql.dll
- Qt6Concurrent.dll *(manually added)*
- Qt6Charts.dll
- Qt6OpenGL.dll
- Qt6OpenGLWidgets.dll
- Qt6Pdf.dll
- Qt6Svg.dll

### ✅ Qt Plugins (Deployed)
- **Platforms**: qwindows.dll
- **Image Formats**: qjpeg, qpng, qsvg, qtiff, qwebp
- **SQL Drivers**: qsqlite, qsqlodbc, qsqlpsql
- **Styles**: qmodernwindowsstyle
- **TLS**: qopensslbackend, qschannelbackend

### ✅ MSVC Runtime Libraries (Copied from System32)
- msvcp140.dll
- vcruntime140.dll
- vcruntime140_1.dll (if available)
- msvcp140_1.dll (if available)
- msvcp140_2.dll (if available)

### ⚠️ DirectX Shader Compiler (Present)
- dxcompiler.dll
- dxil.dll

### ❓ Vulkan Runtime (Status Unknown)
- **vulkan-1.dll** - Not verified in Release folder
- **Required for**: GGML Vulkan backend GPU acceleration
- **Location**: Should be in Vulkan SDK or system PATH

### ✅ GGML Libraries (Statically Linked)
- ggml-base.lib - Linked into executable
- ggml-cpu.lib - Linked into executable
- ggml-vulkan.lib - Linked into executable
- brutal_gzip.lib - Linked into executable
- quant_utils.lib - Linked into executable

---

## Diagnostic Steps Performed

1. ✅ Verified executable exists and is current build
2. ✅ Checked Qt DLL presence (all present)
3. ✅ Added missing Qt6Concurrent.dll from Qt MSVC2022_64 bin
4. ✅ Copied MSVC runtime DLLs from System32
5. ❌ Smoke test still fails with 0xC000007B

---

## Likely Root Causes

### 1. Missing Vulkan Runtime DLL
**Symptom**: Executable links against Vulkan but vulkan-1.dll not in PATH or Release folder  
**Solution**: 
```powershell
# Find Vulkan DLL
Get-ChildItem -Path "C:\VulkanSDK" -Filter "vulkan-1.dll" -Recurse

# Copy to Release folder
Copy-Item "<path>\vulkan-1.dll" "d:\temp\RawrXD-agentic-ide-production\RawrXD-ModelLoader\build\bin\Release"
```

### 2. Architecture Mismatch (32-bit DLL loaded by 64-bit exe)
**Symptom**: Executable is 64-bit but a dependency DLL is 32-bit  
**Verification**:
```powershell
# Check executable architecture
dumpbin /headers RawrXD-AgenticIDE.exe | Select-String "machine"

# Check DLL architecture
dumpbin /headers Qt6Core.dll | Select-String "machine"
```

### 3. Missing Visual C++ 2022 Redistributables
**Symptom**: System doesn't have official MSVC 2022 redistributables installed  
**Solution**: Install from Microsoft:
- Download: https://aka.ms/vs/17/release/vc_redist.x64.exe
- Run installer
- Retest executable

---

## Recommended Next Steps

### Immediate Actions (Priority Order)

1. **Install Vulkan SDK** (if not present):
   ```
   Download from: https://vulkan.lunarg.com/sdk/home#windows
   Install to default location
   ```

2. **Verify Vulkan-1.dll**:
   ```powershell
   # Check if vulkan-1.dll is in system PATH
   where.exe vulkan-1.dll
   
   # If not found, copy from Vulkan SDK
   Copy-Item "C:\VulkanSDK\1.4.328.0\Bin\vulkan-1.dll" "d:\temp\RawrXD-agentic-ide-production\RawrXD-ModelLoader\build\bin\Release"
   ```

3. **Install MSVC 2022 Redistributables**:
   ```powershell
   # Download and run
   Start-Process "https://aka.ms/vs/17/release/vc_redist.x64.exe"
   ```

4. **Use Dependency Walker** (if available):
   ```
   Download: http://www.dependencywalker.com/
   Open RawrXD-AgenticIDE.exe in Dependency Walker
   Check for red-highlighted missing DLLs
   ```

5. **Check Windows Event Viewer**:
   ```
   Open: Event Viewer → Windows Logs → Application
   Look for: Application Error events from RawrXD-AgenticIDE.exe
   Check: Faulting module name for specific DLL causing crash
   ```

### Alternative: Disable Vulkan Backend

If Vulkan is not critical, rebuild without Vulkan support:

```powershell
cd "d:\temp\RawrXD-agentic-ide-production\RawrXD-ModelLoader"

# Reconfigure without Vulkan
cmake -S . -B build -DGGML_VULKAN=OFF -DCMAKE_BUILD_TYPE=Release

# Rebuild
cmake --build build --config Release --target RawrXD-AgenticIDE

# Test
cd build\bin\Release
.\RawrXD-AgenticIDE.exe
```

---

## Build Verification (✅ Complete)

- ✅ Compilation successful (all source files compiled without errors)
- ✅ Linking successful (no unresolved externals)
- ✅ Qt deployment successful (all DLLs and plugins copied)
- ✅ Executable file created (Release\RawrXD-AgenticIDE.exe)
- ❌ **Runtime initialization failing** (DLL dependency issue)

---

## Workaround: Manual Testing

Until runtime dependencies resolved, test core libraries separately:

```powershell
# Test quantization engine (works)
cd "d:\temp\RawrXD-agentic-ide-production\RawrXD-ModelLoader\build\bin\Release"
.\quant_engine_test.exe
```

---

## Summary

**Build Status**: ✅ **COMPLETE**  
**Deployment Status**: ⚠️ **INCOMPLETE** (missing runtime DLLs)  
**Executable Status**: ❌ **NON-FUNCTIONAL** (0xC000007B error)  

**Primary Blocker**: Missing runtime dependency - most likely **vulkan-1.dll**

**Recommended Action**: Copy vulkan-1.dll from Vulkan SDK to Release folder, or install Vulkan runtime redistributable.

---

## Contact Information

**Issue**: Runtime dependency missing (STATUS_INVALID_IMAGE_FORMAT)  
**Next Step**: Resolve Vulkan DLL dependency or rebuild without Vulkan support  
**Documentation**: See LOADER_BUILD_COMPLETE.md for build details
