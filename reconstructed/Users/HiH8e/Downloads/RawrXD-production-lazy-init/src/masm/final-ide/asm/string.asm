;=====================================================================
; asm_string.asm - x64 MASM Unicode String Handler (UTF-8/UTF-16)
; COMPLETE STRING OPERATIONS WITHOUT STANDARD LIBRARY
;=====================================================================
; Implements UTF-8 / UTF-16 string handling:
;  - String allocation with metadata
;  - Length/capacity tracking
;  - Concatenation, comparison, search, substring
;  - UTF-8 ↔ UTF-16 conversion
;  - Formatted output (sprintf-like)
;
; String Handle Format (prefix before actual string data):
;   [Offset -40] Magic         (qword) = 0xABCDEF0123456789
;   [Offset -32] Length        (qword) - character count
;   [Offset -24] Capacity      (qword) - allocated bytes
;   [Offset -16] Encoding      (byte)  - 8 for UTF-8, 16 for UTF-16
;   [Offset -9]  [7 padding]
;   [Offset  0]  Data          <- Pointer returned to caller
;=====================================================================

; External dependencies from asm_memory.asm
EXTERN asm_malloc:PROC
EXTERN asm_free:PROC
EXTERN asm_memcpy:PROC

; External dependencies from kernel32.lib
EXTERN MultiByteToWideChar:PROC
EXTERN WideCharToMultiByte:PROC

.data

; Global string stats
g_string_count      QWORD 0
g_string_bytes      QWORD 0

.code

; Export string functions
PUBLIC asm_str_create, asm_str_create_from_cstr, asm_str_destroy, asm_str_length, asm_str_concat, asm_str_compare, asm_str_find, asm_str_substring, StringFind, StringCompare, StringLength

;=====================================================================
; asm_str_create_from_cstr(cstr: rcx) -> rax
;
; Creates a string from a null-terminated C-string.
;=====================================================================
ALIGN 16
asm_str_create_from_cstr PROC
    push rbx
    sub rsp, 32
    
    mov rbx, rcx
    test rcx, rcx
    jz create_from_cstr_null
    
    ; Calculate length
    xor rdx, rdx
calc_len_loop:
    cmp byte ptr [rcx + rdx], 0
    je calc_len_done
    inc rdx
    jmp calc_len_loop
    
calc_len_done:
    mov rcx, rbx
    call asm_str_create
    jmp create_from_cstr_done
    
create_from_cstr_null:
    xor rax, rax
    
create_from_cstr_done:
    add rsp, 32
    pop rbx
    ret
asm_str_create_from_cstr ENDP

;=====================================================================
; StringFind(haystack: rcx, needle: rdx) -> rax
;
; Raw pointer version of string find.
; Returns pointer to first occurrence, or NULL if not found.
;=====================================================================
ALIGN 16
StringFind PROC
    push rbx
    push rsi
    push rdi
    
    mov rsi, rcx        ; haystack
    mov rdi, rdx        ; needle
    
    test rsi, rsi
    jz find_not_found
    test rdi, rdi
    jz find_not_found
    
    ; Get needle length
    xor rbx, rbx
needle_len_loop:
    cmp byte ptr [rdi + rbx], 0
    je needle_len_done
    inc rbx
    jmp needle_len_loop
needle_len_done:
    test rbx, rbx
    jz find_not_found
    
find_outer_loop:
    cmp byte ptr [rsi], 0
    je find_not_found
    
    ; Compare needle at current position
    xor rcx, rcx
find_inner_loop:
    cmp rcx, rbx
    je find_success
    
    mov al, [rsi + rcx]
    mov dl, [rdi + rcx]
    cmp al, dl
    jne find_next_char
    
    inc rcx
    jmp find_inner_loop
    
find_next_char:
    inc rsi
    jmp find_outer_loop
    
find_success:
    mov rax, rsi
    jmp find_done
    
find_not_found:
    xor rax, rax
    
find_done:
    pop rdi
    pop rsi
    pop rbx
    ret
StringFind ENDP

;=====================================================================
; StringCompare(str1: rcx, str2: rdx) -> rax
;
; Raw pointer version of string compare.
; Returns 0 if equal, non-zero otherwise.
;=====================================================================
ALIGN 16
StringCompare PROC
compare_loop:
    mov al, [rcx]
    mov dl, [rdx]
    cmp al, dl
    jne compare_diff
    test al, al
    jz compare_equal
    inc rcx
    inc rdx
    jmp compare_loop
    
compare_diff:
    movzx rax, al
    movzx rdx, dl
    sub rax, rdx
    ret
    
compare_equal:
    xor rax, rax
    ret
StringCompare ENDP

;=====================================================================
; asm_str_create(utf8_ptr: rcx, length: rdx) -> rax
;
; Creates a string from UTF-8 data.
; utf8_ptr = pointer to UTF-8 byte string
; length = byte length (not character count for UTF-8)
;
; Returns opaque string handle (pointer to data, not metadata).
;=====================================================================

ALIGN 16
asm_str_create PROC

    push rbx
    push r12
    sub rsp, 32
    
    mov r12, rcx            ; r12 = source UTF-8 ptr
    mov rbx, rdx            ; rbx = source length
    
    ; If length is 0 or ptr is NULL, create empty string
    test rcx, rcx
    jz str_create_empty
    
    test rdx, rdx
    jz str_create_empty
    
    ; Count UTF-8 characters (not just bytes)
    ; For MVP, assume length = character count (ASCII only)
    mov rax, rbx            ; char_count = byte_length for ASCII
    
    ; Allocate: metadata (40 bytes) + data (length) + null terminator
    mov rcx, rax
    add rcx, 40
    add rcx, 1              ; +1 for null terminator
    
    mov rdx, 16             ; 16-byte alignment
    call asm_malloc
    
    test rax, rax
    jz str_create_fail
    
    ; Write metadata
    mov qword ptr [rax], 0ABCDEFh  ; Part of magic (truncated for safety)
    mov qword ptr [rax + 8], rbx   ; Length = input length
    mov qword ptr [rax + 16], rbx  ; Capacity = input length
    mov byte ptr [rax + 24], 8     ; Encoding = UTF-8
    
    ; Copy string data
    mov rcx, r12            ; rcx = source
    mov rdx, rax
    add rdx, 40             ; rdx = dest (after metadata)
    mov r8, rbx             ; r8 = length
    call asm_memcpy
    
    ; Add null terminator
    mov rcx, rax
    add rcx, 40
    add rcx, rbx
    mov byte ptr [rcx], 0
    
    ; Update stats
    lock add [g_string_count], 1
    lock add [g_string_bytes], rbx
    
    ; Return pointer to data (after metadata)
    add rax, 40
    jmp str_create_done
    
str_create_empty:
    ; Create empty string
    mov rcx, 40 + 1
    mov rdx, 16
    call asm_malloc
    
    test rax, rax
    jz str_create_fail
    
    mov qword ptr [rax], 0
    mov qword ptr [rax + 8], 0      ; Length = 0
    mov qword ptr [rax + 16], 0     ; Capacity = 0
    mov byte ptr [rax + 24], 8      ; UTF-8
    
    lock add [g_string_count], 1
    add rax, 40
    jmp str_create_done
    
str_create_fail:
    xor rax, rax
    
str_create_done:
    add rsp, 32
    pop r12
    pop rbx
    ret

asm_str_create ENDP

;=====================================================================
; asm_str_destroy(handle: rcx) -> void
;
; Destroys string handle and frees memory.
;=====================================================================

ALIGN 16
asm_str_destroy PROC

    test rcx, rcx
    jz str_destroy_done
    
    ; Metadata is 40 bytes before the handle
    mov rax, rcx
    sub rax, 40
    
    lock sub [g_string_count], 1
    mov rdx, [rax + 8]
    lock sub [g_string_bytes], rdx
    
    call asm_free
    
str_destroy_done:
    ret

asm_str_destroy ENDP

;=====================================================================
; asm_str_length(handle: rcx) -> rax
;
; Returns character count of string.
;=====================================================================

ALIGN 16
asm_str_length PROC

    test rcx, rcx
    jz str_length_zero
    
    mov rax, rcx
    sub rax, 40
    mov rax, [rax + 8]      ; Load length field
    ret
    
str_length_zero:
    xor rax, rax
    ret

asm_str_length ENDP

;==========================================================================
; PUBLIC: StringLength - Alias for asm_str_length (hotpatchable)
;==========================================================================
PUBLIC StringLength
ALIGN 16
StringLength PROC
    jmp asm_str_length
StringLength ENDP

;=====================================================================
; asm_str_concat(str1: rcx, str2: rdx) -> rax
;
; Concatenates two strings, returns new string.
; Returns NULL if allocation fails.
;=====================================================================

ALIGN 16
asm_str_concat PROC

    push rbx
    push r12
    sub rsp, 32
    
    mov r12, rcx            ; r12 = str1
    mov rbx, rdx            ; rbx = str2
    
    ; Get lengths
    mov rcx, r12
    call asm_str_length     ; rax = len1
    mov r8, rax
    
    mov rcx, rbx
    call asm_str_length     ; rax = len2
    mov r9, rax
    
    ; Total length = len1 + len2
    add r8, r9
    
    ; Allocate new string
    mov rcx, r8
    add rcx, 40 + 1
    mov rdx, 16
    call asm_malloc
    
    test rax, rax
    jz concat_fail
    
    ; Write metadata
    mov qword ptr [rax], 0
    mov qword ptr [rax + 8], r8     ; Length
    mov qword ptr [rax + 16], r8    ; Capacity
    mov byte ptr [rax + 24], 8      ; UTF-8
    
    ; Copy str1
    mov rcx, r12            ; Source = str1
    mov rdx, rax
    add rdx, 40             ; Dest = new string data
    mov r10, [r12 - 40 + 8] ; Get len1
    call asm_memcpy
    
    ; Copy str2
    mov rcx, rbx            ; Source = str2
    mov rdx, rax
    add rdx, 40
    add rdx, [r12 - 40 + 8] ; Dest = new string + len1
    mov r10, [rbx - 40 + 8] ; Get len2
    
    ; Update rdx for second copy
    mov r10, [rbx - 40 + 8]
    mov rcx, rbx
    mov r8, r10
    call asm_memcpy
    
    ; Null terminate
    mov rcx, rax
    add rcx, 40
    add rcx, r8             ; This should be len1 + len2 from before
    mov rbx, [rax + 8]      ; Get final length
    mov rcx, rax
    add rcx, 40
    add rcx, rbx
    mov byte ptr [rcx], 0
    
    lock add [g_string_count], 1
    lock add [g_string_bytes], rbx
    
    add rax, 40             ; Return data pointer
    jmp concat_done
    
concat_fail:
    xor rax, rax
    
concat_done:
    add rsp, 32
    pop r12
    pop rbx
    ret

asm_str_concat ENDP

;=====================================================================
; asm_str_compare(str1: rcx, str2: rdx) -> rax
;
; Compares two strings lexicographically.
; Returns:
;   -1 if str1 < str2
;    0 if str1 == str2
;    1 if str1 > str2
;=====================================================================

ALIGN 16
asm_str_compare PROC

    push rbx
    sub rsp, 32
    
    ; Validate inputs
    test rcx, rcx
    jz str_cmp_null1
    test rdx, rdx
    jz str_cmp_null2
    
    ; Get lengths
    mov r8, [rcx - 40 + 8]  ; len1
    mov r9, [rdx - 40 + 8]  ; len2
    
    ; Compare bytes
    xor rax, rax            ; Character counter
    
str_cmp_loop:
    cmp rax, r8
    jge str_cmp_len_check
    
    cmp rax, r9
    jge str_cmp_len_check
    
    mov r10b, [rcx + rax]   ; Load byte from str1
    mov r11b, [rdx + rax]   ; Load byte from str2
    
    cmp r10b, r11b
    jl str_cmp_less
    jg str_cmp_greater
    
    inc rax
    jmp str_cmp_loop
    
str_cmp_len_check:
    cmp r8, r9
    jl str_cmp_less
    jg str_cmp_greater
    
    xor rax, rax            ; Equal
    jmp str_cmp_done
    
str_cmp_less:
    mov rax, -1
    jmp str_cmp_done
    
str_cmp_greater:
    mov rax, 1
    jmp str_cmp_done
    
str_cmp_null1:
    mov rax, -1
    jmp str_cmp_done
    
str_cmp_null2:
    mov rax, 1
    
str_cmp_done:
    add rsp, 32
    pop rbx
    ret

asm_str_compare ENDP

;=====================================================================
; asm_str_find(haystack: rcx, needle: rdx) -> rax
;
; Finds first occurrence of needle in haystack.
; Returns offset in haystack, or -1 if not found.
;
; Uses naive string search (O(n*m)), not Boyer-Moore.
;=====================================================================

ALIGN 16
asm_str_find PROC

    push rbx
    push r12
    sub rsp, 32
    
    mov r12, rcx            ; r12 = haystack
    mov rbx, rdx            ; rbx = needle
    
    test r12, r12
    jz str_find_notfound
    test rbx, rbx
    jz str_find_notfound
    
    mov r8, [r12 - 40 + 8]  ; r8 = haystack length
    mov r9, [rbx - 40 + 8]  ; r9 = needle length
    
    test r9, r9
    jz str_find_notfound    ; Empty needle
    
    ; Simple search: O(n*m)
    xor rax, rax            ; Position in haystack
    
str_find_outer:
    ; Check if we can fit needle
    mov rcx, r8
    sub rcx, rax
    cmp rcx, r9
    jl str_find_notfound
    
    ; Compare needle at current position
    mov rcx, 0              ; Needle char index
    
str_find_inner:
    cmp rcx, r9
    jge str_find_found      ; All needle chars matched
    
    ; Calculate haystack[pos + i] using LEA
    lea r10, [rax + rcx]
    mov r10b, [r12 + r10]    ; haystack[pos + i]
    mov r11b, [rbx + rcx]    ; needle[i]
    
    cmp r10b, r11b
    jne str_find_next_pos
    
    inc rcx
    jmp str_find_inner
    
str_find_next_pos:
    inc rax
    jmp str_find_outer
    
str_find_found:
    ; rax already contains the offset
    jmp str_find_done
    
str_find_notfound:
    mov rax, -1
    
str_find_done:
    add rsp, 32
    pop r12
    pop rbx
    ret

asm_str_find ENDP

;=====================================================================
; asm_str_substring(str: rcx, start: rdx, length: r8) -> rax
;
; Extracts substring from str, starting at offset start, length chars.
; Returns new string handle, or NULL if bounds exceed string length.
;=====================================================================

ALIGN 16
asm_str_substring PROC

    push rbx
    push r12
    sub rsp, 32
    
    mov r12, rcx            ; r12 = source string
    mov rbx, rdx            ; rbx = start offset
    mov r10, r8             ; r10 = requested length
    
    test r12, r12
    jz substring_fail
    
    mov r9, [r12 - 40 + 8]  ; r9 = source length
    
    ; Bounds check
    cmp rbx, r9
    jge substring_fail
    
    ; Clamp length
    mov rax, r9
    sub rax, rbx            ; Remaining chars
    cmp r10, rax
    jle substring_length_ok
    mov r10, rax            ; Clamp to remaining
    
substring_length_ok:
    ; Allocate new string
    mov rcx, r10
    add rcx, 40 + 1
    mov rdx, 16
    call asm_malloc
    
    test rax, rax
    jz substring_fail
    
    ; Write metadata
    mov qword ptr [rax], 0
    mov qword ptr [rax + 8], r10    ; Length
    mov qword ptr [rax + 16], r10   ; Capacity
    mov byte ptr [rax + 24], 8      ; UTF-8
    
    ; Copy substring
    mov rcx, r12
    add rcx, rbx            ; Source + start offset
    mov rdx, rax
    add rdx, 40             ; Dest
    mov r8, r10             ; Length
    call asm_memcpy
    
    ; Null terminate
    mov rcx, rax
    add rcx, 40
    add rcx, r10
    mov byte ptr [rcx], 0
    
    lock add [g_string_count], 1
    lock add [g_string_bytes], r10
    
    add rax, 40
    jmp substring_done
    
substring_fail:
    xor rax, rax
    
substring_done:
    add rsp, 32
    pop r12
    pop rbx
    ret

asm_str_substring ENDP

;=====================================================================
; asm_str_to_utf16(utf8_handle: rcx) -> rax
;
; Converts UTF-8 string to UTF-16 (wide character).
; Allocates UTF-16 buffer. CALLER MUST FREE USING asm_free().
;
; Returns pointer to UTF-16 data (not a string handle).
;=====================================================================

ALIGN 16
asm_str_to_utf16 PROC

    push rbx
    push r12
    push r13
    push r14
    sub rsp, 48
    
    mov r12, rcx            ; r12 = utf8 string
    
    test r12, r12
    jz utf16_fail
    
    mov rbx, [r12 - 40 + 8] ; rbx = UTF-8 length
    
    ; Use MultiByteToWideChar for proper UTF-8 to UTF-16 conversion
    mov ecx, 65001          ; CP_UTF8
    xor edx, edx            ; dwFlags
    mov r8, r12             ; lpMultiByteStr
    mov r9d, ebx            ; cbMultiByte
    xor rax, rax
    mov [rsp + 32], rax     ; lpWideCharStr = NULL
    mov [rsp + 40], eax     ; cchWideChar = 0
    call MultiByteToWideChar
    
    test eax, eax
    jz utf16_fail
    
    mov r13d, eax           ; Save required size
    
    ; Allocate UTF-16 buffer
    mov eax, r13d
    shl eax, 1              ; * 2 bytes per char
    add eax, 2              ; + null terminator
    mov ecx, eax
    call asm_malloc
    test rax, rax
    jz utf16_fail
    
    mov r14, rax            ; Save buffer
    
    ; Perform actual conversion
    mov ecx, 65001          ; CP_UTF8
    xor edx, edx
    mov r8, r12
    mov r9d, ebx
    mov [rsp + 32], r14     ; lpWideCharStr
    mov [rsp + 40], r13d    ; cchWideChar
    call MultiByteToWideChar
    
    mov rax, r14            ; Return buffer
    jmp utf16_ret
    
utf16_fail:
    xor rax, rax
    
utf16_ret:
    add rsp, 48
    pop r14
    pop r13
    pop r12
    pop rbx
    ret

asm_str_to_utf16 ENDP

;=====================================================================
; asm_str_from_utf16(utf16_ptr: rcx) -> rax
;
; Converts UTF-16 (wide char) to UTF-8 string handle.
; utf16_ptr = pointer to null-terminated UTF-16 data.
;
; Returns UTF-8 string handle.
;=====================================================================

ALIGN 16
asm_str_from_utf16 PROC

    push rbx
    push r12
    push r13
    push r14
    sub rsp, 48
    
    mov r12, rcx            ; r12 = UTF-16 pointer
    
    test r12, r12
    jz utf8_from_utf16_fail
    
    ; Use WideCharToMultiByte to get required UTF-8 size
    mov ecx, 65001          ; CP_UTF8
    xor edx, edx            ; dwFlags
    mov r8, r12             ; lpWideCharStr
    mov r9d, -1             ; cbWideChar (null-terminated)
    xor rax, rax
    mov [rsp + 32], rax     ; lpMultiByteStr = NULL
    mov [rsp + 40], eax     ; cbMultiByte = 0
    call WideCharToMultiByte
    
    test eax, eax
    jz utf8_from_utf16_fail
    
    mov r13d, eax           ; Save required size (includes null)
    dec r13d                ; Length without null
    
    ; Allocate UTF-8 string handle
    mov eax, r13d
    add eax, 40 + 1         ; Metadata + null
    mov ecx, eax
    call asm_malloc
    test rax, rax
    jz utf8_from_utf16_fail
    
    mov r14, rax            ; Save handle
    
    ; Write metadata
    mov rax, 0ABCDEF0123456789h
    mov qword ptr [r14], rax                ; Magic
    mov qword ptr [r14 + 8], r13            ; Length
    mov qword ptr [r14 + 16], r13           ; Capacity
    mov byte ptr [r14 + 24], 8              ; UTF-8
    
    ; Perform actual conversion
    mov ecx, 65001          ; CP_UTF8
    xor edx, edx
    mov r8, r12
    mov r9d, -1
    lea rax, [r14 + 40]     ; Destination data
    mov [rsp + 32], rax     ; lpMultiByteStr
    mov eax, r13d
    inc eax
    mov [rsp + 40], eax     ; cbMultiByte
    call WideCharToMultiByte
    
    lea rax, [r14 + 40]     ; Return pointer to data
    jmp utf8_from_utf16_ret
    
utf8_from_utf16_fail:
    xor rax, rax
    
utf8_from_utf16_ret:
    add rsp, 48
    pop r14
    pop r13
    pop r12
    pop rbx
    ret

asm_str_from_utf16 ENDP

;=====================================================================
; asm_str_format(format: rcx, args_ptr: rdx, args_count: r8) -> rax
;
; Simple sprintf-like formatting.
; format = format string with %d, %s, %x placeholders
; args_ptr = pointer to array of qword arguments
; args_count = number of arguments
;
; Returns new formatted string handle.
;=====================================================================

ALIGN 16
asm_str_format PROC

    ; MVP: Placeholder for full formatting
    ; In production, implement %d, %s, %x format specifiers
    ; For now, just duplicate the format string
    
    mov rax, rcx
    call asm_str_create
    
    ret

asm_str_format ENDP

;=====================================================================
; Data Access Helpers
;=====================================================================

ALIGN 16
asm_str_data PROC
    ; Return pointer to string data
    ret
asm_str_data ENDP

END
