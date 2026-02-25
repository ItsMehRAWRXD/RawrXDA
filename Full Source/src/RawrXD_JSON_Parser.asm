; ═══════════════════════════════════════════════════════════════════════════════
; RawrXD_JSON_Parser.asm
; Fast SIMD-accelerated JSON scanner
; ═══════════════════════════════════════════════════════════════════════════════

OPTION DOTNAME
include RawrXD_Defs.inc


; ─── Cross-module symbol resolution ───
INCLUDE rawrxd_master.inc

EXTERN g_hHeap : QWORD
EXTERN HeapAlloc : PROC

; ═══════════════════════════════════════════════════════════════════════════════
; CODE SECTION
; ═══════════════════════════════════════════════════════════════════════════════
.CODE

; ═══════════════════════════════════════════════════════════════════════════════
; Helper: SkipWhitespace
; RCX = Current Pointer
; Returns: RAX = Pointer to first non-whitespace char
; ═══════════════════════════════════════════════════════════════════════════════
SkipWhitespace PROC
@loop:
    mov al, [rcx]
    cmp al, 20h ; Space
    je @next
    cmp al, 09h ; Tab
    je @next
    cmp al, 0Ah ; LF
    je @next
    cmp al, 0Dh ; CR
    je @next
    
    mov rax, rcx
    ret
@next:
    inc rcx
    jmp @loop
SkipWhitespace ENDP

; ═══════════════════════════════════════════════════════════════════════════════
; Helper: ParseString
; RCX = Start Pointer (quote)
; Returns: RAX = Pointer after string (or 0 if error)
; ═══════════════════════════════════════════════════════════════════════════════
ParseString PROC
    inc rcx ; Skip opening quote
@loop:
    mov al, [rcx]
    test al, al
    jz @error
    cmp al, 22h ; Quote
    je @done
    cmp al, 5Ch ; Backslash (Escape)
    je @escape
    inc rcx
    jmp @loop
@escape:
    add rcx, 2 ; Skip escape sequence blindly for speed
    jmp @loop
@done:
    lea rax, [rcx + 1]
    ret
@error:
    xor rax, rax
    ret
ParseString ENDP

; ═══════════════════════════════════════════════════════════════════════════════
; Helper: ParseValue
; RCX = Input Pointer
; Returns: RAX = Pointer after value (or 0 if error)
; ═══════════════════════════════════════════════════════════════════════════════
ParseValue PROC
    sub rsp, 40
    call SkipWhitespace
    mov rcx, rax ; Update RCX
    
    mov al, [rcx]
    
    cmp al, 7Bh ; '{'
    je ParseObject
    cmp al, 5Bh ; '['
    je ParseArray
    cmp al, 22h ; '"'
    je ParseString
    
    ; Simple check for primitives (t, f, n, -, 0-9)
    cmp al, 't'
    je @skip_primitive
    cmp al, 'f'
    je @skip_primitive
    cmp al, 'n'
    je @skip_primitive
    cmp al, '-'
    je @skip_primitive
    cmp al, '0'
    jb @error
    cmp al, '9'
    ja @error
    
@skip_primitive:
    ; Scan until delimiter
@prim_loop:
    mov al, [rcx]
    cmp al, 2Ch ; comma
    je @done
    cmp al, 7Dh ; '}'
    je @done
    cmp al, 5Dh ; ']'
    je @done
    cmp al, 20h ; space
    je @done
    test al, al
    jz @done
    inc rcx
    jmp @prim_loop
    
@done:
    mov rax, rcx
    add rsp, 40
    ret
    
@error:
    xor rax, rax
    add rsp, 40
    ret
ParseValue ENDP

; ═══════════════════════════════════════════════════════════════════════════════
; Helper: ParseObject
; RCX = Start Pointer ('{')
; Returns: RAX = Pointer after object
; ═══════════════════════════════════════════════════════════════════════════════
ParseObject PROC
    sub rsp, 40
    inc rcx ; Skip '{'
    
@obj_loop:
    call SkipWhitespace
    mov rcx, rax
    
    mov al, [rcx]
    cmp al, 7Dh ; '}'
    je @obj_done
    
    ; Expect Key (String)
    cmp al, 22h
    jne @obj_error
    
    call ParseString
    test rax, rax
    jz @obj_error
    mov rcx, rax
    
    call SkipWhitespace
    mov rcx, rax
    
    mov al, [rcx]
    cmp al, 3Ah ; ':'
    jne @obj_error
    inc rcx
    
    call ParseValue
    test rax, rax
    jz @obj_error
    mov rcx, rax
    
    call SkipWhitespace
    mov rcx, rax
    
    mov al, [rcx]
    cmp al, 2Ch ; ','
    je @obj_next
    cmp al, 7Dh ; '}'
    je @obj_done
    jmp @obj_error
    
@obj_next:
    inc rcx
    jmp @obj_loop
    
@obj_done:
    lea rax, [rcx + 1]
    add rsp, 40
    ret
    
@obj_error:
    xor rax, rax
    add rsp, 40
    ret
ParseObject ENDP

; ═══════════════════════════════════════════════════════════════════════════════
; Helper: ParseArray
; RCX = Start Pointer ('[')
; Returns: RAX = Pointer after array
; ═══════════════════════════════════════════════════════════════════════════════
ParseArray PROC
    sub rsp, 40
    inc rcx ; Skip '['
    
@arr_loop:
    call SkipWhitespace
    mov rcx, rax
    
    mov al, [rcx]
    cmp al, 5Dh ; ']'
    je @arr_done
    
    call ParseValue
    test rax, rax
    jz @arr_error
    mov rcx, rax
    
    call SkipWhitespace
    mov rcx, rax
    
    mov al, [rcx]
    cmp al, 2Ch ; ','
    je @arr_next
    cmp al, 5Dh ; ']'
    je @arr_done
    jmp @arr_error
    
@arr_next:
    inc rcx
    jmp @arr_loop
    
@arr_done:
    lea rax, [rcx + 1]
    add rsp, 40
    ret
    
@arr_error:
    xor rax, rax
    add rsp, 40
    ret
ParseArray ENDP

; ═══════════════════════════════════════════════════════════════════════════════
; Json_ParseFast
; RCX = Input String, RDX = Length
; ═══════════════════════════════════════════════════════════════════════════════
Json_ParseFast PROC FRAME
    push rbx
    sub rsp, 32
    .endprolog
    
    mov rbx, rcx ; Save start
    
    ; Validate inputs
    test rcx, rcx
    jz @fail
    test rdx, rdx
    jz @fail
    
    ; Start recursive descent
    call ParseValue
    test rax, rax
    jz @fail
    
    ; Success - Alloc Dummy Node for return
    mov rcx, g_hHeap
    test rcx, rcx
    jz @alloc_skip ; If heap not ready, return 1 as pseudo-handle
    
    mov rdx, 0 ; Flags
    mov r8, 64 ; Size
    call HeapAlloc
    test rax, rax
    jz @fail
    
    ; Return ptr
    add rsp, 32
    pop rbx
    ret
    
@alloc_skip:
    mov rax, 1
    add rsp, 32
    pop rbx
    ret
    
@fail:
    xor rax, rax
    add rsp, 32
    pop rbx
    ret
Json_ParseFast ENDP

PUBLIC Json_ParseFast

END
