; RawrXD_InferenceClient.asm - MASM x64 glue for inference_client.dll
; Bridges MessageRouter -> WinSock -> llama-server -> StreamRenderer
;
; Exports:
;   InitInferenceClient  - Call once at startup
;   ShutdownInference    - Call at exit  
;   QueueInferenceRequest - Called by MessageRouter, streams to RichEdit
;
; Uses: inference_client.dll (Infer_Init, Infer_Stream, Infer_DefaultConfig)

include masm64_compat.inc
include RawrXD_Common.inc

; ── Imports from inference_client.dll ─────────────────────────────
EXTERNDEF Infer_Init:PROC
EXTERNDEF Infer_Stream:PROC
EXTERNDEF Infer_DefaultConfig:PROC
EXTERNDEF Infer_Shutdown:PROC

; ── Imports from StreamRenderer ───────────────────────────────────
EXTERNDEF AppendTokenToChat:PROC

; ── Constants ─────────────────────────────────────────────────────
INFER_OK             EQU 0
INFER_ERR_WSASTARTUP EQU 1
INFER_ERR_CONNECT    EQU 2

; InferConfig structure offsets (matches inference_client.h, pack 8)
INFER_CFG_HOST       EQU 0    ; const char* (8 bytes)
INFER_CFG_PORT       EQU 8    ; uint16_t
INFER_CFG_PAD        EQU 10   ; uint16_t
INFER_CFG_MAXTOKENS  EQU 12   ; int32_t
INFER_CFG_TEMPERATURE EQU 16  ; float
INFER_CFG_TIMEOUT    EQU 20   ; int32_t
INFER_CFG_SIZE       EQU 24

; InferResult structure offsets
INFER_RES_STATUS     EQU 0    ; int32_t
INFER_RES_PROMPT     EQU 4    ; int32_t
INFER_RES_COMPLETION EQU 8    ; int32_t
INFER_RES_PAD        EQU 12   ; int32_t
INFER_RES_ELAPSED    EQU 16   ; int64_t
INFER_RES_TEXT       EQU 24   ; char[4096]
INFER_RES_ERROR      EQU 4120 ; char[256]
INFER_RES_SIZE       EQU 4376

.data
    g_infer_ready   dd 0
    g_infer_config  db INFER_CFG_SIZE dup(0)
    
    szInferHost     db "127.0.0.1", 0
    szInferPrefix   db 13, 10, "[Model]: ", 0
    szInferSuffix   db 13, 10, 0
    szInferError    db 13, 10, "[Error]: Connection failed - is llama-server running on port 8081?", 13, 10, 0
    szInferInit     db "[System]: Inference client initialized (127.0.0.1:8081)", 13, 10, 0
    szChatLabel     db "INFERENCE_CHAT", 0

.data?
    g_infer_result  db INFER_RES_SIZE dup(?)

.code

; ── Token Callback (called by inference_client.dll per token) ─────
; __stdcall: token* in RCX, token_len in RDX, user_data in R8
; Returns 0 to continue, non-zero to abort
TokenStreamCallback PROC FRAME
    .pushreg rbp
    mov rbp, rsp
    sub rsp, 48
    .endprolog

    ; Save token pointer - RCX already has it
    ; AppendTokenToChat expects lpToken in RCX
    ; The token is already null-terminated by inference_client.c
    
    mov [rsp+32], rcx        ; save token ptr
    mov [rsp+40], rdx        ; save token_len
    
    ; Call AppendTokenToChat(lpToken)
    ; RCX = token pointer (already set)
    invoke AppendTokenToChat

    ; Return 0 = continue streaming
    xor eax, eax

    add rsp, 48
    pop rbp
    ret
TokenStreamCallback ENDP

; ── Initialize inference client ───────────────────────────────────
; Call once at WM_CREATE or application startup
; Returns: EAX = 0 on success
InitInferenceClient PROC FRAME
    .pushreg rbp
    mov rbp, rsp
    sub rsp, 32
    .endprolog

    ; Init WinSock via inference_client.dll
    invoke Infer_Init
    test eax, eax
    jnz @F

    ; Setup default config
    lea rcx, g_infer_config
    invoke Infer_DefaultConfig, rcx

    ; Override: set max_tokens to 512 for chat
    lea rax, g_infer_config
    mov DWORD PTR [rax + INFER_CFG_MAXTOKENS], 512

    ; Mark as ready
    mov DWORD PTR [g_infer_ready], 1

    ; Show init message in chat
    lea rcx, szInferInit
    invoke AppendTokenToChat

    xor eax, eax    ; success
@@:
    add rsp, 32
    pop rbp
    ret
InitInferenceClient ENDP

; ── Shutdown ──────────────────────────────────────────────────────
ShutdownInference PROC FRAME
    .pushreg rbp
    mov rbp, rsp
    sub rsp, 32
    .endprolog

    invoke Infer_Shutdown
    mov DWORD PTR [g_infer_ready], 0

    add rsp, 32
    pop rbp
    ret
ShutdownInference ENDP

; ── Queue Inference Request ───────────────────────────────────────
; Called by MessageRouter when a chat message needs inference
; RCX = lpPrompt (pointer to user's UTF-8 message)
; RDX = lpType   (pointer to type string, e.g. "INFERENCE_CHAT")
;
; This function:
; 1. Prints "[Model]: " prefix
; 2. Calls Infer_Stream with TokenStreamCallback
; 3. Each token gets AppendTokenToChat'd in real-time
; 4. Prints newline suffix
QueueInferenceRequest PROC FRAME
    .pushreg rbp
    mov rbp, rsp
    sub rsp, 64
    .endprolog

    mov [rsp+48], rcx        ; save lpPrompt
    mov [rsp+56], rdx        ; save lpType

    ; Check if initialized
    cmp DWORD PTR [g_infer_ready], 0
    je _NotReady

    ; Print prefix "[Model]: "
    lea rcx, szInferPrefix
    invoke AppendTokenToChat

    ; Call Infer_Stream(prompt, config, callback, user_data, result)
    ; RCX = prompt
    ; RDX = &config
    ; R8  = callback function pointer
    ; R9  = user_data (NULL)
    ; [rsp+32] = &result

    mov rcx, [rsp+48]              ; prompt
    lea rdx, g_infer_config        ; config
    lea r8, TokenStreamCallback    ; callback
    xor r9, r9                     ; user_data = NULL
    lea rax, g_infer_result
    mov [rsp+32], rax              ; result pointer on stack
    invoke Infer_Stream

    ; Check result
    test eax, eax
    jnz _Error

    ; Print suffix (newline)
    lea rcx, szInferSuffix
    invoke AppendTokenToChat

    xor eax, eax
    jmp _Done

_NotReady:
    ; Try to init on first use
    invoke InitInferenceClient
    test eax, eax
    jnz _Error

    ; Retry the request
    mov rcx, [rsp+48]
    mov rdx, [rsp+56]
    invoke QueueInferenceRequest
    jmp _Done

_Error:
    lea rcx, szInferError
    invoke AppendTokenToChat
    mov eax, 1

_Done:
    add rsp, 64
    pop rbp
    ret
QueueInferenceRequest ENDP

END
