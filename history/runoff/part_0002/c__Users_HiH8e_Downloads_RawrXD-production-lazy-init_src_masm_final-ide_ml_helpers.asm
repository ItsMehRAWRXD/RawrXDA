;==========================================================================
; ml_helpers.asm - Helper functions for Model Loader (ml_masm.asm)
;==========================================================================

option casemap:none

include windows.inc
includelib kernel32.lib

.code

;==========================================================================
; mem_search_ascii(base: rcx, len: rdx, key: r8) -> ptr (rax)
; Searches for an ASCII key in a memory block.
;==========================================================================
PUBLIC mem_search_ascii
ALIGN 16
mem_search_ascii PROC
    push rbx
    push rsi
    push rdi
    
    mov rsi, rcx        ; base
    mov rbx, rdx        ; len
    mov rdi, r8         ; key
    
    ; Get key length
    xor rdx, rdx
key_len_loop:
    cmp byte ptr [rdi + rdx], 0
    je key_len_done
    inc rdx
    jmp key_len_loop
key_len_done:
    test rdx, rdx
    jz search_fail
    
    mov r10, rdx        ; r10 = key length
    
    ; Search loop
    xor rcx, rcx        ; current offset
search_loop:
    mov rax, rbx
    sub rax, r10
    cmp rcx, rax
    ja search_fail
    
    ; Compare key at current position
    push rcx
    push rsi
    push rdi
    mov rdx, r10
    mov r11, rcx        ; save current offset
compare_loop:
    mov al, [rsi + r11]
    mov ah, [rdi]
    cmp al, ah
    jne compare_fail
    inc r11
    inc rdi
    dec rdx
    jnz compare_loop
    
    ; Found!
    pop rdi
    pop rsi
    pop rcx
    lea rax, [rsi + rcx]
    jmp search_done
    
compare_fail:
    pop rdi
    pop rsi
    pop rcx
    inc rcx
    jmp search_loop
    
search_fail:
    xor rax, rax
    
search_done:
    pop rdi
    pop rsi
    pop rbx
    ret
mem_search_ascii ENDP

;==========================================================================
; parse_uint_after_key(ptr_after_key: rcx) -> val (rax)
; Parses an unsigned integer following a key (skips non-digits).
;==========================================================================
PUBLIC parse_uint_after_key
ALIGN 16
parse_uint_after_key PROC
    mov rsi, rcx
    xor rax, rax
    
    ; Skip non-digits
skip_non_digits:
    mov dl, [rsi]
    test dl, dl
    jz parse_done
    cmp dl, '0'
    jb next_char
    cmp dl, '9'
    ja next_char
    jmp start_parse
next_char:
    inc rsi
    jmp skip_non_digits
    
start_parse:
parse_loop:
    mov dl, [rsi]
    cmp dl, '0'
    jb parse_done
    cmp dl, '9'
    ja parse_done
    
    sub dl, '0'
    movzx rdx, dl
    imul rax, 10
    add rax, rdx
    inc rsi
    jmp parse_loop
    
parse_done:
    ret
parse_uint_after_key ENDP

;==========================================================================
; copy_ascii_value_after_key(ptr_after_key: rcx, out: rdx, max: r8d) -> len (rax)
; Copies an ASCII value following a key (skips whitespace/colons).
;==========================================================================
PUBLIC copy_ascii_value_after_key
ALIGN 16
copy_ascii_value_after_key PROC
    push rsi
    push rdi
    
    mov rsi, rcx
    mov rdi, rdx
    mov r10d, r8d       ; max len
    xor r9, r9          ; current len
    
    ; Skip whitespace and colons
skip_prefix:
    mov al, [rsi]
    test al, al
    jz copy_done
    cmp al, ' '
    je next_prefix
    cmp al, ':'
    je next_prefix
    cmp al, 9           ; tab
    je next_prefix
    jmp start_copy
next_prefix:
    inc rsi
    jmp skip_prefix
    
start_copy:
copy_loop:
    mov al, [rsi]
    test al, al
    jz copy_done
    cmp al, ' '
    je copy_done
    cmp al, 13          ; CR
    je copy_done
    cmp al, 10          ; LF
    je copy_done
    
    cmp r9d, r10d
    jae copy_done
    
    mov [rdi], al
    inc rsi
    inc rdi
    inc r9
    jmp copy_loop
    
copy_done:
    mov byte ptr [rdi], 0
    mov rax, r9
    
    pop rdi
    pop rsi
    ret
copy_ascii_value_after_key ENDP

;==========================================================================
; build_arch_string(out_buf: rcx, arch_struct: rdx)
;==========================================================================
PUBLIC build_arch_string
EXTERN wsprintfA:PROC
ALIGN 16
build_arch_string PROC
    push rbx
    sub rsp, 128 ; plenty of space for params
    
    mov rbx, rdx        ; arch_struct
    mov rdi, rcx        ; out_buf
    
    ; Format: "Model: %s, Layers: %d, Hidden: %d, Heads: %d, Seq: %d, Vocab: %d, Quant: %s"
    lea rdx, szArchFmt
    mov rcx, rdi
    
    ; Parameters for wsprintfA (on stack for x64)
    ; rcx = out_buf, rdx = format, r8 = param1, r9 = param2
    ; param3+ on stack starting at [rsp+32]
    
    lea r8, [rbx]      ; model_name
    mov eax, [rbx + 96] ; num_layers
    mov r9d, eax
    
    mov eax, [rbx + 100] ; hidden_size
    mov [rsp + 32], rax
    mov eax, [rbx + 104] ; num_attention_heads
    mov [rsp + 40], rax
    mov eax, [rbx + 108] ; max_seq_length
    mov [rsp + 48], rax
    mov eax, [rbx + 112] ; vocab_size
    mov [rsp + 56], rax
    lea rax, [rbx + 116] ; quantization_level
    mov [rsp + 64], rax
    
    call wsprintfA
    
    add rsp, 128
    pop rbx
    ret
build_arch_string ENDP

;==========================================================================
; _open_file_for_reading(path: rcx) -> handle (rax)
;==========================================================================
PUBLIC _open_file_for_reading
EXTERN CreateFileA:PROC
GENERIC_READ        EQU 80000000h
FILE_SHARE_READ     EQU 1
OPEN_EXISTING       EQU 3
FILE_ATTRIBUTE_NORMAL EQU 80h
ALIGN 16
_open_file_for_reading PROC
    sub rsp, 64
    mov rdx, GENERIC_READ
    mov r8, FILE_SHARE_READ
    xor r9, r9
    mov QWORD PTR [rsp + 32], OPEN_EXISTING
    mov QWORD PTR [rsp + 40], FILE_ATTRIBUTE_NORMAL
    mov QWORD PTR [rsp + 48], 0
    call CreateFileA
    add rsp, 64
    ret
_open_file_for_reading ENDP

;==========================================================================
; _get_file_size(handle: rcx) -> size (rax)
;==========================================================================
PUBLIC _get_file_size
EXTERN GetFileSizeEx:PROC
ALIGN 16
_get_file_size PROC
    sub rsp, 48
    lea rdx, [rsp + 32] ; PLARGE_INTEGER
    call GetFileSizeEx
    mov rax, [rsp + 32]
    add rsp, 48
    ret
_get_file_size ENDP

;==========================================================================
; ml_helpers_log_append(msg: rcx)
; Appends a message to run.log
;==========================================================================
PUBLIC ml_helpers_log_append
EXTERN WriteFile:PROC
EXTERN CloseHandle:PROC
EXTERN SetFilePointer:PROC
FILE_APPEND_DATA    EQU 4
FILE_SHARE_READ     EQU 1
OPEN_ALWAYS         EQU 4
FILE_ATTRIBUTE_NORMAL EQU 80h
FILE_END            EQU 2

ALIGN 16
ml_helpers_log_append PROC
    push rbx
    push rsi
    sub rsp, 64
    
    mov rsi, rcx ; msg
    
    ; Get length
    xor rbx, rbx
len_loop:
    cmp byte ptr [rsi + rbx], 0
    je len_done
    inc rbx
    jmp len_loop
len_done:
    test rbx, rbx
    jz log_done
    
    ; Open run.log
    lea rcx, szLogFile
    mov rdx, FILE_APPEND_DATA
    mov r8, FILE_SHARE_READ
    xor r9, r9
    mov QWORD PTR [rsp + 32], OPEN_ALWAYS
    mov QWORD PTR [rsp + 40], FILE_ATTRIBUTE_NORMAL
    mov QWORD PTR [rsp + 48], 0
    call CreateFileA
    
    cmp rax, INVALID_HANDLE_VALUE
    je log_done
    mov r12, rax ; hFile
    
    ; Write message
    mov rcx, r12
    mov rdx, rsi
    mov r8, rbx
    lea r9, [rsp + 56] ; bytesWritten
    mov QWORD PTR [rsp + 32], 0
    call WriteFile
    
    ; Write newline
    mov rcx, r12
    lea rdx, szNewline
    mov r8, 2
    lea r9, [rsp + 56]
    mov QWORD PTR [rsp + 32], 0
    call WriteFile
    
    ; Close
    mov rcx, r12
    call CloseHandle
    
log_done:
    add rsp, 64
    pop rsi
    pop rbx
    ret
ml_helpers_log_append ENDP

.data
    szArchFmt BYTE "Model: %s, Layers: %d, Hidden: %d, Heads: %d, Seq: %d, Vocab: %d, Quant: %s", 0
    szLogFile BYTE "run.log", 0
    szNewline BYTE 13, 10, 0

END
