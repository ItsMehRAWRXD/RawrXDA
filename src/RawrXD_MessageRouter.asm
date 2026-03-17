; RawrXD_MessageRouter.asm - Intent Parsing & Dispatch
; Parses direct chat, slash commands, or agentic tasks

include masm64_compat.inc
include RawrXD_Common.inc

.data
    szSlash       db "/", 0
    szAtAgent     db "@agent", 0
    szInference   db "INFERENCE_CHAT", 0

.code
RouteToAgenticEngine PROC FRAME
    .pushreg rbp
    mov rbp, rsp
    sub rsp, 32
    .endprolog

    mov rsi, rcx ; lpMessage

    ; Check for empty
    mov al, [rsi]
    test al, al
    jz _Done

    ; 1. Check for /slash commands
    .IF al == '/'
        invoke ParseSlashCommand, rsi
        jmp _Done
    .ENDIF

    ; 2. Check for @agent mention
    invoke StrStrA, rsi, ADDR szAtAgent
    .IF rax != 0
        invoke DelegateToAgent, rsi
        jmp _Done
    .ENDIF

    ; 3. Default: Queue Inference for Chat
    invoke QueueInferenceRequest, rsi, ADDR szInference

_Done:
    add rsp, 32
    pop rbp
    ret
RouteToAgenticEngine ENDP

END
