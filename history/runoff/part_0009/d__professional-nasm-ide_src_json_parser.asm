; ============================================================================
; json_parser.asm - Minimal JSON parsing utilities for Ollama ASM wrapper
; Platform: x64 (Windows/Linux) - NASM syntax
; ----------------------------------------------------------------------------
; Provided functions (stable minimal API):
;   json_find_key(buffer_ptr, length, key_ptr) -> RAX = value_start (after opening quote) or 0
;       RDI = buffer_ptr
;       RSI = length
;       RDX = key_ptr (null-terminated, e.g. '"response":')
;       Returns pointer positioned at first character of value (if string value) or 0.
;       Only supports pattern: "key":"value" (string) or "key":value (non-string).
;   json_extract_string_array(buffer_ptr, length, key_ptr, dest_ptr, max_items) -> RAX = count
;       Extracts string elements from an array value associated with key.
;       Pattern: "key":["str1","str2",...]
;       Writes each string into dest buffer sequentially null-terminated.
;       Caller must ensure adequate space (approx sum(lengths)+count).
;       Returns number of strings copied.
;   json_parse_response(buffer_ptr, length) -> RAX = response_ptr, RDX = response_len, R8 = done_flag, R9 = context_ptr, R10 = context_len, R11 = error_code
;       Parses top-level response fields: 'response' (string), 'done' (bool), 'context' (array of ints).
;       Fails gracefully, zeroing outputs on error.
; ----------------------------------------------------------------------------
; Limitations:
;   - No full JSON validation.
;   - Does not handle escaped quotes inside strings (basic responses assumed).
;   - Ignores nested objects except for scanning.
;   - Sufficient for Phase 1 requirements (model listing + response extraction).
; ============================================================================

BITS 64
DEFAULT REL

section .text

global json_find_key
json_find_key:
    ; RDI=buffer, RSI=len, RDX=key
    push rbp
    mov rbp, rsp
    push rbx
    push r12
    push r13
    push r14
    mov r12, rdi        ; buffer base
    mov r13, rsi        ; length
    mov r14, rdx        ; key ptr
    xor rbx, rbx        ; index

.scan_loop:
    cmp rbx, r13
    jge .not_found
    mov al, [r12 + rbx]
    cmp al, '"'
    jne .next_byte
    ; potential key start - compare key
    mov rdi, r14        ; key
    lea rsi, [r12 + rbx]
    xor rcx, rcx
.cmp_loop:
    mov dl, [rdi]
    cmp dl, 0
    je .key_match
    mov al, [rsi]
    cmp al, dl
    jne .next_byte
    inc rdi
    inc rsi
    inc rcx
    jmp .cmp_loop
.key_match:
    ; Expect following after key: either '"' or value start after ':' already included in key pattern
    ; Move rsi to after key pattern
    mov rax, rsi
    ; Skip optional quotes or colon already part of key string definition
    ; Find first ':' after key end if not included
    mov dl, ':'
    mov rdx, rax
.seek_colon:
    mov al, [rdx]
    cmp al, 0
    je .not_found
    cmp al, ':'
    je .after_colon
    inc rdx
    jmp .seek_colon
.after_colon:
    inc rdx
    ; Skip whitespace
.skip_ws:
    mov al, [rdx]
    cmp al, ' '
    je .ws_advance
    cmp al, 9
    je .ws_advance
    cmp al, 13
    je .ws_advance
    cmp al, 10
    je .ws_advance
    jmp .value_start
.ws_advance:
    inc rdx
    jmp .skip_ws
.value_start:
    mov al, [rdx]
    cmp al, '"'
    jne .non_string
    inc rdx                ; point to first char inside string
    mov rax, rdx
    jmp .done
.non_string:
    ; Return pointer to start of non-string value (number, true, false, object, array)
    mov rax, rdx
    jmp .done

.next_byte:
    inc rbx
    jmp .scan_loop

.not_found:
    xor rax, rax
.done:
    pop r14
    pop r13
    pop r12
    pop rbx
    pop rbp
    ret

global json_extract_string_array
json_extract_string_array:
    ; RDI=buffer, RSI=len, RDX=key_ptr, RCX=dest_ptr, R8=max_items
    push rbp
    mov rbp, rsp
    push rbx
    push r12
    push r13
    push r14
    push r15
    mov r12, rdi        ; buffer
    mov r13, rsi        ; len
    mov r14, rdx        ; key
    mov r15, rcx        ; dest
    mov rbx, r8         ; max_items
    xor rax, rax        ; count
    xor rdx, rdx        ; scan index

.find_key_loop:
    cmp rdx, r13
    jge .done
    mov al, [r12 + rdx]
    cmp al, '"'
    jne .key_next
    ; compare key
    mov rsi, r14
    lea rdi, [r12 + rdx]
.ek_cmp_loop:
    mov cl, [rsi]
    cmp cl, 0
    je .key_found
    mov bl, [rdi]
    cmp bl, cl
    jne .key_next
    inc rsi
    inc rdi
    jmp .ek_cmp_loop
.key_found:
    ; seek '[' - search for the start of the array value after key match
.seek_lbracket:
    mov cl, [rdi]
    cmp cl, 0
    je .done
    cmp cl, '['
    je .in_array
    inc rdi
    jmp .seek_lbracket
.in_array:
    inc rdi ; after '['
.array_loop:
    cmp rax, rbx
    jge .done
    mov cl, [rdi]
    cmp cl, ']'
    je .done
    cmp cl, '"'
    jne .advance_elem
    inc rdi
    mov rsi, r15        ; write position
    ; copy string until closing quote or end
.copy_loop:
    mov cl, [rdi]
    cmp cl, '"'
    je .end_string
    cmp cl, 0
    je .end_string
    mov [rsi], cl
    inc rsi
    inc rdi
    jmp .copy_loop
.end_string:
    mov byte [rsi], 0
    inc rdi             ; skip closing quote
    ; advance dest pointer by written length
    mov r15, rsi
    inc rax             ; increment count
    ; skip optional comma
    mov cl, [rdi]
    cmp cl, ','
    jne .array_loop
    inc rdi
    jmp .array_loop
.advance_elem:
    inc rdi
    jmp .array_loop
    
.key_next:
    inc rdx
    jmp .find_key_loop

.done:
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    pop rbp
    ret

global json_parse_response
json_parse_response:
    ; RDI=buffer, RSI=len
    ; RAX=response_ptr, RDX=response_len, R8=done_flag, R9=context_ptr, R10=context_len, R11:error_code
    push rbp
    mov rbp, rsp
    push rbx
    push r12
    push r13
    push r14
    push r15
    mov r12, rdi        ; buffer
    mov r13, rsi        ; length
    xor r11, r11        ; error_code = 0
    ; Find "response" key
    lea rdx, [rel key_response]
    mov rcx, 10         ; length of '"response":'
    mov rdi, r12
    mov rsi, r13
    call json_find_key
    test rax, rax
    jz .fail_response
    mov r14, rax        ; response_ptr
    ; Find end of string
    mov rbx, r14
    xor rdx, rdx
.find_resp_end:
    mov al, [rbx + rdx]
    cmp al, '"'
    je .resp_end_found
    cmp al, 0
    je .fail_response
    inc rdx
    jmp .find_resp_end
.resp_end_found:
    mov rax, r14        ; response_ptr
    mov rdx, rdx        ; response_len
    ; Find "done" key
    lea rdx, [rel key_done]
    mov rcx, 7          ; length of '"done":'
    mov rdi, r12
    mov rsi, r13
    call json_find_key
    test rax, rax
    jz .fail_done
    mov al, [rax]
    cmp al, 't'         ; true
    jne .done_false
    mov r8, 1
    jmp .find_context
.done_false:
    mov r8, 0
    jmp .find_context
.fail_done:
    xor r8, r8
    jmp .find_context
.find_context:
    lea rdx, [rel key_context]
    mov rcx, 10         ; length of '"context":'
    mov rdi, r12
    mov rsi, r13
    call json_find_key
    test rax, rax
    jz .no_context
    ; rax points to '['
    mov r9, rax
    mov r10, 0
    mov rbx, r9
    inc rbx
.scan_ctx:
    mov al, [rbx]
    cmp al, ']'
    je .ctx_done
    cmp al, 0
    je .ctx_done
    cmp al, ','
    je .ctx_next
    ; parse int
    mov rcx, 0
    mov rsi, rbx
.parse_int:
    mov dl, [rsi]
    cmp dl, '0'
    jb .ctx_next
    cmp dl, '9'
    ja .ctx_next
    imul rcx, rcx, 10
    sub dl, '0'
    add rcx, rdx
    inc rsi
    jmp .parse_int
.ctx_next:
    mov [r9 + r10*4], ecx
    inc r10
    inc rbx
    jmp .scan_ctx
.ctx_done:
    jmp .done
.no_context:
    xor r9, r9
    xor r10, r10
    jmp .done
.fail_response:
    xor rax, rax
    xor rdx, rdx
    mov r11, 1
    jmp .done
.done:
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    pop rbp
    ret

section .rodata
key_response: db '"response":',0
key_done:     db '"done":',0
key_context:  db '"context":',0

; End of json_parser.asm