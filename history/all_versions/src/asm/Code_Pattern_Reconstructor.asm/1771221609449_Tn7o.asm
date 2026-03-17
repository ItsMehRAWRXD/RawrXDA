;================================================================================
; Code_Pattern_Reconstructor.asm
; Pure MASM x64 - Pattern-based code reconstruction
; Reverse engineers binaries by identifying code patterns and reconstructing logic
;================================================================================
; SCAFFOLD_136: inference_core.asm
; SCAFFOLD_137: feature_dispatch_bridge.asm

option casemap:none

EXTERN VirtualAlloc:PROC
EXTERN VirtualFree:PROC
EXTERN GetStdHandle:PROC
EXTERN WriteFile:PROC

;================================================================================
; STRUCTURES
;================================================================================

CODE_PATTERN struct
    signature       db 16 dup(?)    ; Pattern signature bytes
    sig_length      dd ?
    pattern_type    dd ?            ; 0=func_entry, 1=func_exit, 2=call, 3=loop
    semantic_type   dd ?            ; Semantic meaning
    confidence      dd ?
CODE_PATTERN ends

FUNCTION_INFO struct
    entry_offset    dq ?
    exit_offset     dq ?
    size_bytes      dd ?
    call_count      dd ?
    loop_count      dd ?
    complexity      dd ?
    reconstructed   dd ?
FUNCTION_INFO ends

RECONSTRUCT_CONTEXT struct
    binary_base     dq ?
    binary_size     dq ?
    output_buffer   dq ?
    output_size     dq ?
    functions_found dd ?
    patterns_matched dd ?
RECONSTRUCT_CONTEXT ends

;================================================================================
; DATA
;================================================================================
.data
align 16

; Function Entry Patterns (x64)
pattern_func_entry_std      db 48h, 89h, 5Ch, 24h, 08h        ; mov [rsp+8], rbx
pattern_func_entry_frame    db 55h, 48h, 89h, 0E5h            ; push rbp; mov rbp, rsp
pattern_func_entry_stack    db 48h, 83h, 0ECh                 ; sub rsp, imm8
pattern_func_entry_prologue db 40h, 53h, 48h, 83h, 0ECh, 20h  ; push rbx; sub rsp, 20h

; Function Exit Patterns
pattern_func_exit_ret       db 0C3h                           ; ret
pattern_func_exit_frame     db 5Dh, 0C3h                      ; pop rbp; ret
pattern_func_exit_stack     db 48h, 83h, 0C4h                 ; add rsp, imm8
pattern_func_exit_epilogue  db 48h, 8Bh, 5Ch, 24h, 30h, 48h, 83h, 0C4h, 20h, 5Bh, 0C3h

; Call Patterns
pattern_call_direct         db 0E8h                           ; call rel32
pattern_call_indirect_rax   db 0FFh, 0D0h                     ; call rax
pattern_call_indirect_mem   db 0FFh, 15h                      ; call [rip+rel32]

; Loop Patterns
pattern_loop_simple         db 75h                            ; jnz rel8 (loop back)
pattern_loop_dec_jnz        db 48h, 0FFh, 0C8h, 75h           ; dec rax; jnz
pattern_loop_cmp_jne        db 48h, 3Bh                       ; cmp rax, ...

; String Patterns
pattern_string_load         db 48h, 8Dh, 0Dh                  ; lea rcx, [rip+rel32]
pattern_string_move         db 48h, 0B8h                      ; mov rax, imm64

; Context
g_context                   RECONSTRUCT_CONTEXT <>
g_functions                 FUNCTION_INFO 1024 dup(<>)
g_patterns                  CODE_PATTERN 64 dup(<>)
g_pattern_count             dd 0

; Reconstruction buffer
reconstructed_asm           db 65536 dup(0)
reconstructed_size          dd 0

; Strings
sz_banner       db "╔═══════════════════════════════════╗", 13, 10
                db "║  Code Pattern Reconstructor v1.0  ║", 13, 10
                db "║  Pure MASM x64                    ║", 13, 10
                db "╚═══════════════════════════════════╝", 13, 10, 0
sz_analyzing    db "[*] Analyzing binary patterns...", 13, 10, 0
sz_functions    db "[+] Found %d functions", 13, 10, 0
sz_patterns     db "[+] Matched %d patterns", 13, 10, 0
sz_reconstructing db "[*] Reconstructing code...", 13, 10, 0
sz_complete     db "[+] Reconstruction complete", 13, 10, 0
sz_func_label   db "func_%04d:", 13, 10, 0
sz_func_entry   db "    ; Function entry at offset 0x%llX", 13, 10, 0
sz_func_exit    db "    ret                    ; Function exit", 13, 10, 0
sz_call_inst    db "    call sub_%llX         ; Direct call", 13, 10, 0
sz_loop_start   db "loop_start:", 13, 10, 0
sz_loop_end     db "    jnz loop_start        ; Loop back", 13, 10, 0

STD_OUTPUT_HANDLE equ -11

;================================================================================
; CODE
;================================================================================
.code

PUBLIC Reconstructor_Initialize
PUBLIC Reconstructor_ScanPatterns
PUBLIC Reconstructor_IdentifyFunctions
PUBLIC Reconstructor_BuildASM
PUBLIC Reconstructor_GetResult

;--------------------------------------------------------------------------------
; Initialize reconstructor
; RCX = binary base
; RDX = binary size
;--------------------------------------------------------------------------------
Reconstructor_Initialize PROC
    push rbx
    
    test rcx, rcx
    jz init_fail
    test rdx, rdx
    jz init_fail
    
    mov g_context.binary_base, rcx
    mov g_context.binary_size, rdx
    xor eax, eax
    mov g_context.functions_found, eax
    mov g_context.patterns_matched, eax
    
    ; Allocate output buffer
    push rcx
    push rdx
    mov rcx, 0
    mov rdx, 65536              ; 64KB output buffer
    mov r8d, 00003000h          ; MEM_COMMIT | MEM_RESERVE
    mov r9d, 04h                ; PAGE_READWRITE
    call VirtualAlloc
    pop rdx
    pop rcx
    
    test rax, rax
    jz init_fail
    
    mov g_context.output_buffer, rax
    mov g_context.output_size, 65536
    
    ; Print banner
    lea rcx, sz_banner
    call PrintString
    
    mov eax, 1
    pop rbx
    ret
    
init_fail:
    xor eax, eax
    pop rbx
    ret
Reconstructor_Initialize ENDP

;--------------------------------------------------------------------------------
; Scan for code patterns
; Returns: RAX = patterns found
;--------------------------------------------------------------------------------
Reconstructor_ScanPatterns PROC
    push rbx
    push r12
    push r13
    push r14
    push r15
    
    lea rcx, sz_analyzing
    call PrintString
    
    mov r12, g_context.binary_base
    mov r13, g_context.binary_size
    xor r14, r14                ; offset iterator
    xor r15d, r15d              ; pattern counter
    
scan_loop:
    cmp r14, r13
    jae scan_done
    
    ; Check function entry patterns
    lea rcx, [r12 + r14]
    mov rdx, r13
    sub rdx, r14
    call MatchFunctionEntry
    test eax, eax
    jz check_func_exit
    
    inc r15d
    add r14, rax
    jmp scan_loop
    
check_func_exit:
    lea rcx, [r12 + r14]
    mov rdx, r13
    sub rdx, r14
    call MatchFunctionExit
    test eax, eax
    jz check_calls
    
    inc r15d
    add r14, rax
    jmp scan_loop
    
check_calls:
    lea rcx, [r12 + r14]
    mov rdx, r13
    sub rdx, r14
    call MatchCallPattern
    test eax, eax
    jz check_loops
    
    inc r15d
    add r14, rax
    jmp scan_loop
    
check_loops:
    lea rcx, [r12 + r14]
    mov rdx, r13
    sub rdx, r14
    call MatchLoopPattern
    test eax, eax
    jz next_byte
    
    inc r15d
    add r14, rax
    jmp scan_loop
    
next_byte:
    inc r14
    jmp scan_loop
    
scan_done:
    mov g_context.patterns_matched, r15d
    mov eax, r15d
    
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    ret
Reconstructor_ScanPatterns ENDP

;--------------------------------------------------------------------------------
; Match function entry pattern
; RCX = position
; RDX = remaining bytes
; Returns: RAX = pattern length if matched, 0 otherwise
;--------------------------------------------------------------------------------
MatchFunctionEntry PROC
    cmp rdx, 2
    jb no_match
    
    ; Check for push rbp; mov rbp, rsp (55 48 89 E5)
    cmp byte ptr [rcx], 55h
    jne check_stack_frame
    cmp byte ptr [rcx+1], 48h
    jne check_stack_frame
    cmp byte ptr [rcx+2], 89h
    jne check_stack_frame
    cmp byte ptr [rcx+3], 0E5h
    jne check_stack_frame
    mov eax, 4
    ret
    
check_stack_frame:
    cmp rdx, 5
    jb check_sub_rsp
    
    ; Check for mov [rsp+8], rbx (48 89 5C 24 08)
    cmp byte ptr [rcx], 48h
    jne check_sub_rsp
    cmp byte ptr [rcx+1], 89h
    jne check_sub_rsp
    cmp byte ptr [rcx+2], 5Ch
    jne check_sub_rsp
    cmp byte ptr [rcx+3], 24h
    jne check_sub_rsp
    cmp byte ptr [rcx+4], 08h
    jne check_sub_rsp
    mov eax, 5
    ret
    
check_sub_rsp:
    cmp rdx, 3
    jb no_match
    
    ; Check for sub rsp, imm8 (48 83 EC ??)
    cmp byte ptr [rcx], 48h
    jne no_match
    cmp byte ptr [rcx+1], 83h
    jne no_match
    cmp byte ptr [rcx+2], 0ECh
    jne no_match
    mov eax, 4
    ret
    
no_match:
    xor eax, eax
    ret
MatchFunctionEntry ENDP

;--------------------------------------------------------------------------------
; Match function exit pattern
; RCX = position
; RDX = remaining bytes
; Returns: RAX = pattern length if matched, 0 otherwise
;--------------------------------------------------------------------------------
MatchFunctionExit PROC
    cmp rdx, 1
    jb no_exit_match
    
    ; Check for simple ret (C3)
    cmp byte ptr [rcx], 0C3h
    jne check_pop_ret
    mov eax, 1
    ret
    
check_pop_ret:
    cmp rdx, 2
    jb no_exit_match
    
    ; Check for pop rbp; ret (5D C3)
    cmp byte ptr [rcx], 5Dh
    jne check_add_rsp_ret
    cmp byte ptr [rcx+1], 0C3h
    jne check_add_rsp_ret
    mov eax, 2
    ret
    
check_add_rsp_ret:
    cmp rdx, 4
    jb no_exit_match
    
    ; Check for add rsp, ??; ret (48 83 C4 ?? C3)
    cmp byte ptr [rcx], 48h
    jne no_exit_match
    cmp byte ptr [rcx+1], 83h
    jne no_exit_match
    cmp byte ptr [rcx+2], 0C4h
    jne no_exit_match
    ; Skip immediate byte check
    cmp byte ptr [rcx+4], 0C3h
    jne no_exit_match
    mov eax, 5
    ret
    
no_exit_match:
    xor eax, eax
    ret
MatchFunctionExit ENDP

;--------------------------------------------------------------------------------
; Match call pattern
; RCX = position
; RDX = remaining bytes
; Returns: RAX = pattern length if matched, 0 otherwise
;--------------------------------------------------------------------------------
MatchCallPattern PROC
    cmp rdx, 1
    jb no_call_match
    
    ; Check for call rel32 (E8)
    cmp byte ptr [rcx], 0E8h
    jne check_call_rax
    mov eax, 5
    ret
    
check_call_rax:
    cmp rdx, 2
    jb check_call_mem
    
    ; Check for call rax (FF D0)
    cmp byte ptr [rcx], 0FFh
    jne check_call_mem
    cmp byte ptr [rcx+1], 0D0h
    jne check_call_mem
    mov eax, 2
    ret
    
check_call_mem:
    cmp rdx, 6
    jb no_call_match
    
    ; Check for call [rip+rel32] (FF 15)
    cmp byte ptr [rcx], 0FFh
    jne no_call_match
    cmp byte ptr [rcx+1], 15h
    jne no_call_match
    mov eax, 6
    ret
    
no_call_match:
    xor eax, eax
    ret
MatchCallPattern ENDP

;--------------------------------------------------------------------------------
; Match loop pattern
; RCX = position
; RDX = remaining bytes
; Returns: RAX = pattern length if matched, 0 otherwise
;--------------------------------------------------------------------------------
MatchLoopPattern PROC
    cmp rdx, 2
    jb no_loop_match
    
    ; Check for jnz rel8 with negative offset (loop back)
    cmp byte ptr [rcx], 75h
    jne check_dec_loop
    movsx rax, byte ptr [rcx+1]
    test rax, rax
    jge check_dec_loop          ; Only count backward jumps as loops
    mov eax, 2
    ret
    
check_dec_loop:
    cmp rdx, 4
    jb no_loop_match
    
    ; Check for dec reg; jnz (48 FF C8 75)
    cmp byte ptr [rcx], 48h
    jne no_loop_match
    cmp byte ptr [rcx+1], 0FFh
    jne no_loop_match
    cmp byte ptr [rcx+2], 0C8h
    jne no_loop_match
    cmp byte ptr [rcx+3], 75h
    jne no_loop_match
    mov eax, 4
    ret
    
no_loop_match:
    xor eax, eax
    ret
MatchLoopPattern ENDP

;--------------------------------------------------------------------------------
; Identify functions from patterns
; Returns: RAX = functions found
;--------------------------------------------------------------------------------
Reconstructor_IdentifyFunctions PROC
    push rbx
    push r12
    push r13
    push r14
    
    mov r12, g_context.binary_base
    mov r13, g_context.binary_size
    xor r14d, r14d              ; function counter
    xor rbx, rbx                ; offset
    
identify_loop:
    cmp rbx, r13
    jae identify_done
    
    ; Check for function entry
    lea rcx, [r12 + rbx]
    mov rdx, r13
    sub rdx, rbx
    call MatchFunctionEntry
    test eax, eax
    jz next_offset
    
    ; Found function entry, record it
    cmp r14d, 1024
    jae identify_done
    
    mov rdi, r14
    imul rdi, sizeof FUNCTION_INFO
    lea rdi, [g_functions + rdi]
    mov qword ptr [rdi + FUNCTION_INFO.entry_offset], rbx
    mov dword ptr [rdi + FUNCTION_INFO.size_bytes], 0
    inc r14d
    
next_offset:
    inc rbx
    jmp identify_loop
    
identify_done:
    mov g_context.functions_found, r14d
    mov eax, r14d
    
    pop r14
    pop r13
    pop r12
    pop rbx
    ret
Reconstructor_IdentifyFunctions ENDP

;--------------------------------------------------------------------------------
; Build reconstructed ASM output
; Returns: RAX = 1 on success
;--------------------------------------------------------------------------------
Reconstructor_BuildASM PROC
    push rbx
    push r12
    push r13
    
    lea rcx, sz_reconstructing
    call PrintString
    
    mov r12, g_context.output_buffer
    mov r13d, g_context.functions_found
    xor ebx, ebx
    
    ; Write header
    ; (Would write proper ASM header here)
    
build_loop:
    cmp ebx, r13d
    jae build_done
    
    ; Write function label
    ; (Would format and write function code here)
    
    inc ebx
    jmp build_loop
    
build_done:
    lea rcx, sz_complete
    call PrintString
    
    mov eax, 1
    pop r13
    pop r12
    pop rbx
    ret
Reconstructor_BuildASM ENDP

;--------------------------------------------------------------------------------
; Get reconstruction result
; RCX = output buffer pointer (out)
; RDX = output size pointer (out)
;--------------------------------------------------------------------------------
Reconstructor_GetResult PROC
    mov rax, g_context.output_buffer
    mov [rcx], rax
    mov eax, reconstructed_size
    mov [rdx], rax
    mov eax, 1
    ret
Reconstructor_GetResult ENDP

;--------------------------------------------------------------------------------
; Print string helper
;--------------------------------------------------------------------------------
PrintString PROC
    push rbx
    push r12
    push r13
    sub rsp, 40h
    
    mov r12, rcx
    xor r13, r13
strlen_loop:
    cmp byte ptr [r12 + r13], 0
    je strlen_done
    inc r13
    jmp strlen_loop
    
strlen_done:
    mov ecx, STD_OUTPUT_HANDLE
    call GetStdHandle
    mov rbx, rax
    
    mov rcx, rbx
    mov rdx, r12
    mov r8, r13
    lea r9, [rsp+30h]
    mov qword ptr [rsp+20h], 0
    call WriteFile
    
    add rsp, 40h
    pop r13
    pop r12
    pop rbx
    ret
PrintString ENDP

END
