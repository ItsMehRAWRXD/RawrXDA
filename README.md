# RawrXD PE Writer - Implementation Notes

## Overview

This repository contains a PE32+ writer and machine-code emitter implemented in x64 MASM. The writer builds runnable Windows executables from scratch and manages PE headers, sections, imports, and relocation data.

This document is implementation-aligned with `RawrXD_PE_Writer.asm`.

## Public API Surface

Core PE writer entry points:
- `PEWriter_CreateExecutable`
- `PEWriter_AddSection`
- `PEWriter_AddImport`
- `PEWriter_AddCode`
- `PEWriter_AddData`
- `PEWriter_AddBssSpace`
- `PEWriter_AddBaseRelocation`
- `PEWriter_BuildRelocSection`
- `PEWriter_WriteFile`

Machine-code emitter entry points are also exported (`Emit_*`, label/relocation helpers).

## Key Data Structures

- `IMAGE_DOS_HEADER`
- `IMAGE_NT_HEADERS64`
- `IMAGE_SECTION_HEADER`
- `IMAGE_IMPORT_DESCRIPTOR`
- `PE_CONTEXT` (expanded internal context with code/data/import/reloc/resource/exception fields)

## Runtime and Memory Model

The implementation uses Win32 heap APIs directly:
- `GetProcessHeap`
- `HeapAlloc`
- `HeapFree`

No CRT allocation path is required by the writer itself.

## Current Defaults and Limits

- Default image base: `0x140000000`
- Section alignment: `0x1000`
- File alignment: `0x200`
- Subsystem default: `IMAGE_SUBSYSTEM_WINDOWS_CUI`
- `MAX_IMPORTS = 100` (effective limit used by checks: 99 entries)
- `MAX_CODE_SIZE = 0x100000` (1 MiB)
- `MAX_SECTIONS = 16`

## Section Layout Behavior

The writer supports dynamic section management, and `PEWriter_WriteFile` currently normalizes output to a canonical 5-section baseline:
- `.text`
- `.rdata`
- `.idata`
- `.data`
- `.reloc`

Imports and base relocations are written as part of the final image emission path.

## Import Table Handling

Import support includes:
- Multiple DLL descriptors
- DLL deduplication logic
- Import-by-name entries
- IAT/ILT construction
- Import directory population in PE data directories

## Relocation Support

Base relocation support is implemented:
- `PEWriter_AddBaseRelocation`
- `PEWriter_BuildRelocSection`
- `.reloc` section emission in `PEWriter_WriteFile`

## Minimal Usage Example

```assembly
; Create PE context
mov rcx, 0          ; default image base
mov rdx, 1000h      ; entry point RVA
call PEWriter_CreateExecutable
mov rbx, rax

; Add an import
mov rcx, rbx
mov rdx, offset dll_kernel32
mov r8,  offset func_ExitProcess
call PEWriter_AddImport

; Add code bytes
mov rcx, rbx
mov rdx, offset code_buffer
mov r8,  code_size
call PEWriter_AddCode

; Emit PE file
mov rcx, rbx
mov rdx, offset out_name
call PEWriter_WriteFile
```

## Build and Test Notes

- `build.bat` currently runs a CMake Debug build for the repository workspace.
- The standalone PE writer demonstration source is `RawrXD_PE_Writer_Test.asm`.
- The test source writes `test_output.exe`.

## Error Handling Contract

Writer routines follow a simple return contract:
- `0` = failure
- non-zero = success (or valid RVA/handle for APIs that return addresses)

Checks include allocation validation, input checks, capacity bounds, and file I/O result handling.

## Scope Boundaries

Implemented:
- PE32+ header/section emission
- Import table construction
- Code/data/bss support
- Base relocation support
- Machine-code emitter primitives

Out of scope in current writer path:
- Authenticode/digital signing pipeline
- Full production resource compiler workflow (beyond raw section buffer support)
