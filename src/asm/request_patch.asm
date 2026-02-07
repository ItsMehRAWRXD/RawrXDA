; =============================================================================
; request_patch.asm — Server Layer ASM Kernel
; =============================================================================
; Request/response interception stubs for Layer 3 server hotpatching.
; Called by gguf_server_hotpatch.cpp for high-performance buffer inspection.
;
; Exports:
;   asm_intercept_request   — Inspect/modify request buffer
;   asm_intercept_response  — Inspect/modify response buffer
;
; Architecture: x64 MASM | Windows ABI | No exceptions | No CRT
; Build: ml64.exe /c /Zi /Zd /Fo request_patch.obj request_patch.asm
; Rule: NO SOURCE FILE IS TO BE SIMPLIFIED
; =============================================================================

INCLUDE RawrXD_Common.inc

; =============================================================================
;                             EXPORTS
; =============================================================================
PUBLIC asm_intercept_request
PUBLIC asm_intercept_response

; =============================================================================
;                            DATA
; =============================================================================
.data
ALIGN 8
g_ReqInterceptCount     DQ      0
g_RespInterceptCount    DQ      0

; =============================================================================
;                            CODE
; =============================================================================
.code

; =============================================================================
; asm_intercept_request
; Inspect a request buffer for anomalies or injection points.
; Currently: validates non-null, non-zero length, increments counter.
;
; RCX = request buffer pointer (void*)
; RDX = request buffer length
;
; Returns: EAX = 0 on success, -1 on invalid input
; =============================================================================
asm_intercept_request PROC FRAME
    push    rbx
    .pushreg rbx
    .endprolog

    ; Validate inputs
    test    rcx, rcx
    jz      @@ir_fail
    test    rdx, rdx
    jz      @@ir_fail

    ; Increment intercept counter (atomic)
    lock inc QWORD PTR [g_ReqInterceptCount]

    ; Validate first 4 bytes are not all zero (basic sanity)
    cmp     rdx, 4
    jb      @@ir_ok             ; Too short to check, just pass
    mov     eax, DWORD PTR [rcx]
    test    eax, eax
    jz      @@ir_fail           ; All-zero header = suspicious

@@ir_ok:
    xor     eax, eax            ; Return 0 = success
    jmp     @@ir_done

@@ir_fail:
    mov     eax, -1             ; Return -1 = invalid

@@ir_done:
    pop     rbx
    ret
asm_intercept_request ENDP

; =============================================================================
; asm_intercept_response
; Inspect a response buffer for anomalies or rewriting opportunities.
; Currently: validates non-null, non-zero length, increments counter.
;
; RCX = response buffer pointer (void*)
; RDX = response buffer length
;
; Returns: EAX = 0 on success, -1 on invalid input
; =============================================================================
asm_intercept_response PROC FRAME
    push    rbx
    .pushreg rbx
    .endprolog

    test    rcx, rcx
    jz      @@isp_fail
    test    rdx, rdx
    jz      @@isp_fail

    lock inc QWORD PTR [g_RespInterceptCount]

    ; Basic response sanity: length must be reasonable (< 64MB)
    mov     rax, 67108864       ; 64 * 1024 * 1024
    cmp     rdx, rax
    ja      @@isp_fail          ; Suspiciously large response

    xor     eax, eax
    jmp     @@isp_done

@@isp_fail:
    mov     eax, -1

@@isp_done:
    pop     rbx
    ret
asm_intercept_response ENDP

END
