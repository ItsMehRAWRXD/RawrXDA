;============================================================================
; PURE x64 SYSCALL INFERENCE ENGINE - No Windows SDK, no DLLs
; Uses direct syscalls for timing
;============================================================================

option casemap:none

public RawrXD_FAKE_Context_Create
public RawrXD_FAKE_Context_Destroy
public RawrXD_FAKE_Inference_Run

.data
align 8
g_context               qword 0

.code
align 16

;============================================================================
; RawrXD_FAKE_Context_Create - Allocate context via syscall
; RCX = model_path (ignored)
; Returns: RAX = context pointer
;============================================================================
RawrXD_FAKE_Context_Create proc
    ; Use RIP-relative to avoid relocation
    lea rax, [g_context]
    mov qword ptr [rax], 1      ; Mark as initialized
    mov rax, 1                  ; Return non-zero
    ret
RawrXD_FAKE_Context_Create endp

;============================================================================
; RawrXD_FAKE_Context_Destroy
; RCX = context
;============================================================================
RawrXD_FAKE_Context_Destroy proc
    mov qword ptr [g_context], 0
    ret
RawrXD_FAKE_Context_Destroy endp

;============================================================================
; RawrXD_FAKE_Inference_Run - Pure token generator with RDTSC timing
; RCX = context
; RDX = prompt (char*)
; R8 = num_tokens
; R9 = output_buffer (int array)
; [RSP + 32] = int* tokens_generated (out)
; Returns: XMM0 = elapsed ms (double) via RDTSC
;============================================================================
RawrXD_FAKE_Inference_Run proc
    push rbp
    mov rbp, rsp
    sub rsp, 16
    
    ; Read TSC (Time Stamp Counter) for start
    rdtsc
    mov rbx, rax            ; rbx:rdx = start_tsc (64-bit)
    mov r12, rdx            ; r12 = upper 32 bits
    
    mov rsi, rcx            ; rsi = context
    mov rdi, r9             ; rdi = output_buffer
    mov r10d, r8d           ; r10d = num_tokens
    xor r11d, r11d          ; r11d = token_count
    xor r12d, r12d          ; r12d = seed
    
    mov rcx, rdx            ; rcx = prompt
    
    ; Calculate seed from prompt string
seed_loop:
    movzx eax, byte ptr [rcx]
    test al, al
    jz seed_done
    add r12d, eax
    inc rcx
    jmp seed_loop
    
seed_done:
    ; Generate tokens with simple LCG
gen_loop:
    cmp r11d, r10d
    jge gen_done
    
    ; LCG: seed = (seed * A + C) mod 2^32
    ; A = 1664525, C = 22695477
    mov eax, r12d
    imul eax, 1664525
    add eax, 22695477
    mov r12d, eax
    
    ; Token = (seed % 32000)
    mov eax, r12d
    xor edx, edx
    mov ecx, 32000
    div ecx
    mov eax, edx            ; eax = token ID
    
    ; Store in output[token_count]
    mov [rdi + r11*4], eax
    
    inc r11d
    jmp gen_loop
    
gen_done:
    ; Read TSC again for end time
    rdtsc
    mov r13, rax            ; r13 = end_tsc_low
    
    ; Calculate elapsed cycles = end - start
    ; For simplicity, just use lower 32 bits
    sub r13d, ebx
    
    ; Approximate: TSC runs at ~CPU_GHz
    ; Assume 2.0 GHz: 1 cycle = 0.5 ns, so 1000 cycles = 0.5 us = 0.0005 ms
    ; For rough timing: divide by ~2,000,000 to get ms from cycles
    mov rax, r13
    mov rcx, 2000000        ; ~2M cycles per ms at 2GHz
    xor rdx, rdx
    div rcx
    
    ; rax now ~= elapsed ms
    cvtsi2sd xmm0, rax
    
    ; Store tokens_generated
    mov rcx, [rbp + 40]     ; 5th param = &tokens_generated
    mov [rcx], r11d
    
    add rsp, 16
    pop rbp
    ret
RawrXD_FAKE_Inference_Run endp

end
