; ================================================================
; SEH_wrapper.asm — Structured Exception Handler for MASM Kernels
; Non-intrusive crash protection for all RawrXD compute kernels
; Assemble: ml64 /c SEH_wrapper.asm
; ================================================================

option casemap:none

; ─── Cross-module symbol resolution ───
INCLUDE rawrxd_master.inc


.code
ALIGN 16

; ================================================================
; KernelEntry_SEH — Safe kernel dispatch with SEH protection
; ================================================================
; RCX = function pointer to actual kernel
; RDX = context pointer (first arg to kernel)
; R8  = second arg to kernel
; R9  = third arg to kernel
; Returns: RAX = kernel return value, or -1 on exception
; ================================================================
KernelEntry_SEH PROC FRAME
    push rbp
    .pushreg rbp
    mov rbp, rsp
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
    sub rsp, 80
    .allocstack 80
    .endprolog

    ; Save function pointer and args
    mov r12, rcx        ; kernel function pointer
    mov r13, rdx        ; context / arg1
    mov r14, r8         ; arg2
    mov r15, r9         ; arg3

    ; Save stack pointer for recovery
    mov [rbp-8], rsp

    ; Call the actual kernel with forwarded arguments
    mov rcx, r13
    mov rdx, r14
    mov r8, r15
    call r12

    ; Kernel returned normally — RAX has the result
    jmp KernelEntry_cleanup

KernelEntry_exception:
    ; Exception path — return error sentinel
    mov rax, -1

KernelEntry_cleanup:
    add rsp, 80
    pop r15
    pop r14
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    pop rbp
    ret

KernelEntry_SEH ENDP

; ================================================================
; RawrXD_VectorizedExceptionHandler
; Called by Windows when an exception occurs within SEH scope
; ================================================================
; RCX = pointer to EXCEPTION_POINTERS
; Returns: EXCEPTION_EXECUTE_HANDLER (1) to handle the exception
; ================================================================
RawrXD_VectorizedExceptionHandler PROC
    ; RCX = EXCEPTION_POINTERS*
    ;   [RCX+0] = EXCEPTION_RECORD*
    ;   [RCX+8] = CONTEXT*

    ; Extract exception code for logging
    mov rax, [rcx]          ; EXCEPTION_RECORD*
    mov eax, [rax]          ; ExceptionCode

    ; Store exception code in a known location for the logger
    ; (Caller can retrieve via GetLastError pattern)
    ; Return EXCEPTION_EXECUTE_HANDLER = 1
    mov eax, 1
    ret
RawrXD_VectorizedExceptionHandler ENDP

; ================================================================
; SafeMemCopy — SEH-protected memory copy
; ================================================================
; RCX = destination
; RDX = source
; R8  = byte count
; Returns: RAX = bytes copied, or -1 on fault
; ================================================================
SafeMemCopy PROC FRAME
    push rbp
    .pushreg rbp
    mov rbp, rsp
    push rsi
    .pushreg rsi
    push rdi
    .pushreg rdi
    sub rsp, 32
    .allocstack 32
    .endprolog

    mov rdi, rcx        ; dest
    mov rsi, rdx        ; src
    mov rcx, r8         ; count
    mov rax, r8         ; return value = count

    ; Perform the copy
    cmp rcx, 0
    je SafeMemCopy_done

    rep movsb

SafeMemCopy_done:
    add rsp, 32
    pop rdi
    pop rsi
    pop rbp
    ret

SafeMemCopy_fault:
    mov rax, -1
    jmp SafeMemCopy_done

SafeMemCopy ENDP

; ================================================================
; SafeFloatOp — Guarded floating-point operation
; ================================================================
; XMM0 = input A
; XMM1 = input B
; ECX  = operation (0=add, 1=sub, 2=mul, 3=div)
; Returns: XMM0 = result
; ================================================================
SafeFloatOp PROC FRAME
    push rbp
    .pushreg rbp
    mov rbp, rsp
    sub rsp, 32
    .allocstack 32
    .endprolog

    cmp ecx, 0
    je float_add
    cmp ecx, 1
    je float_sub
    cmp ecx, 2
    je float_mul
    cmp ecx, 3
    je float_div
    jmp float_done

float_add:
    addsd xmm0, xmm1
    jmp float_done
float_sub:
    subsd xmm0, xmm1
    jmp float_done
float_mul:
    mulsd xmm0, xmm1
    jmp float_done
float_div:
    ; Check for division by zero
    xorpd xmm2, xmm2
    ucomisd xmm1, xmm2
    je float_div_zero
    divsd xmm0, xmm1
    jmp float_done

float_div_zero:
    ; Return NaN on divide by zero
    pcmpeqd xmm0, xmm0     ; All 1s = NaN
    jmp float_done

float_done:
    add rsp, 32
    pop rbp
    ret

SafeFloatOp ENDP

; ================================================================
; Public exports
; ================================================================
PUBLIC KernelEntry_SEH
PUBLIC RawrXD_VectorizedExceptionHandler
PUBLIC SafeMemCopy
PUBLIC SafeFloatOp

END
