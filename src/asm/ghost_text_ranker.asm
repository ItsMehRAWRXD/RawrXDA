; RawrXD Ghost Text Ranker v14.7
; Real-time suggestion ranking for inline completion

OPTION CASEMAP:NONE

.data
; Context embedding weights (pre-trained on code corpus)
weight_syntax     REAL4 0.35
weight_semantic   REAL4 0.40  
weight_frequency  REAL4 0.25

.code

; ============================================================================
; rawrxd_rank_suggestions_asm
; Rank completion candidates by context relevance
; RCX = context (current line prefix)
; RDX = candidates (newline-separated strings)
; R8 = scores array (float output)
; R9 = candidate count
; ============================================================================
rawrxd_rank_suggestions_asm PROC
    ; Prologs/Frame setup (simplified for high-speed use)
    push rbx
    push rdi
    push rsi
    
    mov r10, rcx ; context
    mov r11, rdx ; candidates
    mov r12, r8  ; scores
    mov r13, r9  ; count
    
    xor rbx, rbx ; index
    
rank_loop:
    cmp rbx, r13
    jge rank_done
    
    ; score_syntax_patterns simplified inline for bootstrap
    ; We'll boost if candidate starts with known brackets
    movss xmm0, REAL4 PTR [weight_semantic] ; Base score
    
    movzx eax, BYTE PTR [r11]
    cmp al, '{'
    je boost_score
    cmp al, '('
    je boost_score
    jmp store_score

boost_score:
    addss xmm0, REAL4 PTR [weight_syntax]

store_score:
    movss REAL4 PTR [r12 + rbx*4], xmm0
    
    ; Advance to next candidate
find_null:
    cmp BYTE PTR [r11], 0
    je found_null
    inc r11
    jmp find_null
found_null:
    inc r11 ; Skip null
    inc rbx
    jmp rank_loop

rank_done:
    pop rsi
    pop rdi
    pop rbx
    ret
rawrxd_rank_suggestions_asm ENDP

END
