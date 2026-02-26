; src/direct_io/RAWRXD_BURSTCORE_X7.asm
; ════════════════════════════════════════════════════════════════════════════════
; RAWRXD_BURSTCORE_X7.ASM
; Tier-7 Direct I/O Burst Executor (Ultra Mode)
; ════════════════════════════════════════════════════════════════════════════════

OPTION PROLOGUE:NONE
OPTION EPILOGUE:NONE


; ─── Cross-module symbol resolution ───
INCLUDE rawrxd_master.inc

EXTERN GetBurstPlan:PROC                  ; out: rax = pointer to tensor ID array
EXTERN GetBurstCount:PROC                 ; out: eax = tensor count
EXTERN GetTensorOffset:PROC                ; rcx = tensor_id → rax=offset
EXTERN GetTensorSize:PROC                  ; rcx = tensor_id → rax=size
EXTERN ResolveZonePointer:PROC            ; rcx = zone_index → rax=ptr
EXTERN DirectIO_Prefetch:PROC             ; rcx=ctx, rdx=offset, r8=size, r9=dst
EXTERN VulkanDMA_RegisterTensor:PROC      ; rcx=tensor_id, rdx=ptr, r8=size
EXTERN g_pDirectIOCtx:QWORD
EXTERN g_BurstTick:QWORD                  ; incremented once per burst

.const
LaneMask       EQU 3                      ; ZoneIndex % 4

.code

BurstExecute PROC
    push rbp
    mov rbp, rsp
    push rbx
    push rsi
    push rdi
    push r12
    push r13
    push r14
    push r15
    sub rsp, 40               ; Shadow space for function calls

    ; rbx = base tensor list pointer
    call GetBurstPlan
    mov rbx, rax

    ; rdi = count
    call GetBurstCount
    mov rdi, rax

    xor rsi, rsi              ; i = 0

_loop:
    cmp rsi, rdi
    jge _done

    ; tensor_id = *(rbx + rsi * 4)
    mov eax, DWORD PTR [rbx + rsi * 4]
    mov r12, rax             ; r12 = tensor_id

    ; Get offset
    mov rcx, r12
    call GetTensorOffset     ; rax=offset
    mov r13, rax             ; offset

    ; Get size
    mov rcx, r12
    call GetTensorSize       ; rax=size
    mov r14, rax             ; size

    ; Determine lane zone index = (i % LaneCount)
    mov rax, rsi
    and rax, LaneMask
    mov rcx, rax             ; zone_index
    call ResolveZonePointer  ; → rax = buffer ptr
    mov r15, rax

    ; Issue prefetch: DirectIO_Prefetch(ctx, offset, size, buffer)
    mov rcx, g_pDirectIOCtx
    mov rdx, r13             ; offset
    mov r8, r14              ; size
    mov r9, r15              ; dst
    call DirectIO_Prefetch

    ; Register with Vulkan immediately
    mov rcx, r12             ; tensor_id
    mov rdx, r15             ; ptr
    mov r8, r14              ; size
    call VulkanDMA_RegisterTensor

    inc rsi
    jmp _loop

_done:
    ; Tick burst
    inc QWORD PTR g_BurstTick
    
    add rsp, 40
    pop r15
    pop r14
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    pop rbp
    ret
BurstExecute ENDP

END
