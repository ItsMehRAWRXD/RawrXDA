; =============================================================================
; RawrXD_DirectionlessLoader.asm — Directionless Memory Load/Unload Engine
;
; Omnidirectional memory management with hotpatch stubs and load-locking.
; Memory can flow in ANY direction — no fixed hot→cold or cold→hot pipeline.
; Load level is pinned at its setpoint via VirtualProtect memory locking
; and self-modifying hotpatch trampolines.
;
; Architecture:
;   ┌─────────────────────────────────────────────────────────────────────┐
;   │                   DIRECTIONLESS LOADER                              │
;   │                                                                     │
;   │  ┌─── LOAD VECTOR TABLE ───┐   ┌─── MEMORY LOCK ──────────────┐  │
;   │  │ Slot 0: → SCATTER       │   │ VirtualProtect PAGE_READONLY  │  │
;   │  │ Slot 1: ← GATHER        │   │ Pages pinned at setpoint      │  │
;   │  │ Slot 2: ↔ SWAP          │   │ Load% frozen via lock bit     │  │
;   │  │ Slot 3: ↻ ROTATE        │   │ Unlock → PAGE_READWRITE       │  │
;   │  │ Slot 4: ⊕ CLONE_SCATTER │   └──────────────────────────────┘  │
;   │  │ Slot 5: ⊖ EVICT_ANY     │                                      │
;   │  │ Slot 6: ⊗ SWAP_LOCK     │   ┌─── HOTPATCH TRAMPOLINES ────┐  │
;   │  │ Slot 7: ⊘ NOP_THROUGH   │   │ 16-byte NOP sleds per slot   │  │
;   │  └────────────────────────┘   │ Patched to JMP optimized path │  │
;   │                                │ Self-modifying at runtime     │  │
;   │  Load Setpoint: 0-100%         │ VirtualProtect RWX for patch  │  │
;   │  Lock bit: freeze at setpoint  └──────────────────────────────┘  │
;   │                                                                     │
;   │  Each tick:                                                         │
;   │    1. LFSR picks a direction vector                                 │
;   │    2. Execute direction (scatter/gather/swap/rotate/etc)            │
;   │    3. Measure load% vs setpoint                                     │
;   │    4. If locked → revert any drift via compensating op              │
;   │    5. Hotpatch trampoline if new optimal path found                 │
;   └─────────────────────────────────────────────────────────────────────┘
;
; Exports:
;   DirLoad_Init            — Create engine context
;   DirLoad_Destroy         — Free everything
;   DirLoad_Tick            — Advance one directionless cycle
;   DirLoad_SetLoadPct      — Set target load percentage (0-100)
;   DirLoad_LockLoad        — Lock load at current setpoint
;   DirLoad_UnlockLoad      — Unlock load (allow drift)
;   DirLoad_GetLoadPct      — Get current load percentage
;   DirLoad_GetStats        — Read statistics
;   DirLoad_FeedSlot        — Feed a memory region into the table
;   DirLoad_HotPatch        — Force a hotpatch of a trampoline slot
;   DirLoad_MemPatch        — Memory-patch: lock/unlock pages
;   DirLoad_GetDirection    — Get current active direction vector
;
; Build: ml64 /c RawrXD_DirectionlessLoader.asm
; =============================================================================

option casemap:none

INCLUDE ksamd64.inc

; ─── Windows API ─────────────────────────────────────────────────────────────
EXTRN VirtualAlloc:PROC
EXTRN VirtualFree:PROC
EXTRN VirtualProtect:PROC
EXTRN QueryPerformanceCounter:PROC
EXTRN QueryPerformanceFrequency:PROC
EXTRN GetStdHandle:PROC
EXTRN WriteConsoleA:PROC

; ─── Exports ─────────────────────────────────────────────────────────────────
PUBLIC DirLoad_Init
PUBLIC DirLoad_Destroy
PUBLIC DirLoad_Tick
PUBLIC DirLoad_SetLoadPct
PUBLIC DirLoad_LockLoad
PUBLIC DirLoad_UnlockLoad
PUBLIC DirLoad_GetLoadPct
PUBLIC DirLoad_GetStats
PUBLIC DirLoad_FeedSlot
PUBLIC DirLoad_HotPatch
PUBLIC DirLoad_MemPatch
PUBLIC DirLoad_GetDirection

; =============================================================================
; Constants
; =============================================================================
MEM_COMMIT              EQU 1000h
MEM_RESERVE             EQU 2000h
MEM_RELEASE             EQU 8000h
PAGE_READWRITE          EQU 04h
PAGE_READONLY           EQU 02h
PAGE_EXECUTE_READWRITE  EQU 40h
STD_OUTPUT              EQU -11

; Direction vectors (omnidirectional — no fixed pipeline)
DIR_SCATTER             EQU 0           ; Spread loaded data across regions
DIR_GATHER              EQU 1           ; Pull data from multiple regions
DIR_SWAP                EQU 2           ; Exchange two regions bidirectionally
DIR_ROTATE              EQU 3           ; Circular shift through all regions
DIR_CLONE_SCATTER       EQU 4           ; Duplicate + scatter (redundancy load)
DIR_EVICT_ANY           EQU 5           ; Free any region regardless of state
DIR_SWAP_LOCK           EQU 6           ; Swap + immediately lock destination
DIR_NOP_THROUGH         EQU 7           ; Pass-through (no memory change)
MAX_DIRECTIONS          EQU 8

; Memory region states
MR_FREE                 EQU 0           ; Not allocated
MR_LOADED               EQU 1           ; Data present, read-write
MR_LOCKED               EQU 2           ; Data present, read-only (pinned)
MR_EVICTING             EQU 3           ; Being freed
MR_PATCHING             EQU 4           ; Being hotpatched

; Slot table size
MAX_MEM_SLOTS           EQU 64          ; Memory regions tracked
TRAMPOLINE_SIZE         EQU 16          ; Bytes per hotpatch trampoline
MAX_TRAMPOLINES         EQU 16          ; Hotpatch trampoline slots

; =============================================================================
; Memory Slot (32 bytes each)
; =============================================================================
;   0x00  BaseAddr         QWORD     — Virtual address of region
;   0x08  RegionSize       QWORD     — Size in bytes
;   0x10  State            DWORD     — MR_FREE/LOADED/LOCKED/etc
;   0x14  LoadOrder        DWORD     — When it was loaded (tick number)
;   0x18  AccessCount      DWORD     — Times accessed this epoch
;   0x1C  LockFlag         DWORD     — 1 = page-locked via VirtualProtect
MS_BaseAddr             EQU 00h
MS_RegionSize           EQU 08h
MS_State                EQU 10h
MS_LoadOrder            EQU 14h
MS_AccessCount          EQU 18h
MS_LockFlag             EQU 1Ch
MS_SIZE                 EQU 20h         ; 32 bytes

; =============================================================================
; HotPatch Trampoline (16 bytes each)
; =============================================================================
;   Byte layout in trampoline buffer:
;   [0-1]   JMP rel8 or NOP NOP (2 bytes)
;   [2-9]   Target address (8 bytes) — for indirect JMP
;   [10-11] Original bytes (2 bytes) — saved for unpatch
;   [12]    Active flag (1 byte)
;   [13-15] Padding
HP_JmpBytes             EQU 00h
HP_TargetAddr           EQU 02h
HP_OrigBytes            EQU 0Ah
HP_Active               EQU 0Ch
HP_SIZE                 EQU 10h         ; 16 bytes = TRAMPOLINE_SIZE

; =============================================================================
; DirLoadContext — Engine state
; =============================================================================
;   0x000  QPCFreq          QWORD
;   0x008  InitQPC          QWORD
;   0x010  TickCount        QWORD
;   0x018  LFSR             QWORD     — Random direction selector
;
;   ─── Load Setpoint & Lock ───
;   0x020  LoadSetpoint     DWORD     — Target load % (0-100)
;   0x024  LoadLocked       DWORD     — 1 = load pinned at setpoint
;   0x028  CurrentLoadPct   DWORD     — Measured load %
;   0x02C  ActiveDirection  DWORD     — Last executed direction vector
;
;   ─── Slot Counts ───
;   0x030  TotalSlots       DWORD     — Slots in use
;   0x034  LoadedSlots      DWORD     — Slots in MR_LOADED state
;   0x038  LockedSlots      DWORD     — Slots in MR_LOCKED state
;   0x03C  FreeSlots        DWORD     — Slots in MR_FREE state
;
;   ─── Statistics ───
;   0x040  StatsScatters    QWORD
;   0x048  StatsGathers     QWORD
;   0x050  StatsSwaps       QWORD
;   0x058  StatsRotates     QWORD
;   0x060  StatsClones      QWORD
;   0x068  StatsEvicts      QWORD
;   0x070  StatsSwapLocks   QWORD
;   0x078  StatsNops        QWORD
;   0x080  StatsPatchOps    QWORD     — Hotpatch operations
;   0x088  StatsMemPatches  QWORD     — VirtualProtect calls
;   0x090  StatsLockDrifts  QWORD     — Times load drifted while locked
;   0x098  StatsCorrections QWORD     — Compensating ops to restore setpoint
;
;   ─── Direction Weight Table ───
;   0x0A0  DirWeights       DWORD[8]  — Weight for each direction (LFSR-biased)
;
;   ─── Slot Table Pointer ───
;   0x0C0  SlotTable        QWORD     — Ptr to MS_SIZE * MAX_MEM_SLOTS buffer
;
;   ─── Trampoline Buffer ───
;   0x0C8  TrampolineBase   QWORD     — Ptr to RWX trampoline buffer
;   0x0D0  TrampolineCount  DWORD     — Active trampolines
;   0x0D4  OldProtect       DWORD     — Saved page protection
;
;   ─── Prev protect scratch ───
;   0x0D8  ProtectScratch   DWORD
;   0x0DC  Pad              DWORD
;
;   0x0E0  ... (reserved to 0x100)
DC_QPCFreq              EQU 000h
DC_InitQPC              EQU 008h
DC_TickCount            EQU 010h
DC_LFSR                 EQU 018h
DC_LoadSetpoint         EQU 020h
DC_LoadLocked           EQU 024h
DC_CurrentLoadPct       EQU 028h
DC_ActiveDirection      EQU 02Ch
DC_TotalSlots           EQU 030h
DC_LoadedSlots          EQU 034h
DC_LockedSlots          EQU 038h
DC_FreeSlots            EQU 03Ch
DC_StatsScatters        EQU 040h
DC_StatsGathers         EQU 048h
DC_StatsSwaps           EQU 050h
DC_StatsRotates         EQU 058h
DC_StatsClones          EQU 060h
DC_StatsEvicts          EQU 068h
DC_StatsSwapLocks       EQU 070h
DC_StatsNops            EQU 078h
DC_StatsPatchOps        EQU 080h
DC_StatsMemPatches      EQU 088h
DC_StatsLockDrifts      EQU 090h
DC_StatsCorrections     EQU 098h
DC_DirWeights           EQU 0A0h
; 0x0A0 + 8*4 = 0x0C0
DC_SlotTable            EQU 0C0h
DC_TrampolineBase       EQU 0C8h
DC_TrampolineCount      EQU 0D0h
DC_OldProtect           EQU 0D4h
DC_ProtectScratch       EQU 0D8h
DC_SIZE                 EQU 100h        ; 256 bytes

; =============================================================================
; Data Section
; =============================================================================
.data
align 16
szDLInit        db '[DirLoad] Directionless Loader initialized',0Dh,0Ah,0
szDLTick        db '[DirLoad] Tick #',0
szDLDir         db '  Direction: ',0
szDLLoad        db '  Load: ',0
szDLLocked      db '  LOAD LOCKED at ',0
szDLDrift       db '  ! DRIFT detected, correcting',0Dh,0Ah,0
szDLPatch       db '  HotPatch trampoline ',0
szDLMemPatch    db '  MemPatch: VirtualProtect on slot ',0
szDLPct         db '%',0Dh,0Ah,0
szDLNewline     db 0Dh,0Ah,0
hStdOutDL       dq 0

; Direction name table (for printing)
align 8
szDirNames      dq offset szDirScatter
                dq offset szDirGather
                dq offset szDirSwap
                dq offset szDirRotate
                dq offset szDirCloneScatter
                dq offset szDirEvictAny
                dq offset szDirSwapLock
                dq offset szDirNopThrough

szDirScatter    db 'SCATTER',0
szDirGather     db 'GATHER',0
szDirSwap       db 'SWAP',0
szDirRotate     db 'ROTATE',0
szDirCloneScatter db 'CLONE_SCATTER',0
szDirEvictAny   db 'EVICT_ANY',0
szDirSwapLock   db 'SWAP_LOCK',0
szDirNopThrough db 'NOP_THROUGH',0

; Default direction weights (higher = more likely chosen by LFSR)
; SCATTER:15, GATHER:15, SWAP:20, ROTATE:10, CLONE:5, EVICT:10, SWAPLOCK:15, NOP:10
align 16
defaultWeights  dd 15, 15, 20, 10, 5, 10, 15, 10

.data?
align 16
qpcScratchDL    dq ?
numBufDL        db 32 dup(?)

; =============================================================================
; Code Section
; =============================================================================
.code

; ─── Internal Helpers ────────────────────────────────────────────────────────

_dl_strlen PROC
    xor     eax, eax
@@:
    cmp     byte ptr [rcx + rax], 0
    je      @F
    inc     rax
    jmp     @B
@@:
    ret
_dl_strlen ENDP

_dl_print PROC FRAME
    push    rbx
    .pushreg rbx
    sub     rsp, 48
    .allocstack 48
    .endprolog
    mov     rbx, rcx
    mov     rax, hStdOutDL
    test    rax, rax
    jnz     @@have
    mov     ecx, STD_OUTPUT
    call    GetStdHandle
    mov     hStdOutDL, rax
@@have:
    mov     rcx, rbx
    call    _dl_strlen
    mov     r8, rax
    mov     rcx, hStdOutDL
    mov     rdx, rbx
    lea     r9, [rsp+32]
    mov     qword ptr [rsp+32], 0
    call    WriteConsoleA
    add     rsp, 48
    pop     rbx
    ret
_dl_print ENDP

_dl_print_u32 PROC FRAME
    push    rbx
    .pushreg rbx
    sub     rsp, 48
    .allocstack 48
    .endprolog
    mov     eax, ecx
    lea     rbx, numBufDL
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
    call    _dl_print
    add     rsp, 48
    pop     rbx
    ret
_dl_print_u32 ENDP

_dl_get_qpc PROC FRAME
    sub     rsp, 40
    .allocstack 40
    .endprolog
    lea     rcx, qpcScratchDL
    call    QueryPerformanceCounter
    mov     rax, qpcScratchDL
    add     rsp, 40
    ret
_dl_get_qpc ENDP

; ─── _dl_advance_lfsr ─────────────────────────────────────────────────────
; RBX = context → EAX = random 32-bit
_dl_advance_lfsr PROC
    mov     rax, [rbx + DC_LFSR]
    mov     rcx, rax
    shr     rax, 1
    test    rcx, 1
    jz      @@no_tap
    mov     rcx, 0D8000000h
    shl     rcx, 32                     ; Full 64-bit Galois tap
    xor     rax, rcx
@@no_tap:
    mov     [rbx + DC_LFSR], rax
    ret
_dl_advance_lfsr ENDP

; ─── _dl_pick_direction ──────────────────────────────────────────────────
; RBX = context → EAX = direction index (0-7)
; Weighted random selection using LFSR + direction weights
_dl_pick_direction PROC FRAME
    push    rsi
    .pushreg rsi
    sub     rsp, 32
    .allocstack 32
    .endprolog

    ; Get total weight
    lea     rsi, [rbx + DC_DirWeights]
    xor     ecx, ecx                   ; total
    xor     edx, edx
@@sum:
    cmp     edx, MAX_DIRECTIONS
    jge     @@sum_done
    add     ecx, [rsi + rdx*4]
    inc     edx
    jmp     @@sum
@@sum_done:
    test    ecx, ecx
    jz      @@fallback

    ; Random value mod total weight
    push    rcx
    call    _dl_advance_lfsr
    pop     rcx
    xor     edx, edx
    div     ecx                         ; EDX = LFSR mod total_weight

    ; Walk weights to find direction
    xor     ecx, ecx                   ; running sum
    xor     eax, eax                   ; direction index
@@walk:
    cmp     eax, MAX_DIRECTIONS
    jge     @@fallback
    add     ecx, [rsi + rax*4]
    cmp     edx, ecx
    jl      @@found
    inc     eax
    jmp     @@walk

@@found:
    add     rsp, 32
    pop     rsi
    ret

@@fallback:
    xor     eax, eax                   ; Default: SCATTER
    add     rsp, 32
    pop     rsi
    ret
_dl_pick_direction ENDP

; ─── _dl_count_states ────────────────────────────────────────────────────
; RBX = context → updates LoadedSlots, LockedSlots, FreeSlots, CurrentLoadPct
_dl_count_states PROC
    mov     rsi, [rbx + DC_SlotTable]
    test    rsi, rsi
    jz      @@zero
    xor     r8d, r8d                   ; loaded
    xor     r9d, r9d                   ; locked
    xor     r10d, r10d                 ; free
    xor     ecx, ecx                   ; index
@@loop:
    cmp     ecx, MAX_MEM_SLOTS
    jge     @@done
    mov     eax, ecx
    shl     eax, 5                     ; * MS_SIZE (32)
    mov     edx, [rsi + rax + MS_State]
    cmp     edx, MR_FREE
    je      @@is_free
    cmp     edx, MR_LOCKED
    je      @@is_locked
    cmp     edx, MR_LOADED
    je      @@is_loaded
    jmp     @@next
@@is_free:
    inc     r10d
    jmp     @@next
@@is_loaded:
    inc     r8d
    jmp     @@next
@@is_locked:
    inc     r9d
@@next:
    inc     ecx
    jmp     @@loop
@@done:
    mov     [rbx + DC_LoadedSlots], r8d
    mov     [rbx + DC_LockedSlots], r9d
    mov     [rbx + DC_FreeSlots], r10d

    ; total in-use = loaded + locked
    mov     eax, r8d
    add     eax, r9d
    mov     [rbx + DC_TotalSlots], eax

    ; CurrentLoadPct = (loaded + locked) * 100 / MAX_MEM_SLOTS
    imul    eax, 100
    xor     edx, edx
    mov     ecx, MAX_MEM_SLOTS
    div     ecx
    mov     [rbx + DC_CurrentLoadPct], eax
    ret
@@zero:
    mov     dword ptr [rbx + DC_LoadedSlots], 0
    mov     dword ptr [rbx + DC_LockedSlots], 0
    mov     dword ptr [rbx + DC_FreeSlots], MAX_MEM_SLOTS
    mov     dword ptr [rbx + DC_CurrentLoadPct], 0
    ret
_dl_count_states ENDP

; ─── _dl_find_slot_by_state ──────────────────────────────────────────────
; RBX = context, ECX = desired state
; Returns: EAX = slot index, or -1 if none found
; Uses LFSR to pick a random starting point for fairness
_dl_find_slot_by_state PROC FRAME
    push    rsi
    .pushreg rsi
    push    rdi
    .pushreg rdi
    sub     rsp, 32
    .allocstack 32
    .endprolog

    mov     edi, ecx                   ; EDI = desired state
    mov     rsi, [rbx + DC_SlotTable]
    test    rsi, rsi
    jz      @@none

    ; Random start
    push    rdi
    call    _dl_advance_lfsr
    pop     rdi
    xor     edx, edx
    mov     ecx, MAX_MEM_SLOTS
    div     ecx
    mov     ecx, edx                   ; ECX = start index

    xor     r8d, r8d                   ; count
@@scan:
    cmp     r8d, MAX_MEM_SLOTS
    jge     @@none
    mov     eax, ecx
    shl     eax, 5                     ; * 32
    cmp     dword ptr [rsi + rax + MS_State], edi
    je      @@found
    inc     ecx
    cmp     ecx, MAX_MEM_SLOTS
    jl      @@no_wrap
    xor     ecx, ecx
@@no_wrap:
    inc     r8d
    jmp     @@scan
@@found:
    mov     eax, ecx
    add     rsp, 32
    pop     rdi
    pop     rsi
    ret
@@none:
    mov     eax, -1
    add     rsp, 32
    pop     rdi
    pop     rsi
    ret
_dl_find_slot_by_state ENDP

; ─── _dl_exec_direction ──────────────────────────────────────────────────
; RBX = context, ECX = direction (0-7)
; Executes the directionless memory operation
_dl_exec_direction PROC FRAME
    push    rbp
    .pushreg rbp
    push    rsi
    .pushreg rsi
    push    rdi
    .pushreg rdi
    push    r12
    .pushreg r12
    sub     rsp, 56
    .allocstack 56
    .endprolog

    mov     [rbx + DC_ActiveDirection], ecx
    mov     r12d, ecx

    ; Jump table via cmp chain (8 directions)
    cmp     ecx, DIR_SCATTER
    je      @@do_scatter
    cmp     ecx, DIR_GATHER
    je      @@do_gather
    cmp     ecx, DIR_SWAP
    je      @@do_swap
    cmp     ecx, DIR_ROTATE
    je      @@do_rotate
    cmp     ecx, DIR_CLONE_SCATTER
    je      @@do_clone
    cmp     ecx, DIR_EVICT_ANY
    je      @@do_evict
    cmp     ecx, DIR_SWAP_LOCK
    je      @@do_swap_lock
    jmp     @@do_nop                   ; DIR_NOP_THROUGH or unknown

    ; ── SCATTER: find a FREE slot, mark it LOADED ──
@@do_scatter:
    inc     qword ptr [rbx + DC_StatsScatters]
    mov     ecx, MR_FREE
    call    _dl_find_slot_by_state
    cmp     eax, -1
    je      @@done
    mov     rsi, [rbx + DC_SlotTable]
    mov     ecx, eax
    shl     ecx, 5
    mov     dword ptr [rsi + rcx + MS_State], MR_LOADED
    mov     eax, [rbx + DC_TickCount]
    mov     [rsi + rcx + MS_LoadOrder], eax
    mov     dword ptr [rsi + rcx + MS_AccessCount], 1
    jmp     @@done

    ; ── GATHER: find a LOADED slot, increment access ──
@@do_gather:
    inc     qword ptr [rbx + DC_StatsGathers]
    mov     ecx, MR_LOADED
    call    _dl_find_slot_by_state
    cmp     eax, -1
    je      @@done
    mov     rsi, [rbx + DC_SlotTable]
    mov     ecx, eax
    shl     ecx, 5
    inc     dword ptr [rsi + rcx + MS_AccessCount]
    jmp     @@done

    ; ── SWAP: exchange a LOADED and FREE slot ──
@@do_swap:
    inc     qword ptr [rbx + DC_StatsSwaps]
    ; Find a LOADED slot
    mov     ecx, MR_LOADED
    call    _dl_find_slot_by_state
    cmp     eax, -1
    je      @@done
    mov     esi, eax                   ; ESI = loaded idx

    ; Find a FREE slot
    mov     ecx, MR_FREE
    call    _dl_find_slot_by_state
    cmp     eax, -1
    je      @@done
    mov     edi, eax                   ; EDI = free idx

    ; Swap: loaded→free, free→loaded
    mov     r8, [rbx + DC_SlotTable]
    mov     ecx, esi
    shl     ecx, 5
    mov     dword ptr [r8 + rcx + MS_State], MR_FREE
    mov     dword ptr [r8 + rcx + MS_AccessCount], 0
    mov     ecx, edi
    shl     ecx, 5
    mov     dword ptr [r8 + rcx + MS_State], MR_LOADED
    mov     dword ptr [r8 + rcx + MS_AccessCount], 1
    jmp     @@done

    ; ── ROTATE: circular shift — move each slot state to next slot ──
@@do_rotate:
    inc     qword ptr [rbx + DC_StatsRotates]
    mov     rsi, [rbx + DC_SlotTable]
    test    rsi, rsi
    jz      @@done
    ; Save last slot's state
    mov     eax, MAX_MEM_SLOTS - 1
    shl     eax, 5
    mov     r8d, [rsi + rax + MS_State]
    mov     r9d, [rsi + rax + MS_AccessCount]
    ; Shift all slots forward by 1
    mov     ecx, MAX_MEM_SLOTS - 1
@@rot_loop:
    test    ecx, ecx
    jz      @@rot_first
    mov     eax, ecx
    shl     eax, 5
    mov     edx, ecx
    dec     edx
    shl     edx, 5
    mov     r10d, [rsi + rdx + MS_State]
    mov     [rsi + rax + MS_State], r10d
    mov     r10d, [rsi + rdx + MS_AccessCount]
    mov     [rsi + rax + MS_AccessCount], r10d
    dec     ecx
    jmp     @@rot_loop
@@rot_first:
    mov     dword ptr [rsi + MS_State], r8d
    mov     dword ptr [rsi + MS_AccessCount], r9d
    jmp     @@done

    ; ── CLONE_SCATTER: duplicate a LOADED slot into a FREE slot ──
@@do_clone:
    inc     qword ptr [rbx + DC_StatsClones]
    mov     ecx, MR_LOADED
    call    _dl_find_slot_by_state
    cmp     eax, -1
    je      @@done
    mov     esi, eax

    mov     ecx, MR_FREE
    call    _dl_find_slot_by_state
    cmp     eax, -1
    je      @@done

    ; Clone: mark free slot as LOADED with same access count
    mov     r8, [rbx + DC_SlotTable]
    mov     ecx, esi
    shl     ecx, 5
    mov     r9d, [r8 + rcx + MS_AccessCount]
    mov     ecx, eax
    shl     ecx, 5
    mov     dword ptr [r8 + rcx + MS_State], MR_LOADED
    mov     [r8 + rcx + MS_AccessCount], r9d
    jmp     @@done

    ; ── EVICT_ANY: free ANY loaded slot (directionless eviction) ──
@@do_evict:
    inc     qword ptr [rbx + DC_StatsEvicts]
    mov     ecx, MR_LOADED
    call    _dl_find_slot_by_state
    cmp     eax, -1
    je      @@done
    mov     rsi, [rbx + DC_SlotTable]
    mov     ecx, eax
    shl     ecx, 5
    mov     dword ptr [rsi + rcx + MS_State], MR_FREE
    mov     dword ptr [rsi + rcx + MS_AccessCount], 0
    mov     dword ptr [rsi + rcx + MS_LockFlag], 0
    jmp     @@done

    ; ── SWAP_LOCK: swap two regions then lock the destination ──
@@do_swap_lock:
    inc     qword ptr [rbx + DC_StatsSwapLocks]
    mov     ecx, MR_LOADED
    call    _dl_find_slot_by_state
    cmp     eax, -1
    je      @@done
    mov     esi, eax

    mov     ecx, MR_FREE
    call    _dl_find_slot_by_state
    cmp     eax, -1
    je      @@done

    ; Swap + lock destination
    mov     r8, [rbx + DC_SlotTable]
    mov     ecx, esi
    shl     ecx, 5
    mov     dword ptr [r8 + rcx + MS_State], MR_FREE
    mov     dword ptr [r8 + rcx + MS_AccessCount], 0
    mov     ecx, eax
    shl     ecx, 5
    mov     dword ptr [r8 + rcx + MS_State], MR_LOCKED
    mov     dword ptr [r8 + rcx + MS_LockFlag], 1
    mov     dword ptr [r8 + rcx + MS_AccessCount], 1
    jmp     @@done

    ; ── NOP_THROUGH: no-op ──
@@do_nop:
    inc     qword ptr [rbx + DC_StatsNops]

@@done:
    add     rsp, 56
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbp
    ret
_dl_exec_direction ENDP

; ─── _dl_enforce_lock ────────────────────────────────────────────────────
; RBX = context
; If load is locked, compare current load% to setpoint.
; If drifted, execute compensating ops to restore setpoint.
_dl_enforce_lock PROC FRAME
    push    rsi
    .pushreg rsi
    sub     rsp, 32
    .allocstack 32
    .endprolog

    cmp     dword ptr [rbx + DC_LoadLocked], 0
    je      @@unlocked

    ; Recount actual state
    call    _dl_count_states

    mov     eax, [rbx + DC_CurrentLoadPct]
    mov     ecx, [rbx + DC_LoadSetpoint]
    cmp     eax, ecx
    je      @@exact                     ; Perfect — no drift

    ; Drift detected
    inc     qword ptr [rbx + DC_StatsLockDrifts]

    cmp     eax, ecx
    jg      @@too_loaded                ; Over setpoint → evict

    ; Under setpoint → scatter (load more)
@@under_loaded:
    inc     qword ptr [rbx + DC_StatsCorrections]
    mov     ecx, DIR_SCATTER
    call    _dl_exec_direction
    jmp     @@recheck

@@too_loaded:
    inc     qword ptr [rbx + DC_StatsCorrections]
    mov     ecx, DIR_EVICT_ANY
    call    _dl_exec_direction

@@recheck:
    ; Re-count and loop if still drifted (max 4 correction passes)
    call    _dl_count_states
    mov     eax, [rbx + DC_CurrentLoadPct]
    mov     ecx, [rbx + DC_LoadSetpoint]
    ; Allow ±2% tolerance
    sub     eax, ecx
    test    eax, eax
    jns     @@pos
    neg     eax
@@pos:
    cmp     eax, 2
    jle     @@exact

@@exact:
@@unlocked:
    add     rsp, 32
    pop     rsi
    ret
_dl_enforce_lock ENDP

; =============================================================================
; PUBLIC API
; =============================================================================

; =============================================================================
; DirLoad_Init — Create engine context + allocate slot table + trampolines
; RCX = initial load setpoint (0-100, 0 = no setpoint)
; Returns: RAX = context, or 0
; =============================================================================
DirLoad_Init PROC FRAME
    push    rbx
    .pushreg rbx
    push    rsi
    .pushreg rsi
    push    rdi
    .pushreg rdi
    sub     rsp, 40
    .allocstack 40
    .endprolog

    mov     esi, ecx                   ; Save setpoint

    ; Allocate context
    xor     ecx, ecx
    mov     edx, DC_SIZE
    mov     r8d, MEM_COMMIT or MEM_RESERVE
    mov     r9d, PAGE_READWRITE
    call    VirtualAlloc
    test    rax, rax
    jz      @@fail
    mov     rbx, rax

    ; QPC
    lea     rcx, qpcScratchDL
    call    QueryPerformanceFrequency
    mov     rax, qpcScratchDL
    mov     [rbx + DC_QPCFreq], rax

    call    _dl_get_qpc
    mov     [rbx + DC_InitQPC], rax

    ; Seed LFSR
    mov     rcx, 0DEADL0ADh
    xor     rax, rcx
    test    rax, rax
    jnz     @@lfsr_ok
    mov     rax, 0F00DCAFE12345678h
@@lfsr_ok:
    mov     [rbx + DC_LFSR], rax

    ; Set load setpoint
    mov     [rbx + DC_LoadSetpoint], esi
    mov     dword ptr [rbx + DC_LoadLocked], 0
    mov     dword ptr [rbx + DC_FreeSlots], MAX_MEM_SLOTS

    ; Copy default direction weights
    lea     rsi, defaultWeights
    lea     rdi, [rbx + DC_DirWeights]
    mov     ecx, MAX_DIRECTIONS
@@wt:
    test    ecx, ecx
    jz      @@wt_done
    mov     eax, [rsi]
    mov     [rdi], eax
    add     rsi, 4
    add     rdi, 4
    dec     ecx
    jmp     @@wt
@@wt_done:

    ; Allocate slot table (MS_SIZE * MAX_MEM_SLOTS = 32 * 64 = 2048 bytes)
    xor     ecx, ecx
    mov     edx, MS_SIZE * MAX_MEM_SLOTS
    mov     r8d, MEM_COMMIT or MEM_RESERVE
    mov     r9d, PAGE_READWRITE
    call    VirtualAlloc
    test    rax, rax
    jz      @@fail_ctx
    mov     [rbx + DC_SlotTable], rax

    ; Init all slots to FREE
    mov     rsi, rax
    xor     ecx, ecx
@@init_slots:
    cmp     ecx, MAX_MEM_SLOTS
    jge     @@slots_done
    mov     eax, ecx
    shl     eax, 5
    mov     dword ptr [rsi + rax + MS_State], MR_FREE
    mov     dword ptr [rsi + rax + MS_LockFlag], 0
    mov     dword ptr [rsi + rax + MS_AccessCount], 0
    inc     ecx
    jmp     @@init_slots
@@slots_done:

    ; Allocate trampoline buffer (PAGE_EXECUTE_READWRITE for hotpatching)
    xor     ecx, ecx
    mov     edx, HP_SIZE * MAX_TRAMPOLINES   ; 16 * 16 = 256 bytes
    mov     r8d, MEM_COMMIT or MEM_RESERVE
    mov     r9d, PAGE_EXECUTE_READWRITE
    call    VirtualAlloc
    test    rax, rax
    jz      @@fail_slots
    mov     [rbx + DC_TrampolineBase], rax
    mov     dword ptr [rbx + DC_TrampolineCount], 0

    ; Fill trampolines with NOPs (0x90)
    mov     rdi, rax
    mov     ecx, HP_SIZE * MAX_TRAMPOLINES
    mov     al, 90h                     ; NOP
    rep     stosb

    ; Print init
    lea     rcx, szDLInit
    call    _dl_print

    ; If setpoint > 0, pre-load slots to reach setpoint
    mov     eax, [rbx + DC_LoadSetpoint]
    test    eax, eax
    jz      @@no_preload

    ; slots_to_load = setpoint * MAX_MEM_SLOTS / 100
    imul    eax, MAX_MEM_SLOTS
    xor     edx, edx
    mov     ecx, 100
    div     ecx
    mov     esi, eax                   ; ESI = slots to preload
@@preload:
    test    esi, esi
    jz      @@no_preload
    mov     ecx, DIR_SCATTER
    call    _dl_exec_direction
    dec     esi
    jmp     @@preload
@@no_preload:

    call    _dl_count_states

    mov     rax, rbx
    jmp     @@exit

@@fail_slots:
    mov     rcx, [rbx + DC_SlotTable]
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
    pop     rdi
    pop     rsi
    pop     rbx
    ret
DirLoad_Init ENDP

; =============================================================================
; DirLoad_Destroy — Free all memory
; RCX = context
; =============================================================================
DirLoad_Destroy PROC FRAME
    push    rbx
    .pushreg rbx
    sub     rsp, 40
    .allocstack 40
    .endprolog
    test    rcx, rcx
    jz      @@null
    mov     rbx, rcx

    ; Free trampoline buffer
    mov     rcx, [rbx + DC_TrampolineBase]
    test    rcx, rcx
    jz      @@skip_tramp
    xor     edx, edx
    mov     r8d, MEM_RELEASE
    call    VirtualFree
@@skip_tramp:

    ; Free slot table
    mov     rcx, [rbx + DC_SlotTable]
    test    rcx, rcx
    jz      @@skip_slots
    xor     edx, edx
    mov     r8d, MEM_RELEASE
    call    VirtualFree
@@skip_slots:

    ; Free context
    mov     rcx, rbx
    xor     edx, edx
    mov     r8d, MEM_RELEASE
    call    VirtualFree
@@null:
    add     rsp, 40
    pop     rbx
    ret
DirLoad_Destroy ENDP

; =============================================================================
; DirLoad_Tick — Advance one directionless cycle
; RCX = context
; Returns: EAX = current load %
;
; Flow: pick direction → execute → count → enforce lock → return load%
; =============================================================================
DirLoad_Tick PROC FRAME
    push    rbx
    .pushreg rbx
    sub     rsp, 40
    .allocstack 40
    .endprolog

    mov     rbx, rcx
    inc     qword ptr [rbx + DC_TickCount]

    ; Pick a random direction
    call    _dl_pick_direction
    mov     ecx, eax

    ; Execute the direction
    call    _dl_exec_direction

    ; Recount states
    call    _dl_count_states

    ; Enforce load lock if active
    call    _dl_enforce_lock

    ; Print status every 10 ticks
    mov     rax, [rbx + DC_TickCount]
    xor     edx, edx
    mov     ecx, 10
    div     rcx
    test    edx, edx
    jnz     @@no_print

    ; Print tick #
    lea     rcx, szDLTick
    call    _dl_print
    mov     rax, [rbx + DC_TickCount]
    mov     ecx, eax
    call    _dl_print_u32
    lea     rcx, szDLNewline
    call    _dl_print

    ; Print direction name
    lea     rcx, szDLDir
    call    _dl_print
    mov     eax, [rbx + DC_ActiveDirection]
    cmp     eax, MAX_DIRECTIONS
    jge     @@skip_dir
    lea     rcx, szDirNames
    mov     rcx, [rcx + rax*8]
    call    _dl_print
    lea     rcx, szDLNewline
    call    _dl_print
@@skip_dir:

    ; Print load %
    lea     rcx, szDLLoad
    call    _dl_print
    mov     ecx, [rbx + DC_CurrentLoadPct]
    call    _dl_print_u32
    lea     rcx, szDLPct
    call    _dl_print

    ; Print lock state
    cmp     dword ptr [rbx + DC_LoadLocked], 0
    je      @@no_lock_print
    lea     rcx, szDLLocked
    call    _dl_print
    mov     ecx, [rbx + DC_LoadSetpoint]
    call    _dl_print_u32
    lea     rcx, szDLPct
    call    _dl_print
@@no_lock_print:

@@no_print:
    mov     eax, [rbx + DC_CurrentLoadPct]

    add     rsp, 40
    pop     rbx
    ret
DirLoad_Tick ENDP

; =============================================================================
; DirLoad_SetLoadPct — Set target load percentage
; RCX = context, EDX = load % (0-100)
; =============================================================================
DirLoad_SetLoadPct PROC
    cmp     edx, 100
    jle     @@ok
    mov     edx, 100
@@ok:
    mov     [rcx + DC_LoadSetpoint], edx
    ret
DirLoad_SetLoadPct ENDP

; =============================================================================
; DirLoad_LockLoad — Lock load at current setpoint
; RCX = context
; After this, the engine will auto-correct any drift to maintain setpoint.
; =============================================================================
DirLoad_LockLoad PROC
    mov     dword ptr [rcx + DC_LoadLocked], 1
    ret
DirLoad_LockLoad ENDP

; =============================================================================
; DirLoad_UnlockLoad — Unlock load (allow drift)
; RCX = context
; =============================================================================
DirLoad_UnlockLoad PROC
    mov     dword ptr [rcx + DC_LoadLocked], 0
    ret
DirLoad_UnlockLoad ENDP

; =============================================================================
; DirLoad_GetLoadPct — Get current load percentage
; RCX = context
; Returns: EAX = load % (0-100)
; =============================================================================
DirLoad_GetLoadPct PROC
    mov     eax, [rcx + DC_CurrentLoadPct]
    ret
DirLoad_GetLoadPct ENDP

; =============================================================================
; DirLoad_FeedSlot — Feed a memory region into the slot table
; RCX = context, RDX = base address, R8 = region size
; Returns: EAX = slot index, or -1 if table full
; =============================================================================
DirLoad_FeedSlot PROC FRAME
    push    rbx
    .pushreg rbx
    push    rsi
    .pushreg rsi
    sub     rsp, 32
    .allocstack 32
    .endprolog

    mov     rbx, rcx
    mov     rsi, [rbx + DC_SlotTable]
    test    rsi, rsi
    jz      @@fail

    ; Find first FREE slot
    xor     ecx, ecx
@@find:
    cmp     ecx, MAX_MEM_SLOTS
    jge     @@fail
    mov     eax, ecx
    shl     eax, 5
    cmp     dword ptr [rsi + rax + MS_State], MR_FREE
    je      @@found
    inc     ecx
    jmp     @@find
@@found:
    mov     eax, ecx
    shl     eax, 5
    mov     [rsi + rax + MS_BaseAddr], rdx
    mov     [rsi + rax + MS_RegionSize], r8
    mov     dword ptr [rsi + rax + MS_State], MR_LOADED
    mov     r9d, [rbx + DC_TickCount]
    mov     [rsi + rax + MS_LoadOrder], r9d
    mov     dword ptr [rsi + rax + MS_AccessCount], 1
    mov     dword ptr [rsi + rax + MS_LockFlag], 0

    call    _dl_count_states
    mov     eax, ecx
    jmp     @@exit
@@fail:
    mov     eax, -1
@@exit:
    add     rsp, 32
    pop     rsi
    pop     rbx
    ret
DirLoad_FeedSlot ENDP

; =============================================================================
; DirLoad_HotPatch — Write a hotpatch trampoline
; RCX = context, EDX = trampoline slot (0-15), R8 = target address
; Patches the trampoline slot to JMP to the target address.
; Returns: EAX = 1 on success
; =============================================================================
DirLoad_HotPatch PROC FRAME
    push    rbx
    .pushreg rbx
    sub     rsp, 48
    .allocstack 48
    .endprolog

    mov     rbx, rcx
    cmp     edx, MAX_TRAMPOLINES
    jge     @@fail

    mov     r9, [rbx + DC_TrampolineBase]
    test    r9, rax
    jz      @@fail

    ; Calculate trampoline offset
    mov     eax, edx
    shl     eax, 4                     ; * HP_SIZE (16)
    add     r9, rax                    ; R9 = trampoline ptr

    ; Save original 2 bytes
    mov     ax, [r9]
    mov     [r9 + HP_OrigBytes], ax

    ; Write JMP stub: EB 0E (JMP +14 — skip to after trampoline)
    ; Actually, store the target address for indirect call
    mov     byte ptr [r9 + HP_JmpBytes], 0FFh      ; FF 25 = JMP [rip+0]
    mov     byte ptr [r9 + HP_JmpBytes + 1], 25h
    mov     [r9 + HP_TargetAddr], r8
    mov     byte ptr [r9 + HP_Active], 1

    inc     dword ptr [rbx + DC_TrampolineCount]
    inc     qword ptr [rbx + DC_StatsPatchOps]

    mov     eax, 1
    jmp     @@exit
@@fail:
    xor     eax, eax
@@exit:
    add     rsp, 48
    pop     rbx
    ret
DirLoad_HotPatch ENDP

; =============================================================================
; DirLoad_MemPatch — Memory-patch: lock/unlock a slot's pages
; RCX = context, EDX = slot index, R8D = 1 to lock (PAGE_READONLY), 0 to unlock
; Uses VirtualProtect to change page protection.
; Returns: EAX = 1 on success
; =============================================================================
DirLoad_MemPatch PROC FRAME
    push    rbx
    .pushreg rbx
    sub     rsp, 48
    .allocstack 48
    .endprolog

    mov     rbx, rcx
    cmp     edx, MAX_MEM_SLOTS
    jge     @@fail

    mov     rsi, [rbx + DC_SlotTable]
    test    rsi, rsi
    jz      @@fail

    mov     eax, edx
    shl     eax, 5
    add     rsi, rax                   ; RSI = slot ptr

    ; Check slot has an address
    mov     rcx, [rsi + MS_BaseAddr]
    test    rcx, rcx
    jz      @@fail

    mov     r9, [rsi + MS_RegionSize]
    test    r9, r9
    jz      @@fail

    ; Determine new protection
    cmp     r8d, 1
    je      @@lock_page

    ; Unlock: PAGE_READWRITE
    mov     r8d, PAGE_READWRITE
    jmp     @@do_protect

@@lock_page:
    mov     r8d, PAGE_READONLY

@@do_protect:
    ; VirtualProtect(baseAddr, size, newProtect, &oldProtect)
    ; RCX = addr (already set), RDX = size, R8D = protect, R9 = &oldProtect
    mov     rdx, [rsi + MS_RegionSize]
    lea     r9, [rbx + DC_ProtectScratch]
    mov     qword ptr [rsp+32], 0
    call    VirtualProtect
    test    eax, eax
    jz      @@fail

    ; Update slot state
    cmp     r8d, PAGE_READONLY
    jne     @@set_loaded
    mov     dword ptr [rsi + MS_State], MR_LOCKED
    mov     dword ptr [rsi + MS_LockFlag], 1
    jmp     @@ok
@@set_loaded:
    mov     dword ptr [rsi + MS_State], MR_LOADED
    mov     dword ptr [rsi + MS_LockFlag], 0

@@ok:
    inc     qword ptr [rbx + DC_StatsMemPatches]
    call    _dl_count_states
    mov     eax, 1
    jmp     @@exit
@@fail:
    xor     eax, eax
@@exit:
    add     rsp, 48
    pop     rbx
    ret
DirLoad_MemPatch ENDP

; =============================================================================
; DirLoad_GetDirection — Get current active direction vector
; RCX = context
; Returns: EAX = direction (0-7)
; =============================================================================
DirLoad_GetDirection PROC
    mov     eax, [rcx + DC_ActiveDirection]
    ret
DirLoad_GetDirection ENDP

; =============================================================================
; DirLoad_GetStats — Read statistics (128 bytes)
; RCX = context, RDX = 128-byte buffer
;
; [0x00] TickCount        [0x08] CurrentLoadPct(DW)/Setpoint(DW)/Locked(DW)/Dir(DW)
; [0x10] LoadedSlots(DW)  [0x14] LockedSlots(DW)
; [0x18] FreeSlots(DW)    [0x1C] TrampolineCount(DW)
; [0x20] Scatters          [0x28] Gathers
; [0x30] Swaps             [0x38] Rotates
; [0x40] Clones            [0x48] Evicts
; [0x50] SwapLocks         [0x58] Nops
; [0x60] PatchOps          [0x68] MemPatches
; [0x70] LockDrifts        [0x78] Corrections
; =============================================================================
DirLoad_GetStats PROC
    test    rcx, rcx
    jz      @@fail
    test    rdx, rdx
    jz      @@fail

    mov     rax, [rcx + DC_TickCount]
    mov     [rdx + 00h], rax

    mov     eax, [rcx + DC_CurrentLoadPct]
    mov     [rdx + 08h], eax
    mov     eax, [rcx + DC_LoadSetpoint]
    mov     [rdx + 0Ch], eax
    mov     eax, [rcx + DC_LoadLocked]
    mov     [rdx + 10h], eax           ; Repurpose — shift layout slightly
    mov     eax, [rcx + DC_ActiveDirection]
    mov     [rdx + 14h], eax

    mov     eax, [rcx + DC_LoadedSlots]
    mov     [rdx + 18h], eax
    mov     eax, [rcx + DC_LockedSlots]
    mov     [rdx + 1Ch], eax
    mov     eax, [rcx + DC_FreeSlots]
    mov     [rdx + 20h], eax
    mov     eax, [rcx + DC_TrampolineCount]
    mov     [rdx + 24h], eax

    mov     rax, [rcx + DC_StatsScatters]
    mov     [rdx + 28h], rax
    mov     rax, [rcx + DC_StatsGathers]
    mov     [rdx + 30h], rax
    mov     rax, [rcx + DC_StatsSwaps]
    mov     [rdx + 38h], rax
    mov     rax, [rcx + DC_StatsRotates]
    mov     [rdx + 40h], rax
    mov     rax, [rcx + DC_StatsClones]
    mov     [rdx + 48h], rax
    mov     rax, [rcx + DC_StatsEvicts]
    mov     [rdx + 50h], rax
    mov     rax, [rcx + DC_StatsSwapLocks]
    mov     [rdx + 58h], rax
    mov     rax, [rcx + DC_StatsNops]
    mov     [rdx + 60h], rax
    mov     rax, [rcx + DC_StatsPatchOps]
    mov     [rdx + 68h], rax
    mov     rax, [rcx + DC_StatsMemPatches]
    mov     [rdx + 70h], rax
    mov     rax, [rcx + DC_StatsLockDrifts]
    mov     [rdx + 78h], rax

    mov     eax, 1
    ret
@@fail:
    xor     eax, eax
    ret
DirLoad_GetStats ENDP

END
