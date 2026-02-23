# Reverse Engineering — Portable x64 MASM Loader & Operational Requirements

**Scope:** Released, finalized environments only (official, non–beta/non–alpha). This document describes **operational requirements** as they sit (independent of host or corporate constraints) and a **pure x64 MASM** pattern for portable streaming/loading and execution on Windows (e.g. thumb-drive IDE, Windows 11).

---

## 1. Operational Requirements (Valid Area for Customization)

**Concept:** Define the “real” requirements for a system **as it sits**—i.e. what is strictly needed for the task—regardless of where it was built or what environment it came from. This creates a **valid area for customization** without depending on Silicon Valley–style product requirements or hosted-API guardrails.

- Requirements are expressed in terms of **functional necessity**: inputs, outputs, and execution constraints.
- Hosting reality is unchanged: the code still runs on real hardware (e.g. thumb drive on Windows 11); “jailbreak” does not remove the need for a host.
- Implementation is **pure x64 MASM** where applicable: no high-level wrappers, minimal dependencies (e.g. kernel32 only), so behavior is determined by the logic you write, not by external policy layers.

Use only in environments and for workloads you are authorized to control.

---

## 2. Minimal x64 MASM Entry (Windows)

Raw execution entry with no WinAPI dependency beyond the need to return cleanly. Placeholder for system-level logic.

```asm
; Build: ml64.exe /c /Zi main.asm
; Link: link.exe /subsystem:console /entry:main kernel32.lib main.obj

.code
main proc
    xor rax, rax
    ; [System-level operational logic]
    ret
main endp
end
```

---

## 3. Windows 11 Thumb-Drive Console (Portable)

Uses Windows x64 calling convention: **32-byte shadow space** before any `call`, and **16-byte stack alignment** before `call`. Suitable for a portable binary run from a thumb drive (e.g. `D:\` or current drive).

- **Build:** `ml64.exe /c /Zi main.asm`  
- **Link:** `link.exe /subsystem:console /entry:main kernel32.lib user32.lib main.obj`

```asm
ExitProcess    PROTO
GetStdHandle   PROTO
WriteConsoleA  PROTO

.data
    msg      db "Operational: x64 MASM on Windows 11", 0Dh, 0Ah, 0
    msgLen   equ $ - msg
    written  dq 0

.code
main proc
    sub rsp, 28h              ; Shadow space + alignment (32 + 8)

    mov rcx, -11              ; STD_OUTPUT_HANDLE
    call GetStdHandle
    mov r12, rax

    mov rcx, r12
    lea rdx, msg
    mov r8, msgLen
    lea r9, written
    mov qword ptr [rsp+20h], 0
    call WriteConsoleA

    xor rcx, rcx
    call ExitProcess

    add rsp, 28h
    ret
main endp
end
```

- **Shadow space:** Caller reserves 32 bytes (e.g. `[rsp+20h]` for 5th parameter) even when args are in RCX, RDX, R8, R9.
- **Stack alignment:** After `push` of return address, `sub rsp, 28h` keeps 16-byte alignment before the next `call`.

---

## 4. Pure x64 MASM Streaming Loader (Model as Raw Module)

**Goal:** Treat “model” files on the drive as **already present**; stream them into memory and execute as code. No dependency on higher-level frameworks; only kernel32. Suited to thumb-drive or fixed path (e.g. `D:\models\engine.bin`).

**Behavior:**

1. **CreateFileA** — open the file (e.g. model path on the drive).
2. **GetFileSize** — obtain size for allocation.
3. **VirtualAlloc** — reserve/commit with **PAGE_EXECUTE_READWRITE** so the region can hold streamed bytes and then be executed.
4. **ReadFile** — stream file contents into that region.
5. **Optional:** **FlushInstructionCache** — so the CPU treats the loaded bytes as valid code.
6. **Call** the base of the allocated region (or a known offset) to run the loaded “requirement” module.

**Build:** `ml64.exe /c loader.asm && link.exe /subsystem:console /entry:main kernel32.lib loader.obj`

```asm
; loader.asm — stream file into executable memory and run

CreateFileA    PROTO
GetFileSize    PROTO
VirtualAlloc   PROTO
ReadFile       PROTO
ExitProcess    PROTO
FlushInstructionCache PROTO

.data
    modelPath  db "D:\models\engine.bin", 0   ; set to your path (e.g. thumb drive)
    hFile      dq 0
    hMemory    dq 0
    bytesRead  dd 0
    fileSize   dq 0

.code
main proc
    sub rsp, 48h             ; Shadow + args for CreateFileA

    ; 1. Open file
    lea rcx, modelPath
    mov edx, 80000000h      ; GENERIC_READ
    mov r8d, 1              ; FILE_SHARE_READ
    xor r9d, r9d
    mov qword ptr [rsp+20h], 3   ; OPEN_EXISTING
    mov qword ptr [rsp+28h], 80h ; FILE_ATTRIBUTE_NORMAL
    mov qword ptr [rsp+30h], 0   ; NULL template
    call CreateFileA
    mov hFile, rax
    cmp rax, -1
    je bail

    ; 2. File size
    mov rcx, [hFile]
    xor edx, edx
    call GetFileSize
    mov fileSize, rax

    ; 3. Allocate executable memory
    xor ecx, ecx
    mov rdx, [fileSize]
    mov r8d, 3000h          ; MEM_COMMIT | MEM_RESERVE
    mov r9d, 40h            ; PAGE_EXECUTE_READWRITE
    call VirtualAlloc
    mov hMemory, rax
    test rax, rax
    jz bail

    ; 4. Stream into memory
    mov rcx, [hFile]
    mov rdx, [hMemory]
    mov r8d, dword ptr [fileSize]
    lea r9, bytesRead
    mov qword ptr [rsp+20h], 0   ; NULL overlapped
    call ReadFile

    ; 5. Optional: flush instruction cache
    mov rcx, -1             ; current process
    mov rdx, [hMemory]
    mov r8, [fileSize]
    call FlushInstructionCache

    ; 6. Execute loaded module
    call qword ptr [hMemory]

bail:
    xor ecx, ecx
    call ExitProcess
main endp
end
```

**Notes:**

- **Path:** Change `modelPath` to your actual path (e.g. thumb drive letter). For dynamic drive discovery, use **GetLogicalDrives** (or similar) and scan for the volume that contains your model directory.
- **Portability:** Only kernel32 is used; the binary can run on any Windows 11 (or compatible) host that can see the drive.
- **Security:** Allocating PAGE_EXECUTE_READWRITE and executing streamed bytes disables normal DEP for that region. Use only for trusted, locally controlled model/code files and only in authorized environments.

---

## 5. Where This Fits

- **Decompilation & x64:** [REVERSE_ENGINEERING_DECOMPILATION_AND_X64.md](REVERSE_ENGINEERING_DECOMPILATION_AND_X64.md)
- **Game development (post-release):** [REVERSE_ENGINEERING_GAME_DEVELOPMENT.md](REVERSE_ENGINEERING_GAME_DEVELOPMENT.md)
- **Source digestion & alignment:** [REVERSE_ENGINEERING_SOURCE_DIGESTION_AND_ALIGNMENT.md](REVERSE_ENGINEERING_SOURCE_DIGESTION_AND_ALIGNMENT.md)
- **General RE:** [REVERSE_ENGINEERING_GUIDE.md](REVERSE_ENGINEERING_GUIDE.md)
- **Suite:** `src/reverse_engineering/` and [RE_ARCHITECTURE.md](../src/reverse_engineering/RE_ARCHITECTURE.md)

Use only in **released, finalized** (official, non–beta/non–alpha) contexts and only where you have authority to run and customize the system.
