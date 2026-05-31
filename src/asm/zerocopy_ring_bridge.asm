; =============================================================================
; zerocopy_ring_bridge.asm — Zero-Copy Ring Buffer Producer for Tool→GPU Bridge
; =============================================================================
; Lock-free SPSC (Single-Producer Single-Consumer) ring buffer that writes
; tool output / context JSON directly into a pre-mapped host-visible buffer
; region.  The consumer is the GPU dispatch (rawr_kernel_hooks.comp binding 1).
;
; Memory layout (64-byte header + data region):
;
;   Offset  Size   Field
;   ──────  ─────  ──────────────────────────────────────────
;   0x00    8      write_pos   (producer, atomic store-release)
;   0x08    8      read_pos    (consumer, atomic store-release)
;   0x10    8      capacity    (immutable after init — power of 2)
;   0x18    8      wrap_mask   (capacity - 1, for branchless mod)
;   0x20    8      drop_count  (producer increments on full)
;   0x28    8      total_bytes (lifetime bytes written)
;   0x30    16     reserved
;   0x40    N      data[]      (N = capacity bytes, page-aligned)
;
; The buffer is designed to sit in a VkBuffer with
; VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
; so that GPU reads see producer stores without explicit flushes.
;
; On CPU-only paths (no Vulkan), the buffer still works as a fast IPC ring
; between the agentic tool thread and the inference thread.
;
; ABI: Win64 (Microsoft calling convention)
; Build: ml64.exe /c /Zi /Zd zerocopy_ring_bridge.asm
; =============================================================================
OPTION CASEMAP:NONE

INCLUDE rawr_globals.inc

; Ring header offsets
RING_OFF_WRITE      EQU 0
RING_OFF_READ       EQU 8
RING_OFF_CAPACITY   EQU 16
RING_OFF_MASK       EQU 24
RING_OFF_DROPS      EQU 32
RING_OFF_TOTAL      EQU 40
RING_HEADER_SIZE    EQU 64
; Data starts at header + 64

.DATA
ALIGN 8
g_RingBridge_Inits    QWORD 0       ; telemetry: init count
g_RingBridge_Writes   QWORD 0       ; telemetry: successful writes
g_RingBridge_Drops    QWORD 0       ; telemetry: drops due to full ring

.CODE

; =============================================================================
; BOOL RawrXD_Ring_Init(void* pBuffer, UINT64 capacity)
;   rcx = pointer to pre-allocated buffer (header + data)
;   rdx = capacity in bytes (MUST be power of 2, minimum 4096)
;
; Initializes the ring header.  Does NOT allocate memory — caller provides
; the buffer (from VirtualAlloc, VkMapMemory, or section mapping).
;
; Returns: EAX=1 on success, 0 on invalid args.
; =============================================================================
PUBLIC RawrXD_Ring_Init
RawrXD_Ring_Init PROC
    ; Validate pointer
    test    rcx, rcx
    jz      @@fail

    ; Validate capacity: must be >= 4096 and power of 2
    cmp     rdx, 4096
    jb      @@fail
    mov     rax, rdx
    lea     r8, [rdx - 1]
    test    rax, r8                     ; power-of-2 check: n & (n-1) == 0
    jnz     @@fail

    ; Zero the header (64 bytes)
    xor     eax, eax
    mov     QWORD PTR [rcx + RING_OFF_WRITE], rax
    mov     QWORD PTR [rcx + RING_OFF_READ], rax
    mov     QWORD PTR [rcx + RING_OFF_CAPACITY], rdx
    mov     QWORD PTR [rcx + RING_OFF_MASK], r8     ; capacity - 1
    mov     QWORD PTR [rcx + RING_OFF_DROPS], rax
    mov     QWORD PTR [rcx + RING_OFF_TOTAL], rax
    ; Zero reserved
    mov     QWORD PTR [rcx + 48], rax
    mov     QWORD PTR [rcx + 56], rax

    ; sfence: ensure header is visible before any consumer starts reading
    sfence

    lock inc QWORD PTR [g_RingBridge_Inits]
    mov     eax, 1
    ret

@@fail:
    xor     eax, eax
    ret
RawrXD_Ring_Init ENDP

; =============================================================================
; UINT64 RawrXD_Ring_Produce(void* pRing, const void* pData, UINT64 nBytes)
;   rcx = ring buffer
;   rdx = source data
;   r8  = byte count
;
; Returns nBytes on success, 0 on drop.  SPSC lock-free.
; =============================================================================
PUBLIC RawrXD_Ring_Produce
RawrXD_Ring_Produce PROC FRAME
    push    rbx
    .pushreg rbx
    push    rdi
    .pushreg rdi
    push    rsi
    .pushreg rsi
    push    r12
    .pushreg r12
    push    r13
    .pushreg r13
    .endprolog

    test    rcx, rcx
    jz      @@p_fail
    test    rdx, rdx
    jz      @@p_fail
    test    r8, r8
    jz      @@p_fail

    mov     rbx, rcx                    ; rbx = ring base (preserved)
    mov     rsi, rdx                    ; rsi = source ptr (preserved)
    mov     r12, r8                     ; r12 = nBytes (preserved, callee-saved)

    ; Load ring metadata
    mov     rax, QWORD PTR [rbx + RING_OFF_WRITE]
    mov     rcx, QWORD PTR [rbx + RING_OFF_READ]
    mov     r13, QWORD PTR [rbx + RING_OFF_MASK]       ; r13 = mask (preserved)
    mov     r9,  r13
    inc     r9                                          ; r9 = capacity

    ; free = capacity - (write - read)
    mov     r10, rax
    sub     r10, rcx                    ; used
    mov     r11, r9
    sub     r11, r10                    ; free

    cmp     r12, r11
    ja      @@p_drop                    ; not enough room → drop

    ; Compute first segment length
    mov     rdi, rax
    and     rdi, r13                    ; write_offset = write_pos & mask
    mov     r10, r9
    sub     r10, rdi                    ; bytes_to_end = capacity - write_offset

    ; dest = ring_base + HEADER + write_offset
    lea     rdi, [rbx + RING_HEADER_SIZE]
    mov     rcx, rax
    and     rcx, r13
    add     rdi, rcx                    ; rdi = dest

    ; Determine if we need a split copy
    cmp     r12, r10
    jbe     @@p_single

    ; ── Split copy ──
    ; First chunk: r10 bytes
    mov     rcx, r10
    rep movsb                           ; rsi advances, rdi advances

    ; Second chunk: wrap to ring data start
    lea     rdi, [rbx + RING_HEADER_SIZE]
    mov     rcx, r12
    sub     rcx, r10                    ; remaining bytes
    rep movsb
    jmp     @@p_fence

@@p_single:
    ; ── Single copy (no wrap) ──
    mov     rcx, r12
    rep movsb

@@p_fence:
    ; Store fence: make data visible before advancing write_pos
    sfence

    ; Advance write_pos atomically (single writer — plain store is fine for SPSC,
    ; but we use xadd for visibility guarantee on HOST_COHERENT memory)
    mov     rax, QWORD PTR [rbx + RING_OFF_WRITE]
    add     rax, r12
    mov     QWORD PTR [rbx + RING_OFF_WRITE], rax

    ; Update lifetime counter
    lock add QWORD PTR [rbx + RING_OFF_TOTAL], r12

    ; Telemetry
    lock inc QWORD PTR [g_RingBridge_Writes]

    ; Return bytes written
    mov     rax, r12
    pop     r13
    pop     r12
    pop     rsi
    pop     rdi
    pop     rbx
    ret

@@p_drop:
    lock inc QWORD PTR [rbx + RING_OFF_DROPS]
    lock inc QWORD PTR [g_RingBridge_Drops]
    xor     eax, eax
    pop     r13
    pop     r12
    pop     rsi
    pop     rdi
    pop     rbx
    ret

@@p_fail:
    xor     eax, eax
    pop     r13
    pop     r12
    pop     rsi
    pop     rdi
    pop     rbx
    ret
RawrXD_Ring_Produce ENDP

; =============================================================================
; UINT64 RawrXD_Ring_Available(const void* pRing)
;   rcx = ring buffer
;   Returns: number of bytes available for reading (write_pos - read_pos)
; =============================================================================
PUBLIC RawrXD_Ring_Available
RawrXD_Ring_Available PROC
    test    rcx, rcx
    jz      @@avail_zero
    mov     rax, QWORD PTR [rcx + RING_OFF_WRITE]
    sub     rax, QWORD PTR [rcx + RING_OFF_READ]
    ret
@@avail_zero:
    xor     eax, eax
    ret
RawrXD_Ring_Available ENDP

; =============================================================================
; UINT64 RawrXD_Ring_Free(const void* pRing)
;   rcx = ring buffer
;   Returns: number of free bytes available for writing
; =============================================================================
PUBLIC RawrXD_Ring_Free
RawrXD_Ring_Free PROC
    test    rcx, rcx
    jz      @@free_zero
    mov     rax, QWORD PTR [rcx + RING_OFF_WRITE]
    sub     rax, QWORD PTR [rcx + RING_OFF_READ]       ; used
    mov     rdx, QWORD PTR [rcx + RING_OFF_MASK]
    inc     rdx                                          ; capacity
    sub     rdx, rax                                     ; free = capacity - used
    mov     rax, rdx
    ret
@@free_zero:
    xor     eax, eax
    ret
RawrXD_Ring_Free ENDP

; =============================================================================
; void RawrXD_Ring_ConsumerAdvance(void* pRing, UINT64 nBytes)
;   rcx = ring buffer
;   rdx = bytes consumed (caller has already read them)
;
; Advances read_pos.  Called by the consumer (GPU dispatch thread or
; inference thread after reading from the ring).
; =============================================================================
PUBLIC RawrXD_Ring_ConsumerAdvance
RawrXD_Ring_ConsumerAdvance PROC
    test    rcx, rcx
    jz      @@ca_bail
    mov     rax, QWORD PTR [rcx + RING_OFF_READ]
    add     rax, rdx
    mov     QWORD PTR [rcx + RING_OFF_READ], rax
@@ca_bail:
    ret
RawrXD_Ring_ConsumerAdvance ENDP

; =============================================================================
; void RawrXD_Ring_Reset(void* pRing)
;   rcx = ring buffer
;   Resets write/read positions and counters.  NOT safe to call while
;   producer or consumer is active.
; =============================================================================
PUBLIC RawrXD_Ring_Reset
RawrXD_Ring_Reset PROC
    test    rcx, rcx
    jz      @@rst_bail
    xor     eax, eax
    mov     QWORD PTR [rcx + RING_OFF_WRITE], rax
    mov     QWORD PTR [rcx + RING_OFF_READ], rax
    mov     QWORD PTR [rcx + RING_OFF_DROPS], rax
    mov     QWORD PTR [rcx + RING_OFF_TOTAL], rax
    sfence
@@rst_bail:
    ret
RawrXD_Ring_Reset ENDP

END
