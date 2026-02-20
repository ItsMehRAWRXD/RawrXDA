; ═══════════════════════════════════════════════════════════════════════════════
; RawrXD_JSON_Parser.asm
; Stub implementation for JSON Parser
; ═══════════════════════════════════════════════════════════════════════════════

OPTION DOTNAME
OPTION CASEMAP:NONE

; ─── Cross-module symbol resolution ───
INCLUDE rawrxd_master.inc

OPTION WIN64:3

include \masm64\include64\windows.inc
include \masm64\include64\kernel32.inc

.CODE

; Add any exports if needed later
JsonParser_Parse PROC FRAME
    ; Parse a JSON string into key-value token array
    ; RCX = input JSON string (null-terminated)
    ; RDX = output token buffer (array of JSON_TOKEN structs)
    ;   Each JSON_TOKEN: +0 DWORD type (0=obj, 1=arr, 2=str, 3=num, 4=bool, 5=null)
    ;                    +8 QWORD key_ptr, +16 DWORD key_len
    ;                    +24 QWORD val_ptr, +32 DWORD val_len
    ; R8 = max tokens
    ; Returns: RAX = number of tokens parsed, 0 on error
    push rbx
    .pushreg rbx
    push r12
    .pushreg r12
    push r13
    .pushreg r13
    push r14
    .pushreg r14
    push r15
    .pushreg r15
    sub rsp, 40
    .allocstack 40
    .endprolog
    
    mov rbx, rcx                     ; JSON input
    mov r12, rdx                     ; token output
    mov r13d, r8d                    ; max tokens
    xor r14d, r14d                   ; token count = 0
    
    test rbx, rbx
    jz @@jp_fail
    test r12, r12
    jz @@jp_fail
    test r13d, r13d
    jz @@jp_fail
    
    mov r15, rbx                     ; cursor
    
@@jp_skip_ws:
    movzx eax, BYTE PTR [r15]
    cmp al, ' '
    je @@jp_advance
    cmp al, 9                        ; tab
    je @@jp_advance
    cmp al, 10                       ; LF
    je @@jp_advance
    cmp al, 13                       ; CR
    je @@jp_advance
    jmp @@jp_dispatch
@@jp_advance:
    inc r15
    jmp @@jp_skip_ws
    
@@jp_dispatch:
    cmp al, 0                        ; end of string
    je @@jp_done
    cmp al, '{'                      ; object start
    je @@jp_object
    cmp al, '"'                      ; string
    je @@jp_string
    cmp al, '}'                      ; object end
    je @@jp_skip_char
    cmp al, ','                      ; separator
    je @@jp_skip_char
    cmp al, ':'                      ; key-value separator
    je @@jp_skip_char
    ; Number or literal
    jmp @@jp_value
    
@@jp_object:
    ; Emit object token
    cmp r14d, r13d
    jae @@jp_done
    mov rax, r14
    imul rax, 40                     ; sizeof(JSON_TOKEN)
    mov DWORD PTR [r12 + rax], 0     ; type = object
    mov QWORD PTR [r12 + rax + 8], r15  ; position
    inc r14d
    
@@jp_skip_char:
    inc r15
    jmp @@jp_skip_ws
    
@@jp_string:
    ; Parse "key" or "value"
    inc r15                          ; skip opening quote
    mov rcx, r15                     ; string start
@@jp_str_scan:
    movzx eax, BYTE PTR [r15]
    cmp al, 0
    je @@jp_done                     ; unterminated string
    cmp al, '\\'
    je @@jp_str_escape
    cmp al, '"'
    je @@jp_str_end
    inc r15
    jmp @@jp_str_scan
@@jp_str_escape:
    add r15, 2                       ; skip escaped char
    jmp @@jp_str_scan
@@jp_str_end:
    ; RCX = start, R15 = end quote
    cmp r14d, r13d
    jae @@jp_done
    mov rax, r14
    imul rax, 40
    mov DWORD PTR [r12 + rax], 2     ; type = string
    mov QWORD PTR [r12 + rax + 8], rcx  ; key/val ptr
    mov rdx, r15
    sub rdx, rcx
    mov DWORD PTR [r12 + rax + 16], edx ; length
    inc r14d
    inc r15                          ; skip closing quote
    jmp @@jp_skip_ws
    
@@jp_value:
    ; Parse number, bool, or null literal
    mov rcx, r15
@@jp_val_scan:
    movzx eax, BYTE PTR [r15]
    cmp al, ','
    je @@jp_val_end
    cmp al, '}'
    je @@jp_val_end
    cmp al, ']'
    je @@jp_val_end
    cmp al, ' '
    je @@jp_val_end
    cmp al, 0
    je @@jp_val_end
    inc r15
    jmp @@jp_val_scan
@@jp_val_end:
    cmp r14d, r13d
    jae @@jp_done
    mov rax, r14
    imul rax, 40
    mov DWORD PTR [r12 + rax], 3     ; type = number/literal
    mov QWORD PTR [r12 + rax + 24], rcx ; val_ptr
    mov rdx, r15
    sub rdx, rcx
    mov DWORD PTR [r12 + rax + 32], edx ; val_len
    inc r14d
    jmp @@jp_skip_ws
    
@@jp_done:
    mov eax, r14d
    add rsp, 40
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    ret
    
@@jp_fail:
    xor eax, eax
    add rsp, 40
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    ret
JsonParser_Parse ENDP

PUBLIC JsonParser_Parse

END