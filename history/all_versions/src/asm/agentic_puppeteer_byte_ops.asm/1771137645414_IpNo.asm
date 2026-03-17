; ================================================
; Agentic Puppeteer Byte Operations - Enhanced
; Memory-safe manipulation for real-time AI correction
; ================================================

INCLUDE ksamd64.inc

PUBLIC asm_safe_replace
PUBLIC asm_utf8_to_utf16
PUBLIC asm_ring_buffer_write
PUBLIC asm_byte_stream_filter
PUBLIC asm_unicode_normalize

; ================================================
; Result Codes
; ================================================
CORRECTION_SUCCESS EQU 0
CORRECTION_BUFFER_OVERFLOW EQU 1
CORRECTION_INVALID_PARAMS EQU 2
CORRECTION_UNICODE_ERROR EQU 3
CORRECTION_STREAM_ERROR EQU 4

; ================================================
; Data Section
; ================================================
.data
align 16
correction_replace_success DB "Replacement successful", 0
correction_buffer_overflow DB "Buffer overflow prevented", 0
correction_invalid_params  DB "Invalid parameters", 0
unicode_transform_success  DB "Unicode transform success", 0
unicode_invalid_sequence   DB "Invalid UTF-8 sequence", 0
unicode_incomplete_sequence DB "Incomplete UTF-8", 0
stream_process_success     DB "Stream processed", 0
ring_buffer_full_msg       DB "Ring buffer full", 0

; Escape sequence tables
control_char_replacements DB '\n', '\t', '\r', '\b', '\f', '\v', '\a', '\"', '\\', 0
control_char_escapes      DB 'n', 't', 'r', 'b', 'f', 'v', 'a', '"', '\', 0

; ================================================
; Macros for Result Creation
; ================================================
CREATE_CORRECTION_RESULT MACRO result_code, bytes_written, status_msg_ptr
    mov     eax, result_code
    mov     [rcx], eax
    mov     [rcx+4], bytes_written
    lea     rax, status_msg_ptr
    mov     [rcx+8], rax
ENDM

; ================================================
; Safe Memory Pattern Replace (Boyer-Moore Optimized)
; ================================================
.code
align 16
asm_safe_replace PROC FRAME
    ; RCX = dest buffer, RDX = dest size, R8 = search pattern, R9 = replacement
    ; [RSP+40] = pattern_len, [RSP+48] = replacement_len, [RSP+56] = result_out
    
    push    rbp
    .pushreg rbp
    push    r15
    .pushreg r15
    push    r14
    .pushreg r14
    push    r13
    .pushreg r13
    push    r12
    .pushreg r12
    mov     rbp, rsp
    sub     rsp, 128
    .allocstack 128
    .endprolog
    
    mov     r10, [rbp+56]           ; Pattern length
    mov     r11, [rbp+64]           ; Replacement length
    mov     r12, [rbp+72]           ; Result structure
    
    ; Validate parameters
    test    rcx, rcx
    jz      @@invalid_params
    test    rdx, rdx
    jz      @@invalid_params
    test    r8, r8
    jz      @@invalid_params
    test    r9, r9
    jz      @@invalid_params
    test    r10, r10
    jz      @@invalid_params
    
    ; Calculate max possible size after replacement
    mov     rax, rdx
    sub     rax, r10
    add     rax, r11
    cmp     rax, rdx
    ja      @@buffer_overflow
    
    ; Boyer-Moore bad character preprocessing (simplified)
    mov     r13, 0                  ; Matches count
    mov     r14, 0                  ; Current position
    
@@search_loop:
    mov     rax, r14
    add     rax, r10
    cmp     rax, rdx
    ja      @@replace_done
    
    ; Pattern matching from right to left
    mov     r15, r10
    dec     r15                     ; Last character index
    
@@compare_loop:
    mov     al, [rcx+r14+r15]       ; Current character in buffer
    mov     bl, [r8+r15]            ; Pattern character
    cmp     al, bl
    jne     @@mismatch
    
    test    r15, r15
    jz      @@pattern_match
    dec     r15
    jmp     @@compare_loop
    
@@mismatch:
    ; Bad character heuristic - advance by 1 for simplicity
    inc     r14
    jmp     @@search_loop
    
@@pattern_match:
    ; Perform replacement in-place
    push    rcx
    push    rdx
    
    ; Move tail if sizes differ
    cmp     r11, r10
    je      @@same_size
    
    mov     rsi, rcx
    add     rsi, r14
    add     rsi, r10                ; Source: after pattern
    
    mov     rdi, rcx
    add     rdi, r14
    add     rdi, r11                ; Dest: after replacement
    
    mov     rcx, rdx
    sub     rcx, r14
    sub     rcx, r10                ; Bytes to move
    
    cld
    rep     movsb
    
@@same_size:
    ; Copy replacement
    pop     rdx
    pop     rcx
    
    push    rcx
    push    rdx
    mov     rdi, rcx
    add     rdi, r14                ; Destination
    mov     rsi, r9                 ; Source (replacement)
    mov     rcx, r11                ; Length
    rep     movsb
    pop     rdx
    pop     rcx
    
    inc     r13                     ; Increment match count
    add     r14, r11                ; Skip past replacement
    jmp     @@search_loop
    
@@replace_done:
    mov     rax, r13                ; Number of replacements
    CREATE_CORRECTION_RESULT CORRECTION_SUCCESS, rax, correction_replace_success
    mov     rax, r13
    jmp     @@done
    
@@buffer_overflow:
    xor     rax, rax
    CREATE_CORRECTION_RESULT CORRECTION_BUFFER_OVERFLOW, rax, correction_buffer_overflow
    jmp     @@done
    
@@invalid_params:
    xor     rax, rax
    CREATE_CORRECTION_RESULT CORRECTION_INVALID_PARAMS, rax, correction_invalid_params
    
@@done:
    add     rsp, 128
    pop     r12
    pop     r13
    pop     r14
    pop     r15
    pop     rbp
    ret
asm_safe_replace ENDP

; ================================================
; Optimized UTF-8 to UTF-16 Conversion
; ================================================
align 16
asm_utf8_to_utf16 PROC FRAME
    ; RCX = UTF-8 source, RDX = source bytes, R8 = UTF-16 dest, R9 = dest capacity (words)
    ; [RSP+40] = result_struct
    
    push    rbp
    .pushreg rbp
    push    r15
    .pushreg r15
    push    r14
    .pushreg r14
    push    r13
    .pushreg r13
    push    r12
    .pushreg r12
    mov     rbp, rsp
    sub     rsp, 48
    .allocstack 48
    .endprolog
    
    mov     r10, [rbp+56]           ; Result struct
    xor     r11, r11                ; Source index
    xor     r12, r12                ; Dest index
    
@@convert_loop:
    cmp     r11, rdx
    jae     @@convert_success
    
    cmp     r12, r9
    jae     @@dest_overflow
    
    ; Load UTF-8 byte
    movzx   eax, byte ptr [rcx+r11]
    
    ; Check byte class
    cmp     al, 80h
    jb      @@handle_ascii          ; 0xxxxxxx
    cmp     al, 0C0h
    jb      @@invalid_utf8          ; 10xxxxxx (continuation)
    cmp     al, 0E0h
    jb      @@handle_2byte          ; 110xxxxx
    cmp     al, 0F0h
    jb      @@handle_3byte          ; 1110xxxx  
    cmp     al, 0F8h
    jb      @@handle_4byte          ; 11110xxx
    jmp     @@invalid_utf8          ; Invalid start byte
    
@@handle_ascii:
    mov     [r8+r12*2], ax          ; Store as UTF-16
    inc     r11
    inc     r12
    jmp     @@convert_loop
    
@@handle_2byte:
    ; Validate we have enough bytes
    mov     r13, rdx
    sub     r13, r11
    cmp     r13, 2
    jb      @@incomplete_sequence
    
    ; Extract payload
    and     eax, 1Fh                ; 5 bits
    shl     eax, 6
    
    movzx   ebx, byte ptr [rcx+r11+1]
    cmp     bl, 80h
    jb      @@invalid_utf8
    cmp     bl, 0C0h
    jae     @@invalid_utf8
    
    and     ebx, 3Fh                ; 6 bits
    or      eax, ebx
    
    ; Range check (must be > 127)
    cmp     eax, 80h
    jb      @@invalid_utf8
    
    mov     [r8+r12*2], ax
    add     r11, 2
    inc     r12
    jmp     @@convert_loop
    
@@handle_3byte:
    ; Validate length
    mov     r13, rdx
    sub     r13, r11
    cmp     r13, 3
    jb      @@incomplete_sequence
    
    ; Extract payload
    and     eax, 0Fh                ; 4 bits
    shl     eax, 12
    
    movzx   ebx, byte ptr [rcx+r11+1]
    cmp     bl, 80h
    jb      @@invalid_utf8
    cmp     bl, 0C0h
    jae     @@invalid_utf8
    and     ebx, 3Fh
    shl     ebx, 6
    or      eax, ebx
    
    movzx   ebx, byte ptr [rcx+r11+2]
    cmp     bl, 80h
    jb      @@invalid_utf8
    cmp     bl, 0C0h
    jae     @@invalid_utf8
    and     ebx, 3Fh
    or      eax, ebx
    
    ; Range check (must be > 2047 and not surrogate)
    cmp     eax, 800h
    jb      @@invalid_utf8
    cmp     eax, 0D800h
    jb      @@store_3byte
    cmp     eax, 0E000h
    jb      @@invalid_utf8          ; Surrogate range

@@store_3byte:
    mov     [r8+r12*2], ax
    add     r11, 3
    inc     r12
    jmp     @@convert_loop
    
@@handle_4byte:
    ; Convert to surrogate pair
    mov     r13, rdx
    sub     r13, r11
    cmp     r13, 4
    jb      @@incomplete_sequence
    
    ; Check dest has space for surrogate pair
    mov     r13, r9
    sub     r13, r12
    cmp     r13, 2
    jb      @@dest_overflow
    
    ; Decode 4-byte sequence
    and     eax, 07h                ; 3 bits
    shl     eax, 18
    
    movzx   ebx, byte ptr [rcx+r11+1]
    cmp     bl, 80h
    jb      @@invalid_utf8
    and     ebx, 3Fh
    shl     ebx, 12
    or      eax, ebx
    
    movzx   ebx, byte ptr [rcx+r11+2]
    cmp     bl, 80h
    jb      @@invalid_utf8
    and     ebx, 3Fh
    shl     ebx, 6
    or      eax, ebx
    
    movzx   ebx, byte ptr [rcx+r11+3]
    cmp     bl, 80h
    jb      @@invalid_utf8
    and     ebx, 3Fh
    or      eax, ebx
    
    ; Range check
    cmp     eax, 10000h
    jb      @@invalid_utf8
    cmp     eax, 110000h
    jae     @@invalid_utf8
    
    ; Convert to surrogate pair
    sub     eax, 10000h
    mov     r13d, eax
    shr     r13d, 10
    add     r13d, 0D800h            ; High surrogate
    and     eax, 3FFh
    add     eax, 0DC00h             ; Low surrogate
    
    mov     [r8+r12*2], r13w
    mov     [r8+r12*2+2], ax
    
    add     r11, 4
    add     r12, 2
    jmp     @@convert_loop
    
@@convert_success:
    mov     rax, r12                ; Characters written
    CREATE_CORRECTION_RESULT CORRECTION_SUCCESS, rax, unicode_transform_success
    mov     rax, r12
    jmp     @@done
    
@@dest_overflow:
    CREATE_CORRECTION_RESULT CORRECTION_BUFFER_OVERFLOW, r12, unicode_incomplete_sequence
    mov     rax, r12
    jmp     @@done
    
@@incomplete_sequence:
    CREATE_CORRECTION_RESULT CORRECTION_UNICODE_ERROR, r12, unicode_incomplete_sequence
    xor     rax, rax
    jmp     @@done
    
@@invalid_utf8:
    CREATE_CORRECTION_RESULT CORRECTION_UNICODE_ERROR, r12, unicode_invalid_sequence
    xor     rax, rax
    
@@done:
    add     rsp, 48
    pop     r12
    pop     r13
    pop     r14
    pop     r15
    pop     rbp
    ret
asm_utf8_to_utf16 ENDP

; ================================================
; Lock-Free Ring Buffer Writer
; ================================================
align 16
asm_ring_buffer_write PROC FRAME
    ; RCX = buffer base, RDX = buffer_mask (size-1), R8 = head_tail_atomic, R9 = data
    ; [RSP+40] = data_len
    
    push    rbp
    .pushreg rbp
    push    r12
    .pushreg r12
    push    r11
    .pushreg r11
    mov     rbp, rsp
    .endprolog
    
    mov     r10, [rbp+48]           ; Data length
    
    ; Atomic load of head and tail
    mov     rax, [r8]               ; Load 64-bit head|tail
    mov     r11d, eax               ; Tail (low 32)
    shr     rax, 32                 ; Head (high 32)
    mov     r12d, eax
    
    ; Calculate available space
    mov     eax, r11d
    sub     eax, r12d
    and     eax, edx                ; Available = (tail - head) & mask
    
    cmp     r10, rax
    ja      @@buffer_full
    
    ; Write data
    xor     rax, rax
@@write_loop:
    cmp     rax, r10
    jae     @@write_complete
    
    mov     bl, [r9+rax]
    mov     [rcx+r12], bl
    
    inc     r12d
    and     r12d, edx               ; Wrap head
    inc     rax
    jmp     @@write_loop
    
@@write_complete:
    ; Atomic update head
    shl     r12, 32
    or      r12, r11                ; New head|tail
    
    mov     rax, [r8]
@@retry_commit:
    lock cmpxchg [r8], r12
    jne     @@retry_commit
    
    mov     rax, r10                ; Bytes written
    jmp     @@done
    
@@buffer_full:
    xor     rax, rax
    
@@done:
    pop     r11
    pop     r12
    pop     rbp
    ret
asm_ring_buffer_write ENDP

; ================================================
; Stream Filter with Control Character Escape
; ================================================
align 16
asm_byte_stream_filter PROC FRAME
    ; RCX = input, RDX = input_len, R8 = output, R9 = output_capacity
    
    push    rbp
    .pushreg rbp
    push    r12
    .pushreg r12
    push    r11
    .pushreg r11
    mov     rbp, rsp
    .endprolog
    
    xor     r10, r10                ; Input index
    xor     r11, r11                ; Output index
    
@@filter_loop:
    cmp     r10, rdx
    jae     @@filter_done
    cmp     r11, r9
    jae     @@output_full
    
    movzx   eax, byte ptr [rcx+r10]
    
    ; Check if control character needs escaping
    cmp     al, 32
    jae     @@printable
    
    ; Find in control table
    lea     rsi, [control_char_replacements]
    mov     ecx, 9                  ; Table size
    
@@find_control:
    test    ecx, ecx
    jz      @@unknown_control
    cmp     al, [rsi]
    je      @@escape_control
    inc     rsi
    dec     ecx
    jmp     @@find_control
    
@@escape_control:
    ; Write backslash
    cmp     r11, r9
    jae     @@output_full
    mov     byte ptr [r8+r11], '\'
    inc     r11
    
    ; Write escape character
    cmp     r11, r9
    jae     @@output_full
    sub     rsi, control_char_replacements
    add     rsi, control_char_escapes
    mov     al, [rsi]
    mov     [r8+r11], al
    inc     r11
    jmp     @@next_char
    
@@unknown_control:
    ; Hex escape \x##
    cmp     r11, r9
    jae     @@output_full
    mov     byte ptr [r8+r11], '\'
    inc     r11
    cmp     r11, r9
    jae     @@output_full
    mov     byte ptr [r8+r11], 'x'
    inc     r11
    
    ; Convert to hex
    mov     bl, al
    shr     bl, 4
    cmp     bl, 10
    jb      @@digit1
    add     bl, 'A'-10
    jmp     @@store1
@@digit1:
    add     bl, '0'
@@store1:
    cmp     r11, r9
    jae     @@output_full
    mov     [r8+r11], bl
    inc     r11
    
    mov     bl, al
    and     bl, 0Fh
    cmp     bl, 10
    jb      @@digit2
    add     bl, 'A'-10
    jmp     @@store2
@@digit2:
    add     bl, '0'
@@store2:
    cmp     r11, r9
    jae     @@output_full
    mov     [r8+r11], bl
    inc     r11
    jmp     @@next_char
    
@@printable:
    mov     [r8+r11], al
    inc     r11
    
@@next_char:
    inc     r10
    jmp     @@filter_loop
    
@@filter_done:
    mov     rax, r11                ; Bytes written
    jmp     @@done
    
@@output_full:
    mov     rax, r11                ; Partial bytes written
    
@@done:
    pop     r11
    pop     r12
    pop     rbp
    ret
asm_byte_stream_filter ENDP

; ================================================
; Unicode Normalization (NFD/NFC Basic)
; ================================================
align 16
asm_unicode_normalize PROC FRAME
    ; RCX = input UTF-16, RDX = input_len, R8 = output, R9 = capacity
    ; Basic placeholder - full implementation requires Unicode tables
    
    push    rbp
    .pushreg rbp
    mov     rbp, rsp
    .endprolog
    
    ; Simple copy for now (production would decompose/compose)
    cmp     rdx, r9
    jle     @@size_ok
    mov     rdx, r9
    
@@size_ok:
    mov     rax, rdx
    shl     rax, 1                  ; Bytes
    
    push    rcx
    push    rdx
    mov     rdi, r8
    mov     rsi, rcx
    mov     rcx, rax
    rep     movsb
    pop     rdx
    pop     rcx
    
    mov     rax, rdx                ; Characters copied
    
    pop     rbp
    ret
asm_unicode_normalize ENDP

END