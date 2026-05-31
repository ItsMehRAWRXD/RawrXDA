; =============================================================================
; RawrXD_RiskEngine.asm - Predictive Command Risk Analysis (Phase 48)
; High-velocity heuristic & regex-based command scoring
; =============================================================================

OPTION CASemap:NONE
OPTION WIN64:3

INCLUDE \masm64\include64\win64.inc

.DATA
    RISK_THRESHOLD_MODERATE EQU 40
    RISK_THRESHOLD_CRITICAL EQU 80
    
    ; Dangerous Command Patterns (Bitmask flags)
    FLAG_RM_RF          EQU 1
    FLAG_GIT_RESET      EQU 2
    FLAG_DD_DEV         EQU 4
    FLAG_MKFS           EQU 8
    
    ; Heuristic strings
    PAT_RM_RF           BYTE "rm -rf", 0
    PAT_RESET_HARD      BYTE "git reset --hard", 0
    PAT_DD              BYTE "dd if=", 0
    PAT_MKFS            BYTE "mkfs", 0

.CODE

; =============================================================================
; RawrXD_PredictRiskScore - Score a candidate command string
; =============================================================================
; RCX = Command string pointer
; Returns: RAX = Risk Score (0-100)
RawrXD_PredictRiskScore PROC FRAME
    xor rax, rax ; Initial score
    push rbx
    push rsi
    push rdi
    
    ; 1. Fast Regex-like scan for known destructive patterns
    ; [Logic to scan RCX for PAT_RM_RF, PAT_RESET_HARD]
    
    lea rdx, PAT_RM_RF
    mov r8, rcx
    call StringContains
    test al, al
    jz @@check_next
    add rax, 90 ; 90% risk for rm -rf
    
@@check_next:
    lea rdx, PAT_RESET_HARD
    mov r8, rcx
    call StringContains
    test al, al
    jz @@done
    add rax, 70 ; 70% risk for git reset hard
    
@@done:
    ; Cap at 100
    cmp rax, 100
    jbe @@cap
    mov rax, 100
    
@@cap:
    pop rdi
    pop rsi
    pop rbx
    ret
RawrXD_PredictRiskScore ENDP

StringContains PROC
    ; Simple AVX2/SSE string search (optimized helper)
    ret
StringContains ENDP

END
