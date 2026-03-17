; =============================================================================
; RawrXD_SlolorisStreamLoader.asm — Slowloris/DoS-Pattern Tensor Cycling Engine
;
; Strategy:  Gradually increase model strength by progressive tensor saturation.
;   1. DRIP PHASE   — Trickle-load tensors one-at-a-time (Slowloris keepalive)
;   2. BURST PHASE  — Load-3 / Unload-1 circular eviction
;   3. SPOOF PHASE  — Ghost descriptors occupy slots, real data swapped in lazily
;   4. ORBIT PHASE  — Full ring cycles: load→infer→evict→reload, strength grows
;
; Ring Architecture:
;   ┌───────────────────────────────────────────────────┐
;   │  Slot 0  │  Slot 1  │  Slot 2  │  ...  │ Slot N  │
;   │ [GHOST]  │ [LIVE]   │ [LIVE]   │       │ [DRIP]  │
;   └──────┬───┴────┬─────┴────┬─────┴───────┴────┬────┘
;          │        │          │                   │
;          ▼ evict  ▼ infer    ▼ infer             ▼ loading
;       reclaim   strongest   strongest          trickle-in
;
; Exports:
;   Sloloris_Init          — Create engine context
;   Sloloris_Destroy       — Free everything
;   Sloloris_AttachModel   — Bind to a GGUF file handle + tensor index
;   Sloloris_Tick          — Advance one cycle (call from inference loop)
;   Sloloris_GetTensor     — Fetch tensor ptr (may trigger on-demand load)
;   Sloloris_GetStrength   — Return 0-100 model saturation level
;   Sloloris_ForceOrbit    — Force a full ring orbit (DoS burst pattern)
;   Sloloris_SetStrategy   — Switch between DRIP / BURST / ORBIT strategy
;
; Build: ml64 /c RawrXD_SlolorisStreamLoader.asm
; =============================================================================

option casemap:none

INCLUDE ksamd64.inc

; Windows API
EXTRN VirtualAlloc:PROC
EXTRN VirtualFree:PROC
EXTRN MapViewOfFile:PROC
EXTRN UnmapViewOfFile:PROC
EXTRN CreateFileMappingA:PROC
EXTRN CloseHandle:PROC
EXTRN QueryPerformanceCounter:PROC
EXTRN QueryPerformanceFrequency:PROC
EXTRN GetStdHandle:PROC
EXTRN WriteConsoleA:PROC
EXTRN Sleep:PROC

; Exports
PUBLIC Sloloris_Init
PUBLIC Sloloris_Destroy
PUBLIC Sloloris_AttachModel
PUBLIC Sloloris_Tick
PUBLIC Sloloris_GetTensor
PUBLIC Sloloris_GetStrength
PUBLIC Sloloris_ForceOrbit
PUBLIC Sloloris_SetStrategy

; =============================================================================
; Constants
; =============================================================================

; Ring slot count — power-of-2 for fast modulo
RING_SLOTS              EQU 32
RING_MASK               EQU 31          ; RING_SLOTS - 1

; Maximum tensor descriptors tracked
MAX_TENSORS             EQU 4096

; Slot states
SLOT_EMPTY              EQU 0
SLOT_GHOST              EQU 1           ; Descriptor present, no data (spoof)
SLOT_DRIP               EQU 2           ; Partially loaded (Slowloris trickle)
SLOT_LIVE               EQU 3           ; Fully loaded and usable
SLOT_EVICTING           EQU 4           ; Being evicted (transitional)

; Strategy modes
STRATEGY_DRIP           EQU 0           ; Slow keepalive trickle
STRATEGY_BURST          EQU 1           ; Load-3, Unload-1
STRATEGY_ORBIT          EQU 2           ; Full ring cycling

; Load-3 / Unload-1 pattern
BURST_LOAD_COUNT        EQU 3
BURST_EVICT_COUNT       EQU 1

; Drip timing
DRIP_DELAY_MS           EQU 10          ; Milliseconds between trickle loads
DRIP_CHUNK_SIZE         EQU 65536       ; 64KB per drip chunk

; Windows constants
MEM_COMMIT              EQU 1000h
MEM_RESERVE             EQU 2000h
MEM_RELEASE             EQU 8000h
PAGE_READWRITE          EQU 04h
FILE_MAP_READ           EQU 04h
PAGE_READONLY           EQU 02h
STD_OUTPUT              EQU -11

; =============================================================================
; Structures (Manual offset calculation)
; =============================================================================

; RingSlot (128 bytes) — one slot in the circular buffer
;   0x00  TensorID        DWORD     — Index into model tensor table
;   0x04  State           DWORD     — SLOT_EMPTY/GHOST/DRIP/LIVE/EVICTING
;   0x08  DataPtr         QWORD     — Mapped data pointer (or NULL for ghost)
;   0x10  DataSize        QWORD     — Tensor size in bytes
;   0x18  DripOffset      QWORD     — How many bytes loaded so far (for trickle)
;   0x20  FileOffset      QWORD     — Offset into GGUF file
;   0x28  AccessCount     DWORD     — Times this tensor was accessed
;   0x2C  OrbitCount      DWORD     — Times this slot cycled through the ring
;   0x30  Timestamp       QWORD     — QPC tick when last touched
;   0x38  SpoofHash       QWORD     — Lightweight hash for ghost validation
;   0x40  Reserved        QWORD[8]  — Padding to 128 bytes
RS_TensorID             EQU 00h
RS_State                EQU 04h
RS_DataPtr              EQU 08h
RS_DataSize             EQU 10h
RS_DripOffset           EQU 18h
RS_FileOffset           EQU 20h
RS_AccessCount          EQU 28h
RS_OrbitCount           EQU 2Ch
RS_Timestamp            EQU 30h
RS_SpoofHash            EQU 38h
RS_SIZE                 EQU 80h         ; 128 bytes

; SlolorisContext (main engine state)
;   0x000  hFile           QWORD     — GGUF file handle
;   0x008  hMapping        QWORD     — CreateFileMapping handle
;   0x010  FileSize        QWORD     — Total file size
;   0x018  TensorCount     DWORD     — Number of tensors in model
;   0x01C  Strategy        DWORD     — STRATEGY_DRIP/BURST/ORBIT
;   0x020  RingHead        DWORD     — Next slot to load into
;   0x024  RingTail        DWORD     — Next slot to evict from
;   0x028  LiveCount       DWORD     — Currently LIVE tensors in ring
;   0x02C  GhostCount      DWORD     — Currently GHOST tensors in ring
;   0x030  TotalLoaded     QWORD     — Total bytes loaded (for strength calc)
;   0x038  TotalModelSize  QWORD     — Total model size (for strength calc)
;   0x040  TickCount       QWORD     — Number of Tick() calls
;   0x048  OrbitCount      QWORD     — Full ring orbits completed
;   0x050  BurstPhase      DWORD     — Within burst: 0-2=loading, 3=evicting
;   0x054  DripActive      DWORD     — Current drip tensor (or -1)
;   0x058  QPCFreq         QWORD     — QPC frequency for timing
;   0x060  LastTickQPC     QWORD     — QPC of last Tick()
;   0x068  StrengthPct     DWORD     — Cached 0-100 model strength
;   0x06C  Pad0            DWORD
;   0x070  TensorIndex     QWORD     — Ptr to tensor offset table
;   0x078  TensorSizes     QWORD     — Ptr to tensor size table
;   0x080  RingBase        QWORD     — Ptr to ring slot array (RING_SLOTS * RS_SIZE)
;   0x088  StatsLoads      QWORD     — Total load operations
;   0x090  StatsEvicts     QWORD     — Total eviction operations
;   0x098  StatsGhosts     QWORD     — Total ghost installs
;   0x0A0  StatsOrbits     QWORD     — Total orbit completions
;   0x0A8  NextTensorToLoad DWORD    — Round-robin tensor loader cursor
;   0x0AC  Pad1            DWORD
;   0x0B0  Reserved        QWORD[10] — Future use
CTX_hFile               EQU 000h
CTX_hMapping            EQU 008h
CTX_FileSize            EQU 010h
CTX_TensorCount         EQU 018h
CTX_Strategy            EQU 01Ch
CTX_RingHead            EQU 020h
CTX_RingTail            EQU 024h
CTX_LiveCount           EQU 028h
CTX_GhostCount          EQU 02Ch
CTX_TotalLoaded         EQU 030h
CTX_TotalModelSize      EQU 038h
CTX_TickCount           EQU 040h
CTX_OrbitCount          EQU 048h
CTX_BurstPhase          EQU 050h
CTX_DripActive          EQU 054h
CTX_QPCFreq             EQU 058h
CTX_LastTickQPC         EQU 060h
CTX_StrengthPct         EQU 068h
CTX_TensorIndex         EQU 070h
CTX_TensorSizes         EQU 078h
CTX_RingBase            EQU 080h
CTX_StatsLoads          EQU 088h
CTX_StatsEvicts         EQU 090h
CTX_StatsGhosts         EQU 098h
CTX_StatsOrbits         EQU 0A0h
CTX_NextTensorToLoad    EQU 0A8h
CTX_SIZE                EQU 100h        ; 256 bytes

; =============================================================================
; Data Section
; =============================================================================
.data
align 16
szSloInit       db '[Sloloris] Engine initialized — ',0
szSloDrip       db '[Sloloris] DRIP: tensor ',0
szSloBurst      db '[Sloloris] BURST: load-3 evict-1 cycle ',0
szSloOrbit      db '[Sloloris] ORBIT: full ring rotation ',0
szSloGhost      db '[Sloloris] GHOST: spoofing slot ',0
szSloStrength   db '[Sloloris] Strength: ',0
szSloPct        db '%',0Dh,0Ah,0
szNewline       db 0Dh,0Ah,0
hStdOut         dq 0

; Chaos Randomizer Tables
; Operator dispatch: 0=mul, 1=div, 2=add, 3=sub
align 16
chaosOpTable    db 0, 1, 0, 3, 2, 0, 1, 3, 2, 0
; Digit pool: 1-9 and 10 (representing -0)
align 16
chaosDigits     dd 1, 2, 3, 4, 5, 6, 7, 8, 9, 10
; LFSR seed
align 8
chaosLFSR       dq 0DEAD88BEh
chaosTickIdx    dd 0

.data?
align 16
qpcScratch      dq ?
numBufSlo       db 32 dup(?)

; =============================================================================
; Code Section
; =============================================================================
.code

; =============================================================================
; Internal helpers
; =============================================================================

; _slo_strlen: RCX → RAX (string length)
_slo_strlen PROC
    xor     rax, rax
@@loop:
    cmp     byte ptr [rcx + rax], 0
    je      @@done
    inc     rax
    jmp     @@loop
@@done:
    ret
_slo_strlen ENDP

; _slo_print: RCX = string pointer, prints to stdout
_slo_print PROC FRAME
    push    rbx
    .pushreg rbx
    push    rsi
    .pushreg rsi
    sub     rsp, 48
    .allocstack 48
    .endprolog

    mov     rsi, rcx
    call    _slo_strlen
    mov     r8, rax             ; length
    mov     rdx, rsi            ; buffer
    mov     rcx, hStdOut
    xor     r9, r9
    mov     qword ptr [rsp+32], 0
    call    WriteConsoleA

    add     rsp, 48
    pop     rsi
    pop     rbx
    ret
_slo_print ENDP

; _slo_print_u32: ECX = value, prints decimal to stdout
_slo_print_u32 PROC FRAME
    push    rbx
    .pushreg rbx
    push    rdi
    .pushreg rdi
    sub     rsp, 40
    .allocstack 40
    .endprolog

    mov     eax, ecx
    lea     rdi, numBufSlo
    add     rdi, 30             ; Work backwards
    mov     byte ptr [rdi], 0   ; Null terminator

    test    eax, eax
    jnz     @@digits
    dec     rdi
    mov     byte ptr [rdi], '0'
    jmp     @@print

@@digits:
    mov     ebx, 10
@@divloop:
    test    eax, eax
    jz      @@print
    xor     edx, edx
    div     ebx
    add     dl, '0'
    dec     rdi
    mov     [rdi], dl
    jmp     @@divloop

@@print:
    mov     rcx, rdi
    call    _slo_print

    add     rsp, 40
    pop     rdi
    pop     rbx
    ret
_slo_print_u32 ENDP

; _slo_get_qpc: Returns QPC tick in RAX
_slo_get_qpc PROC FRAME
    sub     rsp, 40
    .allocstack 40
    .endprolog

    lea     rcx, qpcScratch
    call    QueryPerformanceCounter
    mov     rax, qpcScratch

    add     rsp, 40
    ret
_slo_get_qpc ENDP

; _slo_ring_index: Compute slot pointer
;   RCX = context pointer, EDX = slot index
;   Returns: RAX = pointer to RingSlot
_slo_ring_index PROC
    mov     rax, [rcx + CTX_RingBase]
    and     edx, RING_MASK              ; Wrap around
    imul    rdx, rdx, RS_SIZE
    add     rax, rdx
    ret
_slo_ring_index ENDP

; =============================================================================
; Sloloris_Init — Create and initialize the engine context
; RCX = MaxTensors (0 for default MAX_TENSORS)
; Returns: RAX = context pointer, 0 on failure
; =============================================================================
Sloloris_Init PROC FRAME
    push    rbx
    .pushreg rbx
    push    r12
    .pushreg r12
    push    r13
    .pushreg r13
    sub     rsp, 64
    .allocstack 64
    .endprolog

    ; Clamp MaxTensors
    mov     r12d, ecx
    test    r12d, r12d
    jnz     @@have_count
    mov     r12d, MAX_TENSORS
@@have_count:
    cmp     r12d, MAX_TENSORS
    jbe     @@count_ok
    mov     r12d, MAX_TENSORS
@@count_ok:

    ; Get stdout handle
    mov     ecx, STD_OUTPUT
    call    GetStdHandle
    mov     hStdOut, rax

    ; Get QPC frequency
    lea     rcx, qpcScratch
    call    QueryPerformanceFrequency
    mov     r13, qpcScratch             ; Save frequency

    ; Seed LFSR from QPC (unique per run)
    lea     rcx, qpcScratch
    call    QueryPerformanceCounter
    mov     rax, qpcScratch
    test    rax, rax
    jz      @@skip_seed                 ; Keep default if QPC fails
    mov     rcx, 0DEAD88BEh
    shl     rcx, 32
    or      rcx, 0EFCAFEh              ; RCX = 0DEAD88BE00EFCAFE
    xor     rax, rcx                    ; Mix with default
    mov     chaosLFSR, rax
@@skip_seed:

    ; Allocate context struct
    xor     ecx, ecx                    ; lpAddress = NULL
    mov     edx, CTX_SIZE
    mov     r8d, MEM_COMMIT OR MEM_RESERVE
    mov     r9d, PAGE_READWRITE
    call    VirtualAlloc
    test    rax, rax
    jz      @@fail
    mov     rbx, rax                    ; RBX = context ptr

    ; Initialize context fields
    mov     dword ptr [rbx + CTX_TensorCount], r12d
    mov     dword ptr [rbx + CTX_Strategy], STRATEGY_DRIP   ; Start in drip mode
    mov     dword ptr [rbx + CTX_RingHead], 0
    mov     dword ptr [rbx + CTX_RingTail], 0
    mov     dword ptr [rbx + CTX_LiveCount], 0
    mov     dword ptr [rbx + CTX_GhostCount], 0
    mov     qword ptr [rbx + CTX_TotalLoaded], 0
    mov     qword ptr [rbx + CTX_TickCount], 0
    mov     qword ptr [rbx + CTX_OrbitCount], 0
    mov     dword ptr [rbx + CTX_BurstPhase], 0
    mov     dword ptr [rbx + CTX_DripActive], -1
    mov     [rbx + CTX_QPCFreq], r13
    mov     dword ptr [rbx + CTX_StrengthPct], 0
    mov     dword ptr [rbx + CTX_NextTensorToLoad], 0

    ; Allocate ring buffer (RING_SLOTS * RS_SIZE)
    xor     ecx, ecx
    mov     edx, RING_SLOTS * RS_SIZE   ; 32 * 128 = 4096 bytes
    mov     r8d, MEM_COMMIT OR MEM_RESERVE
    mov     r9d, PAGE_READWRITE
    call    VirtualAlloc
    test    rax, rax
    jz      @@fail_free_ctx
    mov     [rbx + CTX_RingBase], rax

    ; Allocate tensor offset index table (8 bytes per tensor)
    xor     ecx, ecx
    mov     eax, r12d
    shl     eax, 3                      ; * 8 bytes
    mov     edx, eax
    mov     r8d, MEM_COMMIT OR MEM_RESERVE
    mov     r9d, PAGE_READWRITE
    call    VirtualAlloc
    test    rax, rax
    jz      @@fail_free_ring
    mov     [rbx + CTX_TensorIndex], rax

    ; Allocate tensor size table (8 bytes per tensor)
    xor     ecx, ecx
    mov     eax, r12d
    shl     eax, 3
    mov     edx, eax
    mov     r8d, MEM_COMMIT OR MEM_RESERVE
    mov     r9d, PAGE_READWRITE
    call    VirtualAlloc
    test    rax, rax
    jz      @@fail_free_index
    mov     [rbx + CTX_TensorSizes], rax

    ; Print banner
    lea     rcx, szSloInit
    call    _slo_print
    mov     ecx, r12d
    call    _slo_print_u32
    lea     rcx, szNewline
    call    _slo_print

    ; Return context
    mov     rax, rbx
    jmp     @@exit

@@fail_free_index:
    mov     rcx, [rbx + CTX_TensorIndex]
    xor     edx, edx
    mov     r8d, MEM_RELEASE
    call    VirtualFree

@@fail_free_ring:
    mov     rcx, [rbx + CTX_RingBase]
    xor     edx, edx
    mov     r8d, MEM_RELEASE
    call    VirtualFree

@@fail_free_ctx:
    mov     rcx, rbx
    xor     edx, edx
    mov     r8d, MEM_RELEASE
    call    VirtualFree

@@fail:
    xor     eax, eax

@@exit:
    add     rsp, 64
    pop     r13
    pop     r12
    pop     rbx
    ret
Sloloris_Init ENDP

; =============================================================================
; Sloloris_Destroy — Release all resources
; RCX = context pointer
; =============================================================================
Sloloris_Destroy PROC FRAME
    push    rbx
    .pushreg rbx
    push    r12
    .pushreg r12
    sub     rsp, 40
    .allocstack 40
    .endprolog

    test    rcx, rcx
    jz      @@done
    mov     rbx, rcx

    ; Evict all live slots (unmap views)
    xor     r12d, r12d
@@evict_loop:
    cmp     r12d, RING_SLOTS
    jge     @@free_tables

    mov     rcx, rbx
    mov     edx, r12d
    call    _slo_ring_index
    ; RAX = slot ptr
    cmp     dword ptr [rax + RS_State], SLOT_LIVE
    jne     @@skip_evict
    mov     rcx, [rax + RS_DataPtr]
    test    rcx, rcx
    jz      @@skip_evict
    call    UnmapViewOfFile

@@skip_evict:
    inc     r12d
    jmp     @@evict_loop

@@free_tables:
    ; Close file mapping
    mov     rcx, [rbx + CTX_hMapping]
    test    rcx, rcx
    jz      @@no_mapping
    call    CloseHandle
@@no_mapping:

    ; Free tensor sizes table
    mov     rcx, [rbx + CTX_TensorSizes]
    test    rcx, rcx
    jz      @@no_sizes
    xor     edx, edx
    mov     r8d, MEM_RELEASE
    call    VirtualFree
@@no_sizes:

    ; Free tensor index table
    mov     rcx, [rbx + CTX_TensorIndex]
    test    rcx, rcx
    jz      @@no_index
    xor     edx, edx
    mov     r8d, MEM_RELEASE
    call    VirtualFree
@@no_index:

    ; Free ring buffer
    mov     rcx, [rbx + CTX_RingBase]
    test    rcx, rcx
    jz      @@no_ring
    xor     edx, edx
    mov     r8d, MEM_RELEASE
    call    VirtualFree
@@no_ring:

    ; Free context
    mov     rcx, rbx
    xor     edx, edx
    mov     r8d, MEM_RELEASE
    call    VirtualFree

@@done:
    add     rsp, 40
    pop     r12
    pop     rbx
    ret
Sloloris_Destroy ENDP

; =============================================================================
; Sloloris_AttachModel — Bind engine to a GGUF file
; RCX = context, RDX = hFile (from CreateFileA), R8 = FileSize, R9 = TensorCount
; Stack: [rsp+40] = pTensorOffsets (QWORD array), [rsp+48] = pTensorSizes (QWORD array)
; Returns: EAX = 1 on success, 0 on fail
; =============================================================================
Sloloris_AttachModel PROC FRAME
    push    rbx
    .pushreg rbx
    push    rsi
    .pushreg rsi
    push    rdi
    .pushreg rdi
    push    r12
    .pushreg r12
    sub     rsp, 56
    .allocstack 56
    .endprolog

    mov     rbx, rcx                    ; Context
    mov     [rbx + CTX_hFile], rdx
    mov     [rbx + CTX_FileSize], r8
    mov     [rbx + CTX_TensorCount], r9d

    ; Create file mapping
    mov     rcx, rdx                    ; hFile
    xor     edx, edx                    ; lpAttributes
    mov     r8d, PAGE_READONLY          ; flProtect
    xor     r9d, r9d                    ; dwMaxSizeHigh
    mov     qword ptr [rsp+32], 0       ; dwMaxSizeLow (0 = file size)
    mov     qword ptr [rsp+40], 0       ; lpName
    call    CreateFileMappingA
    test    rax, rax
    jz      @@attach_fail
    mov     [rbx + CTX_hMapping], rax

    ; Copy tensor offset and size tables
    ; Stack layout: sub 56 + 4 pushes (32) = 88 below entry RSP
    ; 5th param at [rsp + 88 + 40] = [rsp + 128]
    ; 6th param at [rsp + 88 + 48] = [rsp + 136]
    mov     rsi, [rsp + 128]            ; pTensorOffsets (5th param)
    mov     rdi, [rbx + CTX_TensorIndex]
    mov     ecx, [rbx + CTX_TensorCount]
    test    rsi, rsi
    jz      @@skip_offsets
@@copy_offsets:
    mov     rax, [rsi]
    mov     [rdi], rax
    add     rsi, 8
    add     rdi, 8
    dec     ecx
    jnz     @@copy_offsets
@@skip_offsets:

    mov     rsi, [rsp + 136]            ; pTensorSizes (6th param)
    mov     rdi, [rbx + CTX_TensorSizes]
    mov     ecx, [rbx + CTX_TensorCount]
    test    rsi, rsi
    jz      @@skip_sizes
@@copy_sizes:
    mov     rax, [rsi]
    mov     [rdi], rax
    add     rsi, 8
    add     rdi, 8
    dec     ecx
    jnz     @@copy_sizes
@@skip_sizes:

    ; Calculate total model size
    xor     rax, rax
    mov     rdi, [rbx + CTX_TensorSizes]
    mov     ecx, [rbx + CTX_TensorCount]
    test    ecx, ecx
    jz      @@no_total
@@sum_loop:
    add     rax, [rdi]
    add     rdi, 8
    dec     ecx
    jnz     @@sum_loop
@@no_total:
    mov     [rbx + CTX_TotalModelSize], rax

    ; Pre-populate ring with GHOST descriptors (spoof phase bootstrap)
    call    _slo_populate_ghosts

    mov     eax, 1
    jmp     @@attach_exit

@@attach_fail:
    xor     eax, eax

@@attach_exit:
    add     rsp, 56
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret
Sloloris_AttachModel ENDP

; =============================================================================
; _slo_populate_ghosts — Fill ring with ghost descriptors
; RBX = context (preserved from caller)
; =============================================================================
_slo_populate_ghosts PROC FRAME
    push    r12
    .pushreg r12
    push    r13
    .pushreg r13
    sub     rsp, 40
    .allocstack 40
    .endprolog

    xor     r12d, r12d                  ; Slot index
    mov     r13d, [rbx + CTX_TensorCount]
    cmp     r13d, RING_SLOTS
    jbe     @@ghost_cap_ok
    mov     r13d, RING_SLOTS            ; Cap at ring size
@@ghost_cap_ok:

@@ghost_loop:
    cmp     r12d, r13d
    jge     @@ghost_done

    ; Get slot pointer
    mov     rcx, rbx
    mov     edx, r12d
    call    _slo_ring_index             ; RAX = slot ptr

    ; Install ghost
    mov     dword ptr [rax + RS_TensorID], r12d
    mov     dword ptr [rax + RS_State], SLOT_GHOST
    mov     qword ptr [rax + RS_DataPtr], 0         ; No data yet
    ; Copy size from tensor table
    mov     rcx, [rbx + CTX_TensorSizes]
    mov     edx, r12d
    shl     edx, 3
    add     rcx, rdx
    mov     rdx, [rcx]
    mov     [rax + RS_DataSize], rdx
    ; Copy file offset
    mov     rcx, [rbx + CTX_TensorIndex]
    mov     edx, r12d
    shl     edx, 3
    add     rcx, rdx
    mov     rdx, [rcx]
    mov     [rax + RS_FileOffset], rdx
    mov     qword ptr [rax + RS_DripOffset], 0
    mov     dword ptr [rax + RS_AccessCount], 0
    mov     dword ptr [rax + RS_OrbitCount], 0
    ; Generate spoof hash (simple: tensor_id XOR file_offset)
    mov     rdx, [rax + RS_FileOffset]
    xor     edx, r12d
    mov     [rax + RS_SpoofHash], rdx

    ; Print ghost install
    lea     rcx, szSloGhost
    call    _slo_print
    mov     ecx, r12d
    call    _slo_print_u32
    lea     rcx, szNewline
    call    _slo_print

    inc     r12d
    jmp     @@ghost_loop

@@ghost_done:
    mov     [rbx + CTX_GhostCount], r13d
    inc     qword ptr [rbx + CTX_StatsGhosts]

    add     rsp, 40
    pop     r13
    pop     r12
    ret
_slo_populate_ghosts ENDP

; =============================================================================
; Sloloris_Tick — Advance one streaming cycle
; RCX = context
; Returns: EAX = number of tensors changed state this tick
;
; This is the core engine heartbeat. Behavior depends on strategy:
;   DRIP:  Load one chunk of the current drip tensor
;   BURST: Load-3 / Evict-1 cycle
;   ORBIT: Full ring rotation pass
; =============================================================================
Sloloris_Tick PROC FRAME
    push    rbx
    .pushreg rbx
    push    r12
    .pushreg r12
    push    r13
    .pushreg r13
    push    r14
    .pushreg r14
    push    r15
    .pushreg r15
    push    rdi
    .pushreg rdi
    push    rsi
    .pushreg rsi
    sub     rsp, 72
    .allocstack 72
    .endprolog

    mov     rbx, rcx                    ; Context
    xor     r15d, r15d                  ; Changes counter

    ; Increment tick
    inc     qword ptr [rbx + CTX_TickCount]

    ; Get current timestamp
    call    _slo_get_qpc
    mov     [rbx + CTX_LastTickQPC], rax

    ; Dispatch based on strategy
    mov     eax, [rbx + CTX_Strategy]
    cmp     eax, STRATEGY_DRIP
    je      @@do_drip
    cmp     eax, STRATEGY_BURST
    je      @@do_burst
    cmp     eax, STRATEGY_ORBIT
    je      @@do_orbit
    jmp     @@tick_done                 ; Unknown strategy

; ─── DRIP PHASE ──────────────────────────────
; Slowloris keepalive: trickle-load one tensor chunk at a time
@@do_drip:
    ; Find current drip target
    mov     r12d, [rbx + CTX_DripActive]
    cmp     r12d, -1
    jne     @@drip_continue

    ; Find next GHOST slot to drip into
    mov     r12d, [rbx + CTX_RingHead]
@@drip_find_ghost:
    mov     rcx, rbx
    mov     edx, r12d
    call    _slo_ring_index
    mov     r14, rax                    ; Slot ptr
    cmp     dword ptr [r14 + RS_State], SLOT_GHOST
    je      @@drip_start
    inc     r12d
    and     r12d, RING_MASK
    cmp     r12d, [rbx + CTX_RingHead]
    je      @@tick_done                 ; All slots occupied, nothing to drip
    jmp     @@drip_find_ghost

@@drip_start:
    ; Transition GHOST → DRIP
    mov     dword ptr [r14 + RS_State], SLOT_DRIP
    mov     qword ptr [r14 + RS_DripOffset], 0
    mov     [rbx + CTX_DripActive], r12d
    dec     dword ptr [rbx + CTX_GhostCount]
    lea     rcx, szSloDrip
    call    _slo_print
    mov     ecx, [r14 + RS_TensorID]
    call    _slo_print_u32
    lea     rcx, szNewline
    call    _slo_print
    jmp     @@drip_continue_with_slot

@@drip_continue:
    ; Get the slot we're dripping into
    mov     rcx, rbx
    mov     edx, r12d
    call    _slo_ring_index
    mov     r14, rax

@@drip_continue_with_slot:
    ; Map view for this tensor's next chunk
    mov     rcx, [rbx + CTX_hMapping]
    mov     edx, FILE_MAP_READ
    ; Calculate file offset (base + drip progress)
    mov     rsi, [r14 + RS_FileOffset]
    add     rsi, [r14 + RS_DripOffset]
    mov     r8, rsi
    shr     r8, 32                      ; dwFileOffsetHigh
    mov     r9d, esi                    ; dwFileOffsetLow
    ; Bytes to map this chunk
    mov     rax, [r14 + RS_DataSize]
    sub     rax, [r14 + RS_DripOffset]
    cmp     rax, DRIP_CHUNK_SIZE
    jbe     @@drip_remainder
    mov     rax, DRIP_CHUNK_SIZE
@@drip_remainder:
    mov     [rsp+32], rax               ; dwNumberOfBytesToMap
    call    MapViewOfFile
    test    rax, rax
    jz      @@tick_done                 ; Map failed, try again next tick

    ; If first chunk, save the data pointer
    cmp     qword ptr [r14 + RS_DripOffset], 0
    jne     @@drip_unmap_chunk
    mov     [r14 + RS_DataPtr], rax
    jmp     @@drip_advance

@@drip_unmap_chunk:
    ; Unmap this intermediate view (we accumulate via full map later)
    mov     rcx, rax
    call    UnmapViewOfFile

@@drip_advance:
    ; Advance drip offset
    mov     rax, [r14 + RS_DripOffset]
    add     rax, DRIP_CHUNK_SIZE
    mov     [r14 + RS_DripOffset], rax
    inc     r15d                        ; Count a change

    ; Check if drip complete
    cmp     rax, [r14 + RS_DataSize]
    jl      @@tick_done                 ; More dripping needed

    ; Drip complete → LIVE
    ; Unmap the partial and do a full map
    mov     rcx, [r14 + RS_DataPtr]
    test    rcx, rcx
    jz      @@drip_full_map
    call    UnmapViewOfFile

@@drip_full_map:
    mov     rcx, [rbx + CTX_hMapping]
    mov     edx, FILE_MAP_READ
    mov     rsi, [r14 + RS_FileOffset]
    mov     r8, rsi
    shr     r8, 32
    mov     r9d, esi
    mov     rax, [r14 + RS_DataSize]
    mov     [rsp+32], rax
    call    MapViewOfFile
    test    rax, rax
    jz      @@tick_done
    mov     [r14 + RS_DataPtr], rax
    mov     dword ptr [r14 + RS_State], SLOT_LIVE
    inc     dword ptr [rbx + CTX_LiveCount]
    inc     qword ptr [rbx + CTX_StatsLoads]
    ; Add to total loaded
    mov     rax, [r14 + RS_DataSize]
    add     [rbx + CTX_TotalLoaded], rax
    ; Reset drip cursor
    mov     dword ptr [rbx + CTX_DripActive], -1
    ; Advance ring head
    mov     eax, [rbx + CTX_RingHead]
    inc     eax
    and     eax, RING_MASK
    mov     [rbx + CTX_RingHead], eax
    inc     r15d

    ; Auto-escalate: after filling half the ring, switch to BURST
    mov     eax, [rbx + CTX_LiveCount]
    cmp     eax, RING_SLOTS / 2
    jl      @@tick_done
    mov     dword ptr [rbx + CTX_Strategy], STRATEGY_BURST
    jmp     @@tick_done

; ─── BURST PHASE ─────────────────────────────
; Load-3, Evict-1 cycling pattern
@@do_burst:
    mov     r12d, [rbx + CTX_BurstPhase]
    cmp     r12d, BURST_LOAD_COUNT
    jge     @@burst_evict

    ; ── Load one tensor into ring ──
    call    _slo_load_next_tensor       ; RAX = 1 if loaded, 0 if nothing to load
    add     r15d, eax

    inc     dword ptr [rbx + CTX_BurstPhase]

    lea     rcx, szSloBurst
    call    _slo_print
    mov     ecx, [rbx + CTX_BurstPhase]
    call    _slo_print_u32
    lea     rcx, szNewline
    call    _slo_print

    jmp     @@tick_done

@@burst_evict:
    ; ── Evict the oldest/coldest slot ──
    call    _slo_evict_coldest          ; RAX = 1 if evicted
    add     r15d, eax

    ; Reset burst phase counter
    mov     dword ptr [rbx + CTX_BurstPhase], 0

    ; Auto-escalate: if >75% capacity, switch to ORBIT
    mov     eax, [rbx + CTX_LiveCount]
    mov     ecx, RING_SLOTS
    shr     ecx, 2                      ; 25% of slots
    mov     edx, RING_SLOTS
    sub     edx, ecx                    ; 75% threshold
    cmp     eax, edx
    jl      @@tick_done
    mov     dword ptr [rbx + CTX_Strategy], STRATEGY_ORBIT
    jmp     @@tick_done

; ─── ORBIT PHASE ─────────────────────────────
; Full ring rotation: evict tail, load head, advance both
@@do_orbit:
    ; Evict at tail
    call    _slo_evict_coldest
    add     r15d, eax

    ; Load at head (3 tensors per orbit tick for acceleration)
    mov     r13d, 3
@@orbit_load_loop:
    call    _slo_load_next_tensor
    add     r15d, eax
    dec     r13d
    jnz     @@orbit_load_loop

    ; Check for full orbit completion
    mov     eax, [rbx + CTX_NextTensorToLoad]
    test    eax, eax
    jnz     @@tick_done                 ; Not back to start yet
    
    ; Full orbit completed
    inc     qword ptr [rbx + CTX_OrbitCount]
    inc     qword ptr [rbx + CTX_StatsOrbits]

    lea     rcx, szSloOrbit
    call    _slo_print
    mov     rax, [rbx + CTX_OrbitCount]
    mov     ecx, eax
    call    _slo_print_u32
    lea     rcx, szNewline
    call    _slo_print

@@tick_done:
    ; Recalculate model strength
    call    _slo_calc_strength

    ; Return changes count
    mov     eax, r15d

    add     rsp, 72
    pop     rsi
    pop     rdi
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     rbx
    ret
Sloloris_Tick ENDP

; =============================================================================
; _slo_load_next_tensor — Load the next tensor in round-robin order
; RBX = context
; Returns: EAX = 1 if loaded, 0 if nothing to load
; =============================================================================
_slo_load_next_tensor PROC FRAME
    push    r12
    .pushreg r12
    push    r13
    .pushreg r13
    push    rsi
    .pushreg rsi
    sub     rsp, 48
    .allocstack 48
    .endprolog

    ; Get next tensor to load
    mov     r12d, [rbx + CTX_NextTensorToLoad]
    cmp     r12d, [rbx + CTX_TensorCount]
    jge     @@wrap_around

    ; Check if this tensor is already in the ring
    xor     r13d, r13d
@@already_check:
    cmp     r13d, RING_SLOTS
    jge     @@not_in_ring
    mov     rcx, rbx
    mov     edx, r13d
    call    _slo_ring_index
    cmp     dword ptr [rax + RS_State], SLOT_LIVE
    jne     @@next_check
    cmp     dword ptr [rax + RS_TensorID], r12d
    je      @@skip_tensor               ; Already loaded
@@next_check:
    inc     r13d
    jmp     @@already_check

@@not_in_ring:
    ; Find a free or ghost slot
    mov     r13d, [rbx + CTX_RingHead]
@@find_slot:
    mov     rcx, rbx
    mov     edx, r13d
    call    _slo_ring_index
    mov     rsi, rax                    ; Slot ptr
    mov     eax, [rsi + RS_State]
    cmp     eax, SLOT_EMPTY
    je      @@load_into_slot
    cmp     eax, SLOT_GHOST
    je      @@load_into_slot
    inc     r13d
    and     r13d, RING_MASK
    cmp     r13d, [rbx + CTX_RingHead]
    je      @@load_nothing              ; Ring full
    jmp     @@find_slot

@@load_into_slot:
    ; If ghost, decrement ghost count
    cmp     dword ptr [rsi + RS_State], SLOT_GHOST
    jne     @@not_ghost
    dec     dword ptr [rbx + CTX_GhostCount]
@@not_ghost:

    ; Set up slot metadata
    mov     dword ptr [rsi + RS_TensorID], r12d
    mov     dword ptr [rsi + RS_State], SLOT_LIVE

    ; Get file offset and size from tables
    mov     rcx, [rbx + CTX_TensorIndex]
    mov     eax, r12d
    shl     eax, 3
    mov     rax, [rcx + rax]
    mov     [rsi + RS_FileOffset], rax

    mov     rcx, [rbx + CTX_TensorSizes]
    mov     eax, r12d
    shl     eax, 3
    mov     rax, [rcx + rax]
    mov     [rsi + RS_DataSize], rax

    ; Map view of file for this tensor
    mov     rcx, [rbx + CTX_hMapping]
    mov     edx, FILE_MAP_READ
    mov     rax, [rsi + RS_FileOffset]
    mov     r8, rax
    shr     r8, 32                      ; High
    mov     r9d, eax                    ; Low
    mov     rax, [rsi + RS_DataSize]
    mov     [rsp+32], rax
    call    MapViewOfFile
    test    rax, rax
    jz      @@load_nothing              ; Map failed
    mov     [rsi + RS_DataPtr], rax

    ; Update stats
    inc     dword ptr [rbx + CTX_LiveCount]
    inc     qword ptr [rbx + CTX_StatsLoads]
    mov     rax, [rsi + RS_DataSize]
    add     [rbx + CTX_TotalLoaded], rax
    mov     dword ptr [rsi + RS_AccessCount], 0
    inc     dword ptr [rsi + RS_OrbitCount]

    ; Timestamp
    call    _slo_get_qpc
    mov     rcx, rbx                    ; Restore context in case clobbered
    ; Find the slot again to get rsi
    mov     [rsi + RS_Timestamp], rax

@@skip_tensor:
    ; Advance cursor
    inc     dword ptr [rbx + CTX_NextTensorToLoad]
    mov     eax, [rbx + CTX_NextTensorToLoad]
    cmp     eax, [rbx + CTX_TensorCount]
    jl      @@load_ok
@@wrap_around:
    mov     dword ptr [rbx + CTX_NextTensorToLoad], 0

@@load_ok:
    mov     eax, 1
    jmp     @@load_exit

@@load_nothing:
    xor     eax, eax

@@load_exit:
    add     rsp, 48
    pop     rsi
    pop     r13
    pop     r12
    ret
_slo_load_next_tensor ENDP

; =============================================================================
; _slo_evict_coldest — Evict the least-accessed LIVE slot
; RBX = context
; Returns: EAX = 1 if evicted, 0 if nothing to evict
; =============================================================================
_slo_evict_coldest PROC FRAME
    push    r12
    .pushreg r12
    push    r13
    .pushreg r13
    push    rsi
    .pushreg rsi
    push    rdi
    .pushreg rdi
    sub     rsp, 40
    .allocstack 40
    .endprolog

    xor     r12d, r12d                  ; Loop counter
    mov     r13d, -1                    ; Best victim index
    mov     edi, 7FFFFFFFh              ; Min access count (high initial)

@@scan_loop:
    cmp     r12d, RING_SLOTS
    jge     @@scan_done

    mov     rcx, rbx
    mov     edx, r12d
    call    _slo_ring_index
    mov     rsi, rax

    cmp     dword ptr [rsi + RS_State], SLOT_LIVE
    jne     @@scan_next

    ; Check if colder than current best
    mov     eax, [rsi + RS_AccessCount]
    cmp     eax, edi
    jge     @@scan_next
    mov     edi, eax
    mov     r13d, r12d

@@scan_next:
    inc     r12d
    jmp     @@scan_loop

@@scan_done:
    cmp     r13d, -1
    je      @@evict_nothing

    ; Evict the victim
    mov     rcx, rbx
    mov     edx, r13d
    call    _slo_ring_index
    mov     rsi, rax

    ; Unmap the view
    mov     rcx, [rsi + RS_DataPtr]
    test    rcx, rcx
    jz      @@clear_slot
    call    UnmapViewOfFile

@@clear_slot:
    ; Subtract from total loaded
    mov     rax, [rsi + RS_DataSize]
    sub     [rbx + CTX_TotalLoaded], rax

    ; Mark as GHOST (spoof — still shows in index but no data)
    mov     dword ptr [rsi + RS_State], SLOT_GHOST
    mov     qword ptr [rsi + RS_DataPtr], 0
    mov     qword ptr [rsi + RS_DripOffset], 0
    dec     dword ptr [rbx + CTX_LiveCount]
    inc     dword ptr [rbx + CTX_GhostCount]
    inc     qword ptr [rbx + CTX_StatsEvicts]

    mov     eax, 1
    jmp     @@evict_exit

@@evict_nothing:
    xor     eax, eax

@@evict_exit:
    add     rsp, 40
    pop     rdi
    pop     rsi
    pop     r13
    pop     r12
    ret
_slo_evict_coldest ENDP

; =============================================================================
; _slo_calc_strength — Compute model strength as percentage of loaded data
; RBX = context
; Updates: CTX_StrengthPct
;
; After 88%, applies randomized chaos operations:
;   Operators cycle through  * / * - +  (from *(/*-+) pattern)
;   Operands drawn from      1,2,3,4,5,6,7,8,9,-0
;   LFSR-driven selection so the growth curve is unpredictable
; =============================================================================
_slo_calc_strength PROC FRAME
    push    rsi
    .pushreg rsi
    push    rdi
    .pushreg rdi
    sub     rsp, 40
    .allocstack 40
    .endprolog

    mov     rax, [rbx + CTX_TotalLoaded]
    mov     rcx, [rbx + CTX_TotalModelSize]
    test    rcx, rcx
    jz      @@zero_strength

    ; strength = (TotalLoaded * 100) / TotalModelSize
    xor     edx, edx
    mov     rsi, 100
    mul     rsi                         ; RDX:RAX = loaded * 100
    div     rcx                         ; RAX = percentage
    cmp     eax, 100
    jbe     @@check_chaos
    mov     eax, 100

@@check_chaos:
    ; If below 88%, store linear and return
    cmp     eax, 88
    jl      @@store_strength

    ; === CHAOS ZONE (>=88%) ===
    mov     esi, eax                    ; ESI = raw linear strength

    ; Step 1: Advance Galois LFSR
    mov     rax, chaosLFSR
    mov     rdi, rax
    shr     rax, 1
    test    rdi, 1
    jz      @@no_tap
    mov     rcx, 0D8000000h
    shl     rcx, 32                     ; Tap polynomial high bits
    xor     rax, rcx
@@no_tap:
    mov     chaosLFSR, rax

    ; Step 2: Pick operator index from LFSR bits [3:0] mod 10
    mov     ecx, eax
    and     ecx, 0Fh
    cmp     ecx, 10
    jl      @@op_ok
    sub     ecx, 10
@@op_ok:
    lea     rdi, chaosOpTable
    movzx   edx, byte ptr [rdi + rcx]   ; EDX = operator (0-3)

    ; Step 3: Pick digit from LFSR bits [7:4] mod 10
    mov     ecx, eax
    shr     ecx, 4
    and     ecx, 0Fh
    cmp     ecx, 10
    jl      @@dig_ok
    sub     ecx, 10
@@dig_ok:
    lea     rdi, chaosDigits
    mov     ecx, [rdi + rcx*4]          ; ECX = digit (1-10)

    ; Step 4: Compute delta from base 88
    mov     eax, esi
    sub     eax, 88                     ; EAX = 0..12

    ; Step 5: Apply operator
    cmp     edx, 0
    je      @@op_mul
    cmp     edx, 1
    je      @@op_div
    cmp     edx, 2
    je      @@op_add
    cmp     edx, 3
    je      @@op_sub
    jmp     @@op_done

@@op_mul:
    imul    eax, ecx
    jmp     @@op_done
@@op_div:
    test    ecx, ecx
    jz      @@op_done
    cdq
    idiv    ecx
    jmp     @@op_done
@@op_add:
    add     eax, ecx
    jmp     @@op_done
@@op_sub:
    sub     eax, ecx
    jmp     @@op_done

@@op_done:
    ; Reconstruct: strength = 88 + chaotic_delta, clamp [88,100]
    add     eax, 88
    cmp     eax, 88
    jge     @@clamp_hi
    mov     eax, 88
@@clamp_hi:
    cmp     eax, 100
    jle     @@store_strength
    mov     eax, 100

@@store_strength:
    mov     [rbx + CTX_StrengthPct], eax
    inc     dword ptr chaosTickIdx
    add     rsp, 40
    pop     rdi
    pop     rsi
    ret

@@zero_strength:
    mov     dword ptr [rbx + CTX_StrengthPct], 0
    add     rsp, 40
    pop     rdi
    pop     rsi
    ret
_slo_calc_strength ENDP

; =============================================================================
; Sloloris_GetTensor — Fetch a tensor pointer by ID (on-demand ghost→live)
; RCX = context, EDX = tensor ID
; Returns: RAX = data pointer (or NULL if unavailable)
; =============================================================================
Sloloris_GetTensor PROC FRAME
    push    rbx
    .pushreg rbx
    push    r12
    .pushreg r12
    push    rsi
    .pushreg rsi
    sub     rsp, 48
    .allocstack 48
    .endprolog

    mov     rbx, rcx
    mov     r12d, edx                   ; Tensor ID

    ; Search ring for this tensor
    xor     ecx, ecx
@@search:
    cmp     ecx, RING_SLOTS
    jge     @@not_found

    push    rcx
    mov     rcx, rbx
    mov     edx, [rsp]                  ; slot index from stack
    call    _slo_ring_index
    mov     rsi, rax
    pop     rcx

    cmp     dword ptr [rsi + RS_TensorID], r12d
    jne     @@search_next

    ; Found matching tensor
    cmp     dword ptr [rsi + RS_State], SLOT_LIVE
    je      @@found_live

    ; GHOST or DRIP — trigger on-demand load (upgrade to LIVE)
    cmp     dword ptr [rsi + RS_State], SLOT_GHOST
    je      @@demand_load
    cmp     dword ptr [rsi + RS_State], SLOT_DRIP
    je      @@demand_load

    ; Other state (EVICTING), return NULL
    jmp     @@not_found

@@demand_load:
    ; Immediate full map (bypass drip)
    mov     rcx, [rbx + CTX_hMapping]
    mov     edx, FILE_MAP_READ
    mov     rax, [rsi + RS_FileOffset]
    mov     r8, rax
    shr     r8, 32
    mov     r9d, eax
    mov     rax, [rsi + RS_DataSize]
    mov     [rsp+32], rax
    call    MapViewOfFile
    test    rax, rax
    jz      @@not_found

    ; Upgrade state
    mov     [rsi + RS_DataPtr], rax
    mov     eax, [rsi + RS_State]
    cmp     eax, SLOT_GHOST
    jne     @@was_not_ghost
    dec     dword ptr [rbx + CTX_GhostCount]
@@was_not_ghost:
    mov     dword ptr [rsi + RS_State], SLOT_LIVE
    inc     dword ptr [rbx + CTX_LiveCount]
    inc     qword ptr [rbx + CTX_StatsLoads]
    mov     rax, [rsi + RS_DataSize]
    add     [rbx + CTX_TotalLoaded], rax

@@found_live:
    ; Increment access count (heat tracking)
    inc     dword ptr [rsi + RS_AccessCount]
    ; Update timestamp
    push    rsi
    call    _slo_get_qpc
    pop     rsi
    mov     [rsi + RS_Timestamp], rax
    ; Return data pointer
    mov     rax, [rsi + RS_DataPtr]
    jmp     @@get_exit

@@search_next:
    inc     ecx
    jmp     @@search

@@not_found:
    xor     eax, eax

@@get_exit:
    add     rsp, 48
    pop     rsi
    pop     r12
    pop     rbx
    ret
Sloloris_GetTensor ENDP

; =============================================================================
; Sloloris_GetStrength — Return current model strength as 0-100 percentage
; RCX = context
; Returns: EAX = strength percentage
; =============================================================================
Sloloris_GetStrength PROC
    mov     eax, [rcx + CTX_StrengthPct]
    ret
Sloloris_GetStrength ENDP

; =============================================================================
; Sloloris_ForceOrbit — Force an aggressive ring orbit (DoS burst pattern)
; RCX = context, EDX = orbit count (how many full rotations)
; Returns: EAX = total tensors cycled
;
; This saturates the pipeline by rapidly cycling load/unload, each orbit
; warms the file mapping cache and increases effective model strength.
; =============================================================================
Sloloris_ForceOrbit PROC FRAME
    push    rbx
    .pushreg rbx
    push    r12
    .pushreg r12
    push    r13
    .pushreg r13
    push    r14
    .pushreg r14
    sub     rsp, 40
    .allocstack 40
    .endprolog

    mov     rbx, rcx                    ; Context
    mov     r12d, edx                   ; Orbit count
    xor     r14d, r14d                  ; Total changes

    ; Force ORBIT strategy
    mov     dword ptr [rbx + CTX_Strategy], STRATEGY_ORBIT

@@orbit_outer:
    test    r12d, r12d
    jz      @@orbit_done

    ; Run tensor_count ticks to do one full orbit
    mov     r13d, [rbx + CTX_TensorCount]
    cmp     r13d, RING_SLOTS
    jbe     @@orbit_cap_ok
    mov     r13d, RING_SLOTS
@@orbit_cap_ok:
    ; Double the iterations for load+evict coverage
    shl     r13d, 1

@@orbit_inner:
    test    r13d, r13d
    jz      @@orbit_next_round

    mov     rcx, rbx
    call    Sloloris_Tick
    add     r14d, eax

    dec     r13d
    jmp     @@orbit_inner

@@orbit_next_round:
    dec     r12d
    jmp     @@orbit_outer

@@orbit_done:
    mov     eax, r14d

    add     rsp, 40
    pop     r14
    pop     r13
    pop     r12
    pop     rbx
    ret
Sloloris_ForceOrbit ENDP

; =============================================================================
; Sloloris_SetStrategy — Switch cycling strategy
; RCX = context, EDX = STRATEGY_DRIP / STRATEGY_BURST / STRATEGY_ORBIT
; Returns: EAX = previous strategy
; =============================================================================
Sloloris_SetStrategy PROC
    mov     eax, [rcx + CTX_Strategy]
    cmp     edx, STRATEGY_ORBIT
    ja      @@bad_strategy
    mov     [rcx + CTX_Strategy], edx
    ret
@@bad_strategy:
    ; Invalid strategy, return current without changing
    ret
Sloloris_SetStrategy ENDP

END
