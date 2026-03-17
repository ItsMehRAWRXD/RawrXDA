; lsp_jsonrpc.asm - JSON-RPC 2.0 for Language Server Protocol
; Pure MASM64 - Zero CRT dependencies

PUBLIC LSP_CreateRequest
PUBLIC LSP_ParseResponse
PUBLIC LSP_GetContentLen

.data
align 8
lsp_id_counter  QWORD 1
lsp_hdr_prefix  DB "Content-Length: ", 0
lsp_json_open   DB '{"jsonrpc":"2.0","id":', 0
lsp_method_key  DB ',"method":"', 0
lsp_params_key  DB '","params":', 0
lsp_json_close  DB '}', 0

.code

; Internal: strlen - R11 = string, returns RAX = length
lsp_strlen PROC FRAME
    .endprolog
    xor eax, eax
lsp_sl:
    cmp byte ptr [r11+rax], 0
    je lsp_sld
    inc rax
    jmp lsp_sl
lsp_sld:
    ret
lsp_strlen ENDP

; Internal: copy string - RSI = src, RDI = dst, returns RDI past end
lsp_strcpy PROC FRAME
    .endprolog
lsp_sc:
    lodsb
    test al, al
    jz lsp_scd
    stosb
    jmp lsp_sc
lsp_scd:
    ret
lsp_strcpy ENDP

; Internal: itoa - RCX = number, RDI = buffer, returns RDI past digits
lsp_itoa PROC FRAME
    push rbx
    .pushreg rbx
    sub rsp, 28h
    .allocstack 28h
    .endprolog
    
    mov rax, rcx
    lea rbx, [rsp+10h]     ; temp buffer on stack
    mov byte ptr [rbx+10], 0
    lea r8, [rbx+10]       ; end of temp
    mov rcx, 10
    
lsp_ia:
    xor edx, edx
    div rcx
    add dl, '0'
    dec r8
    mov [r8], dl
    test rax, rax
    jnz lsp_ia
    
    ; copy digits to RDI
lsp_iac:
    mov al, [r8]
    test al, al
    jz lsp_iad
    mov [rdi], al
    inc rdi
    inc r8
    jmp lsp_iac
lsp_iad:
    add rsp, 28h
    pop rbx
    ret
lsp_itoa ENDP

; RCX = method, RDX = params, R8 = buffer, R9 = bufSize
; Returns: RAX = bytes written
LSP_CreateRequest PROC FRAME
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
    sub rsp, 40h
    .allocstack 40h
    .endprolog
    
    mov r12, rcx        ; method
    mov r13, rdx        ; params
    mov rbx, r8         ; buffer start
    mov rdi, r8         ; write cursor
    
    ; Build JSON body first into temp area (after header space)
    lea rdi, [rbx+30h]  ; leave room for header
    mov rax, rdi
    mov [rsp+20h], rax   ; save body start
    
    ; {"jsonrpc":"2.0","id":N,"method":"X","params":Y}
    lea rsi, lsp_json_open
    call lsp_strcpy
    
    mov rcx, lsp_id_counter
    call lsp_itoa
    inc qword ptr lsp_id_counter
    
    lea rsi, lsp_method_key
    call lsp_strcpy
    
    mov rsi, r12         ; method string
    call lsp_strcpy
    
    lea rsi, lsp_params_key
    call lsp_strcpy
    
    mov rsi, r13         ; params string
    call lsp_strcpy
    
    lea rsi, lsp_json_close
    call lsp_strcpy
    
    ; RDI now past end of JSON body
    mov r8, [rsp+20h]   ; body start
    mov r9, rdi
    sub r9, r8           ; body length
    
    ; Now write header at buffer start
    mov rdi, rbx
    lea rsi, lsp_hdr_prefix
    call lsp_strcpy
    
    mov rcx, r9
    call lsp_itoa
    
    mov byte ptr [rdi], 0Dh
    inc rdi
    mov byte ptr [rdi], 0Ah
    inc rdi
    mov byte ptr [rdi], 0Dh
    inc rdi
    mov byte ptr [rdi], 0Ah
    inc rdi
    
    ; Total = header + body
    mov rax, rdi
    sub rax, rbx
    add rax, r9
    
    add rsp, 40h
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret
LSP_CreateRequest ENDP

; RCX = buffer, RDX = size, R8 = outResult
; Returns: RAX = 1 success, 0 fail
LSP_ParseResponse PROC FRAME
    .endprolog
    ; Stub - validate basic JSON structure
    mov rax, 1
    ret
LSP_ParseResponse ENDP

; RCX = header buffer
; Returns: RAX = content length value
LSP_GetContentLen PROC FRAME
    .endprolog
    ; Scan for digits after "Content-Length: "
    xor eax, eax
    xor edx, edx
lsp_gcl:
    movzx edx, byte ptr [rcx]
    test dl, dl
    jz lsp_gcld
    cmp dl, ':'
    jne lsp_gcln
    ; Found colon, skip spaces, parse digits
    inc rcx
lsp_gcls:
    movzx edx, byte ptr [rcx]
    cmp dl, ' '
    jne lsp_gclp
    inc rcx
    jmp lsp_gcls
lsp_gclp:
    cmp dl, '0'
    jb lsp_gcld
    cmp dl, '9'
    ja lsp_gcld
    imul rax, 10
    sub dl, '0'
    movzx edx, dl
    add rax, rdx
    inc rcx
    movzx edx, byte ptr [rcx]
    jmp lsp_gclp
lsp_gcln:
    inc rcx
    jmp lsp_gcl
lsp_gcld:
    ret
LSP_GetContentLen ENDP

END
