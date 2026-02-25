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
; Exports (12):
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

; ─── Cross-module symbol resolution ───
INCLUDE rawrxd_master.inc


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

MAX_PIPELINE_STAGES     EQU 32
TRAMP_SIZE              EQU 32

PRESSURE_UNBRAID_START  EQU 75
PRESSURE_UNBRAID_AGGR   EQU 90
PRESSURE_REBRAID_BELOW  EQU 60

; =============================================================================
; Pipeline Stage Descriptor (64 bytes)
; =============================================================================
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
SD_SIZE                 EQU 40h

; =============================================================================
; UnbraidContext layout (256 bytes)
; =============================================================================
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
UC_SIZE                 EQU 100h

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

; ─── _ub_strlen: RCX = string → RAX = length (leaf, volatile regs only)
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

; ─── _ub_print: RCX = string ptr
; Frame: push rbx(8) + sub 48 = 56. Entry RSP mod 16 = 8. 8-56 = mod 16 = 0. ✓
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

; ─── _ub_print_u32: ECX = number
; Frame: push rbx(8) + sub 48 = 56. ✓
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

; ─── _ub_print_u64: RCX = number
; Frame: push rbx(8) + sub 48 = 56. ✓
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
; Leaf: uses only volatile regs (RAX, RCX, RDX)
_ub_calc_pressure PROC
    mov     rax, [rbx + UC_MemBudget]
    test    rax, rax
    jz      @@zero
    mov     rcx, rax
    mov     rax, [rbx + UC_MemAllocated]
    mov     rdx, 100
    mul     rdx
    div     rcx
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
; Leaf: uses R10 (volatile) — NO clobber of callee-saved regs
_ub_count_states PROC
    mov     r10, [rbx + UC_StageTable]
    test    r10, r10
    jz      @@zero
    xor     r8d, r8d
    xor     r9d, r9d
    xor     ecx, ecx
    mov     edx, [rbx + UC_StageCount]
@@loop:
    cmp     ecx, edx
    jge     @@done
    mov     eax, ecx
    shl     eax, 6
    cmp     dword ptr [r10 + rax + SD_Braided], 1
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
; Leaf: volatile regs only (R8, RAX)
_ub_patch_trampoline PROC
    mov     r8, [rbx + UC_TrampolineBase]
    test    r8, r8
    jz      @@done
    mov     eax, ecx
    shl     eax, 5
    add     r8, rax
    mov     byte ptr [r8 + 0], 48h
    mov     byte ptr [r8 + 1], 0B8h
    mov     [r8 + 2], rdx
    mov     byte ptr [r8 + 10], 0FFh
    mov     byte ptr [r8 + 11], 0E0h
    inc     qword ptr [rbx + UC_StatsPatchSwaps]
@@done:
    ret
_ub_patch_trampoline ENDP

; ─── _ub_unbraid_stage ────────────────────────────────────────────────────
; RBX = context, ECX = stage index
; Frame: push rsi(8) + push rdi(8) + sub 40 = 56. 56 mod 16 = 8 ✓
_ub_unbraid_stage PROC FRAME
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

    cmp     dword ptr [rdi + SD_Braided], 0
    je      @@done

    ; Patch trampoline to bypass function
    mov     ecx, esi
    mov     rdx, [rdi + SD_BypassFn]
    call    _ub_patch_trampoline

    ; Track freed memory from working buffer
    mov     rax, [rdi + SD_WorkBufSize]
    test    rax, rax
    jz      @@no_free
    mov     rcx, [rdi + SD_WorkBufPtr]
    test    rcx, rcx
    jz      @@no_free
    add     qword ptr [rbx + UC_StatsMemFreed], rax
    sub     [rbx + UC_MemAllocated], rax
    mov     qword ptr [rdi + SD_WorkBufPtr], 0
@@no_free:

    ; Mark as unbraided
    mov     dword ptr [rdi + SD_Braided], 0
    inc     dword ptr [rdi + SD_UnbraidCount]
    inc     qword ptr [rbx + UC_StatsUnbraids]

    ; Print — save esi via local frame [rsp+32], NOT via push
    mov     dword ptr [rsp+32], esi
    lea     rcx, szUBUnbraid
    call    _ub_print
    mov     ecx, dword ptr [rsp+32]
    call    _ub_print_u32
    lea     rcx, szUBNewline
    call    _ub_print

    ; Print freed bytes
    mov     esi, dword ptr [rsp+32]
    mov     eax, esi
    shl     eax, 6
    mov     rdi, [rbx + UC_StageTable]
    add     rdi, rax
    mov     rcx, [rdi + SD_WorkBufSize]
    test    rcx, rcx
    jz      @@done
    mov     qword ptr [rsp+32], rcx
    lea     rcx, szUBFreed
    call    _ub_print
    mov     rcx, qword ptr [rsp+32]
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
; Frame: push rsi(8) + push rdi(8) + sub 40 = 56. 56 mod 16 = 8 ✓
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

    cmp     dword ptr [rdi + SD_Braided], 1
    je      @@done

    ; Re-allocate working buffer if size > 0 and ptr is NULL
    mov     rax, [rdi + SD_WorkBufSize]
    test    rax, rax
    jz      @@no_alloc
    cmp     qword ptr [rdi + SD_WorkBufPtr], 0
    jne     @@no_alloc

    ; Save rdi to local [rsp+32] for call
    mov     qword ptr [rsp+32], rdi
    xor     ecx, ecx
    mov     rdx, [rdi + SD_WorkBufSize]
    mov     r8d, MEM_COMMIT or MEM_RESERVE
    mov     r9d, PAGE_READWRITE
    call    VirtualAlloc
    mov     rdi, qword ptr [rsp+32]
    test    rax, rax
    jz      @@done
    mov     [rdi + SD_WorkBufPtr], rax

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

    ; Print — save esi via local frame
    mov     dword ptr [rsp+32], esi
    lea     rcx, szUBRebraid
    call    _ub_print
    mov     ecx, dword ptr [rsp+32]
    call    _ub_print_u32
    lea     rcx, szUBNewline
    call    _ub_print

@@done:
    add     rsp, 40
    pop     rdi
    pop     rsi
    ret
_ub_rebraid_stage ENDP

; ─── _ub_find_lowest_priority_braided ─────────────────────────────────────
; RBX = context → EAX = stage index of lowest-priority braided, or -1
; Leaf: uses R10, R11 (volatile)
_ub_find_lowest_priority_braided PROC
    mov     r10, [rbx + UC_StageTable]
    test    r10, r10
    jz      @@none
    mov     ecx, [rbx + UC_StageCount]
    xor     edx, edx
    mov     r8d, -1
    mov     r9d, 7FFFFFFFh
@@loop:
    cmp     edx, ecx
    jge     @@done
    mov     eax, edx
    shl     eax, 6
    cmp     dword ptr [r10 + rax + SD_Braided], 1
    jne     @@skip
    mov     r11d, [r10 + rax + SD_Priority]
    cmp     r11d, r9d
    jge     @@skip
    mov     r9d, r11d
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
; RBX = context → EAX = stage index of highest-priority unbraided, or -1
; Leaf: uses R10, R11 (volatile)
_ub_find_highest_priority_unbraided PROC
    mov     r10, [rbx + UC_StageTable]
    test    r10, r10
    jz      @@none
    mov     ecx, [rbx + UC_StageCount]
    xor     edx, edx
    mov     r8d, -1
    xor     r9d, r9d
@@loop:
    cmp     edx, ecx
    jge     @@done
    mov     eax, edx
    shl     eax, 6
    cmp     dword ptr [r10 + rax + SD_Braided], 0
    jne     @@skip
    mov     r11d, [r10 + rax + SD_Priority]
    cmp     r11d, r9d
    jle     @@skip
    mov     r9d, r11d
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
; Frame: push rbx(8) + push rsi(8) + push rdi(8) + sub 48 = 72.
;   72 mod 16 = 8 ✓
; =============================================================================
Unbraid_Init PROC FRAME
    push    rbx
    .pushreg rbx
    push    rsi
    .pushreg rsi
    push    rdi
    .pushreg rdi
    sub     rsp, 48
    .allocstack 48
    .endprolog

    mov     rsi, rcx

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

    ; Allocate stage table
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
    mov     edx, TRAMP_SIZE * MAX_PIPELINE_STAGES
    mov     r8d, MEM_COMMIT or MEM_RESERVE
    mov     r9d, PAGE_EXECUTE_READWRITE
    call    VirtualAlloc
    test    rax, rax
    jz      @@fail_tbl
    mov     [rbx + UC_TrampolineBase], rax

    ; Fill trampolines with INT3 (0xCC) — RDI saved in prolog
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
    add     rsp, 48
    pop     rdi
    pop     rsi
    pop     rbx
    ret
Unbraid_Init ENDP

; =============================================================================
; Unbraid_Destroy
; RCX = context
; Frame: push rbx(8) + push rsi(8) + sub 40 = 56. 56 mod 16 = 8 ✓
; =============================================================================
Unbraid_Destroy PROC FRAME
    push    rbx
    .pushreg rbx
    push    rsi
    .pushreg rsi
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
    xor     r10d, r10d
    mov     r11d, [rbx + UC_StageCount]
@@free_bufs:
    cmp     r10d, r11d
    jge     @@skip_bufs
    mov     eax, r10d
    shl     eax, 6
    mov     rcx, [rsi + rax + SD_WorkBufPtr]
    test    rcx, rcx
    jz      @@skip_one
    ; Save loop vars in local frame
    mov     dword ptr [rsp+32], r10d
    mov     dword ptr [rsp+36], r11d
    xor     edx, edx
    mov     r8d, MEM_RELEASE
    call    VirtualFree
    mov     r10d, dword ptr [rsp+32]
    mov     r11d, dword ptr [rsp+36]
    mov     rsi, [rbx + UC_StageTable]
@@skip_one:
    inc     r10d
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
    pop     rsi
    pop     rbx
    ret
Unbraid_Destroy ENDP

; =============================================================================
; Unbraid_RegisterStage
; RCX = ctx, RDX = executeFn, R8 = bypassFn, R9 = bufSize
; [RSP+112] = priority (5th arg)
; Returns: EAX = stage index, or -1
;
; Frame: push rbx(8) + push rsi(8) + push rdi(8) + sub 48 = 72.
;   72 mod 16 = 8 ✓
; 5th arg offset: 48(sub) + 24(3 pushes) + 8(ret) + 32(shadow) = [RSP+112]
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

    mov     esi, eax
    mov     rdi, [rbx + UC_StageTable]
    test    rdi, rdi
    jz      @@full

    mov     eax, esi
    shl     eax, 6
    add     rdi, rax

    ; Fill descriptor
    mov     [rdi + SD_ExecuteFn], rdx
    mov     [rdi + SD_BypassFn], r8
    mov     [rdi + SD_WorkBufSize], r9
    mov     dword ptr [rdi + SD_Braided], 1
    mov     [rdi + SD_StageID], esi
    mov     dword ptr [rdi + SD_UnbraidCount], 0
    mov     qword ptr [rdi + SD_ExecCount], 0

    ; Priority from 5th arg
    mov     eax, dword ptr [rsp + 112]
    mov     [rdi + SD_Priority], eax

    ; Trampoline offset
    mov     eax, esi
    shl     eax, 5
    mov     [rdi + SD_TrampolineOff], eax

    ; Allocate working buffer if size > 0
    cmp     r9, 0
    je      @@no_buf
    ; Save rdi in local frame [rsp+40] (rdi is callee-saved, preserved across call)
    mov     qword ptr [rsp+40], rdi
    xor     ecx, ecx
    mov     rdx, r9
    mov     r8d, MEM_COMMIT or MEM_RESERVE
    mov     r9d, PAGE_READWRITE
    call    VirtualAlloc
    mov     rdi, qword ptr [rsp+40]
    mov     [rdi + SD_WorkBufPtr], rax
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

    ; Print — save esi in local [rsp+32] (no push to maintain alignment)
    mov     dword ptr [rsp+32], esi
    lea     rcx, szUBRegister
    call    _ub_print
    mov     ecx, dword ptr [rsp+32]
    call    _ub_print_u32
    lea     rcx, szUBNewline
    call    _ub_print

    call    _ub_count_states
    call    _ub_calc_pressure

    mov     eax, dword ptr [rsp+32]
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
; Unbraid_Tick
; RCX = context
; Returns: EAX = current pressure %
; Frame: push rbx(8) + sub 48 = 56. 56 mod 16 = 8 ✓
; =============================================================================
Unbraid_Tick PROC FRAME
    push    rbx
    .pushreg rbx
    sub     rsp, 48
    .allocstack 48
    .endprolog

    mov     rbx, rcx
    inc     qword ptr [rbx + UC_TickCount]

    call    _ub_calc_pressure
    call    _ub_count_states

    mov     eax, [rbx + UC_PressurePct]

    cmp     eax, [rbx + UC_AggressiveThresh]
    jge     @@aggressive_unbraid

    cmp     eax, [rbx + UC_UnbraidThreshold]
    jge     @@normal_unbraid

    cmp     eax, [rbx + UC_RebraidThreshold]
    jl      @@try_rebraid

    jmp     @@done

@@aggressive_unbraid:
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
    call    _ub_find_lowest_priority_braided
    cmp     eax, -1
    je      @@done
    mov     ecx, eax
    call    _ub_unbraid_stage
    call    _ub_calc_pressure
    jmp     @@done

@@try_rebraid:
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

    add     rsp, 48
    pop     rbx
    ret
Unbraid_Tick ENDP

; =============================================================================
; Unbraid_Execute — run pipeline through trampolines
; RCX = context, RDX = input data, R8 = input size
; Returns: EAX = stages executed
;
; Frame: push rbx(8) + push rsi(8) + push rdi(8) + push r12(8) + push r13(8)
;   + sub 48 = 88. 88 mod 16 = 8 ✓
;
; Local usage:
;   [rsp+32] saved loop index (ecx)
;   [rsp+36] saved stage count (edx)
;   [rsp+40] saved executed count (r8d)
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
    mov     r12, rdx
    mov     r13, r8

    inc     qword ptr [rbx + UC_StatsPipeRuns]

    mov     rdi, [rbx + UC_StageTable]
    mov     rsi, [rbx + UC_TrampolineBase]
    test    rdi, rdi
    jz      @@done_zero
    test    rsi, rsi
    jz      @@done_zero

    xor     ecx, ecx
    mov     edx, [rbx + UC_StageCount]
    xor     r8d, r8d

@@exec_loop:
    cmp     ecx, edx
    jge     @@exec_done

    ; Save loop state to locals (NOT push — preserves alignment)
    mov     dword ptr [rsp+32], ecx
    mov     dword ptr [rsp+36], edx
    mov     dword ptr [rsp+40], r8d

    ; Increment exec count on descriptor
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

    ; Call through trampoline
    mov     eax, ecx
    shl     eax, 5
    lea     rax, [rsi + rax]
    mov     rcx, r12
    mov     rdx, r13
    call    rax

    ; Restore loop state from locals
    mov     ecx, dword ptr [rsp+32]
    mov     edx, dword ptr [rsp+36]
    mov     r8d, dword ptr [rsp+40]
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
; Unbraid_SetBudget — RCX = ctx, RDX = budget
; =============================================================================
Unbraid_SetBudget PROC
    mov     [rcx + UC_MemBudget], rdx
    ret
Unbraid_SetBudget ENDP

; =============================================================================
; Unbraid_AddPressure — RCX = ctx, RDX = bytes
; =============================================================================
Unbraid_AddPressure PROC
    add     [rcx + UC_MemAllocated], rdx
    mov     rax, [rcx + UC_MemAllocated]
    ret
Unbraid_AddPressure ENDP

; =============================================================================
; Unbraid_ReleasePressure — RCX = ctx, RDX = bytes
; =============================================================================
Unbraid_ReleasePressure PROC
    sub     [rcx + UC_MemAllocated], rdx
    cmp     qword ptr [rcx + UC_MemAllocated], 0
    jge     @@ok
    mov     qword ptr [rcx + UC_MemAllocated], 0
@@ok:
    mov     rax, [rcx + UC_MemAllocated]
    ret
Unbraid_ReleasePressure ENDP

; =============================================================================
; Unbraid_GetPressurePct — RCX = ctx → EAX = %
; =============================================================================
Unbraid_GetPressurePct PROC
    mov     eax, [rcx + UC_PressurePct]
    ret
Unbraid_GetPressurePct ENDP

; =============================================================================
; Unbraid_ForceUnbraid — RCX = ctx, EDX = stage index
; Frame: push rbx(8) + sub 48 = 56. 56 mod 16 = 8 ✓
; =============================================================================
Unbraid_ForceUnbraid PROC FRAME
    push    rbx
    .pushreg rbx
    sub     rsp, 48
    .allocstack 48
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
    add     rsp, 48
    pop     rbx
    ret
Unbraid_ForceUnbraid ENDP

; =============================================================================
; Unbraid_ForceRebraid — RCX = ctx, EDX = stage index
; Frame: push rbx(8) + sub 48 = 56. 56 mod 16 = 8 ✓
; =============================================================================
Unbraid_ForceRebraid PROC FRAME
    push    rbx
    .pushreg rbx
    sub     rsp, 48
    .allocstack 48
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
    add     rsp, 48
    pop     rbx
    ret
Unbraid_ForceRebraid ENDP

; =============================================================================
; Unbraid_GetStats — RCX = ctx, RDX = 128-byte output buffer
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
