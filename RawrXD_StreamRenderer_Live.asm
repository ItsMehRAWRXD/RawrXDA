; =============================================================================
; RawrXD_StreamRenderer_Live.asm
; Stable token stream renderer for console+GUI editor surface
; =============================================================================

OPTION CASEMAP:NONE

EXTERN RawrXD_Tokenizer_Decode:PROC
EXTERN OutputDebugStringA:PROC
EXTERN SendMessageA:PROC
EXTERN GetProcessHeap:PROC
EXTERN HeapAlloc:PROC

HEAP_ZERO_MEMORY EQU 00000008h
EM_SETSEL        EQU 00B1h
EM_REPLACESEL    EQU 00C2h

STREAM_RENDER_CONFIG STRUCT 16
    hwndEditor      dq ?
    pDmaBuffer      dq ?
    nBufferSize     dd ?
    pad0            dd ?
STREAM_RENDER_CONFIG ENDS

.code

Stream_InitializeRenderer PROC FRAME
    push rbp
    .pushreg rbp
    push rbx
    .pushreg rbx
    sub rsp, 72
    .allocstack 72
    .endprolog

    mov [rsp+32], rcx            ; hwnd target
    mov [rsp+40], rdx            ; dma buffer
    mov dword ptr [rsp+48], r8d  ; dma size

    call GetProcessHeap
    mov rcx, rax
    mov edx, HEAP_ZERO_MEMORY
    mov r8d, SIZEOF STREAM_RENDER_CONFIG
    call HeapAlloc
    test rax, rax
    jz InitFail

    mov rbx, rax
    mov rax, [rsp+32]
    mov [rbx].STREAM_RENDER_CONFIG.hwndEditor, rax
    mov rax, [rsp+40]
    mov [rbx].STREAM_RENDER_CONFIG.pDmaBuffer, rax
    mov eax, dword ptr [rsp+48]
    mov [rbx].STREAM_RENDER_CONFIG.nBufferSize, eax

    mov rax, rbx
    jmp InitDone

InitFail:
    xor eax, eax

InitDone:
    add rsp, 72
    pop rbx
    pop rbp
    ret
Stream_InitializeRenderer ENDP

Stream_RenderFrame PROC FRAME
    push rbp
    .pushreg rbp
    push rbx
    .pushreg rbx
    sub rsp, 1096
    .allocstack 1096
    .endprolog

    mov rbx, rcx                 ; config
    test rbx, rbx
    jz RenderFail

    test r8, r8                  ; input token/text buffer
    jz RenderFail

    test r9, r9
    jnz HaveCount
    mov r9, 1024
HaveCount:
    cmp r9, 1024
    jbe CountReady
    mov r9, 1024
CountReady:

    mov rcx, rdx                 ; tokenizer handle
    mov rdx, r8                  ; input buffer
    mov r8, r9                   ; count hint
    lea r9, [rsp+32]             ; decoded text output
    call RawrXD_Tokenizer_Decode
    test rax, rax
    jz RenderFail

    cmp qword ptr [rbx].STREAM_RENDER_CONFIG.hwndEditor, 0
    je RenderDebug

    mov rcx, [rbx].STREAM_RENDER_CONFIG.hwndEditor
    mov edx, EM_SETSEL
    mov r8, -1
    mov r9, -1
    call SendMessageA

    mov rcx, [rbx].STREAM_RENDER_CONFIG.hwndEditor
    mov edx, EM_REPLACESEL
    xor r8d, r8d
    lea r9, [rsp+32]
    call SendMessageA

    jmp RenderDone

RenderDebug:
    lea rcx, [rsp+32]
    call OutputDebugStringA
    jmp RenderDone

RenderFail:
    xor eax, eax

RenderDone:
    add rsp, 1096
    pop rbx
    pop rbp
    ret
Stream_RenderFrame ENDP

PUBLIC Stream_InitializeRenderer
PUBLIC Stream_RenderFrame

END
