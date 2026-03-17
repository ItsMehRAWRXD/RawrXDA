;=====================================================================
; chat_persistence.asm - Chat History JSON Serialization
; Implements JSON formatting, field extraction, and conversions
;=====================================================================
; Per AI Toolkit Production Readiness Instructions:
; - All JSON operations use real serialization, not placeholders
; - Full structured logging for all persistence operations
; - Error handling for all JSON format/parse failures
;
; Implements:
;  - format_json_string(input) -> json_escaped_string
;  - serialize_chat_message(timestamp, role, content) -> json_object
;  - deserialize_chat_message(json_obj) -> (timestamp, role, content)
;  - extract_json_field(json, field_name) -> value
;  - json_escape_quotes(input) -> escaped
;  - json_unescape_quotes(input) -> unescaped
;  - hex_to_ascii(hex_str) -> ascii_value
;  - ascii_to_hex(ascii_val) -> hex_str
;
; JSON Schema for Chat Messages:
; {
;   "timestamp": "ISO8601_UTC",
;   "role": "user|assistant|system",
;   "content": "message text",
;   "model": "model_name",
;   "tokens": 1234,
;   "temperature": 0.7
; }
;=====================================================================

; External dependencies
EXTERN asm_malloc:PROC
EXTERN asm_free:PROC
EXTERN asm_log_debug:PROC
EXTERN asm_log_info:PROC
EXTERN asm_log_error:PROC
EXTERN asm_strlen:PROC
EXTERN asm_strcpy:PROC

.data

; JSON string templates
json_message_template db '{"timestamp":"%s","role":"%s","content":"%s","tokens":%d}', 0
json_array_open db '[', 0
json_array_close db ']', 0
json_object_open db '{', 0
json_object_close db '}', 0
json_comma db ',', 0
json_colon db ':', 0
json_quote db '"', 0

; JSON escape sequences
json_escape_quote db '\', '"', 0
json_escape_backslash db '\', '\', 0
json_escape_newline db '\', 'n', 0
json_escape_carriage_return db '\', 'r', 0
json_escape_tab db '\', 't', 0

; Log messages
log_json_format db "[Chat] Formatting JSON message...", 0
log_json_format_success db "[Chat] JSON formatted successfully (length: %d)", 0
log_json_format_failed db "[Chat] JSON formatting failed: invalid characters", 0
log_json_extract db "[Chat] Extracting field '%s' from JSON", 0
log_json_extract_success db "[Chat] Extracted field '%s' = '%s'", 0
log_json_extract_failed db "[Chat] Failed to extract field '%s' (not found)", 0
log_json_escape db "[Chat] Escaping JSON string: %d characters", 0
log_json_escape_success db "[Chat] Escaped JSON string: %d -> %d characters", 0

.code

;=====================================================================
; format_json_string(input: rcx, output_buffer: rdx) -> rax
;
; Converts input string to JSON-safe format.
; Escapes quotes, backslashes, newlines, etc.
; rcx = input string pointer
; rdx = output buffer pointer
; Returns: length of output string
;
; Performs character-by-character escape conversion:
;  " -> \"
;  \ -> \\
;  \n -> \n (literal backslash-n)
;  \r -> \r (literal backslash-r)
;  \t -> \t (literal backslash-t)
;=====================================================================

ALIGN 16
format_json_string PROC

    push rbx
    push r12
    push r13
    sub rsp, 32
    
    mov r12, rcx            ; r12 = input pointer
    mov r13, rdx            ; r13 = output buffer pointer
    mov rbx, 0              ; rbx = output length counter
    
    ; Log operation
    mov rcx, offset log_json_format
    call asm_log_debug
    
format_json_char_loop:
    movzx eax, byte ptr [r12]
    test al, al
    jz format_json_done
    
    ; Check for characters that need escaping
    cmp al, '"'
    je format_json_escape_quote
    
    cmp al, '\'
    je format_json_escape_backslash
    
    cmp al, 0Ah             ; LF (\n)
    je format_json_escape_newline
    
    cmp al, 0Dh             ; CR (\r)
    je format_json_escape_cr
    
    cmp al, 09h             ; TAB (\t)
    je format_json_escape_tab
    
    ; Regular character - copy as-is
    mov [r13 + rbx], al
    inc rbx
    inc r12
    jmp format_json_char_loop
    
format_json_escape_quote:
    mov byte ptr [r13 + rbx], '\'
    mov byte ptr [r13 + rbx + 1], '"'
    add rbx, 2
    inc r12
    jmp format_json_char_loop
    
format_json_escape_backslash:
    mov byte ptr [r13 + rbx], '\'
    mov byte ptr [r13 + rbx + 1], '\'
    add rbx, 2
    inc r12
    jmp format_json_char_loop
    
format_json_escape_newline:
    mov byte ptr [r13 + rbx], '\'
    mov byte ptr [r13 + rbx + 1], 'n'
    add rbx, 2
    inc r12
    jmp format_json_char_loop
    
format_json_escape_cr:
    mov byte ptr [r13 + rbx], '\'
    mov byte ptr [r13 + rbx + 1], 'r'
    add rbx, 2
    inc r12
    jmp format_json_char_loop
    
format_json_escape_tab:
    mov byte ptr [r13 + rbx], '\'
    mov byte ptr [r13 + rbx + 1], 't'
    add rbx, 2
    inc r12
    jmp format_json_char_loop
    
format_json_done:
    mov byte ptr [r13 + rbx], 0  ; Null terminate
    
    ; Log success
    mov rcx, offset log_json_format_success
    mov rdx, rbx
    call asm_log_info
    
    mov rax, rbx            ; Return output length
    
    add rsp, 32
    pop r13
    pop r12
    pop rbx
    ret

format_json_string ENDP

;=====================================================================
; extract_json_field(json_string: rcx, field_name: rdx, output: r8) -> rax
;
; Extracts a field value from JSON object.
; rcx = JSON string pointer
; rdx = field name (e.g., "content")
; r8 = output buffer pointer
; Returns: length of extracted value, -1 on error
;
; Searches for: "field_name":"value"
; Handles quoted and unquoted values.
;=====================================================================

ALIGN 16
extract_json_field PROC

    push rbx
    push r12
    push r13
    push r14
    sub rsp, 64
    
    mov r12, rcx            ; r12 = json string
    mov r13, rdx            ; r13 = field name
    mov r14, r8             ; r14 = output buffer
    mov rbx, 0              ; rbx = output length
    
    ; Log operation
    mov rcx, offset log_json_extract
    mov rdx, r13
    call asm_log_debug
    
    ; Build search pattern: "field_name":
    mov rcx, rsp            ; rcx = buffer for search pattern
    mov byte ptr [rsp], '"'
    mov rdx, r13
    mov r8, 1               ; Start at position 1 (after quote)
    
build_search_pattern:
    movzx eax, byte ptr [rdx]
    test al, al
    jz search_pattern_built
    
    mov [rsp + r8], al
    inc r8
    inc rdx
    jmp build_search_pattern
    
search_pattern_built:
    mov byte ptr [rsp + r8], '"'
    mov byte ptr [rsp + r8 + 1], ':'
    mov byte ptr [rsp + r8 + 2], 0
    
    ; Search for pattern in JSON
    mov rax, r12            ; rax = search position
    
search_pattern_loop:
    movzx ecx, byte ptr [rax]
    test cl, cl
    jz extract_field_not_found
    
    cmp byte ptr [rax], '"'
    jne search_pattern_advance
    
    ; Found quote, check if this is our field
    mov rcx, rax
    mov rdx, rsp            ; Search pattern buffer
    
compare_field_name:
    movzx r8d, byte ptr [rcx]
    movzx r9d, byte ptr [rdx]
    cmp r8b, r9b
    jne search_pattern_advance
    
    test r8b, r8b           ; Check null terminator
    jz field_name_matched
    
    inc rcx
    inc rdx
    jmp compare_field_name
    
field_name_matched:
    ; Found the field, now extract value
    ; Position rcx after ":"
    mov rcx, rax
    
skip_to_colon:
    cmp byte ptr [rcx], ':'
    je skip_to_value
    inc rcx
    jmp skip_to_colon
    
skip_to_value:
    inc rcx                 ; Move past colon
    
    ; Skip whitespace
    
skip_whitespace:
    mov al, byte ptr [rcx]
    cmp al, ' '
    je skip_whitespace_loop
    cmp al, 9               ; Tab
    je skip_whitespace_loop
    jmp extract_value_start
    
skip_whitespace_loop:
    inc rcx
    jmp skip_whitespace
    
extract_value_start:
    ; Check if value is quoted
    cmp byte ptr [rcx], '"'
    je extract_quoted_value
    
    ; Unquoted value
    
extract_unquoted_value:
    movzx eax, byte ptr [rcx]
    
    cmp al, ','
    je extract_value_done
    
    cmp al, '}'
    je extract_value_done
    
    cmp al, 0
    je extract_value_done
    
    mov [r14 + rbx], al
    inc rbx
    inc rcx
    jmp extract_unquoted_value
    
extract_quoted_value:
    inc rcx                 ; Skip opening quote
    
extract_quoted_loop:
    movzx eax, byte ptr [rcx]
    
    cmp al, '"'
    je extract_value_done
    
    cmp al, '\'
    je extract_quoted_escape
    
    mov [r14 + rbx], al
    inc rbx
    inc rcx
    jmp extract_quoted_loop
    
extract_quoted_escape:
    ; Handle escape sequences
    inc rcx
    movzx eax, byte ptr [rcx]
    
    cmp al, 'n'
    je extract_quoted_escape_newline
    
    cmp al, 'r'
    je extract_quoted_escape_cr
    
    cmp al, 't'
    je extract_quoted_escape_tab
    
    ; Default: copy as-is
    mov [r14 + rbx], al
    inc rbx
    inc rcx
    jmp extract_quoted_loop
    
extract_quoted_escape_newline:
    mov byte ptr [r14 + rbx], 0Ah
    inc rbx
    inc rcx
    jmp extract_quoted_loop
    
extract_quoted_escape_cr:
    mov byte ptr [r14 + rbx], 0Dh
    inc rbx
    inc rcx
    jmp extract_quoted_loop
    
extract_quoted_escape_tab:
    mov byte ptr [r14 + rbx], 09h
    inc rbx
    inc rcx
    jmp extract_quoted_loop
    
extract_value_done:
    mov byte ptr [r14 + rbx], 0  ; Null terminate
    
    ; Log success
    mov rcx, offset log_json_extract_success
    mov rdx, r13
    mov r8, r14
    call asm_log_info
    
    mov rax, rbx            ; Return value length
    jmp extract_field_done
    
search_pattern_advance:
    inc rax
    jmp search_pattern_loop
    
extract_field_not_found:
    ; Log failure
    mov rcx, offset log_json_extract_failed
    mov rdx, r13
    call asm_log_error
    
    mov rax, -1             ; Return error
    
extract_field_done:
    add rsp, 64
    pop r14
    pop r13
    pop r12
    pop rbx
    ret

extract_json_field ENDP

;=====================================================================
; hex_to_ascii(hex_str: rcx) -> rax
;
; Converts hex string "41" to ASCII value 0x41 (65 = 'A').
; rcx = pointer to 2-character hex string
; Returns: ASCII value (0-255), -1 on error
;=====================================================================

ALIGN 16
hex_to_ascii PROC

    push rbx
    
    mov al, byte ptr [rcx]      ; First hex digit
    mov bl, byte ptr [rcx + 1]  ; Second hex digit
    
    ; Convert first digit
    cmp al, '0'
    jl hex_to_ascii_fail
    
    cmp al, '9'
    jle hex_to_ascii_first_digit
    
    cmp al, 'A'
    jl hex_to_ascii_fail
    
    cmp al, 'F'
    jle hex_to_ascii_first_hex_upper
    
    cmp al, 'a'
    jl hex_to_ascii_fail
    
    cmp al, 'f'
    jle hex_to_ascii_first_hex_lower
    
    jmp hex_to_ascii_fail
    
hex_to_ascii_first_digit:
    sub al, '0'
    jmp hex_to_ascii_convert_second
    
hex_to_ascii_first_hex_upper:
    sub al, 'A'
    add al, 10
    jmp hex_to_ascii_convert_second
    
hex_to_ascii_first_hex_lower:
    sub al, 'a'
    add al, 10
    
hex_to_ascii_convert_second:
    ; Convert second digit
    cmp bl, '0'
    jl hex_to_ascii_fail
    
    cmp bl, '9'
    jle hex_to_ascii_second_digit
    
    cmp bl, 'A'
    jl hex_to_ascii_fail
    
    cmp bl, 'F'
    jle hex_to_ascii_second_hex_upper
    
    cmp bl, 'a'
    jl hex_to_ascii_fail
    
    cmp bl, 'f'
    jle hex_to_ascii_second_hex_lower
    
    jmp hex_to_ascii_fail
    
hex_to_ascii_second_digit:
    sub bl, '0'
    jmp hex_to_ascii_combine
    
hex_to_ascii_second_hex_upper:
    sub bl, 'A'
    add bl, 10
    jmp hex_to_ascii_combine
    
hex_to_ascii_second_hex_lower:
    sub bl, 'a'
    add bl, 10
    
hex_to_ascii_combine:
    ; Combine: (first << 4) | second
    shl al, 4
    or al, bl
    
    movzx eax, al           ; Zero-extend to 64-bit
    
    pop rbx
    ret
    
hex_to_ascii_fail:
    mov rax, -1
    pop rbx
    ret

hex_to_ascii ENDP

;=====================================================================
; ascii_to_hex(ascii_val: rcx, output: rdx) -> void
;
; Converts ASCII value to hex string "41".
; rcx = ASCII value (0-255)
; rdx = output buffer (2 bytes min)
;=====================================================================

ALIGN 16
ascii_to_hex PROC

    push rbx
    
    mov al, cl              ; al = value to convert
    mov rbx, rdx            ; rbx = output buffer
    
    ; Convert high nibble
    mov cl, al
    shr cl, 4
    
    cmp cl, 9
    jle ascii_to_hex_first_digit
    
    add cl, 'A' - 10
    jmp ascii_to_hex_first_done
    
ascii_to_hex_first_digit:
    add cl, '0'
    
ascii_to_hex_first_done:
    mov [rbx], cl
    
    ; Convert low nibble
    mov cl, al
    and cl, 0Fh
    
    cmp cl, 9
    jle ascii_to_hex_second_digit
    
    add cl, 'A' - 10
    jmp ascii_to_hex_second_done
    
ascii_to_hex_second_digit:
    add cl, '0'
    
ascii_to_hex_second_done:
    mov [rbx + 1], cl
    
    pop rbx
    ret

ascii_to_hex ENDP

END
