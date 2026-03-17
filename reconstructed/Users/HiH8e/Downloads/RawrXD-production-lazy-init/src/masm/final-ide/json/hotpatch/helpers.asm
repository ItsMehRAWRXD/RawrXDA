;==========================================================================
; json_hotpatch_helpers.asm - Hotpatchable JSON Helper Functions
;==========================================================================
; Provides runtime-modifiable JSON manipulation functions that can be
; hotpatched via memory, byte-level, or server-layer hotpatching.
; All functions are PUBLIC and can be replaced at runtime.
;==========================================================================

option casemap:none

include windows.inc
includelib kernel32.lib

;==========================================================================
; EXTERNAL DECLARATIONS
;==========================================================================
EXTERN asm_malloc:PROC
EXTERN asm_free:PROC
EXTERN StringLength:PROC
EXTERN StringCompare:PROC

;==========================================================================
; DATA SEGMENT
;==========================================================================
.data
    ; JSON formatting strings
    szJsonNull          BYTE "null", 0
    szJsonTrue          BYTE "true", 0
    szJsonFalse         BYTE "false", 0
    szJsonQuote         BYTE '"', 0
    szJsonColon         BYTE ": ", 0
    szJsonComma         BYTE ", ", 0
    szJsonOpenBrace     BYTE "{", 0
    szJsonCloseBrace    BYTE "}", 0
    szJsonOpenBracket   BYTE "[", 0
    szJsonCloseBracket  BYTE "]", 0
    
    ; Number conversion buffer
    numBuffer           BYTE 32 DUP (0)
    
    ; Hotpatch version tracking
    json_helpers_version DWORD 1
    hotpatch_count       DWORD 0

;==========================================================================
; CODE SEGMENT
;==========================================================================
.code

;==========================================================================
; PUBLIC: append_string - Append string to JSON buffer (hotpatchable)
; INPUT: rcx = buffer, rdx = str_to_append, r8 = current_offset
; OUTPUT: rax = new offset
;==========================================================================
PUBLIC append_string
PUBLIC _append_string
ALIGN 16
append_string PROC
    push rsi
    push rdi
    push rbx
    
    mov rdi, rcx        ; buffer base
    add rdi, r8         ; current position
    mov rsi, rdx        ; string to append
    mov rbx, r8         ; save offset
    
append_loop:
    mov al, byte ptr [rsi]
    test al, al
    jz append_done
    
    mov byte ptr [rdi], al
    inc rsi
    inc rdi
    inc rbx
    jmp append_loop
    
append_done:
    mov rax, rbx        ; return new offset
    
    pop rbx
    pop rdi
    pop rsi
    ret
append_string ENDP

; Alias for underscore naming convention
_append_string EQU append_string

;==========================================================================
; PUBLIC: append_int - Append integer to JSON buffer (hotpatchable)
; INPUT: rcx = buffer, edx = integer value, r8 = current_offset
; OUTPUT: rax = new offset
;==========================================================================
PUBLIC append_int
PUBLIC _append_int
ALIGN 16
append_int PROC
    push rsi
    push rdi
    push rbx
    push r12
    sub rsp, 32
    
    mov rdi, rcx        ; buffer base
    mov r12, r8         ; current offset
    mov eax, edx        ; value to convert
    
    ; Convert integer to string
    lea rcx, numBuffer
    mov edx, eax
    call int_to_string_simple
    
    ; Append the string
    mov rcx, rdi        ; buffer
    lea rdx, numBuffer  ; converted string
    mov r8, r12         ; offset
    call append_string
    
    add rsp, 32
    pop r12
    pop rbx
    pop rdi
    pop rsi
    ret
append_int ENDP

; Alias for underscore naming convention
_append_int EQU append_int

;==========================================================================
; PUBLIC: append_float - Append float to JSON buffer (hotpatchable)
; INPUT: rcx = buffer, xmm0 = float value, rdx = current_offset
; OUTPUT: rax = new offset
;==========================================================================
PUBLIC append_float
PUBLIC _append_float
ALIGN 16
append_float PROC
    push rsi
    push rdi
    push rbx
    sub rsp, 32
    
    mov rdi, rcx        ; buffer base
    mov rbx, rdx        ; current offset
    
    ; Convert float to int (simple truncation for now)
    cvttss2si eax, xmm0
    
    ; Convert to string
    lea rcx, numBuffer
    mov edx, eax
    call int_to_string
    
    ; Append
    mov rcx, rdi
    lea rdx, numBuffer
    mov r8, rbx
    call append_string
    
    add rsp, 32
    pop rbx
    pop rdi
    pop rsi
    ret
append_float ENDP

; Alias for underscore naming convention
_append_float EQU append_float

;==========================================================================
; PUBLIC: append_bool - Append boolean to JSON buffer (hotpatchable)
; INPUT: rcx = buffer, edx = bool value (0/1), r8 = current_offset
; OUTPUT: rax = new offset
;==========================================================================
PUBLIC append_bool
PUBLIC _append_bool
ALIGN 16
append_bool PROC
    push rsi
    push rdi
    
    mov rdi, rcx
    mov rsi, r8
    
    test edx, edx
    jz append_false
    
append_true:
    lea rdx, szJsonTrue
    jmp append_bool_str
    
append_false:
    lea rdx, szJsonFalse
    
append_bool_str:
    mov rcx, rdi
    mov r8, rsi
    call append_string
    
    pop rdi
    pop rsi
    ret
append_bool ENDP

; Alias for underscore naming convention
_append_bool EQU append_bool

;==========================================================================
; PUBLIC: write_json_to_file - Write JSON buffer to file (hotpatchable)
; INPUT: rcx = filename, rdx = buffer, r8 = size
; OUTPUT: rax = 1 on success, 0 on failure
;==========================================================================
PUBLIC write_json_to_file
PUBLIC _write_json_to_file
ALIGN 16
write_json_to_file PROC
    push rbx
    push rsi
    push rdi
    sub rsp, 32
    
    mov rsi, rcx        ; filename
    mov rdi, rdx        ; buffer
    mov rbx, r8         ; size
    
    ; Create file
    mov rcx, rsi
    mov edx, GENERIC_WRITE
    mov r8d, 0          ; no sharing
    xor r9d, r9d        ; no security
    mov qword ptr [rsp+32], CREATE_ALWAYS
    mov dword ptr [rsp+40], FILE_ATTRIBUTE_NORMAL
    xor eax, eax
    mov qword ptr [rsp+48], rax
    call CreateFileA
    
    cmp rax, INVALID_HANDLE_VALUE
    je write_failed
    
    mov r12, rax        ; save handle
    
    ; Write data
    mov rcx, r12
    mov rdx, rdi
    mov r8d, ebx
    lea r9, [rsp+56]
    xor eax, eax
    mov qword ptr [rsp+32], rax
    call WriteFile
    
    ; Close handle
    mov rcx, r12
    call CloseHandle
    
    mov eax, 1
    jmp write_done
    
write_failed:
    xor eax, eax
    
write_done:
    add rsp, 32
    pop rdi
    pop rsi
    pop rbx
    ret
write_json_to_file ENDP

; Alias for underscore naming convention
_write_json_to_file EQU write_json_to_file

;==========================================================================
; PUBLIC: read_json_from_file - Read JSON from file (hotpatchable)
; INPUT: rcx = filename, rdx = buffer, r8 = max_size
; OUTPUT: rax = bytes read, 0 on failure
;==========================================================================
PUBLIC read_json_from_file
PUBLIC _read_json_from_file
ALIGN 16
read_json_from_file PROC
    push rbx
    push rsi
    push rdi
    sub rsp, 32
    
    mov rsi, rcx        ; filename
    mov rdi, rdx        ; buffer
    mov rbx, r8         ; max_size
    
    ; Open file
    mov rcx, rsi
    mov edx, GENERIC_READ
    mov r8d, FILE_SHARE_READ
    xor r9d, r9d
    mov qword ptr [rsp+32], OPEN_EXISTING
    mov dword ptr [rsp+40], FILE_ATTRIBUTE_NORMAL
    xor eax, eax
    mov qword ptr [rsp+48], rax
    call CreateFileA
    
    cmp rax, INVALID_HANDLE_VALUE
    je read_failed
    
    mov r12, rax        ; save handle
    
    ; Read data
    mov rcx, r12
    mov rdx, rdi
    mov r8d, ebx
    lea r9, [rsp+56]
    xor eax, eax
    mov qword ptr [rsp+32], rax
    call ReadFile
    
    mov rax, qword ptr [rsp+56]  ; bytes read
    
    ; Close handle
    push rax
    mov rcx, r12
    call CloseHandle
    pop rax
    
    jmp read_done
    
read_failed:
    xor rax, rax
    
read_done:
    add rsp, 32
    pop rdi
    pop rsi
    pop rbx
    ret
read_json_from_file ENDP

; Alias for underscore naming convention
_read_json_from_file EQU read_json_from_file

;==========================================================================
; PUBLIC: find_json_key - Find key in JSON string (hotpatchable)
; INPUT: rcx = json_string, rdx = key_name
; OUTPUT: rax = pointer to value start, 0 if not found
;==========================================================================
PUBLIC find_json_key
PUBLIC _find_json_key
ALIGN 16
find_json_key PROC
    push rsi
    push rdi
    push rbx
    
    mov rsi, rcx        ; json string
    mov rdi, rdx        ; key to find
    
find_key_loop:
    mov al, byte ptr [rsi]
    test al, al
    jz key_not_found
    
    cmp al, '"'
    jne next_char
    
    ; Found quote, check if this is our key
    inc rsi
    mov rbx, rsi        ; save position
    mov rcx, rdi        ; key name
    
check_key:
    mov al, byte ptr [rsi]
    mov dl, byte ptr [rcx]
    
    cmp al, '"'
    je check_match
    
    cmp al, dl
    jne not_this_key
    
    inc rsi
    inc rcx
    jmp check_key
    
check_match:
    cmp byte ptr [rcx], 0
    je found_key
    
not_this_key:
    mov rsi, rbx
    
next_char:
    inc rsi
    jmp find_key_loop
    
found_key:
    ; Skip to colon and return value position
    inc rsi
find_colon:
    mov al, byte ptr [rsi]
    test al, al
    jz key_not_found
    cmp al, ':'
    je found_colon
    inc rsi
    jmp find_colon
    
found_colon:
    inc rsi
skip_whitespace:
    mov al, byte ptr [rsi]
    cmp al, ' '
    je skip_ws
    cmp al, 9  ; tab
    je skip_ws
    jmp return_value
skip_ws:
    inc rsi
    jmp skip_whitespace
    
return_value:
    mov rax, rsi
    jmp find_done
    
key_not_found:
    xor rax, rax
    
find_done:
    pop rbx
    pop rdi
    pop rsi
    ret
find_json_key ENDP

; Alias for underscore naming convention
_find_json_key EQU find_json_key

;==========================================================================
; PUBLIC: parse_json_int - Parse integer from JSON value (hotpatchable)
; INPUT: rcx = value_string
; OUTPUT: rax = parsed integer
;==========================================================================
PUBLIC parse_json_int
PUBLIC _parse_json_int
ALIGN 16
parse_json_int PROC
    push rsi
    
    mov rsi, rcx
    xor rax, rax        ; result
    xor rdx, rdx        ; digit
    mov r8, 10          ; base
    xor r9, r9          ; sign (0=positive)
    
    ; Check for negative
    mov dl, byte ptr [rsi]
    cmp dl, '-'
    jne parse_digits
    inc rsi
    mov r9, 1
    
parse_digits:
    mov dl, byte ptr [rsi]
    cmp dl, '0'
    jb parse_done
    cmp dl, '9'
    ja parse_done
    
    sub dl, '0'
    imul rax, r8
    add rax, rdx
    inc rsi
    jmp parse_digits
    
parse_done:
    test r9, r9
    jz positive
    neg rax
    
positive:
    pop rsi
    ret
parse_json_int ENDP

; Alias for underscore naming convention
_parse_json_int EQU parse_json_int

;==========================================================================
; PUBLIC: parse_json_bool - Parse boolean from JSON value (hotpatchable)
; INPUT: rcx = value_string
; OUTPUT: rax = 1 for true, 0 for false
;==========================================================================
PUBLIC parse_json_bool
PUBLIC _parse_json_bool
ALIGN 16
parse_json_bool PROC
    push rsi
    
    mov rsi, rcx
    mov eax, dword ptr [rsi]
    
    ; Check for "true"
    cmp eax, 'eurt'     ; "true" reversed (little-endian)
    je return_true
    
    ; Otherwise false
    xor rax, rax
    jmp bool_done
    
return_true:
    mov rax, 1
    
bool_done:
    pop rsi
    ret
parse_json_bool ENDP

; Alias for underscore naming convention
_parse_json_bool EQU parse_json_bool

;==========================================================================
; HELPER: int_to_string - Convert integer to string
; INPUT: rcx = buffer, edx = value
; OUTPUT: none
;==========================================================================
int_to_string PROC
    ;==========================================================================
    ; HELPER: int_to_string_simple - Convert integer to string (simple version)
    ; INPUT: rcx = buffer, edx = value
    ; OUTPUT: none
    ;==========================================================================
    PUBLIC _copy_string
    ALIGN 16
    _copy_string PROC
        ; rcx = source, rdx = destination, r8d = max length
        ; Returns: eax = bytes copied
        push rsi
        push rdi
    
        mov rsi, rcx
        mov rdi, rdx
        xor eax, eax
    
    copy_loop:
        cmp eax, r8d
        jge copy_max
    
        mov cl, BYTE PTR [rsi + rax]
        mov BYTE PTR [rdi + rax], cl
        test cl, cl
        jz copy_done
    
        inc eax
        jmp copy_loop
    
    copy_max:
        mov BYTE PTR [rdi + rax], 0
    
    copy_done:
        pop rdi
        pop rsi
        ret
    _copy_string ENDP
    push rsi
    push rdi
    push rbx
    
    mov rdi, rcx        ; buffer
    mov eax, edx        ; value
    mov ebx, 10         ; divisor
    
    ; Handle negative
    test eax, eax
    jns positive_value
    neg eax
    mov byte ptr [rdi], '-'
    inc rdi
    
positive_value:
    ; Convert digits (reverse order)
    mov rsi, rdi
    
convert_loop:
    xor edx, edx
    div ebx
    add dl, '0'
    mov byte ptr [rsi], dl
    inc rsi
    test eax, eax
    jnz convert_loop
    
    ; Null terminate
    mov byte ptr [rsi], 0
    
    ; Reverse the string
    dec rsi
reverse_loop:
    cmp rdi, rsi
    jge reverse_done
    
    mov al, byte ptr [rdi]
    mov dl, byte ptr [rsi]
    mov byte ptr [rdi], dl
    mov byte ptr [rsi], al
    
    inc rdi
    dec rsi
    jmp reverse_loop
    
reverse_done:
    pop rbx
    pop rdi
    pop rsi
    ret
int_to_string ENDP

;==========================================================================
; HELPER: int_to_string_simple - Convert integer to string (simple version)
; INPUT: rcx = buffer, edx = value
; OUTPUT: none
;==========================================================================
int_to_string_simple PROC
    push rsi
    push rdi
    push rbx
    
    mov rdi, rcx        ; buffer
    mov eax, edx        ; value
    mov ebx, 10         ; divisor
    
    ; Handle negative
    test eax, eax
    jns positive_value
    neg eax
    mov byte ptr [rdi], '-'
    inc rdi
    
positive_value:
    ; Convert digits (reverse order)
    mov rsi, rdi
    
convert_loop:
    xor edx, edx
    div ebx
    add dl, '0'
    mov byte ptr [rsi], dl
    inc rsi
    test eax, eax
    jnz convert_loop
    
    ; Null terminate
    mov byte ptr [rsi], 0
    
    ; Reverse the string
    dec rsi
reverse_loop:
    cmp rdi, rsi
    jge reverse_done
    
    mov al, byte ptr [rdi]
    mov dl, byte ptr [rsi]
    mov byte ptr [rdi], dl
    mov byte ptr [rsi], al
    
    inc rdi
    dec rsi
    jmp reverse_loop
    
reverse_done:
    pop rbx
    pop rdi
    pop rsi
    ret
int_to_string_simple ENDP

;==========================================================================
; PUBLIC: json_helpers_get_version - Get current version (for hotpatch tracking)
; OUTPUT: eax = version number
;==========================================================================
PUBLIC json_helpers_get_version
ALIGN 16
json_helpers_get_version PROC
    mov eax, json_helpers_version
    ret
json_helpers_get_version ENDP

;==========================================================================
; PUBLIC: json_helpers_increment_hotpatch - Track hotpatch operations
; OUTPUT: eax = new hotpatch count
;==========================================================================
PUBLIC json_helpers_increment_hotpatch
ALIGN 16
json_helpers_increment_hotpatch PROC
    mov eax, hotpatch_count
    inc eax
    mov hotpatch_count, eax
    ret
json_helpers_increment_hotpatch ENDP

;==========================================================================
; CONSTANTS
;==========================================================================
GENERIC_WRITE           EQU 40000000h
GENERIC_READ            EQU 80000000h
CREATE_ALWAYS           EQU 2
OPEN_EXISTING           EQU 3
FILE_ATTRIBUTE_NORMAL   EQU 80h
INVALID_HANDLE_VALUE    EQU -1
FILE_SHARE_READ         EQU 1

;==========================================================================
; EXTERNAL WINDOWS API
;==========================================================================
EXTERN CreateFileA:PROC
EXTERN WriteFile:PROC
EXTERN ReadFile:PROC
EXTERN CloseHandle:PROC

END
