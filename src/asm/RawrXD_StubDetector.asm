; ============================================================================
; RawrXD_StubDetector.asm — Phase 31: MASM64 Stub Detection Kernel
; ============================================================================
;
; PURPOSE:
;   Fast-path function prologue scanner. Checks if a given function pointer
;   matches any known stub pattern (bare ret, xor eax ret, mov eax 0 ret,
;   mov rax 0 ret, or int3 padding).
;
; PROTOTYPE (C):
;   extern "C" int __cdecl IsStubFunction(void* funcPtr, size_t maxBytesToScan);
;
; ARGS (x64 ABI):
;   RCX = funcPtr (pointer to start of function code)
;   RDX = maxBytesToScan (max bytes to inspect, typically 32)
;
; RETURNS:
;   EAX = 1 if stub detected, 0 if real implementation
;
; PATTERNS DETECTED:
;   1. C3                          — bare ret
;   2. 33 C0 C3                    — xor eax, eax; ret
;   3. B8 00 00 00 00 C3           — mov eax, 0; ret
;   4. 48 C7 C0 00 00 00 00 C3    — mov rax, 0; ret
;   5. CC CC CC CC ...             — int3 padding (4+ bytes)
;   6. 90+ ... C3                  — NOP slide to ret
;
; SAFETY:
;   Does NOT call VirtualQuery (that's done by C++ caller if needed).
;   Assumes funcPtr points to readable executable memory.
;   Bounds-checks all reads against maxBytesToScan.
;
; RULE: NO SOURCE FILE IS TO BE SIMPLIFIED
; ============================================================================

OPTION CASEMAP:NONE

.code

; ============================================================================
; IsStubFunction PROC
; ============================================================================
; RCX = funcPtr
; RDX = maxBytesToScan
; Returns: EAX = 1 (stub) or 0 (not stub)
; ============================================================================
IsStubFunction PROC FRAME
    ; Prologue
    push    rbp
    .pushreg rbp
    mov     rbp, rsp
    .setframe rbp, 0
    push    rbx
    .pushreg rbx
    push    rsi
    .pushreg rsi
    push    rdi
    .pushreg rdi
    .endprolog

    ; Validate inputs
    test    rcx, rcx
    jz      _not_stub           ; nullptr → not a stub
    test    rdx, rdx
    jz      _not_stub           ; 0 bytes → can't determine

    mov     rsi, rcx            ; RSI = funcPtr (code base)
    mov     rdi, rdx            ; RDI = maxBytesToScan

    ; ---- Pattern 1: bare ret (C3) ----
    ; Need at least 1 byte
    cmp     rdi, 1
    jb      _not_stub
    movzx   eax, byte ptr [rsi]
    cmp     al, 0C3h
    je      _is_stub

    ; ---- Pattern 2: xor eax, eax; ret (33 C0 C3) ----
    ; Need at least 3 bytes
    cmp     rdi, 3
    jb      _check_int3
    movzx   eax, byte ptr [rsi]
    cmp     al, 033h
    jne     _pat3
    movzx   eax, byte ptr [rsi+1]
    cmp     al, 0C0h
    jne     _pat3
    movzx   eax, byte ptr [rsi+2]
    cmp     al, 0C3h
    je      _is_stub

_pat3:
    ; ---- Pattern 3: mov eax, 0; ret (B8 00 00 00 00 C3) ----
    ; Need at least 6 bytes
    cmp     rdi, 6
    jb      _check_int3
    movzx   eax, byte ptr [rsi]
    cmp     al, 0B8h
    jne     _pat4
    ; Check next 4 bytes are 0
    mov     eax, dword ptr [rsi+1]
    test    eax, eax
    jnz     _pat4
    movzx   eax, byte ptr [rsi+5]
    cmp     al, 0C3h
    je      _is_stub

_pat4:
    ; ---- Pattern 4: mov rax, 0; ret (48 C7 C0 00 00 00 00 C3) ----
    ; Need at least 8 bytes
    cmp     rdi, 8
    jb      _check_int3
    movzx   eax, byte ptr [rsi]
    cmp     al, 048h
    jne     _check_frame
    movzx   eax, byte ptr [rsi+1]
    cmp     al, 0C7h
    jne     _check_frame
    movzx   eax, byte ptr [rsi+2]
    cmp     al, 0C0h
    jne     _check_frame
    ; Check next 4 bytes are 0
    mov     eax, dword ptr [rsi+3]
    test    eax, eax
    jnz     _check_frame
    movzx   eax, byte ptr [rsi+7]
    cmp     al, 0C3h
    je      _is_stub

_check_frame:
    ; ---- Pattern 5: push rbp; mov rbp,rsp; xor eax,eax; pop rbp; ret ----
    ; (55 48 89 E5 33 C0 5D C3) — 8 bytes
    cmp     rdi, 8
    jb      _check_int3
    movzx   eax, byte ptr [rsi]
    cmp     al, 055h            ; push rbp
    jne     _check_int3
    movzx   eax, byte ptr [rsi+1]
    cmp     al, 048h            ; REX.W
    jne     _check_int3
    movzx   eax, byte ptr [rsi+2]
    cmp     al, 089h            ; mov
    jne     _check_int3
    movzx   eax, byte ptr [rsi+3]
    cmp     al, 0E5h            ; rbp, rsp
    jne     _check_int3
    movzx   eax, byte ptr [rsi+4]
    cmp     al, 033h            ; xor
    jne     _check_int3
    movzx   eax, byte ptr [rsi+5]
    cmp     al, 0C0h            ; eax, eax
    jne     _check_int3
    movzx   eax, byte ptr [rsi+6]
    cmp     al, 05Dh            ; pop rbp
    jne     _check_int3
    movzx   eax, byte ptr [rsi+7]
    cmp     al, 0C3h            ; ret
    je      _is_stub

_check_int3:
    ; ---- Pattern 6: int3 padding (CC CC CC CC) ----
    ; Need at least 4 bytes, all must be 0xCC
    cmp     rdi, 4
    jb      _check_nop

    xor     ecx, ecx            ; counter
    mov     rbx, rdi
    cmp     rbx, 16             ; cap at 16 bytes
    jbe     _int3_cap_ok
    mov     rbx, 16
_int3_cap_ok:

_int3_loop:
    cmp     rcx, rbx
    jge     _int3_done
    movzx   eax, byte ptr [rsi+rcx]
    cmp     al, 0CCh
    jne     _check_nop          ; not int3, break
    inc     rcx
    jmp     _int3_loop

_int3_done:
    ; If we found 4+ consecutive int3s, it's a stub
    cmp     ecx, 4
    jge     _is_stub

_check_nop:
    ; ---- Pattern 7: NOP slide to ret (90 90 ... C3) ----
    ; At least 2 bytes: one NOP + ret
    cmp     rdi, 2
    jb      _not_stub

    xor     ecx, ecx            ; NOP counter
    mov     rbx, rdi
    dec     rbx                 ; leave room for final byte check

_nop_loop:
    cmp     rcx, rbx
    jge     _not_stub           ; ran out of bytes without finding ret
    movzx   eax, byte ptr [rsi+rcx]
    cmp     al, 090h
    jne     _nop_check_ret
    inc     rcx
    jmp     _nop_loop

_nop_check_ret:
    ; If we had NOPs and the next byte is ret, it's a stub
    test    ecx, ecx
    jz      _not_stub           ; no NOPs found
    cmp     al, 0C3h            ; is it ret?
    je      _is_stub

_not_stub:
    xor     eax, eax            ; return 0 (not a stub)
    jmp     _epilog

_is_stub:
    mov     eax, 1              ; return 1 (stub detected)

_epilog:
    pop     rdi
    pop     rsi
    pop     rbx
    pop     rbp
    ret

IsStubFunction ENDP

END
