; ============================================================================
; CODE_COMPLETION_ENHANCED.ASM - Full Implementation of Completion Features
; Push/pop, Call/cmp, Jump, Register, Directive completions
; ============================================================================

.686
.model flat, stdcall
option casemap:none

PUBLIC CompletePushPop
PUBLIC CompleteCallCmp
PUBLIC CompleteJump
PUBLIC CompleteRegister
PUBLIC CompleteDirective

; ============================================================================
; CONSTANTS
; ============================================================================
NULL equ 0
MAX_COMPLETIONS equ 16

; Instruction types
INST_PUSH equ 1
INST_POP equ 2
INST_CALL equ 3
INST_CMP equ 4
INST_JMP equ 5
INST_JE equ 6
INST_JNE equ 7
INST_JL equ 8
INST_JG equ 9

; Register IDs
REG_EAX equ 0
REG_EBX equ 1
REG_ECX equ 2
REG_EDX equ 3
REG_ESI equ 4
REG_EDI equ 5
REG_ESP equ 6
REG_EBP equ 7

.data
    ; Push instruction completions
    szPushEAX db "push eax",0
    szPushEBX db "push ebx",0
    szPushECX db "push ecx",0
    szPushEDX db "push edx",0
    szPushESI db "push esi",0
    szPushEDI db "push edi",0
    
    ; Pop instruction completions
    szPopEAX db "pop eax",0
    szPopEBX db "pop ebx",0
    szPopECX db "pop ecx",0
    szPopEDX db "pop edx",0
    szPopESI db "pop esi",0
    szPopEDI db "pop edi",0
    
    ; Call/Cmp completions
    szCallEAX db "call eax",0
    szCallEBX db "call ebx",0
    szCmpEAX db "cmp eax, ebx",0
    szCmpECX db "cmp ecx, edx",0
    
    ; Jump completions
    szJmpStart db "jmp ",0
    szJeStart db "je @@",0
    szJneStart db "jne @@",0
    szJlStart db "jl @@",0
    szJgStart db "jg @@",0
    
    ; Register names
    szReg_EAX db "eax",0
    szReg_EBX db "ebx",0
    szReg_ECX db "ecx",0
    szReg_EDX db "edx",0
    szReg_ESI db "esi",0
    szReg_EDI db "edi",0
    szReg_ESP db "esp",0
    szReg_EBP db "ebp",0
    
    ; Directive completions
    szProcStart db ".proc ",0
    szProcEnd db ".endp",0
    szIfStart db ".if ",0
    szIfEnd db ".endif",0
    szDataStart db ".data",0
    szDataEnd db ".data?",0

.code

; ============================================================================
; PUSH/POP INSTRUCTION COMPLETION
; ============================================================================

CompletePushPop proc instType:dword, pCompletions:dword, pCount:dword
    LOCAL i:dword
    LOCAL pList:dword
    LOCAL count:dword
    
    cmp instType, INST_PUSH
    je @@push_completion
    cmp instType, INST_POP
    je @@pop_completion
    xor eax, eax
    ret
    
@@push_completion:
    ; Return push instruction completions
    mov pList, offset szPushEAX
    mov count, 6
    jmp @@fill_completions
    
@@pop_completion:
    ; Return pop instruction completions
    mov pList, offset szPopEAX
    mov count, 6
    
@@fill_completions:
    ; Store completion pointers in output array
    test pCompletions, pCompletions
    jz @@no_fill
    
    mov i, 0
@@fill_loop:
    cmp i, count
    jge @@fill_done
    
    ; For simplicity, store pointer to instruction string
    mov eax, pList
    add eax, i  ; offset by index
    mov ecx, pCompletions
    mov dword ptr [ecx + i*4], eax
    
    inc i
    jmp @@fill_loop
    
@@fill_done:
    ; Store count
    test pCount, pCount
    jz @@no_fill
    mov ecx, pCount
    mov dword ptr [ecx], count
    
@@no_fill:
    mov eax, count
    ret
CompletePushPop endp

; ============================================================================
; CALL/CMP INSTRUCTION COMPLETION
; ============================================================================

CompleteCallCmp proc instType:dword, pCompletions:dword, pCount:dword
    LOCAL count:dword
    
    cmp instType, INST_CALL
    je @@call_completion
    cmp instType, INST_CMP
    je @@cmp_completion
    xor eax, eax
    ret
    
@@call_completion:
    ; Return call instruction completions (call register)
    mov count, 2
    test pCompletions, pCompletions
    jz @@call_no_fill
    
    mov ecx, pCompletions
    mov dword ptr [ecx], offset szCallEAX
    mov dword ptr [ecx+4], offset szCallEBX
    
@@call_no_fill:
    test pCount, pCount
    jz @@call_end
    mov ecx, pCount
    mov dword ptr [ecx], count
    
@@call_end:
    mov eax, count
    ret
    
@@cmp_completion:
    ; Return cmp instruction completions (compare two registers)
    mov count, 2
    test pCompletions, pCompletions
    jz @@cmp_no_fill
    
    mov ecx, pCompletions
    mov dword ptr [ecx], offset szCmpEAX
    mov dword ptr [ecx+4], offset szCmpECX
    
@@cmp_no_fill:
    test pCount, pCount
    jz @@cmp_end
    mov ecx, pCount
    mov dword ptr [ecx], count
    
@@cmp_end:
    mov eax, count
    ret
CompleteCallCmp endp

; ============================================================================
; JUMP INSTRUCTION COMPLETION
; ============================================================================

CompleteJump proc jumpType:dword, pCompletions:dword, pCount:dword
    LOCAL count:dword
    LOCAL pPrefix:dword
    
    ; Determine jump prefix based on type
    cmp jumpType, INST_JMP
    je @@jmp_type
    cmp jumpType, INST_JE
    je @@je_type
    cmp jumpType, INST_JNE
    je @@jne_type
    cmp jumpType, INST_JL
    je @@jl_type
    cmp jumpType, INST_JG
    je @@jg_type
    
    xor eax, eax
    ret
    
@@jmp_type:
    mov pPrefix, offset szJmpStart
    jmp @@jump_fill
    
@@je_type:
    mov pPrefix, offset szJeStart
    jmp @@jump_fill
    
@@jne_type:
    mov pPrefix, offset szJneStart
    jmp @@jump_fill
    
@@jl_type:
    mov pPrefix, offset szJlStart
    jmp @@jump_fill
    
@@jg_type:
    mov pPrefix, offset szJgStart
    
@@jump_fill:
    ; For jump, return 1 completion with prefix
    mov count, 1
    test pCompletions, pCompletions
    jz @@jump_no_fill
    
    mov ecx, pCompletions
    mov eax, pPrefix
    mov dword ptr [ecx], eax
    
@@jump_no_fill:
    test pCount, pCount
    jz @@jump_end
    mov ecx, pCount
    mov dword ptr [ecx], count
    
@@jump_end:
    mov eax, count
    ret
CompleteJump endp

; ============================================================================
; REGISTER COMPLETION
; ============================================================================

CompleteRegister proc pCompletions:dword, pCount:dword
    LOCAL i:dword
    LOCAL count:dword
    
    ; Return all 8 register names
    mov count, 8
    test pCompletions, pCompletions
    jz @@reg_no_fill
    
    mov i, 0
    mov ecx, pCompletions
@@reg_loop:
    cmp i, count
    jge @@reg_no_fill
    
    ; Map register index to string
    mov eax, i
    cmp eax, REG_EAX
    je @@set_eax
    cmp eax, REG_EBX
    je @@set_ebx
    cmp eax, REG_ECX
    je @@set_ecx
    cmp eax, REG_EDX
    je @@set_edx
    cmp eax, REG_ESI
    je @@set_esi
    cmp eax, REG_EDI
    je @@set_edi
    cmp eax, REG_ESP
    je @@set_esp
    cmp eax, REG_EBP
    je @@set_ebp
    
    xor eax, eax
    jmp @@set_reg
    
@@set_eax:
    mov eax, offset szReg_EAX
    jmp @@set_reg
@@set_ebx:
    mov eax, offset szReg_EBX
    jmp @@set_reg
@@set_ecx:
    mov eax, offset szReg_ECX
    jmp @@set_reg
@@set_edx:
    mov eax, offset szReg_EDX
    jmp @@set_reg
@@set_esi:
    mov eax, offset szReg_ESI
    jmp @@set_reg
@@set_edi:
    mov eax, offset szReg_EDI
    jmp @@set_reg
@@set_esp:
    mov eax, offset szReg_ESP
    jmp @@set_reg
@@set_ebp:
    mov eax, offset szReg_EBP
    
@@set_reg:
    mov dword ptr [ecx + i*4], eax
    inc i
    jmp @@reg_loop
    
@@reg_no_fill:
    test pCount, pCount
    jz @@reg_end
    mov ecx, pCount
    mov dword ptr [ecx], count
    
@@reg_end:
    mov eax, count
    ret
CompleteRegister endp

; ============================================================================
; DIRECTIVE COMPLETION
; ============================================================================

CompleteDirective proc directiveType:dword, pCompletions:dword, pCount:dword
    LOCAL count:dword
    LOCAL pStr:dword
    
    ; Map directive type to string
    cmp directiveType, 1  ; .proc
    je @@proc_dir
    cmp directiveType, 2  ; .if
    je @@if_dir
    cmp directiveType, 3  ; .data
    je @@data_dir
    
    xor eax, eax
    ret
    
@@proc_dir:
    mov pStr, offset szProcStart
    mov count, 1
    jmp @@dir_fill
    
@@if_dir:
    mov pStr, offset szIfStart
    mov count, 1
    jmp @@dir_fill
    
@@data_dir:
    mov pStr, offset szDataStart
    mov count, 2
    jmp @@dir_fill
    
@@dir_fill:
    test pCompletions, pCompletions
    jz @@dir_no_fill
    
    mov ecx, pCompletions
    mov eax, pStr
    mov dword ptr [ecx], eax
    
    cmp count, 2
    jne @@dir_no_fill
    mov eax, offset szDataEnd
    mov dword ptr [ecx+4], eax
    
@@dir_no_fill:
    test pCount, pCount
    jz @@dir_end
    mov ecx, pCount
    mov dword ptr [ecx], count
    
@@dir_end:
    mov eax, count
    ret
CompleteDirective endp

end
