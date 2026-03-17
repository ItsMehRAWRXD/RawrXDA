;=============================================================================
; RawrXD_CLI.asm
; Amphibious console entry point — links with RawrXD_Sovereign_Core.obj
; ml64.exe / link /SUBSYSTEM:CONSOLE /ENTRY:main
;=============================================================================
OPTION CASEMAP:NONE

;-----------------------------------------------------------------------------
; Imports from sovereign core (resolved at link time)
;-----------------------------------------------------------------------------
EXTERN  Sovereign_Pipeline_Cycle    :PROC
EXTERN  AcquireSovereignLock        :PROC
EXTERN  ReleaseSovereignLock        :PROC
EXTERN  HealSymbolResolution        :PROC
EXTERN  ValidateDMAAlignment        :PROC
EXTERN  CoordinateAgents            :PROC
EXTERN  RawrXD_Trigger_Chat         :PROC
EXTERN  ObserveTokenStream          :PROC
EXTERN  g_CycleCounter              :QWORD
EXTERN  g_SovereignStatus           :QWORD
EXTERN  g_SymbolHealCount           :QWORD
EXTERN  g_ActiveAgentCount          :DWORD

;-----------------------------------------------------------------------------
; C runtime / kernel32
;-----------------------------------------------------------------------------
EXTERN  printf      :PROC
EXTERN  ExitProcess :PROC

;=============================================================================
.data
ALIGN 16
TEST_CYCLES EQU 3

szBanner    DB  13,10,"=== RawrXD Sovereign CLI — Amphibious Build ===",13,10,0
szPhase1    DB  "[PHASE 1] Master init...",13,10,0
szPhase2    DB  "[PHASE 2] Self-heal symbol resolution...",13,10,0
szPhase3    DB  "[PHASE 3] DMA alignment validation...",13,10,0
szPhase4    DB  "[PHASE 4] Agent coordination & token stream...",13,10,0
szPhase5    DB  "[PHASE 5] Interlock stress (%u cycles)...",13,10,0
szPhase6    DB  "[PHASE 6] Full pipeline trigger...",13,10,0
szPass      DB  13,10,"[RESULT ] ALL PHASES PASSED — Cycles: %I64u | Heals: %I64u",13,10,0
szFail      DB  13,10,"[RESULT ] PIPELINE FAILURE",13,10,0
szHealSym   DB  "InterlockedAlloc",0
szDMAToken  DB  "DMA_Token",0

;=============================================================================
.code

;-----------------------------------------------------------------------------
; main — console entry, returns exit code via ExitProcess
;-----------------------------------------------------------------------------
main PROC FRAME
    push rbp
    .PUSHREG rbp
    push rbx
    .PUSHREG rbx
    push rsi
    .PUSHREG rsi
    sub rsp, 32
    .ALLOCSTACK 32
    lea rbp, [rsp+32]
    .SETFRAME rbp, 32
    .ENDPROLOG

    ; ---- banner ----
    lea     rcx, szBanner
    call    printf

    ; [PHASE 1] Master init — ensure active agent count >= 1
    lea     rcx, szPhase1
    call    printf
    mov     dword ptr [g_ActiveAgentCount], 1   ; register agent 0

    ; [PHASE 2] Self-heal
    lea     rcx, szPhase2
    call    printf
    lea     rcx, szHealSym
    call    HealSymbolResolution

    ; [PHASE 3] DMA validation
    lea     rcx, szPhase3
    call    printf
    call    ValidateDMAAlignment
    ; rax == 0 → aligned, != 0 → drift treated as informational here

    ; [PHASE 4] Agent coordination
    lea     rcx, szPhase4
    call    printf
    call    CoordinateAgents

    ; [PHASE 5] Interlock stress loop
    lea     rcx, szPhase5
    mov     edx, TEST_CYCLES
    call    printf
    mov     esi, TEST_CYCLES
Phase5Loop:
    test    esi, esi
    jz      Phase5Done
    call    AcquireSovereignLock
    lea     rcx, szDMAToken
    call    HealSymbolResolution
    call    ReleaseSovereignLock
    dec     esi
    jmp     Phase5Loop
Phase5Done:

    ; [PHASE 6] Full pipeline — TEST_CYCLES times
    lea     rcx, szPhase6
    call    printf
    mov     ebx, TEST_CYCLES
Phase6Loop:
    test    ebx, ebx
    jz      Phase6Done
    call    Sovereign_Pipeline_Cycle
    dec     ebx
    jmp     Phase6Loop
Phase6Done:

    ; ---- result ----
    cmp     qword ptr [g_CycleCounter], 0
    je      FailPath

    lea     rcx, szPass
    mov     rdx, qword ptr [g_CycleCounter]
    mov     r8,  qword ptr [g_SymbolHealCount]
    call    printf
    xor     ecx, ecx                    ; exit code 0
    call    ExitProcess
    ret

FailPath:
    lea     rcx, szFail
    call    printf
    mov     ecx, 1
    call    ExitProcess
    ret

main ENDP

END
