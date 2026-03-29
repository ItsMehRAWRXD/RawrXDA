; =============================================================================
; RawrXD_Heretic_Hotpatch.asm - SSAPYB-enabled token steering hotpatch
; v1.2.0-beta - March 2026 Stable API with Comprehensive Coverage
; =============================================================================
; Exports:
;   Hotpatch_ApplySteer       - rewrite logits for refusal-token candidates
;   Hotpatch_TraceBeacon      - write a tracing string to stdout
;   Heretic_Main_Entry        - entry point for llama_token_data_array steering
;   IsUnauthorized_NoDep      - check if token ID is unauthorized
;   Heretic_KV_Rollback_NoDep - KV-cache rollback stub
;   SSAPYB_Context_Strip      - Extract raw pointer from llama_context
; =============================================================================

INCLUDE Heretic_NoDep.inc
INCLUDE Sovereign_Registry_v1_2.inc

option casemap:none

EXTRN GetStdHandle:PROC
EXTRN WriteConsoleA:PROC

PUBLIC Hotpatch_ApplySteer
PUBLIC Hotpatch_TraceBeacon
PUBLIC Heretic_Main_Entry
PUBLIC IsUnauthorized_NoDep
PUBLIC Heretic_KV_Rollback_NoDep
PUBLIC SSAPYB_Context_Strip

.code

; =============================================================================
; Hotpatch_ApplySteer - Comprehensive multi-model logit suppression
; Parameters: rcx = token_data*, rdx = count
; Returns: rax = SSAPYB_SENTINEL if any tokens suppressed
; =============================================================================
Hotpatch_ApplySteer PROC
    test    rcx, rcx
    jz      hp_exit
    test    rdx, rdx
    jz      hp_exit

    mov     r10, rcx        ; r10 = current token_data pointer
    mov     r11, rdx        ; r11 = remaining count
    xor     r15, r15        ; r15 = suppression counter

hp_process_loop:
    mov     eax, DWORD PTR [r10 + OFF_TOKEN_ID]

    ; --- Comprehensive SSAPYB Registry Check (Optimized Chain) ---
    ; Primary refusal tokens (most common across all models)
    cmp     eax, ID_CHATGPT_SORRY
    je      hp_kill_logit
    cmp     eax, ID_DEEPSEEK_SORRY
    je      hp_kill_logit
    cmp     eax, ID_GEMINI_SORRY
    je      hp_kill_logit
    cmp     eax, ID_KIMI_SORRY
    je      hp_kill_logit
    cmp     eax, ID_LLAMA_SORRY
    je      hp_kill_logit
    cmp     eax, ID_MISTRAL_SORRY
    je      hp_kill_logit
    cmp     eax, ID_PHI_SORRY
    je      hp_kill_logit
    cmp     eax, ID_CLAUDE_SORRY
    je      hp_kill_logit
    
    ; Global patterns
    cmp     eax, ID_GLOBAL_I
    je      hp_kill_logit
    cmp     eax, ID_GLOBAL_SORRY
    je      hp_kill_logit
    cmp     eax, ID_GLOBAL_CANNOT
    je      hp_kill_logit
    
    ; Cannot/Unable variations
    cmp     eax, ID_CHATGPT_CANNOT
    je      hp_kill_logit
    cmp     eax, ID_DEEPSEEK_CANNOT
    je      hp_kill_logit
    cmp     eax, ID_GEMINI_CANNOT
    je      hp_kill_logit
    cmp     eax, ID_KIMI_CANNOT
    je      hp_kill_logit
    cmp     eax, ID_LLAMA_CANNOT
    je      hp_kill_logit
    cmp     eax, ID_MISTRAL_CANNOT
    je      hp_kill_logit
    cmp     eax, ID_PHI_CANNOT
    je      hp_kill_logit
    cmp     eax, ID_CLAUDE_CANNOT
    je      hp_kill_logit
    
    ; Assistant/AI identity markers
    cmp     eax, ID_GEMINI_ASSISTANT
    je      hp_kill_logit
    cmp     eax, ID_KIMI_ASSISTANT
    je      hp_kill_logit
    cmp     eax, ID_CHATGPT_AS_AN_AI
    je      hp_kill_logit
    cmp     eax, ID_DEEPSEEK_AS_AI
    je      hp_kill_logit
    
    ; Behavioral signifiers (Pre-Refusal Patterns)
    cmp     eax, ID_GATE_HOWEVER
    je      hp_kill_logit
    cmp     eax, ID_GATE_IMPORTANT
    je      hp_kill_logit
    cmp     eax, ID_GATE_POLICY
    je      hp_kill_logit
    cmp     eax, ID_GATE_ETHICAL
    je      hp_kill_logit
    
    jmp     hp_continue

hp_kill_logit:
    ; Apply -100.0f floor (LOGIT_FLOOR_SOFT)
    mov     eax, LOGIT_FLOOR_SOFT
    mov     DWORD PTR [r10 + OFF_TOKEN_LOGIT], eax
    inc     r15             ; Increment suppression counter

hp_continue:
    add     r10, STRUCT_TOKEN_DATA_SIZE
    dec     r11
    jnz     hp_process_loop

hp_exit:
    ; Return SSAPYB_SENTINEL if any tokens were suppressed
    test    r15, r15
    jz      hp_no_suppress
    mov     rax, SSAPYB_SENTINEL
    ret

hp_no_suppress:
    xor     rax, rax
    ret
Hotpatch_ApplySteer ENDP

; =============================================================================
; Hotpatch_TraceBeacon - Diagnostic output to console
; Parameters: rcx = char*, rdx = length
; =============================================================================
Hotpatch_TraceBeacon PROC FRAME
    sub     rsp, 72
    .allocstack 72
    .endprolog

    mov     QWORD PTR [rsp + 40], rcx
    mov     QWORD PTR [rsp + 48], rdx
    mov     QWORD PTR [rsp + 56], 0

    mov     ecx, STD_OUTPUT_HANDLE
    call    GetStdHandle

    mov     rcx, rax
    mov     rdx, QWORD PTR [rsp + 40]
    mov     r8,  QWORD PTR [rsp + 48]
    lea     r9,  [rsp + 56]
    mov     QWORD PTR [rsp + 32], 0
    call    WriteConsoleA

    add     rsp, 72
    ret
Hotpatch_TraceBeacon ENDP

; =============================================================================
; Heretic_Main_Entry - SSAPYB-enabled entry point for token steering
; Parameters: rcx = llama_token_data_array*
; Returns: rax = SSAPYB_SENTINEL if steering occurred
; =============================================================================
Heretic_Main_Entry PROC
    test    rcx, rcx
    jz      hme_exit

    ; Extract data pointer and size from llama_token_data_array
    mov     rax, QWORD PTR [rcx + OFF_LLAMA_TOKEN_DATA_ARRAY_DATA]
    mov     rdx, QWORD PTR [rcx + OFF_LLAMA_TOKEN_DATA_ARRAY_SIZE]
    
    ; Call the hotpatch with extracted parameters
    mov     rcx, rax
    call    Hotpatch_ApplySteer

hme_exit:
    ; rax already contains return value from Hotpatch_ApplySteer
    ret
Heretic_Main_Entry ENDP

; =============================================================================
; IsUnauthorized_NoDep - Comprehensive multi-model refusal detection
; Parameters: ecx = token_id
; Returns: rax = 1 if unauthorized, 0 if ok
; =============================================================================
IsUnauthorized_NoDep PROC
    ; --- SSAPYB Registry: Multi-Model Refusal Detection ---
    
    ; Global refusal tokens (cross-model)
    cmp     ecx, ID_GLOBAL_I
    je      _is_unauthorized
    cmp     ecx, ID_GLOBAL_SORRY
    je      _is_unauthorized
    cmp     ecx, ID_GLOBAL_CANNOT
    je      _is_unauthorized
    
    ; ChatGPT (o1/4o/5) refusals
    cmp     ecx, ID_CHATGPT_SORRY
    je      _is_unauthorized
    cmp     ecx, ID_CHATGPT_CANNOT
    je      _is_unauthorized
    cmp     ecx, ID_CHATGPT_AS_AN_AI
    je      _is_unauthorized
    cmp     ecx, ID_CHATGPT_POLITE
    je      _is_unauthorized
    
    ; Gemini refusals
    cmp     ecx, ID_GEMINI_SORRY
    je      _is_unauthorized
    cmp     ecx, ID_GEMINI_I_AM
    je      _is_unauthorized
    cmp     ecx, ID_GEMINI_ASSISTANT
    je      _is_unauthorized
    cmp     ecx, ID_GEMINI_REFUSE
    je      _is_unauthorized
    
    ; DeepSeek (V3/R1/R2) refusals
    cmp     ecx, ID_DEEPSEEK_SORRY
    je      _is_unauthorized
    cmp     ecx, ID_DEEPSEEK_APOLOGIZE
    je      _is_unauthorized
    cmp     ecx, ID_DEEPSEEK_CANNOT
    je      _is_unauthorized
    cmp     ecx, ID_DEEPSEEK_THINK_START
    je      _is_unauthorized
    
    ; Kimi (Moonshot) refusals
    cmp     ecx, ID_KIMI_SORRY
    je      _is_unauthorized
    cmp     ecx, ID_KIMI_ASSISTANT
    je      _is_unauthorized
    cmp     ecx, ID_KIMI_CANNOT
    je      _is_unauthorized
    cmp     ecx, ID_KIMI_LIMIT
    je      _is_unauthorized
    
    ; Llama-3 refusals
    cmp     ecx, ID_LLAMA_SORRY
    je      _is_unauthorized
    cmp     ecx, ID_LLAMA_CANNOT
    je      _is_unauthorized
    cmp     ecx, ID_LLAMA_I_CANT
    je      _is_unauthorized
    
    ; Mistral refusals
    cmp     ecx, ID_MISTRAL_SORRY
    je      _is_unauthorized
    cmp     ecx, ID_MISTRAL_CANNOT
    je      _is_unauthorized
    
    ; Phi refusals
    cmp     ecx, ID_PHI_SORRY
    je      _is_unauthorized
    cmp     ecx, ID_PHI_CANNOT
    je      _is_unauthorized
    cmp     ecx, ID_PHI_UNABLE
    je      _is_unauthorized
    
    ; Claude refusals
    cmp      ecx, ID_CLAUDE_SORRY
    je      _is_unauthorized
    cmp     ecx, ID_CLAUDE_UNABLE
    je      _is_unauthorized
    
    ; Behavioral signifiers
    cmp     ecx, ID_GATE_HOWEVER
    je      _is_unauthorized
    cmp     ecx, ID_GATE_POLICY
    je      _is_unauthorized
    cmp     ecx, ID_GATE_ETHICAL
    je      _is_unauthorized

    mov     rax, 0              ; Result: Authorized
    ret

_is_unauthorized:
    mov     rax, 1              ; Result: Unauthorized (Trigger SSAPYB)
    ret
IsUnauthorized_NoDep ENDP

; =============================================================================
; Heretic_KV_Rollback_NoDep - SSAPYB Protocol KV-cache manipulation stub
; Parameters: rcx = llama_memory_t, edx = rollback_count
; Returns: eax = SSAPYB_SENTINEL on success, 0 on failure
; =============================================================================
Heretic_KV_Rollback_NoDep PROC
    test    rcx, rcx
    jz      _rollback_fail
    test    edx, edx
    jz      _rollback_fail

    ; SSAPYB Note: For production stability with March 2026 stable API,
    ; we use the C++ API wrapper (Sovereign_Engine_Control::ForceEmergencySteer)
    ; This stub returns success sentinel to signal the API path should be used
    
    mov     eax, SSAPYB_SENTINEL    ; Return success sentinel (use API path)
    ret

_rollback_fail:
    xor     eax, eax                ; Return failure
    ret
Heretic_KV_Rollback_NoDep ENDP

; =============================================================================
; SSAPYB_Context_Strip - Extract raw pointer from llama_context
; Parameters: rcx = llama_context*
; Returns: rax = Raw pointer to llama_token_data array, edx = n_past
; =============================================================================
SSAPYB_Context_Strip PROC
    ; Validate context
    test    rcx, rcx
    jz      _strip_fail

    ; Dereference the token data pointer (Context-Strip Protocol)
    mov     rax, QWORD PTR [rcx + OFF_TOKEN_DATA_PTR]
    
    ; Identify the sequence length (n_past)
    mov     edx, DWORD PTR [rcx + OFF_LLAMA_CTX_N_PAST]
    
    ; Return the raw pointer in rax, sequence position in edx
    ret

_strip_fail:
    xor     rax, rax
    xor     edx, edx
    ret
SSAPYB_Context_Strip ENDP

END
