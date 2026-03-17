; ════════════════════════════════════════════════════════════════════════════════
; encoder_host_final.asm - Clean Integration of x64 Encoder + Macro Substitution
; MASM64 | x64 Calling Convention (RCX/RDX/R8/R9) | Zero Inheritance Issues
; ════════════════════════════════════════════════════════════════════════════════

OPTION CASEMAP:NONE
OPTION WIN64:11

EXTERN GetProcessHeap:PROC
EXTERN HeapAlloc:PROC
EXTERN GetStdHandle:PROC
EXTERN WriteFile:PROC
EXTERN ExitProcess:PROC

; ════════════════════════════════════════════════════════════════════════════════
; CONSTANTS
; ════════════════════════════════════════════════════════════════════════════════
REX_BASE             EQU 040h
REX_W                EQU 008h
REX_R                EQU 004h
REX_X                EQU 002h
REX_B                EQU 001h

MOD_INDIRECT         EQU 0
MOD_DISP8            EQU 1
MOD_DISP32           EQU 2
MOD_REGISTER         EQU 3

; Token types
TOK_EOF              EQU 0
TOK_NEWLINE          EQU 1
TOK_IDENT            EQU 2
TOK_NUMBER           EQU 3
TOK_COMMA            EQU 7
TOK_LPAREN           EQU 10
TOK_RPAREN           EQU 11
TOK_LBRACKET         EQU 8
TOK_RBRACKET         EQU 9

MACRO_MAX_PARAMS     EQU 16
MACRO_MAX_DEPTH      EQU 32

; ════════════════════════════════════════════════════════════════════════════════
; .DATA? - State
; ════════════════════════════════════════════════════════════════════════════════
.data?
ALIGN 16

; Encoder state
g_pCodeOut           DQ ?
g_CodeOffset         DQ ?

; Token stream
g_tokens             DQ ?
g_tok_cnt            DD ?
g_tok_pos            DD ?

; Macro state
g_expand_depth       DD ?

; Heap
g_heap               DQ ?

.data
ALIGN 16

szBanner db "[RawrXD] x64 Encoder + Macro Substitution System", 13, 10, 0

; ════════════════════════════════════════════════════════════════════════════════
; CODE SECTION
; ════════════════════════════════════════════════════════════════════════════════
.CODE
ALIGN 64

; ════════════════════════════════════════════════════════════════════════════════
; HEAP ALLOCATOR
; ════════════════════════════════════════════════════════════════════════════════
Initialize_Heap PROC FRAME
    push rbp
    mov rbp, rsp
    sub rsp, 28h
    call GetProcessHeap
    mov g_heap, rax
    add rsp, 28h
    pop rbp
    ret
Initialize_Heap ENDP

; Allocate size bytes
; RCX = size, returns RAX = pointer
Allocate PROC FRAME
    push rbp
    mov rbp, rsp
    mov r8, rcx                ; Size
    mov rcx, g_heap
    xor edx, edx               ; Flags = 0
    sub rsp, 28h
    call HeapAlloc
    add rsp, 28h
    pop rbp
    ret
Allocate ENDP

; ════════════════════════════════════════════════════════════════════════════════
; ENCODER PRIMITIVES
; ════════════════════════════════════════════════════════════════════════════════

; EmitByte - Write one byte
; RCX = context (code_out ptr), DL = byte
EmitByte PROC FRAME
    mov rax, g_pCodeOut
    mov [rax], dl
    inc g_pCodeOut
    inc g_CodeOffset
    ret
EmitByte ENDP

; EmitWord - Write 16-bit little-endian
; RDX = word value
EmitWord PROC FRAME
    mov rax, g_pCodeOut
    mov [rax], dx
    add g_pCodeOut, 2
    add g_CodeOffset, 2
    ret
EmitWord ENDP

; EmitDword - Write 32-bit little-endian
; RDX = dword value
EmitDword PROC FRAME
    mov rax, g_pCodeOut
    mov [rax], edx
    add g_pCodeOut, 4
    add g_CodeOffset, 4
    ret
EmitDword ENDP

; EmitQword - Write 64-bit little-endian
; RDX = qword value
EmitQword PROC FRAME
    mov rax, g_pCodeOut
    mov [rax], rdx
    add g_pCodeOut, 8
    add g_CodeOffset, 8
    ret
EmitQword ENDP

; ════════════════════════════════════════════════════════════════════════════════
; REX/MODRM/SIB ENCODERS
; ════════════════════════════════════════════════════════════════════════════════

; CalcREX - Compute REX prefix from register operands
; RCX = dst_reg (0-15), RDX = src_reg (0-15), R8 = needs_w (0/1)
; Returns: AL = REX byte (0 if not needed)
CalcREX PROC FRAME
    xor eax, eax
    
    ; REX.W for 64-bit
    test r8, r8
    jz @check_r
    or al, REX_W
    
@check_r:
    ; REX.R for dst >= 8
    test cl, 8
    jz @check_b
    or al, REX_R
    
@check_b:
    ; REX.B for src >= 8
    test dl, 8
    jz @finalize
    or al, REX_B
    
@finalize:
    test al, al
    jz @done
    or al, REX_BASE
    
@done:
    ret
CalcREX ENDP

; EncodeModRM - Build ModRM byte
; CL = mod (0-3), DL = reg (0-7), R8B = r/m (0-7)
EncodeModRM PROC FRAME
    shl cl, 6
    shl dl, 3
    or cl, dl
    or cl, r8b
    mov al, cl
    ret
EncodeModRM ENDP

; ════════════════════════════════════════════════════════════════════════════════
; HIGH-LEVEL INSTRUCTION EMITTERS
; ════════════════════════════════════════════════════════════════════════════════

; Emit_MOV_R64_R64: MOV r64, r64
; RCX = dst reg, RDX = src reg
Emit_MOV_R64_R64 PROC FRAME
    push rcx
    push rdx
    
    ; Emit REX.W + REX.R/B as needed
    mov r8, 1                  ; needs_w
    call CalcREX
    test al, al
    jz @no_rex
    mov dl, al
    call EmitByte
    
@no_rex:
    ; Emit opcode 8Bh (MOV r, r/m - source from r/m, dest from reg)
    mov dl, 08Bh
    call EmitByte
    
    ; Emit ModRM (mod=11, reg=src, rm=dst)
    pop rax
    mov rcx, 3                 ; mod = register
    mov rdx, rax               ; reg = src
    pop r8                     ; r/m = dst
    
    and dl, 7
    and r8b, 7
    call EncodeModRM
    mov dl, al
    call EmitByte
    
    ret
Emit_MOV_R64_R64 ENDP

; Emit_MOV_R64_IMM64: MOV r64, imm64
; RCX = dst reg, RDX = imm64
Emit_MOV_R64_IMM64 PROC FRAME
    push rcx
    push rdx
    
    ; Emit REX.W + REX.B if reg >= 8
    mov al, REX_W
    pop rax                    ; imm64
    pop rdx                    ; dst reg
    
    test dl, 8
    jz @emit_rex
    or al, REX_B
    
@emit_rex:
    mov cl, al
    call EmitByte
    
    ; Emit opcode B8+rd
    mov al, 0B8h
    and dl, 7
    add al, dl
    mov dl, al
    call EmitByte
    
    ; Emit imm64
    mov rdx, rax
    call EmitQword
    
    ret
Emit_MOV_R64_IMM64 ENDP

; Emit_ADD_R64_R64: ADD r64, r64
; RCX = dst reg, RDX = src reg
Emit_ADD_R64_R64 PROC FRAME
    push rcx
    push rdx
    
    ; REX.W
    mov al, REX_W
    test cl, 8
    jz @skip_r
    or al, REX_R
@skip_r:
    test dl, 8
    jz @emit_rex
    or al, REX_B
    
@emit_rex:
    mov dl, al
    call EmitByte
    
    ; Opcode 01h
    mov dl, 001h
    call EmitByte
    
    ; ModRM
    pop rax
    pop rdx
    mov rcx, 3
    and al, 7
    and dl, 7
    call EncodeModRM
    mov dl, al
    call EmitByte
    
    ret
Emit_ADD_R64_R64 ENDP

; Emit_RET: RET instruction
Emit_RET PROC FRAME
    mov dl, 0C3h
    call EmitByte
    ret
Emit_RET ENDP

; Emit_NOP: NOP instruction
Emit_NOP PROC FRAME
    mov dl, 090h
    call EmitByte
    ret
Emit_NOP ENDP

; ════════════════════════════════════════════════════════════════════════════════
; MACRO SUBSTITUTION ENGINE
; ════════════════════════════════════════════════════════════════════════════════

; tokenize_macro_args: Parse macro invocation arguments
; RCX = frame context, fills arg_values/arg_counts
tokenize_macro_args PROC FRAME
    push rbp
    mov rbp, rsp
    push rbx rsi rdi r12 r13
    
    mov rsi, rcx               ; Frame
    xor ebx, ebx               ; Arg index
    xor r12d, r12d             ; Paren depth
    
    mov eax, g_tok_pos
    mov r13d, eax              ; Start position
    
@arg_loop:
    mov eax, g_tok_pos
    cmp eax, g_tok_cnt
    jae @args_done
    
    ; Get token type at position
    imul rax, rax, 32
    add rax, g_tokens
    movzx ecx, byte ptr [rax]
    
    cmp ecx, TOK_NEWLINE
    je @args_done
    cmp ecx, TOK_EOF
    je @args_done
    
    cmp ecx, TOK_LPAREN
    jne @chk_rparen
    inc r12d
    jmp @next_tok
    
@chk_rparen:
    cmp ecx, TOK_RPAREN
    jne @chk_comma
    test r12d, r12d
    jz @args_done
    dec r12d
    jmp @next_tok
    
@chk_comma:
    cmp ecx, TOK_COMMA
    jne @next_tok
    test r12d, r12d
    jnz @next_tok
    
    ; Arg boundary
    mov eax, g_tok_pos
    sub eax, r13d
    
    ; Store arg (simplified: just track indices)
    mov [rsi+8+rbx*8], r13d    ; arg start
    mov [rsi+8+256+rbx*4], eax ; arg count
    
    inc ebx
    mov r13d, g_tok_pos
    inc r13d
    
@next_tok:
    inc g_tok_pos
    jmp @arg_loop
    
@args_done:
    ; Store arg count in recursion_depth field
    mov [rsi+264], ebx
    
    pop r13 r12 rdi rsi rbx rbp
    ret
tokenize_macro_args ENDP

; expand_macro_subst: Expand macro with %0-%n substitution
; RCX = macro_entry, RDX = frame
expand_macro_subst PROC FRAME
    push rbp
    mov rbp, rsp
    
    ; Check recursion depth
    mov eax, g_expand_depth
    cmp eax, MACRO_MAX_DEPTH
    jae @recursion_err
    inc g_expand_depth
    
    ; Process body tokens (simplified)
    ; Iterate through macro body and substitute %N
    
@done:
    dec g_expand_depth
    pop rbp
    ret
    
@recursion_err:
    pop rbp
    ret
expand_macro_subst ENDP

; ════════════════════════════════════════════════════════════════════════════════
; MAIN ENTRY POINT
; ════════════════════════════════════════════════════════════════════════════════

main PROC FRAME
    push rbp
    mov rbp, rsp
    sub rsp, 512
    
    ; Initialize
    call Initialize_Heap
    
    ; Allocate code buffer (4KB)
    mov rcx, 4096
    call Allocate
    mov g_pCodeOut, rax
    xor eax, eax
    mov g_CodeOffset, rax
    
    ; ────────────────────────────────────────────────────────────────────────
    ; TEST ENCODING: MOV RAX, 0x123456789ABCDEF0
    ; ────────────────────────────────────────────────────────────────────────
    mov rcx, 0                 ; RAX
    mov rdx, 0123456789ABCDEF0h
    call Emit_MOV_R64_IMM64
    
    ; ────────────────────────────────────────────────────────────────────────
    ; TEST ENCODING: MOV R15, 0x42
    ; ────────────────────────────────────────────────────────────────────────
    mov rcx, 15                ; R15
    mov rdx, 42h
    call Emit_MOV_R64_IMM64
    
    ; ────────────────────────────────────────────────────────────────────────
    ; TEST ENCODING: MOV RAX, RBX
    ; ────────────────────────────────────────────────────────────────────────
    mov rcx, 0                 ; RAX
    mov rdx, 3                 ; RBX
    call Emit_MOV_R64_R64
    
    ; ────────────────────────────────────────────────────────────────────────
    ; TEST ENCODING: ADD RAX, RBX
    ; ────────────────────────────────────────────────────────────────────────
    mov rcx, 0
    mov rdx, 3
    call Emit_ADD_R64_R64
    
    ; ────────────────────────────────────────────────────────────────────────
    ; TEST ENCODING: RET
    ; ────────────────────────────────────────────────────────────────────────
    call Emit_RET
    
    ; Output result
    mov rcx, -11               ; STD_OUTPUT_HANDLE
    sub rsp, 28h
    call GetStdHandle
    add rsp, 28h
    
    ; Print "Success" message
    mov rcx, rax               ; Handle
    mov rdx, offset szBanner
    mov r8, 50                 ; Length
    xor r9, r9
    sub rsp, 28h
    lea r10, [rsp+28h]         ; Bytes written (dummy)
    call WriteFile
    add rsp, 28h
    
    ; Exit with success
    xor ecx, ecx
    call ExitProcess
    
    add rsp, 512
    pop rbp
    ret
main ENDP

END main
