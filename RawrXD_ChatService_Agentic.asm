; =============================================================================
; RawrXD_ChatService_Agentic.asm
; Stable chat bridge: Prompt -> Local Inference Runtime -> Stream Buffer
; =============================================================================

OPTION CASEMAP:NONE

EXTERN RawrXD_Inference_Init:PROC
EXTERN RawrXD_Inference_Generate:PROC
EXTERN RawrXD_Tokenizer_Init:PROC
EXTERN OutputDebugStringA:PROC
EXTERN GetProcessHeap:PROC
EXTERN HeapAlloc:PROC

CHAT_CONTEXT STRUCT 16
    hInference      dq ?
    hTokenizer      dq ?
CHAT_CONTEXT ENDS

.data
    szChatInit      db "[CHAT] Initializing Agentic Pipeline...",0
    szChatProcess   db "[CHAT] Dispatching prompt to local inference runtime",0

.code

Chat_Init PROC FRAME
    push rbp
    .pushreg rbp
    push rbx
    .pushreg rbx
    sub rsp, 56
    .allocstack 56
    .endprolog

    mov [rsp+32], rcx           ; model path
    mov [rsp+40], rdx           ; vocab path

    lea rcx, szChatInit
    call OutputDebugStringA

    call GetProcessHeap
    mov rcx, rax
    xor edx, edx
    mov r8d, SIZEOF CHAT_CONTEXT
    call HeapAlloc
    test rax, rax
    jz ChatInitFail

    mov rbx, rax

    mov rcx, [rsp+40]
    call RawrXD_Tokenizer_Init
    mov [rbx].CHAT_CONTEXT.hTokenizer, rax

    mov rcx, [rsp+32]
    mov rdx, [rbx].CHAT_CONTEXT.hTokenizer
    call RawrXD_Inference_Init
    mov [rbx].CHAT_CONTEXT.hInference, rax

    mov rax, rbx
    jmp ChatInitDone

ChatInitFail:
    xor eax, eax

ChatInitDone:
    add rsp, 56
    pop rbx
    pop rbp
    ret
Chat_Init ENDP

Chat_ProcessInput PROC FRAME
    push rbp
    .pushreg rbp
    push rbx
    .pushreg rbx
    sub rsp, 56
    .allocstack 56
    .endprolog

    mov rbx, rcx                ; context
    mov [rsp+32], rdx           ; prompt
    mov [rsp+40], r8            ; output buffer

    test rbx, rbx
    jz ChatProcessFail
    test rdx, rdx               ; prompt
    jz ChatProcessFail
    test r8, r8                 ; output buffer
    jz ChatProcessFail

    lea rcx, szChatProcess
    call OutputDebugStringA

    mov rcx, [rbx].CHAT_CONTEXT.hInference
    mov rdx, [rsp+32]
    mov r8, 2048
    mov r9, [rsp+40]
    call RawrXD_Inference_Generate
    jmp ChatProcessDone

ChatProcessFail:
    xor eax, eax

ChatProcessDone:
    add rsp, 56
    pop rbx
    pop rbp
    ret
Chat_ProcessInput ENDP

PUBLIC Chat_Init
PUBLIC Chat_ProcessInput

END
