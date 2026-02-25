; MCP Transport Hook Assembly - MASM64
; Raw buffer manipulation for MCP message interception
; Provides trampoline stubs that preserve all registers before calling C++ handlers


; ─── Cross-module symbol resolution ───
INCLUDE rawrxd_master.inc

.code

; External C++ handlers
EXTERN MCP_ReadMessageHook:PROC
EXTERN MCP_WriteMessageHook:PROC
EXTERN MCP_OnSocketDataHook:PROC
EXTERN MCP_WebSocketFrameHook:PROC

; Storage for original function addresses (filled by hook installer)
PUBLIC g_OrigReadMessage
PUBLIC g_OrigWriteMessage
PUBLIC g_OrigOnSocketData
PUBLIC g_OrigWebSocketFrame

.data
ALIGN 8
g_OrigReadMessage      QWORD 0
g_OrigWriteMessage     QWORD 0
g_OrigOnSocketData     QWORD 0
g_OrigWebSocketFrame   QWORD 0

; Hook enable flags (atomic, can be toggled at runtime)
PUBLIC g_EnableReadHook
PUBLIC g_EnableWriteHook
PUBLIC g_EnableSocketHook
PUBLIC g_EnableWSHook

g_EnableReadHook       DWORD 1
g_EnableWriteHook      DWORD 1
g_EnableSocketHook     DWORD 1
g_EnableWSHook         DWORD 1

.code

;------------------------------------------------------------------------------
; ReadMessage Trampoline
; RCX = buffer pointer, RDX = length
; Captures raw MCP frame BEFORE any JS parsing occurs
;------------------------------------------------------------------------------
PUBLIC MCP_ReadMessage_Trampoline
MCP_ReadMessage_Trampoline PROC
    ; Check if hook is enabled
    mov     eax, [g_EnableReadHook]
    test    eax, eax
    jz      @F_bypass_read
    
    ; Save all volatile registers (Win64 ABI)
    push    rbx
    push    rsi
    push    rdi
    push    r8
    push    r9
    push    r10
    push    r11
    push    r12
    push    r13
    push    r14
    push    r15
    sub     rsp, 40h                    ; Shadow space + alignment
    
    ; Save original parameters
    mov     rbx, rcx                    ; buffer
    mov     rsi, rdx                    ; length
    
    ; Call C++ handler: MCP_ReadMessageHook(buffer, length)
    ; RCX = buffer (already set)
    ; RDX = length (already set)
    call    MCP_ReadMessageHook
    
    ; Restore original parameters for original function
    mov     rcx, rbx
    mov     rdx, rsi
    
    add     rsp, 40h
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     r11
    pop     r10
    pop     r9
    pop     r8
    pop     rdi
    pop     rsi
    pop     rbx
    
@F_bypass_read:
    ; Jump to original function
    mov     rax, [g_OrigReadMessage]
    test    rax, rax
    jz      @F_null_read
    jmp     rax
    
@F_null_read:
    ; No original function - just return
    xor     eax, eax
    ret
MCP_ReadMessage_Trampoline ENDP

;------------------------------------------------------------------------------
; WriteMessage Trampoline
; RCX = buffer pointer, RDX = length
; Captures outgoing MCP response/notification frames
;------------------------------------------------------------------------------
PUBLIC MCP_WriteMessage_Trampoline
MCP_WriteMessage_Trampoline PROC
    mov     eax, [g_EnableWriteHook]
    test    eax, eax
    jz      @F_bypass_write
    
    push    rbx
    push    rsi
    push    rdi
    push    r8
    push    r9
    push    r10
    push    r11
    push    r12
    push    r13
    push    r14
    push    r15
    sub     rsp, 40h
    
    mov     rbx, rcx
    mov     rsi, rdx
    
    ; Call C++ handler
    call    MCP_WriteMessageHook
    
    mov     rcx, rbx
    mov     rdx, rsi
    
    add     rsp, 40h
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     r11
    pop     r10
    pop     r9
    pop     r8
    pop     rdi
    pop     rsi
    pop     rbx
    
@F_bypass_write:
    mov     rax, [g_OrigWriteMessage]
    test    rax, rax
    jz      @F_null_write
    jmp     rax
    
@F_null_write:
    xor     eax, eax
    ret
MCP_WriteMessage_Trampoline ENDP

;------------------------------------------------------------------------------
; OnSocketData Trampoline
; RCX = data buffer, RDX = length, R8 = SOCKET handle
; Captures raw TCP/IPC socket data before MCP framing
;------------------------------------------------------------------------------
PUBLIC MCP_OnSocketData_Trampoline
MCP_OnSocketData_Trampoline PROC
    mov     eax, [g_EnableSocketHook]
    test    eax, eax
    jz      @F_bypass_socket
    
    push    rbx
    push    rsi
    push    rdi
    push    r8
    push    r9
    push    r10
    push    r11
    push    r12
    push    r13
    push    r14
    push    r15
    sub     rsp, 40h
    
    ; Save all three parameters
    mov     rbx, rcx                    ; data
    mov     rsi, rdx                    ; length
    mov     rdi, r8                     ; socket
    
    ; Call C++ handler: MCP_OnSocketDataHook(data, length, socket)
    ; RCX, RDX, R8 already set
    call    MCP_OnSocketDataHook
    
    ; Restore parameters
    mov     rcx, rbx
    mov     rdx, rsi
    mov     r8, rdi
    
    add     rsp, 40h
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     r11
    pop     r10
    pop     r9
    pop     r8
    pop     rdi
    pop     rsi
    pop     rbx
    
@F_bypass_socket:
    mov     rax, [g_OrigOnSocketData]
    test    rax, rax
    jz      @F_null_socket
    jmp     rax
    
@F_null_socket:
    xor     eax, eax
    ret
MCP_OnSocketData_Trampoline ENDP

;------------------------------------------------------------------------------
; WebSocket Frame Trampoline
; RCX = frame buffer, RDX = frame length, R8 = opcode (text=1, binary=2, etc.)
; Captures WebSocket frames for WS-based MCP transport
;------------------------------------------------------------------------------
PUBLIC MCP_WebSocketFrame_Trampoline
MCP_WebSocketFrame_Trampoline PROC
    mov     eax, [g_EnableWSHook]
    test    eax, eax
    jz      @F_bypass_ws
    
    push    rbx
    push    rsi
    push    rdi
    push    r8
    push    r9
    push    r10
    push    r11
    push    r12
    push    r13
    push    r14
    push    r15
    sub     rsp, 40h
    
    mov     rbx, rcx                    ; frame
    mov     rsi, rdx                    ; length
    mov     rdi, r8                     ; opcode
    
    ; Call C++ handler
    call    MCP_WebSocketFrameHook
    
    mov     rcx, rbx
    mov     rdx, rsi
    mov     r8, rdi
    
    add     rsp, 40h
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     r11
    pop     r10
    pop     r9
    pop     r8
    pop     rdi
    pop     rsi
    pop     rbx
    
@F_bypass_ws:
    mov     rax, [g_OrigWebSocketFrame]
    test    rax, rax
    jz      @F_null_ws
    jmp     rax
    
@F_null_ws:
    xor     eax, eax
    ret
MCP_WebSocketFrame_Trampoline ENDP

;------------------------------------------------------------------------------
; Buffer Hex Dump Helper (for debugging)
; RCX = buffer, RDX = length, R8 = output buffer
; Returns number of chars written in RAX
;------------------------------------------------------------------------------
PUBLIC MCP_HexDumpBuffer
MCP_HexDumpBuffer PROC
    push    rbx
    push    rsi
    push    rdi
    
    mov     rsi, rcx                    ; source buffer
    mov     rbx, rdx                    ; length
    mov     rdi, r8                     ; output buffer
    xor     rax, rax                    ; chars written
    
    test    rbx, rbx
    jz      @F_hex_done
    
@F_hex_loop:
    movzx   ecx, BYTE PTR [rsi]
    
    ; High nibble
    mov     edx, ecx
    shr     edx, 4
    add     edx, '0'
    cmp     edx, '9'
    jbe     @F_high_ok
    add     edx, 7                      ; A-F offset
@F_high_ok:
    mov     BYTE PTR [rdi], dl
    inc     rdi
    inc     rax
    
    ; Low nibble
    mov     edx, ecx
    and     edx, 0Fh
    add     edx, '0'
    cmp     edx, '9'
    jbe     @F_low_ok
    add     edx, 7
@F_low_ok:
    mov     BYTE PTR [rdi], dl
    inc     rdi
    inc     rax
    
    ; Space separator
    mov     BYTE PTR [rdi], ' '
    inc     rdi
    inc     rax
    
    inc     rsi
    dec     rbx
    jnz     @F_hex_loop
    
@F_hex_done:
    ; Null terminate
    mov     BYTE PTR [rdi], 0
    
    pop     rdi
    pop     rsi
    pop     rbx
    ret
MCP_HexDumpBuffer ENDP

;------------------------------------------------------------------------------
; Content-Length Parser (for stdio MCP transport)
; RCX = buffer start, RDX = buffer length
; Returns content-length value in RAX, or -1 if not found
;------------------------------------------------------------------------------
PUBLIC MCP_ParseContentLength
MCP_ParseContentLength PROC
    push    rbx
    push    rsi
    push    rdi
    
    mov     rsi, rcx                    ; buffer
    mov     rbx, rdx                    ; length
    
    ; Search for "Content-Length: "
    mov     rax, -1                     ; default: not found
    cmp     rbx, 16
    jb      @F_cl_done
    
    xor     rdi, rdi                    ; offset
    
@F_cl_search:
    cmp     rdi, rbx
    jae     @F_cl_done
    
    ; Check for 'C'
    movzx   ecx, BYTE PTR [rsi + rdi]
    cmp     cl, 'C'
    jne     @F_cl_next
    
    ; Check "Content-Length: " (16 chars)
    lea     rcx, [rsi + rdi]
    push    rax
    
    ; Quick check: "Content-Length:"
    mov     eax, DWORD PTR [rcx]
    cmp     eax, 'tnoC'                 ; "Cont" little-endian
    pop     rax
    jne     @F_cl_next
    
    ; Found potential match - skip header and parse number
    add     rdi, 16
    xor     rax, rax
    
@F_cl_parse:
    cmp     rdi, rbx
    jae     @F_cl_done
    
    movzx   ecx, BYTE PTR [rsi + rdi]
    
    ; Skip whitespace
    cmp     cl, ' '
    je      @F_cl_skip
    cmp     cl, 09h                     ; tab
    je      @F_cl_skip
    
    ; Check for digit
    cmp     cl, '0'
    jb      @F_cl_done
    cmp     cl, '9'
    ja      @F_cl_done
    
    ; Accumulate: rax = rax * 10 + digit
    imul    rax, 10
    sub     cl, '0'
    movzx   ecx, cl
    add     rax, rcx
    
@F_cl_skip:
    inc     rdi
    jmp     @F_cl_parse
    
@F_cl_next:
    inc     rdi
    jmp     @F_cl_search
    
@F_cl_done:
    pop     rdi
    pop     rsi
    pop     rbx
    ret
MCP_ParseContentLength ENDP

;------------------------------------------------------------------------------
; JSON-RPC Method Extractor
; RCX = JSON buffer, RDX = length, R8 = output buffer (max 64 chars)
; Returns method length in RAX, or 0 if not found
;------------------------------------------------------------------------------
PUBLIC MCP_ExtractMethod
MCP_ExtractMethod PROC
    push    rbx
    push    rsi
    push    rdi
    push    r12
    
    mov     rsi, rcx                    ; JSON buffer
    mov     rbx, rdx                    ; length
    mov     rdi, r8                     ; output
    mov     r12, 64                     ; max output length
    
    xor     rax, rax
    cmp     rbx, 10
    jb      @F_method_done
    
    xor     rcx, rcx                    ; offset
    
@F_method_search:
    cmp     rcx, rbx
    jae     @F_method_done
    
    ; Look for "method"
    movzx   edx, BYTE PTR [rsi + rcx]
    cmp     dl, 'm'
    jne     @F_method_skip
    
    ; Check "method": (7 chars + colon)
    lea     rdx, [rsi + rcx]
    cmp     DWORD PTR [rdx], 'htem'     ; "meth"
    jne     @F_method_skip
    cmp     WORD PTR [rdx + 4], 'do'    ; "od"
    jne     @F_method_skip
    
    ; Found "method" - skip to value
    add     rcx, 6
    
@F_method_find_quote:
    cmp     rcx, rbx
    jae     @F_method_done
    movzx   edx, BYTE PTR [rsi + rcx]
    inc     rcx
    cmp     dl, '"'
    jne     @F_method_find_quote
    
    ; Now at start of method string - copy until closing quote
@F_method_copy:
    cmp     rcx, rbx
    jae     @F_method_done
    cmp     rax, r12
    jae     @F_method_done
    
    movzx   edx, BYTE PTR [rsi + rcx]
    cmp     dl, '"'
    je      @F_method_terminate
    
    mov     BYTE PTR [rdi + rax], dl
    inc     rax
    inc     rcx
    jmp     @F_method_copy
    
@F_method_terminate:
    mov     BYTE PTR [rdi + rax], 0
    jmp     @F_method_done
    
@F_method_skip:
    inc     rcx
    jmp     @F_method_search
    
@F_method_done:
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret
MCP_ExtractMethod ENDP

END
