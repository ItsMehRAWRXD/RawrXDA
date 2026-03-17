# RawrXD Reverse Engineering Toolkit - Complete Reference

**Pure MASM x64 Implementation - No Scaffolding, Production Ready**

## Overview

The RawrXD Advanced Reverse Engineering Toolkit provides complete binary analysis, deobfuscation, and code reconstruction capabilities using pure MASM x64 assembly kernels with PowerShell orchestration.

### Components

1. **Advanced-Binary-Reverser.ps1** - PowerShell orchestration script
2. **Advanced_Code_Deobfuscator.asm** - MASM x64 deobfuscation engine
3. **Code_Pattern_Reconstructor.asm** - MASM x64 pattern matching and reconstruction
4. **quantum_beaconism_backend.asm** - Deterministic MASM-backed analysis kernels
5. **re_tools.cpp/h** - PE analysis C++ bridge

## Quick Start

### Basic PE Analysis

```powershell
cd d:\rawrxd
.\scripts\Advanced-Binary-Reverser.ps1 -Target "C:\Windows\System32\notepad.exe" -Mode Quick
```

### Full Analysis with Compiler ID

```powershell
.\scripts\Advanced-Binary-Reverser.ps1 `
    -Target "myapp.exe" `
    -Mode Full `
    -IdentifyCompiler `
    -DetectVM `
    -ExtractStrings `
    -Output "analysis_results"
```

### Deep Analysis with Code Reconstruction

```powershell
.\scripts\Advanced-Binary-Reverser.ps1 `
    -Target "protected.exe" `
    -Mode Reconstruct `
    -ExportASM `
    -ExportC `
    -ExportStructs `
    -FindCrypto `
    -Output "reconstructed"
```

## Features

### Phase 1:  PE Structure Analysis

- **DOS/NT Header Parsing**: Direct byte-levelheader extraction
- **Section Enumeration**: .text, .data, .rdata, .pdata analysis
- **Entry Point Detection**: OEP identification
- **Image Base Calculation**: Relocation base analysis
- **Architecture Detection**: x86/x64/ARM64 identification
- **Characteristic Flags**: DLL/EXE, stripped/debug info

**Output:**
```
Phase 1: PE Structure Analysis
  ✓ Machine: x64 (AMD64)
  ✓ Format: PE32+
  ✓ Entry Point: 0x00001230
  ✓ Image Base: 0x140000000
  ✓ Sections: 6
```

### Phase 2: Code Pattern Extraction

- **Function Prologues**: Multiple prologue patterns (MSVC, GCC, Clang)
  - `push rbp; mov rbp, rsp`
  - `sub rsp, imm8`
  - `mov [rsp+8], rbx`
  - `push rbx; sub rsp, 20h`

- **Function Epilogues**: Return patterns
  - `ret`
  - `pop rbp; ret`
  - `add rsp, imm8; ret`
  - Full epilogue with register restore

- **Call Patterns**:
  - Direct call: `E8 xx xx xx xx`
  - Indirect call via register: `FF D0` (call rax)
  - Indirect call via memory: `FF 15 xx xx xx xx` (call [rip+rel32])

- **Loop Patterns**:
  - Backward conditional jumps
  - `dec rax; jnz` loops
  - `cmp` + conditional jump back

- **String Extraction**: ASCII printable string recovery

**Output:**
```
Phase 2: Code Pattern Extraction
  ✓ Found 342 function prologues
  ✓ Found 298 function epilogues
  ✓ Extracted 1,847 ASCII strings (showing first 100)
```

### Phase 3: Compiler Fingerprinting

Uses pattern frequency analysis to identify compiler:

**MSVC Signatures:**
- `40 53 48 83 EC 20` - Standard x64 prologue
- `48 89 5C 24 08` - Register save pattern
- `.pdata` section presence

**GCC Signatures:**
- `55 48 89 E5` - Classic frame pointer setup
- `48 83 EC 10` - Stack frame allocation

**Clang Signatures:**
- `55 48 89 E5 48 81 EC` - Extended prologue

**Scoring System:**
- Pattern match count × pattern confidence
- Evidence accumulation
- Best match selection

**Output:**
```
Phase 3: Compiler Fingerprinting
  ✓ Compiler: MSVC
  ✓ Confidence: 87%
    • 23 x MSVC pattern (confidence: 80%)
    • 15 x MSVC pattern (confidence: 70%)
    • .pdata section present (MSVC x64)
```

### Phase 4: Anti-Debug Detection

Scans for common anti-debugging techniques:

**Techniques Detected:**

1. **API-Based**:
   - `IsDebuggerPresent` calls
   - `CheckRemoteDebuggerPresent` calls
   - `NtQueryInformationProcess` with ProcessDebugPort (0x07)

2. **Timing-Based**:
   - `RDTSC` (0F 31) instruction usage
   - Multiple timing checks indicating delta analysis

3. **Exception-Based**:
   - Excessive `INT 3` (CC) instructions
   - `INT 2D` debugger detection

4. **Hardware Breakpoint Detection**:
   - DR register access (`0F 21` - mov from debug registers)
   - Context manipulation

**Scoring:**
- Each technique adds to anti-debug score
- Score > 50 indicates strong anti-debugging

**Output:**
```
Phase 4: Anti-Debug Detection
  ⚠ Anti-debug detected! Score: 55/100
    • NtQueryInformationProcess (found 3 instances)
    • RDTSC timing checks (found 8)
    • Hardware breakpoint detection (DR register access: 2)
```

### Phase 5: Results Export

**JSON Report** (`analysis_report.json`):
```json
{
  "Timestamp": "2026-02-16 15:30:45",
  "Target": "sample.exe",
  "Analysis": {
    "Machine": "x64 (AMD64)",
    "Format": "PE32+",
    "EntryPoint": "0x00001230",
    "Sections": 6
  },
  "Patterns": {
    "PrologueCount": 342,
    "EpilogueCount": 298,
    "StringCount": 1847
  },
  "Compiler": {
    "Name": "MSVC",
    "Confidence": 87,
    "Evidence": [...]
  },
  "AntiDebug": {
    "Score": 55,
    "Techniques": [...]
  }
}
```

**ASM Skeleton** (`reconstructed.asm`):
```asm
; Reconstructed from: sample.exe
; Analysis Date: 2026-02-16 15:30:45
; Compiler: MSVC (Confidence: 87%)

.code

; Found 342 potential functions

func_0000:
    ; Function entry at offset 0x1000
    push rbp
    mov rbp, rsp
    sub rsp, 20h
    ; ...
    add rsp, 20h
    pop rbp
    ret
```

**Strings File** (`strings.txt`):
```
Error: Invalid parameter
Copyright (C) 2024
https://example.com
...
```

## MASM x64 Kernels

### Advanced_Code_Deobfuscator.asm

Pure assembly deobfuscation engine with zero C++ dependencies.

**Exported Functions:**
- `Deobfuscator_Initialize` - Initialize context with input buffer
- `Deobfuscator_AnalyzePatterns` - Scan and identify obfuscation
- `Deobfuscator_RemoveJunkCode` - Strip dead/junk code
- `Deobfuscator_UnfoldConstants` - Simplify constant arithmetic
- `Deobfuscator_SimplifyControlFlow` - Remove useless jumps
- `Deobfuscator_CalculateEntropy` - Shannon entropy calculation
- `Deobfuscator_GetResult` - Retrieve cleaned output

**Detects:**
- NOP sequences (`90 90`)
- Identity operations (`mov rax, rax`)
- Double XOR cancel-outs
- Push/pop pairs
- BSWAP pairs (canceling byte swaps)
- Constant folding opportunities
- Useless jumps (`jmp $+2`, `jz $+0`)
- Contradictory conditionals

**Usage from C++:**
```cpp
extern "C" {
    int Deobfuscator_Initialize(void* input, size_t size);
    int Deobfuscator_RemoveJunkCode();
    int Deobfuscator_GetResult(void** output, size_t* out_size);
}

// Initialize
Deobfuscator_Initialize(binary_data, binary_size);

// Clean
int removed = Deobfuscator_RemoveJunkCode();

// Get result
void* clean_code;
size_t clean_size;
Deobfuscator_GetResult(&clean_code, &clean_size);
```

### Code_Pattern_Reconstructor.asm

Pattern-based code reconstruction in pure assembly.

**Exported Functions:**
- `Reconstructor_Initialize` - Initialize with binary
- `Reconstructor_ScanPatterns` - Scan for code patterns
- `Reconstructor_IdentifyFunctions` - Identify function boundaries
- `Reconstructor_BuildASM` - Generate ASM output
- `Reconstructor_GetResult` - Retrieve reconstructed code

**Pattern Types:**
- Function entry/exit
- Call sites (direct/indirect)
- Loop structures
- String references
- Constant loads

**Usage from C++:**
```cpp
extern "C" {
    int Reconstructor_Initialize(void* binary, size_t size);
    int Reconstructor_ScanPatterns();
    int Reconstructor_IdentifyFunctions();
    int Reconstructor_BuildASM();
    int Reconstructor_GetResult(void** asm_output, size_t* asm_size);
}

// Reconstruct
Reconstructor_Initialize(binary, size);
int patterns = Reconstructor_ScanPatterns();
int functions = Reconstructor_IdentifyFunctions();
Reconstructor_BuildASM();

void* asm_code;
size_t asm_size;
Reconstructor_GetResult(&asm_code, &asm_size);
```

## Build Integration

### CMakeLists.txt

The MASM kernels are automatically compiled when building with MSVC:

```cmake
if(RAWR_HAS_MASM)
    set(ASM_KERNEL_SOURCES
        ...
        src/asm/Advanced_Code_Deobfuscator.asm
        src/asm/Code_Pattern_Reconstructor.asm
        ...
    )
    set_source_files_properties(${ASM_KERNEL_SOURCES} PROPERTIES LANGUAGE ASM_MASM)
    add_definitions(-DRAWRXD_LINK_ADVANCED_DEOBFUSCATOR_ASM=1)
    add_definitions(-DRAWRXD_LINK_PATTERN_RECONSTRUCTOR_ASM=1)
endif()
```

### Build Commands

```powershell
# Configure
cmake -B build -G Ninja

# Build all (includes ASM kernels)
cmake --build build --config Release

# Build specific target
cmake --build build --target RawrXD-Gold --config Release
```

## Advanced Usage

### Batch Analysis

```powershell
$binaries = Get-ChildItem "C:\Malware\Samples" -Filter *.exe
foreach ($binary in $binaries) {
    .\scripts\Advanced-Binary-Reverser.ps1 `
        -Target $binary.FullName `
        -Mode Deep `
        -Output "results\$($binary.BaseName)" `
        -IdentifyCompiler `
        -DetectVM `
        -ExtractStrings
}
```

### Integration with Deobfuscator

```cpp
#include <windows.h>

extern "C" int Deobfuscator_Initialize(void*, size_t);
extern "C" int Deobfuscator_RemoveJunkCode();
extern "C" int Deobfuscator_GetResult(void**, size_t*);

void AnalyzeAndClean(const char* path) {
    // Load binary
    HANDLE hFile = CreateFileA(path, GENERIC_READ, 0, nullptr, 
                               OPEN_EXISTING, 0, nullptr);
    HANDLE hMap = CreateFileMapping(hFile, nullptr, PAGE_READONLY, 0, 0, nullptr);
    void* data = MapViewOfFile(hMap, FILE_MAP_READ, 0, 0, 0);
    size_t size = GetFileSize(hFile, nullptr);
    
    // Deobfuscate
    Deobfuscator_Initialize(data, size);
    int removed = Deobfuscator_RemoveJunkCode();
    
    // Get clean code
    void* clean;
    size_t clean_size;
    Deobfuscator_GetResult(&clean, &clean_size);
    
    printf("Removed %d bytes of junk\n", removed);
    printf("Clean size: %zu bytes\n", clean_size);
    
    // Cleanup
    UnmapViewOfFile(data);
    CloseHandle(hMap);
    CloseHandle(hFile);
}
```

## Architecture

```
┌─────────────────────────────────────────┐
│  Advanced-Binary-Reverser.ps1          │
│  (PowerShell Orchestration)             │
└────────────┬────────────────────────────┘
             │
    ┌────────┴────────────────────┬──────────────────┐
    │                             │                  │
┌───▼────────────┐    ┌──────────▼───────┐  ┌──────▼──────────┐
│  Phase 1-4:    │    │  Advanced_Code_  │  │  Code_Pattern_  │
│  PE Analysis   │    │  Deobfuscator    │  │  Reconstructor  │
│  (PowerShell)  │    │  (MASM x64)      │  │  (MASM x64)     │
└────────────────┘    └──────────────────┘  └─────────────────┘
                              │                     │
                              │                     │
                      ┌───────▼─────────────────────▼──────┐
                      │  CMake Build System               │
                      │  (ASM_MASM + ml64.exe)            │
                      └───────────────────────────────────┘
```

## Limitations & Future Work

### Current Limitations

1. **x64 Only**: Patterns optimized for x64, x86 requires separate signatures
2. **Windows PE**: No ELF/Mach-O support yet
3. **Pattern-Based**: Relies on common compiler patterns
4. **Static Analysis Only**: No dynamic analysis/tracing

### Planned Enhancements

1. **ELF Support**: Linux binary analysis
2. **ARM64 Patterns**: Mobile/modern CPU support
3. **Dynamic Tracing**: Runtime behavior analysis
4. **ML-Based Pattern Learning**: Auto-discover new patterns
5. **Control Flow Graph**: Full CFG reconstruction
6. **Type Recovery**: Struct/class reconstruction from assembly

## References

- [SCAFFOLD_134](d:\rawrxd\SCAFFOLD_MARKERS.md#SCAFFOLD_134): NEON/Vulkan fabric ASM
- [SCAFFOLD_136](d:\rawrxd\SCAFFOLD_MARKERS.md#SCAFFOLD_136): inference_core.asm
- [SCAFFOLD_137](d:\rawrxd\SCAFFOLD_MARKERS.md#SCAFFOLD_137): feature_dispatch_bridge.asm
- [Copilot Instructions](d:\.github\copilot-instructions.md): Project architecture
- [RE Architecture](d:\rawrxd\src\reverse_engineering\RE_ARCHITECTURE.md): Reverse engineering framework

## License

Part of RawrXD Advanced GGUF Model Loader project.

**No scaffolding. No placeholders. Production ready.**

---
*Last Updated: 2026-02-16*
*Pure MASM x64 Implementation - Zero Qt Dependencies*
