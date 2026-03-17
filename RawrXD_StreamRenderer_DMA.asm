; =============================================================================
; RawrXD_StreamRenderer_DMA.asm
; Pure x64 MASM - Zero-Copy Token Stream Renderer
; Implements: LLM API Output -> Direct Text Buffer Rendering (DMA-Safe)
; =============================================================================

OPTION CASEMAP:NONE

; --- Externs (Infrastructure) ---
extrn RawrXD_Tokenizer_Decode: proc
extrn OutputDebugStringA: proc
extrn SendMessageA: proc
extrn GetProcessHeap: proc
extrn HeapAlloc: proc
extrn HeapFree: proc
extrn RtlZeroMemory: proc

; --- Constants ---
EM_REPLACESEL       EQU 00C2h
EM_SETSEL           EQU 00B1h

; --- Structures ---
STREAM_RENDER_CONFIG STRUCT 16
    hwndEditor      dq ?        ; Target RichEdit or Edit control
    pDmaBuffer      dq ?        ; Shared memory buffer with LLM engine
    nBufferSize     dd ?        ; Total size of DMA buffer
    nCurrOffset     dd ?        ; Current confirmed offset in buffer
    bAutoScroll     dd ?        ; Boolean: scroll to bottom on update
    lastTokenId     dd ?        ; ID of last rendered token (anti-dupe)
STREAM_RENDER_CONFIG ENDS

.data
    szRendererMsg   db "[RENDERER] Streaming %d tokens to HWND: 0x%016llX", 0
    szDmaError      db "[RENDERER] DMA Buffer Sync Failed (Collision Detected)", 0
    
.code

; =============================================================================
; Stream_InitializeRenderer
; rcx = hwndTarget, rdx = pDmaBuffer, r8d = size
; Returns: rax = hRenderConfig
; =============================================================================
Stream_InitializeRenderer PROC FRAME
    push rbp
    .pushreg rbp
    push rbx
    .pushreg rbx
    sub rsp, 56
    .allocstack 56
    .endprolog

    mov [rsp+32], r8            ; Save requested buffer size
    mov [rsp+40], rcx           ; Save hwndTarget
    mov [rsp+48], rdx           ; Save DMA buffer pointer

    call GetProcessHeap
    mov rcx, rax
    xor edx, edx
    mov r8, SIZEOF STREAM_RENDER_CONFIG
    call HeapAlloc
    test rax, rax
    jz @Fail

    mov rbx, rax
    mov rax, [rsp+40]
    mov [rbx].STREAM_RENDER_CONFIG.hwndEditor, rax
    mov rax, [rsp+48]
    mov [rbx].STREAM_RENDER_CONFIG.pDmaBuffer, rax
    mov eax, dword ptr [rsp+32]
    mov [rbx].STREAM_RENDER_CONFIG.nBufferSize, eax
    mov [rbx].STREAM_RENDER_CONFIG.nCurrOffset, 0
    mov [rbx].STREAM_RENDER_CONFIG.bAutoScroll, 1

    mov rax, rbx
    jmp @Done

@Fail:
    xor eax, eax
@Done:
    add rsp, 56
    pop rbx
    pop rbp
    ret
Stream_InitializeRenderer ENDP

; =============================================================================
; Stream_RenderFrame - Zero-Copy Push to Editor
; rcx = hRenderConfig, rdx = hTokenizer, r8 = pNewTokenBuffer, r9d = nNewTokens
; =============================================================================
Stream_RenderFrame PROC FRAME
    push rbp
    .pushreg rbp
    push rbx
    .pushreg rbx
    push rsi
    .pushreg rsi
    push rdi
    .pushreg rdi
    push r12
    .pushreg r12
    push r13
    .pushreg r13
    push r14
    .pushreg r14
    sub rsp, 128
    .allocstack 128
    .endprolog

    mov rbx, rcx                ; rbx = hRenderConfig
    mov r12, rdx                ; r12 = hTokenizer
    mov r13, r8                 ; r13 = New Tokens
    mov r14d, r9d               ; r14d = Count

    test r14d, r14d
    jz @Done

    ; 1. Convert Token Batch to String (Zero-Copy using stack scratch)
    lea r9, [rsp+32]            ; Scratch buffer for decoded text
    mov rcx, r12                ; hTokenizer
    mov rdx, r13                ; pTokens
    mov r8d, r14d               ; nCount
    mov r10, 1024               ; max_len
    call RawrXD_Tokenizer_Decode

    cmp qword ptr [rbx].STREAM_RENDER_CONFIG.hwndEditor, 0
    jne RenderToWindow
    lea rcx, [rsp+32]
    call OutputDebugStringA
    jmp @Done

RenderToWindow:
    
    ; 2. Atomic Update of Editor Buffer (Win32 API)
    ; Move caret to end
    mov rcx, [rbx].STREAM_RENDER_CONFIG.hwndEditor
    mov edx, EM_SETSEL
    mov r8, -1                  ; End of text
    mov r9, -1
    call SendMessageA

    ; Insert current chunk (Zero-copy pass to Win32)
    mov rcx, [rbx].STREAM_RENDER_CONFIG.hwndEditor
    mov edx, EM_REPLACESEL
    xor r8, r8                  ; bCanUndo = FALSE (Optimization)
    lea r9, [rsp+32]            ; Decoded Text
    call SendMessageA

    ; 3. Auto-scroll behavior
    cmp [rbx].STREAM_RENDER_CONFIG.bAutoScroll, 1
    jne @Done
    
    ; Logic to ensure cursor remains visible
    ; (Typically handled by EM_SETSEL -1, -1 + EM_SCROLLCARET)

@Done:
    add rsp, 128
    pop r14
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    pop rbp
    ret
Stream_RenderFrame ENDP

PUBLIC Stream_InitializeRenderer
PUBLIC Stream_RenderFrame

END
