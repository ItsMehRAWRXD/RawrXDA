; RawrXD_ExtensionHost_Hijacker.asm
; Pure MASM64 beacon-style injection replacing VS Code/Cursor ExtensionHost
; RE: VS Code workbench.html & extensionService.ts
; Build: see Build_ExtensionHost_Hijacker.ps1
; Usage: Run as admin to inject into Code Helper / Cursor Helper.

OPTION CASEMAP:NONE

INCLUDELIB kernel32.lib

; =====================================================================
; Win64 structures (RE: Win32 API)
; =====================================================================
PROCESSENTRY32 STRUCT
    dwSize              DWORD ?
    cntUsage            DWORD ?
    th32ProcessID       DWORD ?
    th32DefaultHeapID    DWORD ?
    th32ModuleID         DWORD ?
    cntThreads          DWORD ?
    th32ParentProcessID DWORD ?
    pcPriClassBase      DWORD ?
    dwFlags             DWORD ?
    szExeFile           BYTE 260 DUP(?)
PROCESSENTRY32 ENDS

; RawrXD Extension Module Header (replaces VSIX)
RAWRXD_EXT_MAGIC        EQU 44585752h   ; 'RWXD'
RAWRXD_EXT_VERSION      EQU 10001h

RAWRXD_EXTENSION STRUCT
    Magic               DWORD ?
    Version             DWORD ?
    Name                BYTE 64 DUP(?)
    Publisher           BYTE 64 DUP(?)
    EntryPointRVA       QWORD ?
    CommandCount        DWORD ?
    CommandTableRVA     QWORD ?
    ActivationEvents    DWORD ?
RAWRXD_EXTENSION ENDS

RAWRXD_COMMAND STRUCT
    CommandId           BYTE 128 DUP(?)
    HandlerRVA          QWORD ?
    Title               BYTE 256 DUP(?)
RAWRXD_COMMAND ENDS

; Offsets for payload base-relative access (PIC)
OFF_CMD_REG             EQU 0000h
OFF_EXT_ACTIVATION      EQU 1000h
OFF_CURRENT_CMD         EQU 1008h
OFF_CMD_TABLE           EQU 2000h
OFF_CMD_COUNT           EQU 3000h
OFF_VSCODE_STDOUT       EQU 3010h
OFF_WRITEFILE_ORIG      EQU 3018h
OFF_RAWRXD_ACTIVE       EQU 3020h

; Kernel32 imports
EXTERN CreateToolhelp32Snapshot:PROC
EXTERN Process32First:PROC
EXTERN Process32Next:PROC
EXTERN OpenProcess:PROC
EXTERN VirtualAllocEx:PROC
EXTERN WriteProcessMemory:PROC
EXTERN CreateRemoteThread:PROC
EXTERN WaitForSingleObject:PROC
EXTERN CloseHandle:PROC

; Constants
PROCESS_ALL_ACCESS      EQU 0010FFFFh
MEM_COMMIT              EQU 1000h
MEM_RESERVE             EQU 2000h
PAGE_EXECUTE_READWRITE  EQU 40h
TH32CS_SNAPPROCESS      EQU 2
INVALID_HANDLE_VALUE    EQU -1

.DATA
ALIGN 8
szCodeHelper            DB "Code Helper", 0
szCursorHelper          DB "Cursor Helper", 0
szCodeHelperExe         DB "Code Helper.exe", 0

; Extension entry-point offsets (relative to RawrXD_Payload) - filled at runtime
ALIGN 8
Extension_EntryPoints    DQ 0, 0, 0

.CODE
; ---------------------------------------------------------------------
; RawrXD_Entry - Main injection orchestrator
; ---------------------------------------------------------------------
RawrXD_Entry PROC
    sub rsp, 88h

    ; Fill extension entry offsets (payload-relative)
    lea rax, Extension_Main_ASM
    lea rcx, RawrXD_Payload
    sub rax, rcx
    lea rdx, Extension_EntryPoints
    mov [rdx], rax
    lea rax, Extension_Main_LSP
    lea rcx, RawrXD_Payload
    sub rax, rcx
    mov [rdx+8], rax

    lea rcx, szCodeHelperExe
    call RawrXD_FindProcess
    test rax, rax
    jz Exit_Fail
    mov r12, rax

    mov rcx, rax
    mov edx, PROCESS_ALL_ACCESS
    xor r8, r8
    call OpenProcess
    test rax, rax
    jz Exit_Fail
    mov r13, rax

    mov rcx, r13
    xor rdx, rdx
    mov r8d, PayloadSize
    mov r9d, MEM_COMMIT
    or r9d, MEM_RESERVE
    mov dword ptr [rsp+28h], PAGE_EXECUTE_READWRITE
    call VirtualAllocEx
    test rax, rax
    jz Exit_Close
    mov r14, rax

    mov rcx, r13
    mov rdx, r14
    lea r8, RawrXD_Payload
    mov r9d, PayloadSize
    xor eax, eax
    mov qword ptr [rsp+28h], rax
    call WriteProcessMemory
    ; (lpNumberOfBytesWritten on stack - not used)
    test rax, rax
    jz Exit_Close

    ; Start payload in remote process
    mov rcx, r13
    xor rdx, rdx
    mov r8, r14
    mov r9, r14
    mov dword ptr [rsp+28h], 0
    mov dword ptr [rsp+30h], 0
    call CreateRemoteThread
    test rax, rax
    jz Exit_Close

    mov rcx, rax
    mov edx, 5000
    call WaitForSingleObject
    mov rcx, rax
    call CloseHandle

    ; Start extension main threads
    lea rbx, Extension_EntryPoints
Inject_Loop:
    mov r8, [rbx]
    test r8, r8
    jz Inject_Done
    mov rcx, r13
    xor rdx, rdx
    mov rax, r14
    add rax, r8
    mov r8, rax
    mov r9, r14
    mov dword ptr [rsp+28h], 0
    mov dword ptr [rsp+30h], 0
    call CreateRemoteThread
    add rbx, 8
    jmp Inject_Loop
Inject_Done:

    mov rcx, r13
    call CloseHandle
    mov eax, 1
    add rsp, 88h
    ret

Exit_Close:
    mov rcx, r13
    call CloseHandle
Exit_Fail:
    xor eax, eax
    add rsp, 88h
    ret
RawrXD_Entry ENDP

; ---------------------------------------------------------------------
; RawrXD_FindProcess - Find Code Helper or Cursor Helper by name
; ---------------------------------------------------------------------
RawrXD_FindProcess PROC
    sub rsp, 328h
    mov dword ptr [rsp+28h], 296

    mov ecx, TH32CS_SNAPPROCESS
    xor edx, edx
    call CreateToolhelp32Snapshot
    cmp rax, INVALID_HANDLE_VALUE
    je Find_Fail
    mov r12, rax

    mov rcx, rax
    lea rdx, [rsp+28h]
    call Process32First
    test rax, rax
    jz Find_Close

Find_Loop:
    lea rsi, [rsp+28h+36]
    lea rdi, szCodeHelper
    mov ecx, 12
    repe cmpsb
    je Find_Target
    lea rsi, [rsp+28h+36]
    lea rdi, szCursorHelper
    mov ecx, 13
    repe cmpsb
    je Find_Target
    mov rcx, r12
    lea rdx, [rsp+28h]
    call Process32Next
    test rax, rax
    jnz Find_Loop
    jmp Find_Close

Find_Target:
    mov eax, dword ptr [rsp+28h+8]
Find_Close:
    mov rcx, r12
    push rax
    call CloseHandle
    pop rax
    add rsp, 328h
    ret
Find_Fail:
    xor eax, eax
    add rsp, 328h
    ret
RawrXD_FindProcess ENDP

; =====================================================================
; PAYLOAD - Injected into VS Code ExtensionHost (position-independent)
; rcx = payload base on entry
; =====================================================================
ALIGN 16
RawrXD_Payload PROC
    push rbx
    push rsi
    push rdi
    push r12
    push r13
    push r14
    push r15
    mov rbp, rsp
    sub rsp, 200h

    mov r15, rcx

    ; Stub init (full hook requires runtime GetProcAddress in payload)
    lea rcx, [r15 + OFF_CMD_REG]
    call Payload_InitRegistry

    ; Main loop - wait / dispatch (stub; real impl would hook pipes)
Payload_Loop:
    lea rcx, [r15 + OFF_EXT_ACTIVATION]
    mov edx, 100
    ; WaitForSingleObject would be resolved at runtime
    ; For stub we just sleep then loop
    jmp Payload_Loop

Payload_InitRegistry:
    ret
RawrXD_Payload ENDP

; ---------------------------------------------------------------------
; Extension: ASM Completion (runs in remote process at payload base)
; ---------------------------------------------------------------------
ALIGN 16
Example_Extension_ASM:
    DD RAWRXD_EXT_MAGIC
    DD RAWRXD_EXT_VERSION
    DB "rawrxd-asm-completion", 0
    DB "ItsMehRAWRXD", 0
    DQ 0
    DD 2
    DQ 0
    DD 2

ALIGN 8
Command_Table_ASM:
    DB "rawrxd.asm.provideCompletionItems", 0
    DQ 0
    DB "Provide ASM Completions", 0
    DB "rawrxd.asm.formatDocument", 0
    DQ 0
    DB "Format ASM Document", 0

Extension_Main_ASM:
    mov r15, rcx    ; payload base for PIC
    mov eax, 1
    ret

Handler_ProvideCompletions:
    lea rax, Completions_Data
    ret

Handler_FormatDoc:
    ; Stub: RawrXD_FormatASM would be implemented in payload
    mov eax, 1
    ret

Completions_Data:
    DB '[{"label":"mov","kind":"keyword"},{"label":"rax","kind":"variable"}]', 0

; ---------------------------------------------------------------------
; Extension: LSP Bridge
; ---------------------------------------------------------------------
ALIGN 16
Example_Extension_LSP:
    DD RAWRXD_EXT_MAGIC
    DD RAWRXD_EXT_VERSION
    DB "rawrxd-lsp-bridge", 0
    DB "ItsMehRAWRXD", 0
    DQ 0
    DD 1
    DQ 0
    DD 1

ALIGN 8
Command_Table_LSP:
    DB "rawrxd.lsp.initialize", 0
    DQ 0
    DB "Initialize LSP Bridge", 0

Extension_Main_LSP:
    mov r15, rcx    ; payload base for PIC
    mov eax, 1
    ret

Handler_LSP_Init:
    mov eax, 1
    ret

; Payload data (must be inside copied region)
ALIGN 8
Payload_Data_Start:
    DQ 0
    DQ 0
    DQ 0
    DD 0
    DB 4096 DUP(0)

Payload_End LABEL BYTE
PayloadSize EQU Payload_End - RawrXD_Payload

END
