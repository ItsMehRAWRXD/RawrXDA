# Reverse Engineering Suite — Architecture Reference

> RawrXD Reverse Engineering Suite v1.0
> Professional Binary Analysis, Deobfuscation, and Compiler Infrastructure

**See also:** [Game development (post-release)](../../docs/REVERSE_ENGINEERING_GAME_DEVELOPMENT.md) — module locating, external/internal frameworks, ved/oemASMx64. [Decompilation & x64 “subtitles”](../../docs/REVERSE_ENGINEERING_DECOMPILATION_AND_X64.md) — extension→source, execution trace, code dump, FLIRT/FLOSS, pure x64, hot reload. [Source/text digestion & alignment](../../docs/REVERSE_ENGINEERING_SOURCE_DIGESTION_AND_ALIGNMENT.md) — stopwords, modality, beaconism, M=T+A−NIP SIMD. [Portable x64 MASM loader](../../docs/REVERSE_ENGINEERING_PORTABLE_X64_LOADER.md) — operational requirements, Windows 11 thumb-drive console, streaming loader. [Boot sector & minimal kernel](../../docs/REVERSE_ENGINEERING_BOOT_AND_MINIMAL_KERNEL.md) — 512-byte/0x55AA rule, bare-bones kernel, MASM boot sector, long-mode trampoline, MBR builder. All apply after released finalization (official, non–beta/alpha).

---

## 1. Overview

The RawrXD Reverse Engineering Suite is a comprehensive collection of binary analysis
tools, disassemblers, deobfuscators, and compiler infrastructure—all written in pure
assembly (MASM32/MASM64/NASM) and C++20 with zero external dependencies.

**Total inventory:** 70 files, ~1.3MB of source code across 10 organized modules.

---

## 2. Directory Layout

```
src/reverse_engineering/
│
├── CMakeLists.txt                    # Master RE build configuration
│
├── RawrCodex.hpp                     # Advanced binary analysis (2,960 lines)
├── RawrCompiler.hpp                  # JIT compiler & code generator (675 lines)
├── RawrDumpBin.hpp                   # Custom dumpbin tool (316 lines)
├── RawrReverseEngine.hpp             # Unified RE engine interface (1,011 lines)
│
├── omega_suite/                      # OMEGA-POLYGLOT PE Analyzer Family
│   ├── build_omega_suite.bat         #   Build script (MASM32)
│   ├── OMEGA_COMPILATION_FIX.asm     #   Compilation fixes
│   ├── v3/                           #   v3.0P Professional Reverse Edition
│   │   ├── omega_pro_v3.asm          #     Main PE analyzer (704 lines)
│   │   ├── omega_pro.asm             #     Base version
│   │   ├── omega_simple.asm          #     Simple PE analysis
│   │   ├── OmegaPro_v3_fixed.asm    #     Fixed v3
│   │   ├── OmegaProProfessional.asm #     Professional edition
│   │   ├── OmegaProProfessional_fixed.asm
│   │   └── test_omega.asm           #     Test harness
│   ├── v4/                           #   v4.0 Multi-Language Polyglot
│   │   ├── OmegaPolyglot_v4.asm     #     Full polyglot (844 lines)
│   │   ├── OmegaPolyglot_v4_fixed.asm
│   │   ├── OmegaPolyglotMax.asm     #     Maximum edition
│   │   ├── OmegaPolyglotPro.asm     #     49KB professional suite
│   │   └── omega_pro_v4.asm         #     v4 iteration
│   ├── v5/                           #   v5.0 "The Unreverseable Reverser"
│   │   └── Omega-Install-Reverser-Protected.asm  # Anti-RE protection
│   ├── v7/                           #   v7.0 Codex Reverse Engine
│   │   ├── CodexAIReverseEngine.asm  #     AI-enhanced RE (40KB)
│   │   ├── CodexPro.asm             #     Professional codex (38KB)
│   │   ├── CodexProfessional.asm    #     Professional edition (29KB)
│   │   ├── CodexReverse.asm         #     v6 reverse engine (42KB)
│   │   ├── CodexUltimate.asm        #     Ultimate edition (68KB)
│   │   └── CodexUltra.asm           #     Ultra edition (30KB)
│   └── src_variants/                 #   Development variants from src/
│       ├── omega_crx.asm
│       ├── omega_final_working.asm
│       ├── omega_final.asm
│       ├── omega_max_v31.asm
│       ├── omega_pro_maximum.asm
│       ├── omega_pro_v4.asm
│       ├── omega_pro.asm
│       ├── omega_professional.asm
│       ├── omega_simple.asm
│       ├── OmegaPro.asm
│       ├── CodexPro.asm
│       ├── CodexUltimate.asm
│       ├── CodexUltimate_fixed.asm
│       └── CodexUltimate_simple.asm
│
├── disassembler/                     # x86/x64 Instruction Decoder
│   └── (via src/asm/RawrCodex.asm)  #   288KB pure MASM64 — PE/ELF/Mach-O
│                                     #   Full prefix, REX, operand handling
│
├── deobfuscator/                     # Anti-Obfuscation Engine
│   ├── build_deobf.bat              #   Build script
│   ├── RawrXD_Deobfuscator.inc      #   Shared deobfuscator definitions (6KB)
│   ├── RawrXD_OmegaDeobfuscator.asm #   Main deobfuscator (1,297 lines, AVX-512)
│   ├── RawrXD_MetaReverse.asm       #   Meta-RE engine (548 lines)
│   ├── RawrXD_Titan_MetaReverse.asm #   Titan MetaReverse variant
│   └── test_deobf.cpp               #   C++ test harness
│
├── reverser_compiler/                # Self-Hosting "Reverser" Language
│   ├── build_reverser.bat           #   Build script (NASM)
│   ├── reverser_lexer.asm           #   Lexer / tokenizer (24.5KB)
│   ├── reverser_parser.asm          #   Recursive descent parser (12.8KB)
│   ├── reverser_ast.asm             #   AST node definitions (11.5KB)
│   ├── reverser_bytecode_gen.asm    #   Bytecode generator (19.2KB)
│   ├── reverser_compiler.asm        #   Main compiler driver (21.5KB)
│   ├── reverser_runtime.asm         #   Runtime library (15.2KB)
│   ├── reverser_vtable.asm          #   Virtual dispatch tables (12.5KB)
│   ├── reverser_platform.asm        #   Platform abstraction (7.1KB)
│   ├── reverser_syscalls.asm        #   Raw syscall wrappers (1.9KB)
│   ├── reverser_compiler_from_scratch.asm  # Minimal bootstrap
│   ├── reverser_compiler_patched.asm       # Patched compiler
│   ├── reverser_compiler_from_scratch_patched.asm
│   ├── bootstrap_reverser.asm       #   Self-hosting bootstrap (9.7KB)
│   ├── reverser_self_compiler.rev   #   Self-hosting source in Reverser language
│   └── tests/
│       ├── reverser_test_suite.asm  #   Full test suite (17.4KB)
│       ├── test_reverser_lexer.asm  #   Lexer tests (17.4KB)
│       ├── test_reverser_parser.asm #   Parser tests (4KB)
│       ├── test_reverser_runtime.asm#   Runtime tests (5.9KB)
│       ├── test_reverser_platform.asm # Platform tests (5.4KB)
│       └── test_reverser_vtable.asm #   VTable tests (7.5KB)
│
├── pe_tools/                         # PE Analysis Utilities (C++)
│   ├── re_tools.cpp                 #   PE analyzer + inline compiler (108 lines)
│   └── re_tools.h                   #   Header declarations
│
├── model_reverse/                    # LLM Model Reverse Pipeline
│   └── model-llm-harvester.asm      #   Dead-weight generator (MASM64)
│
├── security_toolkit/                 # Offensive Security Toolkit
│   └── RawrXD_IDE_unified.asm       #   All-in-one security (1,088 lines)
│                                     #   Polymorphic builder, Camellia-256,
│                                     #   process injection, UAC bypass,
│                                     #   AV evasion, self-decrypting stub
│
├── re_modules/                       # Additional RE Modules
│   └── gpu_dma_complete_reverse_engineered.asm  # GPU DMA RE (33KB)
│
└── codex/                            # (reserved for Codex expansion)
```

---

## 3. Module Details

### 3.1 OMEGA-POLYGLOT PE Analyzer Suite

The flagship reverse engineering tool. Analyzes Windows PE32/PE32+ executables
with support for:

- **PE Structure Parsing:** MZ/PE signatures, all 16 data directories
- **Import/Export Reconstruction:** Full IAT/EAT parsing with ordinals & forwarders
- **Section Analysis:** Headers, entropy calculation, Shannon entropy for packer detection
- **50-Language Detection:** Java, Python, JavaScript, C#, Go, Rust, PHP, Ruby, etc.
- **10 Packer Signatures:** UPX, ASProtect, Themida, VMProtect, PECompact, etc.
- **Source Reconstruction:** Framework for reconstructing source from PE binaries
- **Rich Header Analysis:** MSVC compiler detection from Rich header
- **.NET Metadata:** CLR header and metadata stream parsing
- **Resource Tree:** Full resource directory traversal

**Architecture:** MASM32 (x86, `.386`, flat model) using `windows.inc`, `kernel32.inc`, `user32.inc`.

**Versions:**
| Version | Key Innovation |
|---------|---------------|
| v3.0P | Professional PE parsing, 50-lang detection, packer signatures |
| v4.0 | Multi-language polyglot deobfuscation |
| v5.0 | "The Unreverseable Reverser" — anti-RE self-protection, XOR+AES-NI |
| v7.0 | AI-enhanced reverse engineering, compiler fingerprinting |

### 3.2 RawrCodex Disassembler (288KB MASM64)

Located at `src/asm/RawrCodex.asm` — the largest single assembly file in the project.

**Capabilities:**
- Full x86/x64 instruction decoding
- PE/ELF/Mach-O format parsing
- SSA (Static Single Assignment) intermediate representation
- Control flow graph (CFG) construction
- Pseudocode generation
- Complete prefix handling (LOCK, REP, segment overrides)
- REX bit parsing
- Operand types: REG/IMM/MEM/REL/FAR
- Register sizes: BYTE through ZMM

**Key Exports:**
```
DisassembleX64, AnalyzePEHeader, ParseELFHeader, AnalyzeMachO,
HexDumpMemory, FindStrings, AnalyzeCodePatterns, DetectPacker,
CalculateEntropy, ExtractResources, AnalyzeImportsExports,
DetectAntiDebug, AnalyzeObfuscation, ExtractSections
```

### 3.3 Anti-Obfuscation Engine

**RawrXD_OmegaDeobfuscator** (1,297 lines, AVX-512 capable):
- `OBFUSCATION_LAYER` struct for detecting encoding/control-flow/anti-debug/metamorphic layers
- `CODE_SIGNATURE` pattern matching with wildcards
- Execution tracing
- Anti-debug API detection (IsDebuggerPresent, CheckRemoteDebugger, RtlVirtualUnwind)

**RawrXD_MetaReverse** (548 lines):
- Detects obfuscation disguised as clean reverse-engineered code
- `AUTHENTICITY_MARKER` classification: compiler/linker/hand-coded/synthetic
- `DECEPTION_PATTERN` detection
- Compiler fingerprints: MSVC 2019+, GCC, Clang prologue/epilogue patterns

### 3.4 Reverser Compiler Suite

A complete, self-hosting compiler infrastructure written in pure assembly:

```
Source (.rev) → Lexer → Parser → AST → Bytecode Gen → Runtime
```

**Components:**
| Component | Lines | Purpose |
|-----------|-------|---------|
| Lexer | ~800 | Tokenization, keyword recognition |
| Parser | ~400 | Recursive descent, AST construction |
| AST | ~350 | Node definitions, tree manipulation |
| Bytecode Gen | ~600 | IR to bytecode compilation |
| Compiler | ~700 | Driver, optimization passes |
| Runtime | ~500 | Execution engine, GC, I/O |
| VTable | ~400 | Virtual dispatch, polymorphism |
| Platform | ~250 | Windows/Linux syscall abstraction |

**Self-Hosting:** The compiler can compile itself via `bootstrap_reverser.asm` +
`reverser_self_compiler.rev`.

### 3.5 C++ RE Headers

Four production-grade C++ headers providing the IDE-facing API:

| Header | Lines | Key Types |
|--------|-------|-----------|
| `RawrCodex.hpp` | 2,960 | `Symbol`, `Section`, `Import`, `Export`, `DisassemblyLine`, `SSAOpType`, `SSAVarClass` |
| `RawrReverseEngine.hpp` | 1,011 | `CallGraph`, `DataFlowAnalysis`, `SignatureMatch`, `DecompilationResult`, `BinaryDiff` |
| `RawrCompiler.hpp` | 675 | `CompilationUnit`, `OptimizationLevel`, `TargetArch` |
| `RawrDumpBin.hpp` | 316 | `DumpOptions`, `PEDump`, `ImportDump`, `ExportDump` |

### 3.6 Security Toolkit

`RawrXD_IDE_unified.asm` (1,088 lines) — Offensive security research tool:
- Polymorphic code builder (junk instructions, register permutation, flow inversion)
- Mirage engine for anti-analysis
- Camellia-256 encryption
- Process injection techniques
- UAC bypass methods
- Persistence mechanisms
- DLL sideloading
- AV evasion patterns
- Entropy manipulation
- Self-decrypting stub
- CLI dispatcher
- Self-compiling trace engine

---

## 4. Build System

### CMake Integration

The RE suite integrates into the main RawrXD build via `add_subdirectory()`:

```cmake
# In root CMakeLists.txt:
add_subdirectory(src/reverse_engineering)

# Provides:
#   RawrXD-RE-Library  — Static C++ library (linked into RawrEngine + Win32IDE)
#   re_deobfuscator    — MASM64 objects (optional, MSVC only)
#   re_model_reverse   — MASM64 objects (optional, MSVC only)
#   re_omega_v3        — MASM32 standalone executable (optional, needs C:\masm32)
#   re_reverser_compiler — NASM objects (optional, needs nasm)
```

### Build Targets

| Target | Type | Toolchain | Output |
|--------|------|-----------|--------|
| `RawrXD-RE-Library` | Static lib | MSVC/GCC | `libRawrXD-RE-Library.a` |
| `re_omega_v3` | Executable | MASM32 | `omega_pro_v3.exe` |
| `re_deobfuscator` | Objects | MASM64 | `.obj` files linked into main |
| `re_model_reverse` | Objects | MASM64 | `.obj` files linked into main |
| `re_reverser_compiler` | Objects | NASM | `.obj` files |

### Standalone Build Scripts

```batch
# Omega PE analyzer suite (MASM32)
cd src\reverse_engineering\omega_suite
build_omega_suite.bat all

# Reverser compiler suite (NASM)
cd src\reverse_engineering\reverser_compiler
build_reverser.bat all

# Deobfuscator (MASM64 via deobf build)
cd src\reverse_engineering\deobfuscator
build_deobf.bat
```

---

## 5. IDE Integration

### 5.1 Integration with compiling and debugging

Codex/RE is wired into the **debug** and **build** workflow so the Reverse Engineering menu always has a sensible current binary:

| Trigger | Effect |
|--------|--------|
| **Debug → Launch** (or `/api/debug/launch`) | The launched executable is set as the current RE binary. Disassemble, DumpBin, CFG, Detect Vulnerabilities, Export IDA/Ghidra, etc. operate on it without opening "Analyze Binary" first. |
| **RE → Set binary from active document** | If the active editor tab is an `.exe`, `.dll`, or `.obj` file, that path becomes the current RE binary. Use after building so RE targets the built output. |
| **RE → Analyze Binary** | Opens a file dialog and sets the chosen binary as current (unchanged). |

**API:** `Win32IDE::setCurrentBinaryForReverseEngineering(const std::string& path)` — call after a successful build or link when the output exe path is known to make RE menu items target that binary.

**Compile flow:**
- **RE → Compile Source**: On success, the output `.obj` path is set as the current RE binary so Disassemble/DumpBin/CFG work on the object file immediately.
- **Fortress toolchain (Phase 1 + Phase 2) or external scripts**: After link/build produces an `.exe`, call **POST /api/re/set-binary** with body `{"path":"C:\\path\\to\\output.exe"}` (local server default port 11435). The IDE sets that as the current RE binary and appends a line to the Output panel. Alternatively open the exe in the IDE and use **RE → Set binary from active document**, or **RE → Analyze Binary**.

### Win32IDE Commands (ID range 4300–4320, 5100–5199)

The RE suite integrates into the Win32IDE via:
- `Win32IDE_ReverseEngineering.cpp` — File open dialogs, PE analysis display
- `Win32IDE_DecompilerView.cpp` — Direct2D split-pane decompiler/disassembly view

**Decompiler View Features:**
- Left pane: Decompiled pseudocode with SSA variable display
- Right pane: Disassembly with hex bytes
- Bidirectional selection sync
- Right-click SSA variable renaming
- Syntax coloring with Cascadia Code font

### API Entry Points

```cpp
// From re_tools.h
std::string dump_pe(const char* path);        // PE header analysis
std::string run_compiler(const char* src);    // Inline MASM compilation

// From RawrCodex.hpp (namespace RawrXD::ReverseEngineering)
class RawrCodex {
    AnalysisResult analyzeBinary(const std::string& path);
    DisassemblyResult disassemble(uint64_t addr, size_t len);
    SSAGraph liftToSSA(const BasicBlock& bb);
};
```

---

## 6. Dependencies

| Module | Requires | Notes |
|--------|----------|-------|
| C++ Headers | C++20, `<windows.h>` | Zero external deps |
| Omega Suite | MASM32 (`C:\masm32\`) | 32-bit flat model |
| Deobfuscator | MASM64 (`ml64.exe`) | MSVC only, AVX-512 optional |
| RawrCodex | MASM64 (`ml64.exe`) | MSVC only |
| Reverser Compiler | NASM | Cross-platform |
| PE Tools | C++20, Win32 API | `CreateFileMapping`, `MapViewOfFile` |
| Security Toolkit | MASM64 | Research only |

---

## 7. File Provenance

Files were consolidated from scattered locations across the workspace:

| Original Location | Destination | File Count |
|-------------------|-------------|------------|
| `D:\rawrxd\` (root) | `omega_suite/v3-v7/` | 15 |
| `D:\rawrxd\src\` (scattered) | `omega_suite/src_variants/` | 14 |
| `D:\rawrxd\deobf\` | `deobfuscator/` | 5 |
| `D:\rawrxd\itsmehrawrxd-master\` | `reverser_compiler/` | 17 |
| `D:\rawrxd\compilers\_patched\` | `reverser_compiler/` | 2 |
| `D:\rawrxd\Ship\` | `deobfuscator/` | 1 |
| `D:\rawrxd\src\re_tools.*` | `pe_tools/` | 2 |
| `D:\rawrxd\model-llm-harvester.asm` | `model_reverse/` | 1 |
| `C:\amazonq-local\` | `security_toolkit/` | 1 |
| `D:\rawrxd\gpu_dma_*.asm` | `re_modules/` | 1 |
| `D:\` root | `omega_suite/v3/` | 1 |
| (existing) | Root headers | 4 |

**Originals preserved** — no files were deleted from their original locations.
The copies in `src/reverse_engineering/` are the canonical build sources going forward.

---

## 8. Assembly Kernel Reference

The main disassembler kernel (`src/asm/RawrCodex.asm`, 288KB) exports these
procedures for use by the C++ IDE:

```asm
; PE/ELF/Mach-O Analysis
PROC DisassembleX64       ; (pCode:QWORD, codeLen:QWORD, pOutput:QWORD) → QWORD
PROC AnalyzePEHeader      ; (pBase:QWORD, fileSize:QWORD, pResult:QWORD) → QWORD
PROC ParseELFHeader       ; (pBase:QWORD, fileSize:QWORD, pResult:QWORD) → QWORD
PROC AnalyzeMachO         ; (pBase:QWORD, fileSize:QWORD, pResult:QWORD) → QWORD

; Utility
PROC HexDumpMemory        ; (pData:QWORD, dataLen:QWORD, pOutput:QWORD) → QWORD
PROC FindStrings          ; (pData:QWORD, dataLen:QWORD, minLen:DWORD) → QWORD
PROC CalculateEntropy     ; (pData:QWORD, dataLen:QWORD) → REAL8

; Pattern Analysis
PROC AnalyzeCodePatterns  ; (pCode:QWORD, codeLen:QWORD) → QWORD
PROC DetectPacker         ; (pBase:QWORD, fileSize:QWORD) → DWORD
PROC DetectAntiDebug      ; (pBase:QWORD, fileSize:QWORD) → DWORD
PROC AnalyzeObfuscation   ; (pBase:QWORD, fileSize:QWORD) → QWORD

; Import/Export
PROC AnalyzeImportsExports; (pBase:QWORD, fileSize:QWORD, pResult:QWORD) → QWORD
PROC ExtractResources     ; (pBase:QWORD, fileSize:QWORD, pOutput:QWORD) → QWORD
PROC ExtractSections      ; (pBase:QWORD, fileSize:QWORD, pOutput:QWORD) → QWORD
```

---

## 9. Future Roadmap

- [ ] Consolidate omega_suite versions into a single "best-of" v8.0
- [ ] MASM64 port of the MASM32 omega tools
- [ ] ELF/Mach-O analysis in the omega suite (currently PE only)
- [ ] Integration of reverser_compiler into IDE command palette
- [ ] GPU-accelerated pattern matching via Vulkan compute shaders
- [ ] DWARF debug info parsing for Linux binary analysis
- [ ] IDA Pro / Ghidra export compatibility
- [ ] Scripting engine for custom RE analysis workflows
