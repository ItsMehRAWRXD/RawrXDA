; RawrXD_Streaming_Orchestrator.asm
; Production-ready DMA ring buffer orchestrator for 800B models
; Pure MASM64 | Zero Dependencies | 64GB RAM constraint

include windows.inc
include masm64rt.inc

; ════════════════════════════════════════════════════════════════
; CONFIGURATION
; ════════════════════════════════════════════════════════════════
STREAM_BUFFER_SIZE  equ 67108864   ; 64MB
STREAM_CHUNK_SIZE   equ 16777216   ; 16MB chunks
MAX_CHUNKS_IN_FLIGHT equ 4
STREAM_HIGH_WATER   equ 50331648   ; 48MB (75%)
STREAM_LOW_WATER    equ 16777216   ; 16MB (25%)

; ════════════════════════════════════════════════════════════════
; STREAM STATE STRUCTURES
; ════════════════════════════════════════════════════════════════
STREAM_STATE struct
    buffer_base         dq ?        ; Base pointer to 64MB buffer
    write_ptr           dq ?        ; Current write offset
    read_ptr            dq ?        ; Current read offset
    bytes_available     dq ?        ; Bytes ready for consumption
    chunks_in_flight    dd ?        ; Active DMA transfers
    is_full             db ?        ; Backpressure flag
    is_empty            db ?        ; Underrun flag
    active_engine       db ?        ; 0=FP32, 1=Quantized
    _pad                db ?
STREAM_STATE ends

DMA_CHUNK struct
    src_offset          dq ?        ; Source offset in GGUF file
    dst_offset          dq ?        ; Destination offset in buffer
    size                dq ?        ; Chunk size (multiple of 16MB)
    tensor_name         dq ?        ; Pointer to tensor identifier
    engine_id           db ?        ; Target engine
    status              db ?        ; 0=pending, 1=active, 2=complete, 3=failed
    _pad                dw ?
DMA_CHUNK ends

; ════════════════════════════════════════════════════════════════
; GLOBAL STATE
; ════════════════════════════════════════════════════════════════
.data?
g_stream_state      STREAM_STATE <>
g_dma_chunks        DMA_CHUNK MAX_CHUNKS_IN_FLIGHT dup( <>)
g_chunk_index       dq 0
g_total_bytes_streamed dq 0
g_backpressure_events dq 0
g_engine_switches   dq 0
g_patch_conflicts   dq 0
g_throttle_pauses   dq 0
mem_status          MEMORYSTATUSEX <>

; ════════════════════════════════════════════════════════════════
; INITIALIZATION
; ════════════════════════════════════════════════════════════════
.code

RawrXD_Streaming_Orchestrator_Init PROC
    ; Allocate 64MB streaming buffer
    mov rcx, STREAM_BUFFER_SIZE
    call VirtualAlloc, NULL, rcx, MEM_COMMIT, PAGE_READWRITE
    test rax, rax
    jz @error
    
    mov g_stream_state.buffer_base, rax
    mov g_stream_state.write_ptr, 0
    mov g_stream_state.read_ptr, 0
    mov g_stream_state.bytes_available, 0
    mov g_stream_state.chunks_in_flight, 0
    mov g_stream_state.is_full, 0
    mov g_stream_state.is_empty, 1
    mov g_stream_state.active_engine, 0   ; Start with FP32
    
    ; Clear DMA chunk table
    lea rcx, g_dma_chunks
    mov r8, SIZEOF DMA_CHUNK * MAX_CHUNKS_IN_FLIGHT
    xor edx, edx
    call memset                     ; Zero-fill

    call asm_log, "[STREAM] Orchestrator initialized", ASM_LOG_INFO
    
    xor eax, eax                    ; Success
    ret
    
@error:
    mov eax, 1                      ; Failure
    ret
RawrXD_Streaming_Orchestrator_Init ENDP

; ════════════════════════════════════════════════════════════════
; RING BUFFER OPERATIONS (Lock-Free)
; ════════════════════════════════════════════════════════════════

RawrXD_Stream_WriteChunk PROC
    ; rcx = src_data, rdx = size (must be <= 16MB)
    ; Returns: rax = bytes written, 0 on failure
    LOCAL bytes_to_write:qword
    LOCAL src_ptr:qword

    mov src_ptr, rcx
    
    ; Check for overflow
    mov rax, g_stream_state.bytes_available
    add rax, rdx
    cmp rax, STREAM_BUFFER_SIZE
    jge @backpressure
    
    ; Calculate write position (wraparound)
    mov rax, g_stream_state.write_ptr
    mov bytes_to_write, rdx
    
    ; Copy with AVX-512 (non-temporal)
    mov r8, g_stream_state.buffer_base
    add r8, rax
    
    ; Check if copy crosses buffer boundary
    mov rax, STREAM_BUFFER_SIZE
    sub rax, g_stream_state.write_ptr
    cmp rax, bytes_to_write
    jl @wraparound_copy
    
    ; Single contiguous copy
    call rawrxd_avx512_nt_copy, src_ptr, r8, bytes_to_write
    jmp @update_ptrs
    
@wraparound_copy:
    ; Two-part copy at wrap point
    call rawrxd_avx512_nt_copy, src_ptr, r8, rax
    ; Second part from buffer start
    mov rcx, src_ptr
    add rcx, rax
    mov r8, g_stream_state.buffer_base
    mov rdx, bytes_to_write
    sub rdx, rax
    call rawrxd_avx512_nt_copy, rcx, r8, rdx
    
@update_ptrs:
    ; Atomic update (lock-free using interlocked operations)
    mov rax, g_stream_state.write_ptr
    add rax, bytes_to_write
    cmp rax, STREAM_BUFFER_SIZE
    jl @no_wrap
    sub rax, STREAM_BUFFER_SIZE
    
@no_wrap:
    xchg g_stream_state.write_ptr, rax
    
    ; Update available bytes
    mov rax, bytes_to_write
    lock add g_stream_state.bytes_available, rax
    mov g_stream_state.is_empty, 0
    lock add g_total_bytes_streamed, rax

    call asm_log, "[STREAM] Write chunk committed", ASM_LOG_DEBUG

    mov rax, bytes_to_write         ; Return bytes written
    ret
    
@backpressure:
    ; Signal backpressure to agentic loop
    lock inc g_backpressure_events
    mov g_stream_state.is_full, 1
    
    call asm_log, "[STREAM] Backpressure triggered", ASM_LOG_WARN
    
    xor rax, rax                    ; Return 0 (throttled)
    ret
RawrXD_Stream_WriteChunk ENDP

RawrXD_Stream_ReadChunk PROC
    ; rcx = dst_buffer, rdx = max_size
    ; Returns: rax = bytes read, 0 if underrun
    LOCAL bytes_to_read:qword
    
    mov rax, g_stream_state.bytes_available
    test rax, rax
    jz @underrun
    
    ; Determine read size (min(requested, available))
    cmp rax, rdx
    cmovg rax, rdx
    mov bytes_to_read, rax
    
    ; Read from ring buffer
    mov r8, g_stream_state.buffer_base
    add r8, g_stream_state.read_ptr
    
    ; Check for wraparound
    mov rax, STREAM_BUFFER_SIZE
    sub rax, g_stream_state.read_ptr
    cmp rax, bytes_to_read
    jl @wraparound_read
    
    ; Single copy
    call memcpy, rcx, r8, bytes_to_read
    jmp @update_read
    
@wraparound_read:
    ; Two-part read
    call memcpy, rcx, r8, rax
    ; Second part from buffer start
    mov rcx, dst_buffer
    add rcx, rax
    mov r8, g_stream_state.buffer_base
    mov rdx, bytes_to_read
    sub rdx, rax
    call memcpy, rcx, r8, rdx
    
@update_read:
    ; Update read pointer
    mov rax, g_stream_state.read_ptr
    add rax, bytes_to_read
    cmp rax, STREAM_BUFFER_SIZE
    jl @no_wrap_read
    sub rax, STREAM_BUFFER_SIZE
    
@no_wrap_read:
    lock xchg g_stream_state.read_ptr, rax
    
    ; Update available bytes (atomic decrement)
    mov rax, bytes_to_read
    neg rax
    lock add g_stream_state.bytes_available, rax
    
    ; Clear backpressure if below low water mark
    mov rax, g_stream_state.bytes_available
    cmp rax, STREAM_LOW_WATER
    jg @exit
    
    mov g_stream_state.is_full, 0
    
    call asm_log, "[STREAM] Backpressure cleared", ASM_LOG_DEBUG
    
@exit:
    mov rax, bytes_to_read
    ret
    
@underrun:
    mov g_stream_state.is_empty, 1
    xor rax, rax
    ret
RawrXD_Stream_ReadChunk ENDP

; ════════════════════════════════════════════════════════════════
; ENGINE HANDOFF COORDINATION
; ════════════════════════════════════════════════════════════════

RawrXD_Stream_SwitchEngine PROC
    ; rcx = new_engine_id (0=FP32, 1=Quantized)
    ; Returns: rax = remaining chunks, 0 if switch unsafe
    LOCAL chunks_remaining:qword
    
    ; Check if switch is safe (no chunks in flight)
    mov eax, g_stream_state.chunks_in_flight
    mov chunks_remaining, rax
    
    test rax, rax
    jnz @unsafe
    
    ; Safe to switch
    mov al, cl
    mov g_stream_state.active_engine, al
    lock inc g_engine_switches
    
    call asm_log, "[STREAM] Engine switched", ASM_LOG_INFO
    
    xor eax, eax
    ret
    
@unsafe:
    ; Cannot switch while DMA active
    call asm_log, "[STREAM] Engine switch blocked (chunks in flight)", ASM_LOG_WARN
    
    mov rax, chunks_remaining
    ret
RawrXD_Stream_SwitchEngine ENDP

; ════════════════════════════════════════════════════════════════
; PATCH CONFLICT DETECTION
; ════════════════════════════════════════════════════════════════

RawrXD_Stream_CheckPatchConflict PROC
    ; rcx = patch_target (tensor name)
    ; Returns: rax = 1 if conflict, 0 if safe
    LOCAL is_active:dword
    LOCAL patch_ptr:qword
    
    xor eax, eax
    mov is_active, eax
    mov patch_ptr, rcx
    
    lea r8, g_dma_chunks
    mov r9d, MAX_CHUNKS_IN_FLIGHT
    
@loop:
    cmp r9d, 0
    je @done
    
    mov al, [r8].DMA_CHUNK.status
    cmp al, 1                       ; STATUS_ACTIVE
    jne @next
    
    mov rax, [r8].DMA_CHUNK.tensor_name
    test rax, rax
    jz @next
    
    mov rcx, patch_ptr
    mov rdx, rax
    call lstrcmpA                   ; Returns 0 when equal
    test eax, eax
    jne @next
    
    mov eax, 1
    mov is_active, eax
    lock inc g_patch_conflicts
    call asm_log, "[STREAM] Patch conflict detected", ASM_LOG_INFO
    jmp @done
    
@next:
    add r8, SIZEOF DMA_CHUNK
    dec r9d
    jmp @loop
    
@done:
    mov eax, is_active
    ret
RawrXD_Stream_CheckPatchConflict ENDP

; ════════════════════════════════════════════════════════════════
; BACKPRESSURE SIGNALING
; ════════════════════════════════════════════════════════════════

RawrXD_Stream_QueryBackpressure PROC
    ; Returns: rax = 1 if backpressure active, 0 otherwise
    mov al, g_stream_state.is_full
    movzx rax, al
    ret
RawrXD_Stream_QueryBackpressure ENDP

; ════════════════════════════════════════════════════════════════
; LOAD-BASED THROTTLING (Agentic Integration)
; ════════════════════════════════════════════════════════════════

RawrXD_Stream_AdjustThrottle PROC
    ; Called by agentic loop to adjust streaming rate
    ; Returns: rax = recommended chunk size (0 = pause)
    LOCAL system_load:qword
    
    ; Query available memory
    lea rcx, mem_status
    mov mem_status.dwLength, SIZEOF MEMORYSTATUSEX
    call GlobalMemoryStatusEx
    test eax, eax
    jz @fallback

    mov rax, mem_status.ullAvailPhys
    shr rax, 30                     ; Convert to GB
    
    ; If < 8GB free, throttle aggressively
    cmp rax, 8
    jg @normal
    
    mov rax, STREAM_CHUNK_SIZE / 4  ; 25% rate
    lock inc g_throttle_pauses
    call asm_log, "[STREAM] Throttle: low memory", ASM_LOG_WARN
    ret
    
@normal:
    mov rax, STREAM_CHUNK_SIZE      ; Full rate
    call asm_log, "[STREAM] Throttle: normal", ASM_LOG_DEBUG
    ret

@fallback:
    mov rax, STREAM_CHUNK_SIZE / 2  ; Conservative default on query failure
    call asm_log, "[STREAM] Throttle: fallback", ASM_LOG_WARN
    ret
RawrXD_Stream_AdjustThrottle ENDP

; ════════════════════════════════════════════════════════════════
; METRICS QUERY (Observability)
; ════════════════════════════════════════════════════════════════

RawrXD_Stream_QueryMetrics PROC
    ; Returns (via registers):
    ; rax = total bytes streamed
    ; rdx = backpressure events
    ; r8  = engine switches
    ; r9  = patch conflicts
    ; r10 = throttle pauses (low-memory/fallback)
    mov rax, g_total_bytes_streamed
    mov rdx, g_backpressure_events
    mov r8,  g_engine_switches
    mov r9,  g_patch_conflicts
    mov r10, g_throttle_pauses
    ret
RawrXD_Stream_QueryMetrics ENDP

; ════════════════════════════════════════════════════════════════
; AVX-512 NON-TEMPORAL COPY (For 800B models)
; ════════════════════════════════════════════════════════════════

rawrxd_avx512_nt_copy PROC
    ; rcx = src, rdx = dst, r8 = size (must be 64-byte aligned)
    push rbx
    mov rbx, r8
    
    ; Check alignment
    test rcx, 63
    jnz @unaligned
    test rdx, 63
    jnz @unaligned
    
@aligned_loop:
    ; Load 64 bytes (non-temporal)
    vmovntdqa zmm0, [rcx]
    vmovntdq [rdx], zmm0
    
    add rcx, 64
    add rdx, 64
    sub rbx, 64
    jnz @aligned_loop
    
    ; Fence to ensure write completion
    mfence
    
    pop rbx
    ret
    
@unaligned:
    ; Fallback to rep movsb
    mov rax, rcx
    mov rdi, rdx
    mov rcx, rbx
    rep movsb
    pop rbx
    ret
rawrxd_avx512_nt_copy ENDP

END
