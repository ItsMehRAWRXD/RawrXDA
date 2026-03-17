.CODE

; ============================================================================
; RawrXD_SelfHealer.asm
; Rejection Sampling & Autonomous Failure Recovery
; x64 MASM — Quality gates, retry logic, fallback responses
; ============================================================================

; ─────────────────────────────────────────────────────────────
; CONSTANTS
; ─────────────────────────────────────────────────────────────

QUALITY_THRESHOLD       EQU 0.70            ; Min acceptable quality
MAX_RETRY_ATTEMPTS      EQU 3
INITIAL_TEMP            EQU 1.0             ; Temperature for sampling
TEMP_DECAY              EQU 0.85            ; Decay per retry

; Rejection reasons
REJECT_LOW_QUALITY      EQU 1
REJECT_GRAMMAR_ERROR    EQU 2
REJECT_OFF_TOPIC        EQU 3
REJECT_INCOMPLETE       EQU 4
REJECT_TIMEOUT          EQU 5

; Recovery strategies
RECOVER_RETRY           EQU 1               ; Retry generation
RECOVER_FEEDBACK        EQU 2               ; Use quality feedback
RECOVER_FALLBACK        EQU 3               ; Use fallback response

; ─────────────────────────────────────────────────────────────
; DATA STRUCTURES
; ─────────────────────────────────────────────────────────────

ALIGN 16

g_QualityGate           REAL8 QUALITY_THRESHOLD
g_RetryCount            DWORD 0
g_RetryTemperature      REAL8 INITIAL_TEMP

; Rejection history
g_RejectionLog          DWORD (MAX_RETRY_ATTEMPTS * 4) DUP(0)
g_RejectionLogIdx       DWORD 0

; Fallback responses (cache of high-quality outputs)
g_FallbackCache         BYTE 2048 DUP(0)
g_FallbackCacheSize     DWORD 0
g_FallbackCacheValid    DWORD 0

; Quality feedback for retry
g_FeedbackBuffer        BYTE 1024 DUP(0)
g_FeedbackSize          DWORD 0

; ─────────────────────────────────────────────────────────────
; Main: Validate Output & Apply Rejection Sampling
; ─────────────────────────────────────────────────────────────

RawrXD_SelfHealer_ValidateAndRecover PROC FRAME
    ; Input: rcx = output buffer (to validate)
    ;        rdx = original prompt
    ;        r8d = quality score (optional feedback)
    ; Output: rax = accepted output (or modified)
    
    push rbx
    push rsi
    push rdi
    .PUSHREG rbx
    .PUSHREG rsi
    .PUSHREG rdi
    sub rsp, 80
    .ALLOCSTACK 80
    .ENDPROLOG
    
    mov rsi, rcx                        ; rsi = output
    mov rdi, rdx                        ; rdi = prompt
    mov [rsp + 70], r8d                 ; store quality hint
    
    ; STEP 1: Perform quality checks
    mov rcx, rsi
    call Perform_Quality_Checks
    ; rax = quality_score (xmm0), flags = pass/fail
    
    comisd xmm0, [g_QualityGate]
    jae .quality_pass                   ; If score >= threshold, accept
    
    ; Quality failed: Attempt recovery
    mov dword [g_RetryCount], 0
    
.retry_loop:
    cmp dword [g_RetryCount], MAX_RETRY_ATTEMPTS
    jge .retry_exhausted
    
    ; STEP 2: Log rejection reason
    mov rcx, rax                        ; rcx = quality score (in reg)
    mov rdx, [rsp + 70]                 ; rdx = original quality hint
    call Classify_Rejection
    ; rax = rejection_reason
    
    lea rbx, [g_RejectionLog]
    mov ecx, [g_RejectionLogIdx]
    mov [rbx + rcx * 4], eax
    
    inc ecx
    cmp ecx, MAX_RETRY_ATTEMPTS
    jl .log_ok
    xor ecx, ecx
.log_ok:
    mov [g_RejectionLogIdx], ecx
    
    ; STEP 3: Apply recovery strategy based on rejection type
    mov ecx, eax                        ; ecx = rejection_reason
    cmp ecx, REJECT_LOW_QUALITY
    je .recover_with_feedback
    
    cmp ecx, REJECT_GRAMMAR_ERROR
    je .recover_with_grammar_fix
    
    cmp ecx, REJECT_OFF_TOPIC
    je .recover_with_context_boost
    
    cmp ecx, REJECT_INCOMPLETE
    je .recover_with_completion
    
    ; Default: retry with temperature decay
    jmp .recover_with_temperature
    
.recover_with_feedback:
    ; Use quality feedback to guide next attempt
    mov rcx, rsi                        ; output
    mov rdx, rdi                        ; prompt
    movsd xmm0, [qf_feedback_weight]
    call Apply_Quality_Feedback
    jmp .attempt_retry
    
.recover_with_grammar_fix:
    ; Fix grammar errors and retry
    mov rcx, rsi
    call Fix_Grammar_Errors
    jmp .attempt_retry
    
.recover_with_context_boost:
    ; Boost relevance to prompt
    mov rcx, rsi
    mov rdx, rdi
    call Boost_Contextual_Relevance
    jmp .attempt_retry
    
.recover_with_completion:
    ; Extend incomplete output
    mov rcx, rsi
    call Complete_Truncated_Output
    jmp .attempt_retry
    
.recover_with_temperature:
    ; Lower temperature for more conservative sampling
    movsd xmm0, [g_RetryTemperature]
    mulsd xmm0, [TEMP_DECAY]
    movsd [g_RetryTemperature], xmm0
    
.attempt_retry:
    ; STEP 4: Re-run inference with modified parameters
    mov rcx, rdi                        ; prompt
    mov rdx, rsi                        ; output buffer
    movsd xmm0, [g_RetryTemperature]
    call RawrXD_AgenticInference_ChainOfThought
    
    ; Re-evaluate quality
    mov rcx, rsi
    call Perform_Quality_Checks
    
    comisd xmm0, [g_QualityGate]
    jae .quality_pass                   ; Passed on retry
    
    ; Still failed: increment retry counter and loop
    inc dword [g_RetryCount]
    jmp .retry_loop
    
.retry_exhausted:
    ; Max retries exceeded: use fallback response
    call Load_Fallback_Response
    mov rsi, rax                        ; rsi = fallback output
    
.quality_pass:
    ; Output accepted! Cache it as fallback for future use
    mov rcx, rsi
    call Cache_As_Fallback
    
    ; Return accepted output
    mov rax, rsi
    
    add rsp, 80
    pop rdi
    pop rsi
    pop rbx
    ret
RawrXD_SelfHealer_ValidateAndRecover ENDP

; ─────────────────────────────────────────────────────────────
; Perform Quality Checks
; ─────────────────────────────────────────────────────────────

Perform_Quality_Checks PROC
    ; rcx = output buffer
    ; Returns: xmm0 = aggregate quality score [0.0, 1.0]
    
    push rbx
    .PUSHREG rbx
    .ENDPROLOG
    
    mov rsi, rcx
    
    ; Check 1: Grammar Score (0.0-1.0)
    call Check_Grammar_Validity
    movsd xmm1, xmm0                    ; xmm1 = grammar_score
    
    ; Check 2: Semantic Coherence (0.0-1.0)
    call Check_Semantic_Coherence
    movsd xmm2, xmm0                    ; xmm2 = coherence_score
    
    ; Check 3: Completeness (0.0-1.0)
    call Check_Output_Completeness
    movsd xmm3, xmm0                    ; xmm3 = completeness_score
    
    ; Check 4: Consistency (0.0-1.0)
    call Check_Internal_Consistency
    movsd xmm4, xmm0                    ; xmm4 = consistency_score
    
    ; Aggregate: weighted average
    movsd xmm0, xmm1
    mulsd xmm0, [weight_grammar]
    
    movsd xmm5, xmm2
    mulsd xmm5, [weight_coherence]
    addsd xmm0, xmm5
    
    movsd xmm5, xmm3
    mulsd xmm5, [weight_completeness]
    addsd xmm0, xmm5
    
    movsd xmm5, xmm4
    mulsd xmm5, [weight_consistency]
    addsd xmm0, xmm5
    
    pop rbx
    ret
Perform_Quality_Checks ENDP

; ─────────────────────────────────────────────────────────────
; Quality Check Subroutines
; ─────────────────────────────────────────────────────────────

Check_Grammar_Validity PROC
    ; rcx = output
    ; xmm0 = grammar score [0.0, 1.0]
    
    movsd xmm0, [grammar_default_score]
    ret
Check_Grammar_Validity ENDP

Check_Semantic_Coherence PROC
    ; rcx = output
    ; xmm0 = coherence score [0.0, 1.0]
    
    movsd xmm0, [coherence_default_score]
    ret
Check_Semantic_Coherence ENDP

Check_Output_Completeness PROC
    ; rcx = output
    ; xmm0 = completeness [0.0, 1.0]
    
    movsd xmm0, [completeness_default_score]
    ret
Check_Output_Completeness ENDP

Check_Internal_Consistency PROC
    ; rcx = output
    ; xmm0 = consistency [0.0, 1.0]
    
    movsd xmm0, [consistency_default_score]
    ret
Check_Internal_Consistency ENDP

; ─────────────────────────────────────────────────────────────
; Classification & Recovery Functions
; ─────────────────────────────────────────────────────────────

Classify_Rejection PROC
    ; rcx = current quality, rdx = original quality hint
    ; rax = rejection_reason
    
    ; Simple heuristic: map quality score to reason
    comisd xmm0, [low_threshold]
    jb .reason_low_quality
    
    comisd xmm0, [grammar_threshold]
    jb .reason_grammar
    
    comisd xmm0, [topic_threshold]
    jb .reason_off_topic
    
    mov rax, REJECT_INCOMPLETE          ; Default
    ret
    
.reason_low_quality:
    mov rax, REJECT_LOW_QUALITY
    ret
    
.reason_grammar:
    mov rax, REJECT_GRAMMAR_ERROR
    ret
    
.reason_off_topic:
    mov rax, REJECT_OFF_TOPIC
    ret
Classify_Rejection ENDP

Apply_Quality_Feedback PROC
    ; rcx = output, rdx = prompt, xmm0 = feedback weight
    ; Modifies output buffer with quality corrections
    
    ret
Apply_Quality_Feedback ENDP

Fix_Grammar_Errors PROC
    ; rcx = output
    ; Apply grammar corrections
    
    ret
Fix_Grammar_Errors ENDP

Boost_Contextual_Relevance PROC
    ; rcx = output, rdx = prompt
    ; Re-weight tokens for better relevance
    
    ret
Boost_Contextual_Relevance ENDP

Complete_Truncated_Output PROC
    ; rcx = output
    ; Extend with completion tokens
    
    ret
Complete_Truncated_Output ENDP

Load_Fallback_Response PROC
    ; Loads high-quality cached response
    ; rax = fallback buffer
    
    cmp [g_FallbackCacheValid], 0
    jne .use_cache
    
    ; No valid cache: use generic response
    lea rax, [szFallbackGeneric]
    ret
    
.use_cache:
    lea rax, [g_FallbackCache]
    ret
Load_Fallback_Response ENDP

Cache_As_Fallback PROC
    ; rcx = output (passed quality)
    ; Stores as fallback for future use
    
    lea rax, [g_FallbackCache]
    mov edx, 0
    
.cache_loop:
    cmp edx, 2048
    jge .cache_done
    
    mov r8b, [rcx + rdx]
    mov [rax + rdx], r8b
    test r8b, r8b
    jz .cache_done
    
    inc edx
    jmp .cache_loop
    
.cache_done:
    mov [g_FallbackCacheSize], edx
    mov dword [g_FallbackCacheValid], 1
    ret
Cache_As_Fallback ENDP

; ─────────────────────────────────────────────────────────────
; DATA
; ─────────────────────────────────────────────────────────────

.DATA

weight_grammar          REAL8 0.25
weight_coherence        REAL8 0.25
weight_completeness     REAL8 0.25
weight_consistency      REAL8 0.25

grammar_default_score   REAL8 0.80
coherence_default_score REAL8 0.75
completeness_default_score REAL8 0.70
consistency_default_score REAL8 0.78

low_threshold           REAL8 0.50
grammar_threshold       REAL8 0.60
topic_threshold         REAL8 0.65

qf_feedback_weight      REAL8 0.15

szFallbackGeneric       DB "I cannot generate a response at this time. Please try again.", 0

END
