; =============================================================================
; RawrXD_Hotpatch_Kernel.asm — Shadow-Page Detour Hotpatching Kernel (x64 MASM)
; =============================================================================
;
; Atomic redirect of Camellia-256 / DirectML functions at runtime via
; Shadow-Page Detour system. The SelfRepairLoop identifies a bug, the
; AgenticAssembler re-assembles a fix in MASM, and this kernel swaps the
; opcode stream without a process restart.
;
; Capabilities:
;   - Atomic 14-byte absolute-jump prologue rewrite (FAR JMP via FF 25)
;   - Shadow page allocation (RWX code cave via VirtualAlloc)
;   - Function prologue backup and restore for rollback
;   - Instruction cache flush via FlushInstructionCache
;   - CLFLUSH + MFENCE barrier for L1/L2 cache coherence
;   - Snapshot capture: saves original bytes for "Known Good" rollback
;   - CRC32 pre/post verification guard
;
; Exports (called from shadow_page_detour.cpp / agent_self_repair.cpp):
;   asm_hotpatch_atomic_swap       — Atomic redirect (14-byte prologue rewrite)
;   asm_hotpatch_alloc_shadow      — Allocate RWX shadow page for new code
;   asm_hotpatch_free_shadow       — Release shadow page
;   asm_hotpatch_backup_prologue   — Backup function prologue (16 bytes)
;   asm_hotpatch_restore_prologue  — Restore backed-up prologue (rollback)
;   asm_hotpatch_verify_prologue   — CRC32 verify that prologue matches expected
;   asm_hotpatch_flush_icache      — Flush instruction cache for patched region
;   asm_hotpatch_get_stats         — Read hotpatch statistics
;
; Architecture: x64 MASM64 | Windows x64 ABI | No CRT | No exceptions
; Build: ml64.exe /c /Zi /Zd /Fo RawrXD_Hotpatch_Kernel.obj
; Rule: NO SOURCE FILE IS TO BE SIMPLIFIED
; =============================================================================

INCLUDE RawrXD_Common.inc

option casemap:none

; =============================================================================
;                    HOTPATCH KERNEL CONSTANTS
; =============================================================================

HP_TRAMPOLINE_SIZE      EQU     14          ; FF 25 [00 00 00 00] [8-byte addr]
HP_BACKUP_SIZE          EQU     16          ; Backup first 16 bytes of prologue
HP_SHADOW_PAGE_SIZE     EQU     65536       ; 64 KB shadow page allocation
HP_MAX_SNAPSHOTS        EQU     64          ; Max rollback snapshots

; CRC32 polynomial (IEEE 802.3)
HP_CRC32_POLY           EQU     0EDB88320h

; =============================================================================
;                    WIN32 API IMPORTS
; =============================================================================

EXTERNDEF VirtualAlloc:PROC
EXTERNDEF VirtualFree:PROC
EXTERNDEF VirtualProtect:PROC
EXTERNDEF FlushInstructionCache:PROC
EXTERNDEF GetCurrentProcess:PROC
EXTERNDEF GetTickCount64:PROC

; =============================================================================
;                    EXPORTS
; =============================================================================

PUBLIC asm_hotpatch_atomic_swap
PUBLIC asm_hotpatch_alloc_shadow
PUBLIC asm_hotpatch_free_shadow
PUBLIC asm_hotpatch_backup_prologue
PUBLIC asm_hotpatch_restore_prologue
PUBLIC asm_hotpatch_verify_prologue
PUBLIC asm_hotpatch_flush_icache
PUBLIC asm_hotpatch_get_stats

; =============================================================================
;                    DATA SECTION — Statistics
; =============================================================================

.data
ALIGN 8

; Hotpatch statistics (C++ reads this via asm_hotpatch_get_stats)
hp_stat_swaps_applied       QWORD   0
hp_stat_swaps_rolled_back   QWORD   0
hp_stat_swaps_failed        QWORD   0
hp_stat_shadow_pages_alloc  QWORD   0
hp_stat_shadow_pages_freed  QWORD   0
hp_stat_icache_flushes      QWORD   0
hp_stat_crc_checks          QWORD   0
hp_stat_crc_mismatches      QWORD   0
HP_STATS_SIZE               EQU     64      ; 8 QWORDs

; Prologue backup storage (HP_MAX_SNAPSHOTS * HP_BACKUP_SIZE = 1024 bytes)
hp_backup_table             BYTE    HP_MAX_SNAPSHOTS * HP_BACKUP_SIZE DUP(0)
hp_backup_addrs             QWORD   HP_MAX_SNAPSHOTS DUP(0)
hp_backup_count             DWORD   0

; Pre-computed CRC32 lookup table (256 DWORDs = 1 KB)
hp_crc32_table              DWORD   256 DUP(0)
hp_crc32_initialized        DWORD   0

; =============================================================================
;                    CODE SECTION
; =============================================================================

.code

; =============================================================================
; asm_hotpatch_atomic_swap
; =============================================================================
; Atomically redirect a function by rewriting its prologue with a 14-byte
; absolute jump to the new (patched) implementation.
;
; RCX = TargetFunctionAddr (original function to detour)
; RDX = NewFunctionAddr    (patched replacement function)
;
; Returns: EAX = 0 on success, -1 on failure
;
; The 14-byte absolute jump encoding:
;   FF 25 00 00 00 00       ; JMP QWORD PTR [RIP+0]
;   <8-byte abs address>    ; 64-bit destination
;
; Before writing, we VirtualProtect the target to PAGE_EXECUTE_READWRITE,
; write the trampoline, restore protection, then flush the I-cache.
; =============================================================================
asm_hotpatch_atomic_swap PROC FRAME
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
    sub     rsp, 64                         ; Shadow space + locals
    .allocstack 64
    .endprolog

    ; Validate inputs
    test    rcx, rcx
    jz      @@swap_fail
    test    rdx, rdx
    jz      @@swap_fail

    mov     r12, rcx                        ; r12 = target function address
    mov     r13, rdx                        ; r13 = new function address

    ; --- Step 1: Build the 14-byte trampoline on the stack ---
    ; Opcode: FF 25 00 00 00 00 [64-bit addr]
    mov     word ptr [rsp + 32], 025FFh     ; FF 25
    mov     dword ptr [rsp + 34], 0         ; RIP-relative displacement = 0
    mov     qword ptr [rsp + 38], r13       ; 64-bit destination address

    ; --- Step 2: VirtualProtect to PAGE_EXECUTE_READWRITE ---
    lea     r9, [rsp + 48]                  ; &oldProtect
    mov     r8d, PAGE_EXECUTE_READWRITE
    mov     rdx, HP_TRAMPOLINE_SIZE         ; size = 14
    mov     rcx, r12                        ; address = target function
    call    VirtualProtect
    test    eax, eax
    jz      @@swap_fail

    mov     r14d, dword ptr [rsp + 48]      ; Save oldProtect for restore

    ; --- Step 3: Write the trampoline (14 bytes) ---
    ; We write in two moves to minimize the window where the instruction
    ; stream is partially written. The mutex in C++ ensures no other thread
    ; is executing this prologue.
    ;
    ; First 8 bytes: FF 25 00 00 00 00 XX XX (opcode + low 2 bytes of addr)
    mov     rbx, qword ptr [rsp + 32]
    mov     qword ptr [r12], rbx

    ; Remaining 6 bytes of the 8-byte address
    mov     rbx, qword ptr [rsp + 40]
    mov     qword ptr [r12 + 8], rbx

    ; MFENCE — ensure all stores are globally visible before I-cache flush
    mfence

    ; --- Step 4: CLFLUSH the patched cache lines ---
    ; Ensures L1d → L2 → L3 coherence before I-cache sees new instructions
    clflush [r12]
    clflush [r12 + 8]
    mfence

    ; --- Step 5: Restore original memory protection ---
    lea     r9, [rsp + 52]                  ; &dummy (don't care about returned old)
    mov     r8d, r14d                       ; oldProtect
    mov     rdx, HP_TRAMPOLINE_SIZE
    mov     rcx, r12
    call    VirtualProtect
    ; Ignore failure here — protection restore is best-effort

    ; --- Step 6: FlushInstructionCache ---
    call    GetCurrentProcess
    mov     rcx, rax                        ; hProcess
    mov     rdx, r12                        ; lpBaseAddress
    mov     r8, HP_TRAMPOLINE_SIZE          ; dwSize
    call    FlushInstructionCache

    ; --- Step 7: Update statistics ---
    lock inc qword ptr [hp_stat_swaps_applied]
    lock inc qword ptr [hp_stat_icache_flushes]

    ; Return success
    xor     eax, eax
    jmp     @@swap_done

@@swap_fail:
    lock inc qword ptr [hp_stat_swaps_failed]
    mov     eax, -1

@@swap_done:
    add     rsp, 64
    pop     r14
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret
asm_hotpatch_atomic_swap ENDP

; =============================================================================
; asm_hotpatch_alloc_shadow
; =============================================================================
; Allocate a shadow page (RWX executable memory) for storing patched code.
;
; RCX = desired size (0 = default HP_SHADOW_PAGE_SIZE)
;
; Returns: RAX = base address of shadow page, or 0 on failure
; =============================================================================
asm_hotpatch_alloc_shadow PROC FRAME
    push    rbx
    .pushreg rbx
    sub     rsp, 48
    .allocstack 48
    .endprolog

    ; Default size if 0
    test    rcx, rcx
    jnz     @@has_size
    mov     rcx, HP_SHADOW_PAGE_SIZE
@@has_size:
    mov     rbx, rcx                        ; rbx = size

    ; VirtualAlloc(NULL, size, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE)
    mov     r9d, PAGE_EXECUTE_READWRITE
    mov     r8d, MEM_COMMIT OR MEM_RESERVE
    mov     rdx, rbx                        ; dwSize
    xor     rcx, rcx                        ; lpAddress = NULL
    call    VirtualAlloc

    ; RAX = base address (or NULL on failure)
    test    rax, rax
    jz      @@alloc_fail

    ; Zero the page (security: no stale opcodes)
    mov     rdi, rax
    push    rax                             ; save base for return
    mov     rcx, rbx
    xor     al, al
    rep     stosb
    pop     rax

    lock inc qword ptr [hp_stat_shadow_pages_alloc]
    jmp     @@alloc_done

@@alloc_fail:
    xor     rax, rax

@@alloc_done:
    add     rsp, 48
    pop     rbx
    ret
asm_hotpatch_alloc_shadow ENDP

; =============================================================================
; asm_hotpatch_free_shadow
; =============================================================================
; Release a shadow page allocated by asm_hotpatch_alloc_shadow.
;
; RCX = base address of shadow page
; RDX = size of shadow page (0 = use MEM_RELEASE which ignores size)
;
; Returns: EAX = 0 on success, -1 on failure
; =============================================================================
asm_hotpatch_free_shadow PROC FRAME
    sub     rsp, 40
    .allocstack 40
    .endprolog

    test    rcx, rcx
    jz      @@free_fail

    ; VirtualFree(addr, 0, MEM_RELEASE)
    mov     r8d, MEM_RELEASE
    xor     rdx, rdx                        ; dwSize = 0 for MEM_RELEASE
    ; rcx already = lpAddress
    call    VirtualFree

    test    eax, eax
    jz      @@free_fail

    lock inc qword ptr [hp_stat_shadow_pages_freed]
    xor     eax, eax
    jmp     @@free_done

@@free_fail:
    mov     eax, -1

@@free_done:
    add     rsp, 40
    ret
asm_hotpatch_free_shadow ENDP

; =============================================================================
; asm_hotpatch_backup_prologue
; =============================================================================
; Save the original prologue bytes of a function before hotpatching.
; Used for rollback to "Known Good" state.
;
; RCX = function address to back up
; RDX = snapshot slot index (0..HP_MAX_SNAPSHOTS-1)
;
; Returns: EAX = 0 on success, -1 on failure
; =============================================================================
asm_hotpatch_backup_prologue PROC FRAME
    push    rsi
    .pushreg rsi
    push    rdi
    .pushreg rdi
    sub     rsp, 32
    .allocstack 32
    .endprolog

    ; Validate inputs
    test    rcx, rcx
    jz      @@backup_fail
    cmp     edx, HP_MAX_SNAPSHOTS
    jae     @@backup_fail

    ; Calculate backup table offset: slot * HP_BACKUP_SIZE
    mov     eax, edx
    shl     eax, 4                          ; * 16 (HP_BACKUP_SIZE)
    lea     rdi, [hp_backup_table]
    add     rdi, rax                        ; rdi = &hp_backup_table[slot]

    ; Store the source address in the address table
    lea     rsi, [hp_backup_addrs]
    mov     qword ptr [rsi + rdx * 8], rcx

    ; Copy HP_BACKUP_SIZE (16) bytes from function to backup
    mov     rsi, rcx                        ; source = function address
    mov     ecx, HP_BACKUP_SIZE
    rep     movsb

    ; Track backup count (max high-water mark)
    mov     eax, edx
    inc     eax
    lock cmpxchg dword ptr [hp_backup_count], eax
    ; Ignore failure — high-water mark may already be higher

    xor     eax, eax
    jmp     @@backup_done

@@backup_fail:
    mov     eax, -1

@@backup_done:
    add     rsp, 32
    pop     rdi
    pop     rsi
    ret
asm_hotpatch_backup_prologue ENDP

; =============================================================================
; asm_hotpatch_restore_prologue
; =============================================================================
; Restore a function's original prologue from the backup table (rollback).
;
; RCX = snapshot slot index (0..HP_MAX_SNAPSHOTS-1)
;
; Returns: EAX = 0 on success, -1 on failure
; =============================================================================
asm_hotpatch_restore_prologue PROC FRAME
    push    rbx
    .pushreg rbx
    push    rsi
    .pushreg rsi
    push    rdi
    .pushreg rdi
    push    r12
    .pushreg r12
    sub     rsp, 48
    .allocstack 48
    .endprolog

    ; Validate slot index
    cmp     ecx, HP_MAX_SNAPSHOTS
    jae     @@restore_fail

    mov     r12d, ecx                       ; r12 = slot index

    ; Retrieve the original function address
    lea     rax, [hp_backup_addrs]
    mov     rdi, qword ptr [rax + r12 * 8]  ; rdi = target function address
    test    rdi, rdi
    jz      @@restore_fail

    ; Calculate backup table offset
    mov     eax, r12d
    shl     eax, 4                          ; * 16
    lea     rsi, [hp_backup_table]
    add     rsi, rax                        ; rsi = &saved prologue bytes

    ; VirtualProtect target to writable
    lea     r9, [rsp + 32]                  ; &oldProtect
    mov     r8d, PAGE_EXECUTE_READWRITE
    mov     rdx, HP_BACKUP_SIZE
    mov     rcx, rdi
    call    VirtualProtect
    test    eax, eax
    jz      @@restore_fail

    mov     ebx, dword ptr [rsp + 32]       ; save oldProtect

    ; Copy backed-up bytes back to function
    mov     rcx, HP_BACKUP_SIZE
    ; rsi already points to backup, rdi already points to function
    rep     movsb

    ; MFENCE + CLFLUSH for cache coherence
    ; Recompute rdi (it was advanced by movsb)
    sub     rdi, HP_BACKUP_SIZE
    mfence
    clflush [rdi]
    clflush [rdi + 8]
    mfence

    ; Restore protection
    lea     r9, [rsp + 36]
    mov     r8d, ebx
    mov     rdx, HP_BACKUP_SIZE
    mov     rcx, rdi
    call    VirtualProtect

    ; FlushInstructionCache
    call    GetCurrentProcess
    mov     rcx, rax
    mov     rdx, rdi
    mov     r8, HP_BACKUP_SIZE
    call    FlushInstructionCache

    lock inc qword ptr [hp_stat_swaps_rolled_back]
    lock inc qword ptr [hp_stat_icache_flushes]

    xor     eax, eax
    jmp     @@restore_done

@@restore_fail:
    mov     eax, -1

@@restore_done:
    add     rsp, 48
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret
asm_hotpatch_restore_prologue ENDP

; =============================================================================
; asm_hotpatch_verify_prologue
; =============================================================================
; CRC32 verify that a function prologue matches an expected checksum.
; Used by the SelfRepairLoop to confirm patch integrity post-swap.
;
; RCX = function address
; EDX = expected CRC32 value
;
; Returns: EAX = 0 if CRC matches, -1 if mismatch
; =============================================================================
asm_hotpatch_verify_prologue PROC FRAME
    push    rbx
    .pushreg rbx
    push    rsi
    .pushreg rsi
    sub     rsp, 32
    .allocstack 32
    .endprolog

    test    rcx, rcx
    jz      @@verify_fail

    mov     rsi, rcx                        ; rsi = function address
    mov     ebx, edx                        ; ebx = expected CRC

    ; Initialize CRC32 table if needed
    cmp     dword ptr [hp_crc32_initialized], 1
    je      @@crc_ready
    call    hp_init_crc32_table
@@crc_ready:

    ; Compute CRC32 over HP_BACKUP_SIZE bytes
    mov     ecx, 0FFFFFFFFh                 ; initial CRC = -1
    xor     edx, edx                        ; byte counter

@@crc_loop:
    cmp     edx, HP_BACKUP_SIZE
    jge     @@crc_done_loop
    movzx   eax, byte ptr [rsi + rdx]
    xor     al, cl                          ; al = (crc ^ byte) & 0xFF
    movzx   eax, al
    lea     r8, [hp_crc32_table]
    mov     eax, dword ptr [r8 + rax * 4]   ; table lookup
    shr     ecx, 8                          ; crc >>= 8
    xor     ecx, eax                        ; crc ^= table[index]
    inc     edx
    jmp     @@crc_loop
@@crc_done_loop:

    xor     ecx, 0FFFFFFFFh                 ; finalize CRC

    lock inc qword ptr [hp_stat_crc_checks]

    ; Compare
    cmp     ecx, ebx
    jne     @@verify_mismatch

    xor     eax, eax                        ; match
    jmp     @@verify_done

@@verify_mismatch:
    lock inc qword ptr [hp_stat_crc_mismatches]
@@verify_fail:
    mov     eax, -1

@@verify_done:
    add     rsp, 32
    pop     rsi
    pop     rbx
    ret
asm_hotpatch_verify_prologue ENDP

; =============================================================================
; asm_hotpatch_flush_icache
; =============================================================================
; Flush instruction cache for a memory region after hotpatching.
;
; RCX = base address
; RDX = size in bytes
;
; Returns: EAX = 0 on success, -1 on failure
; =============================================================================
asm_hotpatch_flush_icache PROC FRAME
    push    r12
    .pushreg r12
    push    r13
    .pushreg r13
    sub     rsp, 40
    .allocstack 40
    .endprolog

    test    rcx, rcx
    jz      @@flush_fail

    mov     r12, rcx                        ; base address
    mov     r13, rdx                        ; size

    ; CLFLUSH each cache line (64-byte stride)
    xor     rcx, rcx
@@flush_cl_loop:
    cmp     rcx, r13
    jge     @@flush_cl_done
    clflush [r12 + rcx]
    add     rcx, 64
    jmp     @@flush_cl_loop
@@flush_cl_done:
    mfence

    ; System FlushInstructionCache
    call    GetCurrentProcess
    mov     rcx, rax
    mov     rdx, r12
    mov     r8, r13
    call    FlushInstructionCache

    lock inc qword ptr [hp_stat_icache_flushes]

    xor     eax, eax
    jmp     @@flush_done

@@flush_fail:
    mov     eax, -1

@@flush_done:
    add     rsp, 40
    pop     r13
    pop     r12
    ret
asm_hotpatch_flush_icache ENDP

; =============================================================================
; asm_hotpatch_get_stats
; =============================================================================
; Copy hotpatch statistics to caller-provided buffer.
;
; RCX = pointer to 64-byte output buffer (8 QWORDs)
;
; Returns: EAX = 0 on success, -1 if rcx == NULL
; =============================================================================
asm_hotpatch_get_stats PROC
    test    rcx, rcx
    jz      @@stats_fail

    lea     rax, [hp_stat_swaps_applied]
    mov     rdx, qword ptr [rax]
    mov     qword ptr [rcx], rdx

    lea     rax, [hp_stat_swaps_rolled_back]
    mov     rdx, qword ptr [rax]
    mov     qword ptr [rcx + 8], rdx

    lea     rax, [hp_stat_swaps_failed]
    mov     rdx, qword ptr [rax]
    mov     qword ptr [rcx + 16], rdx

    lea     rax, [hp_stat_shadow_pages_alloc]
    mov     rdx, qword ptr [rax]
    mov     qword ptr [rcx + 24], rdx

    lea     rax, [hp_stat_shadow_pages_freed]
    mov     rdx, qword ptr [rax]
    mov     qword ptr [rcx + 32], rdx

    lea     rax, [hp_stat_icache_flushes]
    mov     rdx, qword ptr [rax]
    mov     qword ptr [rcx + 40], rdx

    lea     rax, [hp_stat_crc_checks]
    mov     rdx, qword ptr [rax]
    mov     qword ptr [rcx + 48], rdx

    lea     rax, [hp_stat_crc_mismatches]
    mov     rdx, qword ptr [rax]
    mov     qword ptr [rcx + 56], rdx

    xor     eax, eax
    ret

@@stats_fail:
    mov     eax, -1
    ret
asm_hotpatch_get_stats ENDP

; =============================================================================
; hp_init_crc32_table  (internal helper, not exported)
; =============================================================================
; Pre-compute the IEEE 802.3 CRC32 lookup table (256 DWORDs).
; Called once lazily from asm_hotpatch_verify_prologue.
; =============================================================================
hp_init_crc32_table PROC
    push    rbx
    push    rsi

    lea     rsi, [hp_crc32_table]
    xor     ecx, ecx                        ; i = 0

@@table_outer:
    cmp     ecx, 256
    jge     @@table_done

    mov     eax, ecx                        ; crc = i
    xor     edx, edx                        ; j = 0

@@table_inner:
    cmp     edx, 8
    jge     @@table_inner_done

    test    eax, 1
    jz      @@table_no_xor
    shr     eax, 1
    xor     eax, HP_CRC32_POLY
    jmp     @@table_next_bit
@@table_no_xor:
    shr     eax, 1
@@table_next_bit:
    inc     edx
    jmp     @@table_inner

@@table_inner_done:
    mov     dword ptr [rsi + rcx * 4], eax
    inc     ecx
    jmp     @@table_outer

@@table_done:
    mov     dword ptr [hp_crc32_initialized], 1

    pop     rsi
    pop     rbx
    ret
hp_init_crc32_table ENDP

END
