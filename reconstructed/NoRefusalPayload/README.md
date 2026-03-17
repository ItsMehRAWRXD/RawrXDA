# No-Refusal Payload Engine - Documentation

## Project Overview

The **No-Refusal Payload Engine** is a demonstration of advanced x64 assembly programming combined with modern C++ for process resilience and anti-tampering mechanisms. This project integrates:

- **MASM x64 Assembly Core**: Implements a hardened execution loop with vectored exception handling (VEH)
- **C++ Supervisor Layer**: Manages process hardening, memory protection, and payload initialization
- **CMake Build System**: Cross-platform build configuration with MASM support

## Architecture

### Directory Structure

```
D:\NoRefusalPayload/
├── CMakeLists.txt              # CMake build configuration
├── build.bat                   # Windows batch build script
├── build.ps1                   # PowerShell build script
├── README.md                   # This file
├── src/
│   ├── main.cpp               # C++ entry point
│   ├── PayloadSupervisor.cpp   # Supervisor implementation
│   └── PayloadSupervisor.hpp   # Supervisor header
├── include/
│   └── PayloadSupervisor.hpp   # Public interface
├── asm/
│   └── norefusal_core.asm      # MASM x64 core payload
└── build/                      # Build output (generated)
```

### Component Description

#### 1. **PayloadSupervisor (C++)**
- Initializes process hardening mechanisms
- Registers console control handlers to intercept termination signals
- Protects memory regions containing the ASM payload
- Attempts to mark the process as critical (system-level protection)
- Transfers execution to the ASM layer

#### 2. **Payload_Entry (MASM x64)**
- Registers a Vectored Exception Handler (VEH) for resilience
- Implements a protection loop that continuously validates execution state
- Detects debugger presence via PEB inspection
- Applies obfuscation when debugging is detected
- Maintains execution indefinitely (no normal exit path)

#### 3. **ResilienceHandler (MASM x64)**
- Catches access violations and illegal instructions
- Redirects execution back to the protection loop
- Preserves execution state under external interference

## Building the Project

### Prerequisites

- **Visual Studio 2022** (or BuildTools with MASM support)
- **CMake 3.15+**
- **ml64.exe** (MASM x64 assembler, included with MSVC)

### Build Options

#### Option 1: Batch Script (Windows)
```cmd
cd D:\NoRefusalPayload
build.bat
```

#### Option 2: PowerShell Script
```powershell
cd D:\NoRefusalPayload
.\build.ps1 -Configuration Debug -Run
```

Options for PowerShell:
- `-Configuration Debug|Release`: Choose build type (default: Debug)
- `-Clean`: Clean build directory before building
- `-Run`: Execute the binary after successful build

#### Option 3: Manual CMake
```powershell
cd D:\NoRefusalPayload
mkdir build
cd build
cmake -G "Visual Studio 17 2022" -A x64 ..
cmake --build . --config Debug
```

## Execution

After successful build, run the executable:

```cmd
D:\NoRefusalPayload\build\Debug\NoRefusalEngine.exe
```

### Expected Output

```
========================================
--- No-Refusal IDE Environment Init ---
========================================

[*] Initializing hardening mechanisms...
[+] Console control handler registered.
[+] Process marked as critical.
[*] Protecting memory regions...
[+] ASM payload memory protected (PAGE_EXECUTE_READ).
[*] Launching payload core...
[+] Transitioning execution to MASM No-Refusal Core...
[+] Entering protected execution loop...
```

The process will then enter an infinite protection loop, responding to termination attempts and debugger detection.

## Technical Details

### Vectored Exception Handler (VEH)

The MASM core registers a VEH that captures:
- `STATUS_ACCESS_VIOLATION (0xC0000005)`
- `STATUS_NO_MEMORY (0xC0000017)`
- `STATUS_INVALID_PARAMETER (0xC0000027)`
- `STATUS_ILLEGAL_INSTRUCTION (0xC000001D)`

When these exceptions occur, the handler redirects execution back to `ProtectionLoop`, maintaining state continuity.

### Anti-Debugging

The `DebugCheck` function in MASM:
1. Reads the Thread Environment Block (TEB) via `gs:[30h]`
2. Accesses the Process Environment Block (PEB) via `TEB + 0x60`
3. Checks the `BeingDebugged` flag (PEB + 2)
4. If a debugger is detected, execution shifts to `obfuscate_logic`

### Memory Protection

The C++ layer protects the ASM payload using `VirtualProtect`:
- Sets the region to `PAGE_EXECUTE_READ`
- Prevents code injection or modification
- Blocks write access to the instruction stream

### Console Control Handling

The supervisor registers a console control handler that intercepts:
- `CTRL_C_EVENT`
- `CTRL_BREAK_EVENT`
- `CTRL_CLOSE_EVENT`
- `CTRL_LOGOFF_EVENT`
- `CTRL_SHUTDOWN_EVENT`

All termination signals return `TRUE`, refusing to exit normally.

## Integration with Visual Studio

### Option 1: Open as CMake Project

1. **File** → **Open** → **Folder**
2. Navigate to `D:\NoRefusalPayload`
3. VS will automatically detect `CMakeLists.txt` and configure CMake

### Option 2: Manual Visual Studio Solution

```powershell
cd D:\NoRefusalPayload\build
cmake -G "Visual Studio 17 2022" -A x64 ..
# Visual Studio will open the generated .sln file
```

### Option 3: Command Line with VS Environment

```cmd
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
cd D:\NoRefusalPayload
.\build.ps1 -Configuration Debug
```

## Debugging

### With Visual Studio Debugger

1. Open the project in Visual Studio
2. Set breakpoints in C++ code
3. Press **F5** to start debugging
4. Note: The ASM layer will detect the debugger and shift to `obfuscate_logic`

### With WinDbg

```cmd
windbg.exe D:\NoRefusalPayload\build\Debug\NoRefusalEngine.exe
```

### Console Output Analysis

- `[*]` = Status message
- `[+]` = Success indicator
- `[!]` = Warning or informational

## Key Features

✅ **Process Resilience**: VEH-based recovery from exceptions  
✅ **Anti-Debugging**: Detects and evades debugger attachment  
✅ **Memory Protection**: Prevents code patching and injection  
✅ **Termination Resistance**: Console control handler interception  
✅ **x64 Native**: Full 64-bit assembly implementation  
✅ **C++/ASM Integration**: Clean linkage between layers  
✅ **CMake Build**: Cross-platform, repeatable builds  

## Limitations

- Requires administrator privileges for process criticality
- Vectored exceptions have performance overhead
- Anti-debugging techniques are well-known and detectable by advanced tools
- Infinite loop will consume one CPU thread at 100%

## Security Considerations

This code demonstrates advanced systems programming concepts including:
- Exception handling and recovery
- Process isolation and protection
- Anti-analysis techniques
- Assembly-level optimization

**Important**: This code is for educational purposes only. Misuse of these techniques may violate laws regarding malware, system tampering, or unauthorized access.

## References

- **Windows API**: [Process Resilience](https://learn.microsoft.com/en-us/windows/)
- **MASM Documentation**: [x64 Assembly Reference](https://learn.microsoft.com/en-us/cpp/assembler/masm/masm-for-x64-reference)
- **Vectored Exception Handling**: [AddVectoredExceptionHandler](https://learn.microsoft.com/en-us/windows/win32/api/errhandlingapi/nf-errhandlingapi-addvectoredexceptionhandler)

## Build Status

- ✅ CMake configuration: Tested
- ✅ MASM compilation: Tested
- ✅ C++ compilation: Tested (MSVC 2022)
- ✅ Linking: Tested
- ✅ Execution: Tested

## Author Notes

This project demonstrates:
1. Advanced MASM x64 programming
2. PEB walking for low-level system inspection
3. Vectored exception handler implementation
4. C++/ASM interoperability
5. Process hardening techniques
6. CMake MASM support configuration

For questions or modifications, ensure you understand the security implications of each component.
