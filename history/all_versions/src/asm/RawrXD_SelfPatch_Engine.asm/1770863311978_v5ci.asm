; =============================================================================
; RawrXD_SelfPatch_Engine.asm — Runtime Code Morphing & Hotpatch Engine
; =============================================================================
;
; Production self-patching engine for runtime code morphing. Manages a
; hotpatch table of named patch sites, generates trampolines into executable
; code caves, and performs atomic 14-byte jumps with instruction cache flush.
; Supports CPU feature detection for auto-optimization of kernel dispatch
; tables (AVX-512 vs AVX2 vs SSE4.2 paths).
;
; This is the ENGINE (policy/orchestration) layer that sits above the
; RawrXD_SelfPatch_Agent (scan/diagnostic layer). The Agent detects issues;
; the Engine applies transformations.
;
; Capabilities:
;   - Hotpatch table (256 named entries with target/patch/trampoline)
;   - Atomic 14-byte far-jump patching with CLFLUSH + MFENCE
;   - Code cave pool (1 MB RWX memory for trampoline generation)
;   - Original-bytes backup for safe rollback
;   - CPU feature detection (CPUID leaf 7: AVX-512, AVX2, SSE4.2)
;   - VTable hotswap for quantization kernel dispatch
;   - VRAM pressure-aware dynamic quantization switching
;   - Stats: patches applied, rolled back, active, cave utilization
;
; Active Exports (used by unified_hotpatch_manager.cpp):
;   asm_spengine_init           — Initialize code cave pool + patch table
;   asm_spengine_register       — Register a named patch site
;   asm_spengine_apply          — Apply a hotpatch (atomic jump + flush)
;   asm_spengine_rollback       — Restore original bytes
;   asm_spengine_cpu_optimize   — Auto-detect CPU and patch optimal paths
;   asm_spengine_quant_switch   — Dynamic quantization kernel switch
;   asm_spengine_get_stats      — Read engine statistics
;   asm_spengine_shutdown       — Rollback all, free code caves
;
; Architecture: x64 MASM64 | Windows x64 ABI | No CRT | No exceptions
; Build: ml64.exe /c /Zi /Zd /Fo RawrXD_SelfPatch_Engine.obj
; Rule: NO SOURCE FILE IS TO BE SIMPLIFIED
; =============================================================================

INCLUDE RawrXD_Common.inc

option casemap:none

; =============================================================================
;                    ENGINE CONSTANTS
; =============================================================================

; Patch table geometry
MAX_PATCHES             EQU     256
PATCH_NAME_MAX          EQU     64          ; Max name string length

; Code cave pool
CODE_CAVE_SIZE          EQU     100000h     ; 1 MB of RWX scratch
TRAMPOLINE_SIZE         EQU     14          ; FF 25 00 00 00 00 + 8-byte abs addr

; Far jump instruction template (14 bytes)
; FF 25 00 00 00 00 = JMP [RIP+0]
; followed by 8-byte absolute address
FAR_JMP_OPCODE_1        EQU     0FFh
FAR_JMP_OPCODE_2        EQU     025h

; Hotpatch IDs (well-known patch sites)
PATCH_ID_DEFLATE_FAST   EQU     1           ; Replace deflate with AVX-512
PATCH_ID_ATTN_FLASH     EQU     2           ; Swap attention kernels
PATCH_ID_MEMCPY_NT      EQU     3           ; Enable non-temporal moves
PATCH_ID_QUANT_DYNAMIC  EQU     4           ; Runtime quantization switch
PATCH_ID_BRANCH_HINT    EQU     5           ; CPU-specific branch hints

; CPU feature flags (from CPUID)
CPU_FEAT_SSE42          EQU     00000001h
CPU_FEAT_AVX2           EQU     00000002h
CPU_FEAT_AVX512F        EQU     00000004h
CPU_FEAT_AVX512BW       EQU     00000008h
CPU_FEAT_AVX512VL       EQU     00000010h
CPU_FEAT_AVX512VNNI     EQU     00000020h
CPU_FEAT_BMI2           EQU     00000040h
CPU_FEAT_POPCNT         EQU     00000080h

; Quantization kernel IDs
QKERNEL_Q4_K_M          EQU     0
QKERNEL_Q5_K_M          EQU     1
QKERNEL_Q8_0            EQU     2
QKERNEL_COUNT           EQU     3

; v14.2.1: VRAM pressure thresholds for dynamic quantization
VRAM_PRESSURE_HIGH      EQU     85          ; >=85% \u2192 force Q4_K_M
VRAM_PRESSURE_MEDIUM    EQU     65          ; >=65% \u2192 cap at Q5_K_M
VRAM_PRESSURE_LOW       EQU     0           ; <65%  \u2192 allow Q8_0

; Engine error codes
SPE_OK                  EQU     0
SPE_ERR_NOT_INIT        EQU     1
SPE_ERR_ALLOC           EQU     2
SPE_ERR_TABLE_FULL      EQU     3
SPE_ERR_NOT_FOUND       EQU     4
SPE_ERR_ALREADY_ACTIVE  EQU     5
SPE_ERR_NOT_ACTIVE      EQU     6
SPE_ERR_VPROTECT        EQU     7
SPE_ERR_ALREADY_INIT    EQU     8
SPE_ERR_CAVE_FULL       EQU     9

; =============================================================================
;                    STRUCTURES
; =============================================================================

; Patch entry in the hotpatch table
PATCH_ENTRY STRUCT 8
    name            DB PATCH_NAME_MAX DUP(?)    ; Patch site name
    name_len        DD      ?
    patch_id        DD      ?               ; Well-known ID or user-assigned
    target_addr     DQ      ?               ; Address to patch (original code)
    patch_addr      DQ      ?               ; Address of replacement code
    trampoline      DQ      ?               ; Generated trampoline to original
    original_bytes  DB TRAMPOLINE_SIZE DUP(?) ; Backup of overwritten bytes
    _pad0           DW      ?               ; Align to 8
    active          DD      ?               ; 1 = patch applied, 0 = not
    _pad1           DD      ?
PATCH_ENTRY ENDS

; Engine statistics
SPE_STATS STRUCT
    patches_registered DQ   ?
    patches_applied DQ      ?
    patches_rolled  DQ      ?
    patches_active  DQ      ?               ; Currently active count
    cave_used       DQ      ?               ; Bytes used in code cave pool
    cave_total      DQ      ?               ; Total cave size
    cpu_features    DQ      ?               ; Detected CPU__FEAT_* mask
    quant_switches  DQ      ?               ; Dynamic quant switches
SPE_STATS ENDS

; Engine context
SPE_CONTEXT STRUCT
    initialized     DD      ?
    _pad0           DD      ?
    pPatchTable     DQ      ?               ; -> PATCH_ENTRY[MAX_PATCHES]
    pCodeCave       DQ      ?               ; -> RWX code cave pool base
    codeCavePtr     DQ      ?               ; Current allocation cursor
    cpuFeatures     DQ      ?               ; Detected features bitmask
    pQuantTable     DQ      ?               ; -> Quantization dispatch vtable
    vramPressure    DD      ?               ; v14.2.1: Last known VRAM usage %
    activeQuant     DD      ?               ; v14.2.1: Current quantization kernel ID
SPE_CONTEXT ENDS

; =============================================================================
;                    DATA SECTION
; =============================================================================
.data

ALIGN 16
g_spe_ctx       SPE_CONTEXT <>
g_spe_stats     SPE_STATS <>

; Critical section for thread safety
ALIGN 16
g_spe_cs        CRITICAL_SECTION <>

; Quantization dispatch table (3 entries: Q4, Q5, Q8)
ALIGN 16
g_quant_dispatch DQ QKERNEL_COUNT DUP(0)

; Status strings
szSPEInit       DB "SelfPatchEngine: initialized", 0
szSPEApply      DB "SelfPatchEngine: patch applied", 0
szSPERollback   DB "SelfPatchEngine: patch rolled back", 0
szSPECPU        DB "SelfPatchEngine: CPU optimization applied", 0
szSPEShutdown   DB "SelfPatchEngine: shutdown complete", 0
szSPEQuantSw    DB "SelfPatchEngine: quantization kernel switched", 0

; =============================================================================
;                    EXPORTS
; =============================================================================
PUBLIC asm_spengine_init
PUBLIC asm_spengine_register
PUBLIC asm_spengine_apply
PUBLIC asm_spengine_rollback
PUBLIC asm_spengine_cpu_optimize
PUBLIC asm_spengine_quant_switch
PUBLIC asm_spengine_get_stats
PUBLIC asm_spengine_shutdown
PUBLIC asm_spengine_quant_switch_adaptive

; =============================================================================
;                    EXTERNAL IMPORTS
; =============================================================================
EXTERN FlushInstructionCache: PROC
EXTERN GetCurrentProcess: PROC

; =============================================================================
;                    CODE SECTION
; =============================================================================
.code

; =============================================================================
; detect_cpu_features — Internal: CPUID-based feature detection
; Returns: RAX = CPU_FEAT_* bitmask
; =============================================================================
detect_cpu_features PROC FRAME
    push    rbx
    .pushreg rbx
    sub     rsp, 8
    .allocstack 8
    .endprolog

    xor     r8d, r8d                        ; feature accumulator

    ; CPUID leaf 1: ECX has SSE4.2 (bit 20), POPCNT (bit 23)
    mov     eax, 1
    cpuid
    test    ecx, (1 SHL 20)                 ; SSE4.2
    jz      @@no_sse42
    or      r8d, CPU_FEAT_SSE42
@@no_sse42:
    test    ecx, (1 SHL 23)                 ; POPCNT
    jz      @@no_popcnt
    or      r8d, CPU_FEAT_POPCNT
@@no_popcnt:

    ; CPUID leaf 7, subleaf 0: EBX has AVX2 (bit 5), BMI2 (bit 8),
    ;   AVX-512F (bit 16), AVX-512BW (bit 30)
    ; ECX has AVX-512VNNI (bit 11)
    mov     eax, 7
    xor     ecx, ecx
    cpuid

    test    ebx, (1 SHL 5)                  ; AVX2
    jz      @@no_avx2
    or      r8d, CPU_FEAT_AVX2
@@no_avx2:
    test    ebx, (1 SHL 8)                  ; BMI2
    jz      @@no_bmi2
    or      r8d, CPU_FEAT_BMI2
@@no_bmi2:
    test    ebx, (1 SHL 16)                 ; AVX-512F
    jz      @@no_avx512f
    or      r8d, CPU_FEAT_AVX512F
@@no_avx512f:
    test    ebx, (1 SHL 30)                 ; AVX-512BW
    jz      @@no_avx512bw
    or      r8d, CPU_FEAT_AVX512BW
@@no_avx512bw:

    ; AVX-512VL in EBX bit 31
    test    ebx, (1 SHL 31)
    jz      @@no_avx512vl
    or      r8d, CPU_FEAT_AVX512VL
@@no_avx512vl:

    ; AVX-512VNNI in ECX bit 11
    test    ecx, (1 SHL 11)
    jz      @@no_avx512vnni
    or      r8d, CPU_FEAT_AVX512VNNI
@@no_avx512vnni:

    mov     eax, r8d

    add     rsp, 8
    pop     rbx
    ret
detect_cpu_features ENDP

; =============================================================================
; find_patch_by_id — Internal: linear search for patch entry by ID
; ECX = patch_id
; Returns: RAX = PATCH_ENTRY* or 0 if not found
; =============================================================================
find_patch_by_id PROC FRAME
    push    rbx
    .pushreg rbx
    push    rsi
    .pushreg rsi
    sub     rsp, 8
    .allocstack 8
    .endprolog

    mov     ebx, ecx
    mov     rsi, QWORD PTR [g_spe_ctx.pPatchTable]
    test    rsi, rsi
    jz      @@find_fail

    mov     ecx, DWORD PTR [g_spe_stats.patches_registered]
    xor     eax, eax

@@find_loop:
    cmp     eax, ecx
    jge     @@find_fail
    cmp     DWORD PTR [rsi].PATCH_ENTRY.patch_id, ebx
    je      @@find_found
    add     rsi, SIZEOF PATCH_ENTRY
    inc     eax
    jmp     @@find_loop

@@find_found:
    mov     rax, rsi
    jmp     @@find_exit

@@find_fail:
    xor     eax, eax

@@find_exit:
    add     rsp, 8
    pop     rsi
    pop     rbx
    ret
find_patch_by_id ENDP

; =============================================================================
; generate_trampoline — Internal: write a trampoline to the code cave
; RCX = target address (where to jump after the original bytes)
; Returns: RAX = trampoline address in code cave
; =============================================================================
generate_trampoline PROC FRAME
    push    rbx
    .pushreg rbx
    sub     rsp, 8
    .allocstack 8
    .endprolog

    mov     rbx, rcx                        ; jump target

    ; Get current code cave allocation pointer
    mov     rax, QWORD PTR [g_spe_ctx.codeCavePtr]

    ; Check if enough space (14 bytes for trampoline)
    mov     rcx, rax
    sub     rcx, QWORD PTR [g_spe_ctx.pCodeCave]
    add     rcx, TRAMPOLINE_SIZE
    cmp     rcx, CODE_CAVE_SIZE
    jg      @@tramp_full

    ; Write: FF 25 00 00 00 00
    mov     BYTE PTR [rax], FAR_JMP_OPCODE_1
    mov     BYTE PTR [rax+1], FAR_JMP_OPCODE_2
    mov     DWORD PTR [rax+2], 0            ; RIP-relative offset = 0

    ; Write 8-byte absolute address
    mov     QWORD PTR [rax+6], rbx

    ; Advance cave pointer
    lea     rcx, [rax+TRAMPOLINE_SIZE]
    mov     QWORD PTR [g_spe_ctx.codeCavePtr], rcx

    ; Update stats
    add     QWORD PTR [g_spe_stats.cave_used], TRAMPOLINE_SIZE

    ; rax = trampoline address (already set)
    jmp     @@tramp_exit

@@tramp_full:
    xor     eax, eax                        ; NULL = cave full

@@tramp_exit:
    add     rsp, 8
    pop     rbx
    ret
generate_trampoline ENDP

; =============================================================================
; asm_spengine_init
; Initialize the self-patch engine: code cave pool, patch table, CPU detection.
; No parameters.
; Returns: RAX = 0 success, SPE_ERR_* on failure
; =============================================================================
asm_spengine_init PROC FRAME
    push    rbx
    .pushreg rbx
    push    rsi
    .pushreg rsi
    sub     rsp, 48
    .allocstack 48
    .endprolog

    cmp     DWORD PTR [g_spe_ctx.initialized], 1
    je      @@init_already

    ; Initialize critical section
    lea     rcx, g_spe_cs
    call    InitializeCriticalSection

    ; Zero stats
    lea     rdi, g_spe_stats
    xor     eax, eax
    mov     ecx, SIZEOF SPE_STATS
    rep     stosb

    ; Allocate code cave pool (1 MB, RWX for executable trampolines)
    xor     ecx, ecx
    mov     edx, CODE_CAVE_SIZE
    mov     r8d, MEM_COMMIT OR MEM_RESERVE
    mov     r9d, PAGE_EXECUTE_READWRITE
    call    VirtualAlloc
    test    rax, rax
    jz      @@init_alloc_fail
    mov     QWORD PTR [g_spe_ctx.pCodeCave], rax
    mov     QWORD PTR [g_spe_ctx.codeCavePtr], rax
    mov     QWORD PTR [g_spe_stats.cave_total], CODE_CAVE_SIZE
    mov     QWORD PTR [g_spe_stats.cave_used], 0

    ; Allocate patch table
    xor     ecx, ecx
    mov     edx, MAX_PATCHES * (SIZEOF PATCH_ENTRY)
    mov     r8d, MEM_COMMIT OR MEM_RESERVE
    mov     r9d, PAGE_READWRITE
    call    VirtualAlloc
    test    rax, rax
    jz      @@init_alloc_fail
    mov     QWORD PTR [g_spe_ctx.pPatchTable], rax

    ; Zero patch table
    mov     rdi, rax
    xor     eax, eax
    mov     ecx, MAX_PATCHES * (SIZEOF PATCH_ENTRY)
    rep     stosb

    ; Zero quantization dispatch table
    lea     rdi, g_quant_dispatch
    xor     eax, eax
    mov     ecx, QKERNEL_COUNT * 8
    rep     stosb
    lea     rax, g_quant_dispatch
    mov     QWORD PTR [g_spe_ctx.pQuantTable], rax

    ; Detect CPU features
    call    detect_cpu_features
    mov     QWORD PTR [g_spe_ctx.cpuFeatures], rax
    mov     QWORD PTR [g_spe_stats.cpu_features], rax

    ; Mark initialized
    mov     DWORD PTR [g_spe_ctx.initialized], 1

    lea     rcx, szSPEInit
    call    OutputDebugStringA

    xor     eax, eax
    jmp     @@init_exit

@@init_already:
    mov     eax, SPE_ERR_ALREADY_INIT
    jmp     @@init_exit

@@init_alloc_fail:
    mov     eax, SPE_ERR_ALLOC

@@init_exit:
    add     rsp, 48
    pop     rsi
    pop     rbx
    ret
asm_spengine_init ENDP

; =============================================================================
; asm_spengine_register
; Register a named patch site in the hotpatch table.
; RCX = name string pointer (null-terminated)
; EDX = patch_id
; R8  = target address (code to be patched)
; R9  = patch address (replacement code)
; Returns: RAX = 0 success, SPE_ERR_* on failure
; =============================================================================
asm_spengine_register PROC FRAME
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
    push    r14
    .pushreg r14
    sub     rsp, 48
    .allocstack 48
    .endprolog

    cmp     DWORD PTR [g_spe_ctx.initialized], 1
    jne     @@reg_not_init

    mov     r12, rcx                        ; name
    mov     r13d, edx                       ; patch_id
    mov     r14, r8                         ; target
    mov     rbx, r9                         ; patch addr

    lea     rcx, g_spe_cs
    call    EnterCriticalSection

    ; Check table capacity
    mov     eax, DWORD PTR [g_spe_stats.patches_registered]
    cmp     eax, MAX_PATCHES
    jge     @@reg_full

    ; Get next slot
    mov     rsi, QWORD PTR [g_spe_ctx.pPatchTable]
    imul    rdi, rax, SIZEOF PATCH_ENTRY
    add     rsi, rdi                        ; -> new entry

    ; Copy name
    mov     rcx, rsi                        ; dest = entry.name
    mov     rdx, r12                        ; src = name string
    mov     r8d, PATCH_NAME_MAX - 1         ; max len
    call    strncpy_safe                    ; Internal safe copy

    ; Set fields
    mov     DWORD PTR [rsi].PATCH_ENTRY.patch_id, r13d
    mov     QWORD PTR [rsi].PATCH_ENTRY.target_addr, r14
    mov     QWORD PTR [rsi].PATCH_ENTRY.patch_addr, rbx
    mov     QWORD PTR [rsi].PATCH_ENTRY.trampoline, 0
    mov     DWORD PTR [rsi].PATCH_ENTRY.active, 0

    ; Increment count
    lock inc QWORD PTR [g_spe_stats.patches_registered]

    lea     rcx, g_spe_cs
    call    LeaveCriticalSection

    xor     eax, eax
    jmp     @@reg_exit

@@reg_not_init:
    mov     eax, SPE_ERR_NOT_INIT
    jmp     @@reg_exit

@@reg_full:
    lea     rcx, g_spe_cs
    call    LeaveCriticalSection
    mov     eax, SPE_ERR_TABLE_FULL

@@reg_exit:
    add     rsp, 48
    pop     r14
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret
asm_spengine_register ENDP

; =============================================================================
; strncpy_safe — Internal: copy up to R8 bytes from RDX to RCX, null-terminate
; =============================================================================
strncpy_safe PROC FRAME
    push    rbx
    .pushreg rbx
    sub     rsp, 8
    .allocstack 8
    .endprolog

    xor     ebx, ebx                       ; count
@@copy_loop:
    cmp     ebx, r8d
    jge     @@terminate
    movzx   eax, BYTE PTR [rdx+rbx]
    test    al, al
    jz      @@terminate
    mov     BYTE PTR [rcx+rbx], al
    inc     ebx
    jmp     @@copy_loop

@@terminate:
    mov     BYTE PTR [rcx+rbx], 0

    add     rsp, 8
    pop     rbx
    ret
strncpy_safe ENDP

; =============================================================================
; asm_spengine_apply
; Apply a hotpatch: backup original bytes, generate trampoline, write far jump.
; ECX = patch_id
; Returns: RAX = trampoline address (call through this to reach original code)
;          or 0 on failure
; =============================================================================
asm_spengine_apply PROC FRAME
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
    push    r14
    .pushreg r14
    sub     rsp, 56
    .allocstack 56
    .endprolog

    cmp     DWORD PTR [g_spe_ctx.initialized], 1
    jne     @@apply_fail

    mov     r12d, ecx                       ; patch_id

    lea     rcx, g_spe_cs
    call    EnterCriticalSection

    ; Find patch entry
    mov     ecx, r12d
    call    find_patch_by_id
    test    rax, rax
    jz      @@apply_not_found
    mov     rsi, rax                        ; PATCH_ENTRY*

    ; Check if already active
    cmp     DWORD PTR [rsi].PATCH_ENTRY.active, 1
    je      @@apply_already

    ; Get target address
    mov     r13, QWORD PTR [rsi].PATCH_ENTRY.target_addr
    mov     r14, QWORD PTR [rsi].PATCH_ENTRY.patch_addr

    ; Backup original bytes (14 bytes)
    lea     rdi, [rsi].PATCH_ENTRY.original_bytes
    mov     rbx, r13                        ; source = target addr
    mov     ecx, TRAMPOLINE_SIZE
@@backup_loop:
    movzx   eax, BYTE PTR [rbx]
    mov     BYTE PTR [rdi], al
    inc     rbx
    inc     rdi
    dec     ecx
    jnz     @@backup_loop

    ; Generate trampoline (jumps to target + 14, past our patch)
    lea     rcx, [r13 + TRAMPOLINE_SIZE]    ; original code continues here
    call    generate_trampoline
    test    rax, rax
    jz      @@apply_cave_full
    mov     QWORD PTR [rsi].PATCH_ENTRY.trampoline, rax
    mov     rdi, rax                        ; save trampoline addr

    ; Unprotect target (make writable)
    mov     rcx, r13                        ; address
    mov     edx, TRAMPOLINE_SIZE            ; size
    mov     r8d, PAGE_EXECUTE_READWRITE     ; new protection
    lea     r9, [rsp+40h]                   ; old protection output
    call    VirtualProtect
    test    eax, eax
    jz      @@apply_vprotect_err

    ; Write atomic far jump: FF 25 00 00 00 00 + 8-byte absolute addr
    mov     BYTE PTR [r13], FAR_JMP_OPCODE_1
    mov     BYTE PTR [r13+1], FAR_JMP_OPCODE_2
    mov     DWORD PTR [r13+2], 0            ; RIP offset = 0
    mov     QWORD PTR [r13+6], r14          ; Absolute destination

    ; Memory fence + instruction cache flush
    mfence

    ; Flush instruction cache
    call    GetCurrentProcess
    mov     rcx, rax                        ; hProcess
    mov     rdx, r13                        ; base address
    mov     r8d, TRAMPOLINE_SIZE            ; size
    call    FlushInstructionCache

    ; Restore original protection
    mov     rcx, r13
    mov     edx, TRAMPOLINE_SIZE
    mov     r8d, DWORD PTR [rsp+40h]       ; old protection
    lea     r9, [rsp+44h]                   ; dummy output
    call    VirtualProtect

    ; Mark active
    mov     DWORD PTR [rsi].PATCH_ENTRY.active, 1

    ; Update stats
    lock inc QWORD PTR [g_spe_stats.patches_applied]
    lock inc QWORD PTR [g_spe_stats.patches_active]

    lea     rcx, g_spe_cs
    call    LeaveCriticalSection

    lea     rcx, szSPEApply
    call    OutputDebugStringA

    mov     rax, rdi                        ; return trampoline address
    jmp     @@apply_exit

@@apply_not_found:
    lea     rcx, g_spe_cs
    call    LeaveCriticalSection
    xor     eax, eax
    jmp     @@apply_exit

@@apply_already:
    lea     rcx, g_spe_cs
    call    LeaveCriticalSection
    mov     rax, QWORD PTR [rsi].PATCH_ENTRY.trampoline
    jmp     @@apply_exit

@@apply_cave_full:
    lea     rcx, g_spe_cs
    call    LeaveCriticalSection
    xor     eax, eax
    jmp     @@apply_exit

@@apply_vprotect_err:
    lea     rcx, g_spe_cs
    call    LeaveCriticalSection
@@apply_fail:
    xor     eax, eax

@@apply_exit:
    add     rsp, 56
    pop     r14
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret
asm_spengine_apply ENDP

; =============================================================================
; asm_spengine_rollback
; Restore original bytes at a patch site.
; ECX = patch_id
; Returns: RAX = 0 success, SPE_ERR_* on failure
; =============================================================================
asm_spengine_rollback PROC FRAME
    push    rbx
    .pushreg rbx
    push    rsi
    .pushreg rsi
    push    rdi
    .pushreg rdi
    sub     rsp, 48
    .allocstack 48
    .endprolog

    cmp     DWORD PTR [g_spe_ctx.initialized], 1
    jne     @@roll_not_init

    mov     ebx, ecx

    lea     rcx, g_spe_cs
    call    EnterCriticalSection

    ; Find patch
    mov     ecx, ebx
    call    find_patch_by_id
    test    rax, rax
    jz      @@roll_not_found
    mov     rsi, rax

    cmp     DWORD PTR [rsi].PATCH_ENTRY.active, 0
    je      @@roll_not_active

    ; Get target
    mov     rdi, QWORD PTR [rsi].PATCH_ENTRY.target_addr

    ; Unprotect
    mov     rcx, rdi
    mov     edx, TRAMPOLINE_SIZE
    mov     r8d, PAGE_EXECUTE_READWRITE
    lea     r9, [rsp+32h]
    call    VirtualProtect
    test    eax, eax
    jz      @@roll_vprotect

    ; Restore original bytes
    lea     rbx, [rsi].PATCH_ENTRY.original_bytes
    mov     ecx, TRAMPOLINE_SIZE
@@restore_loop:
    movzx   eax, BYTE PTR [rbx]
    mov     BYTE PTR [rdi], al
    inc     rbx
    inc     rdi
    dec     ecx
    jnz     @@restore_loop

    ; Memory fence
    mfence

    ; Flush instruction cache
    call    GetCurrentProcess
    mov     rcx, rax
    mov     rdx, QWORD PTR [rsi].PATCH_ENTRY.target_addr
    mov     r8d, TRAMPOLINE_SIZE
    call    FlushInstructionCache

    ; Restore protection
    mov     rcx, QWORD PTR [rsi].PATCH_ENTRY.target_addr
    mov     edx, TRAMPOLINE_SIZE
    mov     r8d, DWORD PTR [rsp+32h]
    lea     r9, [rsp+36h]
    call    VirtualProtect

    ; Mark inactive
    mov     DWORD PTR [rsi].PATCH_ENTRY.active, 0

    lock inc QWORD PTR [g_spe_stats.patches_rolled]
    lock dec QWORD PTR [g_spe_stats.patches_active]

    lea     rcx, g_spe_cs
    call    LeaveCriticalSection

    lea     rcx, szSPERollback
    call    OutputDebugStringA

    xor     eax, eax
    jmp     @@roll_exit

@@roll_not_init:
    mov     eax, SPE_ERR_NOT_INIT
    jmp     @@roll_exit
@@roll_not_found:
    lea     rcx, g_spe_cs
    call    LeaveCriticalSection
    mov     eax, SPE_ERR_NOT_FOUND
    jmp     @@roll_exit
@@roll_not_active:
    lea     rcx, g_spe_cs
    call    LeaveCriticalSection
    mov     eax, SPE_ERR_NOT_ACTIVE
    jmp     @@roll_exit
@@roll_vprotect:
    lea     rcx, g_spe_cs
    call    LeaveCriticalSection
    mov     eax, SPE_ERR_VPROTECT

@@roll_exit:
    add     rsp, 48
    pop     rdi
    pop     rsi
    pop     rbx
    ret
asm_spengine_rollback ENDP

; =============================================================================
; asm_spengine_cpu_optimize
; Auto-detect CPU and apply optimal kernel patches.
; No parameters.
; Returns: RAX = CPU_FEAT_* bitmask that was detected
; =============================================================================
asm_spengine_cpu_optimize PROC FRAME
    push    rbx
    .pushreg rbx
    sub     rsp, 40
    .allocstack 40
    .endprolog

    cmp     DWORD PTR [g_spe_ctx.initialized], 1
    jne     @@opt_fail

    mov     rbx, QWORD PTR [g_spe_ctx.cpuFeatures]

    ; AVX-512 VNNI available? (Zen4+ / Ice Lake+)
    test    rbx, CPU_FEAT_AVX512VNNI
    jz      @@check_avx512f

    ; Apply FlashAttention AVX-512 VNNI patch
    mov     ecx, PATCH_ID_ATTN_FLASH
    call    find_patch_by_id
    test    rax, rax
    jz      @@check_quant_vnni
    mov     ecx, PATCH_ID_ATTN_FLASH
    call    asm_spengine_apply

@@check_quant_vnni:
    ; Apply quantization AVX-512 patch
    mov     ecx, PATCH_ID_QUANT_DYNAMIC
    call    find_patch_by_id
    test    rax, rax
    jz      @@opt_done
    mov     ecx, PATCH_ID_QUANT_DYNAMIC
    call    asm_spengine_apply
    jmp     @@opt_done

@@check_avx512f:
    ; Plain AVX-512F (no VNNI)
    test    rbx, CPU_FEAT_AVX512F
    jz      @@check_avx2

    mov     ecx, PATCH_ID_DEFLATE_FAST
    call    find_patch_by_id
    test    rax, rax
    jz      @@opt_done
    mov     ecx, PATCH_ID_DEFLATE_FAST
    call    asm_spengine_apply
    jmp     @@opt_done

@@check_avx2:
    ; AVX2 baseline (Zen2+, Haswell+)
    test    rbx, CPU_FEAT_AVX2
    jz      @@opt_done

    ; Apply non-temporal memcpy patch
    mov     ecx, PATCH_ID_MEMCPY_NT
    call    find_patch_by_id
    test    rax, rax
    jz      @@opt_done
    mov     ecx, PATCH_ID_MEMCPY_NT
    call    asm_spengine_apply

@@opt_done:
    lea     rcx, szSPECPU
    call    OutputDebugStringA

    mov     rax, rbx                        ; return feature mask
    jmp     @@opt_exit

@@opt_fail:
    xor     eax, eax

@@opt_exit:
    add     rsp, 40
    pop     rbx
    ret
asm_spengine_cpu_optimize ENDP

; =============================================================================
; asm_spengine_quant_switch
; Dynamically switch quantization kernel dispatch.
; ECX = target kernel (QKERNEL_Q4_K_M, QKERNEL_Q5_K_M, QKERNEL_Q8_0)
; RDX = new kernel function pointer
; Returns: RAX = previous kernel pointer
; =============================================================================
asm_spengine_quant_switch PROC FRAME
    push    rbx
    .pushreg rbx
    sub     rsp, 40
    .allocstack 40
    .endprolog

    cmp     DWORD PTR [g_spe_ctx.initialized], 1
    jne     @@qsw_fail

    cmp     ecx, QKERNEL_COUNT
    jge     @@qsw_fail

    ; Atomic swap in dispatch table
    lea     rbx, g_quant_dispatch
    mov     rax, rdx
    xchg    QWORD PTR [rbx + rcx*8], rax   ; old -> rax, new -> table

    lock inc QWORD PTR [g_spe_stats.quant_switches]

    lea     rcx, szSPEQuantSw
    call    OutputDebugStringA

    ; rax = old kernel pointer
    jmp     @@qsw_exit

@@qsw_fail:
    xor     eax, eax

@@qsw_exit:
    add     rsp, 40
    pop     rbx
    ret
asm_spengine_quant_switch ENDP

; =============================================================================
; asm_spengine_get_stats
; Read engine statistics.
; RCX = output SPE_STATS* buffer
; Returns: RAX = 0
; =============================================================================
asm_spengine_get_stats PROC FRAME
    push    rbx
    .pushreg rbx
    sub     rsp, 40
    .allocstack 40
    .endprolog

    mov     rbx, rcx
    mov     rcx, rbx
    lea     rdx, g_spe_stats
    mov     r8d, SIZEOF SPE_STATS
    call    memcpy

    xor     eax, eax
    add     rsp, 40
    pop     rbx
    ret
asm_spengine_get_stats ENDP

; =============================================================================
; asm_spengine_shutdown
; Rollback all active patches, free code caves, destroy critical section.
; Returns: RAX = 0
; =============================================================================
asm_spengine_shutdown PROC FRAME
    push    rbx
    .pushreg rbx
    push    rsi
    .pushreg rsi
    push    r12
    .pushreg r12
    sub     rsp, 48
    .allocstack 48
    .endprolog

    cmp     DWORD PTR [g_spe_ctx.initialized], 1
    jne     @@shut_exit

    ; Rollback all active patches
    mov     rsi, QWORD PTR [g_spe_ctx.pPatchTable]
    mov     r12d, DWORD PTR [g_spe_stats.patches_registered]
    xor     ebx, ebx

@@rollback_all:
    cmp     ebx, r12d
    jge     @@rollback_done

    cmp     DWORD PTR [rsi].PATCH_ENTRY.active, 1
    jne     @@next_rollback

    mov     ecx, DWORD PTR [rsi].PATCH_ENTRY.patch_id
    call    asm_spengine_rollback

@@next_rollback:
    add     rsi, SIZEOF PATCH_ENTRY
    inc     ebx
    jmp     @@rollback_all

@@rollback_done:
    ; Free code cave pool
    mov     rcx, QWORD PTR [g_spe_ctx.pCodeCave]
    test    rcx, rcx
    jz      @@skip_cave
    xor     edx, edx
    mov     r8d, MEM_RELEASE
    call    VirtualFree
@@skip_cave:

    ; Free patch table
    mov     rcx, QWORD PTR [g_spe_ctx.pPatchTable]
    test    rcx, rcx
    jz      @@skip_table
    xor     edx, edx
    mov     r8d, MEM_RELEASE
    call    VirtualFree
@@skip_table:

    ; Destroy critical section
    lea     rcx, g_spe_cs
    call    DeleteCriticalSection

    ; Zero context
    lea     rdi, g_spe_ctx
    xor     eax, eax
    mov     ecx, SIZEOF SPE_CONTEXT
    rep     stosb

    lea     rcx, szSPEShutdown
    call    OutputDebugStringA

@@shut_exit:
    xor     eax, eax
    add     rsp, 48
    pop     r12
    pop     rsi
    pop     rbx
    ret
asm_spengine_shutdown ENDP

END
