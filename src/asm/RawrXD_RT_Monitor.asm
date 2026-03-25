; =============================================================================
; RawrXD_RT_Monitor.asm — Pure x64 MASM real-time monitor
; No Qt, no CRT, no std:: dependencies.
; =============================================================================

option casemap:none

include RawrXD_Common.inc

; --- RRTM buffer size constants ---
RRTM_LAST_BUILD_SIZE    EQU     128
RRTM_LAST_AGENT_SIZE    EQU     64
RRTM_LAST_STATUS_SIZE   EQU     96
RRTM_LAST_MODEL_SIZE    EQU     96
RRTM_CRLF_LEN           EQU     2

EXTERN OutputDebugStringA:PROC
EXTERN CreateFileA:PROC
EXTERN WriteFile:PROC
EXTERN FlushFileBuffers:PROC
EXTERN CloseHandle:PROC

PUBLIC RRTM_Init
PUBLIC RRTM_RecordBuildEvent
PUBLIC RRTM_RecordAgentEvent
PUBLIC RRTM_RecordModelEvent
PUBLIC RRTM_SnapshotStatus
PUBLIC RRTM_Flush
PUBLIC RRTM_Shutdown

.data
    g_RRTM_Initialized      dq 0
    g_RRTM_LogHandle        dq INVALID_HANDLE_VALUE

    g_RRTM_BuildEvents      dq 0
    g_RRTM_AgentEvents      dq 0
    g_RRTM_ModelEvents      dq 0
    g_RRTM_LastTokens       dq 0

    g_RRTM_LastBuild        db RRTM_LAST_BUILD_SIZE dup(0)
    g_RRTM_LastAgent        db RRTM_LAST_AGENT_SIZE dup(0)
    g_RRTM_LastStatus       db RRTM_LAST_STATUS_SIZE dup(0)
    g_RRTM_LastModel        db RRTM_LAST_MODEL_SIZE dup(0)

    szRTLogPath             db "rawrxd_rt_monitor.log", 0
    szCRLF                  db 13, 10, 0
    szInit                  db "[RRTM] initialized", 0
    szBuildPrefix           db "[RRTM][build] ", 0
    szAgentPrefix           db "[RRTM][agent] ", 0
    szModelPrefix           db "[RRTM][model] ", 0
    szArrow                 db " -> ", 0

    szSnapHdr               db "[RRTM] live monitor (pure MASM64)", 13, 10, 0
    szSnapBuild             db "build.last=", 0
    szSnapAgent             db "agent.last=", 0
    szSnapStatus            db "agent.status=", 0
    szSnapModel             db "model.last=", 0

.code

; RCX=zstr -> RAX=length
RRTM_StrLen PROC
    xor     eax, eax
    test    rcx, rcx
    jz      @done
@loop:
    mov     dl, byte ptr [rcx+rax]
    test    dl, dl
    jz      @done
    inc     rax
    jmp     @loop
@done:
    ret
RRTM_StrLen ENDP

; RCX=dst, RDX=src, R8D=capacity bytes (incl terminator)
RRTM_CopyZLimit PROC
    xor     r9d, r9d
    test    rcx, rcx
    jz      @ret
    test    r8d, r8d
    jz      @ret
    test    rdx, rdx
    jnz     @copy
    mov     byte ptr [rcx], 0
    jmp     @ret
@copy:
    cmp     r9d, r8d
    jae     @term_last
    mov     al, byte ptr [rdx+r9]
    mov     byte ptr [rcx+r9], al
    test    al, al
    jz      @ret
    inc     r9d
    cmp     r9d, r8d
    jb      @copy
@term_last:
    mov     eax, r8d
    dec     eax
    mov     byte ptr [rcx+rax], 0
@ret:
    ret
RRTM_CopyZLimit ENDP

; RCX=zstr -> debugger + file sink
RRTM_EmitZ PROC FRAME
    sub     rsp, 72
    .allocstack 72
    .endprolog

    mov     qword ptr [rsp+40], rcx

    test    rcx, rcx
    jz      @emit_crlf

    call    OutputDebugStringA

    mov     rax, g_RRTM_LogHandle
    cmp     rax, INVALID_HANDLE_VALUE
    je      @emit_crlf

    mov     rcx, qword ptr [rsp+40]
    call    RRTM_StrLen
    test    eax, eax
    jz      @emit_crlf

    mov     dword ptr [rsp+16], 0
    mov     qword ptr [rsp+32], 0

    mov     rcx, g_RRTM_LogHandle
    mov     rdx, qword ptr [rsp+40]
    mov     r8d, eax
    lea     r9, [rsp+16]
    call    WriteFile

@emit_crlf:
    lea     rcx, szCRLF
    call    OutputDebugStringA

    mov     rax, g_RRTM_LogHandle
    cmp     rax, INVALID_HANDLE_VALUE
    je      @done

    mov     dword ptr [rsp+16], 0
    mov     qword ptr [rsp+32], 0

    mov     rcx, g_RRTM_LogHandle
    lea     rdx, szCRLF
    mov     r8d, RRTM_CRLF_LEN
    lea     r9, [rsp+16]
    call    WriteFile

@done:
    add     rsp, 72
    ret
RRTM_EmitZ ENDP

; RCX=prefix, RDX=arg1, R8=arg2(optional)
RRTM_Emit3 PROC FRAME
    sub     rsp, 72
    .allocstack 72
    .endprolog

    mov     qword ptr [rsp+40], rcx
    mov     qword ptr [rsp+48], rdx
    mov     qword ptr [rsp+56], r8

    mov     rcx, qword ptr [rsp+40]
    call    RRTM_EmitZ

    mov     rcx, qword ptr [rsp+48]
    call    RRTM_EmitZ

    mov     rcx, qword ptr [rsp+56]
    test    rcx, rcx
    jz      @done
    call    RRTM_EmitZ

@done:
    add     rsp, 72
    ret
RRTM_Emit3 ENDP

; RCX=customLogPath|null, RAX=0 success
RRTM_Init PROC FRAME
    sub     rsp, 72
    .allocstack 72
    .endprolog

    cmp     qword ptr g_RRTM_Initialized, 0
    je      @do_init
    xor     eax, eax
    jmp     @done

@do_init:
    test    rcx, rcx
    jnz     @have_path
    lea     rcx, szRTLogPath

@have_path:
    mov     rdx, GENERIC_WRITE
    mov     r8d, FILE_SHARE_READ
    xor     r9d, r9d
    mov     dword ptr [rsp+32], CREATE_ALWAYS
    mov     dword ptr [rsp+40], FILE_ATTRIBUTE_NORMAL
    mov     qword ptr [rsp+48], 0
    call    CreateFileA

    mov     g_RRTM_LogHandle, rax
    cmp     rax, INVALID_HANDLE_VALUE
    je      @mark_init

@mark_init:
    mov     qword ptr g_RRTM_Initialized, 1

    mov     qword ptr g_RRTM_BuildEvents, 0
    mov     qword ptr g_RRTM_AgentEvents, 0
    mov     qword ptr g_RRTM_ModelEvents, 0
    mov     qword ptr g_RRTM_LastTokens, 0

    lea     rcx, g_RRTM_LastBuild
    xor     rdx, rdx
    mov     r8d, RRTM_LAST_BUILD_SIZE
    call    RRTM_CopyZLimit

    lea     rcx, g_RRTM_LastAgent
    xor     rdx, rdx
    mov     r8d, RRTM_LAST_AGENT_SIZE
    call    RRTM_CopyZLimit

    lea     rcx, g_RRTM_LastStatus
    xor     rdx, rdx
    mov     r8d, RRTM_LAST_STATUS_SIZE
    call    RRTM_CopyZLimit

    lea     rcx, g_RRTM_LastModel
    xor     rdx, rdx
    mov     r8d, RRTM_LAST_MODEL_SIZE
    call    RRTM_CopyZLimit

    lea     rcx, szInit
    call    RRTM_EmitZ

    xor     eax, eax
@done:
    add     rsp, 72
    ret
RRTM_Init ENDP

; RCX=eventText, RDX=level
RRTM_RecordBuildEvent PROC FRAME
    sub     rsp, 72
    .allocstack 72
    .endprolog

    lock inc qword ptr g_RRTM_BuildEvents

    mov     qword ptr [rsp+40], rcx

    lea     rcx, g_RRTM_LastBuild
    mov     rdx, qword ptr [rsp+40]
    mov     r8d, RRTM_LAST_BUILD_SIZE
    call    RRTM_CopyZLimit

    lea     rcx, szBuildPrefix
    mov     rdx, qword ptr [rsp+40]
    xor     r8d, r8d
    call    RRTM_Emit3

    add     rsp, 72
    ret
RRTM_RecordBuildEvent ENDP

; RCX=agentId, RDX=statusText
RRTM_RecordAgentEvent PROC FRAME
    sub     rsp, 72
    .allocstack 72
    .endprolog

    lock inc qword ptr g_RRTM_AgentEvents

    mov     qword ptr [rsp+40], rcx
    mov     qword ptr [rsp+48], rdx

    lea     rcx, g_RRTM_LastAgent
    mov     rdx, qword ptr [rsp+40]
    mov     r8d, RRTM_LAST_AGENT_SIZE
    call    RRTM_CopyZLimit

    lea     rcx, g_RRTM_LastStatus
    mov     rdx, qword ptr [rsp+48]
    mov     r8d, RRTM_LAST_STATUS_SIZE
    call    RRTM_CopyZLimit

    lea     rcx, szAgentPrefix
    mov     rdx, qword ptr [rsp+40]
    lea     r8, szArrow
    call    RRTM_Emit3

    mov     rcx, qword ptr [rsp+48]
    call    RRTM_EmitZ

    add     rsp, 72
    ret
RRTM_RecordAgentEvent ENDP

; RCX=modelName, RDX=tokensPerSec
RRTM_RecordModelEvent PROC FRAME
    sub     rsp, 72
    .allocstack 72
    .endprolog

    lock inc qword ptr g_RRTM_ModelEvents
    mov     g_RRTM_LastTokens, rdx
    mov     qword ptr [rsp+40], rcx

    lea     rcx, g_RRTM_LastModel
    mov     rdx, qword ptr [rsp+40]
    mov     r8d, RRTM_LAST_MODEL_SIZE
    call    RRTM_CopyZLimit

    lea     rcx, szModelPrefix
    mov     rdx, qword ptr [rsp+40]
    xor     r8d, r8d
    call    RRTM_Emit3

    add     rsp, 72
    ret
RRTM_RecordModelEvent ENDP

; RCX=outBuf, RDX=bufLen, R8=label, R9=value -> RAX=bytes copied so far
RRTM_AppendSnapshotField PROC FRAME
    sub     rsp, 72
    .allocstack 72
    .endprolog

    mov     qword ptr [rsp+40], rcx
    mov     qword ptr [rsp+48], rdx
    mov     qword ptr [rsp+56], r8
    mov     qword ptr [rsp+64], r9

    mov     rcx, qword ptr [rsp+40]
    call    RRTM_StrLen
    mov     r10, rax

    mov     rcx, qword ptr [rsp+48]
    cmp     r10, rcx
    jae     @return_len

    mov     rcx, qword ptr [rsp+40]
    add     rcx, r10
    mov     rdx, qword ptr [rsp+56]
    mov     r8d, dword ptr [rsp+48]
    sub     r8d, r10d
    call    RRTM_CopyZLimit

    mov     rcx, qword ptr [rsp+40]
    call    RRTM_StrLen
    mov     r10, rax

    mov     rcx, qword ptr [rsp+48]
    cmp     r10, rcx
    jae     @return_len

    mov     rcx, qword ptr [rsp+40]
    add     rcx, r10
    mov     rdx, qword ptr [rsp+64]
    mov     r8d, dword ptr [rsp+48]
    sub     r8d, r10d
    call    RRTM_CopyZLimit

    mov     rcx, qword ptr [rsp+40]
    call    RRTM_StrLen
    mov     r10, rax

    mov     rcx, qword ptr [rsp+48]
    cmp     r10, rcx
    jae     @return_len

    mov     rcx, qword ptr [rsp+40]
    add     rcx, r10
    lea     rdx, szCRLF
    mov     r8d, dword ptr [rsp+48]
    sub     r8d, r10d
    call    RRTM_CopyZLimit

@return_len:
    mov     rcx, qword ptr [rsp+40]
    call    RRTM_StrLen

    add     rsp, 72
    ret
RRTM_AppendSnapshotField ENDP

; RCX=outBuf, RDX=bufLen -> RAX=bytes copied
RRTM_SnapshotStatus PROC FRAME
    sub     rsp, 72
    .allocstack 72
    .endprolog

    mov     qword ptr [rsp+40], rcx
    mov     qword ptr [rsp+48], rdx

    test    rcx, rcx
    jz      @ret_zero
    test    rdx, rdx
    jz      @ret_zero

    ; Start with static header then best-effort last fields.
    mov     rcx, qword ptr [rsp+40]
    lea     rdx, szSnapHdr
    mov     r8d, dword ptr [rsp+48]
    call    RRTM_CopyZLimit

    mov     rcx, qword ptr [rsp+40]
    mov     rdx, qword ptr [rsp+48]
    lea     r8, szSnapBuild
    lea     r9, g_RRTM_LastBuild
    call    RRTM_AppendSnapshotField

    mov     rcx, qword ptr [rsp+40]
    mov     rdx, qword ptr [rsp+48]
    lea     r8, szSnapAgent
    lea     r9, g_RRTM_LastAgent
    call    RRTM_AppendSnapshotField

    mov     rcx, qword ptr [rsp+40]
    mov     rdx, qword ptr [rsp+48]
    lea     r8, szSnapStatus
    lea     r9, g_RRTM_LastStatus
    call    RRTM_AppendSnapshotField

    mov     rcx, qword ptr [rsp+40]
    mov     rdx, qword ptr [rsp+48]
    lea     r8, szSnapModel
    lea     r9, g_RRTM_LastModel
    call    RRTM_AppendSnapshotField

    mov     rcx, qword ptr [rsp+40]
    call    RRTM_StrLen
    jmp     @done

@ret_zero:
    xor     eax, eax

@done:
    add     rsp, 72
    ret
RRTM_SnapshotStatus ENDP

RRTM_Flush PROC FRAME
    sub     rsp, 40
    .allocstack 40
    .endprolog

    mov     rcx, g_RRTM_LogHandle
    cmp     rcx, INVALID_HANDLE_VALUE
    je      @done
    call    FlushFileBuffers

@done:
    add     rsp, 40
    ret
RRTM_Flush ENDP

RRTM_Shutdown PROC FRAME
    sub     rsp, 40
    .allocstack 40
    .endprolog

    call    RRTM_Flush

    mov     rcx, g_RRTM_LogHandle
    cmp     rcx, INVALID_HANDLE_VALUE
    je      @mark_off

    call    CloseHandle

@mark_off:
    mov     qword ptr g_RRTM_LogHandle, INVALID_HANDLE_VALUE
    mov     qword ptr g_RRTM_Initialized, 0

    add     rsp, 40
    ret
RRTM_Shutdown ENDP

END
