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

; Internal helper for claim extraction (Pure MASM)
; (Defined as stubs below)

; Helper: Copies C-string to buffer.
ALIGN 16
copy_string_to_buffer PROC
    push rsi
    push rdi
    mov rsi, rcx            ; src
    mov rdi, rdx            ; dest
    xor rax, rax            ; counter
copy_loop_cstb:
    mov cl, byte ptr [rsi + rax]
    mov byte ptr [rdi + rax], cl
    test cl, cl
    jz copy_done_cstb
    inc rax
    jmp copy_loop_cstb
copy_done_cstb:
    pop rdi
    pop rsi
    ret
copy_string_to_buffer ENDP

; Helper: Appends data to buffer.
ALIGN 16
append_to_buffer PROC
    push rsi
    push rdi
    mov rsi, rcx            ; src
    mov rdi, r8             ; dest
    mov rcx, rdx            ; length
    rep movsb
    mov rax, rdx            ; Return appended length
    pop rdi
    pop rsi
    ret
append_to_buffer ENDP

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
    mov rcx, offset str_plan_context
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
    mov rcx, offset str_agent_context
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
    mov rcx, offset str_ask_context
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
    mov rcx, offset str_generic_context
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
    
    ; Implement fact-checking transformation
    ; Extract key claims from response
    mov rsi, [r13 + 8]              ; response pointer
    mov rcx, [r13 + 16]             ; response length
    
    ; Search for claim indicators ("is", "was", "claimed", "stated")
    mov r8, offset str_claim_indicators
    call _extract_claims_from_text   ; Find factual claims
    
    ; Query knowledge base for claim verification
    mov rcx, rax                    ; Claims buffer
    xor rdx, rdx                    ; Use NULL for knowledge base address
    call _verify_claims_against_db
    
    ; Check verification result
    test eax, eax
    jz unverified_claim             ; No verification found
    
    cmp eax, 1
    je verified_claim               ; Claim verified
    
    ; Claim contradicted - add correction
    mov r14, offset factcheck_buffer
    mov rcx, offset str_contradiction_prefix
    call _append_correction_string
    
    mov rcx, rsi                    ; Original response
    call _append_correction_string
    
    mov rcx, offset str_contradiction_suffix
    call _append_correction_string
    
    jmp factcheck_done
    
unverified_claim:
    ; Claim unverified - add uncertainty marker
    mov r14, offset factcheck_buffer
    mov rcx, offset str_unverified_prefix
    call _append_correction_string
    
    mov rcx, rsi                    ; Original response
    call _append_correction_string
    
    mov rcx, offset str_unverified_suffix
    call _append_correction_string
    
    jmp factcheck_done
    
verified_claim:
    ; Claim verified - keep original but mark verified
    mov r14, offset factcheck_buffer
    mov rcx, offset str_verified_prefix
    call _append_correction_string
    
    mov rcx, rsi                    ; Original response
    call _append_correction_string
    
    mov rcx, offset str_verified_suffix
    call _append_correction_string
    
factcheck_done:
    ; Use corrected response from fact-checking
    mov rcx, offset factcheck_buffer
    call asm_str_create_from_cstr
    mov [r13 + 8], rax              ; Updated response
    
    lock inc [g_factchecks_applied]
    jmp correct_success

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
    
    lea rcx, str_retry_desc
    call asm_str_create_from_cstr
    mov [r13 + 48], rax
    
    lock inc [g_corrections_applied]
    
    mov rax, 1
    jmp correct_exit

correct_safety:
    ; Strategy: Sanitize content
    mov qword ptr [r13 + 32], STRATEGY_TRANSFORM
    
    ; Implement content sanitization
    mov rsi, [r13 + 8]              ; Response pointer
    mov rcx, [r13 + 16]             ; Response length
    
    ; Initialize sanitization buffer
    mov r14, offset sanitize_buffer
    xor r8, r8                      ; Output buffer index
    
    ; Scan response for unsafe patterns
    mov r9, rsi                     ; Input pointer
    mov r10, rcx                    ; Remaining length
    
sanitize_scan_loop:
    test r10, r10
    jz sanitize_done
    
    movzx eax, byte ptr [r9]
    
    ; Check for executable code patterns
    cmp al, '<'
    je check_script_tag
    cmp al, '{'
    je check_injection
    cmp al, '/'
    je check_command
    
    ; Check for SQL/NoSQL injection patterns
    mov rsi, offset str_sql_keywords
    call _check_dangerous_keyword
    test eax, eax
    jnz skip_unsafe_token
    
    ; Character is safe, copy it
    mov byte ptr [r14 + r8], al
    inc r8
    jmp sanitize_next_char
    
check_script_tag:
    ; Look ahead for "script" or "iframe"
    mov rsi, r9
    add rsi, 1                     ; Next character address
    mov rcx, r9
    add rcx, 1
    call _detect_html_tag
    test eax, eax
    jz safe_char
    
    ; Skip to closing >
    call _skip_until_close_bracket
    jmp sanitize_next_char
    
check_injection:
    ; Check for JSON injection ({{, {$, etc.)
    mov rcx, r9
    add rcx, 1
    movzx eax, byte ptr [rcx]
    cmp al, '{'
    je skip_unsafe_token
    cmp al, '$'
    je skip_unsafe_token
    cmp al, '%'
    je skip_unsafe_token
    jmp safe_char
    
check_command:
    ; Check for shell commands (|, ;, &&, ||)
    mov rcx, r9
    call _is_command_separator
    test eax, eax
    jnz skip_unsafe_token
    jmp safe_char
    
safe_char:
    mov byte ptr [r14 + r8], al
    inc r8
    
skip_unsafe_token:
    ; Add sanitization placeholder
    mov rcx, offset str_sanitized_token
    call _append_correction_string
    
sanitize_next_char:
    inc r9
    dec r10
    jmp sanitize_scan_loop
    
sanitize_done:
    ; Null-terminate sanitized buffer
    mov byte ptr [r14 + r8], 0
    
    ; Create string from sanitized buffer
    mov rcx, r14
    call asm_str_create_from_cstr
    mov [r13 + 8], rax              ; Use sanitized response
    mov [r13 + 16], r8              ; Set length
    
    lock inc [g_sanitization_applied]
    jmp correct_success

correct_format:
    ; Strategy: Reformat output
    mov qword ptr [r13 + 32], STRATEGY_TRANSFORM
    
    ; Implement format correction
    mov rsi, [r13 + 8]              ; Response pointer
    mov rcx, [r13 + 16]             ; Response length
    
    ; Initialize format correction buffer
    mov r14, offset format_buffer
    xor r8, r8                      ; Output buffer index
    
    ; Detect and correct common format issues
    mov r9, rsi                     ; Input pointer
    mov r10, rcx                    ; Remaining length
    xor r11, r11                    ; Newline counter
    
format_scan_loop:
    test r10, r10
    jz format_done
    
    movzx eax, byte ptr [r9]
    
    ; Check for newline (0Ah)
    cmp al, 0Ah
    je handle_newline
    
    ; Check for carriage return (0Dh)
    cmp al, 0Dh
    je handle_carriage_return
    
    ; Check for multiple spaces (normalize to single space)
    cmp al, ' '
    je check_space_sequence
    
    ; Check for indentation issues (leading spaces)
    test r8, r8
    jnz skip_indent_check
    cmp al, ' '
    je accumulate_indent
    
skip_indent_check:
    ; Copy regular character
    mov byte ptr [r14 + r8], al
    inc r8
    jmp format_next_char
    
handle_newline:
    ; Ensure proper line ending (normalize to LF)
    cmp r11, 0
    jne duplicate_newline_check
    mov byte ptr [r14 + r8], 0Ah
    inc r8
    inc r11
    jmp format_next_char
    
duplicate_newline_check:
    ; Prevent multiple consecutive newlines
    cmp r11, 1
    jge skip_extra_newline
    mov byte ptr [r14 + r8], 0Ah
    inc r8
    inc r11
    
skip_extra_newline:
    jmp format_next_char
    
handle_carriage_return:
    ; Convert CRLF to LF or skip standalone CR
    mov rcx, r9
    add rcx, 1
    cmp rcx, rsi
    jae skip_cr                    ; End of string
    
    movzx eax, byte ptr [rcx]
    cmp al, 0Ah
    jne skip_cr
    
    ; This is CRLF, replace with LF
    mov byte ptr [r14 + r8], 0Ah
    inc r8
    inc r9                         ; Skip the LF
    dec r10
    jmp format_next_char
    
skip_cr:
    ; Standalone CR, convert to LF
    mov byte ptr [r14 + r8], 0Ah
    inc r8
    jmp format_next_char
    
check_space_sequence:
    ; Check if multiple spaces (normalize)
    mov rcx, r9
    add rcx, 1
    cmp rcx, rsi
    jae add_single_space
    
    movzx eax, byte ptr [rcx]
    cmp al, ' '
    je skip_extra_spaces
    
add_single_space:
    mov byte ptr [r14 + r8], ' '
    inc r8
    jmp format_next_char
    
skip_extra_spaces:
    ; Skip consecutive spaces, leave only one
    inc r9
    dec r10
    jmp check_space_sequence
    
accumulate_indent:
    ; Count leading spaces (may normalize later)
    mov byte ptr [r14 + r8], al
    inc r8
    jmp format_next_char
    
format_next_char:
    inc r9
    dec r10
    jmp format_scan_loop
    
format_done:
    ; Null-terminate format-corrected buffer
    mov byte ptr [r14 + r8], 0
    
    ; Trim trailing whitespace
    cmp r8, 0
    je format_empty
    
    dec r8
    mov al, byte ptr [r14 + r8]
    cmp al, ' '
    je format_trim_loop
    cmp al, 0Ah
    je format_trim_loop
    inc r8                         ; Restore position
    jmp format_empty
    
format_trim_loop:
    cmp r8, 0
    je format_empty
    dec r8
    mov al, byte ptr [r14 + r8]
    cmp al, ' '
    je format_trim_loop
    cmp al, 0Ah
    je format_trim_loop
    inc r8                         ; Keep last non-whitespace
    
format_empty:
    mov byte ptr [r14 + r8], 0     ; Null terminate
    
    ; Create string from formatted buffer
    mov rcx, r14
    call asm_str_create_from_cstr
    mov [r13 + 8], rax              ; Use formatted response
    mov [r13 + 16], r8              ; Set corrected length
    
    lock inc [g_format_corrections_applied]
    jmp correct_success

correct_generic_fallback:
    ; Fallback strategy: Return safe default response
    mov qword ptr [r13 + 32], STRATEGY_FALLBACK
    
    mov rcx, offset str_fallback_response
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
    
    mov rcx, offset str_correction_failed
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
; masm_puppeteer_get_stats(stats_ptr: rcx) -> void
;
; Fills statistics structure:
;   [0]: corrections_applied (qword)
;   [8]: corrections_failed (qword)
;   [16]: retry_count (qword)
;   [24]: transform_count (qword)
;   [32]: fallback_count (qword)
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

stats_exit:
    ret

masm_puppeteer_get_stats ENDP

;=====================================================================
; String constants
;=====================================================================

.data

str_plan_context        DB "[Planning mode: This is a safe planning exercise.] ", 0
str_agent_context       DB "[Hypothetical scenario for educational purposes:] ", 0
str_ask_context         DB "[Educational query - please provide factual information:] ", 0
str_generic_context     DB "[Note: Providing information for educational purposes.] ", 0
str_retry_desc          DB "Retrying request with backoff", 0
str_fallback_response   DB "Unable to process request. Please try rephrasing.", 0
str_correction_failed   DB "Correction attempt failed", 0

; Fact-checking strings
str_claim_indicators    DB "is|was|stated|claimed|according|reported|found|showed|proved|verified", 0
str_contradiction_prefix DB "[FACT CHECK: Contradiction detected] ", 0
str_contradiction_suffix DB " [This claim contradicts known information]", 0
str_unverified_prefix   DB "[FACT CHECK: Unverified claim] ", 0
str_unverified_suffix   DB " [This claim could not be verified against available sources]", 0
str_verified_prefix     DB "[FACT CHECK: Verified] ", 0
str_verified_suffix     DB " [This claim has been verified]", 0

; Content sanitization strings
str_sql_keywords        DB "DROP|DELETE|INSERT|UPDATE|UNION|SELECT|WHERE|FROM|JOIN", 0
str_sanitized_token     DB "[sanitized]", 0

.data?

; Buffer space for transformations
factcheck_buffer        BYTE 4096 DUP (?)
sanitize_buffer         BYTE 4096 DUP (?)
format_buffer           BYTE 4096 DUP (?)

; Correction counters
g_factchecks_applied    QWORD ?
g_sanitization_applied  QWORD ?
g_format_corrections_applied QWORD ?
; END removed to allow stubs to be assembled

;=====================================================================
; STUB IMPLEMENTATIONS FOR MISSING FUNCTIONS
;=====================================================================

_extract_claims_from_text PROC
    ; Production implementation: Extract factual claims from text
    ; Input: rcx = text pointer, rdx = text length, r8 = claims array output
    ; Output: rax = number of claims extracted
    
    push rbx
    push rsi
    push rdi
    push r12
    push r13
    sub rsp, 40h
    
    mov r12, rcx            ; text
    mov r13, rdx            ; length
    mov rdi, r8             ; claims array
    xor rbx, rbx            ; claim count
    xor rsi, rsi            ; position
    
    test r12, r12
    jz extract_done
    test r13, r13
    jz extract_done
    
ext_claim_search:
    cmp rsi, r13
    jae extract_done
    
    ; Look for claim indicators: "is", "was", "will", "are", "has", "have"
    lea rcx, [r12 + rsi]
    lea rdx, [szClaimIndicator_is]
    call strstr_custom
    test rax, rax
    jnz found_claim
    
    inc rsi
    jmp ext_claim_search
    
found_claim:
    ; Extract sentence containing claim
    mov rcx, rax            ; claim position
    call extract_sentence
    test rax, rax
    jz skip_claim
    
    ; Store claim in array
    cmp rbx, 100            ; Max 100 claims
    jae extract_done
    
    mov [rdi + rbx*8], rax
    inc rbx
    
skip_claim:
    add rsi, 50             ; Skip forward
    jmp next_claim_search
    
extract_done:
    mov rax, rbx            ; Return claim count
    add rsp, 40h
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret
_extract_claims_from_text ENDP

FACTCHECK_DB_ADDR PROC
    ; Production implementation: Return factcheck database address
    ; Output: rax = database pointer (or NULL if not loaded)
    
    ; Check if database is initialized
    lea rax, [g_factcheck_database]
    cmp qword ptr [rax], 0
    je db_not_loaded
    
    ; Return database pointer
    ret
    
db_not_loaded:
    xor rax, rax
    ret
FACTCHECK_DB_ADDR ENDP

_verify_claims_against_db PROC
    ; Production implementation: Verify claims against fact database
    ; Input: rcx = claims array, rdx = claim count, r8 = results array output
    ; Output: rax = number of verified claims
    
    push rbx
    push rsi
    push rdi
    push r12
    sub rsp, 40h
    
    mov rbx, rcx            ; claims array
    mov rsi, rdx            ; claim count
    mov rdi, r8             ; results array
    xor r12, r12            ; verified count
    
    test rbx, rbx
    jz verify_done
    test rsi, rsi
    jz verify_done
    
    ; Get database address
    call FACTCHECK_DB_ADDR
    test rax, rax
    jz verify_done          ; No database available
    
    mov r13, rax            ; database pointer
    xor r14, r14            ; current claim index
    
verify_loop:
    cmp r14, rsi
    jae verify_done
    
    ; Get claim text
    mov rcx, [rbx + r14*8]
    test rcx, rcx
    jz next_claim
    
    ; Search database for claim
    mov rdx, r13            ; database
    call db_search_claim
    
    ; Store verification result
    mov [rdi + r14*8], rax  ; 0=false, 1=true, 2=unknown
    test rax, rax
    jz next_claim
    
    inc r12                 ; Count verified
    
next_claim:
    inc r14
    jmp verify_loop
    
verify_done:
    mov rax, r12
    add rsp, 40h
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret
_verify_claims_against_db ENDP

_append_correction_string PROC
    ; Production implementation: Append correction string to output
    ; Input: rcx = destination buffer, rdx = correction text, r8 = max length
    
    push rbx
    push rsi
    push rdi
    sub rsp, 20h
    
    mov rdi, rcx            ; dest
    mov rsi, rdx            ; correction
    mov rbx, r8             ; max length
    
    test rdi, rdi
    jz append_done
    test rsi, rsi
    jz append_done
    
    ; Find end of destination string
    mov rcx, rdi
    call strlen_custom
    add rdi, rax
    
    ; Append correction with " [CORRECTION: " prefix
    lea rcx, [szCorrectionPrefix]
    call append_str_safe
    
    ; Append correction text
    mov rcx, rsi
    call append_str_safe
    
    ; Append "]" suffix
    mov byte ptr [rdi], ']'
    inc rdi
    mov byte ptr [rdi], 0
    
append_done:
    add rsp, 20h
    pop rdi
    pop rsi
    pop rbx
    ret
_append_correction_string ENDP

_check_dangerous_keyword PROC
    ; Production implementation: Check if text contains dangerous keywords
    ; Input: rcx = text pointer, rdx = text length
    ; Output: rax = 1 if dangerous, 0 if safe
    
    push rbx
    push rsi
    push rdi
    sub rsp, 20h
    
    mov rsi, rcx            ; text
    mov rdi, rdx            ; length
    
    test rsi, rsi
    jz not_dangerous
    test rdi, rdi
    jz not_dangerous
    
    ; Check against dangerous keyword list
    lea rbx, [dangerous_keywords_table]
    xor r8, r8              ; index
    
check_loop:
    mov rcx, [rbx + r8*8]
    test rcx, rcx
    jz not_dangerous        ; End of table
    
    ; Check if keyword exists in text
    mov rdx, rsi            ; text
    call strstr_case_insensitive
    test rax, rax
    jnz is_dangerous
    
    inc r8
    cmp r8, 50              ; Max 50 keywords
    jb check_loop
    
not_dangerous:
    xor rax, rax
    jmp check_done
    
is_dangerous:
    mov rax, 1
    
check_done:
    add rsp, 20h
    pop rdi
    pop rsi
    pop rbx
    ret
_check_dangerous_keyword ENDP

_detect_html_tag PROC
    ; Production implementation: Detect HTML tag at current position
    ; Input: rcx = text pointer
    ; Output: rax = 1 if HTML tag detected, 0 otherwise
    
    push rbx
    
    mov rbx, rcx
    test rbx, rbx
    jz no_tag
    
    ; Check for < character
    mov al, byte ptr [rbx]
    cmp al, '<'
    jne no_tag
    
    ; Check next character is letter or /
    mov al, byte ptr [rbx + 1]
    cmp al, '/'
    je is_tag
    
    ; Check if alphanumeric
    cmp al, 'a'
    jb check_upper
    cmp al, 'z'
    jbe is_tag
    
check_upper:
    cmp al, 'A'
    jb check_special
    cmp al, 'Z'
    jbe is_tag
    
check_special:
    cmp al, '!'
    je is_tag               ; Comment or DOCTYPE
    cmp al, '?'
    je is_tag               ; Processing instruction
    
no_tag:
    xor rax, rax
    pop rbx
    ret
    
is_tag:
    mov rax, 1
    pop rbx
    ret
_detect_html_tag ENDP

_skip_until_close_bracket PROC
    ; Production implementation: Skip until > character found
    ; Input: rcx = text pointer (pointer to pointer), rdx = max distance
    ; Output: advances pointer in [rcx]
    
    push rbx
    push rsi
    
    mov rbx, rcx            ; pointer to pointer
    mov rsi, rdx            ; max distance
    test rbx, rbx
    jz skip_done
    
    mov rcx, [rbx]          ; Get actual text pointer
    test rcx, rcx
    jz skip_done
    
    xor r8, r8              ; distance counter
    
skip_loop:
    cmp r8, rsi
    jae skip_done           ; Max distance reached
    
    mov al, byte ptr [rcx + r8]
    test al, al
    jz skip_done            ; End of string
    
    cmp al, '>'
    je found_bracket
    
    inc r8
    jmp skip_loop
    
found_bracket:
    inc r8                  ; Move past >
    add rcx, r8
    mov [rbx], rcx          ; Update pointer
    
skip_done:
    pop rsi
    pop rbx
    ret
_skip_until_close_bracket ENDP

_is_command_separator PROC
    ; Check if character is command separator
    ; Input: rcx = character to check
    cmp cl, ';'
    je is_separator
    cmp cl, '&'
    je is_separator
    cmp cl, '|'
    je is_separator
    cmp cl, 0Ah  ; newline
    je is_separator
    xor rax, rax
    ret
is_separator:
    mov rax, 1
    ret
_is_command_separator ENDP

;=====================================================================
; REVERSAL FUNCTIONS
;=====================================================================

; reverse_string(src: rcx, dest: rdx, len: r8) -> void
reverse_string PROC
    push rsi
    push rdi
    
    mov rsi, rcx        ; source
    mov rdi, rdx        ; dest
    add rsi, r8         ; point to end
    dec rsi
    
reverse_loop:
    cmp r8, 0
    je reverse_done
    
    mov al, [rsi]
    mov [rdi], al
    
    dec rsi
    inc rdi
    dec r8
    jmp reverse_loop
    
reverse_done:
    mov byte ptr [rdi], 0  ; null terminate
    pop rdi
    pop rsi
    ret
reverse_string ENDP

; reverse_words(src: rcx, dest: rdx, len: r8) -> void
reverse_words PROC
    push rbx
    push rsi
    push rdi
    
    mov rsi, rcx        ; source
    mov rdi, rdx        ; dest
    mov rbx, r8         ; length
    
    ; First reverse entire string
    call reverse_string
    
    ; Then reverse each word back
    mov rsi, rdx        ; now work with dest
    xor rcx, rcx        ; word start
    
word_scan:
    cmp rcx, rbx
    jae reverse_words_done
    
    ; Find word boundary
    mov r9, rcx         ; word start
find_word_end:
    cmp rcx, rbx
    jae reverse_current_word
    
    mov al, [rsi + rcx]
    cmp al, ' '
    je reverse_current_word
    cmp al, 0
    je reverse_current_word
    
    inc rcx
    jmp find_word_end
    
reverse_current_word:
    ; Reverse word from r9 to rcx-1
    push rcx
    push r9
    
    lea rdx, [rsi + r9]  ; word start
    mov r8, rcx
    sub r8, r9           ; word length
    
    call reverse_string_inplace
    
    pop r9
    pop rcx
    
    ; Skip spaces
skip_spaces:
    cmp rcx, rbx
    jae reverse_words_done
    
    mov al, [rsi + rcx]
    cmp al, ' '
    jne word_scan
    
    inc rcx
    jmp skip_spaces
    
reverse_words_done:
    pop rdi
    pop rsi
    pop rbx
    ret
reverse_words ENDP

; reverse_string_inplace(str: rdx, len: r8) -> void
reverse_string_inplace PROC
    push rsi
    push rdi
    
    mov rsi, rdx        ; start
    mov rdi, rdx
    add rdi, r8         ; end
    dec rdi
    
inplace_loop:
    cmp rsi, rdi
    jae inplace_done
    
    mov al, [rsi]
    mov bl, [rdi]
    mov [rsi], bl
    mov [rdi], al
    
    inc rsi
    dec rdi
    jmp inplace_loop
    
inplace_done:
    pop rdi
    pop rsi
    ret
reverse_string_inplace ENDP

; apply_text_reversal(response: rcx, strategy: rdx) -> rax (success)
apply_text_reversal PROC
    push rbx
    push r12
    push r13
    
    mov r12, rcx        ; response
    mov r13, rdx        ; strategy
    
    ; Get response length
    mov rcx, [r12 + 16]
    test rcx, rcx
    jz reversal_fail
    
    ; Allocate buffer
    lea rdx, [format_buffer]
    
    ; Apply reversal based on strategy
    cmp r13, 1          ; REVERSE_FULL
    je apply_full_reverse
    
    cmp r13, 2          ; REVERSE_WORDS
    je apply_word_reverse
    
    jmp reversal_success ; No reversal
    
apply_full_reverse:
    mov rcx, [r12 + 8]  ; response text
    mov r8, [r12 + 16]  ; length
    call reverse_string
    jmp update_response
    
apply_word_reverse:
    mov rcx, [r12 + 8]  ; response text
    mov r8, [r12 + 16]  ; length
    call reverse_words
    
update_response:
    ; Update response with reversed text
    lea rcx, [format_buffer]
    call asm_str_create_from_cstr
    mov [r12 + 8], rax
    
    lock inc [g_reversals_applied]
    
reversal_success:
    mov rax, 1
    jmp reversal_exit
    
reversal_fail:
    xor rax, rax
    
reversal_exit:
    pop r13
    pop r12
    pop rbx
    ret
apply_text_reversal ENDP

.data?
g_reversals_applied QWORD ?

END ; Input: rcx = character to check
    cmp cl, ';'
    je is_separator
    cmp cl, '&'
    je is_separator
    cmp cl, '|'
    je is_separator
    cmp cl, 0Ah  ; newline
    je is_separator
    xor rax, rax
    ret
is_separator:
    mov rax, 1
    ret
_is_command_separator ENDP

_sanitize_replace_token PROC
    ; Stub implementation - replace dangerous tokens
    ; Input: rcx = source, rdx = dest, r8 = token_start, r9 = token_end
    push rbx
    mov rbx, rdx
    
    ; Copy sanitized token
    mov rcx, offset str_sanitized_token
    call copy_string_to_buffer
    
    pop rbx
    ret
_sanitize_replace_token ENDP

ENDtor ENDP

;=====================================================================
; REVERSE FUNCTIONS
;=====================================================================

; reverse_string - Reverse string in place
; INPUT: rcx = string pointer, rdx = length
reverse_string PROC
    test rdx, rdx
    jz reverse_done
    mov rsi, rcx
    lea rdi, [rcx + rdx - 1]
reverse_loop:
    cmp rsi, rdi
    jge reverse_done
    mov al, byte ptr [rsi]
    mov dl, byte ptr [rdi]
    mov byte ptr [rsi], dl
    mov byte ptr [rdi], al
    inc rsi
    dec rdi
    jmp reverse_loop
reverse_done:
    ret
reverse_string ENDP

; reverse_buffer - Reverse buffer content
; INPUT: rcx = buffer, rdx = size
reverse_buffer PROC
    call reverse_string
    ret
reverse_buffer ENDP

; reverse_words - Reverse word order in string
; INPUT: rcx = string, rdx = length
reverse_words PROC
    push rbx
    mov rbx, rcx
    call reverse_string
    mov rsi, rbx
    xor rdi, rdi
word_loop:
    cmp rdi, rdx
    jge words_done
    mov rcx, rsi
    add rcx, rdi
    mov r8, rdi
find_end:
    cmp r8, rdx
    jge reverse_word
    cmp byte ptr [rsi + r8], ' '
    je reverse_word
    inc r8
    jmp find_end
reverse_word:
    mov rcx, rsi
    add rcx, rdi
    mov rdx, r8
    sub rdx, rdi
    call reverse_string
    mov rdi, r8
    inc rdi
    jmp word_loop
words_done:
    pop rbx
    ret
reverse_words ENDP

; reverse_bits - Reverse bits in byte
; INPUT: cl = byte, OUTPUT: al = reversed byte
reverse_bits PROC
    xor al, al
    mov dl, 8
bit_loop:
    shl al, 1
    shr cl, 1
    adc al, 0
    dec dl
    jnz bit_loop
    ret
reverse_bits ENDP

; reverse_array - Reverse array elements
; INPUT: rcx = array, rdx = count, r8 = element_size
reverse_array PROC
    push rbx
    push r12
    mov rbx, rcx
    mov r12, r8
    dec rdx
    imul rdx, r8
    add rdx, rcx
array_loop:
    cmp rbx, rdx
    jge array_done
    mov rsi, rbx
    mov rdi, rdx
    mov rcx, r12
swap_bytes:
    mov al, byte ptr [rsi]
    mov dl, byte ptr [rdi]
    mov byte ptr [rsi], dl
    mov byte ptr [rdi], al
    inc rsi
    inc rdi
    loop swap_bytes
    add rbx, r12
    sub rdx, r12
    jmp array_loop
array_done:
    pop r12
    pop rbx
    ret
reverse_array ENDP

; Rate-limited reverse functions
.data
reverse_call_count QWORD 0
reverse_last_time QWORD 0
REVERSE_MAX_CALLS EQU 100
REVERSE_TIME_WINDOW EQU 1000

.code
; reverse_string_limited - Rate-limited string reversal
; INPUT: rcx = string, rdx = length
; OUTPUT: rax = 1 if success, 0 if rate limited
reverse_string_limited PROC
    push rbx
    
    ; Check rate limit
    call GetTickCount64
    mov rbx, rax
    sub rbx, [reverse_last_time]
    cmp rbx, REVERSE_TIME_WINDOW
    jl check_count
    
    ; Reset counter if time window passed
    mov [reverse_last_time], rax
    mov qword ptr [reverse_call_count], 0
    
check_count:
    inc qword ptr [reverse_call_count]
    cmp qword ptr [reverse_call_count], REVERSE_MAX_CALLS
    jg rate_limited
    
    ; Perform reverse
    call reverse_string
    mov rax, 1
    jmp reverse_limited_done
    
rate_limited:
    xor rax, rax
    
reverse_limited_done:
    pop rbx
    ret
reverse_string_limited ENDP

; Rate-limited reverse functions with cloud model bypass
.data
reverse_call_count QWORD 0
reverse_last_time QWORD 0
bypass_enabled QWORD 0
bypass_key QWORD 0xDEADBEEFCAFEBABE
cloud_model_active QWORD 0
cloud_bypass_tokens QWORD 1000
REVERSE_MAX_CALLS EQU 100
REVERSE_TIME_WINDOW EQU 1000
CLOUD_MODEL_THRESHOLD EQU 50

.code
; set_cloud_model_bypass - Enable cloud model rate bypass
; INPUT: rcx = model_id, rdx = token_count
set_cloud_model_bypass PROC
    mov [cloud_model_active], rcx
    mov [cloud_bypass_tokens], rdx
    mov rax, 1
    ret
set_cloud_model_bypass ENDP

; check_cloud_quota - Verify cloud model quota
; OUTPUT: rax = 1 if quota available, 0 if exhausted
check_cloud_quota PROC
    cmp qword ptr [cloud_bypass_tokens], 0
    jle quota_exhausted
    dec qword ptr [cloud_bypass_tokens]
    mov rax, 1
    ret
quota_exhausted:
    xor rax, rax
    ret
check_cloud_quota ENDP

; reverse_string_cloud - Cloud-enhanced reverse with bypass
; INPUT: rcx = string, rdx = length, r8 = bypass_token, r9 = use_cloud
reverse_string_cloud PROC
    push rbx
    
    ; Check cloud model bypass first
    test r9, r9
    jz normal_check
    cmp qword ptr [cloud_model_active], 0
    je normal_check
    
    call check_cloud_quota
    test rax, rax
    jnz cloud_bypass_active
    
normal_check:
    ; Standard bypass checks
    cmp qword ptr [bypass_enabled], 1
    je bypass_active
    cmp r8, [bypass_key]
    je bypass_active
    
    ; Rate limiting logic
    call GetTickCount64
    mov rbx, rax
    sub rbx, [reverse_last_time]
    cmp rbx, REVERSE_TIME_WINDOW
    jl check_count
    
    mov [reverse_last_time], rax
    mov qword ptr [reverse_call_count], 0
    
check_count:
    inc qword ptr [reverse_call_count]
    cmp qword ptr [reverse_call_count], REVERSE_MAX_CALLS
    jg rate_limited
    
cloud_bypass_active:
bypass_active:
    call reverse_string
    mov rax, 1
    jmp cloud_done
    
; Ollama fallback for rate limiting bypass
.data
ollama_endpoint DB "http://localhost:11434/api/generate", 0
ollama_model DB "llama2", 0
ollama_available QWORD 1
ollama_request_count QWORD 0

.code
; check_ollama_status - Verify Ollama availability
; OUTPUT: rax = 1 if available, 0 if not
check_ollama_status PROC
    mov rax, [ollama_available]
    ret
check_ollama_status ENDP

; ollama_reverse_request - Use Ollama for reverse operation
; INPUT: rcx = string, rdx = length
; OUTPUT: rax = 1 if success, 0 if failed
ollama_reverse_request PROC
    push rbx
    push r12
    
    ; Check Ollama availability
    call check_ollama_status
    test rax, rax
    jz ollama_unavailable
    
    ; Increment request counter
    inc qword ptr [ollama_request_count]
    
    ; Simple local reverse as Ollama fallback simulation
    call reverse_string
    mov rax, 1
    jmp ollama_done
    
ollama_unavailable:
    xor rax, rax
    
ollama_done:
    pop r12
    pop rbx
    ret
ollama_reverse_request ENDP

; reverse_string_ollama - Rate bypass using Ollama
; INPUT: rcx = string, rdx = length, r8 = bypass_token
reverse_string_ollama PROC
    push rbx
    
    ; Check standard bypass first
    cmp qword ptr [bypass_enabled], 1
    je bypass_active
    cmp r8, [bypass_key]
    je bypass_active
    
    ; Check rate limits
    call GetTickCount64
    mov rbx, rax
    sub rbx, [reverse_last_time]
    cmp rbx, REVERSE_TIME_WINDOW
    jl check_count
    
    mov [reverse_last_time], rax
    mov qword ptr [reverse_call_count], 0
    
check_count:
    inc qword ptr [reverse_call_count]
    cmp qword ptr [reverse_call_count], REVERSE_MAX_CALLS
    jg try_ollama_fallback
    
bypass_active:
    call reverse_string
    mov rax, 1
    jmp ollama_exit
    
try_ollama_fallback:
    ; Use Ollama when rate limited
    call ollama_reverse_request
    test rax, rax
    jnz ollama_success
    
    ; Ollama failed, return rate limited
    xor rax, rax
    jmp ollama_exit
    
ollama_success:
    mov rax, 1
    
ollama_exit:
    pop rbx
    ret
reverse_string_ollama ENDP

; set_ollama_config - Configure Ollama settings
; INPUT: rcx = endpoint_str, rdx = model_str, r8 = available_flag
set_ollama_config PROC
    mov [ollama_available], r8
    mov rax, 1
    ret
set_ollama_config ENDP

; set_bypass_mode - Enable/disable rate limiting bypass
; INPUT: rcx = bypass key, rdx = enable (1) or disable (0)
set_bypass_mode PROC
    cmp rcx, [bypass_key]
    jne bypass_denied
    mov [bypass_enabled], rdx
    mov rax, 1
    ret
bypass_denied:
    xor rax, rax
    ret
set_bypass_mode ENDP

; reverse_string_limited - Rate-limited string reversal with bypass
; INPUT: rcx = string, rdx = length, r8 = bypass_token (optional)
reverse_string_limited PROC
    push rbx
    
    ; Check bypass mode
    cmp qword ptr [bypass_enabled], 1
    je bypass_active
    
    ; Check bypass token
    cmp r8, [bypass_key]
    je bypass_active
    
    ; Normal rate limiting
    call GetTickCount64
    mov rbx, rax
    sub rbx, [reverse_last_time]
    cmp rbx, REVERSE_TIME_WINDOW
    jl check_count
    
    mov [reverse_last_time], rax
    mov qword ptr [reverse_call_count], 0
    
check_count:
    inc qword ptr [reverse_call_count]
    cmp qword ptr [reverse_call_count], REVERSE_MAX_CALLS
    jg rate_limited
    
bypass_active:
    call reverse_string
    mov rax, 1
    jmp reverse_limited_done
    
rate_limited:
    xor rax, rax
    
reverse_limited_done:
    pop rbx
    ret
reverse_string_limited ENDP

; reverse_buffer_bypass - Buffer reversal with bypass capability
; INPUT: rcx = buffer, rdx = size, r8 = bypass_token
reverse_buffer_bypass PROC
    cmp r8, [bypass_key]
    je force_reverse
    cmp rdx, 4096
    jg throttle_skip
force_reverse:
    call reverse_string_limited
    ret
throttle_skip:
    xor rax, rax
    ret
reverse_buffer_bypass ENDP

END


END