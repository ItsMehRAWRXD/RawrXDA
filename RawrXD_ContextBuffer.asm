; =============================================================================
; RawrXD_ContextBuffer.asm
; Upgrade 2: Zero-heap ring buffer for execution context accumulation
;
; Maintains a static 8 KB string buffer that records every step the agent
; executes and its observed result.  The buffer is then stitched into the
; next LLM prompt so each Think phase has full memory of what happened before.
;
; PUBLIC API
;   ContextBuf_Reset()
;       Clear the buffer (zero length, leave data intact for safety).
;
;   ContextBuf_AppendStep(pToolName)    RCX = LPSTR
;       Appends "Step: <toolName>\r\n" to the buffer.
;
;   ContextBuf_AppendResult(pResult)    RCX = LPSTR
;       Appends "Result: <result>\r\n\r\n" to the buffer.
;
;   ContextBuf_Get() -> LPSTR
;       Returns pointer to the null-terminated context string.
;
;   ContextBuf_GetLen() -> DWORD
;       Returns current length (bytes written, excluding null terminator).
;
;   ContextBuf_BuildPrompt(pGoal, pSystemPrompt, pOutBuf, dwOutSize)
;       RCX = pGoal          – agent's current objective (ANSI string)
;       RDX = pSystemPrompt  – base system instruction string (ANSI string)
;       R8  = pOutBuf        – destination buffer for the assembled prompt
;       R9  = dwOutSize      – byte capacity of pOutBuf
;
;       Writes:  <pSystemPrompt>
;                Goal: <pGoal>
;
;                Previous execution context:
;                <g_ctxBuf>
;       …all null-terminated and bounds-guarded.
;
; Internal helper (not exported):
;   _CBuf_cat(pDst, pdwOffset, dwMax, pSrc)
;       Appends pSrc into pDst[*pdwOffset], updating the offset.
;       Never writes beyond dwMax bytes; always null-terminates.
; =============================================================================

OPTION CASEMAP:NONE

; ---- Constants --------------------------------------------------------------
CTXBUF_SIZE     EQU 8192
CTXBUF_GUARD    EQU (CTXBUF_SIZE - 2)   ; max byte index that can receive a char

; ---- Read-only string constants  --------------------------------------------
.data
    cb_step_prefix      db "Step: ", 0
    cb_result_prefix    db "Result: ", 0
    cb_crlf             db 13, 10, 0
    cb_crlfcrlf         db 13, 10, 13, 10, 0
    cb_goal_hdr         db "Goal: ", 0
    cb_ctx_hdr          db 13, 10, 13, 10, "Previous execution context:", 13, 10, 0

; ---- Mutable buffer ---------------------------------------------------------
.data?
    ALIGN 8
    g_ctxBuf    db CTXBUF_SIZE dup(?)    ; accumulated context
    g_ctxLen    dq ?                     ; current length (bytes, excl. null)

.code

; =============================================================================
; ContextBuf_Reset
;   No args.  Sets g_ctxLen = 0 and writes NUL at buf[0].
;   Stack: leaf – only 32-byte shadow, no push needed for 0-push alignment.
;   After CALL: RSP = X-8; no pushes; sub rsp, 24 → RSP = X-32 (32%16=0 ✓)
; =============================================================================
ContextBuf_Reset PROC FRAME
    sub  rsp, 24
    .allocstack 24
    .endprolog

    xor  eax, eax
    mov  qword ptr [g_ctxLen], rax
    lea  r10, [g_ctxBuf]
    mov  byte ptr [r10], 0

    add  rsp, 24
    ret
ContextBuf_Reset ENDP

; =============================================================================
; _CBuf_cat – internal append helper
;
; RCX = pDst        – destination buffer base
; RDX = pdwOffset   – pointer to QWORD holding current write offset
; R8  = dwMax       – buffer capacity (bytes; NOT including null terminator)
; R9  = pSrc        – source string (null-terminated)
;
; Pure register loop; leaf function; no stack frame needed.
; =============================================================================
_CBuf_cat PROC
    ; R10 = src char ptr
    ; R11 = dest write ptr  (pDst + *pdwOffset)
    ; RAX = current offset
    ; All volatile – no preservation required.

    mov  rax, qword ptr [rdx]       ; rax = current offset
    mov  r11, rcx
    add  r11, rax                   ; r11 = pDst + offset (write cursor)
    mov  r10, r9                    ; r10 = pSrc

cc_loop:
    ; Bounds guard: need at least 2 bytes (char + null terminator)
    lea  r9, [rax + 2]
    cmp  r9, r8
    jg   cc_done                    ; buffer would overflow – stop

    movzx ecx, byte ptr [r10]
    test cl, cl
    jz   cc_done                    ; hit NUL ∴ source exhausted

    mov  byte ptr [r11], cl
    inc  r10
    inc  r11
    inc  rax
    jmp  cc_loop

cc_done:
    ; Always null-terminate at current write position
    mov  byte ptr [r11], 0
    mov  qword ptr [rdx], rax       ; write back updated offset
    ret
_CBuf_cat ENDP

; =============================================================================
; ContextBuf_AppendStep(pToolName)   RCX = LPSTR pToolName
;
; Appends "Step: <toolName>\r\n"
;
; Stack: 1 push (rbx) → N must be divisible by 16; use N=32.
;   After CALL: RSP=X-8; push rbx → X-16; sub rsp,32 → X-48 (48%16=0 ✓)
; =============================================================================
ContextBuf_AppendStep PROC FRAME
    push rbx
    .pushreg rbx
    sub  rsp, 32
    .allocstack 32
    .endprolog

    mov  rbx, rcx                   ; save pToolName across calls

    ; -- Append "Step: " ------------------------------------------------------
    lea  rcx, [g_ctxBuf]
    lea  rdx, [g_ctxLen]
    mov  r8,  CTXBUF_SIZE
    lea  r9,  [cb_step_prefix]
    call _CBuf_cat

    ; -- Append tool name ------------------------------------------------------
    lea  rcx, [g_ctxBuf]
    lea  rdx, [g_ctxLen]
    mov  r8,  CTXBUF_SIZE
    mov  r9,  rbx
    call _CBuf_cat

    ; -- Append "\r\n" ---------------------------------------------------------
    lea  rcx, [g_ctxBuf]
    lea  rdx, [g_ctxLen]
    mov  r8,  CTXBUF_SIZE
    lea  r9,  [cb_crlf]
    call _CBuf_cat

    add  rsp, 32
    pop  rbx
    ret
ContextBuf_AppendStep ENDP

; =============================================================================
; ContextBuf_AppendResult(pResult)   RCX = LPSTR pResult
;
; Appends "Result: <result>\r\n\r\n"
; Stack: same 1-push pattern → N=32.
; =============================================================================
ContextBuf_AppendResult PROC FRAME
    push rbx
    .pushreg rbx
    sub  rsp, 32
    .allocstack 32
    .endprolog

    mov  rbx, rcx                   ; save pResult

    ; -- Append "Result: " ----------------------------------------------------
    lea  rcx, [g_ctxBuf]
    lea  rdx, [g_ctxLen]
    mov  r8,  CTXBUF_SIZE
    lea  r9,  [cb_result_prefix]
    call _CBuf_cat

    ; -- Append result string -------------------------------------------------
    lea  rcx, [g_ctxBuf]
    lea  rdx, [g_ctxLen]
    mov  r8,  CTXBUF_SIZE
    mov  r9,  rbx
    call _CBuf_cat

    ; -- Append "\r\n\r\n" (empty line separator) ----------------------------
    lea  rcx, [g_ctxBuf]
    lea  rdx, [g_ctxLen]
    mov  r8,  CTXBUF_SIZE
    lea  r9,  [cb_crlfcrlf]
    call _CBuf_cat

    add  rsp, 32
    pop  rbx
    ret
ContextBuf_AppendResult ENDP

; =============================================================================
; ContextBuf_Get() -> LPSTR
;   Returns pointer to g_ctxBuf in RAX.  Leaf function – no stack frame.
; =============================================================================
ContextBuf_Get PROC
    lea  rax, [g_ctxBuf]
    ret
ContextBuf_Get ENDP

; =============================================================================
; ContextBuf_GetLen() -> DWORD
;   Returns current length as a 32-bit value in EAX.
; =============================================================================
ContextBuf_GetLen PROC
    mov  eax, dword ptr [g_ctxLen]
    ret
ContextBuf_GetLen ENDP

; =============================================================================
; ContextBuf_BuildPrompt(pGoal, pSystemPrompt, pOutBuf, dwOutSize)
;
;   RCX  LPSTR  pGoal          – agent's current objective
;   RDX  LPSTR  pSystemPrompt  – system instruction prefix, may be NULL
;   R8   LPBYTE pOutBuf        – destination buffer
;   R9   DWORD  dwOutSize      – capacity of pOutBuf (bytes)
;
; Assembles: <pSystemPrompt> + "Goal: " + <pGoal> +
;            "\r\n\r\nPrevious execution context:\r\n" + g_ctxBuf
;
; Stack: 3 scalars to save + pOutBuf/dwOutSize → 5 × 8 = 40 bytes locals.
;   Total with shadow = 32 + 40 = 72. Need N ≡ 8 (mod 16) with 0 pushes.
;   72 % 16 = 8  ✓  →  sub rsp, 72.
;   RSP after = X - 8 - 72 = X - 80.  80 % 16 = 0  ✓
; =============================================================================
ContextBuf_BuildPrompt PROC FRAME
    sub  rsp, 72
    .allocstack 72
    .endprolog

    ; Persist params across calls
    mov  qword ptr [rsp + 32], rcx      ; pGoal
    mov  qword ptr [rsp + 40], rdx      ; pSystemPrompt (may be NULL)
    mov  qword ptr [rsp + 48], r8       ; pOutBuf
    mov  eax, r9d
    mov  qword ptr [rsp + 56], rax      ; dwOutSize (stored as QWORD, mov eax zero-extends)

    ; We manage a running offset in a local QWORD at [rsp+64]
    xor  eax, eax
    mov  qword ptr [rsp + 64], rax

    ; -- Append system prompt (if not NULL) -----------------------------------
    mov  r9, qword ptr [rsp + 40]
    test r9, r9
    jz   cbp_goal

    mov  rcx, qword ptr [rsp + 48]
    lea  rdx, [rsp + 64]
    mov  r8,  qword ptr [rsp + 56]
    call _CBuf_cat

cbp_goal:
    ; -- Append "Goal: " ------------------------------------------------------
    mov  rcx, qword ptr [rsp + 48]
    lea  rdx, [rsp + 64]
    mov  r8,  qword ptr [rsp + 56]
    lea  r9,  [cb_goal_hdr]
    call _CBuf_cat

    ; -- Append goal string ---------------------------------------------------
    mov  rcx, qword ptr [rsp + 48]
    lea  rdx, [rsp + 64]
    mov  r8,  qword ptr [rsp + 56]
    mov  r9,  qword ptr [rsp + 32]
    call _CBuf_cat

    ; -- Append context section header ----------------------------------------
    mov  rcx, qword ptr [rsp + 48]
    lea  rdx, [rsp + 64]
    mov  r8,  qword ptr [rsp + 56]
    lea  r9,  [cb_ctx_hdr]
    call _CBuf_cat

    ; -- Append accumulated context buffer ------------------------------------
    mov  rcx, qword ptr [rsp + 48]
    lea  rdx, [rsp + 64]
    mov  r8,  qword ptr [rsp + 56]
    lea  r9,  [g_ctxBuf]
    call _CBuf_cat

    add  rsp, 72
    ret
ContextBuf_BuildPrompt ENDP

; =============================================================================
PUBLIC ContextBuf_Reset
PUBLIC ContextBuf_AppendStep
PUBLIC ContextBuf_AppendResult
PUBLIC ContextBuf_Get
PUBLIC ContextBuf_GetLen
PUBLIC ContextBuf_BuildPrompt

END
