;================================================================================
; RawrXD_Lexer_AVX2.asm - Parallel Syntax Highlighting
; Inputs: rcx = Buffer, rdx = Length
; Outputs: r8 = ColorAttributeBuffer (1 byte per char)
;================================================================================


; ─── Cross-module symbol resolution ───
INCLUDE rawrxd_master.inc

.code

PUBLIC Lexer_Scan_Parallel
Lexer_Scan_Parallel PROC FRAME
    push rbp
    .pushreg rbp
    mov rbp, rsp
    .setframe rbp, 0
    .endprolog

    ; Check for zero length
    test rdx, rdx
    jz scan_done

    ; Preload constants (simplified)
    ; vmovdqu ymm_quote, [constant_quote] 

scan_loop:
    cmp rdx, 32
    jl scan_serial ; Fallback for trailing bytes
    
    ; vmovdqu ymm1, [rcx]           ; Load 32 characters
    ; vpcmpeqb ymm2, ymm1, ymm_quote ; Find all quotes (")
    ; vpmovmskb eax, ymm2           ; Create mask of string boundaries
    
    ; Logic: Use bit-scan forward (BSF) to flip "String" state bit
    ; This replaces slow character-by-character loops
    xor r9, r9                    ; Current State: 0=Code, 1=String, 2=Comment
    ; ... (FSM state transition logic) ...
    
    ; vmovdqu [r8], ymm_colors      ; Store color indices for the GUI
    add rcx, 32
    add r8, 32
    sub rdx, 32
    jnz scan_loop

scan_serial:
    ; Handle remaining bytes...
    
scan_done:
    mov rsp, rbp
    pop rbp
    ret
Lexer_Scan_Parallel ENDP

END
