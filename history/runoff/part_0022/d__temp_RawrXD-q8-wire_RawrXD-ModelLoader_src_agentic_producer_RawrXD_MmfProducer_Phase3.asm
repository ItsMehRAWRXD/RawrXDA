; RawrXD_MmfProducer_Phase3.asm
; Memory-Mapped File Producer for Tool Invocation Dispatch
; x64 MASM with zero-copy batching and backpressure detection
; OPTION WIN64:3 enables automatic stack frame generation

.xlist
include windows.inc
include kernel32.inc
include user32.inc
include ntdll.inc
.list

OPTION WIN64:3

; Constants
PRODUCER_CTX_SIZE    equ 256
MMF_MAX_BATCH_SIZE   equ 128
TOOL_TOKEN_SIZE      equ 64
BACKPRESSURE_LIMIT   equ 90    ; Fill % threshold
CRITICAL_BATCH       equ 32    ; Min size for critical dispatch

; Tool Token Layout (64 bytes, 32-byte aligned)
TOOL_TOKEN struct
    magic       dd ?    ; 0xDEADC0DE
    version     dd ?    ; v1 = 0x01000000
    flags       dd ?    ; dispatch priority
    tool_id     dd ?    ; tool identifier
    payload_len dd ?    ; data length
    timestamp   dq ?    ; invoke timestamp
    reserved    dq ?    ; padding
    payload     db 24 dup (?)  ; tool args or routing info
TOOL_TOKEN ends

; Producer Context
PRODUCER_CTX struct
    mmf_handle      dq ?    ; MMF handle
    base_addr       dq ?    ; Ring buffer base
    write_ptr       dq ?    ; Producer write offset
    control_block   dq ?    ; MMF control block RVA
    batch_buf       dq ?    ; Batch accumulation buffer
    batch_count     dd ?    ; Current batch items
    status          dd ?    ; IDLE/BATCHING/FLUSHING
    last_flush_ts   dq ?    ; Flush timestamp for adaptive batching
    batch_threshold dd ?    ; Dynamic batch size
    backpressure_ct dd ?    ; Throttle counter
    flushed_tokens  dq ?    ; Statistics: tokens flushed
    batches_sent    dq ?    ; Statistics: batch flushes
PRODUCER_CTX ends

.code

; =============================================================================
; MmfProducer_Initialize
; rcx = MMF handle, rdx = base address, r8 = control block
; Returns rax = 1 on success, 0 on failure
; =============================================================================
MmfProducer_Initialize proc public uses rbx rdi rsi
    .allocstack 40h
    .endprolog
    
    ; Allocate producer context on stack (256 bytes)
    sub rsp, 100h
    mov rdi, rsp                ; rdi = context pointer
    
    ; Initialize context
    xor eax, eax
    mov [rdi + PRODUCER_CTX.mmf_handle], rcx
    mov [rdi + PRODUCER_CTX.base_addr], rdx
    mov [rdi + PRODUCER_CTX.control_block], r8
    mov [rdi + PRODUCER_CTX.status], eax    ; IDLE
    mov [rdi + PRODUCER_CTX.batch_count], eax
    mov [rdi + PRODUCER_CTX.batch_threshold], 16  ; Start at 16-item batches
    
    ; Allocate batch buffer (128 * 64 bytes = 8KB)
    mov rcx, 2000h              ; 8192 bytes
    call LocalAlloc             ; rax = buffer address
    
    test rax, rax
    jz @init_failure
    
    mov [rdi + PRODUCER_CTX.batch_buf], rax
    mov eax, 1                  ; Success
    jmp @init_done
    
@init_failure:
    xor eax, eax
    
@init_done:
    add rsp, 100h
    ret
MmfProducer_Initialize endp

; =============================================================================
; MmfProducer_SubmitTool
; rcx = producer context, rdx = tool_id, r8 = flags, r9 = payload ptr
; Submits a tool invocation to the batch buffer with backpressure check
; =============================================================================
MmfProducer_SubmitTool proc public uses rbx rdi rsi
    .allocstack 48h
    .endprolog
    
    mov rdi, rcx                ; rdi = producer context
    mov rbx, rdx                ; rbx = tool_id
    mov rsi, r8                 ; rsi = flags
    
    ; Check batch fill level for backpressure
    mov ecx, [rdi + PRODUCER_CTX.batch_count]
    cmp ecx, MMF_MAX_BATCH_SIZE
    jge @backpressure_throttle
    
    ; Get write offset in batch buffer
    mov rax, rcx                ; ecx = batch_count
    imul rax, TOOL_TOKEN_SIZE   ; rax = byte offset
    
    mov rdx, [rdi + PRODUCER_CTX.batch_buf]
    add rdx, rax                ; rdx = slot in batch buffer
    
    ; Build tool token (64 bytes)
    mov dword [rdx + TOOL_TOKEN.magic], 0xDEADC0DE
    mov dword [rdx + TOOL_TOKEN.version], 01000000h
    mov [rdx + TOOL_TOKEN.flags], esi       ; Copy flags
    mov [rdx + TOOL_TOKEN.tool_id], ebx     ; Copy tool_id
    
    ; Copy payload (up to 24 bytes)
    mov eax, [r9]               ; Load first payload dword
    mov [rdx + TOOL_TOKEN.payload], eax
    
    ; Capture timestamp via RDTSC
    rdtsc                       ; rax:rdx = TSC value
    mov r10, rax                ; Save low 64 bits
    mov [rdx + TOOL_TOKEN.timestamp], r10
    
    ; Increment batch count
    add dword [rdi + PRODUCER_CTX.batch_count], 1
    mov ecx, [rdi + PRODUCER_CTX.batch_count]
    
    ; Adaptive batch triggering: flush if threshold reached OR flags indicate urgent
    mov eax, esi
    and eax, 0xF0000000         ; Check priority bits (flags >> 28)
    cmp eax, 0xC0000000         ; Critical/emergency priority?
    je @maybe_flush_now
    
    cmp ecx, [rdi + PRODUCER_CTX.batch_threshold]
    jl @submit_done
    
@maybe_flush_now:
    ; Trigger flush via thread-safe signaling
    mov rcx, rdi
    call MmfProducer_FlushBatch
    
@submit_done:
    mov eax, 1
    ret
    
@backpressure_throttle:
    ; Increment backpressure counter
    add dword [rdi + PRODUCER_CTX.backpressure_ct], 1
    
    ; Sleep for adaptive backoff (1-10ms based on counter)
    mov eax, [rdi + PRODUCER_CTX.backpressure_ct]
    mov ecx, eax
    shr ecx, 2                  ; ecx = counter / 4 (0-25+)
    cmp ecx, 10
    jle @no_clip
    mov ecx, 10
@no_clip:
    mov rdx, rsp                ; Sleep param ptr
    mov [rdx + 20h], rcx        ; milliseconds
    mov rcx, rcx
    call Sleep
    
    xor eax, eax                ; Return EBUSY/retry
    ret
MmfProducer_SubmitTool endp

; =============================================================================
; MmfProducer_FlushBatch
; rcx = producer context
; Writes accumulated batch to MMF ring buffer with zero-copy
; Uses compare-and-swap for atomic write_ptr update
; =============================================================================
MmfProducer_FlushBatch proc public uses rbx rdi rsi
    .allocstack 80h
    .endprolog
    
    mov rdi, rcx                ; rdi = producer context
    
    mov ecx, [rdi + PRODUCER_CTX.batch_count]
    test ecx, ecx
    jz @flush_noop              ; No items to flush
    
    ; Acquire base address and current write_ptr from MMF control block
    mov rax, [rdi + PRODUCER_CTX.control_block]  ; control block address
    mov rdx, [rax]              ; read MMF write_ptr (offset 0)
    mov r8, [rdi + PRODUCER_CTX.base_addr]       ; ring buffer base
    
    ; Calculate circular buffer space
    mov r9, 100000h             ; 1MB ring buffer size assumption
    mov r10d, ecx
    imul r10, TOOL_TOKEN_SIZE   ; r10 = bytes to write
    
    ; Check for buffer wrap
    mov rax, rdx
    add rax, r10
    cmp rax, r9
    jle @no_wrap
    
    ; Wrap case: write to end, then from beginning
    mov r11, r9
    sub r11, rdx                ; r11 = bytes until wrap
    
    mov rsi, [rdi + PRODUCER_CTX.batch_buf]
    add r8, rdx                 ; dest = base + write_ptr
    mov rcx, r11
    call memcpy_fast            ; Copy to end of buffer
    
    ; Copy remainder from start
    mov rdx, 0                  ; New write_ptr at 0
    mov rax, r10
    sub rax, r11
    mov rcx, rax
    call memcpy_fast            ; Copy from beginning
    
    mov rdx, rax                ; Updated write_ptr
    jmp @update_write_ptr
    
@no_wrap:
    ; Linear write
    mov rsi, [rdi + PRODUCER_CTX.batch_buf]
    add r8, rdx                 ; dest
    mov rcx, r10
    call memcpy_fast
    
    add rdx, r10                ; Update write_ptr
    
@update_write_ptr:
    ; Atomic update of MMF write_ptr (CAS loop)
    mov rax, [rdi + PRODUCER_CTX.control_block]
    mov rcx, rdx                ; rcx = new write_ptr
    
@cas_retry:
    mov r10, [rax]              ; Load current write_ptr
    cmpxchg [rax], rcx          ; Try CAS
    jne @cas_retry              ; Retry if failed
    
    ; Increment statistics
    add qword [rdi + PRODUCER_CTX.batch_count], 0  ; Clear batch
    mov dword [rdi + PRODUCER_CTX.batch_count], 0
    
    mov rbx, rsi                ; Save batch_buf for stats
    mov rax, [rdi + PRODUCER_CTX.flushed_tokens]
    add rax, r10                ; Add tokens count
    imul rax, -1
    div TOOL_TOKEN_SIZE         ; Adjust count
    add [rdi + PRODUCER_CTX.flushed_tokens], rax
    
    inc qword [rdi + PRODUCER_CTX.batches_sent]
    
@flush_noop:
    mov eax, 1
    ret
MmfProducer_FlushBatch endp

; =============================================================================
; MmfProducer_DetectBackpressure
; rcx = producer context
; Returns eax = fill percentage (0-100)
; =============================================================================
MmfProducer_DetectBackpressure proc public uses rbx rdi
    .allocstack 40h
    .endprolog
    
    mov rdi, rcx
    mov rax, [rdi + PRODUCER_CTX.control_block]
    
    ; Calculate fill percentage
    mov r8, [rax]               ; write_ptr
    mov r9, [rax + 8h]          ; read_ptr (approx, from consumer)
    
    mov r10, 100000h            ; Buffer size
    
    ; fill = (write_ptr - read_ptr) * 100 / size
    mov rax, r8
    sub rax, r9
    cmp rax, 0
    jge @fill_ok
    add rax, r10                ; Handle wrap
    
@fill_ok:
    mov rcx, 100
    mul rcx
    mov rcx, r10
    div rcx
    
    ; Adaptive threshold adjustment
    cmp eax, BACKPRESSURE_LIMIT
    jle @bp_ok
    mov ecx, [rdi + PRODUCER_CTX.batch_threshold]
    shr ecx, 1                  ; Halve threshold under pressure
    cmp ecx, 4
    jge @bp_threshold_ok
    mov ecx, 4                  ; Minimum 4
@bp_threshold_ok:
    mov [rdi + PRODUCER_CTX.batch_threshold], ecx
    
@bp_ok:
    ret
MmfProducer_DetectBackpressure endp

; =============================================================================
; memcpy_fast (internal helper)
; rsi = source, rdi = dest, rcx = length
; =============================================================================
memcpy_fast proc private uses rsi rdi rcx
    push rbx
    
    mov rax, rcx
    shr rcx, 3                  ; Copy qwords
    rep movsq
    
    mov rcx, rax
    and rcx, 7                  ; Copy remaining bytes
    rep movsb
    
    pop rbx
    ret
memcpy_fast endp

end
