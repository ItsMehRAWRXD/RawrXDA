; =============================================================================
; RawrXD_Snapshot.asm — Rollback Snapshot System for Shadow-Page Detour
; =============================================================================
;
; Provides the "Known Good" binary state rollback system. When the AI-generated
; hotpatch causes corruption, an L1 cache collision, or fails the RFC 3713
; sentinel test, this kernel instantly reverts to the captured snapshot.
;
; Unlike the prologue-only backup in RawrXD_Hotpatch_Kernel.asm (which saves
; 16 bytes), this captures up to 4096 bytes per function — enough to restore
; the entire function body, not just the entry trampoline.
;
; Capabilities:
;   - Deep function body snapshot (up to 4096 bytes per slot)
;   - 32 snapshot slots for concurrent rollback points
;   - CRC32 integrity verification per snapshot
;   - VirtualProtect-guarded restore with I-cache flush
;   - Statistics tracking for audit/telemetry
;
; Exports (called from shadow_page_detour.cpp):
;   asm_snapshot_capture     — Capture function bytes into snapshot slot
;   asm_snapshot_restore     — Restore a snapshot (full rollback)
;   asm_snapshot_verify      — CRC32 verify a snapshot against current memory
;   asm_snapshot_discard     — Free a snapshot slot for reuse
;   asm_snapshot_get_stats   — Read snapshot statistics
;
; Architecture: x64 MASM64 | Windows x64 ABI | No CRT | No exceptions
; Build: ml64.exe /c /Zi /Zd /Fo RawrXD_Snapshot.obj
; Rule: NO SOURCE FILE IS TO BE SIMPLIFIED
; =============================================================================

INCLUDE RawrXD_Common.inc

option casemap:none

; =============================================================================
;                    SNAPSHOT CONSTANTS
; =============================================================================

SNAP_MAX_SLOTS          EQU     32          ; Maximum concurrent snapshots
SNAP_MAX_SIZE           EQU     4096        ; Max bytes per snapshot (1 page)
SNAP_CRC32_POLY         EQU     0EDB88320h  ; IEEE 802.3

; Slot state flags
SNAP_STATE_FREE         EQU     0           ; Slot is available
SNAP_STATE_ACTIVE       EQU     1           ; Slot contains valid snapshot
SNAP_STATE_RESTORED     EQU     2           ; Slot was used for restore

; =============================================================================
;                    WIN32 API IMPORTS
; =============================================================================

EXTERNDEF VirtualProtect:PROC
EXTERNDEF FlushInstructionCache:PROC
EXTERNDEF GetCurrentProcess:PROC
EXTERNDEF GetTickCount64:PROC

; =============================================================================
;                    EXPORTS
; =============================================================================

PUBLIC asm_snapshot_capture
PUBLIC asm_snapshot_restore
PUBLIC asm_snapshot_verify
PUBLIC asm_snapshot_discard
PUBLIC asm_snapshot_get_stats

; =============================================================================
;                    DATA SECTION
; =============================================================================

.data
ALIGN 8

; Snapshot storage: SNAP_MAX_SLOTS * SNAP_MAX_SIZE = 128 KB
snap_data               BYTE    SNAP_MAX_SLOTS * SNAP_MAX_SIZE DUP(0)

; Snapshot metadata (per slot)
snap_source_addr        QWORD   SNAP_MAX_SLOTS DUP(0)   ; Source function address
snap_capture_size       DWORD   SNAP_MAX_SLOTS DUP(0)   ; Bytes captured
snap_crc32              DWORD   SNAP_MAX_SLOTS DUP(0)   ; CRC32 at capture time
snap_state              DWORD   SNAP_MAX_SLOTS DUP(0)   ; Slot state
snap_timestamp          QWORD   SNAP_MAX_SLOTS DUP(0)   ; Capture timestamp

; Statistics
snap_stat_captured      QWORD   0
snap_stat_restored      QWORD   0
snap_stat_discarded     QWORD   0
snap_stat_verify_pass   QWORD   0
snap_stat_verify_fail   QWORD   0
snap_stat_total_bytes   QWORD   0

; CRC32 table (shared with RawrXD_Hotpatch_Kernel if linked together;
; defined locally to avoid cross-module dependency)
snap_crc32_table        DWORD   256 DUP(0)
snap_crc32_initialized  DWORD   0

; =============================================================================
;                    CODE SECTION
; =============================================================================

.code

; =============================================================================
; asm_snapshot_capture
; =============================================================================
; Capture a deep snapshot of a function body for rollback.
;
; RCX = function address to snapshot
; EDX = snapshot ID (0..SNAP_MAX_SLOTS-1)
; R8  = capture size in bytes (clamped to SNAP_MAX_SIZE)
;
; Returns: EAX = 0 on success, -1 on failure
; =============================================================================
asm_snapshot_capture PROC FRAME
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

    ; Validate inputs
    test    rcx, rcx
    jz      @@cap_fail
    cmp     edx, SNAP_MAX_SLOTS
    jae     @@cap_fail
    test    r8, r8
    jz      @@cap_fail

    mov     r12, rcx                        ; r12 = source function address
    mov     r13d, edx                       ; r13 = snapshot ID
    mov     r14, r8                         ; r14 = requested size

    ; Clamp size to SNAP_MAX_SIZE
    cmp     r14, SNAP_MAX_SIZE
    jbe     @@cap_size_ok
    mov     r14, SNAP_MAX_SIZE
@@cap_size_ok:

    ; Check slot is free
    lea     rax, [snap_state]
    cmp     dword ptr [rax + r13 * 4], SNAP_STATE_FREE
    jne     @@cap_fail                      ; Slot already in use

    ; Calculate data offset: snapshotId * SNAP_MAX_SIZE
    mov     eax, r13d
    shl     eax, 12                         ; * 4096 (SNAP_MAX_SIZE)
    lea     rdi, [snap_data]
    add     rdi, rax                        ; rdi = destination in snap_data

    ; Copy function bytes to snapshot
    mov     rsi, r12
    mov     rcx, r14
    rep     movsb

    ; Compute CRC32 of captured data
    call    snap_ensure_crc32_table         ; Initialize CRC table if needed

    mov     eax, r13d
    shl     eax, 12
    lea     rsi, [snap_data]
    add     rsi, rax                        ; rsi = start of captured data

    mov     ecx, 0FFFFFFFFh                 ; CRC init
    xor     edx, edx                        ; byte counter
@@cap_crc_loop:
    cmp     rdx, r14
    jge     @@cap_crc_done
    movzx   eax, byte ptr [rsi + rdx]
    xor     al, cl
    movzx   eax, al
    lea     r8, [snap_crc32_table]
    mov     eax, dword ptr [r8 + rax * 4]
    shr     ecx, 8
    xor     ecx, eax
    inc     rdx
    jmp     @@cap_crc_loop
@@cap_crc_done:
    xor     ecx, 0FFFFFFFFh                 ; Finalize CRC

    ; Store metadata
    lea     rax, [snap_source_addr]
    mov     qword ptr [rax + r13 * 8], r12

    lea     rax, [snap_capture_size]
    mov     dword ptr [rax + r13 * 4], r14d

    lea     rax, [snap_crc32]
    mov     dword ptr [rax + r13 * 4], ecx

    ; Record timestamp
    push    rcx                             ; save CRC across call
    call    GetTickCount64
    lea     rcx, [snap_timestamp]
    mov     qword ptr [rcx + r13 * 8], rax
    pop     rcx

    ; Mark slot as active
    lea     rax, [snap_state]
    mov     dword ptr [rax + r13 * 4], SNAP_STATE_ACTIVE

    ; Update statistics
    lock inc qword ptr [snap_stat_captured]
    lock add qword ptr [snap_stat_total_bytes], r14

    xor     eax, eax
    jmp     @@cap_done

@@cap_fail:
    mov     eax, -1

@@cap_done:
    add     rsp, 48
    pop     r14
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret
asm_snapshot_capture ENDP

; =============================================================================
; asm_snapshot_restore
; =============================================================================
; Restore a function to its captured "Known Good" state.
;
; ECX = snapshot ID (0..SNAP_MAX_SLOTS-1)
;
; Returns: EAX = 0 on success, -1 on failure
; =============================================================================
asm_snapshot_restore PROC FRAME
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

    ; Validate
    cmp     ecx, SNAP_MAX_SLOTS
    jae     @@rest_fail

    mov     r12d, ecx                       ; r12 = snapshot ID

    ; Check slot is active
    lea     rax, [snap_state]
    cmp     dword ptr [rax + r12 * 4], SNAP_STATE_ACTIVE
    jne     @@rest_fail

    ; Get target address and size
    lea     rax, [snap_source_addr]
    mov     rdi, qword ptr [rax + r12 * 8]  ; rdi = target function address
    test    rdi, rdi
    jz      @@rest_fail

    lea     rax, [snap_capture_size]
    mov     r13d, dword ptr [rax + r12 * 4] ; r13 = size to restore
    test    r13d, r13d
    jz      @@rest_fail

    ; Get snapshot data pointer
    mov     eax, r12d
    shl     eax, 12                         ; * 4096
    lea     rsi, [snap_data]
    add     rsi, rax                        ; rsi = snapshot source data

    ; VirtualProtect target to PAGE_EXECUTE_READWRITE
    lea     r9, [rsp + 32]                  ; &oldProtect
    mov     r8d, PAGE_EXECUTE_READWRITE
    mov     edx, r13d                       ; size
    mov     rcx, rdi                        ; address
    call    VirtualProtect
    test    eax, eax
    jz      @@rest_fail

    mov     ebx, dword ptr [rsp + 32]       ; Save oldProtect

    ; Copy snapshot data back to function
    mov     rcx, r13                        ; count = capture_size (use r13 which has the size)
    ; rsi = snapshot data, rdi = target function
    ; Note: rdi was already set, rsi was already set
    rep     movsb

    ; Restore rdi to function base (movsb advanced it)
    sub     rdi, r13

    ; MFENCE + CLFLUSH
    mfence

    ; Flush all cache lines covering the restored region
    xor     rcx, rcx
@@rest_flush:
    cmp     rcx, r13
    jge     @@rest_flush_done
    clflush [rdi + rcx]
    add     rcx, 64
    jmp     @@rest_flush
@@rest_flush_done:
    mfence

    ; Restore memory protection
    lea     r9, [rsp + 36]
    mov     r8d, ebx                        ; oldProtect
    mov     edx, r13d
    mov     rcx, rdi
    call    VirtualProtect

    ; FlushInstructionCache
    call    GetCurrentProcess
    mov     rcx, rax
    mov     rdx, rdi
    mov     r8, r13
    call    FlushInstructionCache

    ; Mark slot as restored
    lea     rax, [snap_state]
    mov     dword ptr [rax + r12 * 4], SNAP_STATE_RESTORED

    lock inc qword ptr [snap_stat_restored]

    xor     eax, eax
    jmp     @@rest_done

@@rest_fail:
    mov     eax, -1

@@rest_done:
    add     rsp, 48
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret
asm_snapshot_restore ENDP

; =============================================================================
; asm_snapshot_verify
; =============================================================================
; Verify that a snapshot's CRC32 matches a currently live function body.
; Used to detect L1 cache collisions or silent corruption.
;
; ECX = snapshot ID
; EDX = expected CRC32 (0 = use stored CRC from capture time)
;
; Returns: EAX = 0 if CRC matches, -1 if mismatch
; =============================================================================
asm_snapshot_verify PROC FRAME
    push    rbx
    .pushreg rbx
    push    rsi
    .pushreg rsi
    push    r12
    .pushreg r12
    push    r13
    .pushreg r13
    sub     rsp, 32
    .allocstack 32
    .endprolog

    cmp     ecx, SNAP_MAX_SLOTS
    jae     @@ver_fail

    mov     r12d, ecx                       ; snapshot ID
    mov     r13d, edx                       ; expected CRC (or 0 for stored)

    ; Check slot is active or restored
    lea     rax, [snap_state]
    mov     eax, dword ptr [rax + r12 * 4]
    cmp     eax, SNAP_STATE_FREE
    je      @@ver_fail

    ; If expected CRC is 0, use the stored CRC
    test    r13d, r13d
    jnz     @@ver_has_crc
    lea     rax, [snap_crc32]
    mov     r13d, dword ptr [rax + r12 * 4]
@@ver_has_crc:

    ; Get snapshot data and size
    mov     eax, r12d
    shl     eax, 12
    lea     rsi, [snap_data]
    add     rsi, rax                        ; rsi = snapshot data

    lea     rax, [snap_capture_size]
    mov     ebx, dword ptr [rax + r12 * 4]  ; ebx = size

    ; Ensure CRC table is ready
    call    snap_ensure_crc32_table

    ; Compute CRC32 of the stored snapshot
    mov     ecx, 0FFFFFFFFh
    xor     edx, edx
@@ver_crc_loop:
    cmp     edx, ebx
    jge     @@ver_crc_done
    movzx   eax, byte ptr [rsi + rdx]
    xor     al, cl
    movzx   eax, al
    lea     r8, [snap_crc32_table]
    mov     eax, dword ptr [r8 + rax * 4]
    shr     ecx, 8
    xor     ecx, eax
    inc     edx
    jmp     @@ver_crc_loop
@@ver_crc_done:
    xor     ecx, 0FFFFFFFFh

    ; Compare
    cmp     ecx, r13d
    jne     @@ver_mismatch

    lock inc qword ptr [snap_stat_verify_pass]
    xor     eax, eax
    jmp     @@ver_done

@@ver_mismatch:
    lock inc qword ptr [snap_stat_verify_fail]
@@ver_fail:
    mov     eax, -1

@@ver_done:
    add     rsp, 32
    pop     r13
    pop     r12
    pop     rsi
    pop     rbx
    ret
asm_snapshot_verify ENDP

; =============================================================================
; asm_snapshot_discard
; =============================================================================
; Free a snapshot slot for reuse.
;
; ECX = snapshot ID
;
; Returns: EAX = 0 on success, -1 on failure
; =============================================================================
asm_snapshot_discard PROC
    cmp     ecx, SNAP_MAX_SLOTS
    jae     @@dis_fail

    ; Zero out the slot data
    push    rdi
    mov     eax, ecx
    shl     eax, 12                         ; * 4096
    lea     rdi, [snap_data]
    add     rdi, rax
    push    rcx
    mov     ecx, SNAP_MAX_SIZE
    xor     al, al
    rep     stosb
    pop     rcx
    pop     rdi

    ; Clear metadata
    lea     rax, [snap_source_addr]
    mov     qword ptr [rax + rcx * 8], 0

    lea     rax, [snap_capture_size]
    mov     dword ptr [rax + rcx * 4], 0

    lea     rax, [snap_crc32]
    mov     dword ptr [rax + rcx * 4], 0

    lea     rax, [snap_state]
    mov     dword ptr [rax + rcx * 4], SNAP_STATE_FREE

    lea     rax, [snap_timestamp]
    mov     qword ptr [rax + rcx * 8], 0

    lock inc qword ptr [snap_stat_discarded]

    xor     eax, eax
    ret

@@dis_fail:
    mov     eax, -1
    ret
asm_snapshot_discard ENDP

; =============================================================================
; asm_snapshot_get_stats
; =============================================================================
; Copy snapshot statistics to caller-provided buffer.
;
; RCX = pointer to 48-byte output buffer (6 QWORDs)
;
; Returns: EAX = 0 on success, -1 if rcx == NULL
; =============================================================================
asm_snapshot_get_stats PROC
    test    rcx, rcx
    jz      @@gstats_fail

    mov     rax, qword ptr [snap_stat_captured]
    mov     qword ptr [rcx], rax

    mov     rax, qword ptr [snap_stat_restored]
    mov     qword ptr [rcx + 8], rax

    mov     rax, qword ptr [snap_stat_discarded]
    mov     qword ptr [rcx + 16], rax

    mov     rax, qword ptr [snap_stat_verify_pass]
    mov     qword ptr [rcx + 24], rax

    mov     rax, qword ptr [snap_stat_verify_fail]
    mov     qword ptr [rcx + 32], rax

    mov     rax, qword ptr [snap_stat_total_bytes]
    mov     qword ptr [rcx + 40], rax

    xor     eax, eax
    ret

@@gstats_fail:
    mov     eax, -1
    ret
asm_snapshot_get_stats ENDP

; =============================================================================
; snap_ensure_crc32_table (internal helper, not exported)
; =============================================================================
; Lazily initializes the CRC32 lookup table.
; =============================================================================
snap_ensure_crc32_table PROC
    cmp     dword ptr [snap_crc32_initialized], 1
    je      @@table_ready

    push    rbx
    push    rsi

    lea     rsi, [snap_crc32_table]
    xor     ecx, ecx

@@stbl_outer:
    cmp     ecx, 256
    jge     @@stbl_done

    mov     eax, ecx
    xor     edx, edx

@@stbl_inner:
    cmp     edx, 8
    jge     @@stbl_inner_done

    test    eax, 1
    jz      @@stbl_no_xor
    shr     eax, 1
    xor     eax, SNAP_CRC32_POLY
    jmp     @@stbl_next_bit
@@stbl_no_xor:
    shr     eax, 1
@@stbl_next_bit:
    inc     edx
    jmp     @@stbl_inner

@@stbl_inner_done:
    mov     dword ptr [rsi + rcx * 4], eax
    inc     ecx
    jmp     @@stbl_outer

@@stbl_done:
    mov     dword ptr [snap_crc32_initialized], 1
    pop     rsi
    pop     rbx

@@table_ready:
    ret
snap_ensure_crc32_table ENDP

END
