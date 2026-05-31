; ============================================================================
; extension_process_broker.asm — MASM64 Extension Process Broker
; ============================================================================
; Pure x64 MASM primitives for process spawn and IPC.
; Exports C-callable routines used by process_broker.cpp.
; Assemble with: ml64.exe /c /Zi /Zd extension_process_broker.asm
; ============================================================================

.code

; ----------------------------------------------------------------------------
; Broker_SpawnProcess
;   rcx = LPSTR  cmdLine
;   rdx = LPSTR  workingDir (nullable)
;   r8  = PHANDLE hProcessOut
;   r9  = PHANDLE hThreadOut
;   [rsp+0x28] = PDWORD pidOut
; Returns: rax = TRUE on success, FALSE on failure
; ----------------------------------------------------------------------------
Broker_SpawnProcess PROC FRAME
    push    rbx
    .pushreg rbx
    push    rsi
    .pushreg rsi
    push    rdi
    .pushreg rdi
    sub     rsp, 128
    .allocstack 128
    .endprolog

    mov     rbx, rcx            ; cmdLine
    mov     rsi, rdx            ; workingDir
    mov     rdi, r8             ; hProcessOut
    mov     r10, r9             ; hThreadOut
    mov     r11, [rsp+128+0x28] ; pidOut

    ; Zero local STARTUPINFOA (104 bytes) + PROCESS_INFORMATION (24 bytes)
    lea     rcx, [rsp+0]
    xor     eax, eax
    mov     edx, 128
    rep stosb
    lea     rdi, [rsp+0]        ; restore rdi

    ; STARTUPINFOA.cb = sizeof(STARTUPINFOA)
    lea     rax, [rsp+0]
    mov     dword ptr [rax], 104

    ; CreateProcessA
    xor     ecx, ecx            ; lpApplicationName = NULL
    mov     rdx, rbx            ; lpCommandLine
    xor     r8, r8              ; lpProcessAttributes = NULL
    xor     r9, r9              ; lpThreadAttributes = NULL
    mov     qword ptr [rsp+0x20], 0 ; bInheritHandles = FALSE
    mov     qword ptr [rsp+0x28], 0x00000200 ; dwCreationFlags = CREATE_SUSPENDED
    mov     qword ptr [rsp+0x30], 0 ; lpEnvironment = NULL
    mov     rax, rsi
    test    rax, rax
    jnz     @F
    xor     rax, rax
@@: mov     qword ptr [rsp+0x38], rax ; lpCurrentDirectory
    lea     rax, [rsp+0]
    mov     qword ptr [rsp+0x40], rax ; lpStartupInfo
    lea     rax, [rsp+64]
    mov     qword ptr [rsp+0x48], rax ; lpProcessInformation

    call    CreateProcessA
    test    eax, eax
    jz      .fail

    ; Copy out handles
    lea     rax, [rsp+64]
    mov     ecx, [rax+16]       ; dwProcessId
    mov     [r11], ecx
    mov     rcx, [rax]          ; hProcess
    mov     [rdi], rcx
    mov     rcx, [rax+8]        ; hThread
    mov     [r10], rcx
    mov     eax, 1
    jmp     .done

.fail:
    xor     eax, eax
.done:
    add     rsp, 128
    pop     rdi
    pop     rsi
    pop     rbx
    ret
Broker_SpawnProcess ENDP

; ----------------------------------------------------------------------------
; Broker_CreateJobObjectWithLimits
;   rcx = HANDLE hProcess
;   rdx = SIZE_T memoryLimitBytes
;   r8  = DWORD  activeProcessLimit
; Returns: rax = job handle or NULL
; ----------------------------------------------------------------------------
Broker_CreateJobObjectWithLimits PROC FRAME
    push    rbx
    .pushreg rbx
    push    rsi
    .pushreg rsi
    sub     rsp, 176
    .allocstack 176
    .endprolog

    mov     rbx, rcx            ; hProcess
    mov     rsi, rdx            ; memoryLimitBytes

    ; CreateJobObjectA(NULL, NULL)
    xor     ecx, ecx
    xor     edx, edx
    call    CreateJobObjectA
    test    rax, rax
    jz      .fail

    mov     r10, rax            ; hJob

    ; JOBOBJECT_EXTENDED_LIMIT_INFORMATION = 144 bytes on x64
    lea     rcx, [rsp+0]
    xor     eax, eax
    mov     edx, 176
    rep stosb
    lea     rdi, [rsp+0]

    ; LimitFlags
    lea     rax, [rsp+0]
    mov     dword ptr [rax+0], 0x00000200 | 0x00000008 | 0x00002000
    ; JOB_OBJECT_LIMIT_JOB_MEMORY | JOB_OBJECT_LIMIT_ACTIVE_PROCESS | JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE
    mov     [rax+8], rsi        ; JobMemoryLimit
    mov     ecx, r8d
    mov     [rax+16], ecx       ; ActiveProcessLimit

    ; SetInformationJobObject
    mov     rcx, r10
    mov     edx, 9              ; JobObjectExtendedLimitInformation
    lea     r8, [rsp+0]
    mov     r9d, 176
    call    SetInformationJobObject

    ; AssignProcessToJobObject
    mov     rcx, r10
    mov     rdx, rbx
    call    AssignProcessToJobObject

    mov     rax, r10
    jmp     .done

.fail:
    xor     rax, rax
.done:
    add     rsp, 176
    pop     rsi
    pop     rbx
    ret
Broker_CreateJobObjectWithLimits ENDP

; ----------------------------------------------------------------------------
; Broker_WritePipeFrame
;   rcx = HANDLE hPipe
;   rdx = PVOID  header (16 bytes: magic, type, len, crc)
;   r8  = PVOID  payload
;   r9  = DWORD  payloadLen
; Returns: rax = TRUE on success
; ----------------------------------------------------------------------------
Broker_WritePipeFrame PROC FRAME
    push    rbx
    .pushreg rbx
    push    rsi
    .pushreg rsi
    sub     rsp, 40
    .allocstack 40
    .endprolog

    mov     rbx, rcx            ; hPipe
    mov     rsi, rdx            ; header

    ; Write header
    mov     rcx, rbx
    mov     rdx, rsi
    mov     r8d, 16
    lea     r9, [rsp+0x20]      ; lpNumberOfBytesWritten
    mov     qword ptr [rsp+0x28], 0 ; lpOverlapped = NULL
    call    WriteFile
    test    eax, eax
    jz      .fail
    cmp     dword ptr [rsp+0x20], 16
    jne     .fail

    ; Write payload if any
    test    r9d, r9d
    jz      .ok
    mov     rcx, rbx
    mov     rdx, r8
    mov     r8d, r9d
    lea     r9, [rsp+0x20]
    mov     qword ptr [rsp+0x28], 0
    call    WriteFile
    test    eax, eax
    jz      .fail

.ok:
    mov     eax, 1
    jmp     .done
.fail:
    xor     eax, eax
.done:
    add     rsp, 40
    pop     rsi
    pop     rbx
    ret
Broker_WritePipeFrame ENDP

; ----------------------------------------------------------------------------
; Broker_ReadPipeFrame
;   rcx = HANDLE hPipe
;   rdx = PVOID  headerOut (16 bytes)
;   r8  = PVOID  payloadBuf
;   r9  = DWORD  payloadBufSize
;   [rsp+0x28] = PDWORD payloadReadOut
; Returns: rax = TRUE on success
; ----------------------------------------------------------------------------
Broker_ReadPipeFrame PROC FRAME
    push    rbx
    .pushreg rbx
    push    rsi
    .pushreg rsi
    push    rdi
    .pushreg rdi
    sub     rsp, 48
    .allocstack 48
    .endprolog

    mov     rbx, rcx            ; hPipe
    mov     rsi, rdx            ; headerOut
    mov     rdi, r8             ; payloadBuf
    mov     r10, r9             ; payloadBufSize
    mov     r11, [rsp+48+0x28]  ; payloadReadOut

    ; Read header
    mov     rcx, rbx
    mov     rdx, rsi
    mov     r8d, 16
    lea     r9, [rsp+0x20]
    mov     qword ptr [rsp+0x28], 0
    call    ReadFile
    test    eax, eax
    jz      .fail
    cmp     dword ptr [rsp+0x20], 16
    jne     .fail

    ; Validate magic
    cmp     dword ptr [rsi], 0x5242574D
    jne     .fail

    ; Get payload length
    mov     ecx, [rsi+8]        ; payloadLen
    test    ecx, ecx
    jz      .ok_empty
    cmp     ecx, r10d
    ja      .fail               ; buffer too small

    ; Read payload
    mov     rcx, rbx
    mov     rdx, rdi
    mov     r8d, ecx
    lea     r9, [rsp+0x20]
    mov     qword ptr [rsp+0x28], 0
    call    ReadFile
    test    eax, eax
    jz      .fail
    mov     eax, [rsp+0x20]
    mov     [r11], eax

.ok:
    mov     eax, 1
    jmp     .done
.ok_empty:
    mov     dword ptr [r11], 0
    mov     eax, 1
    jmp     .done
.fail:
    xor     eax, eax
.done:
    add     rsp, 48
    pop     rdi
    pop     rsi
    pop     rbx
    ret
Broker_ReadPipeFrame ENDP

; ----------------------------------------------------------------------------
; Broker_TerminateProcess
;   rcx = HANDLE hProcess
;   rdx = UINT   exitCode
; Returns: rax = TRUE on success
; ----------------------------------------------------------------------------
Broker_TerminateProcess PROC FRAME
    sub     rsp, 8
    .allocstack 8
    .endprolog
    call    TerminateProcess
    add     rsp, 8
    ret
Broker_TerminateProcess ENDP

; ----------------------------------------------------------------------------
; Broker_GetProcessPeakMemory
;   rcx = HANDLE hProcess
; Returns: rax = peak working set size in bytes
; ----------------------------------------------------------------------------
Broker_GetProcessPeakMemory PROC FRAME
    push    rbx
    .pushreg rbx
    sub     rsp, 80
    .allocstack 80
    .endprolog

    mov     rbx, rcx
    lea     rdx, [rsp+0]
    mov     r8d, 80
    call    GetProcessMemoryInfo
    test    eax, eax
    jz      .fail
    mov     rax, [rsp+32]       ; PeakWorkingSetSize offset in PROCESS_MEMORY_COUNTERS
    jmp     .done
.fail:
    xor     rax, rax
.done:
    add     rsp, 80
    pop     rbx
    ret
Broker_GetProcessPeakMemory ENDP

END
