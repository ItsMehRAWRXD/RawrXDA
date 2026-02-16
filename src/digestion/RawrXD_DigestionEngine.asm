; RawrXD_DigestionEngine.asm
; Minimal MASM64 stub for RunDigestionEngine used by RawrXD IDE
; Assemble with: ml64 /c /FoRawrXD_DigestionEngine.obj RawrXD_DigestionEngine.asm

.code

; DWORD RunDigestionEngine(
;   LPCWSTR szSource,   ; RCX
;   LPCWSTR szOutput,   ; RDX
;   DWORD   dwChunk,    ; R8D
;   DWORD   dwThreads,  ; R9D
;   DWORD   dwFlags,    ; [RSP+40]
;   LPVOID  pCtx        ; [RSP+48]
; );
RunDigestionEngine PROC FRAME
    push    rbx
    push    rsi
    push    rdi
    push    r12
    push    r13
    push    r14
    push    r15
    .endprolog

    ; Validate inputs
    mov     r12, rcx        ; szSource
    mov     r13, rdx        ; szOutput
    test    r12, r12
    jz      invalid_arg
    test    r13, r13
    jz      invalid_arg

    ; Stub: real AVX-512 digestion pipeline not yet implemented.
    mov     eax, 120        ; ERROR_CALL_NOT_IMPLEMENTED
    jmp     done

invalid_arg:
    mov     eax, 87         ; E_DIGEST_INVALIDARG / ERROR_INVALID_PARAMETER

done:
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret

RunDigestionEngine ENDP

END
