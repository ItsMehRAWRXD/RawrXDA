; ═══════════════════════════════════════════════════════════════════════════════
; RawrXD_Streaming_Formatter.asm
; Server-Sent Events (SSE) formatting for chunked responses
; ═══════════════════════════════════════════════════════════════════════════════

OPTION DOTNAME
include RawrXD_Defs.inc


; ─── Cross-module symbol resolution ───
INCLUDE rawrxd_master.inc

EXTERN send : PROC

; ═══════════════════════════════════════════════════════════════════════════════
; CODE SECTION
; ═══════════════════════════════════════════════════════════════════════════════
.CODE

; ═══════════════════════════════════════════════════════════════════════════════
; StreamFormatter_WriteToken
; RCX = Socket Handle, RDX = Token Data, R8 = Length
; ═══════════════════════════════════════════════════════════════════════════════
StreamFormatter_WriteToken PROC FRAME
    push rbx
    push rsi
    push rdi
    sub rsp, 2096                ; 2KB buffer + shadow space
    .endprolog
    
    mov rbx, rcx                 ; Socket
    mov rsi, rdx                 ; Token Data
    mov rdi, r8                  ; Length
    
    ; Check if token too large for buffer (limit 2000 chars)
    cmp rdi, 2000
    ja @alloc_large              ; Fallback or truncate? Truncate for now.
    
    ; Construct "data: " prefix
    lea rcx, [rsp + 32]          ; Buffer start
    mov byte ptr [rcx+0], 'd'
    mov byte ptr [rcx+1], 'a'
    mov byte ptr [rcx+2], 't'
    mov byte ptr [rcx+3], 'a'
    mov byte ptr [rcx+4], ':'
    mov byte ptr [rcx+5], ' '
    
    ; Copy Token
    lea rdx, [rcx + 6]           ; Dest
    
    ; Inline MemCopy (rep movsb)
    push rsi
    push rdi
    push rcx
    mov rcx, rdi                 ; Count
    mov rdi, rdx                 ; Dest
    rep movsb
    pop rcx
    pop rdi
    pop rsi
    
    ; Add Suffix "\n\n"
    lea rdx, [rsp + 32 + 6]
    add rdx, r8                  ; End of token
    mov byte ptr [rdx], 0Ah
    mov byte ptr [rdx+1], 0Ah
    
    ; Send
    mov rcx, rbx                 ; Socket
    lea rdx, [rsp + 32]          ; Buffer
    mov r8, rdi                  ; Token Len
    add r8, 8                    ; +6 prefix + 2 suffix
    mov r9, 0                    ; Flags
    call send
    
    jmp @done
    
@alloc_large:
    ; Ignore massive tokens to prevent overflow in this fast-path formatter
    xor eax, eax

@done:
    add rsp, 2096
    pop rdi
    pop rsi
    pop rbx
    ret
StreamFormatter_WriteToken ENDP

PUBLIC StreamFormatter_WriteToken

END
