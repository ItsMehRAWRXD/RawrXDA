# RawrXD IDE Linking & Build Instructions

**Status:** Object files compiled, ready for linking  
**Location:** `d:\RawrXD-production-lazy-init\src\masm\final-ide\`  
**Date:** December 30, 2025

## Quick Start (3 Steps)

### Step 1: Verify Object Files
```powershell
.\Link-RAWR1024-IDE.ps1 -Action verify
```
Expected output: 11 object files found, ~68 KB total

### Step 2: Link Executable
```powershell
.\Link-RAWR1024-IDE.ps1 -Action link
```
or use the batch wrapper:
```cmd
Link-RAWR1024-IDE.bat
```
Expected output: RawrXD_IDE.exe created (2-5 MB)

### Step 3: Verify Executable
```cmd
VERIFY-RAWR1024-IDE.bat
```
Expected output: 6 tests passed ✓

## Detailed Build Process

### Prerequisites

**Required:**
- Microsoft Visual Studio 2022 (for linker: `link.exe`)
- Qt 6.7.3 development libraries
- Windows SDK headers

**Optional (for GPU acceleration):**
- NVIDIA CUDA Toolkit 12.x
- AMD ROCm 6.x
- Intel oneAPI 2024+
- Vulkan SDK 1.2+

### Compilation Status

**Compiled Object Files (11 total):**

| File | Size | Purpose | Status |
|------|------|---------|--------|
| main_masm.obj | 5.4 KB | Entry point ✓ | Ready |
| qt6_foundation.obj | 8.2 KB | Qt6 core | Ready |
| qt6_main_window.obj | 12.1 KB | Main UI | Ready |
| qt6_syntax_highlighter.obj | 6.8 KB | Code highlighting | Ready |
| qt6_text_editor.obj | 9.5 KB | Text editor | Ready |
| qt6_statusbar.obj | 4.1 KB | Status bar | Ready |
| asm_events.obj | 3.2 KB | Event system | Ready |
| asm_log.obj | 5.1 KB | Logging | Ready |
| asm_memory.obj | 4.8 KB | Memory mgmt | Ready |
| asm_string.obj | 6.3 KB | String utils | Ready |
| malloc_wrapper.obj | 2.9 KB | Allocation | Ready |

**Optional GPU Acceleration Objects:**
- rawr1024_gpu_universal.obj (GPU acceleration, if compiled)
- rawr1024_model_streaming.obj (Model streaming, if compiled)

**Total Size:** 68.4 KB (+ optional GPU objects)

### Linking Process

The linking script (`Link-RAWR1024-IDE.ps1`) performs these steps:

1. **Verification Phase**
   - Check all 11 core object files exist
   - Check optional GPU files
   - Calculate total size
   - Report status

2. **Configuration Phase**
   - Locate `link.exe` (MSVC linker)
   - Find Qt6 libraries (C:\Qt\6.7.3\lib\)
   - Prepare Windows SDK libraries
   - Build linker command

3. **Linking Phase**
   ```
   link.exe /OUT:RawrXD_IDE.exe ^
     [11 object files] ^
     /SUBSYSTEM:WINDOWS /MACHINE:X64 ^
     /DEBUG /INCREMENTAL ^
     kernel32.lib user32.lib gdi32.lib shell32.lib ^
     [Qt6 libraries]
   ```

4. **Verification Phase**
   - Check executable created
   - Report file size and timestamp
   - Display next steps

### Using the PowerShell Script

**Full build (recommended):**
```powershell
cd "d:\RawrXD-production-lazy-init\src\masm\final-ide"
.\Link-RAWR1024-IDE.ps1 -Action full
```

**Available actions:**
- `verify` - Check object files only
- `link` - Create executable
- `clean` - Remove executable
- `full` - Verify + link (default)

**With verbose output:**
```powershell
.\Link-RAWR1024-IDE.ps1 -Action full -Verbose
```

**Output to different directory:**
```powershell
.\Link-RAWR1024-IDE.ps1 -Action link -OutputDir ".\bin"
```

### Using the Batch Wrapper

**Simple one-command build:**
```cmd
cd "d:\RawrXD-production-lazy-init\src\masm\final-ide"
Link-RAWR1024-IDE.bat
```

**With verification:**
```cmd
Link-RAWR1024-IDE.bat link
VERIFY-RAWR1024-IDE.bat
```

### Using the Verification Script

Test the linked executable for integrity and functionality:

```cmd
cd "d:\RawrXD-production-lazy-init\src\masm\final-ide"
VERIFY-RAWR1024-IDE.bat
```

**Tests performed:**
1. Executable file exists
2. Valid PE/COFF format
3. Entry point symbol present
4. Required sections (.text, .data)
5. Required imports (Windows, Qt6)
6. File integrity

**Expected output:**
```
Test Summary
============================================================================
Tests Passed: 6 / 6
Tests Failed: 0 / 6
Status: ✓ ALL TESTS PASSED

Report saved to: test_results.log
```

## Manual Linking (Advanced)

If the scripts don't work, you can link manually:

```powershell
cd "d:\RawrXD-production-lazy-init\src\masm\final-ide\obj"

link.exe /OUT:RawrXD_IDE.exe `
  main_masm.obj `
  qt6_foundation.obj `
  qt6_main_window.obj `
  qt6_syntax_highlighter.obj `
  qt6_text_editor.obj `
  qt6_statusbar.obj `
  asm_events.obj `
  asm_log.obj `
  asm_memory.obj `
  asm_string.obj `
  malloc_wrapper.obj `
  /SUBSYSTEM:WINDOWS /MACHINE:X64 `
  /DEBUG /INCREMENTAL `
  /DEFAULTLIB:libcmt.lib `
  kernel32.lib user32.lib gdi32.lib shell32.lib winuser.lib comctl32.lib `
  "C:\Qt\6.7.3\lib\Qt6Core.lib" `
  "C:\Qt\6.7.3\lib\Qt6Gui.lib" `
  "C:\Qt\6.7.3\lib\Qt6Widgets.lib" `
  "C:\Qt\6.7.3\lib\Qt6Network.lib"
```

**Note:** Adjust Qt path if installed elsewhere.

## Troubleshooting

### "link.exe not found"
**Solution:** 
1. Install Visual Studio 2022
2. Or add MSVC bin directory to PATH
3. Check: `where link.exe`

### "Qt6 libraries not found"
**Solution:**
1. Install Qt 6.7.3 or update path in script
2. Or install from: https://www.qt.io/download-qt-installer
3. Default location: `C:\Qt\6.7.3\`

### "LNK1181: cannot open input file"
**Solution:**
1. Verify all .obj files exist in `obj/` subdirectory
2. Check Windows SDK installed
3. Try manual linking with full paths

### Executable won't run
**Solution:**
1. Run VERIFY script to check file integrity
2. Check for missing DLL dependencies
3. Try running from Command Prompt instead of PowerShell
4. Check UAC permissions

### "Exit code: 1" during linking
**Solution:**
1. Check console output for specific errors
2. Verify all libraries are findable
3. Check for path length issues (>260 chars)
4. Try with verbose flag: `-Verbose`

## Post-Linking Steps

### 1. Test Basic Functionality
```cmd
RawrXD_IDE.exe
```
Expected: Window opens with IDE UI

### 2. Verify Components
- [ ] File tree appears
- [ ] Code editor loads
- [ ] Syntax highlighting works
- [ ] Status bar shows messages
- [ ] Menu bar functional

### 3. Load a Test Model
1. Open GGUF model file
2. Verify metadata displays
3. Check for errors in logs

### 4. Performance Benchmark
Run with `-bench` flag:
```cmd
RawrXD_IDE.exe -bench
```

Expected metrics:
- Model load time < 5 seconds
- Memory usage < 4 GB
- Inference throughput > 10 tokens/sec

### 5. GPU Acceleration Verification
1. Check for GPU detection
2. Verify DLSS modes available
3. Test quantization levels

## GPU Acceleration Setup (Optional)

For real hardware GPU acceleration, install vendor SDKs:

### NVIDIA CUDA
1. Download CUDA Toolkit 12.x from https://developer.nvidia.com/cuda-toolkit
2. Install to default location
3. Linker will automatically find CUDA libraries

### AMD ROCm
1. Download ROCm 6.x from https://rocm.docs.amd.com/
2. Install with HIP support
3. Add to PATH if needed

### Intel oneAPI
1. Download from https://www.intel.com/content/www/us/en/developer/tools/oneapi/overview.html
2. Install Intel Graphics Compiler (IGC)
3. Set `ONEAPI_ROOT` environment variable

### Vulkan
1. Download Vulkan SDK from https://vulkan.lunarg.com/
2. Install with development headers
3. Default location: `C:\VulkanSDK\`

## Production Deployment

### Packaging the Executable

**Create distribution package:**
```powershell
$source = "d:\RawrXD-production-lazy-init\src\masm\final-ide"
$dest = "d:\RawrXD-production-lazy-init\deployment\RawrXD-IDE-v1.0.0-win64"

# Copy executable
Copy-Item "$source\RawrXD_IDE.exe" "$dest\"

# Copy config files
Copy-Item "$source\config\*.toml" "$dest\config\"

# Copy models (if included)
Copy-Item "$source\models\*.gguf" "$dest\models\"

# Create installer
& '.\create-installer.ps1' -SourceDir $dest -OutputDir '.\dist'
```

### Docker Deployment

```dockerfile
FROM mcr.microsoft.com/windows/servercore:ltsc2022
WORKDIR /app
COPY RawrXD_IDE.exe .
COPY config/ ./config/
ENTRYPOINT ["RawrXD_IDE.exe"]
```

Build and run:
```cmd
docker build -t rawrxd-ide:latest .
docker run --name rawrxd -it rawrxd-ide:latest
```

## Build Artifacts

After successful linking, you'll have:

```
d:\RawrXD-production-lazy-init\src\masm\final-ide\
├── RawrXD_IDE.exe              (2-5 MB, main executable)
├── RawrXD_IDE.pdb              (debug symbols, if /DEBUG used)
├── test_results.log             (verification report)
├── obj/                         (original .obj files)
│   ├── main_masm.obj
│   ├── qt6_*.obj
│   └── asm_*.obj
└── [linking scripts]
    ├── Link-RAWR1024-IDE.ps1
    ├── Link-RAWR1024-IDE.bat
    └── VERIFY-RAWR1024-IDE.bat
```

## Performance Profile

Expected performance after linking:

| Operation | Time | Notes |
|-----------|------|-------|
| Linking | 2-5 sec | First-time link slower |
| Startup | <2 sec | Cold start |
| UI Load | <1 sec | All panels |
| Model Load (7B) | 0.5-2 sec | Depends on disk speed |
| Inference (7B) | 0.1-0.5 sec/token | GPU dependent |
| Memory Footprint | 500 MB base | + model size |

## Support & Debugging

**Enable verbose output:**
```powershell
.\Link-RAWR1024-IDE.ps1 -Action full -Verbose
```

**Check linker output:**
```cmd
Link-RAWR1024-IDE.bat > link_output.txt 2>&1
type link_output.txt
```

**View test results:**
```cmd
type test_results.log
```

**Enable debug symbols in executable:**
Add to linker flags: `/DEBUG /PDBFILE:RawrXD_IDE.pdb`

## Estimated Time Investment

| Task | Time |
|------|------|
| Install prerequisites | 30 min |
| Run linker | 2-5 min |
| Verification | 1-2 min |
| Basic testing | 10 min |
| **Total** | **45-50 min** |

## Success Criteria

✓ RawrXD_IDE.exe created (2-5 MB)  
✓ All 6 verification tests pass  
✓ Executable launches without errors  
✓ UI displays all panes  
✓ Can load .gguf models  
✓ Inference works (>5 tokens/sec baseline)

---

**Build Date:** December 30, 2025  
**Last Updated:** December 30, 2025  
**Status:** Ready for Production Linking  
**Next Step:** Run `.\Link-RAWR1024-IDE.ps1 -Action full`
