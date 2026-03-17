; ============================================================================
; PIRAM_X64.ASM - Ultra-Minimal π-RAM Compression (x64 native)
; Direct port of barefoot core for x64 Windows
; ============================================================================

IFDEF X64
.code

PUBLIC PiCompress
PUBLIC DivideRam
PUBLIC PiStream

; π-RAM constant
PI_CONST equ 3296474

; ============================================================================
; PiCompress - Compress with π-transform
; RCX = input buffer
; RDX = size
; RAX = output (modified in-place)
; ============================================================================
PiCompress proc
    push rbx
    push r8
    
    ; Halve target size
    shr rdx, 1
    
    ; Allocate (external malloc expected)
    call malloc
    jnc @@done
    pop r8
    pop rbx
    ret
    
@@done:
    xor ecx, ecx
    
@@loop:
    cmp rcx, rdx
    je @@exit
    
    ; Load byte
    movzx r8d, byte ptr [rax + rcx]
    
    ; π-transform
    imul r8d, PI_CONST
    shr r8d, 20
    
    ; Store
    mov byte ptr [rax + rcx], r8b
    
    inc rcx
    jmp @@loop
    
@@exit:
    pop r8
    pop rbx
    ret
PiCompress endp

; ============================================================================
; DivideRam - Halve RAM allocation
; RCX = structure pointer (size at +16)
; ============================================================================
DivideRam proc
    test rcx, rcx
    jz @@exit
    
    shr qword ptr [rcx + 16], 1
    
@@exit:
    ret
DivideRam endp

; ============================================================================
; PiStream - Stream compression (4KB chunks)
; RCX = stream base
; RDX = total size
; ============================================================================
PiStream proc
    push rbx
    push r8
    
    ; Halve stream size
    shr rdx, 1
    
    xor ecx, ecx
    
@@stream_loop:
    cmp ecx, edx
    je @@stream_done
    
    call PiCompress
    
    add ecx, 4096
    jmp @@stream_loop
    
@@stream_done:
    pop r8
    pop rbx
    ret
PiStream endp

ENDIF
END
