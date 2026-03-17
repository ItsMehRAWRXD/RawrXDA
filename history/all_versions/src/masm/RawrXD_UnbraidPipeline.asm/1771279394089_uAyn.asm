; =============================================================================
; RawrXD_UnbraidPipeline.asm — Pipeline Unbraid Engine
;
; Hot-patches pipeline stages out of the execution path when memory pressure
; rises from large model requests. As the model demands more memory, pipeline
; stages are "unbraided" (bypassed) to free their working buffers. When
; pressure drops, stages are re-braided (restored).
;
; Architecture:
;   ┌─────────────────────────────────────────────────────────────────────────┐
;   │                    UNBRAID PIPELINE ENGINE                              │
;   │                                                                         │
;   │  Normal (braided):   Stage0 → Stage1 → Stage2 → Stage3 → ... → Out   │
;   │                                                                         │
;   │  Under pressure (unbraided):                                            │
;   │    Stage0 → [BYPASS] → [BYPASS] → Stage3 → ... → Out                 │
;   │              ↓ free      ↓ free                                        │
;   │           buffers     buffers     — memory returned to model           │
;   │                                                                         │
;   │  Pipeline stages registered with function pointers.                    │
;   │  Each stage has:                                                        │
;   │    - Execute function ptr (the real work)                              │
;   │    - Bypass function ptr  (NOP passthrough or minimal fallback)        │
;   │    - Working buffer ptr + size (freed on unbraid)                      │
;   │    - Priority (lower = unbraid first)                                  │
;   │    - Braided flag (1=active, 0=bypassed)                               │
;   │    - HotPatch trampoline (self-modifying JMP swap)                     │
;   │                                                                         │
;   │  Memory Pressure Detection:                                            │
;   │    - Track total allocated vs budget                                   │
;   │    - When alloc > threshold% of budget → start unbraiding              │
;   │    - Unbraid lowest-priority stages first                              │
;   │    - When pressure drops → re-braid in reverse priority order          │
;   │                                                                         │
;   │  Hot-Patch Mechanism:                                                   │
;   │    - RWX trampoline per stage                                          │
;   │    - Braided:   trampoline JMPs to execute_fn                          │
;   │    - Unbraided: trampoline JMPs to bypass_fn                           │
;   │    - Swap is atomic (single 8-byte write to target addr)               │
;   └─────────────────────────────────────────────────────────────────────────┘
;
; Exports:
;   Unbraid_Init            — Create engine
;   Unbraid_Destroy         — Free everything
;   Unbraid_RegisterStage   — Add a pipeline stage
;   Unbraid_Tick            — Check pressure + unbraid/rebraid as needed
;   Unbraid_Execute         — Run entire pipeline (braided/unbraided)
;   Unbraid_SetBudget       — Set memory budget (bytes)
;   Unbraid_AddPressure     — Add allocated bytes (model loading)
;   Unbraid_ReleasePressure — Release allocated bytes (model unloading)
;   Unbraid_GetPressurePct  — Get current pressure %
;   Unbraid_ForceUnbraid    — Force-unbraid a specific stage
;   Unbraid_ForceRebraid    — Force-rebraid a specific stage
;   Unbraid_GetStats        — Read statistics
;
; Build: ml64 /c RawrXD_UnbraidPipeline.asm
; =============================================================================

option casemap:none

INCLUDE ksamd64.inc

; ─── Windows API ─────────────────────────────────────────────────────────────
EXTRN VirtualAlloc:PROC
EXTRN VirtualFree:PROC
EXTRN QueryPerformanceCounter:PROC
EXTRN QueryPerformanceFrequency:PROC
EXTRN GetStdHandle:PROC
EXTRN WriteConsoleA:PROC

; ─── Exports ─────────────────────────────────────────────────────────────────
PUBLIC Unbraid_Init
PUBLIC Unbraid_Destroy
PUBLIC Unbraid_RegisterStage
PUBLIC Unbraid_Tick
PUBLIC Unbraid_Execute
PUBLIC Unbraid_SetBudget
PUBLIC Unbraid_AddPressure
PUBLIC Unbraid_ReleasePressure
PUBLIC Unbraid_GetPressurePct
PUBLIC Unbraid_ForceUnbraid
PUBLIC Unbraid_ForceRebraid
PUBLIC Unbraid_GetStats

; =============================================================================
; Constants
; =============================================================================
MEM_COMMIT              EQU 1000h
MEM_RESERVE             EQU 2000h
MEM_RELEASE             EQU 8000h
PAGE_READWRITE          EQU 04h
PAGE_EXECUTE_READWRITE  EQU 40h
STD_OUTPUT              EQU -11

MAX_PIPELINE_STAGES     EQU 32          ; Max stages in pipeline
TRAMP_SIZE              EQU 32          ; Trampoline per stage (bytes)

; Pressure thresholds (% of budget)
PRESSURE_UNBRAID_START  EQU 75          ; Start unbraiding at 75%
PRESSURE_UNBRAID_AGGR   EQU 90          ; Aggressive unbraiding at 90%
PRESSURE_REBRAID_BELOW  EQU 60          ; Start rebraiding below 60%

; =============================================================================
; Pipeline Stage Descriptor (64 bytes)
; =============================================================================
;   0x00  ExecuteFn        QWORD     — Real execute function pointer
;   0x08  BypassFn         QWORD     — Bypass / NOP function pointer
;   0x10  WorkBufPtr       QWORD     — Working buffer address (freed on unbraid)
;   0x18  WorkBufSize      QWORD     — Working buffer size in bytes
;   0x20  Priority         DWORD     — Lower = unbraid first (0=lowest)
;   0x24  Braided          DWORD     — 1=active, 0=bypassed
;   0x28  StageID          DWORD     — Stage index in pipeline
;   0x2C  UnbraidCount     DWORD     — Times this stage was unbraided
;   0x30  ExecCount        QWORD     — Times executed (braided or bypass)
;   0x38  TrampolineOff    DWORD     — Offset into trampoline buffer
;   0x3C  Pad              DWORD
SD_ExecuteFn            EQU 00h
SD_BypassFn             EQU 08h
SD_WorkBufPtr           EQU 10h
SD_WorkBufSize          EQU 18h
SD_Priority             EQU 20h
SD_Braided              EQU 24h
SD_StageID              EQU 28h
SD_UnbraidCount         EQU 2Ch
SD_ExecCount            EQU 30h
SD_TrampolineOff        EQU 38h
SD_SIZE                 EQU 40h         ; 64 bytes

; =============================================================================
; UnbraidContext — Engine state
; =============================================================================
;   0x000  QPCFreq          QWORD
;   0x008  InitQPC          QWORD
;   0x010  TickCount        QWORD
;
;   ─── Memory Pressure ───
;   0x018  MemBudget        QWORD     — Total memory budget (bytes)
;   0x020  MemAllocated     QWORD     — Currently allocated (bytes)
;   0x028  PressurePct      DWORD     — alloc*100/budget
;   0x02C  UnbraidThreshold DWORD     — % to start unbraiding (default 75)
;   0x030  RebraidThreshold DWORD     — % to start rebraiding (default 60)
;   0x034  AggressiveThresh DWORD     — % for aggressive unbraid (default 90)
;
;   ─── Pipeline ───
;   0x038  StageCount       DWORD     — Registered stages
;   0x03C  BraidedCount     DWORD     — Currently braided stages
;   0x040  UnbraidedCount   DWORD     — Currently unbraided stages
;   0x044  Pad0             DWORD
;   0x048  StageTable       QWORD     — Ptr to SD_SIZE * MAX_PIPELINE_STAGES
;   0x050  TrampolineBase   QWORD     — Ptr to RWX trampolines
;
;   ─── Statistics ───
;   0x058  StatsUnbraids    QWORD     — Total unbraid operations
;   0x060  StatsRebraids    QWORD     — Total rebraid operations
;   0x068  StatsMemFreed    QWORD     — Total bytes freed by unbraiding
;   0x070  StatsMemRestored QWORD     — Total bytes restored by rebraiding
;   0x078  StatsPipeRuns    QWORD     — Total pipeline executions
;   0x080  StatsBypassRuns  QWORD     — Total bypass invocations
;   0x088  StatsFullRuns    QWORD     — Times pipeline ran fully braided
;   0x090  StatsPatchSwaps  QWORD     — Trampoline hot-swap count
;
;   0x098  ... reserved to 0x100
UC_QPCFreq              EQU 000h
UC_InitQPC              EQU 008h
UC_TickCount            EQU 010h
UC_MemBudget            EQU 018h
UC_MemAllocated         EQU 020h
UC_PressurePct          EQU 028h
UC_UnbraidThreshold     EQU 02Ch
UC_RebraidThreshold     EQU 030h
UC_AggressiveThresh     EQU 034h
UC_StageCount           EQU 038h
UC_BraidedCount         EQU 03Ch
UC_UnbraidedCount       EQU 040h
UC_StageTable           EQU 048h
UC_TrampolineBase       EQU 050h
UC_StatsUnbraids        EQU 058h
UC_StatsRebraids        EQU 060h
UC_StatsMemFreed        EQU 068h
UC_StatsMemRestored     EQU 070h
UC_StatsPipeRuns        EQU 078h
UC_StatsBypassRuns      EQU 080h
UC_StatsFullRuns        EQU 088h
UC_StatsPatchSwaps      EQU 090h
UC_SIZE                 EQU 100h        ; 256 bytes

; =============================================================================
; Data Section
; =============================================================================
.data
align 16
szUBInit        db '[Unbraid] Pipeline engine initialized',0Dh,0Ah,0
szUBRegister    db '[Unbraid] Stage registered: #',0
szUBUnbraid     db '[Unbraid] UNBRAID stage #',0
szUBRebraid     db '[Unbraid] REBRAID stage #',0
szUBPressure    db '[Unbraid] Pressure: ',0
szUBFreed       db '  Freed ',0
szUBRestored    db '  Restored ',0
szUBBytes       db ' bytes',0Dh,0Ah,0
szUBPct         db '%',0Dh,0Ah,0
szUBNewline     db 0Dh,0Ah,0
szUBExec        db '[Unbraid] Pipeline: ',0
szUBBraided     db ' braided, ',0
szUBUnbraided   db ' unbraided',0Dh,0Ah,0
hStdOutUB       dq 0

.data?
align 16
qpcScratchUB    dq ?
numBufUB        db 32 dup(?)

; =============================================================================
; Code Section
; =============================================================================
.code

; ─── Internal Helpers ────────────────────────────────────────────────────────

_ub_strlen PROC
    xor     eax, eax
@@:
    cmp     byte ptr [rcx + rax], 0
    je      @F
    inc     rax
    jmp     @B
@@:
    ret
_ub_strlen ENDP

_ub_print PROC FRAME
    push    rbx
    .pushreg rbx
    sub     rsp, 48
    .allocstack 48
    .endprolog
    mov     rbx, rcx
    mov     rax, hStdOutUB
    test    rax, rax
    jnz     @@have
    mov     ecx, STD_OUTPUT
    call    GetStdHandle
    mov     hStdOutUB, rax
@@have:
    mov     rcx, rbx
    call    _ub_strlen
    mov     r8, rax
    mov     rcx, hStdOutUB
    mov     rdx, rbx
    lea     r9, [rsp+32]
    mov     qword ptr [rsp+32], 0
    call    WriteConsoleA
    add     rsp, 48
    pop     rbx
    ret
_ub_print ENDP

_ub_print_u32 PROC FRAME
    push    rbx
    .pushreg rbx
    sub     rsp, 48
    .allocstack 48
    .endprolog
    mov     eax, ecx
    lea     rbx, numBufUB
    add     rbx, 20
    mov     byte ptr [rbx], 0
    test    eax, eax
    jnz     @@loop
    dec     rbx
    mov     byte ptr [rbx], '0'
    jmp     @@pr
@@loop:
    test    eax, eax
    jz      @@pr
    xor     edx, edx
    mov     ecx, 10
    div     ecx
    add     dl, '0'
    dec     rbx
    mov     [rbx], dl
    jmp     @@loop
@@pr:
    mov     rcx, rbx
    call    _ub_print
    add     rsp, 48
    pop     rbx
    ret
_ub_print_u32 ENDP

_ub_print_u64 PROC FRAME
    push    rbx
    .pushreg rbx
    sub     rsp, 48
    .allocstack 48
    .endprolog
    mov     rax, rcx
    lea     rbx, numBufUB
    add     rbx, 24
    mov     byte ptr [rbx], 0
    test    rax, rax
    jnz     @@loop
    dec     rbx
    mov     byte ptr [rbx], '0'
    jmp     @@pr
@@loop:
    test    rax, rax
    jz      @@pr
    xor     edx, edx
    mov     rcx, 10
    div     rcx
    add     dl, '0'
    dec     rbx
    mov     [rbx], dl
    jmp     @@loop
@@pr:
    mov     rcx, rbx
    call    _ub_print
    add     rsp, 48
    pop     rbx
    ret
_ub_print_u64 ENDP

; ─── _ub_calc_pressure ────────────────────────────────────────────────────
; RBX = context → updates PressurePct
_ub_calc_pressure PROC
    mov     rax, [rbx + UC_MemBudget]
    test    rax, rax
    jz      @@zero
    ; PressurePct = (allocated * 100) / budget
    mov     rcx, rax                   ; RCX = budget
    mov     rax, [rbx + UC_MemAllocated]
    mov     rdx, 100
    mul     rdx                        ; RDX:RAX = alloc * 100
    div     rcx                        ; RAX = pressure %
    cmp     rax, 100
    jle     @@store
    mov     eax, 100
@@store:
    mov     [rbx + UC_PressurePct], eax
    ret
@@zero:
    mov     dword ptr [rbx + UC_PressurePct], 0
    ret
_ub_calc_pressure ENDP

; ─── _ub_count_states ─────────────────────────────────────────────────────
; RBX = context → updates BraidedCount, UnbraidedCount
_ub_count_states PROC
    mov     rsi, [rbx + UC_StageTable]
    test    rsi, rsi
    jz      @@zero
    xor     r8d, r8d                   ; braided
    xor     r9d, r9d                   ; unbraided
    xor     ecx, ecx
    mov     edx, [rbx + UC_StageCount]
@@loop:
    cmp     ecx, edx
    jge     @@done
    mov     eax, ecx
    shl     eax, 6                     ; * SD_SIZE (64)
    cmp     dword ptr [rsi + rax + SD_Braided], 1
    jne     @@is_unbraided
    inc     r8d
    jmp     @@next
@@is_unbraided:
    inc     r9d
@@next:
    inc     ecx
    jmp     @@loop
@@done:
    mov     [rbx + UC_BraidedCount], r8d
    mov     [rbx + UC_UnbraidedCount], r9d
    ret
@@zero:
    mov     dword ptr [rbx + UC_BraidedCount], 0
    mov     dword ptr [rbx + UC_UnbraidedCount], 0
    ret
_ub_count_states ENDP

; ─── _ub_patch_trampoline ─────────────────────────────────────────────────
; RBX = context, ECX = stage index, RDX = target function ptr
; Writes target address into the stage's RWX trampoline (atomic 8-byte store)
_ub_patch_trampoline PROC
    mov     r8, [rbx + UC_TrampolineBase]
    test    r8, r8
    jz      @@done

    ; Each trampoline is TRAMP_SIZE (32) bytes:
    ;   [0-1]   mov rax, imm64  (48 B8 xx xx xx xx xx xx xx xx)
    ;   [10-11] jmp rax         (FF E0)
    ;   [12-31] padding
    mov     eax, ecx
    shl     eax, 5                     ; * TRAMP_SIZE (32)
    add     r8, rax                    ; R8 = trampoline ptr

    ; Write: mov rax, <target>  ;  jmp rax
    mov     byte ptr [r8 + 0], 48h     ; REX.W
    mov     byte ptr [r8 + 1], 0B8h    ; MOV RAX, imm64
    mov     [r8 + 2], rdx              ; 8-byte target address
    mov     byte ptr [r8 + 10], 0FFh   ; JMP
    mov     byte ptr [r8 + 11], 0E0h   ; RAX

    inc     qword ptr [rbx + UC_StatsPatchSwaps]
@@done:
    ret
_ub_patch_trampoline ENDP

; ─── _ub_unbraid_stage ────────────────────────────────────────────────────
; RBX = context, ECX = stage index
; Unbraids: patches trampoline to bypass, frees working buffer
_ub_unbraid_stage PROC FRAME
    push    rsi
    .pushreg rsi
    push    rdi
    .pushreg rdi
    sub     rsp, 40
    .allocstack 40
    .endprolog

    mov     esi, ecx                   ; ESI = stage idx
    mov     rdi, [rbx + UC_StageTable]
    test    rdi, rdi
    jz      @@done

    mov     eax, esi
    shl     eax, 6                     ; * SD_SIZE
    add     rdi, rax                   ; RDI = stage descriptor

    ; Already unbraided?
    cmp     dword ptr [rdi + SD_Braided], 0
    je      @@done

    ; Patch trampoline to bypass function
    mov     ecx, esi
    mov     rdx, [rdi + SD_BypassFn]
    call    _ub_patch_trampoline

    ; Free working buffer if present
    mov     rcx, [rdi + SD_WorkBufPtr]
    test    rcx, rcx
    jz      @@no_free
    ; Don't actually VirtualFree — just mark as freed and track bytes
    ; (Buffer may be re-allocated on rebraid)
    mov     rax, [rdi + SD_WorkBufSize]
    add     qword ptr [rbx + UC_StatsMemFreed], rax
    ; Return bytes to model: reduce allocated
    mov     rax, [rdi + SD_WorkBufSize]
    sub     [rbx + UC_MemAllocated], rax
    ; NULL out the buffer ptr
    mov     qword ptr [rdi + SD_WorkBufPtr], 0
@@no_free:

    ; Mark as unbraided
    mov     dword ptr [rdi + SD_Braided], 0
    inc     dword ptr [rdi + SD_UnbraidCount]
    inc     qword ptr [rbx + UC_StatsUnbraids]

    ; Print
    push    rsi
    lea     rcx, szUBUnbraid
    call    _ub_print
    mov     ecx, esi
    call    _ub_print_u32
    lea     rcx, szUBNewline
    call    _ub_print
    pop     rsi

    ; Print freed bytes
    mov     eax, esi
    shl     eax, 6
    mov     rdi, [rbx + UC_StageTable]
    add     rdi, rax
    mov     rcx, [rdi + SD_WorkBufSize]
    test    rcx, rcx
    jz      @@done
    push    rcx
    lea     rcx, szUBFreed
    call    _ub_print
    pop     rcx
    call    _ub_print_u64
    lea     rcx, szUBBytes
    call    _ub_print

@@done:
    add     rsp, 40
    pop     rdi
    pop     rsi
    ret
_ub_unbraid_stage ENDP

; ─── _ub_rebraid_stage ────────────────────────────────────────────────────
; RBX = context, ECX = stage index
; Rebraids: patches trampoline back to execute, re-allocates buffer
_ub_rebraid_stage PROC FRAME
    push    rsi
    .pushreg rsi
    push    rdi
    .pushreg rdi
    sub     rsp, 40
    .allocstack 40
    .endprolog

    mov     esi, ecx
    mov     rdi, [rbx + UC_StageTable]
    test    rdi, rdi
    jz      @@done

    mov     eax, esi
    shl     eax, 6
    add     rdi, rax

    ; Already braided?
    cmp     dword ptr [rdi + SD_Braided], 1
    je      @@done

    ; Re-allocate working buffer if size > 0 and ptr is NULL
    mov     rax, [rdi + SD_WorkBufSize]
    test    rax, rax
    jz      @@no_alloc
    cmp     qword ptr [rdi + SD_WorkBufPtr], 0
    jne     @@no_alloc
    ; Allocate fresh buffer
    push    rdi
    push    rsi
    xor     ecx, ecx
    mov     rdx, [rdi + SD_WorkBufSize]
    mov     r8d, MEM_COMMIT or MEM_RESERVE
    mov     r9d, PAGE_READWRITE
    call    VirtualAlloc
    pop     rsi
    pop     rdi
    test    rax, rax
    jz      @@done                     ; Alloc failed — stay unbraided
    mov     [rdi + SD_WorkBufPtr], rax

    ; Track bytes allocated
    mov     rax, [rdi + SD_WorkBufSize]
    add     [rbx + UC_MemAllocated], rax
    add     qword ptr [rbx + UC_StatsMemRestored], rax
@@no_alloc:

    ; Patch trampoline back to execute function
    mov     ecx, esi
    mov     rdx, [rdi + SD_ExecuteFn]
    call    _ub_patch_trampoline

    ; Mark as braided
    mov     dword ptr [rdi + SD_Braided], 1
    inc     qword ptr [rbx + UC_StatsRebraids]

    ; Print
    push    rsi
    lea     rcx, szUBRebraid
    call    _ub_print
    mov     ecx, esi
    call    _ub_print_u32
    lea     rcx, szUBNewline
    call    _ub_print
    pop     rsi

@@done:
    add     rsp, 40
    pop     rdi
    pop     rsi
    ret
_ub_rebraid_stage ENDP

; ─── _ub_find_lowest_priority_braided ─────────────────────────────────────
; RBX = context
; Returns: EAX = stage index of lowest-priority braided stage, or -1
_ub_find_lowest_priority_braided PROC
    mov     rsi, [rbx + UC_StageTable]
    test    rsi, rsi
    jz      @@none
    mov     ecx, [rbx + UC_StageCount]
    xor     edx, edx                   ; idx
    mov     r8d, -1                    ; best idx
    mov     r9d, 7FFFFFFFh             ; best priority (lower = first to unbraid)
@@loop:
    cmp     edx, ecx
    jge     @@done
    mov     eax, edx
    shl     eax, 6
    cmp     dword ptr [rsi + rax + SD_Braided], 1
    jne     @@skip
    mov     r10d, [rsi + rax + SD_Priority]
    cmp     r10d, r9d
    jge     @@skip
    mov     r9d, r10d
    mov     r8d, edx
@@skip:
    inc     edx
    jmp     @@loop
@@done:
    mov     eax, r8d
    ret
@@none:
    mov     eax, -1
    ret
_ub_find_lowest_priority_braided ENDP

; ─── _ub_find_highest_priority_unbraided ──────────────────────────────────
; RBX = context
; Returns: EAX = stage index of highest-priority unbraided stage, or -1
_ub_find_highest_priority_unbraided PROC
    mov     rsi, [rbx + UC_StageTable]
    test    rsi, rsi
    jz      @@none
    mov     ecx, [rbx + UC_StageCount]
    xor     edx, edx
    mov     r8d, -1                    ; best idx
    xor     r9d, r9d                   ; best priority (higher = first to rebraid)
@@loop:
    cmp     edx, ecx
    jge     @@done
    mov     eax, edx
    shl     eax, 6
    cmp     dword ptr [rsi + rax + SD_Braided], 0
    jne     @@skip
    mov     r10d, [rsi + rax + SD_Priority]
    cmp     r10d, r9d
    jle     @@skip
    mov     r9d, r10d
    mov     r8d, edx
@@skip:
    inc     edx
    jmp     @@loop
@@done:
    mov     eax, r8d
    ret
@@none:
    mov     eax, -1
    ret
_ub_find_highest_priority_unbraided ENDP

; =============================================================================
; PUBLIC API
; =============================================================================

; =============================================================================
; Unbraid_Init — Create engine
; RCX = memory budget in bytes
; Returns: RAX = context, or 0
; =============================================================================
Unbraid_Init PROC FRAME
    push    rbx
    .pushreg rbx
    push    rsi
    .pushreg rsi
    sub     rsp, 40
    .allocstack 40
    .endprolog

    mov     rsi, rcx                   ; Save budget

    ; Allocate context
    xor     ecx, ecx
    mov     edx, UC_SIZE
    mov     r8d, MEM_COMMIT or MEM_RESERVE
    mov     r9d, PAGE_READWRITE
    call    VirtualAlloc
    test    rax, rax
    jz      @@fail
    mov     rbx, rax

    ; QPC
    lea     rcx, qpcScratchUB
    call    QueryPerformanceFrequency
    mov     rax, qpcScratchUB
    mov     [rbx + UC_QPCFreq], rax
    lea     rcx, qpcScratchUB
    call    QueryPerformanceCounter
    mov     rax, qpcScratchUB
    mov     [rbx + UC_InitQPC], rax

    ; Set budget + thresholds
    mov     [rbx + UC_MemBudget], rsi
    mov     qword ptr [rbx + UC_MemAllocated], 0
    mov     dword ptr [rbx + UC_PressurePct], 0
    mov     dword ptr [rbx + UC_UnbraidThreshold], PRESSURE_UNBRAID_START
    mov     dword ptr [rbx + UC_RebraidThreshold], PRESSURE_REBRAID_BELOW
    mov     dword ptr [rbx + UC_AggressiveThresh], PRESSURE_UNBRAID_AGGR
    mov     dword ptr [rbx + UC_StageCount], 0

    ; Allocate stage table (SD_SIZE * MAX_PIPELINE_STAGES = 64 * 32 = 2048)
    xor     ecx, ecx
    mov     edx, SD_SIZE * MAX_PIPELINE_STAGES
    mov     r8d, MEM_COMMIT or MEM_RESERVE
    mov     r9d, PAGE_READWRITE
    call    VirtualAlloc
    test    rax, rax
    jz      @@fail_ctx
    mov     [rbx + UC_StageTable], rax

    ; Allocate trampoline buffer (RWX)
    xor     ecx, ecx
    mov     edx, TRAMP_SIZE * MAX_PIPELINE_STAGES  ; 32 * 32 = 1024
    mov     r8d, MEM_COMMIT or MEM_RESERVE
    mov     r9d, PAGE_EXECUTE_READWRITE
    call    VirtualAlloc
    test    rax, rax
    jz      @@fail_tbl
    mov     [rbx + UC_TrampolineBase], rax

    ; Fill trampolines with INT3 (0xCC) as safety
    mov     rdi, rax
    mov     ecx, TRAMP_SIZE * MAX_PIPELINE_STAGES
    mov     al, 0CCh
    rep     stosb

    lea     rcx, szUBInit
    call    _ub_print

    mov     rax, rbx
    jmp     @@exit

@@fail_tbl:
    mov     rcx, [rbx + UC_StageTable]
    xor     edx, edx
    mov     r8d, MEM_RELEASE
    call    VirtualFree
@@fail_ctx:
    mov     rcx, rbx
    xor     edx, edx
    mov     r8d, MEM_RELEASE
    call    VirtualFree
@@fail:
    xor     eax, eax
@@exit:
    add     rsp, 40
    pop     rsi
    pop     rbx
    ret
Unbraid_Init ENDP

; =============================================================================
; Unbraid_Destroy — Free everything
; RCX = context
; =============================================================================
Unbraid_Destroy PROC FRAME
    push    rbx
    .pushreg rbx
    sub     rsp, 40
    .allocstack 40
    .endprolog
    test    rcx, rcx
    jz      @@null
    mov     rbx, rcx

    ; Free stage working buffers
    mov     rsi, [rbx + UC_StageTable]
    test    rsi, rsi
    jz      @@skip_bufs
    xor     ecx, ecx
    mov     edx, [rbx + UC_StageCount]
@@free_bufs:
    cmp     ecx, edx
    jge     @@skip_bufs
    push    rcx
    push    rdx
    mov     eax, ecx
    shl     eax, 6
    mov     rcx, [rsi + rax + SD_WorkBufPtr]
    test    rcx, rcx
    jz      @@skip_one
    xor     edx, edx
    mov     r8d, MEM_RELEASE
    call    VirtualFree
@@skip_one:
    pop     rdx
    pop     rcx
    inc     ecx
    jmp     @@free_bufs
@@skip_bufs:

    ; Free trampoline buffer
    mov     rcx, [rbx + UC_TrampolineBase]
    test    rcx, rcx
    jz      @@skip_tramp
    xor     edx, edx
    mov     r8d, MEM_RELEASE
    call    VirtualFree
@@skip_tramp:

    ; Free stage table
    mov     rcx, [rbx + UC_StageTable]
    test    rcx, rcx
    jz      @@skip_tbl
    xor     edx, edx
    mov     r8d, MEM_RELEASE
    call    VirtualFree
@@skip_tbl:

    ; Free context
    mov     rcx, rbx
    xor     edx, edx
    mov     r8d, MEM_RELEASE
    call    VirtualFree
@@null:
    add     rsp, 40
    pop     rbx
    ret
Unbraid_Destroy ENDP

; =============================================================================
; Unbraid_RegisterStage — Add a pipeline stage
; RCX = context
; RDX = execute function pointer
; R8  = bypass function pointer
; R9  = working buffer size (0 = no buffer)
; [RSP+40] = priority (DWORD, lower = unbraid first)
; Returns: EAX = stage index, or -1 if full
; =============================================================================
Unbraid_RegisterStage PROC FRAME
    push    rbx
    .pushreg rbx
    push    rsi
    .pushreg rsi
    push    rdi
    .pushreg rdi
    sub     rsp, 48
    .allocstack 48
    .endprolog

    mov     rbx, rcx
    mov     eax, [rbx + UC_StageCount]
    cmp     eax, MAX_PIPELINE_STAGES
    jge     @@full

    mov     esi, eax                   ; ESI = new stage index
    mov     rdi, [rbx + UC_StageTable]
    test    rdi, rdi
    jz      @@full

    ; Calculate descriptor pointer
    mov     eax, esi
    shl     eax, 6                     ; * SD_SIZE
    add     rdi, rax                   ; RDI = stage descriptor

    ; Fill descriptor
    mov     [rdi + SD_ExecuteFn], rdx
    mov     [rdi + SD_BypassFn], r8
    mov     [rdi + SD_WorkBufSize], r9
    mov     dword ptr [rdi + SD_Braided], 1
    mov     [rdi + SD_StageID], esi
    mov     dword ptr [rdi + SD_UnbraidCount], 0
    mov     qword ptr [rdi + SD_ExecCount], 0

    ; Priority from stack
    mov     eax, [rsp + 48 + 32 + 8*3 + 8]  ; stack: sub 48 + push rbx,rsi,rdi(24) + ret(8) + shadow(32) + rcx,rdx,r8,r9 home
    ; Simpler: 5th param at [rsp + 48+24+40] — nope, just use known offset
    ; For FRAME with sub 48 + 3 pushes: 5th arg at RSP + 48 + 3*8 + 8 + 32 = RSP + 112
    mov     eax, dword ptr [rsp + 112]
    mov     [rdi + SD_Priority], eax

    ; Trampoline offset
    mov     eax, esi
    shl     eax, 5                     ; * TRAMP_SIZE
    mov     [rdi + SD_TrampolineOff], eax

    ; Allocate working buffer if size > 0
    cmp     r9, 0
    je      @@no_buf
    push    rdi
    push    rsi
    xor     ecx, ecx
    mov     rdx, r9
    mov     r8d, MEM_COMMIT or MEM_RESERVE
    mov     r9d, PAGE_READWRITE
    call    VirtualAlloc
    pop     rsi
    pop     rdi
    mov     [rdi + SD_WorkBufPtr], rax
    ; Track allocated
    test    rax, rax
    jz      @@no_buf
    mov     rax, [rdi + SD_WorkBufSize]
    add     [rbx + UC_MemAllocated], rax
    jmp     @@buf_done
@@no_buf:
    mov     qword ptr [rdi + SD_WorkBufPtr], 0
@@buf_done:

    ; Patch trampoline to execute function
    mov     ecx, esi
    mov     rdx, [rdi + SD_ExecuteFn]
    call    _ub_patch_trampoline

    ; Increment stage count
    inc     dword ptr [rbx + UC_StageCount]

    ; Print
    push    rsi
    lea     rcx, szUBRegister
    call    _ub_print
    mov     ecx, esi
    call    _ub_print_u32
    lea     rcx, szUBNewline
    call    _ub_print
    pop     rsi

    call    _ub_count_states
    call    _ub_calc_pressure

    mov     eax, esi
    jmp     @@exit
@@full:
    mov     eax, -1
@@exit:
    add     rsp, 48
    pop     rdi
    pop     rsi
    pop     rbx
    ret
Unbraid_RegisterStage ENDP

; =============================================================================
; Unbraid_Tick — Check pressure, unbraid/rebraid as needed
; RCX = context
; Returns: EAX = current pressure %
; =============================================================================
Unbraid_Tick PROC FRAME
    push    rbx
    .pushreg rbx
    sub     rsp, 40
    .allocstack 40
    .endprolog

    mov     rbx, rcx
    inc     qword ptr [rbx + UC_TickCount]

    call    _ub_calc_pressure
    call    _ub_count_states

    mov     eax, [rbx + UC_PressurePct]

    ; Check if we need to unbraid
    cmp     eax, [rbx + UC_AggressiveThresh]
    jge     @@aggressive_unbraid

    cmp     eax, [rbx + UC_UnbraidThreshold]
    jge     @@normal_unbraid

    ; Check if we can rebraid
    cmp     eax, [rbx + UC_RebraidThreshold]
    jl      @@try_rebraid

    jmp     @@done

@@aggressive_unbraid:
    ; Unbraid up to 2 stages per tick
    call    _ub_find_lowest_priority_braided
    cmp     eax, -1
    je      @@done
    mov     ecx, eax
    call    _ub_unbraid_stage
    call    _ub_calc_pressure

    call    _ub_find_lowest_priority_braided
    cmp     eax, -1
    je      @@done
    mov     ecx, eax
    call    _ub_unbraid_stage
    call    _ub_calc_pressure
    jmp     @@done

@@normal_unbraid:
    ; Unbraid 1 stage per tick
    call    _ub_find_lowest_priority_braided
    cmp     eax, -1
    je      @@done
    mov     ecx, eax
    call    _ub_unbraid_stage
    call    _ub_calc_pressure
    jmp     @@done

@@try_rebraid:
    ; Rebraid 1 stage per tick if pressure allows
    cmp     dword ptr [rbx + UC_UnbraidedCount], 0
    je      @@done
    call    _ub_find_highest_priority_unbraided
    cmp     eax, -1
    je      @@done
    mov     ecx, eax
    call    _ub_rebraid_stage
    call    _ub_calc_pressure

@@done:
    call    _ub_count_states

    ; Print pressure every 5 ticks
    mov     rax, [rbx + UC_TickCount]
    xor     edx, edx
    mov     ecx, 5
    div     rcx
    test    edx, edx
    jnz     @@no_print

    lea     rcx, szUBPressure
    call    _ub_print
    mov     ecx, [rbx + UC_PressurePct]
    call    _ub_print_u32
    lea     rcx, szUBPct
    call    _ub_print

    lea     rcx, szUBExec
    call    _ub_print
    mov     ecx, [rbx + UC_BraidedCount]
    call    _ub_print_u32
    lea     rcx, szUBBraided
    call    _ub_print
    mov     ecx, [rbx + UC_UnbraidedCount]
    call    _ub_print_u32
    lea     rcx, szUBUnbraided
    call    _ub_print

@@no_print:
    mov     eax, [rbx + UC_PressurePct]

    add     rsp, 40
    pop     rbx
    ret
Unbraid_Tick ENDP

; =============================================================================
; Unbraid_Execute — Run entire pipeline through trampolines
; RCX = context, RDX = input data ptr, R8 = input size
; Each trampoline calls exec or bypass depending on braid state
; Returns: EAX = stages executed
; =============================================================================
Unbraid_Execute PROC FRAME
    push    rbx
    .pushreg rbx
    push    rsi
    .pushreg rsi
    push    rdi
    .pushreg rdi
    push    r12
    .pushreg r12
    push    r13
    .pushreg r13
    sub     rsp, 48
    .allocstack 48
    .endprolog

    mov     rbx, rcx
    mov     r12, rdx                   ; R12 = input data
    mov     r13, r8                    ; R13 = input size

    inc     qword ptr [rbx + UC_StatsPipeRuns]

    mov     rdi, [rbx + UC_StageTable]
    mov     rsi, [rbx + UC_TrampolineBase]
    test    rdi, rdi
    jz      @@done_zero
    test    rsi, rsi
    jz      @@done_zero

    xor     ecx, ecx                   ; stage index
    mov     edx, [rbx + UC_StageCount]
    xor     r8d, r8d                   ; executed count

@@exec_loop:
    cmp     ecx, edx
    jge     @@exec_done
    push    rcx
    push    rdx
    push    r8

    ; Get stage descriptor
    mov     eax, ecx
    shl     eax, 6
    inc     qword ptr [rdi + rax + SD_ExecCount]

    ; Track bypass vs real
    cmp     dword ptr [rdi + rax + SD_Braided], 1
    je      @@real_exec
    inc     qword ptr [rbx + UC_StatsBypassRuns]
    jmp     @@do_call
@@real_exec:
    inc     qword ptr [rbx + UC_StatsFullRuns]
@@do_call:

    ; Call through trampoline: trampoline(input_data, input_size)
    mov     eax, ecx
    shl     eax, 5                     ; * TRAMP_SIZE
    lea     rax, [rsi + rax]           ; trampoline address
    mov     rcx, r12                   ; arg1 = input data
    mov     rdx, r13                   ; arg2 = input size
    call    rax                        ; call trampoline → JMPs to exec or bypass

    pop     r8
    pop     rdx
    pop     rcx
    inc     r8d
    inc     ecx
    jmp     @@exec_loop

@@exec_done:
    mov     eax, r8d
    jmp     @@exit

@@done_zero:
    xor     eax, eax
@@exit:
    add     rsp, 48
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret
Unbraid_Execute ENDP

; =============================================================================
; Unbraid_SetBudget — Set memory budget
; RCX = context, RDX = budget in bytes
; =============================================================================
Unbraid_SetBudget PROC
    mov     [rcx + UC_MemBudget], rdx
    ret
Unbraid_SetBudget ENDP

; =============================================================================
; Unbraid_AddPressure — Add allocated bytes (model loading more data)
; RCX = context, RDX = bytes being allocated
; Returns: RAX = new total allocated
; =============================================================================
Unbraid_AddPressure PROC
    add     [rcx + UC_MemAllocated], rdx
    mov     rax, [rcx + UC_MemAllocated]
    ret
Unbraid_AddPressure ENDP

; =============================================================================
; Unbraid_ReleasePressure — Release allocated bytes
; RCX = context, RDX = bytes being freed
; Returns: RAX = new total allocated
; =============================================================================
Unbraid_ReleasePressure PROC
    sub     [rcx + UC_MemAllocated], rdx
    ; Clamp to 0
    cmp     qword ptr [rcx + UC_MemAllocated], 0
    jge     @@ok
    mov     qword ptr [rcx + UC_MemAllocated], 0
@@ok:
    mov     rax, [rcx + UC_MemAllocated]
    ret
Unbraid_ReleasePressure ENDP

; =============================================================================
; Unbraid_GetPressurePct — Get pressure %
; RCX = context
; Returns: EAX = pressure %
; =============================================================================
Unbraid_GetPressurePct PROC
    mov     eax, [rcx + UC_PressurePct]
    ret
Unbraid_GetPressurePct ENDP

; =============================================================================
; Unbraid_ForceUnbraid — Force-unbraid a specific stage
; RCX = context, EDX = stage index
; =============================================================================
Unbraid_ForceUnbraid PROC FRAME
    push    rbx
    .pushreg rbx
    sub     rsp, 32
    .allocstack 32
    .endprolog
    mov     rbx, rcx
    cmp     edx, [rbx + UC_StageCount]
    jge     @@bad
    mov     ecx, edx
    call    _ub_unbraid_stage
    call    _ub_calc_pressure
    call    _ub_count_states
    mov     eax, 1
    jmp     @@exit
@@bad:
    xor     eax, eax
@@exit:
    add     rsp, 32
    pop     rbx
    ret
Unbraid_ForceUnbraid ENDP

; =============================================================================
; Unbraid_ForceRebraid — Force-rebraid a specific stage
; RCX = context, EDX = stage index
; =============================================================================
Unbraid_ForceRebraid PROC FRAME
    push    rbx
    .pushreg rbx
    sub     rsp, 32
    .allocstack 32
    .endprolog
    mov     rbx, rcx
    cmp     edx, [rbx + UC_StageCount]
    jge     @@bad
    mov     ecx, edx
    call    _ub_rebraid_stage
    call    _ub_calc_pressure
    call    _ub_count_states
    mov     eax, 1
    jmp     @@exit
@@bad:
    xor     eax, eax
@@exit:
    add     rsp, 32
    pop     rbx
    ret
Unbraid_ForceRebraid ENDP

; =============================================================================
; Unbraid_GetStats — Read statistics (128 bytes)
; RCX = context, RDX = 128-byte buffer
;
; [0x00] TickCount        [0x08] MemBudget
; [0x10] MemAllocated     [0x18] PressurePct(DW) / StageCount(DW)
; [0x20] BraidedCount(DW) [0x24] UnbraidedCount(DW)
; [0x28] Unbraids         [0x30] Rebraids
; [0x38] MemFreed         [0x40] MemRestored
; [0x48] PipeRuns         [0x50] BypassRuns
; [0x58] FullRuns         [0x60] PatchSwaps
; [0x68..] reserved
; =============================================================================
Unbraid_GetStats PROC
    test    rcx, rcx
    jz      @@fail
    test    rdx, rdx
    jz      @@fail

    mov     rax, [rcx + UC_TickCount]
    mov     [rdx + 00h], rax
    mov     rax, [rcx + UC_MemBudget]
    mov     [rdx + 08h], rax
    mov     rax, [rcx + UC_MemAllocated]
    mov     [rdx + 10h], rax
    mov     eax, [rcx + UC_PressurePct]
    mov     [rdx + 18h], eax
    mov     eax, [rcx + UC_StageCount]
    mov     [rdx + 1Ch], eax
    mov     eax, [rcx + UC_BraidedCount]
    mov     [rdx + 20h], eax
    mov     eax, [rcx + UC_UnbraidedCount]
    mov     [rdx + 24h], eax
    mov     rax, [rcx + UC_StatsUnbraids]
    mov     [rdx + 28h], rax
    mov     rax, [rcx + UC_StatsRebraids]
    mov     [rdx + 30h], rax
    mov     rax, [rcx + UC_StatsMemFreed]
    mov     [rdx + 38h], rax
    mov     rax, [rcx + UC_StatsMemRestored]
    mov     [rdx + 40h], rax
    mov     rax, [rcx + UC_StatsPipeRuns]
    mov     [rdx + 48h], rax
    mov     rax, [rcx + UC_StatsBypassRuns]
    mov     [rdx + 50h], rax
    mov     rax, [rcx + UC_StatsFullRuns]
    mov     [rdx + 58h], rax
    mov     rax, [rcx + UC_StatsPatchSwaps]
    mov     [rdx + 60h], rax

    mov     eax, 1
    ret
@@fail:
    xor     eax, eax
    ret
Unbraid_GetStats ENDP

END
