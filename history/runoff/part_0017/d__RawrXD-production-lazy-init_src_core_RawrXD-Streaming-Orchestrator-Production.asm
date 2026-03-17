; ============================================================================
; RawrXD Streaming Orchestrator - Production Implementation
; Pure MASM64 | Zero-copy streaming orchestrator | Cache-aware, backpressure-safe
; Interfaces: attach/detach/submit/fetch/prefetch
; ============================================================================

.code

; External helpers (defined elsewhere)
EXTERN print_message: PROC
EXTERN memory_zero_stub: PROC
EXTERN asm_log: PROC
EXTERN GetStdHandle: PROC
EXTERN WriteFile: PROC
EXTERN Sleep: PROC

; Export orchestrator API symbols for linking with C/C++
PUBLIC orchestrator_init
PUBLIC stream_attach
PUBLIC stream_detach
PUBLIC submit_chunk
PUBLIC fetch_chunk
PUBLIC prefetch_stream_window
PUBLIC align_up

; Constants
STREAM_MAX_QUEUES      EQU 64
STREAM_QUEUE_CAPACITY  EQU 1024        ; entries per queue (recommend power-of-two)
STREAM_CHUNK_ALIGN     EQU 64          ; cache-line alignment
STREAM_PREFETCH_AHEAD  EQU 4

; Result codes
SR_OK                  EQU 0
SR_ERR_BACKPRESSURE    EQU 1
SR_ERR_INVALID         EQU 2
SR_ERR_FULL            EQU 3

.data?
; Per-stream control region (array of control slots)
; Each slot is a small control block; we keep a flat array for simple indexing
; Layout per-slot (qwords):
; [0] queue_head    ; consumer index
; [1] queue_tail    ; producer index
; [2] queue_base    ; pointer to qword array (entries)
; [3] capacity      ; capacity (power-of-two recommended)
; [4] flags         ; reserved
; Slot size = 5*qword = 40 bytes (rounded by usage)
g_stream_ctl dq STREAM_MAX_QUEUES*5 dup(0)

; Global metrics
g_stream_attach_count dq 0
g_stream_detach_count dq 0
g_stream_overflow_count dq 0

scratch_log_buf db 128 dup(0)

; ============================================================================
; align_up helper - rax = align_up(rcx, rdx)
align_up PROC
    mov rax, rcx
    mov rbx, rdx
    dec rbx
    add rax, rbx
    and rax, -rdx
    ret
align_up ENDP

; ============================================================================
; orchestrator_init - initialize global state
; Output: rax = SR_OK
; ============================================================================
orchestrator_init PROC
    sub rsp, 28h
    lea rcx, g_stream_ctl
    mov rdx, SIZEOF g_stream_ctl
    call memory_zero_stub

    mov qword ptr g_stream_attach_count, 0
    mov qword ptr g_stream_detach_count, 0
    mov qword ptr g_stream_overflow_count, 0

    lea rcx, scratch_log_buf
    mov byte ptr [rcx], 'S'
    mov byte ptr [rcx+1], 't'
    mov byte ptr [rcx+2], 'r'
    mov byte ptr [rcx+3], 'm'
    mov byte ptr [rcx+4], 0
    call print_message

    add rsp, 28h
    mov rax, SR_OK
    ret
orchestrator_init ENDP

; ============================================================================
; stream_attach - attach a preallocated ring buffer as a stream
; Input: rcx = pointer to ring base (qword array), rdx = capacity (power-of-two)
; Output: rax = pointer to control slot (on success) or 0
; ============================================================================
stream_attach PROC
    sub rsp, 48h
    mov r8, rcx            ; ring base
    mov r9, rdx            ; capacity

    test r8, r8
    jz .bad
    test r9, r9
    jz .bad

    ; Linear find free slot (0 value in queue_base marks free)
    lea rbx, g_stream_ctl
    xor rax, rax
.find_slot:
    cmp rax, STREAM_MAX_QUEUES
    jae .noslot
    mov r10, qword ptr [rbx + rax*8 + 16] ; queue_base offset = slot*40 + 16
    test r10, r10
    jnz .next

    ; we claim slot by writing control fields
    lea r11, [rbx + rax*8]
    mov qword ptr [r11+0], 0        ; head
    mov qword ptr [r11+8], 0        ; tail
    mov qword ptr [r11+16], r8      ; queue_base
    mov qword ptr [r11+24], r9      ; capacity
    mov qword ptr [r11+32], 0       ; flags

    ; bump global attach metric
    mov rdx, 1
    lea rcx, g_stream_attach_count
    lock xadd qword ptr [rcx], rdx

    ; return control slot pointer
    lea rax, [r11]
    add rsp, 48h
    ret
.next:
    inc rax
    jmp .find_slot

.noslot:
    xor rax, rax
    add rsp, 48h
    ret

.bad:
    xor rax, rax
    add rsp, 48h
    ret
stream_attach ENDP

; ============================================================================
; stream_detach - mark control slot free
; Input: rcx = pointer to control slot
; Output: rax = SR_OK
; ============================================================================
stream_detach PROC
    sub rsp, 28h
    test rcx, rcx
    jz .done

    ; clear queue_base (slot becomes free)
    mov qword ptr [rcx+16], 0

    ; increment detach metric
    mov rdx, 1
    lea rcx, g_stream_detach_count
    lock xadd qword ptr [rcx], rdx

.done:
    add rsp, 28h
    mov rax, SR_OK
    ret
stream_detach ENDP

; ============================================================================
; submit_chunk - enqueue a qword pointer into the stream ring
; Input: rcx = control slot ptr, rdx = chunk pointer
; Output: rax = SR_OK on success, SR_ERR_BACKPRESSURE if full
; Implementation notes:
; - Uses atomic XADD on queue_tail to reserve a slot
; - Capacity must be power-of-two for fast masking (mask = capacity-1)
; ============================================================================
submit_chunk PROC
    sub rsp, 40h
    test rcx, rcx
    jz .invalid

    mov r14, rdx              ; save chunk pointer
    mov r12, qword ptr [rcx+24] ; capacity
    mov r13, qword ptr [rcx+16] ; queue_base
    test r13, r13
    jz .invalid

    ; reserve slot: XADD on queue_tail
    lea r9, [rcx+8]
    mov r10, 1
    lock xadd qword ptr [r9], r10   ; r10 = previous tail index
    mov r11, r10                     ; index = previous tail

    ; compute depth = tail_prev - head
    mov r15, qword ptr [rcx+0]       ; head
    mov rax, r11
    sub rax, r15
    cmp rax, r12
    jb .have_slot

    ; undo increment and signal backpressure
    mov r10, -1
    lock xadd qword ptr [r9], r10
    ; increment overflow metric
    mov rdx, 1
    lea rcx, g_stream_overflow_count
    lock xadd qword ptr [rcx], rdx
    mov rax, SR_ERR_BACKPRESSURE
    add rsp, 40h
    ret

.have_slot:
    ; check power-of-two: if (capacity & (capacity-1)) == 0 then use mask, else use div
    mov rax, r12
    dec rax
    test r12, rax
    jnz .use_div
    mov rdx, r12
    dec rdx
    and r11, rdx            ; slot index = previous_tail & mask
    jmp .store_slot

.use_div:
    ; non-power-of-two capacity: compute index = previous_tail % capacity
    ; preserve rbx (callee-saved) across the division
    push rbx
    mov rax, r11
    xor rdx, rdx
    mov rbx, r12
    div rbx                 ; rax = quotient, rdx = remainder
    mov r11, rdx            ; r11 = remainder
    pop rbx

.store_slot:
    ; store chunk pointer into ring slot
    mov qword ptr [r13 + r11*8], r14

    ; success
    mov rax, SR_OK
    add rsp, 40h
    ret

.invalid:
    mov rax, SR_ERR_INVALID
    add rsp, 40h
    ret
submit_chunk ENDP

; ============================================================================
; fetch_chunk - dequeue a qword pointer from the stream ring
; Input: rcx = control slot ptr
; Output: rax = chunk pointer or 0 if empty
; Implementation notes:
; - Uses XADD on queue_head; if empty, undoes increment
; ============================================================================
fetch_chunk PROC
    sub rsp, 40h
    test rcx, rcx
    jz .none

    lea r9, [rcx + 0]        ; &queue_head
    mov r10, 1
    lock xadd qword ptr [r9], r10   ; r10 = previous head
    mov r11, r10

    mov r12, qword ptr [rcx+8] ; tail
    cmp r11, r12
    jae .empty_undo

    ; compute slot index
    mov r13, qword ptr [rcx+24] ; capacity
    mov rdx, r13
    dec rdx
    and r11, rdx
    mov rbx, qword ptr [rcx+16] ; base
    mov rax, qword ptr [rbx + r11*8]
    add rsp, 40h
    ret

.empty_undo:
    ; nothing to fetch - undo head increment
    mov r10, -1
    lock xadd qword ptr [r9], r10
    xor rax, rax
    add rsp, 40h
    ret

.none:
    xor rax, rax
    add rsp, 40h
    ret
fetch_chunk ENDP

; ============================================================================
; prefetch_stream_window - prefetch upcoming entries for low-latency
; Input: rcx = control slot ptr, rdx = start_index, r8 = count
; ============================================================================
prefetch_stream_window PROC
    sub rsp, 28h
    test rcx, rcx
    jz .done
    mov rbx, qword ptr [rcx+16]
    test rbx, rbx
    jz .done

    mov r9, rdx
    mov r10, r8
.loop_prefetch:
    test r10, r10
    jz .done
    mov rax, r9
    mov r11, qword ptr [rcx+24]
    dec r11
    and rax, r11
    mov rsi, qword ptr [rbx + rax*8]
    PREFETCHT0 [rsi]
    PREFETCHT0 [rsi+64]
    inc r9
    dec r10
    jmp .loop_prefetch

.done:
    add rsp, 28h
    ret
prefetch_stream_window ENDP

; ============================================================================
; Implementation notes and hardening guidance (see file header)
; - Use power-of-two capacities to avoid division and improve throughput.
; - Consider non-temporal stores (MOVNTI / MOVNTPS / MOVNTDQ) when writing
;   producer-only buffers whose data will not be immediately read by the
;   producer to avoid cache write-allocate churn. When using non-temporal
;   stores, pair with "SFENCE" to ensure ordering before publishing the
;   tail index.
; - For very hot paths, prefer large-page allocations for the ring arrays
;   to reduce TLB misses. Provide a dedicated allocator for large pages.
; - Add a kernel-wait fallback for producers when backpressure persists
;   (avoid busy-waiting for long durations).

END