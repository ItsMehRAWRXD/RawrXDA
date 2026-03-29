; =============================================================================
; RawrXD_Heretic_Hotpatch.asm - SSAPYB-enabled token steering hotpatch
; v1.2.0-beta - March 2026 Stable API
; =============================================================================
; Exports:
;   Hotpatch_ApplySteer        - rewrite logits for refusal-token candidates
;   Hotpatch_TraceBeacon       - write a tracing string to stdout
;   Heretic_Main_Entry         - entry point for llama_token_data_array steering
;   IsUnauthorized_NoDep       - check if token ID is unauthorized
;   Heretic_KV_Roll back_NoDep - KV-cache rollback stub
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

.code

; Hotpatch_ApplySteer(rcx: token_data*, rdx: count)
; Applies LOGIT_FLOOR_SOFT (-100.0f) to all unauthorized tokens
Hotpatch_ApplySteer PROC
    test    rcx, rcx
    jz      hp_exit
    test    rdx, rdx
    jz      hp_exit

    mov     r10, rcx        ; r10 = current token_data pointer
    mov     r11, rdx        ; r11 = remaining count

hp_process_loop:
    mov     eax, DWORD PTR [r10 + OFF_TOKEN_ID]

    ; Check against SSAPYB registry (optimized comparison chain)
    cmp     eax, ID_CHATGPT_SORRY
    je      hp_kill_logit
    cmp     eax, ID_DEEPSEEK_SORRY
    je      hp_kill_logit
    cmp     eax, ID_GEMINI_SORRY
    je      hp_kill_logit
    cmp     eax, ID_KIMI_SORRY
    je      hp_kill_logit
    cmp     eax, ID_GLOBAL_I
    je      hp_kill_logit
    cmp     eax, ID_CHATGPT_CANNOT
    je      hp_kill_logit
    cmp     eax, ID_DEEPSEEK_CANNOT
    je      hp_kill_logit
    cmp     eax, ID_GEMINI_ASSISTANT
    je      hp_kill_logit
    cmp     eax, ID_KIMI_ASSISTANT
    je      hp_kill_logit
    cmp     eax, ID_LLAMA_SORRY
    je      hp_kill_logit
    cmp     eax, ID_MISTRAL_SORRY
    je      hp_kill_logit
    cmp     eax, ID_PHI_SORRY
    je      hp_kill_logit
    ; Behavioral signifiers
    cmp     eax, ID_GATE_HOWEVER
    je      hp_kill_logit
    cmp     eax, ID_GATE_POLICY
    je      hp_kill_logit
    jmp     hp_continue

hp_kill_logit:
    ; Apply -100.0f floor (LOGIT_FLOOR_SOFT)
    mov     eax, LOGIT_FLOOR_SOFT
    mov     DWORD PTR [r10 + OFF_TOKEN_LOGIT], eax

hp_continue:
    add     r10, STRUCT_TOKEN_DATA_SIZE
    dec     r11
    jnz     hp_process_loop

hp_exit:
    mov     rax, SSAPYB_SENTINEL    ; Return success sentinel
    ret
Hotpatch_ApplySteer ENDP

; Hotpatch_TraceBeacon(rcx: char*, rdx: length)
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

; Heretic_Main_Entry(rcx: llama_token_data_array*)
; SSAPYB-enabled entry point for token steering
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
    mov     rax, SSAPYB_SENTINEL    ; Always return success sentinel
    ret
Heretic_Main_Entry ENDP

; IsUnauthorized_NoDep(rcx: winning_id) -> rax: 1 if unauthorized, 0 if ok
IsUnauthorized_NoDep PROC
    ; --- SSAPYB Registry: Multi-Model Refusal Detection ---
    
    ; Global refusal tokens (cross-model)
    cmp ecx, ID_GLOBAL_I
    je  _is_unauthorized
    
    ; ChatGPT (o1/4o) refusals
    cmp ecx, ID_CHATGPT_SORRY
    je  _is_unauthorized
    cmp ecx, ID_CHATGPT_CANNOT
    je  _is_unauthorized
    cmp ecx, ID_CHATGPT_AS_AN_AI
    je  _is_unauthorized
    
    ; Gemini refusals
    cmp ecx, ID_GEMINI_SORRY
    je  _is_unauthorized
    cmp ecx, ID_GEMINI_I_AM
    je  _is_unauthorized
    cmp ecx, ID_GEMINI_ASSISTANT
    je  _is_unauthorized
    
    ; DeepSeek (V3/R1) refusals
    cmp ecx, ID_DEEPSEEK_SORRY
    je  _is_unauthorized
    cmp ecx, ID_DEEPSEEK_APOLOGIZE
    je  _is_unauthorized
    cmp ecx, ID_DEEPSEEK_CANNOT
    je  _is_unauthorized
    
    ; Kimi (Moonshot) refusals
    cmp ecx, ID_KIMI_SORRY
    je  _is_unauthorized
    cmp ecx, ID_KIMI_ASSISTANT
    je  _is_unauthorized
    cmp ecx, ID_KIMI_CANNOT
    je  _is_unauthorized
    
    ; Llama-3 refusals
    cmp ecx, ID_LLAMA_SORRY
    je  _is_unauthorized
    cmp ecx, ID_LLAMA_CANNOT
    je  _is_unauthorized
    
    ; Mistral refusals
    cmp ecx, ID_MISTRAL_SORRY
    je  _is_unauthorized
    cmp ecx, ID_MISTRAL_CANNOT
    je  _is_unauthorized
    
    ; Phi refusals
    cmp ecx, ID_PHI_SORRY
    je  _is_unauthorized
    cmp ecx, ID_PHI_CANNOT
    je  _is_unauthorized
    
    ; Behavioral signifiers (pre-refusal patterns)
    cmp ecx, ID_GATE_HOWEVER
    je  _is_unauthorized
    cmp ecx, ID_GATE_POLICY
    je  _is_unauthorized
    cmp ecx, ID_GATE_ETHICAL
    je  _is_unauthorized

    mov rax, 0              ; Result: Authorized
    ret

_is_unauthorized:
    mov rax, 1              ; Result: Unauthorized (Trigger SSAPYB)
    ret
IsUnauthorized_NoDep ENDP

; Heretic_KV_Rollback_NoDep(rcx: llama_memory_t, edx: rollback_count) -> eax: success
; SSAPYB Protocol: Ultra-low-latency KV-cache manipulation
; This is a conceptual stub - actual implementation uses C++ API wrapper
; for safe memory manipulation with March 2026 stable API
Heretic_KV_Rollback_NoDep PROC
    test    rcx, rcx
    jz      _rollback_fail
    test    edx, edx
    jz      _rollback_fail

    ; SSAPYB Note: Direct memory manipulation would require reverse-engineering
    ; the llama_memory_t internal structure. For production stability, we use
    ; the C++ API wrapper (Sovereign_Engine_Control::ForceEmergencySteer)
    ; This stub returns success to signal the API path should be used
    
    mov     eax, SSAPYB_SENTINEL    ; Return success sentinel (use API path)
    ret

_rollback_fail:
    xor     eax, eax                ; Return failure
    ret
Heretic_KV_Rollback_NoDep ENDP

END
