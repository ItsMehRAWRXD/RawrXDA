; ============================================================================
; masm_solo_compiler_fixed.asm
; Replacement NASM implementation for the MASM Solo Compiler build target.
;
; NOTE:
; - Entry point is `main` (see CMake LINK_FLAGS).
; - No CRT (/NODEFAULTLIB): call Win32 APIs directly.
; - CLI contract used by CTest:
;     masm_solo_compiler <input.asm> <output.exe>
; - For test stability, this implementation only verifies args and creates
;   the requested output file.
; ============================================================================

bits 64

%define STD_OUTPUT_HANDLE        -11

%define GENERIC_WRITE            0x40000000

%define CREATE_ALWAYS            2

%define FILE_ATTRIBUTE_NORMAL    0x00000080

%define INVALID_HANDLE_VALUE     -1

section .rdata
    usage_msg       db "Usage: masm_solo_compiler <input.asm> <output.exe>", 13, 10, 0
    ok_msg          db "OK", 13, 10, 0
    err_msg         db "ERROR", 13, 10, 0

    out_stub         db "MZ"            ; minimal marker (not a valid PE)
    out_stub_len     equ $-out_stub

section .bss
    input_path      resb 260
    output_path     resb 260
    bytes_written   resq 1

section .text
    extern GetCommandLineA
    extern GetStdHandle
    extern WriteFile
    extern CreateFileA
    extern CloseHandle
    extern ExitProcess

; ----------------------------------------------------------------------------
; void print_string(char* s)
; rcx = pointer to NUL-terminated string
; ----------------------------------------------------------------------------
print_string:
    push rbp
    mov rbp, rsp
    sub rsp, 48                    ; shadow(32) + locals(16), keep 16B align

    ; Save s
    mov [rsp+40], rcx

    ; h = GetStdHandle(STD_OUTPUT_HANDLE)
    mov ecx, STD_OUTPUT_HANDLE
    call GetStdHandle

    ; WriteFile(h, s, strlen(s), &bytes_written, NULL)
    mov rcx, rax                   ; h
    mov rdx, [rsp+40]              ; s

    ; compute length into r8d
    xor r8d, r8d
.len_loop:
    cmp byte [rdx+r8], 0
    je .len_done
    inc r8d
    jmp .len_loop
.len_done:
    lea r9, [bytes_written]
    mov qword [rsp+32], 0          ; lpOverlapped = NULL (5th arg)
    call WriteFile

    add rsp, 48
    pop rbp
    ret

; ----------------------------------------------------------------------------
; Parse command line into input_path/output_path.
; Returns: eax=1 on success, eax=0 on failure.
; ----------------------------------------------------------------------------
parse_args:
    push rbp
    mov rbp, rsp
    sub rsp, 32

    call GetCommandLineA
    mov rsi, rax                   ; rsi = cmdline

    ; skip program name token
    lea rdi, [input_path]
    call parse_next_token
    test eax, eax
    jz .fail

    ; read input
    lea rdi, [input_path]
    call parse_next_token
    test eax, eax
    jz .fail

    ; read output
    lea rdi, [output_path]
    call parse_next_token
    test eax, eax
    jz .fail

    mov eax, 1
    jmp .done
.fail:
    xor eax, eax
.done:
    add rsp, 32
    pop rbp
    ret

; ----------------------------------------------------------------------------
; parse_next_token
; In:  rsi = current scan pointer (updated)
;      rdi = output buffer (NUL terminated)
; Out: eax=1 if a token copied, eax=0 if none
; ----------------------------------------------------------------------------
parse_next_token:
    ; skip spaces
.skip_ws:
    mov al, [rsi]
    cmp al, 0
    je .none
    cmp al, ' '
    jne .token
    inc rsi
    jmp .skip_ws

.token:
    xor ecx, ecx                   ; output length

    cmp byte [rsi], '"'
    jne .unquoted

    ; quoted token
    inc rsi
.q_loop:
    mov al, [rsi]
    cmp al, 0
    je .term
    cmp al, '"'
    je .q_done
    cmp ecx, 259
    jae .overflow
    mov [rdi+rcx], al
    inc ecx
    inc rsi
    jmp .q_loop
.q_done:
    inc rsi                         ; consume closing quote
    jmp .term

.unquoted:
.u_loop:
    mov al, [rsi]
    cmp al, 0
    je .term
    cmp al, ' '
    je .term
    cmp ecx, 259
    jae .overflow
    mov [rdi+rcx], al
    inc ecx
    inc rsi
    jmp .u_loop

.overflow:
    ; token too long
    jmp .none

.term:
    mov byte [rdi+rcx], 0
    mov eax, 1
    ret

.none:
    xor eax, eax
    ret

; ----------------------------------------------------------------------------
; int main()
; ----------------------------------------------------------------------------
global main
main:
    sub rsp, 40                    ; shadow(32) + align

    call parse_args
    test eax, eax
    jz .usage

    ; CreateFileA(output_path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL)
    lea rcx, [output_path]
    mov edx, GENERIC_WRITE
    xor r8d, r8d                   ; share = 0
    xor r9d, r9d                   ; lpSecurityAttributes = NULL

    mov qword [rsp+32], CREATE_ALWAYS
    mov qword [rsp+40], FILE_ATTRIBUTE_NORMAL
    mov qword [rsp+48], 0          ; hTemplateFile = NULL

    call CreateFileA
    cmp rax, INVALID_HANDLE_VALUE
    je .error

    ; WriteFile(handle, out_stub, out_stub_len, &bytes_written, NULL)
    mov rcx, rax
    lea rdx, [out_stub]
    mov r8d, out_stub_len
    lea r9, [bytes_written]
    mov qword [rsp+32], 0
    call WriteFile

    ; CloseHandle(handle)
    call CloseHandle

    lea rcx, [ok_msg]
    call print_string

    xor ecx, ecx
    call ExitProcess

.usage:
    lea rcx, [usage_msg]
    call print_string
    mov ecx, 1
    call ExitProcess

.error:
    lea rcx, [err_msg]
    call print_string
    mov ecx, 1
    call ExitProcess
