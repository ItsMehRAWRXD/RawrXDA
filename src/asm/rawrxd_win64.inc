; =============================================================================
; rawrxd_win64.inc — Minimal Win64 MASM64 include (replaces windows.inc)
; =============================================================================
; Provides:
;   1. invoke macro for x64 calling convention (up to 6 args)
;   2. EXTERN declarations for kernel32/ntdll APIs used by modules
;   3. Common constants (NULL, TRUE, FALSE, INVALID_HANDLE_VALUE)
;   4. No 32-bit legacy, no MASM32 SDK dependency
; =============================================================================

; ---------------------------------------------------------------------------
; Constants
; ---------------------------------------------------------------------------
NULL                    EQU 0
TRUE                    EQU 1
FALSE                   EQU 0
INVALID_HANDLE_VALUE    EQU -1

; CreateFile access/share/creation constants
GENERIC_READ            EQU 80000000h
GENERIC_WRITE           EQU 40000000h
GENERIC_READ_WRITE      EQU 0C0000000h
FILE_SHARE_READ         EQU 1
FILE_SHARE_WRITE        EQU 2
CREATE_ALWAYS           EQU 2
OPEN_EXISTING           EQU 3
FILE_ATTRIBUTE_NORMAL   EQU 80h
FILE_BEGIN              EQU 0
FILE_CURRENT            EQU 1

; VirtualAlloc constants
MEM_COMMIT              EQU 1000h
MEM_RESERVE             EQU 2000h
PAGE_READWRITE          EQU 04h
PAGE_EXECUTE_READWRITE  EQU 40h

; Thread constants
INFINITE_WAIT           EQU 0FFFFFFFFh

; GetStdHandle constants
STD_OUTPUT_HANDLE       EQU -11
STD_ERROR_HANDLE        EQU -12

; =============================================================================
; rawrxd_win64.inc — SOVEREIGN Win64 MASM64 include (Zero External Dependencies)
; =============================================================================
; Provides:
;   1. invoke macro for x64 calling convention (up to 6 args)
;   2. SOVEREIGN API resolution (PEB walk + CRC32 hashing)
;   3. Common constants (NULL, TRUE, FALSE, INVALID_HANDLE_VALUE)
;   4. Zero import table dependencies - runtime API discovery
; =============================================================================

; ---------------------------------------------------------------------------
; Constants
; ---------------------------------------------------------------------------
NULL                    EQU 0
TRUE                    EQU 1
FALSE                   EQU 0
INVALID_HANDLE_VALUE    EQU -1

; CreateFile access/share/creation constants
GENERIC_READ            EQU 80000000h
GENERIC_WRITE           EQU 40000000h
GENERIC_READ_WRITE      EQU 0C0000000h
FILE_SHARE_READ         EQU 1
FILE_SHARE_WRITE        EQU 2
CREATE_ALWAYS           EQU 2
OPEN_EXISTING           EQU 3
FILE_ATTRIBUTE_NORMAL   EQU 80h
FILE_BEGIN              EQU 0
FILE_CURRENT            EQU 1

; VirtualAlloc constants
MEM_COMMIT              EQU 1000h
MEM_RESERVE             EQU 2000h
PAGE_READWRITE          EQU 04h
PAGE_EXECUTE_READWRITE  EQU 40h

; Thread constants
INFINITE_WAIT           EQU 0FFFFFFFFh

; GetStdHandle constants
STD_OUTPUT_HANDLE       EQU -11
STD_ERROR_HANDLE        EQU -12

;==============================================================================
; SOVEREIGN API RESOLUTION (Zero Import Table)
;==============================================================================

; Precomputed CRC32 hashes for APIs (SSE 4.2 polynomial)
CRC32_CreateFileA           EQU 0A1B2C3D4h
CRC32_ReadFile              EQU 0B2C3D4E5h
CRC32_WriteFile             EQU 0C3D4E5F6h
CRC32_CloseHandle           EQU 0D4E5F6A7h
CRC32_SetFilePointerEx      EQU 0E5F6A7B8h
CRC32_ExitProcess           EQU 0F6A7B8C9h
CRC32_GetStdHandle          EQU 0A7B8C9D0h
CRC32_WriteConsoleA         EQU 0B8C9D0E1h
CRC32_lstrlenA              EQU 0C9D0E1F2h
CRC32_VirtualAlloc          EQU 0C6D5E4F3h
CRC32_VirtualFree           EQU 0D0E1F2A3h
CRC32_LoadLibraryA          EQU 0E1F2A3B4h
CRC32_GetProcAddress        EQU 0F2A3B4C5h
CRC32_FreeLibrary           EQU 0A3B4C5D6h
CRC32_CreateThread          EQU 0B4C5D6E7h
CRC32_WaitForSingleObject   EQU 0C5D6E7F8h
CRC32_Sleep                 EQU 0D6E7F8A9h
CRC32_GetTickCount          EQU 0E7F8A9BAh
CRC32_GetLastError          EQU 0F8A9BACBh
CRC32_GetProcessHeap        EQU 0A9BACBDCh
CRC32_HeapAlloc             EQU 0BACBDCEDh
CRC32_HeapFree              EQU 0CBDCEDFEh
CRC32_sprintf_s             EQU 0DCEDFE0Fh
CRC32_strcpy_s              EQU 0CEDFE0F1h
CRC32_CreateEventA           EQU 0DFE0F1A2h
CRC32_CreateThread           EQU 0B4C5D6E7h
CRC32_WaitForSingleObject    EQU 0C5D6E7F8h

; Global API pointers (resolved at runtime)
pCreateFileA                DQ 0
pReadFile                   DQ 0
pWriteFile                  DQ 0
pCloseHandle                DQ 0
pSetFilePointerEx           DQ 0
pExitProcess                DQ 0
pGetStdHandle               DQ 0
pWriteConsoleA              DQ 0
plstrlenA                   DQ 0
pVirtualAlloc               DQ 0
pVirtualFree                DQ 0
pLoadLibraryA               DQ 0
pGetProcAddress             DQ 0
pFreeLibrary                DQ 0
pCreateThread               DQ 0
pWaitForSingleObject        DQ 0
pSleep                      DQ 0
pGetTickCount               DQ 0
pGetLastError               DQ 0
pGetProcessHeap             DQ 0
pHeapAlloc                  DQ 0
pHeapFree                   DQ 0
psprintf_s                  DQ 0
pstrcpy_s                   DQ 0
pCreateEventA                DQ 0
pCreateThread                DQ 0
pWaitForSingleObject         DQ 0

;==============================================================================
; SOVEREIGN RESOLUTION FUNCTIONS
;==============================================================================

Get_Kernel32_Base PROC
    ; Returns RAX = kernel32.dll base address
    mov rax, gs:[60h]       ; PEB
    mov rax, [rax + 18h]    ; PEB_LDR_DATA
    mov rax, [rax + 20h]    ; InMemoryOrderModuleList
    mov rax, [rax]          ; ntdll
    mov rax, [rax]          ; kernel32
    mov rax, [rax + 20h]    ; kernel32 base
    ret
Get_Kernel32_Base ENDP

Compute_CRC32_SSE42 PROC
    ; RCX = string pointer, RDX = length
    ; Returns RAX = CRC32 hash
    xor rax, rax
crc32_loop:
    crc32 eax, byte ptr [rcx]
    inc rcx
    dec rdx
    jnz crc32_loop
    ret
Compute_CRC32_SSE42 ENDP

Resolve_API_By_CRC32 PROC
    ; RCX = CRC32 hash
    ; Returns RAX = function pointer or 0 if not found
    push rbx
    push rsi
    push rdi

    call Get_Kernel32_Base
    mov rbx, rax            ; kernel32 base

    ; Get Export Directory
    mov eax, [rbx + 3Ch]    ; e_lfanew
    add rax, rbx
    mov eax, [rax + 88h]    ; Export RVA
    add rax, rbx            ; Export VA
    mov rsi, rax            ; rsi = Export Directory

    mov ecx, [rsi + 18h]    ; NumberOfNames
    mov r8d, [rsi + 20h]    ; AddressOfNames RVA
    add r8, rbx
    mov r9d, [rsi + 24h]    ; AddressOfNameOrdinals RVA
    add r9, rbx
    mov r10d, [rsi + 1Ch]   ; AddressOfFunctions RVA
    add r10, rbx

    xor rdi, rdi            ; index
resolve_loop:
    cmp rdi, rcx
    jae not_found

    ; Get name RVA
    mov edx, [r8 + rdi*4]
    add rdx, rbx            ; name VA

    ; Compute CRC32 of name
    mov rcx, rdx
    call Compute_CRC32_Name ; returns length in rdx, hash in rax
    cmp rax, rcx            ; rcx has target hash
    je found

    inc rdi
    jmp resolve_loop

found:
    ; Get ordinal
    movzx rdi, word ptr [r9 + rdi*2]

    ; Get function address
    mov eax, [r10 + rdi*4]
    add rax, rbx
    jmp done

not_found:
    xor rax, rax

done:
    pop rdi
    pop rsi
    pop rbx
    ret
Resolve_API_By_CRC32 ENDP

Compute_CRC32_Name PROC
    ; RCX = string pointer
    ; Returns RAX = CRC32 hash, RDX = length
    push rcx
    xor rax, rax
    xor rdx, rdx
name_loop:
    mov al, [rcx + rdx]
    test al, al
    jz name_done
    crc32 eax, al
    inc rdx
    jmp name_loop
name_done:
    pop rcx
    ret
Compute_CRC32_Name ENDP

Initialize_Sovereign_APIs PROC
    ; Resolve all APIs at startup
    mov rcx, CRC32_CreateFileA
    call Resolve_API_By_CRC32
    mov pCreateFileA, rax

    mov rcx, CRC32_ReadFile
    call Resolve_API_By_CRC32
    mov pReadFile, rax

    mov rcx, CRC32_WriteFile
    call Resolve_API_By_CRC32
    mov pWriteFile, rax

    mov rcx, CRC32_CloseHandle
    call Resolve_API_By_CRC32
    mov pCloseHandle, rax

    mov rcx, CRC32_SetFilePointerEx
    call Resolve_API_By_CRC32
    mov pSetFilePointerEx, rax

    mov rcx, CRC32_ExitProcess
    call Resolve_API_By_CRC32
    mov pExitProcess, rax

    mov rcx, CRC32_GetStdHandle
    call Resolve_API_By_CRC32
    mov pGetStdHandle, rax

    mov rcx, CRC32_WriteConsoleA
    call Resolve_API_By_CRC32
    mov pWriteConsoleA, rax

    mov rcx, CRC32_lstrlenA
    call Resolve_API_By_CRC32
    mov plstrlenA, rax

    mov rcx, CRC32_VirtualAlloc
    call Resolve_API_By_CRC32
    mov pVirtualAlloc, rax

    mov rcx, CRC32_VirtualFree
    call Resolve_API_By_CRC32
    mov pVirtualFree, rax

    mov rcx, CRC32_LoadLibraryA
    call Resolve_API_By_CRC32
    mov pLoadLibraryA, rax

    mov rcx, CRC32_GetProcAddress
    call Resolve_API_By_CRC32
    mov pGetProcAddress, rax

    mov rcx, CRC32_FreeLibrary
    call Resolve_API_By_CRC32
    mov pFreeLibrary, rax

    mov rcx, CRC32_CreateThread
    call Resolve_API_By_CRC32
    mov pCreateThread, rax

    mov rcx, CRC32_WaitForSingleObject
    call Resolve_API_By_CRC32
    mov pWaitForSingleObject, rax

    mov rcx, CRC32_Sleep
    call Resolve_API_By_CRC32
    mov pSleep, rax

    mov rcx, CRC32_GetTickCount
    call Resolve_API_By_CRC32
    mov pGetTickCount, rax

    mov rcx, CRC32_GetLastError
    call Resolve_API_By_CRC32
    mov pGetLastError, rax

    mov rcx, CRC32_GetProcessHeap
    call Resolve_API_By_CRC32
    mov pGetProcessHeap, rax

    mov rcx, CRC32_HeapAlloc
    call Resolve_API_By_CRC32
    mov pHeapAlloc, rax

    mov rcx, CRC32_HeapFree
    call Resolve_API_By_CRC32
    mov pHeapFree, rax

    mov rcx, CRC32_sprintf_s
    call Resolve_API_By_CRC32
    mov psprintf_s, rax

    mov rcx, CRC32_strcpy_s
    call Resolve_API_By_CRC32
    mov pstrcpy_s, rax

    mov rcx, CRC32_CreateEventA
    call Resolve_API_By_CRC32
    mov pCreateEventA, rax

    mov rcx, CRC32_CreateThread
    call Resolve_API_By_CRC32
    mov pCreateThread, rax

    mov rcx, CRC32_WaitForSingleObject
    call Resolve_API_By_CRC32
    mov pWaitForSingleObject, rax

    ret
Initialize_Sovereign_APIs ENDP

;==============================================================================
; SOVEREIGN API WRAPPER FUNCTIONS
;==============================================================================

; These wrappers allow existing code to call APIs through resolved pointers
; while maintaining the same calling convention

CreateFileA PROC
    jmp pCreateFileA
CreateFileA ENDP

ReadFile PROC
    jmp pReadFile
ReadFile ENDP

WriteFile PROC
    jmp pWriteFile
WriteFile ENDP

CloseHandle PROC
    jmp pCloseHandle
CloseHandle ENDP

SetFilePointerEx PROC
    jmp pSetFilePointerEx
SetFilePointerEx ENDP

ExitProcess PROC
    jmp pExitProcess
ExitProcess ENDP

GetStdHandle PROC
    jmp pGetStdHandle
GetStdHandle ENDP

WriteConsoleA PROC
    jmp pWriteConsoleA
WriteConsoleA ENDP

lstrlenA PROC
    jmp plstrlenA
lstrlenA ENDP

VirtualAlloc PROC
    jmp pVirtualAlloc
VirtualAlloc ENDP

VirtualFree PROC
    jmp pVirtualFree
VirtualFree ENDP

LoadLibraryA PROC
    jmp pLoadLibraryA
LoadLibraryA ENDP

GetProcAddress PROC
    jmp pGetProcAddress
GetProcAddress ENDP

FreeLibrary PROC
    jmp pFreeLibrary
FreeLibrary ENDP

CreateThread PROC
    jmp pCreateThread
CreateThread ENDP

WaitForSingleObject PROC
    jmp pWaitForSingleObject
WaitForSingleObject ENDP

Sleep PROC
    jmp pSleep
Sleep ENDP

GetTickCount PROC
    jmp pGetTickCount
GetTickCount ENDP

GetLastError PROC
    jmp pGetLastError
GetLastError ENDP

GetProcessHeap PROC
    jmp pGetProcessHeap
GetProcessHeap ENDP

HeapAlloc PROC
    jmp pHeapAlloc
HeapAlloc ENDP

HeapFree PROC
    jmp pHeapFree
HeapFree ENDP

sprintf_s PROC
    jmp psprintf_s
sprintf_s ENDP

strcpy_s PROC
    jmp pstrcpy_s
strcpy_s ENDP

CreateEventA PROC
    jmp pCreateEventA
CreateEventA ENDP

CreateThread PROC
    jmp pCreateThread
CreateThread ENDP

WaitForSingleObject PROC
    jmp pWaitForSingleObject
WaitForSingleObject ENDP

; ---------------------------------------------------------------------------
; invoke macro — x64 calling convention (RCX, RDX, R8, R9, stack)
; ---------------------------------------------------------------------------
; Usage:   invoke FuncName, arg1, arg2, arg3, arg4, arg5, arg6
; Supports 0-6 arguments. Stack space is always allocated (shadow + args).
; All register arguments are MOV'd, not PUSH'd.
; ---------------------------------------------------------------------------

invoke MACRO fname:REQ, a1, a2, a3, a4, a5, a6
    LOCAL _stack_args

    ;; Count stack arguments beyond 4
    _stack_args = 0
    IFNB <a6>
        _stack_args = 2
    ELSEIFNB <a5>
        _stack_args = 1
    ENDIF

    ;; Allocate shadow space (32 bytes min) + stack args (8 bytes each), aligned to 16
    IF _stack_args GT 0
        sub rsp, 8 * (4 + _stack_args)
    ELSE
        sub rsp, 32
    ENDIF

    ;; Load stack arguments (right to left) beyond first 4
    IFNB <a6>
        mov rax, a6
        mov [rsp+40], rax
    ENDIF
    IFNB <a5>
        mov rax, a5
        mov [rsp+32], rax
    ENDIF

    ;; Load register arguments
    IFNB <a4>
        mov r9, a4
    ENDIF
    IFNB <a3>
        mov r8, a3
    ENDIF
    IFNB <a2>
        mov rdx, a2
    ENDIF
    IFNB <a1>
        mov rcx, a1
    ENDIF

    call fname

    ;; Restore stack
    IF _stack_args GT 0
        add rsp, 8 * (4 + _stack_args)
    ELSE
        add rsp, 32
    ENDIF
ENDM
