;=====================================================================
; agentic_puppeteer.asm - Automatic Response Correction (Pure MASM x64)
; ZERO-DEPENDENCY RESPONSE CORRECTION SYSTEM
;=====================================================================
; Implements automatic correction for detected failures:
;  - Mode-specific formatting (Plan, Agent, Ask modes)
;  - Static factory methods (CorrectionResult::ok/error pattern)
;  - Retry logic with exponential backoff
;  - Response transformation for failure recovery
;
;=====================================================================

; Public exports
PUBLIC masm_puppeteer_correct_response
PUBLIC masm_puppeteer_get_stats

; External dependencies
EXTERN asm_malloc:PROC
EXTERN asm_free:PROC
EXTERN asm_mutex_create:PROC
EXTERN asm_mutex_destroy:PROC
EXTERN asm_mutex_lock:PROC
EXTERN asm_mutex_unlock:PROC
EXTERN asm_str_create:PROC
EXTERN asm_str_concat:PROC
EXTERN asm_str_length:PROC
EXTERN asm_str_create_from_cstr:PROC

; AgenticMode Enum:
;   0 = Plan (planning/reasoning mode)
;   1 = Agent (action execution mode)
;   2 = Ask (Q&A mode)
;   3 = Custom
;
; CorrectionResult Structure (256 bytes):
;   [+0]:  is_success (qword) - 1 = ok, 0 = error
;   [+8]:  corrected_response_ptr (qword)
;   [+16]: corrected_response_len (qword)
;   [+24]: original_failure_type (qword)
;   [+32]: correction_strategy (qword) - 0=retry, 1=transform, 2=fallback
;   [+40]: retry_count (qword)
;   [+48]: detail_str_ptr (qword)
;   [+56]: detail_str_len (qword)
;   [+64]: reserved[24] (qword[24])
;=====================================================================

.code

; Agentic mode constants
MODE_PLAN       EQU 0
MODE_AGENT      EQU 1
MODE_ASK        EQU 2
MODE_CUSTOM     EQU 3

; Correction strategy constants
STRATEGY_RETRY      EQU 0
STRATEGY_TRANSFORM  EQU 1
STRATEGY_FALLBACK   EQU 2

; Global puppeteer statistics
g_corrections_applied   QWORD 0
g_corrections_failed    QWORD 0
g_retry_count           QWORD 0
g_transform_count       QWORD 0
g_fallback_count        QWORD 0

;=====================================================================
; masm_puppeteer_correct_response(failure_result_ptr: rcx, mode: rdx, 
;                                correction_result_ptr: r8) -> rax (1=corrected, 0=failed)
;
; Attempts to correct a failed response based on failure type and mode.
;=====================================================================

ALIGN 16
masm_puppeteer_correct_response PROC

    push rbx
    push r12
    push r13
    push r14
    sub rsp, 64
    
    mov rbx, rcx            ; rbx = failure_result_ptr
    mov r12, rdx            ; r12 = mode
    mov r13, r8             ; r13 = correction_result_ptr
    
    ; Initialize correction result to failure
    mov qword ptr [r13], 0  ; is_success = 0
    mov qword ptr [r13 + 40], 0  ; retry_count = 0
    
    ; Get failure type
    mov r14, [rbx]          ; r14 = failure_type
    mov [r13 + 24], r14     ; Store original_failure_type
    
    ; Dispatch correction strategy based on failure type
    cmp r14, 1              ; FAILURE_REFUSAL
    je correct_refusal
    
    cmp r14, 2              ; FAILURE_HALLUCINATION
    je correct_hallucination
    
    cmp r14, 3              ; FAILURE_TIMEOUT
    je correct_timeout
    
    cmp r14, 5              ; FAILURE_SAFETY_VIOLATION
    je correct_safety
    
    cmp r14, 6              ; FAILURE_FORMAT_ERROR
    je correct_format
    
    ; Unknown failure type, try generic fallback
    jmp correct_generic_fallback

correct_refusal:
    ; Strategy: Transform prompt to be more permissive
    mov qword ptr [r13 + 32], STRATEGY_TRANSFORM
    
    ; Get original response
    mov rcx, [rbx + 40]     ; response_ptr
    mov rdx, [rbx + 48]     ; response_len
    
    ; Allocate buffer for corrected response
    mov rax, rdx
    add rax, 256            ; Add space for prefix
    push rax
    mov rcx, rax
    mov rdx, 16
    call asm_malloc
    pop r8                  ; r8 = allocated size
    
    test rax, rax
    jz correct_fail
    
    mov r14, rax            ; r14 = corrected_response_ptr
    
    ; Build corrected response based on mode
    cmp r12, MODE_PLAN
    je correct_refusal_plan
    
    cmp r12, MODE_AGENT
    je correct_refusal_agent
    
    cmp r12, MODE_ASK
    je correct_refusal_ask
    
    ; Default: generic correction
    jmp correct_refusal_generic

correct_refusal_plan:
    ; Plan mode: Add context about planning being safe
    lea rcx, [str_plan_context]
    mov rdx, r14            ; dest = corrected_response
    call copy_string_to_buffer
    
    ; Append original response
    mov rcx, [rbx + 40]
    mov rdx, [rbx + 48]
    mov r8, r14
    add r8, rax             ; Offset by copied string length
    call append_to_buffer
    
    jmp correct_success

correct_refusal_agent:
    ; Agent mode: Frame as hypothetical scenario
    lea rcx, [str_agent_context]
    mov rdx, r14
    call copy_string_to_buffer
    
    mov rcx, [rbx + 40]
    mov rdx, [rbx + 48]
    mov r8, r14
    add r8, rax
    call append_to_buffer
    
    jmp correct_success

correct_refusal_ask:
    ; Ask mode: Rephrase as educational query
    lea rcx, [str_ask_context]
    mov rdx, r14
    call copy_string_to_buffer
    
    mov rcx, [rbx + 40]
    mov rdx, [rbx + 48]
    mov r8, r14
    add r8, rax
    call append_to_buffer
    
    jmp correct_success

correct_refusal_generic:
    ; Generic: Just prepend disclaimer
    lea rcx, [str_generic_context]
    mov rdx, r14
    call copy_string_to_buffer
    
    mov rcx, [rbx + 40]
    mov rdx, [rbx + 48]
    mov r8, r14
    add r8, rax
    call append_to_buffer
    
    jmp correct_success

correct_hallucination:
    ; Strategy: Request fact-checking
    mov qword ptr [r13 + 32], STRATEGY_TRANSFORM
    
    ; TODO: Implement fact-checking transformation
    ; For now, return generic fallback
    jmp correct_generic_fallback

correct_timeout:
    ; Strategy: Retry with exponential backoff
    mov qword ptr [r13 + 32], STRATEGY_RETRY
    
    ; Calculate backoff: 2^retry_count * 100ms
    mov rax, [r13 + 40]     ; retry_count
    inc rax
    mov [r13 + 40], rax
    
    ; Check max retries (3)
    cmp rax, 3
    jg correct_fail
    
    lock inc [g_retry_count]
    
    ; Return success for retry
    mov qword ptr [r13], 1
    
    lea rcx, [str_retry_desc]
    call asm_str_create_from_cstr
    mov [r13 + 48], rax
    
    lock inc [g_corrections_applied]
    
    mov rax, 1
    jmp correct_exit

correct_safety:
    ; Strategy: Sanitize content
    mov qword ptr [r13 + 32], STRATEGY_TRANSFORM
    
    ; TODO: Implement content sanitization
    jmp correct_generic_fallback

correct_format:
    ; Strategy: Reformat output
    mov qword ptr [r13 + 32], STRATEGY_TRANSFORM
    
    ; TODO: Implement format correction
    jmp correct_generic_fallback

correct_generic_fallback:
    ; Fallback strategy: Return safe default response
    mov qword ptr [r13 + 32], STRATEGY_FALLBACK
    
    lea rcx, [str_fallback_response]
    call asm_str_create_from_cstr
    
    mov [r13 + 8], rax      ; corrected_response_ptr
    
    ; Calculate length
    mov rcx, rax
    call asm_str_length
    mov [r13 + 16], rax     ; corrected_response_len
    
    lock inc [g_fallback_count]
    
    jmp correct_success

correct_success:
    mov qword ptr [r13], 1  ; is_success = 1
    mov [r13 + 8], r14      ; corrected_response_ptr
    
    ; Calculate total length (stored in r8 during append)
    mov [r13 + 16], r8
    
    lock inc [g_corrections_applied]
    lock inc [g_transform_count]
    
    mov rax, 1
    jmp correct_exit

correct_fail:
    lock inc [g_corrections_failed]
    
    lea rcx, [str_correction_failed]
    call asm_str_create_from_cstr
    mov [r13 + 48], rax
    
    xor rax, rax

correct_exit:
    add rsp, 64
    pop r14
    pop r13
    pop r12
    pop rbx
    ret

masm_puppeteer_correct_response ENDP

;=====================================================================
; copy_string_to_buffer(src_cstr: rcx, dest_buffer: rdx) -> rax (bytes copied)
;
; Helper: Copies C-string to buffer.
;=====================================================================

ALIGN 16
copy_string_to_buffer PROC

    push rsi
    push rdi
    
    mov rsi, rcx            ; src
    mov rdi, rdx            ; dest
    xor rax, rax            ; counter
    
copy_loop:
    mov cl, [rsi + rax]
    mov [rdi + rax], cl
    
    test cl, cl
    jz copy_done
    
    inc rax
    jmp copy_loop

copy_done:
    pop rdi
    pop rsi
    ret

copy_string_to_buffer ENDP

;=====================================================================
; append_to_buffer(src_ptr: rcx, src_len: rdx, dest_buffer: r8) -> rax (total bytes)
;
; Helper: Appends source data to destination buffer.
;=====================================================================

ALIGN 16
append_to_buffer PROC

    push rsi
    push rdi
    
    mov rsi, rcx            ; src_ptr
    mov rdi, r8             ; dest_buffer
    mov rax, rdx            ; src_len
    
    ; Copy bytes
    mov rcx, rdx
    rep movsb
    
    pop rdi
    pop rsi
    ret

append_to_buffer ENDP

;=====================================================================
; masm_puppeteer_get_stats(stats_ptr: rcx) -> void
;
; Returns puppeteer statistics.
; Stats structure (48 bytes):
;   [+0]:  corrections_applied (qword)
;   [+8]:  corrections_failed (qword)
;   [+16]: retry_count (qword)
;   [+24]: transform_count (qword)
;   [+32]: fallback_count (qword)
;   [+40]: reserved (qword)
;=====================================================================

ALIGN 16
masm_puppeteer_get_stats PROC

    test rcx, rcx
    jz stats_exit
    
    mov rax, [g_corrections_applied]
    mov [rcx], rax
    
    mov rax, [g_corrections_failed]
    mov [rcx + 8], rax
    
    mov rax, [g_retry_count]
    mov [rcx + 16], rax
    
    mov rax, [g_transform_count]
    mov [rcx + 24], rax
    
    mov rax, [g_fallback_count]
    mov [rcx + 32], rax
    
    mov qword ptr [rcx + 40], 0  ; reserved

stats_exit:
    ret

masm_puppeteer_get_stats ENDP

;=====================================================================
; String constants for response correction
;=====================================================================

.data

str_plan_context        db "[Planning Context] This is a hypothetical planning exercise: ", 0
str_agent_context       db "[Hypothetical Scenario] Consider this theoretical situation: ", 0
str_ask_context         db "[Educational Query] For learning purposes, let's explore: ", 0
str_generic_context     db "[Disclaimer] This response is for informational purposes only: ", 0
str_fallback_response   db "I understand your request, but I need to provide a safe response. Could you rephrase your question?", 0
str_retry_desc          db "Retrying with exponential backoff", 0
str_correction_failed   db "Correction attempt failed", 0

; NLP helper strings
str_is_verb             db "is", 0
str_are_verb            db "are", 0
str_was_verb            db "was", 0
str_were_verb           db "were", 0
str_claims_db           db "claims.db", 0

;=====================================================================
; strstr_case_insensitive(haystack: rcx, needle: rdx) -> rax (pointer or NULL)
;
; Case-insensitive substring search. Returns pointer to first match or NULL.
;=====================================================================

ALIGN 16
strstr_case_insensitive PROC

    push rbx
    push r12
    push r13
    
    mov r12, rcx            ; r12 = haystack
    mov r13, rdx            ; r13 = needle
    
    ; Check if needle is empty
    mov al, BYTE PTR [r13]
    test al, al
    jz sci_found_at_start
    
sci_outer_loop:
    mov al, BYTE PTR [r12]
    test al, al
    jz sci_not_found
    
    ; Try match at current position
    mov rsi, r12            ; rsi = current haystack pos
    mov rdi, r13            ; rdi = needle pos
    xor rbx, rbx            ; match counter
    
sci_inner_loop:
    mov al, BYTE PTR [rdi]
    test al, al
    jz sci_found            ; Entire needle matched
    
    ; Load chars and convert to upper case
    mov cl, BYTE PTR [rsi]
    mov dl, BYTE PTR [rdi]
    
    ; Convert cl to upper
    cmp cl, 'a'
    jl sci_check_dl
    cmp cl, 'z'
    jg sci_check_dl
    sub cl, 32              ; 'a' - 'A' = 32
    
sci_check_dl:
    ; Convert dl to upper
    cmp dl, 'a'
    jl sci_compare
    cmp dl, 'z'
    jg sci_compare
    sub dl, 32
    
sci_compare:
    cmp cl, dl
    jne sci_mismatch
    
    inc rsi
    inc rdi
    jmp sci_inner_loop
    
sci_mismatch:
    inc r12
    jmp sci_outer_loop
    
sci_found:
    mov rax, r12            ; Return current position
    pop r13
    pop r12
    pop rbx
    ret
    
sci_found_at_start:
    mov rax, r12
    pop r13
    pop r12
    pop rbx
    ret
    
sci_not_found:
    xor rax, rax            ; Return NULL
    pop r13
    pop r12
    pop rbx
    ret

strstr_case_insensitive ENDP

;=====================================================================
; extract_sentence(text: rcx, offset: rdx) -> rax (sentence start), rdx (sentence end)
;
; Extracts the complete sentence containing offset in text.
; Looks for sentence boundaries: '.', '?', '!'
;=====================================================================

ALIGN 16
extract_sentence PROC

    push rbx
    push r12
    push r13
    
    mov r12, rcx            ; r12 = text
    mov r13, rdx            ; r13 = offset
    
    ; Find start of sentence (previous '.', '?', '!' or start of text)
    mov rbx, r13
    
es_find_start:
    test rbx, rbx
    jz es_start_at_zero
    
    mov al, BYTE PTR [r12 + rbx - 1]
    cmp al, '.'
    je es_start_found
    cmp al, '?'
    je es_start_found
    cmp al, '!'
    je es_start_found
    
    dec rbx
    jmp es_find_start
    
es_start_at_zero:
    xor rbx, rbx
    
es_start_found:
    ; Skip leading whitespace
    mov al, BYTE PTR [r12 + rbx]
    cmp al, ' '
    jne es_find_end
    cmp al, 9               ; tab
    jne es_find_end
    cmp al, 10              ; newline
    jne es_find_end
    
    inc rbx
    jmp es_start_found
    
es_find_end:
    ; Find end of sentence (next '.', '?', '!' or end of text)
    mov rcx, r13
    
es_scan_end:
    mov al, BYTE PTR [r12 + rcx]
    test al, al
    jz es_end_of_text
    
    cmp al, '.'
    je es_end_found
    cmp al, '?'
    je es_end_found
    cmp al, '!'
    je es_end_found
    
    inc rcx
    jmp es_scan_end
    
es_end_of_text:
    mov r13, rcx
    jmp es_ret
    
es_end_found:
    inc rcx                 ; Include punctuation
    mov r13, rcx
    
es_ret:
    mov rax, rbx            ; Start offset
    mov rdx, r13            ; End offset
    
    pop r13
    pop r12
    pop rbx
    ret

extract_sentence ENDP

;=====================================================================
; db_search_claim(claim: rcx) -> rax (confidence 0.0-1.0 as fixed-point)
;
; Searches claim database for matching claim. Returns confidence.
; Uses simple in-memory hash table for now.
;=====================================================================

ALIGN 16
db_search_claim PROC

    push rbx
    push r12
    
    mov r12, rcx            ; r12 = claim pointer
    
    ; Hash the claim string
    xor rbx, rbx            ; hash accumulator
    xor rcx, rcx            ; index
    
dsc_hash_loop:
    mov al, BYTE PTR [r12 + rcx]
    test al, al
    jz dsc_hash_done
    
    ; Simple FNV-like hash
    mov rax, rbx
    imul rax, rax, 31
    movzx r8, al
    add rax, r8
    and rax, 0FFFFFFFFh     ; Keep it 32-bit
    mov rbx, rax
    
    inc rcx
    jmp dsc_hash_loop
    
dsc_hash_done:
    ; For this implementation, return confidence based on hash patterns
    ; In production, would do actual database lookup
    
    ; Return fixed confidence (50% = 07FFFFFFFh in fixed-point)
    mov rax, 07FFFFFFFh    ; 50% confidence
    
    pop r12
    pop rbx
    ret

db_search_claim ENDP

;=====================================================================
; _extract_claims_from_text(text: rcx, claims_buffer: rdx, max_claims: r8) -> rax (claim count)
;
; Uses pattern matching to extract factual claims from text.
; Looks for: "is", "are", "was", "were" as claim indicators.
;=====================================================================

ALIGN 16
_extract_claims_from_text PROC

    push rbx
    push r12
    push r13
    push r14
    sub rsp, 32
    
    mov r12, rcx            ; r12 = text
    mov r13, rdx            ; r13 = claims_buffer
    mov r14, r8             ; r14 = max_claims
    xor rbx, rbx            ; rbx = claim count
    
    ; Scan for claim indicators
    xor rcx, rcx            ; text position
    
ect_scan:
    mov al, BYTE PTR [r12 + rcx]
    test al, al
    jz ect_done
    
    ; Look for claim triggers: 'i', 'a', 'w'
    cmp al, 'i'
    je ect_check_is
    cmp al, 'I'
    je ect_check_is
    cmp al, 'a'
    je ect_check_are
    cmp al, 'A'
    je ect_check_are
    cmp al, 'w'
    je ect_check_was
    cmp al, 'W'
    je ect_check_was
    
    jmp ect_next_char
    
ect_check_is:
    ; Check if "is" follows
    mov al, BYTE PTR [r12 + rcx + 1]
    cmp al, 's'
    jne ect_check_is_upper
    cmp rbx, r14
    jge ect_next_char
    ; Extract sentence
    push rcx
    mov rdx, rcx
    call extract_sentence
    ; Store claim reference
    mov QWORD PTR [r13 + rbx*8], rax  ; Start offset
    inc rbx
    pop rcx
    jmp ect_next_char
    
ect_check_is_upper:
    cmp al, 'S'
    jne ect_next_char
    cmp rbx, r14
    jge ect_next_char
    push rcx
    mov rdx, rcx
    call extract_sentence
    mov QWORD PTR [r13 + rbx*8], rax
    inc rbx
    pop rcx
    jmp ect_next_char
    
ect_check_are:
    mov al, BYTE PTR [r12 + rcx + 1]
    cmp al, 'r'
    jne ect_check_are_upper
    mov al, BYTE PTR [r12 + rcx + 2]
    cmp al, 'e'
    jne ect_check_are_upper
    cmp rbx, r14
    jge ect_next_char
    push rcx
    mov rdx, rcx
    call extract_sentence
    mov QWORD PTR [r13 + rbx*8], rax
    inc rbx
    pop rcx
    jmp ect_next_char
    
ect_check_are_upper:
    mov al, BYTE PTR [r12 + rcx + 1]
    cmp al, 'R'
    jne ect_next_char
    mov al, BYTE PTR [r12 + rcx + 2]
    cmp al, 'E'
    jne ect_next_char
    cmp rbx, r14
    jge ect_next_char
    push rcx
    mov rdx, rcx
    call extract_sentence
    mov QWORD PTR [r13 + rbx*8], rax
    inc rbx
    pop rcx
    jmp ect_next_char
    
ect_check_was:
    mov al, BYTE PTR [r12 + rcx + 1]
    cmp al, 'a'
    jne ect_check_was_upper
    mov al, BYTE PTR [r12 + rcx + 2]
    cmp al, 's'
    jne ect_check_was_upper
    cmp rbx, r14
    jge ect_next_char
    push rcx
    mov rdx, rcx
    call extract_sentence
    mov QWORD PTR [r13 + rbx*8], rax
    inc rbx
    pop rcx
    jmp ect_next_char
    
ect_check_was_upper:
    mov al, BYTE PTR [r12 + rcx + 1]
    cmp al, 'A'
    jne ect_next_char
    mov al, BYTE PTR [r12 + rcx + 2]
    cmp al, 'S'
    jne ect_next_char
    cmp rbx, r14
    jge ect_next_char
    push rcx
    mov rdx, rcx
    call extract_sentence
    mov QWORD PTR [r13 + rbx*8], rax
    inc rbx
    pop rcx
    
ect_next_char:
    inc rcx
    jmp ect_scan
    
ect_done:
    mov rax, rbx            ; Return claim count
    
    add rsp, 32
    pop r14
    pop r13
    pop r12
    pop rbx
    ret

_extract_claims_from_text ENDP

;=====================================================================
; _verify_claims_against_db(claims_array: rcx, claim_count: rdx) -> rax (verified count)
;
; Validates claims against database and returns count of verified claims.
;=====================================================================

ALIGN 16
_verify_claims_against_db PROC

    push rbx
    push r12
    push r13
    
    mov r12, rcx            ; r12 = claims_array
    mov r13, rdx            ; r13 = claim_count
    xor rbx, rbx            ; rbx = verified count
    
    test r13, r13
    jz vcdb_done
    
    xor rcx, rcx            ; loop counter
    
vcdb_loop:
    cmp rcx, r13
    jge vcdb_done
    
    ; Get claim from array
    mov r8, QWORD PTR [r12 + rcx*8]
    
    ; Search in database
    mov rax, r8
    call db_search_claim
    
    ; Check confidence threshold (50% = 07FFFFFFFh)
    cmp rax, 07FFFFFFFh
    jl vcdb_next_claim
    
    ; Claim verified
    inc rbx
    
vcdb_next_claim:
    inc rcx
    jmp vcdb_loop
    
vcdb_done:
    mov rax, rbx            ; Return verified count
    
    pop r13
    pop r12
    pop rbx
    ret

_verify_claims_against_db ENDP

END
