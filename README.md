<<<<<<< HEAD
# RawrXD PE Writer - Complete Implementation

## Overview

This is a complete PE32+ writer and machine code emitter implemented in pure x64 MASM assembly with zero dependencies and no CRT usage. It generates runnable Windows executables from scratch.

## Architecture

The PE writer follows a backend designer approach, providing a clean API for:
- Creating PE executable contexts
- Adding imports with proper IAT/INT structures  
- Emitting machine code with section management
- Writing complete PE files with all required headers

## Core Components

### 1. Structures Defined

- **IMAGE_DOS_HEADER**: Standard DOS header with e_lfanew pointer
- **IMAGE_NT_HEADERS64**: NT headers with file header and optional header
- **IMAGE_SECTION_HEADER**: Section headers for .text, .rdata, .idata
- **IMAGE_IMPORT_DESCRIPTOR**: Import table descriptors
- **PE_CONTEXT**: Internal context structure for building PE files

### 2. Key Functions

#### PEWriter_CreateExecutable
- **Input**: RCX = image base (0 = default), RDX = entry point RVA
- **Output**: RAX = PE context handle (0 = failure)
- **Purpose**: Allocates and initializes complete PE context structure

**Implementation Details**:
- Allocates memory for DOS header, NT headers, section headers
- Initializes DOS header with proper signature and e_lfanew
- Sets up NT headers with AMD64 machine type and PE32+ magic
- Configures optional header with image base, section/file alignment
- Allocates code buffer and import tables
- Sets default virtual addresses and file offsets

#### PEWriter_AddImport
- **Input**: RCX = PE context, RDX = DLL name, R8 = function name  
- **Output**: RAX = 1 success, 0 failure
- **Purpose**: Builds import table with proper IAT/INT structures

**Implementation Details**:
- Manages import descriptors for multiple DLLs
- Creates import lookup table (INT) entries
- Sets up import address table (IAT) entries
- Handles import by name structures
- Tracks import count and validates limits

#### PEWriter_AddCode
- **Input**: RCX = PE context, RDX = code buffer, R8 = code size
- **Output**: RAX = RVA of code (0 = failure)
- **Purpose**: Handles machine code emission with proper section management

**Implementation Details**:
- Copies machine code to internal buffer
- Validates code size against maximum limits
- Calculates and returns RVA for the code
- Updates internal code size tracking
- Manages .text section content

#### PEWriter_WriteFile
- **Input**: RCX = PE context, RDX = filename
- **Output**: RAX = 1 success, 0 failure  
- **Purpose**: Complete file writing with all headers, sections, and import table

**Implementation Details**:
- Creates output file with proper Win32 API calls
- Writes DOS header and DOS stub
- Calculates and updates final NT header values
- Writes section headers for .text, .rdata, .idata
- Implements proper file padding and alignment
- Writes section data with import table
- Handles all RVA calculations and file offsets

## Memory Management

The implementation uses Windows heap APIs:
- **GetProcessHeap()**: Gets current process heap
- **HeapAlloc()**: Allocates zero-initialized memory
- **HeapFree()**: Frees allocated memory on cleanup

All memory allocation includes proper error handling and cleanup.

## File Structure Layout

```
DOS Header (64 bytes)
DOS Stub (variable size, padded to 0x80)  
NT Headers (248 bytes for PE32+)
Section Headers (40 bytes × 3 sections)
Padding to file alignment (0x400)

.text Section (code)
- Machine code
- Padded to file alignment

.rdata Section (read-only data)
- String constants, resources
- Padded to file alignment  

.idata Section (import data)
- Import descriptors
- Import lookup table
- Import address table  
- Import by name structures
- Padded to file alignment
```

## Virtual Address Layout

```
0x1000: .text section (SECTION_ALIGNMENT)
0x2000: .rdata section  
0x3000: .idata section
0x4000: Next available virtual address
```

## Constants and Defaults

- **Image Base**: 0x140000000 (default for x64)
- **Section Alignment**: 0x1000 (4KB)  
- **File Alignment**: 0x200 (512 bytes)
- **Entry Point**: Configurable RVA
- **Subsystem**: Console application (IMAGE_SUBSYSTEM_WINDOWS_CUI)

## Usage Example

```assembly
; Create PE context
mov rcx, 0          ; Default image base
mov rdx, 1000h      ; Entry point RVA  
call PEWriter_CreateExecutable
mov rbx, rax        ; Save context

; Add kernel32.dll imports
mov rcx, rbx
mov rdx, offset dll_kernel32
mov r8, offset func_GetStdHandle
call PEWriter_AddImport

; Add machine code
mov rcx, rbx
mov rdx, offset code_buffer
mov r8, code_size
call PEWriter_AddCode

; Write executable file
mov rcx, rbx  
mov rdx, offset filename
call PEWriter_WriteFile
```

## Build Instructions

1. Ensure MASM64 and Windows SDK are installed
2. Run `build.bat` to compile PE writer and example
3. Execute `PE_Writer_Example.exe` to generate `hello.exe`
4. Test the generated executable

## Error Handling

All functions return 0 on failure and non-zero on success. The implementation includes:
- Memory allocation failure checks
- File I/O error handling  
- Input validation
- Proper resource cleanup
- Bounds checking for buffers

## Limitations

- Maximum 99 imports (MAX_IMPORTS - 1)
- Maximum 64KB code size (MAX_CODE_SIZE)  
- Three fixed sections (.text, .rdata, .idata)
- Console subsystem only
- No relocations support
- No digital signatures

## Advanced Features

The implementation provides a solid foundation for:
- Custom section creation
- Complex import binding
- Resource embedding  
- Digital signing
- Relocation support
- Exception handling tables

## Technical Notes

- Pure x64 MASM assembly - no CRT dependencies
- Uses Windows heap for memory management
- Generates PE32+ format for x64
- Compatible with Windows Vista and later
- Follows Microsoft PE specification
- Zero external library dependencies except kernel32.dll

This implementation provides a complete, production-ready PE32+ writer suitable for code generation backends, custom compilers, and executable packers.
=======
# RawrXD v3.0 - Native Agentic AI IDE

> **Win32 Native** | **No Qt Dependencies** | **Agentic Core** | **AVX512 Inference** | **Production Ready**

![Build Status](https://github.com/ItsMehRAWRXD/RawrXD/actions/workflows/build.yml/badge.svg)
![License](https://img.shields.io/badge/license-MIT-blue.svg)
![Platform](https://img.shields.io/badge/platform-Windows%20x64-lightgrey)

**RawrXD v3.0** marks the complete transition to a native Windows architecture, eliminating all legacy Qt dependencies in favor of pure **C++20/Win32 APIs**. It features a fully integrated **Agentic Engine** capable of autonomous deep thinking, file research, and self-correction.

## 🎯 v3.0 Highlights

### 🧠 Agentic Engine
- **Deep Thinking**: Integrated Chain-of-Thought (CoT) reasoning for complex problem solving without external API calls.
- **Deep Research**: Autonomous file system scanning (`FileOps`) and context gathering.
- **Self-Correction**: Automated "Code Surgery" using `AgentHotPatcher` techniques for real-time fixes.
- **Reactor Generation**: Experimental support for generating React Server Components.

### ⚡ Native Inference (AVX512)
- **Custom Inference Engine**: Built from scratch for AVX512-optimized CPU inference (`RawrXDTransformer`).
- **Universal Model Loader**: Supports standard **GGUF** and the experimental **RawrBlob** (flat float) format.
- **Direct-to-Hardware**: Hardware-aware scheduling and memory management.
- **Vulkan Types**: Compute Queue Family detection for hybrid inference.

### 💻 Interactive CLI (`rawrxd_cli.exe`)
The new Native CLI provides a powerful interactive shell for AI interaction and system control:
- `/load <path>`: Load GGUF/Blob models directly.
- `/agent <query>`: Dispatch tasks to the **Advanced Coding Agent**.
- `/patch <target>`: Apply hot-patches to code or running instances.
- `/bugreport`: Generate security and optimization audits using `CliSecurityIssue` scanners.
- **Hotkeys**:
    - `x` : Analyze File (Security/Optimization)
    - `t` : Generate Test Stubs
    - `g` : Toggle Overclock Governor
    - `p` : System Status (Thermal/Power)

## 🛠️ Architecture

### AIIntegrationHub
The **AIIntegrationHub** acts as the central nervous system, routing messages between the CLI, the Inference Engine, and the Agentic Core. It replaces the previous stub-based simulation layer.

### Native Networking
- **Winsock API Server**: Built-in REST API (Port 11434) compatible with Ollama/OpenAI clients.
- **WinHTTP**: Native HTTP client for cloud connectivity.

## 📦 Build Instructions

### Prerequisites
- Visual Studio 2022 (C++20 support)
- CMake 3.20+
- AVX512-capable CPU (Recommended)

### Building (Native Win32)
```powershell
cd RawrXD
mkdir build -Force
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . --config Release
```

### Verification & finalizing
After a successful build, run the verification suite (Qt-free policy, binary checks, Win32 linkage):
```powershell
.\Verify-Build.ps1 -BuildDir ".\build"
```
All seven checks must pass. Optionally generate the source manifest (≈1450 files in `src` + `Ship`):
```powershell
.\scripts\Digest-SourceManifest.ps1 -OutDir ".\build" -Format both
# or: cmake --build build --target source_manifest
```
Open items and ship checklist: **UNFINISHED_FEATURES.md**. Which IDE exe to run: **IDE_LAUNCH.md**. Mac/Linux: **docs/MAC_LINUX_WRAPPER.md** (Wine bootable space).

## 🏗️ Build Targets

| Target | Description |
|--------|-------------|
| `RawrXD_CLI` | **Pure CLI** — full chat + agentic parity with Win32 IDE (`/chat`, `/agent`, `/smoke`, `/tools`) |
| `RawrEngine` | CLI inference engine + agentic core |
| `RawrXD-Win32IDE` | Full Win32 GUI IDE with all subsystems |
| `RawrXD-InferenceEngine` | **Standalone inference** — loads GGUF, emits tokens, no IDE |
| `rawrxd-monaco-gen` | Monaco/React IDE generator |

### Pure CLI (101% Win32 Parity)
```powershell
cmake --build build_ide --target RawrXD_CLI --config Release
# Output: build_ide/bin/RawrXD_CLI.exe — default port 23959
# Commands: /chat, /agent, /smoke, /tools, /subagent, /chain, /swarm, /autonomy
# See Ship/CLI_PARITY.md and AGENTIC_IDE_INTEGRATION.md
```

### Standalone Inference Engine (Phase 6)
```powershell
cmake --build . --config Release --target RawrXD-InferenceEngine
# Usage:
bin\RawrXD-InferenceEngine.exe model.gguf --prompt "Hello" --tokens 256
bin\RawrXD-InferenceEngine.exe model.gguf --interactive
bin\RawrXD-InferenceEngine.exe model.gguf --benchmark
```

## 🔄 Tier System & Phase Deprecation

The original numbered phase system (Phases 0–46) has been superseded by a tier-based maturity model. Phases 7–17 were **merged into core infrastructure**, not abandoned:

| Old Phase | Status | Where It Went |
|-----------|--------|---------------|
| Phase 7 (Security/Policy) | Merged | `agent_policy.h/cpp` — T3-C Hotpatch Safety |
| Phase 8 (Explainability) | Merged | `agent_explainability.cpp` — Agent Transparency |
| Phase 9 (Swarm I/O) | Merged | ASM init sequence + `swarm_coordinator.cpp` |
| Phase 10 (Orchestration) | Merged | `SafetyContract`, `ConfidenceGate`, `ExecutionGovernor` |
| Phase 11 (Swarm Coordinator) | Merged | `RawrXD_Swarm_Network.asm` + `Win32IDE_SwarmPanel.cpp` |
| Phase 12 (Native Debugger) | Merged | `RawrXD_Debug_Engine.asm` + `Win32IDE_NativeDebugPanel.cpp` |
| Phase 13 | Never defined | — |

**Mac/Linux:** Use `./scripts/rawrxd-space.sh` — see **docs/MAC_LINUX_WRAPPER.md** (Wine bootable space).
| Phase 14 (Hotpatch UI) | Merged | `Win32IDE_HotpatchPanel.cpp` + T3-C |
| Phases 15–16 (CFG/SSA) | Merged | `RawrCodex.asm` prerequisites |
| Phase 17 (Type Recovery) | Merged | `RawrCodex.asm` + `enterprise_license.cpp` |
| Phase 18 (Distributed) | Rewritten | Swarm Subsystem (Phase 21) |

### Current Tier Status
- **T3: COMPLETE** — Telemetry Kernel → Deterministic Replay → Hotpatch Safety
- **T4: COMPLETE** — Autonomous Recovery Orchestrator (divergence → symbolize → fix → verify → commit)
- **Inference Engine: Phase 6 compilation target added** — `RawrXD-InferenceEngine.exe`

## ⚠️ Migration Notes (v2.0 → v3.0)
- **Qt Removal**: All `qtapp/` references are deprecated. The core engine is now `src/agentic_engine.cpp` (Native).
- **Simulations**: Legacy simulation stubs (`cli_extras_stubs.cpp`, `stubs.cpp`) have been removed or neutralized.
- **Config**: Settings are now stored in `settings.json` via pure JSON parsing.

---

**Verification:** `Verify-Build.ps1` · **Open work:** `UNFINISHED_FEATURES.md` · **Gap vs top 50:** `docs/TOP_50_GAP_ANALYSIS.md` · *RawrXD v3.0 — Native AI Development*
>>>>>>> origin/main
