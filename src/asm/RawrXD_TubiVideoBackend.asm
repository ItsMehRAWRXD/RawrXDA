OPTION CASEMAP:NONE

PUBLIC RawrXD_TubiFillFrameRGBA

.code

; int RawrXD_TubiFillFrameRGBA(uint8_t* dst, uint32_t width, uint32_t height, uint32_t stride,
;                              uint32_t frameIndex, uint32_t totalFrames, uint32_t seed, uint32_t flags)
; RCX=dst, EDX=width, R8D=height, R9D=stride
; [rsp+40]=frameIndex, [rsp+48]=totalFrames, [rsp+56]=seed, [rsp+64]=flags
RawrXD_TubiFillFrameRGBA PROC FRAME
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
    .endprolog

    test rcx, rcx
    jz fail
    test edx, edx
    jz fail
    test r8d, r8d
    jz fail
    test r9d, r9d
    jz fail

    mov rbx, rcx                ; dst
    mov esi, edx                ; width
    mov edi, r8d                ; height
    mov r12d, r9d               ; stride
    mov r13d, dword ptr [rsp + 96]   ; frameIndex
    mov r14d, dword ptr [rsp + 104]  ; totalFrames
    mov r15d, dword ptr [rsp + 112]  ; seed

    xor r10d, r10d              ; y

y_loop:
    cmp r10d, edi
    jae done

    xor r11d, r11d              ; x

x_loop:
    cmp r11d, esi
    jae next_row

    mov eax, r10d
    imul eax, r12d
    mov ecx, r11d
    shl ecx, 2
    add eax, ecx
    lea r8, [rbx + rax]         ; pixel ptr

    ; Base blue channel
    mov eax, r10d
    imul eax, 120
    xor edx, edx
    div edi
    add eax, 48
    mov ecx, r11d
    add ecx, r13d
    and ecx, 31
    add eax, ecx
    cmp eax, 255
    jbe blue_ok
    mov eax, 255
blue_ok:
    mov byte ptr [r8 + 2], al

    ; Base green channel
    mov eax, r10d
    imul eax, 60
    xor edx, edx
    div edi
    add eax, 12
    mov ecx, r13d
    and ecx, 15
    add eax, ecx
    cmp eax, 255
    jbe green_ok
    mov eax, 255
green_ok:
    mov byte ptr [r8 + 1], al

    ; Base red channel
    mov eax, r10d
    imul eax, 36
    xor edx, edx
    div edi
    add eax, 10
    mov ecx, r15d
    and ecx, 7
    add eax, ecx
    cmp eax, 255
    jbe red_ok
    mov eax, 255
red_ok:
    mov byte ptr [r8 + 0], al

    ; Star/noise sparkle
    mov eax, r11d
    imul eax, 13
    mov ecx, r10d
    imul ecx, 7
    add eax, ecx
    add eax, r13d
    add eax, r15d
    and eax, 63
    cmp eax, 0
    jne no_star
    mov byte ptr [r8 + 0], 220
    mov byte ptr [r8 + 1], 220
    mov byte ptr [r8 + 2], 255
no_star:

    ; Darken the lower area for the ground.
    mov eax, edi
    imul eax, 62
    mov ecx, 100
    xor edx, edx
    div ecx
    cmp r10d, eax
    jb alpha_out
    mov byte ptr [r8 + 0], 8
    mov byte ptr [r8 + 1], 6
    mov byte ptr [r8 + 2], 14

alpha_out:
    mov byte ptr [r8 + 3], 255

    inc r11d
    jmp x_loop

next_row:
    inc r10d
    jmp y_loop

done:
    mov eax, 1
    pop r15
    pop r14
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret

fail:
    xor eax, eax
    pop r15
    pop r14
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret
RawrXD_TubiFillFrameRGBA ENDP

END
