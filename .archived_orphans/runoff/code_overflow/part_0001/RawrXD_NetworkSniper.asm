; =============================================================================
; RawrXD_NetworkSniper.asm — Runtime Traffic Inspection Hooks
; Installs into active relay connections via hotpatch trampolines
; =============================================================================

INCLUDE ksamd64.inc

EXTRN   __imp_RtlCompareMemory:QWORD
EXTRN   __imp_RtlCopyMemory:QWORD
EXTRN   RelayEngine_RunBiDirectional:PROC

; -----------------------------------------------------------------------------
; Sniper Types
; -----------------------------------------------------------------------------
SNIPER_TYPE_INSPECT       EQU 0       ; Read-only analysis
SNIPER_TYPE_MUTATE        EQU 1       ; Modify payload in-flight
SNIPER_TYPE_BLOCK         EQU 2       ; Drop matching packets
SNIPER_TYPE_MIRROR        EQU 3       ; Copy to analysis buffer

; -----------------------------------------------------------------------------
; Sniper Context offsets (flat EQU - avoids STRUCT packing issues)
; -----------------------------------------------------------------------------
SCTX_Type              EQU 0        ; DD
SCTX_PatternPtr        EQU 8        ; DQ (aligned)
SCTX_PatternLen        EQU 16       ; DD
SCTX_ReplacementPtr    EQU 24       ; DQ (aligned)
SCTX_ReplacementLen    EQU 32       ; DD
SCTX_HitCount          EQU 40       ; DQ
SCTX_ByteCounter       EQU 48       ; DQ
SCTX_Flags             EQU 56       ; DD
SIZEOF_SCTX            EQU 64

; -----------------------------------------------------------------------------
; Trampoline Templates
; -----------------------------------------------------------------------------

; ─── Cross-module symbol resolution ───
INCLUDE rawrxd_master.inc

.data?
align 16
g_SniperTable           DQ      64 DUP(?)   ; 64 sniper slots
g_SniperActiveMask      DQ      ?           ; Bitmask of active snipers

.code

; =============================================================================
; Hotpatch Installation
; =============================================================================

; -----------------------------------------------------------------------------
; Sniper_InstallAtOffset — Patch relay buffer processing
; rcx = ConnectionContext, rdx = SniperContextPtr, r8 = OffsetInRelayProc
; -----------------------------------------------------------------------------
PUBLIC Sniper_InstallAtOffset
Sniper_InstallAtOffset PROC FRAME
    push    rbp
    .pushreg rbp
    mov     rbp, rsp
    .setframe rbp, 0
    push    rbx
    .pushreg rbx
    push    rdi
    .pushreg rdi
    push    rsi
    .pushreg rsi
    sub     rsp, 40
    .allocstack 40
    .endprolog

    mov     rbx, rcx                    ; Connection
    mov     rsi, rdx                    ; Sniper config
    mov     edi, r8d                    ; Offset

    ; Find free sniper slot atomically
    xor     rax, rax
@@find_slot:
    cmp     rax, 64
    jae     @@error_full
    bt      [g_SniperActiveMask], rax
    jnc     @@found
    inc     rax
    jmp     @@find_slot

@@found:
    ; Set bit
    lock bts [g_SniperActiveMask], rax

    ; Store context
    mov     [g_SniperTable + rax*8], rsi

    ; Patch the relay procedure at specified offset
    ; Calculate target: RelayEngine_RunBiDirectional + offset
    lea     rdi, [RelayEngine_RunBiDirectional + rdi]

    ; Generate trampoline: 14-byte absolute jump to SniperTrampolineDispatcher
    mov     WORD PTR [rdi], 025FFh      ; JMP [RIP+0]
    mov     DWORD PTR [rdi + 2], 0
    lea     rax, [SniperTrampolineDispatcher]
    mov     [rdi + 6], rax

    ; Store slot index in connection context for uninstall
    mov     [rbx + 56], rax             ; Assuming offset 56 is free in context

    xor     eax, eax                    ; Success

@@exit:
    add     rsp, 40
    pop     rsi
    pop     rdi
    pop     rbx
    pop     rbp
    ret

@@error_full:
    mov     eax, -1
    jmp     @@exit
Sniper_InstallAtOffset ENDP

; =============================================================================
; Sniper Dispatch Engine
; =============================================================================

; -----------------------------------------------------------------------------
; SniperTrampolineDispatcher — Called from hotpatched relay code
; Preserves all registers, inspects buffer, resumes original flow
; On entry: RBX = buffer ptr, RCX = length, RDI = direction (0=client->target, 1=target->client)
; -----------------------------------------------------------------------------
PUBLIC SniperTrampolineDispatcher
SniperTrampolineDispatcher PROC FRAME
    push    rax
    .pushreg rax
    push    rcx
    .pushreg rcx
    push    rdx
    .pushreg rdx
    push    r8
    .pushreg r8
    push    r9
    .pushreg r9
    push    r10
    .pushreg r10
    push    r11
    .pushreg r11
    sub     rsp, 40
    .allocstack 40
    .endprolog

    ; Save volatile regs
    mov     r10, rbx                    ; Buffer
    mov     r11d, ecx                   ; Length
    mov     r12d, edi                   ; Direction

    ; Iterate active snipers
    xor     r8, r8                      ; Slot index
    mov     r9, [g_SniperActiveMask]

@@sniper_loop:
    test    r9, r9
    jz      @@done                      ; No more snipers

    bsf     rax, r9                     ; Find first set bit
    btr     r9, rax                     ; Clear it
    mov     r8, rax

    ; Load sniper context
    mov     rsi, [g_SniperTable + r8*8]
    test    rsi, rsi
    jz      @@sniper_loop

    ; Check type
    mov     eax, [rsi + SCTX_Type]      ; Type offset
    cmp     eax, SNIPER_TYPE_INSPECT
    je      @@handle_inspect
    cmp     eax, SNIPER_TYPE_MUTATE
    je      @@handle_mutate
    cmp     eax, SNIPER_TYPE_BLOCK
    je      @@handle_block
    cmp     eax, SNIPER_TYPE_MIRROR
    je      @@handle_mirror
    jmp     @@sniper_loop

@@handle_inspect:
    ; Pattern scan
    mov     rcx, r10                    ; Haystack
    mov     edx, r11d                   ; Haystack len
    mov     r8, [rsi + SCTX_PatternPtr] ; PatternPtr
    mov     r9d, [rsi + SCTX_PatternLen]; PatternLen
    call    Sniper_ScanPattern
    test    rax, rax
    jz      @@sniper_loop

    ; Pattern found - increment hit counter
    lock inc QWORD PTR [rsi + SCTX_HitCount] ; HitCount
    jmp     @@sniper_loop

@@handle_mutate:
    ; Find and replace in buffer
    mov     rcx, r10
    mov     edx, r11d
    mov     r8, [rsi + SCTX_PatternPtr]     ; PatternPtr
    mov     r9d, [rsi + SCTX_PatternLen]    ; PatternLen
    mov     rax, [rsi + SCTX_ReplacementPtr] ; ReplacementPtr
    mov     ebx, [rsi + SCTX_ReplacementLen]  ; ReplacementLen
    push    rbx                         ; Replacement len (stack arg)
    push    rax                         ; Replacement ptr
    call    Sniper_MutateBuffer
    add     rsp, 16
    jmp     @@sniper_loop

@@handle_block:
    ; Check if pattern present - if so, block entire packet
    mov     rcx, r10
    mov     edx, r11d
    mov     r8, [rsi + SCTX_PatternPtr]     ; PatternPtr
    mov     r9d, [rsi + SCTX_PatternLen]    ; PatternLen
    call    Sniper_ScanPattern
    test    rax, rax
    jz      @@sniper_loop

    ; Block packet - return 0 length to relay (packet dropped)
    mov     r11d, 0

@@handle_mirror:
    ; Copy to secondary buffer for analysis (non-blocking)
    mov     rcx, [rsi + SCTX_PatternPtr]    ; Mirror buffer ptr (reusing PatternPtr)
    mov     rdx, r10
    mov     r8d, r11d
    cmp     r8d, 65536                  ; Cap at buffer size
    jbe     @@mirror_ok
    mov     r8d, 65536
@@mirror_ok:
    call    qword ptr [__imp_RtlCopyMemory]
    jmp     @@sniper_loop

@@done:
    ; Restore registers
    mov     rbx, r10
    mov     ecx, r11d
    mov     edi, r12d

    add     rsp, 40
    pop     r11
    pop     r10
    pop     r9
    pop     r8
    pop     rdx
    pop     rcx
    pop     rax

    ; Resume original relay flow
    ; (The hotpatch jumps here, we return to original code after patch site)
    ret
SniperTrampolineDispatcher ENDP

; =============================================================================
; Sniper Helper Functions
; =============================================================================

; -----------------------------------------------------------------------------
; Sniper_ScanPattern — Boyer-Moore-Horspool substring search
; rcx = Buffer, edx = BufLen, r8 = Pattern, r9d = PatLen
; Returns: RAX = pointer to match or 0
; -----------------------------------------------------------------------------
Sniper_ScanPattern PROC FRAME
    push    rbx
    .pushreg rbx
    push    rdi
    .pushreg rdi
    push    rsi
    .pushreg rsi
    .endprolog

    mov     rsi, rcx                    ; Buffer
    mov     ebx, edx                    ; BufLen
    mov     rdi, r8                     ; Pattern
    mov     ecx, r9d                    ; PatLen

    ; Validate lengths
    cmp     ebx, ecx
    jb      @@not_found
    test    ecx, ecx
    jz      @@not_found

    ; Compute skip table (simplified - use first/last char only for speed)
    movzx   eax, BYTE PTR [rdi]         ; First char
    movzx   edx, BYTE PTR [rdi + rcx - 1] ; Last char

    ; Search loop (simplified Boyer-Moore)
    mov     r8d, ebx
    sub     r8d, ecx                    ; Max offset
    xor     r9, r9                      ; Current offset

@@search_loop:
    cmp     r9d, r8d
    ja      @@not_found

    ; Check last char first (Boyer-Moore optimization)
    lea     r11, [rsi + r9]              ; base + offset
    add     r11, rcx
    dec     r11                          ; rsi + r9 + rcx - 1
    movzx   r10d, BYTE PTR [r11]
    cmp     r10d, edx
    jne     @@next_offset

    ; Check first char
    movzx   r10d, BYTE PTR [rsi + r9]
    cmp     r10d, eax
    jne     @@next_offset

    ; Full compare
    push    rcx
    push    rdi
    push    rsi
    add     rsi, r9
    repe    cmpsb
    pop     rsi
    pop     rdi
    pop     rcx
    jne     @@next_offset

    ; Found
    lea     rax, [rsi + r9]
    jmp     @@exit

@@next_offset:
    inc     r9d
    jmp     @@search_loop

@@not_found:
    xor     eax, eax

@@exit:
    pop     rsi
    pop     rdi
    pop     rbx
    ret
Sniper_ScanPattern ENDP

; -----------------------------------------------------------------------------
; Sniper_MutateBuffer — Find/replace in relay buffer
; rcx = Buffer, edx = BufLen, r8 = Pattern, r9d = PatLen
; [rsp+40] = ReplacementPtr, [rsp+48] = ReplacementLen
; -----------------------------------------------------------------------------
Sniper_MutateBuffer PROC FRAME
    push    rbp
    .pushreg rbp
    mov     rbp, rsp
    .setframe rbp, 0
    push    rbx
    .pushreg rbx
    push    rdi
    .pushreg rdi
    push    rsi
    .pushreg rsi
    sub     rsp, 40
    .allocstack 40
    .endprolog

    mov     rsi, rcx
    mov     ebx, edx
    mov     rdi, r8
    mov     ecx, r9d

    mov     rax, [rbp + 56]             ; ReplacementPtr (after pushes)
    mov     r10d, [rbp + 64]            ; ReplacementLen

    ; Find pattern
    call    Sniper_ScanPattern
    test    rax, rax
    jz      @@exit

    ; Calculate lengths
    mov     r8, rax                     ; Match position
    sub     r8, rsi                     ; Offset into buffer
    mov     r9d, ebx
    sub     r9d, r8d                    ; Remaining after match

    ; Move tail to make room or collapse (simplified: overwrite in place)
    cmp     ecx, r10d
    jne     @@size_diff                 ; Different sizes - would need memmove

    ; Same size - direct overwrite
    mov     rdi, rax
    mov     rsi, [rbp + 56]
    mov     ecx, r10d
    rep     movsb
    jmp     @@exit

@@size_diff:
    ; For now, just truncate/pad to match size
    mov     rdi, rax
    mov     rsi, [rbp + 56]
    mov     ecx, r10d
    cmp     ecx, [rbp + 72]             ; Original pat len
    cmova   ecx, [rbp + 72]             ; Min of replacement vs pattern
    rep     movsb

@@exit:
    add     rsp, 40
    pop     rsi
    pop     rdi
    pop     rbx
    pop     rbp
    ret
Sniper_MutateBuffer ENDP

END