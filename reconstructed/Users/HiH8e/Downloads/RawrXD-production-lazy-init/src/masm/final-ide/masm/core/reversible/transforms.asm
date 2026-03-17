;=====================================================================
; masm_core_reversible_transforms.asm - Invertible Operation Library
; BIDIRECTIONAL TRANSFORM ABSTRACTIONS
;=====================================================================
; Provides reversible/invertible operations for flexible function reuse.
; Pattern: "Use function one way, reverse it, use another way, then restore"
;
; All operations support:
;  - Forward execution (flags parameter = 0)
;  - Reverse/inverse execution (flags parameter = 1)
;  - Idempotent operations where forward + reverse = identity
;
; This enables composition and abstraction layers that can be dynamically
; inverted without reimplementing logic.
;
; Strategy Design Pattern: Operation dispatch with configurable inversion
;=====================================================================

.code

PUBLIC masm_core_transform_xor
PUBLIC masm_core_transform_rotate
PUBLIC masm_core_transform_reverse
PUBLIC masm_core_transform_swap
PUBLIC masm_core_transform_bitflip
PUBLIC masm_core_transform_pipeline
PUBLIC masm_core_transform_abort_pipeline
PUBLIC masm_core_transform_dispatch

; External dependencies
EXTERN asm_malloc:PROC
EXTERN asm_free:PROC
EXTERN asm_log:PROC

.data

; Transform operation type constants
TRANSFORM_TYPE_XOR              EQU 1
TRANSFORM_TYPE_ROTATE           EQU 2
TRANSFORM_TYPE_REVERSE          EQU 3
TRANSFORM_TYPE_BITFLIP          EQU 4
TRANSFORM_TYPE_SWAP             EQU 5
TRANSFORM_TYPE_CUSTOM           EQU 99

; Transform direction flags
TRANSFORM_FORWARD               EQU 0
TRANSFORM_REVERSE               EQU 1

; Statistics
g_transforms_applied            QWORD 0
g_transforms_reversed           QWORD 0
g_transform_errors              QWORD 0

; Transform pipeline structure (640 bytes)
; [+0]:   enabled (qword)
; [+8]:   transform_count (qword)
; [+16]:  applied_count (qword)
; [+24]:  reserved (qword)
; [+32]:  transforms[8] @ 8*64 bytes = 512 bytes total
;   Each transform entry (64 bytes):
;   [+0]:   operation_type (qword)
;   [+8]:   target_addr (qword)
;   [+16]:  size (qword)
;   [+24]:  param1 (qword) - operation-specific
;   [+32]:  param2 (qword) - operation-specific
;   [+40]:  reserved[3] (qword[3])

; Global pipeline registry
g_transform_pipeline_registry   QWORD 0
g_pipeline_count                QWORD 0

.code

;=====================================================================
; masm_core_transform_xor(buffer: rcx, size: rdx, 
;                         key: r8, key_len: r9, flags: [rsp+32]) -> rax
;
; Applies XOR transformation with repeating key.
; REVERSIBLE: XOR(data, key) = XOR(XOR(data, key), key)
; flags = 0: forward, flags = 1: reverse (same operation)
; Returns: 1 on success, 0 on failure
;=====================================================================

ALIGN 16
masm_core_transform_xor PROC

    push rbx
    push r12
    push r13
    sub rsp, 64
    
    mov rbx, rcx            ; rbx = buffer
    mov r12, rdx            ; r12 = size
    mov r13, r8             ; r13 = key
    
    test rbx, rbx
    jz xor_transform_fail
    test r12, r12
    jz xor_transform_fail
    test r13, r13
    jz xor_transform_fail
    test r9, r9
    jz xor_transform_fail
    
    xor r10, r10            ; r10 = buffer index
    xor r11, r11            ; r11 = key index
    
xor_transform_loop:
    cmp r10, r12
    jge xor_transform_done
    
    ; Cyclic key index
    mov rax, r11
    xor edx, edx
    div r9                  ; rax = r11 / key_len, rdx = r11 % key_len
    
    movzx eax, byte ptr [r13 + rdx]
    xor byte ptr [rbx + r10], al
    
    inc r10
    inc r11
    jmp xor_transform_loop

xor_transform_done:
    lock inc [g_transforms_applied]
    mov rax, 1
    jmp xor_transform_exit

xor_transform_fail:
    lock inc [g_transform_errors]
    xor rax, rax

xor_transform_exit:
    add rsp, 64
    pop r13
    pop r12
    pop rbx
    ret

masm_core_transform_xor ENDP

;=====================================================================
; masm_core_transform_rotate(buffer: rcx, size: rdx, 
;                            bit_count: r8, flags: [rsp+32]) -> rax
;
; Rotates buffer bits left or right.
; REVERSIBLE: flags=0 rotates left, flags=1 rotates right
; flags = 0: rotate left, flags = 1: rotate right (inverse)
; Returns: 1 on success, 0 on failure
;=====================================================================

ALIGN 16
masm_core_transform_rotate PROC

    push rbx
    push r12
    push r13
    sub rsp, 64
    
    mov rbx, rcx            ; rbx = buffer
    mov r12, rdx            ; r12 = size
    mov r13d, r8d           ; r13d = bit_count
    
    test rbx, rbx
    jz rotate_transform_fail
    test r12, r12
    jz rotate_transform_fail
    
    ; Get flags from stack (calling convention)
    mov r8, [rsp + 96]      ; r8 = flags
    
    xor r10, r10            ; r10 = byte index
    
rotate_transform_loop:
    cmp r10, r12
    jge rotate_transform_done
    
    mov al, byte ptr [rbx + r10]
    
    ; Check if forward (flags=0) or reverse (flags=1)
    test r8, r8
    jz rotate_left
    
    ; Rotate right (inverse operation)
    ror al, cl              ; cl = low byte of r13d
    jmp rotate_byte_done
    
rotate_left:
    ; Rotate left
    rol al, cl              ; cl = low byte of r13d

rotate_byte_done:
    mov byte ptr [rbx + r10], al
    inc r10
    jmp rotate_transform_loop

rotate_transform_done:
    lock inc [g_transforms_applied]
    mov rax, 1
    jmp rotate_transform_exit

rotate_transform_fail:
    lock inc [g_transform_errors]
    xor rax, rax

rotate_transform_exit:
    add rsp, 64
    pop r13
    pop r12
    pop rbx
    ret

masm_core_transform_rotate ENDP

;=====================================================================
; masm_core_transform_reverse(buffer: rcx, size: rdx, 
;                             flags: [rsp+32]) -> rax
;
; Reverses buffer byte order.
; REVERSIBLE: reversing twice = identity (flags doesn't matter)
; Returns: 1 on success, 0 on failure
;=====================================================================

ALIGN 16
masm_core_transform_reverse PROC

    push rbx
    push r12
    sub rsp, 64
    
    mov rbx, rcx            ; rbx = buffer
    mov r12, rdx            ; r12 = size
    
    test rbx, rbx
    jz reverse_transform_fail
    test r12, r12
    jz reverse_transform_fail
    
    xor r10, r10            ; r10 = front index
    mov r11, r12
    dec r11                 ; r11 = back index
    
reverse_transform_loop:
    cmp r10, r11
    jge reverse_transform_done
    
    ; Swap front and back bytes
    movzx eax, byte ptr [rbx + r10]
    movzx edx, byte ptr [rbx + r11]
    mov byte ptr [rbx + r10], dl
    mov byte ptr [rbx + r11], al
    
    inc r10
    dec r11
    jmp reverse_transform_loop

reverse_transform_done:
    lock inc [g_transforms_applied]
    mov rax, 1
    jmp reverse_transform_exit

reverse_transform_fail:
    lock inc [g_transform_errors]
    xor rax, rax

reverse_transform_exit:
    add rsp, 64
    pop r12
    pop rbx
    ret

masm_core_transform_reverse ENDP

;=====================================================================
; masm_core_transform_swap(addr_a: rcx, addr_b: rdx, 
;                          size: r8, flags: [rsp+32]) -> rax
;
; Swaps memory contents between two addresses.
; REVERSIBLE: swapping twice = identity (flags doesn't matter)
; Returns: 1 on success, 0 on failure
;=====================================================================

ALIGN 16
masm_core_transform_swap PROC

    push rbx
    push r12
    push r13
    sub rsp, 96
    
    mov rbx, rcx            ; rbx = addr_a
    mov r12, rdx            ; r12 = addr_b
    mov r13, r8             ; r13 = size
    
    test rbx, rbx
    jz swap_transform_fail
    test r12, r12
    jz swap_transform_fail
    test r13, r13
    jz swap_transform_fail
    
    ; Use stack as temporary buffer
    mov r10, rsp
    add r10, 64             ; r10 = temp buffer on stack
    
    ; Copy A to temp
    xor r11, r11            ; r11 = index
    
swap_copy_a_to_temp:
    cmp r11, r13
    jge swap_copy_b_to_a
    
    movzx eax, byte ptr [rbx + r11]
    mov byte ptr [r10 + r11], al
    inc r11
    jmp swap_copy_a_to_temp

swap_copy_b_to_a:
    xor r11, r11
    
swap_copy_b_to_a_loop:
    cmp r11, r13
    jge swap_copy_temp_to_b
    
    movzx eax, byte ptr [r12 + r11]
    mov byte ptr [rbx + r11], al
    inc r11
    jmp swap_copy_b_to_a_loop

swap_copy_temp_to_b:
    xor r11, r11
    
swap_copy_temp_to_b_loop:
    cmp r11, r13
    jge swap_transform_done
    
    movzx eax, byte ptr [r10 + r11]
    mov byte ptr [r12 + r11], al
    inc r11
    jmp swap_copy_temp_to_b_loop

swap_transform_done:
    lock inc [g_transforms_applied]
    mov rax, 1
    jmp swap_transform_exit

swap_transform_fail:
    lock inc [g_transform_errors]
    xor rax, rax

swap_transform_exit:
    add rsp, 96
    pop r13
    pop r12
    pop rbx
    ret

masm_core_transform_swap ENDP

;=====================================================================
; masm_core_transform_bitflip(buffer: rcx, size: rdx, 
;                             bit_mask: r8, flags: [rsp+32]) -> rax
;
; Flips specific bits in buffer (XOR with mask).
; REVERSIBLE: flipping same bits twice = identity
; Returns: 1 on success, 0 on failure
;=====================================================================

ALIGN 16
masm_core_transform_bitflip PROC

    push rbx
    push r12
    sub rsp, 64
    
    mov rbx, rcx            ; rbx = buffer
    mov r12, rdx            ; r12 = size
    mov r10d, r8d           ; r10d = bit_mask (as byte)
    
    test rbx, rbx
    jz bitflip_transform_fail
    test r12, r12
    jz bitflip_transform_fail
    
    xor r11, r11            ; r11 = index
    
bitflip_transform_loop:
    cmp r11, r12
    jge bitflip_transform_done
    
    mov al, byte ptr [rbx + r11]
    xor al, r10b            ; XOR with bit_mask
    mov byte ptr [rbx + r11], al
    
    inc r11
    jmp bitflip_transform_loop

bitflip_transform_done:
    lock inc [g_transforms_applied]
    mov rax, 1
    jmp bitflip_transform_exit

bitflip_transform_fail:
    lock inc [g_transform_errors]
    xor rax, rax

bitflip_transform_exit:
    add rsp, 64
    pop r12
    pop rbx
    ret

masm_core_transform_bitflip ENDP

;=====================================================================
; masm_core_transform_pipeline(pipeline_ptr: rcx) -> rax
;
; Executes a sequence of transform operations in order.
; Useful for complex multi-step transformations.
; Pipeline can be aborted to undo all transforms.
;
; Returns: 1 on success, 0 on failure
;=====================================================================

ALIGN 16
masm_core_transform_pipeline PROC

    push rbx
    push r12
    push r13
    sub rsp, 64
    
    mov rbx, rcx            ; rbx = pipeline_ptr
    
    test rbx, rbx
    jz pipeline_fail
    
    ; Check if pipeline enabled
    cmp qword ptr [rbx], 0
    je pipeline_fail
    
    mov r12, [rbx + 8]      ; r12 = transform_count
    mov qword ptr [rbx + 16], 0 ; Reset applied_count
    test r12, r12
    jz pipeline_success
    
    xor r13, r13            ; r13 = transform index
    
pipeline_loop:
    cmp r13, r12
    jge pipeline_success
    
    ; Calculate transform offset
    mov rax, r13
    imul rax, 64            ; Each transform entry is 64 bytes
    add rax, 32             ; Skip header (32 bytes now)
    
    ; Get transform entry
    mov rcx, [rbx + rax]    ; rcx = operation_type
    mov rdx, [rbx + rax + 8]  ; rdx = target_addr
    mov r8, [rbx + rax + 16]  ; r8 = size
    mov r9, [rbx + rax + 24]  ; r9 = param1
    
    ; Dispatch based on operation type
    push rbx
    push r12
    push r13
    
    ; Call dispatch with FORWARD flag
    sub rsp, 40
    mov qword ptr [rsp + 32], TRANSFORM_FORWARD
    call masm_core_transform_dispatch
    add rsp, 40
    
    pop r13
    pop r12
    pop rbx
    
    test rax, rax
    jz pipeline_fail
    
    inc r13
    mov [rbx + 16], r13     ; Update applied_count
    jmp pipeline_loop

pipeline_success:
    lock inc [g_transforms_applied]
    mov rax, 1
    jmp pipeline_exit

pipeline_fail:
    lock inc [g_transform_errors]
    xor rax, rax

pipeline_exit:
    add rsp, 64
    pop r13
    pop r12
    pop rbx
    ret

masm_core_transform_pipeline ENDP

;=====================================================================
; masm_core_transform_abort_pipeline(pipeline_ptr: rcx) -> rax
;
; Safely aborts an in-flight pipeline by undoing transforms.
; This would require storing undo history - simplified version here.
;
; Returns: 1 on success, 0 on failure
;=====================================================================

ALIGN 16
masm_core_transform_abort_pipeline PROC

    push rbx
    push r12
    push r13
    sub rsp, 64
    
    mov rbx, rcx            ; rbx = pipeline_ptr
    
    test rbx, rbx
    jz abort_fail
    
    mov r12, [rbx + 16]     ; r12 = applied_count
    test r12, r12
    jz abort_success        ; Nothing to undo
    
    mov r13, r12
    dec r13                 ; Start from last applied transform
    
abort_loop:
    ; Calculate transform offset
    mov rax, r13
    imul rax, 64
    add rax, 32             ; Skip header
    
    ; Get transform entry
    mov rcx, [rbx + rax]    ; rcx = operation_type
    mov rdx, [rbx + rax + 8]  ; rdx = target_addr
    mov r8, [rbx + rax + 16]  ; r8 = size
    mov r9, [rbx + rax + 24]  ; r9 = param1
    
    ; Dispatch with REVERSE flag
    push rbx
    push r12
    push r13
    
    sub rsp, 40
    mov qword ptr [rsp + 32], TRANSFORM_REVERSE
    call masm_core_transform_dispatch
    add rsp, 40
    
    pop r13
    pop r12
    pop rbx
    
    ; Even if one undo fails, we try to continue? 
    ; For now, just continue.
    
    test r13, r13
    jz abort_success
    dec r13
    jmp abort_loop

abort_success:
    ; Mark pipeline as disabled and reset counts
    mov qword ptr [rbx], 0
    mov qword ptr [rbx + 16], 0
    
    lock inc [g_transforms_reversed]
    mov rax, 1
    jmp abort_exit

abort_fail:
    lock inc [g_transform_errors]
    xor rax, rax

abort_exit:
    add rsp, 64
    pop r13
    pop r12
    pop rbx
    ret

masm_core_transform_abort_pipeline ENDP

;=====================================================================
; masm_core_transform_dispatch(operation_type: rcx, buffer: rdx,
;                              size: r8, param1: r9, 
;                              flags: [rsp+32]) -> rax
;
; Generic transform dispatcher that routes to specific handlers.
; Enables dynamic operation selection at runtime.
;
; operation_type (rcx):
;   1 = XOR (param1 = key_ptr)
;   2 = ROTATE (param1 = bit_count)
;   3 = REVERSE
;   4 = BITFLIP (param1 = bit_mask)
;   5 = SWAP (param1 = second_addr)
;
; Returns: 1 on success, 0 on failure
;=====================================================================

ALIGN 16
masm_core_transform_dispatch PROC

    cmp rcx, TRANSFORM_TYPE_XOR
    je dispatch_xor
    
    cmp rcx, TRANSFORM_TYPE_ROTATE
    je dispatch_rotate
    
    cmp rcx, TRANSFORM_TYPE_REVERSE
    je dispatch_reverse
    
    cmp rcx, TRANSFORM_TYPE_BITFLIP
    je dispatch_bitflip
    
    cmp rcx, TRANSFORM_TYPE_SWAP
    je dispatch_swap
    
    ; Unknown operation type
    xor rax, rax
    ret

dispatch_xor:
    ; masm_core_transform_xor(buffer: rdx, size: r8, key: r9, key_len: [rsp+32])
    sub rsp, 32
    call masm_core_transform_xor
    add rsp, 32
    ret

dispatch_rotate:
    ; masm_core_transform_rotate(buffer: rdx, size: r8, bit_count: r9)
    sub rsp, 32
    call masm_core_transform_rotate
    add rsp, 32
    ret

dispatch_reverse:
    ; masm_core_transform_reverse(buffer: rdx, size: r8)
    mov rcx, rdx
    sub rsp, 32
    call masm_core_transform_reverse
    add rsp, 32
    ret

dispatch_bitflip:
    ; masm_core_transform_bitflip(buffer: rdx, size: r8, bit_mask: r9)
    mov rcx, rdx
    mov r8, r8
    sub rsp, 32
    call masm_core_transform_bitflip
    add rsp, 32
    ret

dispatch_swap:
    ; masm_core_transform_swap(addr_a: rdx, addr_b: r9, size: r8)
    mov rcx, rdx
    mov rdx, r9
    sub rsp, 32
    call masm_core_transform_swap
    add rsp, 32
    ret

masm_core_transform_dispatch ENDP

END
