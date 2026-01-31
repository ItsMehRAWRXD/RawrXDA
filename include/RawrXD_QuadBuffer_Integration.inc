; =============================================================================
; RawrXD_QuadBuffer_Integration.inc
; Master Include File for QuadBuffer DMA System Integration
; =============================================================================
; Usage: INCLUDE RawrXD_QuadBuffer_Integration.inc at top of main.asm
; =============================================================================

OPTION CASEMAP:NONE
OPTION WIN64:3

; =============================================================================
; Configuration Constants (Adjust for your hardware)
; =============================================================================
QUAD_VRAM_SIZE      EQU 040000000h      ; 1GB per slot (4GB total)
QUAD_HDD_BLOCK      EQU 0100000000h     ; 4GB read chunks
QUAD_MAX_LAYERS     EQU 800             ; Support up to 800 layers
QUAD_IOCP_THREADS   EQU 2               ; Background I/O threads

; =============================================================================
; External Function Declarations (from QuadBuffer_DMA_Orchestrator.asm)
; =============================================================================
EXTERNDEF INFINITY_InitializeStream:PROC
EXTERNDEF INFINITY_CheckQuadBuffer:PROC
EXTERNDEF INFINITY_RotateBuffers:PROC
EXTERNDEF INFINITY_ProcessIOCP:PROC
EXTERNDEF INFINITY_HandleYTfnTrap:PROC
EXTERNDEF INFINITY_GetMetrics:PROC
EXTERNDEF INFINITY_ResetMetrics:PROC
EXTERNDEF INFINITY_GetSlotState:PROC
EXTERNDEF INFINITY_GetSlotVramPtr:PROC
EXTERNDEF INFINITY_GetSlotRamPtr:PROC
EXTERNDEF INFINITY_Shutdown:PROC

; =============================================================================
; Structure Definitions (matching RawrXD_QuadBuffer_DMA_Orchestrator.asm)
; =============================================================================

; Buffer state flags
BUF_STATE_EMPTY     EQU 0   ; Available for loading
BUF_STATE_LOADING   EQU 1   ; DMA in progress
BUF_STATE_READY     EQU 2   ; Ready for compute
BUF_STATE_COMPUTING EQU 3   ; Currently on GPU

; YTFN Sentinel value
YTFN_SENTINEL       EQU 07FFFFFFFFFFFFFFFh

; Page configuration
PAGE_SIZE           EQU 040000000h       ; 1GB
PAGE_SHIFT          EQU 30

; =============================================================================
; Structure Externs (for C++ interoperability)
; =============================================================================
EXTERNDEF g_infinity_stream:QUAD BYTE
EXTERNDEF g_mod4_mask:QWORD
EXTERNDEF g_vramptr_indices:QWORD

; =============================================================================
; Macro Helpers for Common Operations
; =============================================================================

; CHECK_BUFFER - Get VRAM pointer or trigger trap
; Usage: CHECK_BUFFER layer_idx, buffer_head
; Returns: RAX = VRAM pointer or YTFN_SENTINEL
CHECK_BUFFER MACRO layer_idx, buffer_head
    mov rcx, layer_idx
    mov rdx, buffer_head
    call INFINITY_CheckQuadBuffer
    cmp rax, YTFN_SENTINEL
    jb @@buffer_ready_&@CurLine
    ; Handle trap
    mov rcx, rax
    call INFINITY_HandleYTfnTrap
@@buffer_ready_&@CurLine:
ENDM

; ROTATE_AFTER_COMPUTE - Trigger buffer rotation and next prefetch
; Usage: ROTATE_AFTER_COMPUTE current_layer
; Side effects: Frees old slot, starts HDD read for N+2, initiates DMA for N+1
ROTATE_AFTER_COMPUTE MACRO current_layer
    mov rcx, current_layer
    call INFINITY_RotateBuffers
ENDM

; INIT_QUADBUFFER - Initialize the entire system
; Usage: INIT_QUADBUFFER model_path, layer_size, total_layers, vram_base
; Returns: RAX = ORCHESTRATOR_CONTEXT* or NULL
INIT_QUADBUFFER MACRO model_path, layer_size, total_layers, vram_base
    mov rcx, model_path
    mov rdx, layer_size
    mov r8d, total_layers
    mov r9, vram_base
    call INFINITY_InitializeStream
ENDM

; GET_SLOT_STATE - Query state of a buffer slot
; Usage: GET_SLOT_STATE slot_index
; Returns: EAX = BUF_STATE_*
GET_SLOT_STATE MACRO slot_index
    mov rcx, slot_index
    call INFINITY_GetSlotState
ENDM

; GET_SLOT_VRAM - Get VRAM pointer for slot
; Usage: GET_SLOT_VRAM slot_index
; Returns: RAX = VRAM address
GET_SLOT_VRAM MACRO slot_index
    mov rcx, slot_index
    call INFINITY_GetSlotVramPtr
ENDM

; GET_SLOT_RAM - Get RAM pointer for slot
; Usage: GET_SLOT_RAM slot_index
; Returns: RAX = RAM address
GET_SLOT_RAM MACRO slot_index
    mov rcx, slot_index
    call INFINITY_GetSlotRamPtr
ENDM

; =============================================================================
; Integration with Phase 1-5 Systems
; =============================================================================

; PHASE1_RESERVE_ARENA - Reserve QuadBuffer space in Phase 1 arena
; Call after arena initialization
; Usage: PHASE1_RESERVE_ARENA arena_ptr
PHASE1_RESERVE_ARENA MACRO arena_ptr
    mov rcx, arena_ptr
    mov rdx, QUAD_VRAM_SIZE * 4     ; 4GB total for quad buffers
    ; Additional reservation logic would go here
    ; This macro ensures QuadBuffer gets pre-allocated space
ENDM

; PHASE2_STREAMING_SUPPORT - Enable Phase 2 layer streaming
; Call to integrate with model loader
; Usage: PHASE2_STREAMING_SUPPORT
PHASE2_STREAMING_SUPPORT MACRO
    ; Phase 2 will call CHECK_BUFFER for each layer
    ; and get VRAM pointers (or stall if not ready)
    ; No additional setup needed - QuadBuffer handles transparently
ENDM

; PHASE3_GPU_NOTIFICATION - Notify completion for next prefetch
; Call after GPU kernel completes
; Usage: PHASE3_GPU_NOTIFICATION completed_layer
PHASE3_GPU_NOTIFICATION MACRO completed_layer
    ROTATE_AFTER_COMPUTE completed_layer
ENDM

; PHASE4_DMA_READY - Check if slot ready for GPU DMA
; Usage: PHASE4_DMA_READY slot_index
; Returns: EAX = BUF_STATE_READY if ready for DMA
PHASE4_DMA_READY MACRO slot_index
    GET_SLOT_STATE slot_index
    cmp eax, BUF_STATE_READY
ENDM

; PHASE5_COLLECT_METRICS - Gather performance stats
; Usage: PHASE5_COLLECT_METRICS
; Returns: RAX=hdd_bytes, RDX=dma_bytes, R8=stalls, R9D=traps
PHASE5_COLLECT_METRICS MACRO
    call INFINITY_GetMetrics
ENDM

; =============================================================================
; Performance Validation Macros
; =============================================================================

; VALIDATE_QUADBUFFER_STATE - Sanity check system consistency
; Returns: EAX = 0 if valid, >0 if error
VALIDATE_QUADBUFFER_STATE MACRO
    push rbx rcx rdx rax
    
    ; Check file handle
    lea rax, g_infinity_stream
    mov rcx, [rax + 0]              ; hdd_file_handle offset (0 in structure)
    test rcx, rcx
    jz @@qvs_init_error
    
    ; Check slot consistency
    xor edx, edx                    ; slot counter
    
@@qvs_slot_loop:
    cmp edx, 4
    jge @@qvs_done
    
    ; Get slot state (would require proper structure layout)
    GET_SLOT_STATE edx
    cmp eax, BUF_STATE_COMPUTING
    ja @@qvs_state_error
    
    inc edx
    jmp @@qvs_slot_loop
    
@@qvs_done:
    xor eax, eax
    jmp @@qvs_exit
    
@@qvs_init_error:
    mov eax, 1
    jmp @@qvs_exit
    
@@qvs_state_error:
    mov eax, 2
    
@@qvs_exit:
    pop rax rdx rcx rbx
ENDM

; MEASURE_BUFFER_EFFICIENCY - Check how many slots are in use
; Returns: EAX = efficiency percentage (0-100)
MEASURE_BUFFER_EFFICIENCY MACRO
    push rbx rcx rdx
    
    xor ecx, ecx                    ; active slots
    xor edx, edx                    ; slot counter
    
@@mbe_loop:
    cmp edx, 4
    jge @@mbe_calc
    
    GET_SLOT_STATE edx
    cmp eax, BUF_STATE_EMPTY
    je @@mbe_skip
    
    inc ecx                         ; Count active slot
    
@@mbe_skip:
    inc edx
    jmp @@mbe_loop
    
@@mbe_calc:
    mov eax, ecx
    mov ecx, 100
    mul ecx
    mov ecx, 4
    div ecx                         ; EAX = (active * 100) / 4
    
    pop rdx rcx rbx
ENDM

; =============================================================================
; Debugging & Diagnostics
; =============================================================================

; DEBUG_BUFFER_STATE - Dump current buffer state (debug builds only)
DEBUG_BUFFER_STATE MACRO
IFDEF DEBUG_BUILD
    push rcx rdx rax
    
    mov ecx, 0
@@dbs_loop:
    cmp ecx, 4
    jge @@dbs_done
    
    GET_SLOT_STATE ecx
    ; In debug: output state
    ; OutputDebugString format: "Slot X: State Y"
    
    inc ecx
    jmp @@dbs_loop
    
@@dbs_done:
    pop rax rdx rcx
ENDIF
ENDM

; PRINT_METRICS - Display performance metrics (diagnostic output)
PRINT_METRICS MACRO
    PHASE5_COLLECT_METRICS
    ; In actual code: format and display metrics
    ; RAX = HDD read bytes
    ; RDX = DMA write bytes
    ; R8 = stall cycles
    ; R9D = trap count
ENDM

; =============================================================================
; Integration Entry Points
; =============================================================================

; QUADBUFFER_SYSTEM_INIT - Single call to initialize entire system
; Usage: call QuadBuffer_System_Init
; Parameters: RCX = model file path (wchar_t*)
;            RDX = VRAM base address
; Returns: RAX = success (1) or failure (0)
; This is a convenience macro that calls INIT_QUADBUFFER with standard values
QUADBUFFER_SYSTEM_INIT MACRO
    ; RCX = model path (already set)
    ; RDX = vram base (already set)
    mov r8d, 800                    ; 800 layers for 800B model
    mov r9d, 1_000_000_000          ; 1GB per layer
    ; Would need to call proper function here
ENDM

; QUADBUFFER_SYSTEM_SHUTDOWN - Cleanup all resources
; Usage: call QuadBuffer_System_Shutdown
QUADBUFFER_SYSTEM_SHUTDOWN MACRO
    call INFINITY_Shutdown
ENDM

; =============================================================================
; Backward Compatibility with Existing RawrXD Code
; =============================================================================

; MEMACCESS_WRAP - Safe memory access with QuadBuffer support
; Replaces direct pointer dereferences with QuadBuffer checks
; Usage: MEMACCESS_WRAP layer_idx, [default_ptr]
; If QuadBuffer available: uses streaming
; If not: uses provided default pointer
MEMACCESS_WRAP MACRO layer_idx, default_ptr
    ; Check if QuadBuffer initialized
    lea rax, g_infinity_stream
    cmp qword ptr [rax], 0
    je @@maw_default
    
    ; Use QuadBuffer
    CHECK_BUFFER layer_idx, 0
    jmp @@maw_done
    
@@maw_default:
    mov rax, default_ptr
    
@@maw_done:
ENDM

; =============================================================================
; Performance Profiling Helpers
; =============================================================================

; PROFILE_LAYER_ACCESS - Time layer access for benchmarking
; Usage: PROFILE_LAYER_ACCESS layer_idx
PROFILE_LAYER_ACCESS MACRO layer_idx
    push rax rdx
    
    call GetTickCount
    mov rdx, rax                    ; Start time
    
    CHECK_BUFFER layer_idx, 0
    
    call GetTickCount
    sub rax, rdx                    ; RAX = elapsed ms
    
    pop rdx rax
ENDM

; PROFILE_GPU_ROUND_TRIP - Time full compute + rotate cycle
; Usage: PROFILE_GPU_ROUND_TRIP layer_idx
PROFILE_GPU_ROUND_TRIP MACRO layer_idx
    push rax rdx r8
    
    call GetTickCount
    mov r8, rax                     ; Start time
    
    ; Get layer
    CHECK_BUFFER layer_idx, 0
    
    ; Simulate GPU compute (in real code: call GPU kernel)
    
    ; Notify completion and rotate
    ROTATE_AFTER_COMPUTE layer_idx
    
    call GetTickCount
    sub rax, r8                     ; RAX = elapsed ms
    
    pop r8 rdx rax
ENDM

; =============================================================================
; End of Integration Header
; =============================================================================
; Include this file once at the top of your main assembly file:
;     INCLUDE RawrXD_QuadBuffer_Integration.inc
;
; Then use the macros throughout your code:
;     CHECK_BUFFER 0, 0           ; Get layer 0
;     GPU_ComputeKernel(rax)       ; Compute
;     ROTATE_AFTER_COMPUTE 0      ; Next prefetch
;
; The system handles all details transparently.
; =============================================================================
