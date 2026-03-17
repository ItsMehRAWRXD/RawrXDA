; ═══════════════════════════════════════════════════════════════════════════════
; MACRO ARGUMENT SUBSTITUTION ENGINE
; RawrXD Assembler - Phase 2: Parameterized Macro Expansion
; ═══════════════════════════════════════════════════════════════════════════════

OPTION CASEMAP:NONE

EXTERN VirtualAlloc:PROC

MEM_COMMIT      EQU 1000h
MEM_RESERVE     EQU 2000h
PAGE_READWRITE  EQU 4

; ═══════════════════════════════════════════════════════════════════════════════
; ParseMacroArguments - Extract argument list from invocation
; Stubbed for now (returns 0 args)
ParseMacroArguments PROC
    xor eax, eax
    xor edx, edx
    ret
ParseMacroArguments ENDP
    mov [r13 + rax].ArgVec.pData, rsi
    
    ; Scan to comma or end, respecting nesting
    xor r10d, r10d                      ; Paren depth
    xor r11d, r11d                      ; Bracket depth
    
@@arg_scan:
    cmp rsi, rdi
    jae @@arg_end_of_list
    
    movzx eax, byte ptr [rsi]
    cmp al, '('
    jne @@check_rparen
    inc r10d
    inc rsi
    jmp @@arg_scan
    
@@check_rparen:
    cmp al, ')'
    jne @@check_lbracket
    test r10d, r10d
    jz @@arg_end_of_list
    dec r10d
    inc rsi
    jmp @@arg_scan
    
@@check_lbracket:
    cmp al, '['
    jne @@check_rbracket
    inc r11d
    inc rsi
    jmp @@arg_scan
    
@@check_rbracket:
    cmp al, ']'
    jne @@check_comma
    dec r11d
    inc rsi
    jmp @@arg_scan
    
@@check_comma:
    cmp al, ','
    jne @@not_comma
    test r10d, r10d
    jnz @@comma_inside
    test r11d, r11d
    jnz @@comma_inside
    
    ; Top-level comma - end this arg
    mov rax, rsi
    imul rcx, rbx, SIZEOF ArgVec
    sub rax, [r13 + rcx].ArgVec.pData
    mov dword ptr [r13 + rcx].ArgVec.nLen, eax
    
    inc ebx
    cmp ebx, MACRO_MAX_ARGS
    jae @@too_many_args
    
    inc rsi                             ; Skip comma
    jmp @@skip_ws
    
@@comma_inside:
    inc rsi
    jmp @@arg_scan
    
@@not_comma:
    inc rsi
    jmp @@arg_scan
    
@@arg_end_of_list:
    ; Store final arg length
    mov rax, rsi
    imul rcx, rbx, SIZEOF ArgVec
    sub rax, [r13 + rcx].ArgVec.pData
    mov dword ptr [r13 + rcx].ArgVec.nLen, eax
    inc ebx
    
@@args_done:
    ; Verify arg count
    cmp ebx, [r12].MacroEntry.min_required
    jb @@too_few_args
    cmp ebx, [r12].MacroEntry.param_count
    ja @@too_many_args
    
    mov rax, r13
    mov edx, ebx
    jmp @@exit
    
@@too_few_args:
    lea rcx, szErrMacroArgs
    mov edx, [r12].MacroEntry.min_required
    mov r8d, ebx
    call report_error
    xor eax, eax
    jmp @@exit
    
@@too_many_args:
    lea rcx, szErrMacroArgs
    mov edx, [r12].MacroEntry.param_count
    mov r8d, ebx
    call report_error
    xor eax, eax
    
@@exit:
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    mov rsp, rbp
    pop rbp
    ret
ParseMacroArguments ENDP

; ═══════════════════════════════════════════════════════════════════════════════
; ExpandMacroWithArgs - Main macro expansion with argument substitution
; RCX = MacroEntry ptr
; RDX = ArgVec array ptr
; R8D = Argument count
; Output: RAX = expanded text ptr, RDX = length
; ═══════════════════════════════════════════════════════════════════════════════
ExpandMacroWithArgs PROC FRAME
    push rbp
    .pushreg rbp
    mov rbp, rsp
    .setframe rbp, 0
    sub rsp, 2048
    .allocstack 2048
    push rbx
    .pushreg rbx
    push rsi
    .pushreg rsi
    push rdi
    .pushreg rdi
    push r12
    .pushreg r12
    push r13
    .pushreg r13
    push r14
    .pushreg r14
    push r15
    .pushreg r15
    .endprolog
    
    mov r12, rcx                        ; MacroEntry
    mov r13, rdx                        ; ArgVec ptr
    mov r14d, r8d                       ; Arg count
    mov rsi, [r12].MacroEntry.body_ptr   ; Body text start
    mov r15d, [r12].MacroEntry.body_len  ; Body length
    
    ; Check recursion depth
    cmp g_expand_depth, MACRO_MAX_DEPTH
    jae @@recursion_error
    inc g_expand_depth
    mov rbx, g_expand_buffer             ; Output buffer
    
    ; Walk body character by character, substituting %N references
@@expand_loop:
    cmp r15d, 0
    je @@expand_done
    
    movzx eax, byte ptr [rsi]
    cmp al, '%'
    jne @@copy_char
    
    ; Potential parameter reference
    cmp r15d, 1
    je @@copy_char                      ; Lone % at end
    
    movzx eax, byte ptr [rsi+1]
    
    ; Check for %0-%9 (parameter reference)
    cmp al, '0'
    jb @@check_special
    cmp al, '9'
    ja @@check_special
    
    ; Parameter reference %1-%9
    sub al, '0'
    movzx rcx, al
    
    cmp al, 0
    je @@param_count                    ; %0 = parameter count
    
    cmp ecx, r14d
    ja @@param_missing
    
    ; Substitute with argument
    dec ecx
    imul rax, rcx, SIZEOF ArgVec
    mov rdx, [r13 + rax].ArgVec.pData
    mov eax, [r13 + rax].ArgVec.nLen
    call emit_string
    
    add rsi, 2
    sub r15d, 2
    jmp @@expand_loop
    
@@check_special:
    cmp al, '*'
    je @@param_all                      ; %* = all args
    cmp al, '='
    je @@param_count                    ; %= = arg count (alternate)
    cmp al, '%'
    je @@escaped_percent                ; %% = literal %
    
    ; Unknown escape
    lea rcx, szErrBadSubst
    movzx edx, al
    call report_error
    inc rsi
    dec r15d
    jmp @@expand_loop
    
@@copy_char:
    mov [rbx], al
    inc rbx
    inc rsi
    dec r15d
    jmp @@expand_loop
    
@@param_count:
    ; Emit argument count as decimal
    mov eax, r14d
    call emit_decimal
    add rsi, 2
    sub r15d, 2
    jmp @@expand_loop
    
@@param_all:
    ; Emit all arguments separated by commas
    xor ecx, ecx
@@concat_loop:
    cmp ecx, r14d
    jae @@concat_done
    
    test ecx, ecx
    jz @@no_leading_comma
    mov byte ptr [rbx], ','
    inc rbx
@@no_leading_comma:
    push rcx
    imul rax, rcx, SIZEOF ArgVec
    mov rdx, [r13 + rax].ArgVec.pData
    mov eax, [r13 + rax].ArgVec.nLen
    call emit_string
    pop rcx
    
    inc ecx
    jmp @@concat_loop
@@concat_done:
    add rsi, 2
    sub r15d, 2
    jmp @@expand_loop
    
@@escaped_percent:
    mov byte ptr [rbx], '%'
    inc rbx
    add rsi, 2
    sub r15d, 2
    jmp @@expand_loop
    
@@param_missing:
    ; Check for default value
    mov rax, [r12].MacroEntry.defaults_ptr
    test rax, rax
    jz @@error_missing_arg
    
    ; Extract default for this param
    dec ecx
    mov rdx, [rax + rcx*8]
    test rdx, rdx
    jz @@error_missing_arg
    
    ; Emit default value
    push rdx
    mov rcx, rdx
    call strlen
    mov rdx, [rsp]
    pop rax                             ; RDX has ptr, RAX has length
    xchg rax, rdx                       ; RDX=ptr, EAX=len
    call emit_string
    
    add rsi, 2
    sub r15d, 2
    jmp @@expand_loop
    
@@error_missing_arg:
    lea rcx, szErrMacroArgs
    mov edx, [r12].MacroEntry.min_required
    mov r8d, r14d
    call report_error
    jmp @@expand_done
    
@@recursion_error:
    lea rcx, szErrMacroRec
    mov edx, g_expand_depth
    call report_error
    
@@expand_done:
    ; Calculate output length
    mov rax, rbx
    sub rax, g_expand_buffer            ; Result length in RAX
    
    ; Decrement recursion depth
    dec g_expand_depth
    
    mov rdx, rax                        ; Length
    mov rax, g_expand_buffer            ; Buffer

    add rsp, 2048
    pop r15
    pop r14
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret
ExpandMacroWithArgs ENDP

; ═══════════════════════════════════════════════════════════════════════════════
; Helper: emit_string
; RDX = string ptr, EAX = length
; Appends to expansion buffer
; ═══════════════════════════════════════════════════════════════════════════════
emit_string PROC
    push rcx
    push rsi
    push rdi
    mov rsi, rdx
    mov ecx, eax
    mov rdi, rbx
    
    rep movsb
    mov rbx, rdi                        ; Update output position
    pop rdi
    pop rsi
    pop rcx
    ret
emit_string ENDP

; ═══════════════════════════════════════════════════════════════════════════════
; Helper: emit_decimal
; EAX = value to emit as decimal
; ═══════════════════════════════════════════════════════════════════════════════
emit_decimal PROC
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push r8
    
    mov ecx, 10
    mov rsi, rsp
    sub rsp, 24                         ; Temp buffer for digits
    mov rdi, rsi
    
    ; Convert to string (reversed)
@@cvt_loop:
    xor edx, edx
    div ecx
    add dl, '0'
    mov [rdi], dl
    inc rdi
    test eax, eax
    jnz @@cvt_loop
    
    ; Reverse and emit
    dec rdi
@@emit_loop:
    cmp rdi, rsi
    jb @@emit_done
    mov al, [rdi]
    mov [rbx], al
    inc rbx
    dec rdi
    jmp @@emit_loop
    
@@emit_done:
    add rsp, 24
    pop r8
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    ret
emit_decimal ENDP

; Simple report_error stub (no-op)
report_error PROC
    ret
report_error ENDP

; strlen alias to helper
strlen PROC
    jmp strlen_helper
strlen ENDP

; ═══════════════════════════════════════════════════════════════════════════════
; Helper: strlen
; RDX = string ptr
; Output: EAX = length
; ═══════════════════════════════════════════════════════════════════════════════
strlen_helper PROC
    xor eax, eax
@@loop:
    cmp byte ptr [rdx + rax], 0
    je @@done
    inc eax
    jmp @@loop
@@done:
    ret
strlen_helper ENDP

END
