# PHASE 5: BUILD GUIDE - Complete Compilation & Deployment

**Status:** ✅ Production Ready | **Last Updated:** Jan 27, 2026

---

## 1. PREREQUISITES

### Required Tools
- **Visual Studio 2022 Enterprise** with C++ workload
- **MASM (ml64.exe)** - x64 assembler (included with MSVC)
- **CMake 3.22+** (optional, for automated builds)
- **Winsock2** headers & libraries (included with Windows SDK)
- **Crypto API (BCrypt)** (included with Windows SDK)

### Verify Installation

```powershell
# Check ml64.exe
where ml64.exe

# Check link.exe
where link.exe

# Check compiler
cl.exe /?
```

---

## 2. BUILD METHODS

### Method 1: PowerShell Script (Recommended)

**Location:** `D:\rawrxd\scripts\Build-Phase5.ps1`

```powershell
cd D:\rawrxd

# Debug build
.\scripts\Build-Phase5.ps1 -Debug

# Release build with optimizations
.\scripts\Build-Phase5.ps1 -Release

# Clean rebuild
.\scripts\Build-Phase5.ps1 -Clean -Release
```

**Script Actions:**
1. Assemble x64 MASM
2. Compile C++ wrappers
3. Link with all phases (1-5)
4. Generate PDB (debug symbols)
5. Run tests

### Method 2: Manual Compilation

```powershell
cd D:\rawrxd

# Set Visual Studio environment
"C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvars64.bat"

# Step 1: Assemble Phase 5
ml64.exe /c /O2 /Zi /W3 /nologo `
    /Fo build\phase5\Phase5_Master_Complete.obj `
    src\orchestrator\Phase5_Master_Complete.asm

if ($LASTEXITCODE -ne 0) {
    Write-Error "Assembly failed"
    exit 1
}

# Step 2: Compile C++ wrapper
cl.exe /c /O2 /Zi /W4 /EHsc /std:c++17 `
    /I include `
    /Fo build\phase5\Phase5_Foundation.obj `
    src\orchestrator\Phase5_Foundation.cpp

if ($LASTEXITCODE -ne 0) {
    Write-Error "C++ compilation failed"
    exit 1
}

# Step 3: Link with all phases
link /DLL /OUT:build\phase5\Phase5.dll `
    /SUBSYSTEM:WINDOWS /OPT:REF /OPT:ICF `
    /DEBUG /DEBUGTYPE:CV `
    build\phase5\Phase5_Master_Complete.obj `
    build\phase5\Phase5_Foundation.obj `
    build\phase4\Phase4_Master_Complete.obj `
    build\phase3\Phase3_Master.obj `
    build\phase2\Phase2_Master.obj `
    build\phase1\Phase1_Foundation.obj `
    kernel32.lib user32.lib advapi32.lib ntdll.lib `
    ws2_32.lib bcrypt.lib

if ($LASTEXITCODE -ne 0) {
    Write-Error "Linking failed"
    exit 1
}

# Step 4: Create static library
lib /OUT:build\phase5\Phase5.lib `
    build\phase5\Phase5_Master_Complete.obj `
    build\phase5\Phase5_Foundation.obj

# Step 5: Build tests
cl.exe /c /O2 /Zi /EHsc `
    /I include `
    /Fo build\phase5\Phase5_Test.obj `
    test\Phase5_Test.cpp

link /OUT:build\phase5\Phase5Test.exe `
    /SUBSYSTEM:CONSOLE `
    build\phase5\Phase5_Test.obj

# Step 6: Run tests
.\build\phase5\Phase5Test.exe
```

### Method 3: CMake Build

**File:** `CMakeLists_Phase5.txt`

```cmake
cmake_minimum_required(VERSION 3.22)
project(Phase5 LANGUAGES C CXX ASM)

enable_language(ASM)

# Phase 5 Assembly
add_library(Phase5_ASM OBJECT
    src/orchestrator/Phase5_Master_Complete.asm
)

set_source_files_properties(
    src/orchestrator/Phase5_Master_Complete.asm
    PROPERTIES LANGUAGE ASM
)

# Phase 5 C++
add_library(Phase5_Foundation STATIC
    src/orchestrator/Phase5_Foundation.cpp
    include/Phase5_Foundation.h
)

target_include_directories(Phase5_Foundation PUBLIC include)
target_link_libraries(Phase5_Foundation Phase5_ASM)

# Main library
add_library(Phase5 SHARED
    $<TARGET_OBJECTS:Phase5_ASM>
    src/orchestrator/Phase5_Foundation.cpp
)

target_link_libraries(Phase5
    Phase1::Foundation
    Phase2::Foundation
    Phase3::Foundation
    Phase4::Foundation
    ws2_32
    bcrypt
)

# Tests
add_executable(Phase5Test test/Phase5_Test.cpp)
target_link_libraries(Phase5Test Phase5)

enable_testing()
add_test(NAME Phase5Tests COMMAND Phase5Test)
```

**Build with CMake:**
```powershell
cd D:\rawrxd
mkdir build_cmake
cd build_cmake
cmake -G "Visual Studio 17 2022" -A x64 ..
cmake --build . --config Release
ctest --output-on-failure
```

---

## 3. OUTPUT FILES

### Artifacts Generated

```
build/phase5/
├── Phase5_Master_Complete.obj        (~60 KB)
├── Phase5_Foundation.obj              (~15 KB)
├── Phase5.lib                         (~75 KB static library)
├── Phase5.dll                         (~200 KB with debug)
├── Phase5.pdb                         (~500 KB debug symbols)
├── Phase5_Test.obj                    (~10 KB)
└── Phase5Test.exe                     (~500 KB test executable)
```

### Size Breakdown

| Component | Size | Notes |
|-----------|------|-------|
| Assembly (.obj) | 60 KB | Raft, BFT, Gossip, RS, healing |
| C++ (.obj) | 15 KB | Wrappers, utilities |
| Linked DLL | 200 KB | Includes all phases |
| PDB | 500 KB | Debug symbols |
| **Total** | ~775 KB | Production deployable |

---

## 4. CONFIGURATION

### Compile Flags

```
/c              Compile without linking
/O2             Maximize speed optimization
/Zi             Generate debug information
/W4             Warning level 4 (highest)
/EHsc           Enable exception handling
/std:c++17      C++17 standard
/D_WINDOWS      Windows platform define
/D_WIN64        64-bit define
/DNDEBUG        Release (no debug)
```

### Link Flags

```
/DLL            Build dynamic library
/SUBSYSTEM:WINDOWS   Windows subsystem
/OPT:REF        Remove unreferenced functions
/OPT:ICF        Identical code folding
/DEBUG          Debug information
/DEBUGTYPE:CV   CodeView debug format
/MANIFEST:EMBED Embed manifest
```

### Optional Flags (TLS Support)

```
# Link with OpenSSL/mbedTLS for HTTPS/gRPC with TLS
link /OUT:Phase5_TLS.dll ... ssleay32.lib libeay32.lib
```

---

## 5. LINKING WITH PHASES

### Dependency Order

```
Phase5 (Orchestrator)
  ↓
Phase4 (Inference)
  ↓
Phase3 (Agents)
  ↓
Phase2 (Models)
  ↓
Phase1 (Foundation)
  ↓
Windows APIs (kernel32, ws2_32, bcrypt)
```

### Import Libraries Required

```powershell
# System libraries
kernel32.lib            # Process, memory, threading
user32.lib              # Windows GUI (if needed)
advapi32.lib            # Security, registry
ntdll.lib               # Native API (optional)

# Networking
ws2_32.lib              # Winsock2 (TCP/UDP/IOCP)

# Cryptography
bcrypt.lib              # Cryptography API (AES-GCM, SHA-256)

# Previous phases
Phase4_Foundation.lib
Phase3_Foundation.lib
Phase2_Foundation.lib
Phase1_Foundation.lib
```

---

## 6. TROUBLESHOOTING

### Problem: "ml64.exe not found"

**Solution 1: Add to PATH**
```powershell
$env:PATH += ";C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Tools\MSVC\14.39.33519\bin\Hostx64\x64"
```

**Solution 2: Use vcvars64.bat**
```powershell
& "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvars64.bat"
```

### Problem: "Unresolved external symbol"

**Check:** Missing library in link line

```powershell
# List all unresolved symbols
link /VERBOSE ... 2>&1 | Select-String "unresolved"

# Add missing lib to command
link ... missing_lib.lib ...
```

### Problem: "error A2188: invalid character in line"

**Cause:** Syntax error in assembly

**Debug:**
```powershell
# Compile with verbose output
ml64.exe /c /W3 Phase5_Master_Complete.asm

# Check specific line numbers in error
# Review assembly syntax at that line
```

### Problem: "Access violation at runtime"

**Likely causes:**
- Null pointer dereference in assembly
- Stack misalignment
- Incorrect calling convention

**Debug:**
```powershell
# Run under debugger
windbg.exe Phase5Test.exe

# Or use Visual Studio
devenv /debugexe Phase5Test.exe
```

### Problem: Tests fail with "Stack overflow"

**Cause:** Excessive stack allocation in assembly

**Solution:**
```asm
; Instead of:
sub rsp, 10000h    ; 64 KB - BAD

; Use:
sub rsp, 1000h     ; 4 KB - OK
call ArenaAllocate ; Allocate from heap
```

---

## 7. CI/CD INTEGRATION

### GitHub Actions Example

```yaml
name: Phase 5 Build

on: [push, pull_request]

jobs:
  build:
    runs-on: windows-latest
    
    steps:
      - uses: actions/checkout@v3
      
      - name: Setup MSVC
        uses: microsoft/setup-msbuild@v1
        with:
          vs-version: '17'
      
      - name: Build Phase 5
        run: |
          cd D:\rawrxd
          .\scripts\Build-Phase5.ps1 -Release
      
      - name: Run Tests
        run: |
          .\build\phase5\Phase5Test.exe
      
      - name: Upload Artifacts
        uses: actions/upload-artifact@v3
        with:
          name: phase5-builds
          path: build/phase5/*.{dll,lib,exe}
```

---

## 8. DEPLOYMENT CHECKLIST

### Pre-Deployment Verification

- [ ] All 56 tests pass
- [ ] No linker warnings
- [ ] PDB symbols generated
- [ ] Performance benchmarks meet targets
- [ ] Memory allocation doesn't exceed limits
- [ ] Thread creation succeeds
- [ ] Network sockets bind correctly
- [ ] Metrics server responds to scrapes

### Deployment Steps

```powershell
# 1. Build Release
.\scripts\Build-Phase5.ps1 -Release

# 2. Test on staging
Copy-Item build\phase5\Phase5.dll C:\Staging\
Run-StageTests

# 3. Verify metrics
curl http://localhost:9090/metrics

# 4. Check gRPC health
grpcurl localhost:31340 list

# 5. Deploy to production
Deploy-ToCluster -Version 1.0.0

# 6. Verify cluster
Get-ClusterHealth
```

---

## 9. PERFORMANCE VERIFICATION

### Benchmark Suite

```powershell
# Measure assembly performance
$sw = [Diagnostics.Stopwatch]::StartNew()

# Run 10,000 Raft log entries
for ($i = 0; $i -lt 10000; $i++) {
    $orchestrator->AppendLogEntry($entry)
}

$sw.Stop()
Write-Host "Time: $($sw.ElapsedMilliseconds)ms"
Write-Host "Throughput: $(10000 / ($sw.ElapsedMilliseconds / 1000)) ops/sec"
```

### Expected Performance

- Raft log append: <1µs per entry
- Consensus decision: <200ms (election timeout)
- Healing reconstruction: <200ms per 10MB block
- gRPC latency: <100ms P99
- Gossip propagation: 4 rounds for 16 nodes

---

## 10. DEBUGGING

### Generate Detailed Map File

```powershell
link ... /MAP:build\phase5\Phase5.map ...

# View symbol addresses
Get-Content build\phase5\Phase5.map | Select-String "OrchestratorInitialize"
```

### Inspect Object Files

```powershell
# List exported symbols
dumpbin /EXPORTS build\phase5\Phase5.dll

# View sections
dumpbin /HEADERS build\phase5\Phase5.dll
```

### Debug in Visual Studio

```powershell
# Run under Visual Studio debugger
devenv build\phase5\Phase5Test.exe
```

---

**PHASE 5 BUILD GUIDE - COMPLETE** ✅
