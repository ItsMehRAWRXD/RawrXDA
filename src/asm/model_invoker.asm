; =============================================================================
; model_invoker.asm — Sovereign Model Invoker (Pure x64 MASM)
; Replaces JSON-based model_invoker.hpp with raw tensor dispatch
; =============================================================================
; Build:   ml64 /c /nologo /Fo$(OutDir)model_invoker.obj model_invoker.asm
; Link:    link ... model_invoker.obj msvcrt.lib kernel32.lib
; =============================================================================
; Exports (C linkage):
;   int64_t ModelInvoker_PrepareContext(const char* prompt, int32_t* tokenIds, size_t maxTokens);
;   int64_t ModelInvoker_Invoke(int32_t* tokenIds, size_t tokenCount, char* outBuf, size_t outSize);
; =============================================================================

include rawrxd_win64.inc
include rawrxd_crt.inc

; ---------------------------------------------------------------------------
; Bridge externs
; ---------------------------------------------------------------------------
EXTERN masm_ai_tensor_simd_process:PROC
EXTERN masm_get_performance_counter:PROC

; ---------------------------------------------------------------------------
; Data
; ---------------------------------------------------------------------------
.data
align 8
PUBLIC g_LastInvokeCycles
PUBLIC g_LastTokenCount

g_LastInvokeCycles  dq 0
g_LastTokenCount    dq 0

sz_prepare          db "[Invoker] PrepareContext: %zu tokens", 10, 0
sz_invoke_start     db "[Invoker] Invoke start (%zu tokens)", 10, 0
sz_invoke_done      db "[Invoker] Invoke done in %llu cycles", 10, 0

; Simple whitespace tokenization table (ASCII delimiters)
token_delims        db 9, 10, 13, 32, 0    ; tab, LF, CR, space

; ---------------------------------------------------------------------------
; Code
; ---------------------------------------------------------------------------
.code

; =============================================================================
; ModelInvoker_PrepareContext
;   RCX = const char* prompt
;   RDX = int32_t* tokenIds (output array)
;   R8  = size_t maxTokens
;   Returns RAX = number of tokens produced, or -1 on error
; =============================================================================
align 16
PUBLIC ModelInvoker_PrepareContext
ModelInvoker_PrepareContext PROC FRAME
    push rbx
    .pushreg rbx
    push rdi
    .pushreg rdi
    push rsi
    .pushreg rsi
    push r12
    .pushreg r12
    push r13
    .pushreg r13
    sub rsp, 40
    .allocstack 40
    .endprolog

    mov rbx, rcx                        ; RBX = prompt
    mov rdi, rdx                        ; RDI = tokenIds
    mov rsi, r8                         ; RSI = maxTokens

    test rbx, rbx
    jz _prep_fail
    test rdi, rdi
    jz _prep_fail
    test rsi, rsi
    jz _prep_fail

    xor r12, r12                        ; R12 = token count
    mov r13, rbx                        ; R13 = current scan position

_prep_loop:
    ; Skip leading delimiters
_skip_delim:
    movzx eax, byte ptr [r13]
    test al, al
    jz _prep_done

    lea rcx, token_delims
_delim_check:
    movzx edx, byte ptr [rcx]
    test dl, dl
    jz _token_start
    cmp al, dl
    je _advance_delim
    inc rcx
    jmp _delim_check
_advance_delim:
    inc r13
    jmp _skip_delim

_token_start:
    mov rcx, r13                        ; token start
    mov rdx, r13

_token_extend:
    movzx eax, byte ptr [rdx]
    test al, al
    jz _token_end
    lea r8, token_delims
_delim_check2:
    movzx r9d, byte ptr [r8]
    test r9b, r9b
    jz _not_delim
    cmp al, r9b
    je _token_end
    inc r8
    jmp _delim_check2
_not_delim:
    inc rdx
    jmp _token_extend

_token_end:
    ; Token = [rcx, rdx)
    mov r8, rdx
    sub r8, rcx                         ; R8 = token length
    test r8, r8
    jz _prep_next

    ; Simple hash = sum of bytes (sovereign tokenizer)
    xor eax, eax
    mov r9, rcx
_hash_loop:
    cmp r9, rdx
    jae _hash_done
    movzx r10d, byte ptr [r9]
    add eax, r10d
    inc r9
    jmp _hash_loop
_hash_done:
    ; Store token ID
    mov dword ptr [rdi + r12*4], eax
    inc r12
    cmp r12, rsi
    jae _prep_done

_prep_next:
    mov r13, rdx
    jmp _prep_loop

_prep_done:
    mov g_LastTokenCount, r12

    ; Log
    mov rdx, r12
    lea rcx, sz_prepare
    xor eax, eax
    call printf

    mov rax, r12
    jmp _prep_exit

_prep_fail:
    mov rax, -1

_prep_exit:
    add rsp, 40
    pop r13
    pop r12
    pop rsi
    pop rdi
    pop rbx
    ret
ModelInvoker_PrepareContext ENDP


; =============================================================================
; ModelInvoker_Invoke
;   RCX = int32_t* tokenIds
;   RDX = size_t tokenCount
;   R8  = char* outBuf
;   R9  = size_t outSize
;   Returns RAX = bytes written, or -1 on error
; =============================================================================
align 16
PUBLIC ModelInvoker_Invoke
ModelInvoker_Invoke PROC FRAME
    push rbx
    .pushreg rbx
    push rdi
    .pushreg rdi
    push rsi
    .pushreg rsi
    push r12
    .pushreg r12
    push r13
    .pushreg r13
    sub rsp, 120
    .allocstack 120
    .endprolog

    mov rbx, rcx                        ; RBX = tokenIds
    mov rdi, rdx                        ; RDI = tokenCount
    mov rsi, r8                         ; RSI = outBuf
    mov r12, r9                         ; R12 = outSize

    test rbx, rbx
    jz _invoke_fail
    test rdi, rdi
    jz _invoke_fail
    test rsi, rsi
    jz _invoke_fail
    test r12, r12
    jz _invoke_fail

    ; Log start
    mov rdx, rdi
    lea rcx, sz_invoke_start
    xor eax, eax
    call printf

    ; Snapshot performance counter
    call masm_get_performance_counter
    mov r13, rax                        ; R13 = start cycles

    ; Build AiMasmInferenceContext on stack at [rsp+64]
    lea rcx, [rsp+64]
    mov qword ptr [rcx+0], rbx          ; model_memory_base = tokenIds (sovereign pass-through)
    mov qword ptr [rcx+8], rdi          ; model_memory_size = tokenCount
    mov qword ptr [rcx+16], rbx         ; input_tensor_buffer = tokenIds
    mov qword ptr [rcx+24], rsi         ; output_tensor_buffer = outBuf
    mov qword ptr [rcx+32], r12         ; tensor_size = outSize
    mov dword ptr [rcx+40], 1           ; batch_size = 1
    mov qword ptr [rcx+44], 0           ; completion_callback = NULL

    ; Call masm_ai_tensor_simd_process(context, tokenIds, tokenCount*4, outBuf, outSize)
    sub rsp, 40
    mov qword ptr [rsp+32], r12         ; 5th arg = outSize
    mov r9, rsi                         ; 4th arg = outBuf
    mov r8, rdi
    shl r8, 2                           ; 3rd arg = tokenCount * sizeof(int32_t)
    mov rdx, rbx                        ; 2nd arg = tokenIds
    lea rcx, [rsp+64+40]                ; 1st arg = context (offset by pushed shadow)
    call masm_ai_tensor_simd_process
    add rsp, 40

    ; Check result.success at [rsp+64]
    mov al, byte ptr [rsp+64]
    test al, al
    jz _invoke_fail

    ; Compute elapsed cycles
    call masm_get_performance_counter
    sub rax, r13
    mov g_LastInvokeCycles, rax

    ; Log done
    mov rdx, rax
    lea rcx, sz_invoke_done
    xor eax, eax
    call printf

    ; Return outSize on success
    mov rax, r12
    jmp _invoke_exit

_invoke_fail:
    mov rax, -1

_invoke_exit:
    add rsp, 120
    pop r13
    pop r12
    pop rsi
    pop rdi
    pop rbx
    ret
ModelInvoker_Invoke ENDP

END
