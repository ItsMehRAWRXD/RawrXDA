; ai_chat_integration.asm - AI Chat Integration for MASM IDE
; Connects UI layer to model loader and provides chat functionality

option casemap:none

include windows.inc
includelib kernel32.lib
includelib user32.lib

;==========================================================================
; EXTERNAL DECLARATIONS
;==========================================================================
EXTERN ml_masm_init:PROC
EXTERN ml_masm_inference:PROC
EXTERN ml_masm_get_response:PROC
EXTERN ml_masm_free:PROC
EXTERN ui_add_chat_message:PROC
EXTERN asm_log:PROC
EXTERN AgenticEngine_ProcessResponse:PROC
EXTERN AgenticEngine_GetStats:PROC
EXTERN CurrentAgentMode:DWORD

;==========================================================================
; CONSTANTS
;==========================================================================
MAX_PROMPT_LEN      EQU 4096
MAX_RESPONSE_LEN    EQU 8192
CHAT_BUFFER_SIZE    EQU 16384

;==========================================================================
; DATA SEGMENT
;==========================================================================
.data?
    chat_buffer         BYTE CHAT_BUFFER_SIZE DUP (?)
    prompt_buffer       BYTE MAX_PROMPT_LEN DUP (?)
    response_buffer     BYTE MAX_RESPONSE_LEN DUP (?)
    model_loaded        DWORD ?
    chat_active         DWORD ?

.data
    ai_prefix           BYTE "AI: ", 0
    user_prefix         BYTE "You: ", 0
    error_prefix        BYTE "Error: ", 0
    system_prefix       BYTE "System: ", 0
    
    msg_model_loading   BYTE "Loading AI model...", 0
    msg_model_ready     BYTE "AI model loaded and ready!", 0
    msg_model_error     BYTE "Failed to load AI model", 0
    msg_thinking        BYTE "AI is thinking...", 0
    msg_no_model        BYTE "No model loaded. Please load a model first.", 0
    
    welcome_message     BYTE "Welcome to RawrXD AI IDE! Load a model to start chatting.", 0
    
    log_chat_init       BYTE "AI Chat: Initializing...", 0
    log_chat_send       BYTE "AI Chat: Sending message...", 0
    log_chat_response   BYTE "AI Chat: Processing response...", 0

;==========================================================================
; CODE SEGMENT
;==========================================================================
.code

;==========================================================================
; PUBLIC: ai_chat_init()
; Initialize AI chat system
;==========================================================================
PUBLIC ai_chat_init
ALIGN 16
ai_chat_init PROC
    sub rsp, 28h
    
    lea rcx, log_chat_init
    call asm_log
    
    ; Initialize state
    mov model_loaded, 0
    mov chat_active, 0
    
    ; Show welcome message
    lea rcx, system_prefix
    call ui_add_chat_message
    
    lea rcx, welcome_message
    call ui_add_chat_message
    
    xor eax, eax
    add rsp, 28h
    ret
ai_chat_init ENDP

;==========================================================================
; PUBLIC: ai_chat_load_model(model_path: rcx)
; Load AI model for chat
;==========================================================================
PUBLIC ai_chat_load_model
ALIGN 16
ai_chat_load_model PROC
    push rbx
    sub rsp, 30h
    
    mov rbx, rcx                    ; save model path
    
    ; Show loading message
    lea rcx, system_prefix
    call ui_add_chat_message
    
    lea rcx, msg_model_loading
    call ui_add_chat_message
    
    ; Load model
    mov rcx, rbx
    xor rdx, rdx                    ; flags = 0
    call ml_masm_init
    test eax, eax
    jz load_failed
    
    ; Success
    mov model_loaded, 1
    mov chat_active, 1
    
    lea rcx, system_prefix
    call ui_add_chat_message
    
    lea rcx, msg_model_ready
    call ui_add_chat_message
    
    mov eax, 1
    jmp done
    
load_failed:
    mov model_loaded, 0
    mov chat_active, 0
    
    lea rcx, error_prefix
    call ui_add_chat_message
    
    lea rcx, msg_model_error
    call ui_add_chat_message
    
    xor eax, eax
    
done:
    add rsp, 30h
    pop rbx
    ret
ai_chat_load_model ENDP

;==========================================================================
; PUBLIC: ai_chat_send_message(message: rcx)
; Send message to AI and get response
;==========================================================================
PUBLIC ai_chat_send_message
ALIGN 16
ai_chat_send_message PROC
    push rbx
    push rsi
    sub rsp, 38h
    
    mov rbx, rcx                    ; save message
    
    lea rcx, log_chat_send
    call asm_log
    
    ; Check if model is loaded
    cmp model_loaded, 0
    je no_model
    
    ; Add user message to chat
    lea rcx, user_prefix
    call ui_add_chat_message
    
    mov rcx, rbx
    call ui_add_chat_message
    
    ; Show thinking message
    lea rcx, ai_prefix
    call ui_add_chat_message
    
    lea rcx, msg_thinking
    call ui_add_chat_message
    
    ; Copy message to prompt buffer
    mov rsi, rbx
    lea rdi, prompt_buffer
    mov rcx, MAX_PROMPT_LEN - 1
    call strcpy_safe
    
    ; Run inference
    lea rcx, prompt_buffer
    call ml_masm_inference
    test eax, eax
    jz inference_failed
    
    ; Get response
    lea rcx, response_buffer
    mov rdx, MAX_RESPONSE_LEN
    call ml_masm_get_response
    
    ; --- BEYOND ENTERPRISE: Process via Agentic Orchestrator ---
    lea rcx, response_buffer
    mov rdx, MAX_RESPONSE_LEN
    mov r8d, CurrentAgentMode ; Use the active agent mode
    call AgenticEngine_ProcessResponse
    ; rax now contains the (potentially corrected) response pointer
    mov rbx, rax ; Save final response pointer
    
    lea rcx, log_chat_response
    call asm_log
    
    ; Add AI response to chat
    lea rcx, ai_prefix
    call ui_add_chat_message
    
    mov rcx, rbx ; Use the processed response
    call ui_add_chat_message
    
    mov eax, 1
    jmp done
    
no_model:
    lea rcx, error_prefix
    call ui_add_chat_message
    
    lea rcx, msg_no_model
    call ui_add_chat_message
    
    xor eax, eax
    jmp done
    
inference_failed:
    lea rcx, error_prefix
    call ui_add_chat_message
    
    lea rcx, msg_inference_error
    call ui_add_chat_message
    
    xor eax, eax
    
done:
    add rsp, 38h
    pop rsi
    pop rbx
    ret
ai_chat_send_message ENDP

;==========================================================================
; PUBLIC: ai_chat_clear()
; Clear chat history
;==========================================================================
PUBLIC ai_chat_clear
ALIGN 16
ai_chat_clear PROC
    sub rsp, 28h
    
    ; Clear chat display (would need UI function)
    ; For now, just add separator
    lea rcx, system_prefix
    call ui_add_chat_message
    
    lea rcx, msg_chat_cleared
    call ui_add_chat_message
    
    add rsp, 28h
    ret
ai_chat_clear ENDP

;==========================================================================
; PUBLIC: ai_chat_is_model_loaded()
; Check if model is loaded
;==========================================================================
PUBLIC ai_chat_is_model_loaded
ALIGN 16
ai_chat_is_model_loaded PROC
    mov eax, model_loaded
    ret
ai_chat_is_model_loaded ENDP

;==========================================================================
; PUBLIC: ai_chat_shutdown()
; Shutdown AI chat system
;==========================================================================
PUBLIC ai_chat_shutdown
ALIGN 16
ai_chat_shutdown PROC
    sub rsp, 28h
    
    ; Free model if loaded
    cmp model_loaded, 0
    je done_shutdown
    
    call ml_masm_free
    mov model_loaded, 0
    mov chat_active, 0
    
done_shutdown:
    add rsp, 28h
    ret
ai_chat_shutdown ENDP

;==========================================================================
; Helper: strcpy_safe - Safe string copy with length limit
; RCX = max length, RSI = source, RDI = destination
;==========================================================================
strcpy_safe PROC
    push rax
    push rbx
    
    xor rbx, rbx                    ; counter
    
copy_loop:
    cmp rbx, rcx
    jge done_strcpy
    
    mov al, BYTE PTR [rsi + rbx]
    mov BYTE PTR [rdi + rbx], al
    
    test al, al
    jz done_strcpy
    
    inc rbx
    jmp copy_loop
    
done_strcpy:
    ; Ensure null termination
    cmp rbx, rcx
    jl null_term
    dec rbx
    
null_term:
    mov BYTE PTR [rdi + rbx], 0
    
    pop rbx
    pop rax
    ret
strcpy_safe ENDP

.data
    msg_inference_error BYTE "AI inference failed", 0
    msg_chat_cleared    BYTE "Chat history cleared", 0

END