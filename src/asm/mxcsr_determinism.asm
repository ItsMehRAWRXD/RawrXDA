; =============================================================================
; mxcsr_determinism.asm — IEEE-754 / Performance MXCSR Wrappers for Zen4
; =============================================================================
; Locks MXCSR state before inference kernels to guarantee deterministic FP
; behavior across cores and prevent Windows scheduler from leaving stale
; rounding/flush modes after context switches.
;
; EXPORTS:
;   RawrXD_MXCSR_LockDeterministic()  → Sets IEEE-754 strict mode
;   RawrXD_MXCSR_LockPerformance()    → Sets FTZ+DAZ for max throughput
;   RawrXD_MXCSR_Save(pSlot:rcx)      → Snapshots current MXCSR to [rcx]
;   RawrXD_MXCSR_Restore(pSlot:rcx)   → Restores MXCSR from [rcx]
;   RawrXD_MXCSR_GetMode()            → EAX=0 (unknown), 1 (strict), 2 (perf)
;
; MXCSR Layout (bits):
;   [0]  IE   Invalid Operation flag
;   [1]  DE   Denormal flag
;   [2]  ZE   Divide-by-zero flag
;   [3]  OE   Overflow flag
;   [4]  UE   Underflow flag
;   [5]  PE   Precision flag
;   [6]  DAZ  Denormals-Are-Zeros         ← performance
;   [7]  IM   Invalid Operation mask
;   [8]  DM   Denormal mask
;   [9]  ZM   Divide-by-zero mask
;   [10] OM   Overflow mask
;   [11] UM   Underflow mask
;   [12] PM   Precision mask
;   [13-14] RC Rounding Control (00=nearest)
;   [15] FTZ  Flush-To-Zero               ← performance
;
; Build: ml64.exe /c /Zi /Zd mxcsr_determinism.asm
; =============================================================================
OPTION CASEMAP:NONE

INCLUDE rawr_globals.inc

; MXCSR bitmasks
MXCSR_FTZ          EQU 08000h           ; bit 15: Flush-To-Zero
MXCSR_DAZ          EQU 00040h           ; bit  6: Denormals-Are-Zeros
MXCSR_RC_MASK      EQU 06000h           ; bits 13-14: Rounding Control
MXCSR_RC_NEAREST   EQU 00000h           ; 00 = Round to Nearest Even
MXCSR_EXCEPTION_MASK EQU 01F80h         ; bits 7-12: all exception masks
MXCSR_FLAG_CLEAR   EQU 0FFC0h           ; clear all status flags [0:5]

; Mode tracking (per-thread would be BETTER but global is fine for single-
; inference-thread designs — the UMS scheduler guarantees affinity anyway)
.DATA
ALIGN 8
g_MxcsrMode     DWORD   0              ; 0=unknown, 1=strict, 2=perf
g_MxcsrSaveSlot DWORD   0              ; internal save for bracketed calls
g_MxcsrCallCount QWORD  0              ; telemetry

.CODE

; =============================================================================
; void RawrXD_MXCSR_LockDeterministic(void)
;
; IEEE-754 strict: no FTZ, no DAZ, round-to-nearest, all exceptions masked.
; Use BEFORE consensus voting, swarm aggregation, or any path where bit-exact
; reproducibility across Zen4 CCDs matters.
; =============================================================================
PUBLIC RawrXD_MXCSR_LockDeterministic
RawrXD_MXCSR_LockDeterministic PROC
    ; Read current MXCSR
    sub     rsp, 8
    stmxcsr [rsp]
    mov     eax, [rsp]

    ; Clear FTZ (bit 15) and DAZ (bit 6) for IEEE-754 strict
    and     eax, NOT (MXCSR_FTZ OR MXCSR_DAZ)

    ; Force round-to-nearest (clear bits 13-14, set 00)
    and     eax, NOT MXCSR_RC_MASK
    or      eax, MXCSR_RC_NEAREST

    ; Mask all FP exceptions (bits 7-12) to prevent #XF on denormals
    or      eax, MXCSR_EXCEPTION_MASK

    ; Clear sticky status flags (bits 0-5)
    and     eax, MXCSR_FLAG_CLEAR

    ; Write back
    mov     [rsp], eax
    ldmxcsr [rsp]
    add     rsp, 8

    ; Track mode
    mov     DWORD PTR [g_MxcsrMode], 1
    lock inc QWORD PTR [g_MxcsrCallCount]
    ret
RawrXD_MXCSR_LockDeterministic ENDP

; =============================================================================
; void RawrXD_MXCSR_LockPerformance(void)
;
; FTZ+DAZ enabled, round-to-nearest, all exceptions masked.
; Use BEFORE GEMM/GEMV hot loops where denormal stalls cost 100x cycles.
; Zen4 FP pipeline runs 4x faster with FTZ on denormal-heavy quantized weights.
; =============================================================================
PUBLIC RawrXD_MXCSR_LockPerformance
RawrXD_MXCSR_LockPerformance PROC
    sub     rsp, 8
    stmxcsr [rsp]
    mov     eax, [rsp]

    ; Enable FTZ (bit 15) and DAZ (bit 6) for throughput
    or      eax, MXCSR_FTZ OR MXCSR_DAZ

    ; Round-to-nearest (safest for inference)
    and     eax, NOT MXCSR_RC_MASK
    or      eax, MXCSR_RC_NEAREST

    ; Mask all FP exceptions
    or      eax, MXCSR_EXCEPTION_MASK

    ; Clear sticky flags
    and     eax, MXCSR_FLAG_CLEAR

    mov     [rsp], eax
    ldmxcsr [rsp]
    add     rsp, 8

    mov     DWORD PTR [g_MxcsrMode], 2
    lock inc QWORD PTR [g_MxcsrCallCount]
    ret
RawrXD_MXCSR_LockPerformance ENDP

; =============================================================================
; void RawrXD_MXCSR_Save(DWORD* pSlot)
;   rcx = pointer to caller's DWORD save slot (must be 4-byte aligned)
;
; Snapshot current MXCSR so caller can bracket a kernel with
; Save → LockPerformance → kernel → Restore.
; =============================================================================
PUBLIC RawrXD_MXCSR_Save
RawrXD_MXCSR_Save PROC
    test    rcx, rcx
    jz      @@bail
    stmxcsr [rcx]
@@bail:
    ret
RawrXD_MXCSR_Save ENDP

; =============================================================================
; void RawrXD_MXCSR_Restore(const DWORD* pSlot)
;   rcx = pointer to caller's saved MXCSR value
;
; Restores MXCSR from snapshot. Use after kernel returns.
; =============================================================================
PUBLIC RawrXD_MXCSR_Restore
RawrXD_MXCSR_Restore PROC
    test    rcx, rcx
    jz      @@bail
    ldmxcsr [rcx]
    mov     DWORD PTR [g_MxcsrMode], 0      ; mode unknown after restore
@@bail:
    ret
RawrXD_MXCSR_Restore ENDP

; =============================================================================
; UINT32 RawrXD_MXCSR_GetMode(void)
;   Returns: 0=unknown, 1=strict (deterministic), 2=performance (FTZ+DAZ)
; =============================================================================
PUBLIC RawrXD_MXCSR_GetMode
RawrXD_MXCSR_GetMode PROC
    mov     eax, DWORD PTR [g_MxcsrMode]
    ret
RawrXD_MXCSR_GetMode ENDP

END
