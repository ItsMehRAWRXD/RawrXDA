; exception/SEH_wrapper.asm
; Structured Exception Handling wrapper for native functions
; Provides safe execution context for inference operations

.code
ALIGN 16

EXTERNDEF seh_safe_execute:PROC
EXTERNDEF seh_get_last_error:PROC

; ============================================================================
; int seh_safe_execute(void (*func)(void*), void* context);
; RCX = function pointer
; RDX = context pointer
; Returns: 0 on success, exception code on failure
; ============================================================================
seh_safe_execute PROC FRAME
    push    rbx
    .pushreg rbx
    push    rsi
    .pushreg rsi
    push    rdi
    .pushreg rdi
    sub     rsp, 40
    .allocstack 40
    .endprolog

    ; Set up SEH frame
    .allocstack 8
    push    offset exception_handler
    push    fs:[0]
    mov     fs:[0], rsp

    ; Call the function
    mov     rax, rcx
    mov     rcx, rdx
    call    rax

    ; Success - clean up SEH frame
    mov     eax, 0          ; Return 0 for success

cleanup:
    pop     fs:[0]
    add     rsp, 8

    add     rsp, 40
    pop     rdi
    pop     rsi
    pop     rbx
    ret

exception_handler:
    ; Exception occurred
    mov     rax, [rsp + 8]  ; Exception record
    mov     eax, [rax]      ; Exception code
    jmp     cleanup

seh_safe_execute ENDP

; ============================================================================
; DWORD seh_get_last_error();
; Returns the last Win32 error code
; ============================================================================
seh_get_last_error PROC
    call    GetLastError
    ret
seh_get_last_error ENDP

END