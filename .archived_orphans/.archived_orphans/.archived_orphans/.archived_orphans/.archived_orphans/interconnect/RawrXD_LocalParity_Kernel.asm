; RawrXD_LocalParity_Kernel.asm - Zero-API Local Inference Parity (x64 MASM)
; Init, Shutdown, NextToken (callback), ManifestGet stub, EncodeChunk stub
; See: docs/LOCAL_PARITY_NO_API_KEY_SPEC.md

OPTION DOTNAME
OPTION CASEMAP:NONE
OPTION WIN64:3

include \masm64\include64\windows.inc
include \masm64\include64\kernel32.inc

includelib \masm64\lib64\kernel32.lib

LOCAL_PARITY_OK   EQU 1
LOCAL_PARITY_FAIL EQU 0

.DATA
align 8
g_pInferenceCallback QWORD 0
g_bInitialized       DWORD 0

.CODE

LocalParity_Init PROC FRAME
    push rbx
    .pushreg rbx
    sub rsp, 28h
    .allocstack 28h
    .endprolog
    cmp g_bInitialized, 1
    je @ok
    mov g_bInitialized, 1
@ok:
    mov eax, LOCAL_PARITY_OK
    add rsp, 28h
    pop rbx
    ret
LocalParity_Init ENDP

LocalParity_Shutdown PROC FRAME
    sub rsp, 28h
    .allocstack 28h
    .endprolog
    mov g_pInferenceCallback, 0
    mov g_bInitialized, 0
    add rsp, 28h
    ret
LocalParity_Shutdown ENDP

LocalParity_RegisterInferenceCallback PROC
    mov g_pInferenceCallback, rcx
    ret
LocalParity_RegisterInferenceCallback ENDP

LocalParity_NextToken PROC FRAME
    push rbx
    .pushreg rbx
    sub rsp, 28h
    .allocstack 28h
    .endprolog
    mov rax, g_pInferenceCallback
    test rax, rax
    jz @fail
    call rax
    mov eax, LOCAL_PARITY_OK
    jmp @done
@fail:
    xor eax, eax
@done:
    add rsp, 28h
    pop rbx
    ret
LocalParity_NextToken ENDP

LocalParity_ManifestGet PROC FRAME
    sub rsp, 28h
    .allocstack 28h
    .endprolog
    ; Stub: return 0. Bridge implements WinHTTP fetch.
    xor eax, eax
    test r9, r9
    jz @mget_done
    mov qword ptr [r9], 0
@mget_done:
    add rsp, 28h
    ret
LocalParity_ManifestGet ENDP

LocalParity_EncodeChunk PROC
    xor eax, eax
    ret
LocalParity_EncodeChunk ENDP

END
