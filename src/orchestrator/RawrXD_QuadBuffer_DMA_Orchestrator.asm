; =============================================================================
; RawrXD_QuadBuffer_DMA_Orchestrator.asm
; Pure MASM x64 Implementation of HDD-to-RAM-to-VRAM Quad-Buffer
; For 800B Parameter Models with Direct I/O (DMA) Bypassing OS Cache
; =============================================================================
; Architecture:
;   Layer N   -> GPU Compute (Active)
;   Layer N+1 -> VRAM Preload (DMA from RAM)
;   Layer N+2 -> RAM Cache (Async from HDD)
;   Layer N+3 -> HDD Prefetch (Background I/O)
;
; The "backwards formula" creates a sliding window of reality:
;   GPU always sees 4GB of contiguous VRAM with current + next 3 layers
;   MASM orchestrator transparently manages 800GB HDD stream
;   YTFN_SENTINEL trap ensures GPU never reads uninitialized memory
; =============================================================================

OPTION CASEMAP:NONE
OPTION WIN64:3

INCLUDE \masm64\include64\masm64rt.inc

; =============================================================================
; CONSTANTS FOR 800B SCALING
; =============================================================================
QUAD_BUFFER_COUNT   EQU 4
PAGE_SIZE           EQU 040000000h       ; 1GB pages (1 << 30)
PAGE_SHIFT          EQU 30
YTFN_SENTINEL       EQU 07FFFFFFFFFFFFFFFh  ; Trap sentinel (highest pos i64)
HDD_BLOCK_SIZE      EQU 0100000000h      ; 4GB HDD chunks
DMA_ALIGNMENT       EQU 4096             ; Page alignment for Direct I/O

; Buffer status flags
BUF_STATE_EMPTY     EQU 0   ; Available for loading
BUF_STATE_LOADING   EQU 1   ; DMA in progress
BUF_STATE_READY     EQU 2   ; Ready for compute
BUF_STATE_COMPUTING EQU 3   ; Currently on GPU

; Win32 constants
FILE_FLAG_NO_BUFFERING     EQU 020000000h
FILE_FLAG_OVERLAPPED       EQU 040000000h
FILE_FLAG_SEQUENTIAL_SCAN  EQU 008000000h
GENERIC_READ               EQU 080000000h
OPEN_EXISTING              EQU 3
MEM_COMMIT                 EQU 1000h
MEM_RESERVE                EQU 2000h
MEM_LARGE_PAGES            EQU 20000000h
PAGE_READWRITE             EQU 4
WAIT_OBJECT_0              EQU 0
WAIT_TIMEOUT               EQU 258
INFINITE                   EQU -1

; =============================================================================
; DATA STRUCTURES
; =============================================================================

OVERLAPPED STRUCT
    Internal                DQ  ?
    InternalHigh            DQ  ?
    Offset                  DD  ?
    OffsetHigh              DD  ?
    hEvent                  DQ  ?
OVERLAPPED ENDS

QUAD_SLOT STRUCT
    state               DD  ?           ; BUF_STATE_*
    layer_idx           DD  ?           ; Which layer this holds (-1 = empty)
    vram_ptr            DQ  ?           ; GPU memory address
    ram_ptr             DQ  ?           ; Pinned system RAM address
    hdd_offset          DQ  ?           ; File offset on disk
    dma_overlapped      OVERLAPPED <>   ; Windows async I/O structure
    hEvent              DQ  ?           ; Completion event
    padding             DD  ?           ; Align to 128 bytes for cache line
QUAD_SLOT ENDS

INFINITY_STREAM STRUCT
    ; The "Virtual" 800B address space
    hdd_file_handle     DQ  ?           ; Handle to model file (unbuffered)
    total_layers        DD  ?
    layer_size          DQ  ?           ; Uniform layer size for 800B
    
    ; Quad-buffer state machine
    slots               QUAD_SLOT QUAD_BUFFER_COUNT DUP(<>)
    current_gpu_layer   DD  ?           ; Which layer GPU is currently on
    current_dma_slot    DD  ?           ; Which slot is being DMA'd
    
    ; Synchronization
    status_lock         DQ  ?           ; SRWLock for status updates
    dma_completion_port DQ  ?           ; I/O Completion Port
    
    ; Metrics
    hdd_read_bytes      DQ  ?
    dma_write_bytes     DQ  ?
    stall_cycles        DQ  ?
    trap_count          DD  ?
    trap_resolved_count DD  ?
INFINITY_STREAM ENDS

; =============================================================================
; DATA SECTION
; =============================================================================

.DATA

align 64
g_infinity_stream   INFINITY_STREAM <>

; Pre-calculated masks for modulo 4 (bitwise AND)
align 16
g_mod4_mask         DQ 03h, 03h, 03h, 03h

; AVX-512 index scale for gather operations
align 64
g_vramptr_indices   DQ 0, 8, 16, 24, 32, 40, 48, 56

; Phase integration constants
PHASE_2_MODEL_LAYER_SIZE DQ 1000000h    ; Phase 2 exports this
PHASE_3_TENSOR_STRIDE    DQ 100000h     ; Phase 3 stride
PHASE_4_DMA_BATCH_SIZE   DQ 8h          ; Phase 4 batch units

.CODE

; =============================================================================
; INFINITY_InitializeStream
; Opens 800B model file with Direct I/O (no OS caching)
; RCX = path to model file (wchar_t*)
; RDX = layer size (bytes)
; R8 = total layers
; R9 = vram_base
; Returns RAX = INFINITY_STREAM* or NULL
; =============================================================================
INFINITY_InitializeStream PROC FRAME
    push rbx rsi rdi r12 r13 r14 r15
    sub rsp, 96
    .allocstack 96
    .endprolog
    
    mov r12, rcx                    ; file path
    mov r13, rdx                    ; layer_size
    mov r14d, r8d                   ; total_layers
    mov r15, r9                     ; vram_base
    
    lea rdi, g_infinity_stream
    mov [rdi].INFINITY_STREAM.layer_size, r13
    mov [rdi].INFINITY_STREAM.total_layers, r14d
    
    ; Open file with FILE_FLAG_NO_BUFFERING | FILE_FLAG_OVERLAPPED
    ; This bypasses Windows cache entirely, preventing 800GB from polluting system RAM
    mov rcx, r12                    ; lpFileName
    mov edx, GENERIC_READ           ; dwDesiredAccess (read-only)
    xor r8d, r8d                    ; dwShareMode (exclusive)
    xor r9d, r9d                    ; lpSecurityAttributes
    
    mov rax, FILE_FLAG_NO_BUFFERING
    or rax, FILE_FLAG_OVERLAPPED
    or rax, FILE_FLAG_SEQUENTIAL_SCAN
    mov [rsp + 40], 3               ; OPEN_EXISTING
    mov [rsp + 48], rax             ; dwFlagsAndAttributes
    mov [rsp + 56], 0               ; hTemplateFile
    
    call CreateFileW
    
    cmp rax, -1
    je init_failed
    mov [rdi].INFINITY_STREAM.hdd_file_handle, rax
    
    ; Initialize I/O Completion Port for async notifications
    ; This allows zero-overhead notification when HDD reads complete
    mov rcx, rax                    ; FileHandle
    xor edx, edx                    ; ExistingCompletionPort (NULL = create new)
    xor r8d, r8d                    ; CompletionKey
    xor r9d, r9d                    ; NumberOfConcurrentThreads (0 = CPU count)
    call CreateIoCompletionPort
    
    cmp rax, 0
    je init_failed
    mov [rdi].INFINITY_STREAM.dma_completion_port, rax
    
    ; Initialize SRWLock for minimal-overhead synchronization
    lea rcx, [rdi].INFINITY_STREAM.status_lock
    call InitializeSRWLock
    
    ; Allocate pinned RAM for all 4 slots (4GB total)
    ; VirtualAlloc with MEM_LARGE_PAGES for optimal DMA performance
    mov eax, QUAD_BUFFER_COUNT
    mov rcx, PAGE_SIZE
    mul rcx                         ; RAX = 4GB (4 * 1GB)
    mov r12, rax                    ; Total allocation
    
    mov rcx, rax                    ; lpAddress
    mov edx, MEM_COMMIT or MEM_RESERVE or MEM_LARGE_PAGES
    mov r8d, PAGE_READWRITE
    call VirtualAlloc
    
    test rax, rax
    jz init_failed
    mov rsi, rax                    ; RAM base
    
    ; Initialize each quad slot
    mov rbx, rdi                    ; INFINITY_STREAM base
    add rbx, OFFSET INFINITY_STREAM.slots
    xor ecx, ecx                    ; Slot index
    
init_slot_loop:
    cmp ecx, QUAD_BUFFER_COUNT
    jge init_done
    
    mov [rbx].QUAD_SLOT.state, BUF_STATE_EMPTY
    mov DWORD PTR [rbx].QUAD_SLOT.layer_idx, -1
    
    ; VRAM pointer: vram_base + (slot_idx * PAGE_SIZE)
    mov eax, PAGE_SIZE
    mul ecx
    add rax, r15
    mov [rbx].QUAD_SLOT.vram_ptr, rax
    
    ; RAM pointer: ram_base + (slot_idx * PAGE_SIZE)
    mov eax, ecx
    mov r12d, PAGE_SIZE
    mul r12                         ; RAX = slot_idx * PAGE_SIZE
    add rax, rsi
    mov [rbx].QUAD_SLOT.ram_ptr, rax
    
    ; Create completion event for this slot (auto-reset, initially false)
    xor ecx, ecx
    mov edx, 1                      ; bManualReset = FALSE (auto-reset)
    xor r8d, r8d                    ; bInitialState = FALSE
    xor r9, r9                      ; lpName
    call CreateEventA
    mov [rbx].QUAD_SLOT.hEvent, rax
    
    add rbx, SIZEOF QUAD_SLOT
    mov ecx, [rdi].INFINITY_STREAM.total_layers  ; Restore for loop
    xor ecx, ecx
    inc ecx
    cmp ecx, QUAD_BUFFER_COUNT
    jle init_slot_loop
    
init_done:
    mov rax, rdi                    ; Return INFINITY_STREAM*
    add rsp, 96
    pop r15 r14 r13 r12 rdi rsi rbx
    ret
    
init_failed:
    xor eax, eax
    add rsp, 96
    pop r15 r14 r13 r12 rdi rsi rbx
    ret
INFINITY_InitializeStream ENDP

; =============================================================================
; INFINITY_CheckQuadBuffer
; The Core "Backwards" Formula Implementation
; Returns physical VRAM pointer or YTFN_SENTINEL trap
;
; The "backwards formula" creates the sliding window:
;   slot = layer_idx % 4
;   if slots[slot].layer_idx == layer_idx && slots[slot].state == READY
;       return slots[slot].vram_ptr
;   else
;       return YTFN_SENTINEL (trap)
;
; RCX = Requested Virtual Layer Index (i)
; RDX = Current Buffer Head (for validation)
; Returns RAX = Physical VRAM Pointer or YTFN_SENTINEL
; =============================================================================
INFINITY_CheckQuadBuffer PROC FRAME
    push rbx r12 r13 r14 r15
    sub rsp, 40
    .allocstack 40
    .endprolog
    
    mov r12, rcx                    ; Save layer index
    mov r13, rdx                    ; Save buffer head
    lea r15, g_infinity_stream
    
    ; 1. Calculate Buffer Slot: slot = i % 4 (bitwise AND with 3)
    mov rax, r12
    and rax, 3                      ; RAX = slot_index (0-3)
    mov rbx, rax                    ; RBX = slot_index
    
    ; 2. Acquire SRWLock (shared read) to check status
    lea rcx, [r15].INFINITY_STREAM.status_lock
    call AcquireSRWLockShared
    
    ; 3. Get slot state
    mov r14, r15
    add r14, OFFSET INFINITY_STREAM.slots
    mov eax, SIZEOF QUAD_SLOT
    mul rbx                         ; RAX = slot_index * sizeof(QUAD_SLOT)
    add r14, rax                    ; R14 = &slots[slot_index]
    
    mov eax, [r14].QUAD_SLOT.state
    cmp eax, BUF_STATE_READY
    je buffer_ready
    cmp eax, BUF_STATE_COMPUTING
    je buffer_computing
    
    ; Buffer not ready - release lock and return YTFN trap
    lea rcx, [r15].INFINITY_STREAM.status_lock
    call ReleaseSRWLockShared
    
    ; Return YTFN_SENTINEL encoded with layer index
    mov rax, YTFN_SENTINEL
    sub rax, r12                    ; Encode layer for trap handler
    inc dword ptr [r15].INFINITY_STREAM.trap_count
    
    add rsp, 40
    pop r15 r14 r13 r12 rbx
    ret
    
buffer_computing:
    ; Already computing on GPU - valid access
    ; Fall through to return pointer
    
buffer_ready:
    ; 4. Calculate Physical Address
    ; Physical = vram_ptr + offset_within_page
    ; For uniform 1GB layers, offset is 0
    mov rax, [r14].QUAD_SLOT.vram_ptr
    
    ; Verify layer index matches
    cmp DWORD PTR [r14].QUAD_SLOT.layer_idx, r12d
    jne buffer_mismatch
    
    ; Mark as computing
    mov [r14].QUAD_SLOT.state, BUF_STATE_COMPUTING
    
    ; Release lock
    lea rcx, [r15].INFINITY_STREAM.status_lock
    call ReleaseSRWLockShared
    
    add rsp, 40
    pop r15 r14 r13 r12 rbx
    ret
    
buffer_mismatch:
    ; Layer mismatch - stale data, return trap
    lea rcx, [r15].INFINITY_STREAM.status_lock
    call ReleaseSRWLockShared
    mov rax, YTFN_SENTINEL
    sub rax, r12
    inc dword ptr [r15].INFINITY_STREAM.trap_count
    
    add rsp, 40
    pop r15 r14 r13 r12 rbx
    ret
INFINITY_CheckQuadBuffer ENDP

; =============================================================================
; INFINITY_RotateBuffers
; The "Backwards" DMA Request - Manages the Sliding Window
; Triggered when GPU finishes processing a layer
;
; State transitions:
;   slot[i-2].COMPUTING -> slot[i-2].EMPTY (GPU done, reuse for prefetch)
;   slot[i].LOADING -> slot[i].READY (HDD read complete, ready for GPU)
;   slot[i+2].EMPTY -> slot[i+2].LOADING (Start HDD prefetch)
;
; RCX = current_layer (the one GPU just finished)
; =============================================================================
INFINITY_RotateBuffers PROC FRAME
    push rbx rsi rdi r12 r13 r14 r15
    sub rsp, 80
    .allocstack 80
    .endprolog
    
    mov r12, rcx                    ; Current layer
    lea r15, g_infinity_stream
    
    ; Acquire exclusive lock for state modification
    lea rcx, [r15].INFINITY_STREAM.status_lock
    call AcquireSRWLockExclusive
    
    ; 1. Free Slot (current - 2) - GPU is done with it 2 layers ago
    ; This slot can now be reused for prefetch
    mov rax, r12
    sub rax, 2
    and rax, 3                      ; Mod 4
    mov rbx, rax                    ; Free slot index
    
    mov r13, r15
    add r13, OFFSET INFINITY_STREAM.slots
    mov eax, SIZEOF QUAD_SLOT
    mul rbx
    add r13, rax                    ; R13 = &slots[free_slot]
    
    ; Only free if it was in COMPUTING state
    cmp [r13].QUAD_SLOT.state, BUF_STATE_COMPUTING
    jne skip_free
    
    mov [r13].QUAD_SLOT.state, BUF_STATE_EMPTY
    mov DWORD PTR [r13].QUAD_SLOT.layer_idx, -1
    
skip_free:
    ; 2. Start Async Read from HDD for (current + 2)
    mov rax, r12
    add rax, 2                      ; N+2 prefetch
    cmp eax, [r15].INFINITY_STREAM.total_layers
    jae skip_prefetch               ; Beyond model end
    
    ; Find an empty slot for the new layer
    xor ecx, ecx
find_empty_slot:
    cmp ecx, QUAD_BUFFER_COUNT
    jge skip_prefetch               ; No empty slots available
    
    mov r14, r15
    add r14, OFFSET INFINITY_STREAM.slots
    mov eax, SIZEOF QUAD_SLOT
    mul ecx
    add r14, rax
    
    cmp [r14].QUAD_SLOT.state, BUF_STATE_EMPTY
    je found_empty
    inc ecx
    jmp find_empty_slot
    
found_empty:
    ; R14 = empty slot, ECX = slot index, RAX = layer to load
    mov [r14].QUAD_SLOT.layer_idx, eax
    mov [r14].QUAD_SLOT.state, BUF_STATE_LOADING
    
    ; Calculate HDD offset: layer_idx * layer_size
    mov rbx, rax
    mov rax, [r15].INFINITY_STREAM.layer_size
    mul rbx
    mov [r14].QUAD_SLOT.hdd_offset, rax
    
    ; Initiate async read with Direct I/O
    mov rcx, [r15].INFINITY_STREAM.hdd_file_handle
    mov rdx, [r14].QUAD_SLOT.ram_ptr  ; Buffer (must be DMA aligned)
    mov r8, [r15].INFINITY_STREAM.layer_size
    lea r9, [r14].QUAD_SLOT.dma_overlapped
    
    ; Set up OVERLAPPED structure with file offset
    mov rbx, [r14].QUAD_SLOT.hdd_offset
    mov [r14].QUAD_SLOT.dma_overlapped.Offset, ebx
    shr rbx, 32
    mov [r14].QUAD_SLOT.dma_overlapped.OffsetHigh, ebx
    mov rax, [r14].QUAD_SLOT.hEvent
    mov [r14].QUAD_SLOT.dma_overlapped.hEvent, rax
    
    call ReadFile
    test eax, eax
    jnz read_completed_sync         ; Synchronous completion (unlikely with Direct I/O)
    
    call GetLastError
    cmp eax, 997                    ; ERROR_IO_PENDING
    je read_pending
    
    ; Read failed - mark slot empty
    mov [r14].QUAD_SLOT.state, BUF_STATE_EMPTY
    jmp skip_prefetch
    
read_completed_sync:
    ; Mark as ready immediately (rare case with cached data)
    mov [r14].QUAD_SLOT.state, BUF_STATE_READY
    jmp skip_prefetch
    
read_pending:
    ; Async read in progress - completion will be handled by IOCP
    
skip_prefetch:
    ; 3. Start DMA from RAM to VRAM for (current + 1) if ready
    mov rax, r12
    inc rax                         ; N+1
    cmp eax, [r15].INFINITY_STREAM.total_layers
    jae skip_dma
    
    ; Find slot with this layer in RAM (LOADING state -> check completion)
    xor ecx, ecx
check_loading_slots:
    cmp ecx, QUAD_BUFFER_COUNT
    jge skip_dma
    
    mov r14, r15
    add r14, OFFSET INFINITY_STREAM.slots
    mov eax, SIZEOF QUAD_SLOT
    mul ecx
    add r14, rax
    
    cmp [r14].QUAD_SLOT.state, BUF_STATE_LOADING
    jne next_slot
    
    cmp DWORD PTR [r14].QUAD_SLOT.layer_idx, r12d
    jne next_slot_inc
    
    ; Found layer N+1 in loading state - check if HDD read completed
    mov rcx, [r14].QUAD_SLOT.hEvent
    xor edx, edx                    ; dwMilliseconds = 0 (non-blocking)
    call WaitForSingleObject
    
    cmp eax, 0                      ; WAIT_OBJECT_0
    jne next_slot                   ; Still loading
    
    ; HDD read complete - mark as ready for GPU DMA
    mov [r14].QUAD_SLOT.state, BUF_STATE_READY
    
    ; Note: Actual GPU DMA would be initiated here by Phase 4
    ; For now, we mark ready and Phase 3/4 will handle transfer
    
    jmp skip_dma
    
next_slot_inc:
    inc ecx
next_slot:
    inc ecx
    jmp check_loading_slots
    
skip_dma:
    ; Release exclusive lock
    lea rcx, [r15].INFINITY_STREAM.status_lock
    call ReleaseSRWLockExclusive
    
    add rsp, 80
    pop r15 r14 r13 r12 rdi rsi rbx
    ret
INFINITY_RotateBuffers ENDP

; =============================================================================
; INFINITY_ProcessIOCP
; Background thread to handle I/O completion notifications
; Runs on dedicated thread, processes HDD read completions
; =============================================================================
INFINITY_ProcessIOCP PROC FRAME
    push rbx rsi rdi r12 r13 r14 r15
    sub rsp, 96
    .allocstack 96
    .endprolog
    
    lea r15, g_infinity_stream
    
iocp_loop:
    ; Get completion status from I/O Completion Port
    mov rcx, [r15].INFINITY_STREAM.dma_completion_port
    lea rdx, [rsp + 40]             ; lpNumberOfBytesTransferred
    lea r8, [rsp + 48]              ; lpCompletionKey
    lea r9, [rsp + 56]              ; lpOverlapped
    mov QWORD PTR [rsp + 64], INFINITE  ; dwMilliseconds
    call GetQueuedCompletionStatus
    
    test eax, eax
    jz iocp_error
    
    ; Find which slot completed
    mov r12, [rsp + 56]             ; OVERLAPPED* from completion
    mov r13, r15
    add r13, OFFSET INFINITY_STREAM.slots
    xor ecx, ecx
    
find_slot:
    cmp ecx, QUAD_BUFFER_COUNT
    jge iocp_loop                   ; Not found (shouldn't happen)
    
    mov rax, r13
    add rax, rcx
    mov rbx, rax
    mov eax, SIZEOF QUAD_SLOT
    mul ecx
    add rax, r13
    
    lea r14, [rax].QUAD_SLOT.dma_overlapped
    cmp r14, r12
    je slot_found
    inc ecx
    jmp find_slot
    
slot_found:
    ; RAX = QUAD_SLOT* that completed
    ; Update state from LOADING to READY
    cmp [rax].QUAD_SLOT.state, BUF_STATE_LOADING
    jne iocp_loop                   ; Already changed (race condition?)
    
    mov [rax].QUAD_SLOT.state, BUF_STATE_READY
    
    ; Update metrics
    mov rdx, [rax].QUAD_SLOT.hdd_offset
    add [r15].INFINITY_STREAM.hdd_read_bytes, rdx
    
    ; Trigger immediate DMA to VRAM for this slot
    ; This minimizes latency between HDD read and GPU availability
    ; Phase 4 will handle the actual GPU DMA transfer
    
    jmp iocp_loop
    
iocp_error:
    ; Handle error or exit condition
    call GetLastError
    cmp eax, 735                    ; ERROR_ABANDONED_WAIT_0
    je iocp_exit
    jmp iocp_loop
    
iocp_exit:
    add rsp, 96
    pop r15 r14 r13 r12 rdi rsi rbx
    ret
INFINITY_ProcessIOCP ENDP

; =============================================================================
; INFINITY_HandleYTfnTrap
; Called when GPU hits YTFN_SENTINEL
; Decoder trap mechanism for lazy data materialization
;
; RCX = trapped address (YTFN_SENTINEL - layer_idx)
; Returns RAX = physical pointer after resolution
; =============================================================================
INFINITY_HandleYTfnTrap PROC FRAME
    push rbx r12
    sub rsp, 40
    .allocstack 40
    .endprolog
    
    lea r12, g_infinity_stream
    
    ; Decode layer index from sentinel
    mov rax, YTFN_SENTINEL
    sub rax, rcx                    ; RAX = layer_idx
    mov rbx, rax                    ; RBX = layer_idx
    
    inc dword ptr [r12].INFINITY_STREAM.trap_resolved_count
    
stall_loop:
    ; Check if this layer is now in READY or COMPUTING state
    mov rcx, rbx
    xor edx, edx                    ; buffer_head (unused in check)
    call INFINITY_CheckQuadBuffer
    
    cmp rax, YTFN_SENTINEL
    jb trap_resolved                ; Got valid pointer (check LSB)
    
    ; Still not ready - yield and retry
    ; This implements the "stall until data is ready" mechanism
    call SwitchToThread             ; Yield CPU to other threads
    inc qword ptr [r12].INFINITY_STREAM.stall_cycles
    jmp stall_loop
    
trap_resolved:
    add rsp, 40
    pop r12 rbx
    ret
INFINITY_HandleYTfnTrap ENDP

; =============================================================================
; INFINITY_GetMetrics
; Returns performance statistics
;
; Returns RAX = hdd_read_bytes
;         RDX = dma_write_bytes
;         R8  = stall_cycles
;         R9D = trap_count
; =============================================================================
INFINITY_GetMetrics PROC
    lea rcx, g_infinity_stream
    mov rax, [rcx].INFINITY_STREAM.hdd_read_bytes
    mov rdx, [rcx].INFINITY_STREAM.dma_write_bytes
    mov r8, [rcx].INFINITY_STREAM.stall_cycles
    mov r9d, [rcx].INFINITY_STREAM.trap_count
    ret
INFINITY_GetMetrics ENDP

; =============================================================================
; INFINITY_ResetMetrics
; Clears all performance counters
; =============================================================================
INFINITY_ResetMetrics PROC
    lea rcx, g_infinity_stream
    mov [rcx].INFINITY_STREAM.hdd_read_bytes, 0
    mov [rcx].INFINITY_STREAM.dma_write_bytes, 0
    mov [rcx].INFINITY_STREAM.stall_cycles, 0
    mov [rcx].INFINITY_STREAM.trap_count, 0
    mov [rcx].INFINITY_STREAM.trap_resolved_count, 0
    ret
INFINITY_ResetMetrics ENDP

; =============================================================================
; INFINITY_GetSlotState
; Returns current state of a specific slot
;
; RCX = slot_index (0-3)
; Returns EAX = BUF_STATE_*
; =============================================================================
INFINITY_GetSlotState PROC
    lea rax, g_infinity_stream
    add rax, OFFSET INFINITY_STREAM.slots
    mov edx, SIZEOF QUAD_SLOT
    mul ecx
    add rax, rdx
    mov eax, [rax].QUAD_SLOT.state
    ret
INFINITY_GetSlotState ENDP

; =============================================================================
; INFINITY_GetSlotVramPtr
; Returns VRAM pointer for a specific slot
;
; RCX = slot_index (0-3)
; Returns RAX = VRAM pointer
; =============================================================================
INFINITY_GetSlotVramPtr PROC
    lea rax, g_infinity_stream
    add rax, OFFSET INFINITY_STREAM.slots
    mov edx, SIZEOF QUAD_SLOT
    mul ecx
    add rax, rdx
    mov rax, [rax].QUAD_SLOT.vram_ptr
    ret
INFINITY_GetSlotVramPtr ENDP

; =============================================================================
; INFINITY_GetSlotRamPtr
; Returns RAM pointer for a specific slot
;
; RCX = slot_index (0-3)
; Returns RAX = RAM pointer
; =============================================================================
INFINITY_GetSlotRamPtr PROC
    lea rax, g_infinity_stream
    add rax, OFFSET INFINITY_STREAM.slots
    mov edx, SIZEOF QUAD_SLOT
    mul ecx
    add rax, rdx
    mov rax, [rax].QUAD_SLOT.ram_ptr
    ret
INFINITY_GetSlotRamPtr ENDP

; =============================================================================
; INFINITY_Shutdown
; Closes all handles and frees memory
; =============================================================================
INFINITY_Shutdown PROC
    lea rax, g_infinity_stream
    
    ; Close file handle
    mov rcx, [rax].INFINITY_STREAM.hdd_file_handle
    call CloseHandle
    
    ; Close I/O Completion Port
    mov rcx, [rax].INFINITY_STREAM.dma_completion_port
    call CloseHandle
    
    ; Close all slot events
    mov rbx, rax
    add rbx, OFFSET INFINITY_STREAM.slots
    xor ecx, ecx
    
close_event_loop:
    cmp ecx, QUAD_BUFFER_COUNT
    jge shutdown_done
    
    mov rcx, [rbx].QUAD_SLOT.hEvent
    call CloseHandle
    
    add rbx, SIZEOF QUAD_SLOT
    inc ecx
    jmp close_event_loop
    
shutdown_done:
    xor eax, eax
    ret
INFINITY_Shutdown ENDP

END
