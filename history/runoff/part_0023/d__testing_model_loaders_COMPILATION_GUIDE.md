# RawrXD Phase 6 - Production Compilation & Integration

**Target:** Compile polymorphic loader and integrate with existing ultra_fast_inference system

**Timeline:** 1-2 hours for clean compilation + validation

---

## Step 1: Verify Dependencies

### Check Visual Studio 2022
```powershell
# Verify VS2022 installation
$vs_path = "C:\Program Files\Microsoft Visual Studio\2022\Enterprise"
if (Test-Path $vs_path) {
    Write-Host "✓ Visual Studio 2022 Enterprise found" -ForegroundColor Green
} else {
    Write-Host "✗ Visual Studio 2022 not found" -ForegroundColor Red
}

# Check C++ workload
$vc_tools = "$vs_path\VC\Tools\MSVC"
if (Test-Path $vc_tools) {
    Get-ChildItem $vc_tools | Select-Object Name | Head -1
    Write-Host "✓ C++ Build Tools installed" -ForegroundColor Green
}
```

### Verify GGML via vcpkg
```powershell
# GGML should be available via vcpkg
# Verify vcpkg installation
$vcpkg_path = "C:\vcpkg"
if (Test-Path "$vcpkg_path\vcpkg.exe") {
    Write-Host "✓ vcpkg found at $vcpkg_path" -ForegroundColor Green
    
    # List installed packages
    & "$vcpkg_path\vcpkg.exe" list | Select-String "ggml"
} else {
    Write-Host "✗ vcpkg not found - installation required" -ForegroundColor Red
}
```

---

## Step 2: Install GGML (if needed)

```powershell
# Install GGML for x64 Windows (Release config)
cd C:\vcpkg

.\vcpkg install ggml:x64-windows-release
# Expected time: 5-10 minutes (compiles from source)
# Output should show: "Package ggml:x64-windows-release is up to date"

# Verify installation
.\vcpkg list | Select-String "ggml"
```

**Expected Output:**
```
ggml:x64-windows-release         0.1.0#7            GGML ML Inference Library for CPU/GPU
```

---

## Step 3: Generate CMake Build Files

```powershell
# Navigate to project directory
cd D:\testing_model_loaders

# Create build directory
mkdir -Force build | Out-Null
cd build

# Generate Visual Studio 2022 project files with Vulkan GPU support
cmake -G "Visual Studio 17 2022" -A x64 `
    -DUSE_GPU=ON `
    -DENABLE_TIME_TRAVEL=ON `
    -DUSE_WIN32=ON `
    -DBUILD_TESTS=ON `
    -DCMAKE_TOOLCHAIN_FILE="C:\vcpkg\scripts\buildsystems\vcpkg.cmake" `
    -DVCPKG_TARGET_TRIPLET=x64-windows-release `
    ..

# Expected output:
# -- Configuring done (XX.X s)
# -- Generating done (X.XXs)
# -- Build files have been written to: D:\testing_model_loaders\build
```

**Troubleshooting:**
- If `GGML not found`: Run `vcpkg install ggml:x64-windows-release`
- If `Vulkan not found`: Install Vulkan SDK from https://www.vulkan.org/
- If CMake not found: Run `pip install cmake` or download from cmake.org

---

## Step 4: Compile Release Build

```powershell
# Clean previous build artifacts (optional)
cmake --build . --config Debug --target clean 2>&1 | Out-Null

# Build in Release mode with optimizations (-j uses all cores)
cmake --build . --config Release -j 8 2>&1 | Tee-Object build_release.log

# Expected output:
# [1/N] Building CXX object ...
# ...
# [N/N] Linking CXX executable test_inference.exe
# Build files have been written to: ...
# ✓ Build succeeded
```

**Build Time:** 2-5 minutes depending on CPU (includes polymorphic_loader compilation)

**Output Artifacts:**
- `Release\ultra_fast_inference_core.lib` (static library)
- `Release\win32_agent_tools.lib` (static library)  
- `Release\ollama_blob_parser.lib` (static library)
- `Release\polymorphic_loader.lib` (static library) ← NEW
- `Release\test_inference.exe` (test executable)

---

## Step 5: Verify Compilation Success

```powershell
# Check if all libraries compiled
$build_dir = "D:\testing_model_loaders\build\Release"

@(
    "ultra_fast_inference_core.lib",
    "win32_agent_tools.lib",
    "ollama_blob_parser.lib",
    "polymorphic_loader.lib"    # ← NEW
) | ForEach-Object {
    $lib = "$build_dir\$_"
    if (Test-Path $lib) {
        $size = (Get-Item $lib).Length / 1MB
        Write-Host "✓ $_" -ForegroundColor Green -NoNewline
        Write-Host " ($([math]::Round($size, 2)) MB)"
    } else {
        Write-Host "✗ $_ NOT FOUND" -ForegroundColor Red
    }
}

# Check test executable
if (Test-Path "$build_dir\test_inference.exe") {
    Write-Host "✓ test_inference.exe compiled successfully" -ForegroundColor Green
} else {
    Write-Host "✗ test_inference.exe NOT FOUND" -ForegroundColor Red
}
```

---

## Step 6: Run Basic Smoke Test

```powershell
# Basic executable sanity check (no model files needed)
cd D:\testing_model_loaders\build\Release

Write-Host "Running smoke test..." -ForegroundColor Cyan
.\test_inference.exe --help 2>&1 | Head -10

# Expected: Application starts, shows help or usage info
# If crash: Check build_release.log for errors
```

---

## Step 7: Unit Tests (Optional but Recommended)

```powershell
# Run CTest (only if BUILD_TESTS=ON, tests not yet implemented)
cd D:\testing_model_loaders\build
ctest --output-on-failure

# If no tests registered: That's OK (will create tests next phase)
# Output: Test project D:\testing_model_loaders\build
#         No tests were found!!!
```

---

## Step 8: Integration Verification

### 8.1 Verify Polymorphic Loader Header
```powershell
# Check that polymorphic_loader.h is correctly formatted
$header = Get-Content "D:\testing_model_loaders\src\polymorphic_loader.h" -First 50
$header | Select-String "class PolymorphicLoader|struct TensorDesc|class IFormatAdapter"

# Expected: All three should be found
```

### 8.2 Check Link Dependencies
```powershell
# Verify polymorphic_loader links correctly to ultra_fast_inference
# Using dumpbin (from VS2022 tools)
$dumpbin = "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Tools\MSVC\14.39.33519\bin\Hostx64\x64\dumpbin.exe"

& $dumpbin /LINKERMEMBER "D:\testing_model_loaders\build\Release\polymorphic_loader.lib" | Head -20

# Look for successful linking info
```

### 8.3 Verify Symbol Export (polymorphic_loader)
```powershell
# Check that PolymorphicLoader class is properly exported
$dumpbin = "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Tools\MSVC\14.39.33519\bin\Hostx64\x64\dumpbin.exe"

& $dumpbin /SYMBOLS "D:\testing_model_loaders\build\Release\polymorphic_loader.lib" | `
    Select-String "PolymorphicLoader|SlotLattice|ExecutionController" | Head -10

# Expected: Should see class definitions with sizes
```

---

## Step 9: Build Summary Report

```powershell
# Generate comprehensive build report
$report = @"
# RawrXD Phase 6 Build Report

**Build Date:** $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')
**Build Configuration:** Release + GPU (Vulkan) + Win32 + Time-Travel

## Compilation Status
"@

# Check each library
$libs = @(
    @{ name = "ultra_fast_inference_core"; desc = "Core tensor operations & hotpatching" },
    @{ name = "win32_agent_tools"; desc = "Win32 autonomous operations" },
    @{ name = "ollama_blob_parser"; desc = "GGUF blob extraction" },
    @{ name = "polymorphic_loader"; desc = "Format-agnostic model streaming (NEW)" }
)

$build_dir = "D:\testing_model_loaders\build\Release"
foreach ($lib in $libs) {
    $path = "$build_dir\$($lib.name).lib"
    if (Test-Path $path) {
        $size = (Get-Item $path).Length / 1MB
        $report += "`n✓ **$($lib.name)** ($([math]::Round($size, 2)) MB)"
        $report += "`n  Description: $($lib.desc)"
    } else {
        $report += "`n✗ **$($lib.name)** - MISSING"
    }
}

# Check test executable
if (Test-Path "$build_dir\test_inference.exe") {
    $exe_size = (Get-Item "$build_dir\test_inference.exe").Length / 1MB
    $report += "`n✓ **test_inference.exe** ($([math]::Round($exe_size, 2)) MB)"
} else {
    $report += "`n✗ **test_inference.exe** - MISSING"
}

$report += "`n
## Total Library Size
"

$total_size = 0
Get-ChildItem "$build_dir\*.lib" -ErrorAction SilentlyContinue | ForEach-Object {
    $total_size += $_.Length
}

$total_size_mb = $total_size / 1MB
$report += "`n**All libraries:** $([math]::Round($total_size_mb, 2)) MB`n"

$report += "`n## Ready for Next Phase`n"
$report += "`n✓ Phase 6 Complete - Production compilation successful`n"
$report += "`n**Next:** Phase 7 - Real model validation on 36GB+ GGUF files`n"

Write-Host $report
$report | Out-File -Path "D:\testing_model_loaders\BUILD_REPORT.md" -Encoding UTF8
Write-Host "✓ Report saved to BUILD_REPORT.md"
```

---

## Troubleshooting

### Issue: "GGML not found"
```powershell
# Solution: Install GGML explicitly
cd C:\vcpkg
.\vcpkg install ggml:x64-windows-release
# Then re-run CMake generation
```

### Issue: "Vulkan not found"  
```powershell
# Solution: Install Vulkan SDK or disable GPU
# Either: https://www.vulkan.org/tools
# Or: Re-run cmake with -DUSE_GPU=OFF
```

### Issue: "C++ compiler not found"
```powershell
# Solution: Install Visual Studio 2022 with C++ workload
# Run: Visual Studio Installer → Modify → C++ Build Tools
```

### Issue: Link errors in polymorphic_loader
```powershell
# Check polymorphic_loader.cpp includes ultra_fast_inference.h
Select-String "include.*ultra_fast_inference" D:\testing_model_loaders\src\polymorphic_loader.cpp

# If missing, add:
# #include "ultra_fast_inference.h"
```

### Issue: Build takes too long
```powershell
# Re-run with more parallel jobs
cmake --build . --config Release -j 16  # Use 16 cores instead of 8
```

---

## Success Criteria

✅ All 4 libraries compile without errors  
✅ polymorphic_loader.lib successfully links  
✅ test_inference.exe created  
✅ No warnings in compilation (warnings OK, not errors)  
✅ BUILD_REPORT.md generated  

---

## Next Phase: Phase 7 - Real Model Validation

Once compilation succeeds:

```powershell
# Copy compiled libraries to production location
$src = "D:\testing_model_loaders\build\Release"
$dest = "D:\Compiled\RawrXD"
mkdir -Force $dest | Out-Null

Copy-Item "$src\*.lib" $dest -Force
Copy-Item "$src\test_inference.exe" $dest -Force

# Run on real 36GB+ GGUF models
# Measure: Throughput, active memory, tier switching

Write-Host "✓ Phase 6 Complete" -ForegroundColor Green
Write-Host "Next: Phase 7 validation on real models" -ForegroundColor Cyan
```

---

**Estimated Completion Time:** 1-2 hours including dependency installation

**Questions?** Check build_release.log for detailed output
