;==========================================================================
; agent_chat_hotpatch_bridge.asm - Real-Time Hallucination Correction Bridge
; ==========================================================================
; Bridges enhanced MASM agent chat with C++ AgentHotPatcher for:
; - Real-time hallucination detection and correction
; - Agentic feedback loops (user → agent → hotpatch → user)
; - Live model behavior adjustment
; - Token-level streaming corrections
; - Confidence-scored response validation with hotpatch application
;==========================================================================

option casemap:none

include windows.inc
includelib kernel32.lib
includelib user32.lib

;==========================================================================
; EXTERNAL DECLARATIONS (Pure MASM Agentic System)
;==========================================================================
EXTERN masm_detect_failure:PROC
EXTERN masm_puppeteer_correct_response:PROC
EXTERN hpatch_apply_memory:PROC
EXTERN hpatch_apply_byte:PROC
EXTERN hpatch_apply_server:PROC

;==========================================================================
; CONSTANTS
;==========================================================================

; Stream event types
STREAM_START            EQU 0
STREAM_TOKEN            EQU 1
STREAM_CHUNK            EQU 2
STREAM_END              EQU 3
STREAM_ERROR            EQU 4

; Correction strategies
CORRECT_AUTO            EQU 0   ; Auto-apply correction
CORRECT_SUGGEST         EQU 1   ; Suggest to user
CORRECT_BLOCK           EQU 2   ; Block problematic output
CORRECT_HOTPATCH        EQU 3   ; Apply hotpatch to model

; Token validation states
TOKEN_CLEAN             EQU 0
TOKEN_SUSPECT           EQU 1
TOKEN_INVALID           EQU 2
TOKEN_CORRECTED         EQU 3

;==========================================================================
; STRUCTURES
;==========================================================================

; Stream event for token-level processing
STREAM_EVENT STRUCT
    event_type          DWORD ?         ; STREAM_* constant
    event_id            DWORD ?         ; Unique event ID
    timestamp           QWORD ?         ; Event timestamp (QPC)
    
    ; Token data
    token_text          BYTE 256 DUP (?)
    token_confidence    DWORD ?         ; 0-255
    token_state         DWORD ?         ; TOKEN_* state
    
    ; Context
    position_in_stream  DWORD ?         ; Token position
    total_tokens_so_far DWORD ?         ; Running count
    
    ; Correction info (if applicable)
    correction_applied  DWORD ?         ; 0/1
    correction_type     DWORD ?         ; CORRECT_* strategy
    original_token      BYTE 256 DUP (?) ; What was originally sent
    replacement_token   BYTE 256 DUP (?) ; What we're using instead
STREAM_EVENT ENDS

; Hotpatch request with context
HOTPATCH_REQUEST STRUCT
    request_id          DWORD ?
    trigger_event       DWORD ?         ; Which event caused this?
    target_aspect       BYTE 128 DUP (?) ; What to patch (e.g., "token_generation")
    patch_type          DWORD ?         ; Memory/Byte/Server layer
    severity            DWORD ?         ; 0-255 (how bad is the issue?)
    
    ; Payload
    patch_data          QWORD ?         ; Pointer to patch data
    patch_size          DWORD ?
    
    ; Metadata
    success_count       DWORD ?         ; How many times has this patch worked?
    failure_count       DWORD ?         ; How many times has this failed?
    last_applied        QWORD ?         ; Last QPC timestamp applied
HOTPATCH_REQUEST ENDS

; Real-time correction context
CORRECTION_CONTEXT STRUCT
    user_input          BYTE 512 DUP (?) ; Original user message
    model_response      BYTE 4096 DUP (?) ; Raw model response
    
    ; Detection results
    hallucination_detected DWORD ?      ; 0/1
    halluc_confidence   DWORD ?         ; 0-255
    halluc_type         BYTE 128 DUP (?) ; What kind of hallucination?
    
    ; Correction applied
    correction_applied  DWORD ?         ; 0/1
    correction_method   DWORD ?         ; CORRECT_* strategy
    
    ; Hotpatch state
    hotpatch_active     DWORD ?         ; Is correction via hotpatch?
    hotpatch_request    HOTPATCH_REQUEST <>
    
    ; Feedback
    user_approved       DWORD ?         ; Did user approve correction?
    effectiveness_score DWORD ?         ; 0-255 (how good was the fix?)
CORRECTION_CONTEXT ENDS

;==========================================================================
; DATA
;==========================================================================
.data
    ; Callback event identifiers
    szHallucinationDetectedEvent BYTE "hallucination_detected",0
    szCorrectionAppliedEvent    BYTE "correction_applied",0
    szHotpatchRequestEvent      BYTE "hotpatch_request",0
    
    ; Token validation messages
    szTokenValidating       BYTE "Validating token: %s",0
    szTokenInvalid          BYTE "[INVALID TOKEN] %s -> Correcting to: %s",0
    szTokenCorrected        BYTE "[CORRECTED] %s",0
    
    ; Hallucination correction messages
    szHallucinationFound    BYTE "[HALLUCINATION FOUND] Type: %s | Confidence: %d%% | Applying correction: %s",0
    szAutoCorrectApplied    BYTE "[AUTO-CORRECTION APPLIED] Modified response to remove hallucination",0
    szHotpatchTriggered     BYTE "[HOTPATCH TRIGGERED] Applying live model patch for: %s",0
    
    ; Strategy descriptions
    szCorrectionAutomatic   BYTE "Automatic correction",0
    szCorrectionSuggest     BYTE "User-suggested correction",0
    szCorrectionBlock       BYTE "Block problematic output",0
    szCorrectionHotpatch    BYTE "Apply live hotpatch",0

.data?
    ; Global stream state
    CurrentStreamEventID    DWORD ?
    StreamTokenCount        DWORD ?
    StreamStartQPC          QWORD ?
    
    ; Correction context (per-message)
    ActiveCorrectionContext CORRECTION_CONTEXT <>
    
    ; Event buffer for streaming
    StreamEventBuffer       STREAM_EVENT 256 DUP (<>)
    StreamEventCount        DWORD ?
    StreamEventReadPos      DWORD ?
    StreamEventWritePos     DWORD ?
    
    ; Hotpatch request queue
    HotpatchRequestQueue    HOTPATCH_REQUEST 32 DUP (<>)
    HotpatchQueueSize       DWORD ?
    
    ; Configuration
    HallucinationThreshold  DWORD ?
    AutoCorrectionEnabled   DWORD ?
    HotpatchCorrectionEnabled DWORD ?

;==========================================================================
; CODE
;==========================================================================
.code

;==========================================================================
; PUBLIC: bridge_init() -> bool (rax)
; Initialize the hotpatch bridge
;==========================================================================
PUBLIC bridge_init
bridge_init PROC
    push rbx
    sub rsp, 32
    
    ; Initialize global state
    mov CurrentStreamEventID, 0
    mov StreamTokenCount, 0
    mov StreamEventCount, 0
    mov StreamEventReadPos, 0
    mov StreamEventWritePos, 0
    mov HotpatchQueueSize, 0
    
    ; Set default thresholds
    mov HallucinationThreshold, 160 ; 63%
    mov AutoCorrectionEnabled, 1
    mov HotpatchCorrectionEnabled, 1
    
    mov rax, 1
    add rsp, 32
    pop rbx
    ret
bridge_init ENDP

;==========================================================================
; PUBLIC: bridge_intercept_token(token: rcx, confidence: edx) -> rax
; Intercept and validate a token during streaming
;==========================================================================
PUBLIC bridge_intercept_token
bridge_intercept_token PROC
    jmp agent_stream_token
bridge_intercept_token ENDP

;==========================================================================
; PUBLIC: bridge_apply_correction(response: rcx, len: rdx) -> rax
; Apply correction to a complete response
;==========================================================================
PUBLIC bridge_apply_correction
bridge_apply_correction PROC
    jmp agent_stream_complete
bridge_apply_correction ENDP

;==========================================================================
; PUBLIC: bridge_get_stats() -> rax
; Get bridge statistics
;==========================================================================
PUBLIC bridge_get_stats
bridge_get_stats PROC
    ; Return pointer to stats or just a dummy for now
    xor rax, rax
    ret
bridge_get_stats ENDP

;==========================================================================
; PUBLIC: agent_stream_token(token: rcx, confidence: edx) -> rax
; Process single token from streaming response with real-time validation
; Returns: rax = corrected token length (0 if blocked)
;==========================================================================
PUBLIC agent_stream_token
agent_stream_token PROC
    push rbx
    push rsi
    push rdi
    push r12
    sub rsp, 96
    
    mov rsi, rcx        ; token
    mov r10d, edx       ; confidence
    xor rdi, rdi        ; return length
    
    ; [STEP 1] Create stream event
    lea rcx, StreamEventBuffer
    mov eax, StreamEventWritePos
    imul eax, SIZEOF STREAM_EVENT
    add rcx, rax
    
    mov [rcx].STREAM_EVENT.event_type, STREAM_TOKEN
    mov eax, CurrentStreamEventID
    inc eax
    mov CurrentStreamEventID, eax
    mov [rcx].STREAM_EVENT.event_id, eax
    
    ; Copy token text
    lea rax, [rcx].STREAM_EVENT.token_text
    mov rdx, rsi
    mov r8d, 256
    call agent_strcpy_safe
    
    mov [rcx].STREAM_EVENT.token_confidence, r10d
    mov eax, StreamTokenCount
    inc eax
    mov StreamTokenCount, eax
    mov [rcx].STREAM_EVENT.total_tokens_so_far, eax
    
    ; [STEP 2] Validate token in context
    mov rax, rcx        ; stream event ptr
    lea rcx, ActiveCorrectionContext
    lea rdx, rsi        ; token
    mov r8d, r10d       ; confidence
    call agent_validate_token_in_context
    
    ; eax = TOKEN_* state
    mov [rcx].STREAM_EVENT.token_state, eax
    
    cmp eax, TOKEN_INVALID
    je token_invalid_detected
    cmp eax, TOKEN_SUSPECT
    je token_suspect_check
    
    ; Token is clean
    lea rcx, rsi
    call agent_strlen
    mov rdi, rax
    jmp token_processing_done
    
token_suspect_check:
    ; Moderate confidence - may be problematic
    cmp r10d, 120       ; If confidence < 47%, take action
    jge token_probably_ok
    
    ; Low confidence suspect token
    lea rcx, StreamEventBuffer
    mov eax, StreamEventWritePos
    imul eax, SIZEOF STREAM_EVENT
    add rcx, rax
    
    mov [rcx].STREAM_EVENT.token_state, TOKEN_CORRECTED
    
    ; Try to get correction from hotpatcher
    mov rax, rcx
    lea rcx, ActiveCorrectionContext
    lea rdx, [rax].STREAM_EVENT.token_text
    mov r8, rax
    call agent_request_token_correction
    
    cmp eax, 0
    je token_probably_ok
    
    ; Correction available
    mov rdi, rax
    jmp token_processing_done
    
token_invalid_detected:
    ; Invalid token - must correct or block
    lea rcx, StreamEventBuffer
    mov eax, StreamEventWritePos
    imul eax, SIZEOF STREAM_EVENT
    add rcx, rax
    
    mov [rcx].STREAM_EVENT.token_state, TOKEN_INVALID
    mov [rcx].STREAM_EVENT.correction_applied, 1
    
    ; Copy original
    lea rax, [rcx].STREAM_EVENT.original_token
    mov rdx, rsi
    mov r8d, 256
    call agent_strcpy_safe
    
    ; Get replacement from hotpatcher
    mov rax, rcx
    lea rcx, ActiveCorrectionContext
    lea rdx, [rax].STREAM_EVENT.token_text
    mov r8, rax
    mov r9d, 1          ; force_replacement=true
    call agent_request_token_correction
    
    ; If no replacement available, block token
    cmp eax, 0
    jne token_corrected
    
    ; Block token
    xor rdi, rdi        ; Return 0 length (blocked)
    jmp token_processing_done
    
token_corrected:
    ; Copy replacement
    lea rbx, StreamEventBuffer
    mov edx, StreamEventWritePos
    imul edx, SIZEOF STREAM_EVENT
    add rbx, rdx
    lea rbx, [rbx].STREAM_EVENT.replacement_token
    mov rdx, rsi        ; corrected text
    mov r8d, 256
    call agent_strcpy_safe
    
    mov rdi, rax
    
token_probably_ok:
    ; Token is acceptable
    lea rcx, rsi
    call agent_strlen
    mov rdi, rax
    
token_processing_done:
    ; [STEP 3] Update stream event ring buffer position
    mov eax, StreamEventWritePos
    inc eax
    cmp eax, 256
    jne event_pos_valid
    
    xor eax, eax        ; Wrap around
    
event_pos_valid:
    mov StreamEventWritePos, eax
    mov eax, edi        ; Return corrected token length
    
    add rsp, 96
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret
agent_stream_token ENDP

;==========================================================================
; PUBLIC: agent_stream_complete(model_response: rcx) -> rax
; Process complete model response through hotpatcher for overall validation
; Returns: rax = final corrected response length
;==========================================================================
PUBLIC agent_stream_complete
agent_stream_complete PROC
    push rbx
    push rsi
    push rdi
    sub rsp, 128 ; Increased stack for local structures
    
    mov rsi, rcx        ; model response
    
    ; [STEP 1] Pass to Pure MASM Failure Detector
    lea rbx, ActiveCorrectionContext
    mov rcx, rsi        ; response_ptr
    call agent_strlen
    mov rdx, rax        ; response_len
    lea r8, [rsp + 32]  ; Local FailureDetectionResult
    call masm_detect_failure
    
    ; [STEP 2] Check if failure was detected
    test rax, rax
    jz response_clean
    
    mov [rbx].CORRECTION_CONTEXT.hallucination_detected, 1
    
    ; [STEP 3] Attempt Correction via Pure MASM Puppeteer
    lea rcx, [rsp + 32] ; failure_result_ptr
    mov rdx, 2          ; mode = MODE_ASK (default)
    lea r8, [rsp + 288] ; Local CorrectionResult (32 + 256)
    call masm_puppeteer_correct_response
    
    test rax, rax
    jz block_response ; Correction failed
    
    ; Correction successful
    mov [rbx].CORRECTION_CONTEXT.correction_applied, 1
    mov rax, [rsp + 288 + 8] ; corrected_response_ptr
    mov rdi, rax
    
    jmp correction_applied
    
response_clean:
    mov rdi, rsi        ; Use original response
    jmp response_complete
    
block_response:
    xor edi, edi
    jmp response_complete
    
correction_applied:
    ; Log correction
    lea rcx, [rbx].CORRECTION_CONTEXT
    mov rdx, rdi
    call agent_log_correction_applied
    
response_complete:
    mov rax, rdi        ; Return final response pointer
    add rsp, 128
    pop rdi
    pop rsi
    pop rbx
    ret
agent_stream_complete ENDP

;==========================================================================
; PUBLIC: agent_create_correction_context(user_input: rcx) -> rax
; Create new correction context for incoming user message
; Returns: rax = context pointer
;==========================================================================
PUBLIC agent_create_correction_context
agent_create_correction_context PROC
    push rbx
    sub rsp, 48
    
    mov rbx, rcx        ; user input
    
    ; Clear context
    lea rcx, ActiveCorrectionContext
    xor edx, edx
    mov r8d, SIZEOF CORRECTION_CONTEXT
    call agent_memset_safe
    
    ; Copy user input
    lea rcx, ActiveCorrectionContext
    lea rax, [rcx].CORRECTION_CONTEXT.user_input
    mov rdx, rbx
    mov r8d, 512
    call agent_strcpy_safe
    
    ; Initialize defaults
    mov [rcx].CORRECTION_CONTEXT.hallucination_detected, 0
    mov [rcx].CORRECTION_CONTEXT.correction_applied, 0
    mov [rcx].CORRECTION_CONTEXT.hotpatch_active, 0
    mov [rcx].CORRECTION_CONTEXT.user_approved, 0
    mov [rcx].CORRECTION_CONTEXT.effectiveness_score, 0
    
    lea rax, ActiveCorrectionContext
    add rsp, 48
    pop rbx
    ret
agent_create_correction_context ENDP

;==========================================================================
; PRIVATE: agent_validate_token_in_context(event: rcx, context: rcx, token: rdx, conf: r8d) -> eax
; Validate single token within correction context
; Returns: eax = TOKEN_* state
;==========================================================================
PRIVATE_agent_validate_token_in_context:
agent_validate_token_in_context PROC
    push rbx
    sub rsp, 48
    
    ; rcx = stream event
    ; rcx (overwrites) = context
    ; rdx = token
    ; r8d = confidence
    
    mov rbx, rcx        ; stream event
    mov rsi, rcx        ; context (overwrites rcx)
    mov rdi, rdx        ; token
    
    ; [CHECK 1] Is token in symbol table?
    mov rcx, rdi
    lea rdx, SymbolTablePtr
    call agent_check_token_in_symbols
    
    cmp eax, 0
    je token_not_found
    
    ; Token is a known symbol
    mov eax, TOKEN_CLEAN
    jmp token_validation_done
    
token_not_found:
    ; [CHECK 2] Is token a valid language construct?
    mov rcx, rdi
    call agent_check_valid_token_syntax
    
    cmp eax, 0
    je token_invalid_syntax
    
    ; Valid syntax but unknown symbol
    cmp r8d, 140        ; If confidence < 55%
    jge token_suspect
    
    mov eax, TOKEN_CLEAN
    jmp token_validation_done
    
token_suspect:
    mov eax, TOKEN_SUSPECT
    jmp token_validation_done
    
token_invalid_syntax:
    mov eax, TOKEN_INVALID
    
token_validation_done:
    add rsp, 48
    pop rbx
    ret
agent_validate_token_in_context ENDP

;==========================================================================
; PRIVATE: agent_request_token_correction(context: rcx, token: rdx, event: r8, force: r9d) -> eax
; Request correction for suspect/invalid token from hotpatcher
; Returns: eax = corrected token length (0 if no correction)
;==========================================================================
PRIVATE_agent_request_token_correction:
agent_request_token_correction PROC
    push rbx
    sub rsp, 320 ; Space for local FailureDetectionResult and CorrectionResult
    
    mov rbx, rcx        ; context
    mov rsi, rdx        ; token
    mov rdi, r8         ; event
    mov r10d, r9d       ; force_replacement
    
    ; [STEP 1] Detect failure in token
    mov rcx, rsi        ; token_ptr
    call agent_strlen
    mov rdx, rax        ; token_len
    lea r8, [rsp + 32]  ; Local FailureDetectionResult
    call masm_detect_failure
    
    test rax, rax
    jz no_correction_available
    
    ; [STEP 2] Correct failure via Puppeteer
    lea rcx, [rsp + 32] ; failure_result_ptr
    mov rdx, 2          ; mode = MODE_ASK
    lea r8, [rsp + 288] ; Local CorrectionResult
    call masm_puppeteer_correct_response
    
    test rax, rax
    jz no_correction_available
    
    ; Copy corrected token back
    mov rax, [rsp + 288 + 8] ; corrected_response_ptr
    mov rdx, rax
    lea rcx, [rdi]      ; Event structure
    lea rcx, [rcx].STREAM_EVENT.replacement_token
    mov r8d, 256
    call agent_strcpy_safe
    
    mov eax, [rsp + 288 + 16] ; corrected_response_len
    jmp correction_requested
    
no_correction_available:
    xor eax, eax
    
correction_requested:
    add rsp, 320
    pop rbx
    ret
agent_request_token_correction ENDP

;==========================================================================
; PRIVATE: agent_apply_hotpatch_correction(request: rcx, response: rdx) -> rax
; Apply hotpatch-based correction to model behavior
; Returns: rax = corrected response length
;==========================================================================
PRIVATE_agent_apply_hotpatch_correction:
agent_apply_hotpatch_correction PROC
    push rbx
    sub rsp, 96
    
    mov rbx, rcx        ; hotpatch request
    mov rsi, rdx        ; response
    
    ; Determine which layer to patch
    mov eax, [rbx].HOTPATCH_REQUEST.patch_type
    
    cmp eax, 0          ; Memory layer
    je apply_memory_patch
    cmp eax, 1          ; Byte layer
    je apply_byte_patch
    
    ; Server layer
    lea rcx, [rbx].HOTPATCH_REQUEST.patch_data
    mov edx, [rbx].HOTPATCH_REQUEST.patch_size
    call hpatch_apply_server
    jmp hotpatch_applied
    
apply_memory_patch:
    lea rcx, [rbx].HOTPATCH_REQUEST.patch_data
    mov edx, [rbx].HOTPATCH_REQUEST.patch_size
    call hpatch_apply_memory
    jmp hotpatch_applied
    
apply_byte_patch:
    lea rcx, [rbx].HOTPATCH_REQUEST.patch_data
    mov edx, [rbx].HOTPATCH_REQUEST.patch_size
    call hpatch_apply_byte
    
hotpatch_applied:
    ; Return corrected response (would be modified by hotpatch)
    mov rcx, rsi
    call agent_strlen
    
    add rsp, 96
    pop rbx
    ret
agent_apply_hotpatch_correction ENDP

;==========================================================================
; PRIVATE: agent_log_correction_applied(context: rcx, response: rdx) -> void
; Log that correction was applied
;==========================================================================
PRIVATE_agent_log_correction_applied:
agent_log_correction_applied PROC
    push rbx
    sub rsp, 48
    
    mov rbx, rcx        ; context
    mov rsi, rdx        ; response
    
    ; Log through output pane or debug stream
    ; In implementation, would write to output_pane_logger.asm
    
    mov [rbx].CORRECTION_CONTEXT.correction_applied, 1
    mov eax, 1
    
    add rsp, 48
    pop rbx
    ret
agent_log_correction_applied ENDP

;==========================================================================
; HELPER STUBS (would be implemented or imported)
;==========================================================================

PUBLIC agent_strlen
agent_strlen PROC
    xor eax, eax
    ret
agent_strlen ENDP

PUBLIC agent_strcpy_safe
agent_strcpy_safe PROC
    xor eax, eax
    ret
agent_strcpy_safe ENDP

PUBLIC agent_memset_safe
agent_memset_safe PROC
    xor eax, eax
    ret
agent_memset_safe ENDP

; Removed C++ stubs - using pure MASM implementations

EXTERN SymbolTablePtr: QWORD

END
