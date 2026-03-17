;-------------------------------------------------------------------------------
; RawrXD_Emitter_v1.asm
; Track C Step 1: Integrated MASM64 Inline Emitter v1.0
; Direct binary JIT emitter for x64
;-------------------------------------------------------------------------------

option casemap:none

RawrXD_Emit_Buffer struct
    base_ptr    dq ?    ; Start of buffer
    current_ptr dq ?    ; Current write position
    capacity    dq ?    ; Total size of buffer
RawrXD_Emit_Buffer ends

.code

;-------------------------------------------------------------------------------
; RawrXD_Emit_Reset
; RCX = Ptr to RawrXD_Emit_Buffer
;-------------------------------------------------------------------------------
RawrXD_Emit_Reset proc
    mov rax, [rcx + RawrXD_Emit_Buffer.base_ptr]
    mov [rcx + RawrXD_Emit_Buffer.current_ptr], rax
    ret
RawrXD_Emit_Reset endp

;-------------------------------------------------------------------------------
; Emit_Byte
; RCX = Ptr to RawrXD_Emit_Buffer
; RDX = Byte value
; Returns: RAX = 1 on success, 0 on overflow
;-------------------------------------------------------------------------------
Emit_Byte proc
    mov rax, [rcx + RawrXD_Emit_Buffer.current_ptr]
    mov r8, [rcx + RawrXD_Emit_Buffer.base_ptr]
    add r8, [rcx + RawrXD_Emit_Buffer.capacity]
    
    cmp rax, r8
    jae overflow
    
    mov [rax], dl
    inc qword ptr [rcx + RawrXD_Emit_Buffer.current_ptr]
    mov rax, 1
    ret

overflow:
    xor rax, rax
    ret
Emit_Byte endp

;-------------------------------------------------------------------------------
; Emit_Word
; RCX = Ptr to RawrXD_Emit_Buffer
; RDX = Word value
;-------------------------------------------------------------------------------
Emit_Word proc
    mov rax, [rcx + RawrXD_Emit_Buffer.current_ptr]
    mov r8, [rcx + RawrXD_Emit_Buffer.base_ptr]
    add r8, [rcx + RawrXD_Emit_Buffer.capacity]
    
    lea r9, [rax + 2]
    cmp r9, r8
    ja overflow
    
    mov [rax], dx
    mov [rcx + RawrXD_Emit_Buffer.current_ptr], r9
    mov rax, 1
    ret

overflow:
    xor rax, rax
    ret
Emit_Word endp

;-------------------------------------------------------------------------------
; Emit_Dword
; RCX = Ptr to RawrXD_Emit_Buffer
; RDX = Dword value
;-------------------------------------------------------------------------------
Emit_Dword proc
    mov rax, [rcx + RawrXD_Emit_Buffer.current_ptr]
    mov r8, [rcx + RawrXD_Emit_Buffer.base_ptr]
    add r8, [rcx + RawrXD_Emit_Buffer.capacity]
    
    lea r9, [rax + 4]
    cmp r9, r8
    ja overflow
    
    mov [rax], edx
    mov [rcx + RawrXD_Emit_Buffer.current_ptr], r9
    mov rax, 1
    ret

overflow:
    xor rax, rax
    ret
Emit_Dword endp

;-------------------------------------------------------------------------------
; Emit_Qword
; RCX = Ptr to RawrXD_Emit_Buffer
; RDX = Qword value
;-------------------------------------------------------------------------------
Emit_Qword proc
    mov rax, [rcx + RawrXD_Emit_Buffer.current_ptr]
    mov r8, [rcx + RawrXD_Emit_Buffer.base_ptr]
    add r8, [rcx + RawrXD_Emit_Buffer.capacity]
    
    lea r9, [rax + 8]
    cmp r9, r8
    ja overflow
    
    mov [rax], rdx
    mov [rcx + RawrXD_Emit_Buffer.current_ptr], r9
    mov rax, 1
    ret

overflow:
    xor rax, rax
    ret
Emit_Qword endp

;-------------------------------------------------------------------------------
; Instruction Encoders (v1.0 Basic)
;-------------------------------------------------------------------------------

; MOV RAX, imm64
; Opcode: 48 B8 <8 bytes>
Emit_Mov_Rax_Imm64 proc
    push rbx
    mov rbx, rdx ; Save imm64
    
    ; Emit Rex.W (48) + B8
    mov rdx, 0B848h
    call Emit_Word
    test rax, rax
    jz fail
    
    mov rdx, rbx
    call Emit_Qword
    
fail:
    pop rbx
    ret
Emit_Mov_Rax_Imm64 endp

; INT 3
; Opcode: CC
Emit_Int3 proc
    mov rdx, 0CCh
    jmp Emit_Byte
Emit_Int3 endp

; RET
; Opcode: C3
Emit_Ret proc
    mov rdx, 0C3h
    jmp Emit_Byte
Emit_Ret endp

; NOP
; Opcode: 90
Emit_Nop proc
    mov rdx, 90h
    jmp Emit_Byte
Emit_Nop endp

end
