# PHASE 2: MODEL LOADER - BUILD GUIDE

## System Requirements

- **OS**: Windows 10 x64 or later
- **Architecture**: x64 Intel/AMD
- **Compiler**: Microsoft Visual Studio 2022 (MSVC 17.x)
- **RAM**: 16GB recommended (8GB minimum for testing)
- **Disk**: 50GB free (for model storage)

## Build System Setup

### Prerequisites

1. **Visual Studio 2022 with C++ workload**
   ```bash
   # If not installed, use Visual Studio Installer
   # Select: Desktop development with C++
   # Include: MASM assembler (checked by default)
   ```

2. **CMake 3.20+**
   ```bash
   cmake --version  # Should show 3.20 or later
   ```

3. **Phase 1 Foundation** (must be built first)
   ```bash
   cd D:\rawrxd
   .\scripts\Build-Phase1.ps1 -Release
   ```

## Build Methods

### Method 1: PowerShell Build Script (Recommended)

```powershell
cd D:\rawrxd
.\scripts\Build-Phase2.ps1 -Release
```

**Parameters:**
- `-Release` - Optimized build (default)
- `-Debug` - Debug symbols, no optimization
- `-Rebuild` - Clean rebuild
- `-NoTest` - Skip running tests

**Output:**
```
D:\rawrxd\build\phase2\
├── Phase2_Master.obj
├── Phase2_Foundation.obj
├── Phase2.lib
└── Phase2.dll (if DLL enabled)
```

### Method 2: CMake (Multi-platform)

```bash
cd D:\rawrxd
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release --target Phase2
```

**Targets:**
```bash
cmake --build build --target Phase2      # Build library
cmake --build build --target Phase2Test  # Build tests
cmake --build build --target Phase2Docs  # Generate docs
```

### Method 3: Manual (MASM + MSVC)

```bash
cd D:\rawrxd\src\loader

# Step 1: Assemble Phase 2 Master
ml64.exe /c /O2 /Zi /W3 /nologo Phase2_Master.asm
# Output: Phase2_Master.obj (60KB)

# Step 2: Compile C++ implementation
cl.exe /c /O2 /W4 /Zi /I..\..\include Phase2_Foundation.cpp
# Output: Phase2_Foundation.obj (15KB)

# Step 3: Create static library
lib /OUT:Phase2.lib Phase2_Master.obj Phase2_Foundation.obj
# Output: Phase2.lib (75KB)

# Step 4: Or create DLL
link /DLL /OUT:Phase2.dll /OPT:REF /OPT:ICF ^
    Phase2_Master.obj Phase2_Foundation.obj ^
    ..\..\..\VS2022Enterprise\VC\lib\x64\kernel32.lib ^
    ..\..\..\VS2022Enterprise\VC\lib\x64\user32.lib
# Output: Phase2.dll (50KB), Phase2.lib (10KB import)
```

## CMakeLists.txt Configuration

```cmake
# D:\rawrxd\CMakeLists.txt (relevant section)

project(RawrXD)

# Find MASM compiler
enable_language(ASM_MASM)
set(CMAKE_ASM_MASM_COMPILER ml64.exe)
set(CMAKE_ASM_MASM_COMPILE_OBJECT "<CMAKE_ASM_MASM_COMPILER> <DEFINES> <INCLUDES> <FLAGS> /c <SOURCE> /Fo<OBJECT>")

# Phase 2 Library
add_library(Phase2
    src/loader/Phase2_Master.asm
    src/loader/Phase2_Foundation.cpp
)

target_include_directories(Phase2 PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(Phase2
    Phase1  # Depends on Phase 1 Foundation
)

# Phase 2 Tests
add_executable(Phase2Test test/Phase2_Test.cpp)
target_link_libraries(Phase2Test Phase2 Phase1)

# Set MASM properties
set_source_files_properties(
    src/loader/Phase2_Master.asm
    PROPERTIES
    LANGUAGE ASM_MASM
    COMPILE_FLAGS "/O2 /Zi /W3"
)
```

## Build Configuration Details

### Optimization Flags

**Release Build (-O2):**
```asm
ml64.exe /c /O2 /Zi /W3 /nologo Phase2_Master.asm
```
- `/O2` - Maximum optimization
- `/Zi` - Generate debug symbols (PDB)
- `/W3` - Warning level 3
- `/nologo` - No assembly header

**Debug Build:**
```asm
ml64.exe /c /Z7 /W4 Phase2_Master.asm
```
- `/Z7` - Debug info in object file
- `/W4` - Maximum warnings

### MSVC Flags

**Release:**
```cmd
cl.exe /c /O2 /W4 /Zi /Ob2 /DNDEBUG Phase2_Foundation.cpp
```

**Debug:**
```cmd
cl.exe /c /Od /W4 /Zi /D_DEBUG Phase2_Foundation.cpp
```

## Linking Dependencies

### Static Link (Phase2.lib)

```cmd
link /LIB /OUT:Phase2.lib ^
    Phase2_Master.obj ^
    Phase2_Foundation.obj
```

**Consumers link with:**
```cpp
#pragma comment(lib, "Phase2.lib")
#pragma comment(lib, "kernel32.lib")
#pragma comment(lib, "ntdll.lib")  // For NTDLL functions
```

### Dynamic Link (Phase2.dll)

```cmd
link /DLL /OUT:Phase2.dll /SUBSYSTEM:WINDOWS /MACHINE:X64 ^
    Phase2_Master.obj ^
    Phase2_Foundation.obj ^
    kernel32.lib ^
    ntdll.lib ^
    ws2_32.lib ^
    /PDB:Phase2.pdb
```

**Import lib automatically generated:**
- `Phase2.lib` (10KB, import stubs)
- `Phase2.dll` (50KB, actual code)
- `Phase2.pdb` (debug symbols, optional)

### Dependent Libraries

| Library | Functionality | Required? |
|---------|---------------|-----------|
| kernel32.lib | Win32 API (file I/O, memory, threading) | ✅ Yes |
| ntdll.lib | Native API (compression, crypto) | ✅ Yes |
| ws2_32.lib | Winsock (networking for HF/Ollama) | ⚠️ Optional* |
| bcrypt.lib | Cryptography (SHA-256 verification) | ⚠️ Optional* |

*Optional if HF Hub / Ollama / Verification not used

## Build Output

### Successful Build

```
Building Phase 2 Model Loader...
  Assembling Phase2_Master.asm (2100 LOC)
    - 1,200 symbols
    - 8 forward declarations
    - Output: Phase2_Master.obj (60 KB)
  
  Compiling Phase2_Foundation.cpp (400 LOC)
    - 25 external symbols resolved
    - Output: Phase2_Foundation.obj (15 KB)
  
  Linking Phase2.lib
    - 32 public exports
    - Output: Phase2.lib (75 KB)

Build successful!
✓ Phase2_Master.obj (60 KB)
✓ Phase2_Foundation.obj (15 KB)
✓ Phase2.lib (75 KB)
✓ Phase2.pdb (optional, 200 KB)
```

### Build Artifacts

```
D:\rawrxd\build\phase2\
├── Phase2_Master.obj          (60 KB)  Assembly object
├── Phase2_Foundation.obj      (15 KB)  C++ object
├── Phase2.lib                 (75 KB)  Static library
├── Phase2.dll                 (50 KB)  Dynamic library (if enabled)
├── Phase2.pdb                 (200 KB) Debug symbols (if enabled)
└── CMakeFiles\               (8 KB)   CMake metadata
```

## Troubleshooting

### Issue: "ml64.exe not found"

**Solution:** Add MASM to PATH
```powershell
# Option 1: Add to PATH
$env:Path += ";C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Tools\MASM\bin\x64"

# Option 2: Set CMAKE_ASM_MASM_COMPILER
cmake -DCMAKE_ASM_MASM_COMPILER="C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Tools\MASM\bin\x64\ml64.exe" -B build
```

### Issue: "Cannot find Phase1"

**Solution:** Build Phase 1 first
```powershell
cd D:\rawrxd
.\scripts\Build-Phase1.ps1 -Release
.\scripts\Build-Phase2.ps1 -Release
```

### Issue: Linker errors "undefined reference to Phase1Initialize"

**Solution:** Link with Phase1.lib
```cmd
link Phase2.obj Phase1.lib kernel32.lib /OUT:test.exe
```

### Issue: "Phase2_Master.asm(125): error A1000: cannot open include file"

**Solution:** Check INCLUDE path for Phase1 headers
```cmd
ml64.exe /I"D:\rawrxd\include" /c Phase2_Master.asm
```

### Issue: Assertion failures in tests

**Check:**
1. Phase 1 initialized correctly: `Phase1Initialize()` must succeed first
2. Sufficient disk space: Phase 2 needs 50GB for full models
3. File permissions: Models must be readable
4. Correct build flags: Debug vs Release mismatch

## Integration with Existing Projects

### As Static Library

```cpp
// main.cpp
#include "Phase1_Foundation.h"
#include "Phase2_Foundation.h"

#pragma comment(lib, "Phase1.lib")
#pragma comment(lib, "Phase2.lib")

int main() {
    auto* phase1 = Phase1::Foundation::GetInstance();
    auto* loader = Phase2::ModelLoader::Create(phase1->GetNativeContext());
    loader->LoadModel("model.gguf");
}
```

### As DLL

```cpp
// main.cpp (DLL consumer)
#include "Phase2_Foundation.h"

#pragma comment(lib, "Phase2.lib")  // Import lib

int main() {
    // Automatically links to Phase2.dll at runtime
    auto* loader = Phase2::ModelLoader::Create(phase1_ctx);
}

// Release:
// ✓ Distribute Phase2.dll alongside executable
// ✓ Phase1.dll (if also used as DLL)
```

### CMake Integration

```cmake
# In your project's CMakeLists.txt
find_package(RawrXD REQUIRED COMPONENTS Phase1 Phase2)

add_executable(myapp main.cpp)
target_link_libraries(myapp RawrXD::Phase2)
```

## Performance Verification

### Run Benchmark

```powershell
.\build\phase2\Phase2Benchmark.exe
```

**Expected output:**
```
Phase 2 Model Loader Benchmark
==============================

Format Detection:
  - GGUF magic check: 45µs
  - File open/close: 120µs
  - Total: 165µs ✓

GGUF Parsing (7B model):
  - Header read: 80µs
  - Tensor info parse: 450µs
  - Hash table build: 230µs
  - Total: 760µs ✓

Memory Operations:
  - Arena allocation: 5µs
  - Hash lookup (1000 tensors): 35ns ✓
  - Tensor load (100 MB): 850ms (disk limited)

Overall Performance: PASS ✓
```

## CI/CD Pipeline

### GitHub Actions Example

```yaml
name: Phase 2 Build

on: [push, pull_request]

jobs:
  build:
    runs-on: windows-latest
    
    steps:
      - uses: actions/checkout@v3
      
      - name: Setup MSVC
        uses: microsoft/setup-msbuild@v1
      
      - name: Build Phase 1
        run: |
          cd D:\rawrxd
          .\scripts\Build-Phase1.ps1 -Release
      
      - name: Build Phase 2
        run: |
          .\scripts\Build-Phase2.ps1 -Release
      
      - name: Run Tests
        run: |
          .\build\phase2\Phase2Test.exe
      
      - name: Upload Artifacts
        uses: actions/upload-artifact@v3
        with:
          name: phase2-build
          path: build/phase2/
```

## Deployment

### Production Release Checklist

- [ ] Build in Release mode: `Build-Phase2.ps1 -Release`
- [ ] Run full test suite: `Phase2Test.exe --verbose`
- [ ] Generate PDB files: Include `Phase2.pdb` for debugging
- [ ] Include documentation: `docs/PHASE2_*`
- [ ] Version information: Update `VERSION` file
- [ ] Sign DLL (if required): Use signtool.exe
- [ ] Create installer: WiX / NSIS / MSI

### Archive Release

```powershell
# Create release archive
$version = "1.0.0"
$archive = "RawrXD-Phase2-v$version.zip"

Compress-Archive -Path @(
    "build/phase2/Phase2.dll",
    "build/phase2/Phase2.lib",
    "include/Phase2_Foundation.h",
    "docs/PHASE2_ARCHITECTURE.md",
    "docs/PHASE2_API_REFERENCE.md",
    "README.md"
) -DestinationPath $archive

Write-Host "Release: $archive"
```

## Next Steps

1. **Verify Build:**
   ```powershell
   .\scripts\Build-Phase2.ps1 -Release
   .\build\phase2\Phase2Test.exe
   ```

2. **Integrate with Phase 1:**
   - Both Phase 1 and Phase 2 should be in memory
   - Create loader: `Phase2::ModelLoader::Create(phase1_ctx)`

3. **Test Model Loading:**
   - Download a 7B GGUF model
   - Load via Phase 2
   - Query tensors

4. **Ready for Phase 3/4:**
   - Phase 3 (Agent Kernel) will depend on Phase 2
   - Phase 4 (Swarm Inference) will use Phase 2 for model loading

## Support & Debugging

### Enable Verbose Logging

```cpp
// In Phase2_Foundation.cpp
#define PHASE2_DEBUG 1

// Then rebuild with:
cl.exe /c /O2 /DPHASE2_DEBUG Phase2_Foundation.cpp
```

### Use WinDbg for Assembly Debugging

```powershell
# Start with debugger
windbg -o Phase2Test.exe

# Set breakpoints on assembly functions:
bp Phase2_Master!LoadGGUFLocal
g  # Continue

# Examine memory:
!address <address>
```

### Memory Profiling

```cpp
// In test application
auto initial = GetProcessMemory();
loader->LoadModel("model.gguf");
auto final = GetProcessMemory();
printf("Memory used: %llu MB\n", (final - initial) / (1024*1024));
```

## See Also

- `PHASE2_ARCHITECTURE.md` - Design overview
- `PHASE2_API_REFERENCE.md` - API documentation
- `Phase1_BUILD_GUIDE.md` - Phase 1 build (prerequisite)
- `Phase4_BUILD_GUIDE.md` - Phase 4 (depends on Phase 2)
