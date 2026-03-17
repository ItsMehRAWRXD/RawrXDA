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