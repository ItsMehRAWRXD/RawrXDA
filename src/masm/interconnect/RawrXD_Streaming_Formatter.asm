; ═══════════════════════════════════════════════════════════════════════════════
; RawrXD_Streaming_Formatter.asm
; Stub implementation for Streaming Formatter
; ═══════════════════════════════════════════════════════════════════════════════

OPTION DOTNAME
OPTION CASEMAP:NONE

; ─── Cross-module symbol resolution ───
INCLUDE rawrxd_master.inc

OPTION WIN64:3

include \masm64\include64\windows.inc
include \masm64\include64\kernel32.inc

.CODE

StreamFormatter_WriteToken PROC FRAME
    ; Format and write a decoded token to the output stream
    ; RCX = Context pointer (stream state)
    ;   +0: QWORD output_buffer_ptr
    ;   +8: DWORD buffer_size
    ;   +12: DWORD write_offset
    ;   +16: QWORD callback_fn (optional)
    ; RDX = TokenData pointer (UTF-8 string)
    ; R8 = Length in bytes
    ; Returns: RAX = bytes written, 0 on error
    push rbx
    .pushreg rbx
    push r12
    .pushreg r12
    push r13
    .pushreg r13
    sub rsp, 40
    .allocstack 40
    .endprolog
    
    mov rbx, rcx                     ; context
    mov r12, rdx                     ; token data
    mov r13d, r8d                    ; length
    
    test rbx, rbx
    jz @@swt_fail
    test r12, r12
    jz @@swt_fail
    test r13d, r13d
    jz @@swt_fail
    
    ; Get output buffer and check capacity
    mov rcx, QWORD PTR [rbx]         ; output_buffer
    test rcx, rcx
    jz @@swt_fail
    mov eax, DWORD PTR [rbx+8]       ; buffer_size
    mov edx, DWORD PTR [rbx+12]      ; write_offset
    
    ; Check if enough space: offset + length < size
    lea r8d, [edx + r13d]
    cmp r8d, eax
    jae @@swt_fail                   ; buffer full
    
    ; Copy token bytes to output at write_offset
    add rcx, rdx                     ; dst = buffer + offset
    mov rsi, r12                     ; src = token data
    mov edx, r13d                    ; count
@@swt_copy:
    test edx, edx
    jz @@swt_copied
    mov al, BYTE PTR [rsi]
    mov BYTE PTR [rcx], al
    inc rsi
    inc rcx
    dec edx
    jmp @@swt_copy
    
@@swt_copied:
    ; Update write offset
    add DWORD PTR [rbx+12], r13d
    
    ; Null-terminate
    mov BYTE PTR [rcx], 0
    
    ; Call streaming callback if set
    mov rcx, QWORD PTR [rbx+16]
    test rcx, rcx
    jz @@swt_done
    ; callback(token_data, length)
    mov rdx, r12
    mov r8d, r13d
    call rcx
    
@@swt_done:
    mov eax, r13d                    ; return bytes written
    add rsp, 40
    pop r13
    pop r12
    pop rbx
    ret
    
@@swt_fail:
    xor eax, eax
    add rsp, 40
    pop r13
    pop r12
    pop rbx
    ret
StreamFormatter_WriteToken ENDP

PUBLIC StreamFormatter_WriteToken

END