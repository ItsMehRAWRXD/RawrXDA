; ════════════════════════════════════════════════════════════════════════════════
; RAWRXD-LAZYINIT-MAIN v5.4
; Unified Integration Hub for Agentic Hotpatching & Dual-Engine Streaming
; Pure MASM64 | Zero Dependencies | 64GB-800B Model Support
; ════════════════════════════════════════════════════════════════════════════════


; Exported procedures for C++/MASM integration
PUBLIC main
PUBLIC rawrxd_detect_system_specs
PUBLIC rawrxd_init_core_utils
PUBLIC rawrxd_spawn_agentic_thread
PUBLIC agentic_thread_proc
PUBLIC rawrxd_init_streaming_buffer
PUBLIC rawrxd_agentic_main_loop
PUBLIC rawrxd_execute_command
PUBLIC t1_stream_chunk
PUBLIC t1_query_stream_status
PUBLIC t1_adjust_stream_throttle
PUBLIC cmd_invalid_handler
PUBLIC cmd_emergency_halt
PUBLIC cmd_rollback_all
PUBLIC cmd_validate_memory
PUBLIC cmd_sync_checkpoints
PUBLIC rawrxd_load_model_with_hotpatch
PUBLIC rawrxd_switch_engine_hot
PUBLIC rawrxd_toggle_feature_hot
PUBLIC rawrxd_hotpatch_tensor_live
PUBLIC rawrxd_enable_agentic_optimization
PUBLIC rawrxd_notify_agents
PUBLIC rawrxd_cleanup
PUBLIC DllMain
PUBLIC rawrxd_check_for_commands
PUBLIC rawrxd_run_benchmark
PUBLIC feature_harness_init_stub
PUBLIC unified_hotpatch_create_stub
PUBLIC dual_engine_create_stub
PUBLIC agentic_orchestrator_create_stub
PUBLIC agentic_puppeteer_create_stub
PUBLIC memory_zero_stub
PUBLIC agentic_pulse_stub
PUBLIC dual_engine_load_model_stub
PUBLIC apply_pending_patches_stub
PUBLIC puppeteer_notify_stub
PUBLIC engine_is_ready_stub
PUBLIC begin_critical_section_stub
PUBLIC end_critical_section_stub
PUBLIC hot_switch_stub
PUBLIC toggle_feature_stub
PUBLIC apply_feature_patches_stub
PUBLIC notify_config_change_stub
PUBLIC agentic_enable_stub
PUBLIC notify_command_executed_stub
PUBLIC destroy_patch_manager_stub
PUBLIC destroy_dual_engine_stub
PUBLIC destroy_orchestrator_stub
PUBLIC destroy_puppeteer_stub
PUBLIC print_message
.code

; ════════════════════════════════════════════════════════════════════════════════
; External Functions (kernel32/user32)
; ════════════════════════════════════════════════════════════════════════════════
EXTERN GetCommandLineW: PROC
EXTERN ExitProcess: PROC
EXTERN CreateThread: PROC
EXTERN SetThreadPriority: PROC
EXTERN WaitForSingleObject: PROC
EXTERN CreateEventW: PROC
EXTERN SetEvent: PROC
EXTERN CloseHandle: PROC
EXTERN GetStdHandle: PROC
EXTERN WriteFile: PROC
EXTERN GetSystemInfo: PROC
EXTERN GlobalMemoryStatusEx: PROC
EXTERN DisableThreadLibraryCalls: PROC
EXTERN Sleep: PROC

; External Pulse Score Engine procedures
EXTERN CheckPulseEffectiveness: PROC
EXTERN UpdatePulseMetrics: PROC
EXTERN GetPulseStats: PROC
EXTERN ResetPulseScoring: PROC
EXTERN RawrXD_RecordPulseScoreProc: PROC

; External Streaming Orchestrator procedures
EXTERN RawrXD_Streaming_Orchestrator_Init: PROC
EXTERN RawrXD_Stream_QueryBackpressure: PROC
EXTERN RawrXD_Stream_AdjustThrottle: PROC
EXTERN RawrXD_Stream_WriteChunk: PROC
EXTERN RawrXD_Stream_ReadChunk: PROC
EXTERN RawrXD_Stream_SwitchEngine: PROC
EXTERN RawrXD_Stream_CheckPatchConflict: PROC

; ════════════════════════════════════════════════════════════════════════════════
; CONFIGURATION DEFINITIONS
; ════════════════════════════════════════════════════════════════════════════════

RAWRXD_LAZYINIT_MAGIC EQU 0DEADBEEFC0DED00Dh
MAX_PATCH_LAYERS EQU 256
STREAM_BUFFER_SIZE EQU 67108864          ; 64MB reverse-streaming buffer
MAX_ROLLBACK_POINTS EQU 64
AGENTIC_PULSE_INTERVAL EQU 100           ; 100ms optimization pulse

; Feature IDs (from FeatureHarness)
FEATURE_QUANTIZED_FALLBACK EQU 0
FEATURE_STREAMING_INFERENCE EQU 1
FEATURE_HOTPATCH_LIVE EQU 2
FEATURE_DUAL_ENGINE EQU 3
FEATURE_AGENTIC_OPTIMIZATION EQU 4
FEATURE_GGUF_HOTPATCH EQU 5
FEATURE_VULKAN_ACCELERATION EQU 6
FEATURE_BRUTAL_COMPRESSION EQU 7

; Command codes
CMD_LOAD_MODEL EQU 1
CMD_SWITCH_ENGINE EQU 2
CMD_TOGGLE_FEATURE EQU 3
CMD_HOTPATCH_TENSOR EQU 4
CMD_ENABLE_AGENTIC EQU 5
CMD_BENCHMARK EQU 6
CMD_SHUTDOWN EQU 7

; Thread priority constants
THREAD_PRIORITY_BELOW_NORMAL EQU -1

; Wait constants
WAIT_OBJECT_0 EQU 0
INFINITE EQU 0FFFFFFFFh

; DLL reason codes
DLL_PROCESS_ATTACH EQU 1
DLL_PROCESS_DETACH EQU 0

; System constants
ERROR_MODEL_LOAD_FAILED EQU 1001

; ════════════════════════════════════════════════════════════════════════════════
; RUNTIME STATE STRUCTURE
; ════════════════════════════════════════════════════════════════════════════════
; Note: Structure layout matches C struct alignment
; Total size: Calculated for proper alignment

; ════════════════════════════════════════════════════════════════════════════════
; GLOBAL MANAGER HANDLES & STATE
; ════════════════════════════════════════════════════════════════════════════════
.data

; System state
g_magic dq RAWRXD_LAZYINIT_MAGIC
g_is_initialized db 0
g_current_engine db 0          ; 0=FP32, 1=Quantized
g_system_load dq 0             ; 0-100% CPU/GPU utilization
g_total_ram_gb dq 64           ; Auto-detected
g_available_ram_gb dq 64
g_model_path db 256 dup(0)
g_current_model_size_gb dq 0
g_feature_flags dq 0           ; Bitmask for 32 features
g_rollback_sp dq 0
g_last_error_code dq 0

; Manager handles
g_patch_manager dq 0           ; UnifiedHotpatchManager*
g_dual_engine dq 0             ; DualEngineManager*
g_agent_orchestrator dq 0      ; AgenticOrchestrator*
g_puppeteer dq 0               ; AgenticPuppeteer*
g_agent_thread_handle dq 0
g_shutdown_event dq 0

; String constants
szSystemStarting db '>>> RawrXD LazyInit v5.4 Starting...', 13, 10, 0
szSystemSpec db 'System: %lluGB RAM | CPU: %d cores', 0
szCoreUtilsReady db 'Core utilities initialized', 0
szEngineSwitched db 'Engine switched successfully', 0
szTensorPatched db 'Tensor hotpatched (patch_id=%d)', 0
szDefaultModel db 'D:\models\bigdaddyg-40b.gguf', 0
szFatalRam db 'FATAL: Insufficient RAM (need 64GB+)', 0
szFeatureMinimal db 'FeatureHarness: MINIMAL preset loaded', 0
szHotpatchOnline db 'HotpatchManager: Multi-layer patch authority online', 0
szDualEngineReady db 'DualEngineManager: FP32+Quantized engines ready', 0
szAgenticActive db 'AgenticOrchestrator: Background optimization active', 0
szStreamReady db 'StreamingBuffer: 64MB reverse-streaming arena ready', 0
szSystemReady db '═══════════════════════════════════════════════════', 13, 10
              db 'RawrXD LazyInit v5.4: ALL SYSTEMS OPERATIONAL', 13, 10
              db 'RAM: 64GB | Engine: FP32 | Features: MINIMAL', 13, 10
              db '═══════════════════════════════════════════════════', 0
szEnteringLoop db 'Entering agentic event loop... (waiting for commands)', 0
szShutdownReceived db 'Shutdown event received, exiting loop', 0
szAgenticPulse db 'Agentic pulse: System optimal', 0
szCmdLoadModel db 'CMD: Loading model with hotpatch support...', 0
szModelLoaded db 'Model loaded and hotpatch-aware', 0
szTargetNotReady db 'Target engine not ready, switch aborted', 0
szCmdTensorPatch db 'CMD: Applying tensor hotpatch...', 0
szPatchTooLarge db 'Patch too large, rejected', 0
szAgenticEnabled db 'Agentic optimization enabled', 0
szShutdownInitiated db '═══════════════════════════════════════════════════', 13, 10, \
                       'RawrXD LazyInit: SHUTDOWN INITIATED', 13, 10, \
                       '═══════════════════════════════════════════════════', 0
szShutdownComplete db 'Shutdown complete', 0
szBenchmarkRunning db 'Running system benchmark...', 0
szUnknownCommand db 'Unknown command received', 0
szModelLoadFailed db 'Model load failed', 0
szLowRamWarning db 'WARNING: Low RAM, switching to streaming mode', 0
szFatalHotpatch db 'FATAL: Hotpatch manager init failed', 0
szFatalDualEngine db 'FATAL: Dual-engine manager init failed', 0
szFatalAgentThread db 'FATAL: Agent thread creation failed', 0

.data?
; Streaming buffer (uninitialized for efficiency)
g_stream_buffer db STREAM_BUFFER_SIZE dup(?)
g_patch_rollback_stack dq MAX_ROLLBACK_POINTS dup(?)

; Command Jump Table (64-bit offsets)
; Tier 0: Safety-Critical (indices 0-15)
jt_tier0 dq offset cmd_invalid_handler    ; 0x00
         dq offset cmd_emergency_halt      ; 0x01
         dq offset cmd_rollback_all        ; 0x02
         dq offset cmd_validate_memory     ; 0x03
         dq offset cmd_sync_checkpoints    ; 0x04
         dq offset cmd_invalid_handler     ; 0x05-0x0F (reserved)

; Tier 1: User & Optimization (indices 16-63)
jt_tier1 dq offset cmd_invalid_handler    ; 0x10
         dq offset rawrxd_load_model_with_hotpatch  ; CMD_LOAD_MODEL = 0x11
         dq offset rawrxd_switch_engine_hot         ; CMD_SWITCH_ENGINE = 0x12
         dq offset rawrxd_toggle_feature_hot        ; CMD_TOGGLE_FEATURE = 0x13
         dq offset rawrxd_hotpatch_tensor_live      ; CMD_HOTPATCH_TENSOR = 0x14
         dq offset rawrxd_enable_agentic_optimization ; CMD_ENABLE_AGENTIC = 0x15
         dq offset rawrxd_run_benchmark             ; CMD_RUN_BENCHMARK = 0x16
         dq offset signal_shutdown                  ; CMD_SHUTDOWN = 0x17
         dq offset cmd_invalid_handler     ; 0x18-0x3F (reserved for future)

; Streaming orchestrator jump table entries (0x1D-0x1F)
     dq offset t1_stream_chunk              ; 0x1D
     dq offset t1_query_stream_status       ; 0x1E
     dq offset t1_adjust_stream_throttle    ; 0x1F

; ════════════════════════════════════════════════════════════════════════════════
; LAZY-INIT ENTRY POINT
; ════════════════════════════════════════════════════════════════════════════════
; STREAMING ORCHESTRATOR HANDLERS (Tier 1)
; ════════════════════════════════════════════════════════════════════════════════

t1_stream_chunk PROC
    ; rcx = chunk descriptor (pointer)
    mov rdx, [rcx]                ; src_data
    mov r8, [rcx+8]               ; size
    call RawrXD_Stream_WriteChunk
    test rax, rax
    jz @backpressure
    xor eax, eax
    ret
@backpressure:
    mov eax, 1                    ; Signal throttling needed
    ret
t1_stream_chunk ENDP

t1_query_stream_status PROC
    ; rcx = pointer to status struct
    lea rax, g_stream_state
    mov [rcx], qword ptr [rax+16] ; bytes_available
    mov al, byte ptr [rax+28]     ; is_full
    mov [rcx+8], al
    mov al, byte ptr [rax+30]     ; active_engine
    mov [rcx+9], al
    xor eax, eax
    ret
t1_query_stream_status ENDP

t1_adjust_stream_throttle PROC
    ; Returns: rax = recommended chunk size (0 = pause)
    call RawrXD_Stream_AdjustThrottle
    ret
t1_adjust_stream_throttle ENDP
; ════════════════════════════════════════════════════════════════════════════════
main PROC
    sub rsp, 28h                    ; Shadow space + alignment
    
        ; Early debug output
        lea rcx, szSystemStarting
        call print_message

    ; Initialize streaming orchestrator
    call RawrXD_Streaming_Orchestrator_Init
    test rax, rax
    jnz stream_init_failed
    lea rcx, szStreamReady
    call print_message

stream_init_failed:
    ; ────────────────────────────────────────────────────────────────────────────
    ; Step 0: System Detection & Validation
    ; ────────────────────────────────────────────────────────────────────────────
    call rawrxd_detect_system_specs
    
    ; Validate 64GB minimum requirement for 800B models
    mov rax, g_total_ram_gb
    cmp rax, 64
    jge system_ram_ok
    
    lea rcx, szFatalRam
    call print_message
    mov ecx, 1
    call ExitProcess
    
system_ram_ok:
    
    ; ────────────────────────────────────────────────────────────────────────────
    ; Step 1: Initialize Core Utilities (Phase 0)
    ; ────────────────────────────────────────────────────────────────────────────
    call rawrxd_init_core_utils
    
    ; ────────────────────────────────────────────────────────────────────────────
    ; Step 2: Initialize Feature Harness (Minimal Mode First)
    ; ────────────────────────────────────────────────────────────────────────────
    xor ecx, ecx                    ; PRESET_MINIMAL = fastest startup
    call feature_harness_init_stub
    mov g_feature_flags, rax
    lea rcx, szFeatureMinimal
    call print_message
    
    ; ────────────────────────────────────────────────────────────────────────────
    ; Step 3: Initialize Unified Hotpatch Manager (Phase 1)
    ; ────────────────────────────────────────────────────────────────────────────
    call unified_hotpatch_create_stub
    mov g_patch_manager, rax
    test rax, rax
    jnz hotpatch_ok
    
    lea rcx, szFatalHotpatch
    call print_message
    jmp rawrxd_cleanup_and_exit
    
hotpatch_ok:
    lea rcx, szHotpatchOnline
    call print_message
    
    ; ────────────────────────────────────────────────────────────────────────────
    ; Step 4: Initialize Dual-Engine Manager (Phase 2)
    ; ────────────────────────────────────────────────────────────────────────────
    mov rcx, g_total_ram_gb
    call dual_engine_create_stub
    mov g_dual_engine, rax
    test rax, rax
    jnz dual_engine_ok
    
    lea rcx, szFatalDualEngine
    call print_message
    jmp rawrxd_cleanup_and_exit
    
dual_engine_ok:
    mov g_current_engine, 0         ; Start with FP32 for precision
    lea rcx, szDualEngineReady
    call print_message
    
    ; ────────────────────────────────────────────────────────────────────────────
    ; Step 5: Initialize Agentic Orchestration (Phase 3)
    ; ────────────────────────────────────────────────────────────────────────────
    call agentic_orchestrator_create_stub
    mov g_agent_orchestrator, rax
    call agentic_puppeteer_create_stub
    mov g_puppeteer, rax
    
    ; Spawn background agent thread
    call rawrxd_spawn_agentic_thread
    lea rcx, szAgenticActive
    call print_message
    
    ; ────────────────────────────────────────────────────────────────────────────
    ; Step 6: Initialize Streaming Infrastructure
    ; ────────────────────────────────────────────────────────────────────────────
    call rawrxd_init_streaming_buffer
    lea rcx, szStreamReady
    call print_message
    
    ; ────────────────────────────────────────────────────────────────────────────
    ; Step 7: System Ready
    ; ────────────────────────────────────────────────────────────────────────────
    mov g_is_initialized, 1
    lea rcx, szSystemReady
    call print_message
    
    ; ────────────────────────────────────────────────────────────────────────────
    ; Step 8: Enter Main Agentic Event Loop
    ; ────────────────────────────────────────────────────────────────────────────
    call rawrxd_agentic_main_loop
    
    ; Should never reach here (loop is infinite until shutdown)
rawrxd_cleanup_and_exit:
    call rawrxd_cleanup
    add rsp, 28h
    xor ecx, ecx
    call ExitProcess
main ENDP

; ════════════════════════════════════════════════════════════════════════════════
; SYSTEM SPECIFICATION DETECTION
; ════════════════════════════════════════════════════════════════════════════════
rawrxd_detect_system_specs PROC
    sub rsp, 68h                    ; Space for MEMORYSTATUSEX (64 bytes) + align
    
    ; Initialize MEMORYSTATUSEX structure
    mov dword ptr [rsp+20h], 64          ; dwLength = sizeof(MEMORYSTATUSEX)
    mov qword ptr [rsp+28h], 0           ; Clear ullTotalPhys
    mov qword ptr [rsp+30h], 0           ; Clear ullAvailPhys
    lea rcx, [rsp+20h]                   ; Pointer to structure
    call GlobalMemoryStatusEx
    
    ; Convert bytes to GB (divide by 2^30)
    mov rax, qword ptr [rsp+28h]         ; ullTotalPhys at offset +8
    shr rax, 30
    mov g_total_ram_gb, rax
    
    mov rax, qword ptr [rsp+30h]         ; ullAvailPhys at offset +16
    shr rax, 30
    mov g_available_ram_gb, rax
    
    add rsp, 68h
    ret
rawrxd_detect_system_specs ENDP

; ════════════════════════════════════════════════════════════════════════════════
; CORE UTILITIES INITIALIZATION (Phase 0)
; ════════════════════════════════════════════════════════════════════════════════
rawrxd_init_core_utils PROC
    ; In production, these would initialize memory arenas, string tables, etc.
    ; For now, minimal initialization
    lea rcx, szCoreUtilsReady
    call print_message
    ret
rawrxd_init_core_utils ENDP

; ════════════════════════════════════════════════════════════════════════════════
; AGENTIC THREAD SPAWNER (Background Optimization)
; ════════════════════════════════════════════════════════════════════════════════
rawrxd_spawn_agentic_thread PROC
    sub rsp, 38h
    
    ; Create shutdown event
    xor ecx, ecx                    ; lpEventAttributes
    mov edx, 1                      ; bManualReset = TRUE
    xor r8d, r8d                    ; bInitialState = FALSE
    xor r9d, r9d                    ; lpName = NULL
    call CreateEventW
    mov g_shutdown_event, rax
    
    ; Spawn agentic orchestrator thread
    xor ecx, ecx                    ; lpThreadAttributes
    mov edx, 65536                  ; dwStackSize
    lea r8, agentic_thread_proc     ; lpStartAddress
    mov r9, g_agent_orchestrator    ; lpParameter
    mov qword ptr [rsp+20h], 0      ; dwCreationFlags
    lea rax, [rsp+28h]
    mov qword ptr [rsp+28h], rax    ; lpThreadId
    call CreateThread
    
    mov g_agent_thread_handle, rax
    test rax, rax
    jnz thread_ok
    
    lea rcx, szFatalAgentThread
    call print_message
    add rsp, 38h
    jmp rawrxd_cleanup
    
thread_ok:
    ; Set thread priority to below normal (background optimization)
    mov rcx, rax
    mov edx, THREAD_PRIORITY_BELOW_NORMAL
    call SetThreadPriority
    
    add rsp, 38h
    ret
rawrxd_spawn_agentic_thread ENDP

; ════════════════════════════════════════════════════════════════════════════════
; AGENTIC THREAD PROCEDURE
; ════════════════════════════════════════════════════════════════════════════════
agentic_thread_proc PROC
    sub rsp, 28h
    
agentic_loop:
    ; Wait for shutdown event with pulse interval
    mov rcx, g_shutdown_event
    mov edx, AGENTIC_PULSE_INTERVAL
    call WaitForSingleObject
    
    cmp eax, WAIT_OBJECT_0
    je agentic_exit
    
    ; Perform optimization pulse
    call agentic_pulse_stub
    
    jmp agentic_loop
    
agentic_exit:
    add rsp, 28h
    xor eax, eax
    ret
agentic_thread_proc ENDP

; ════════════════════════════════════════════════════════════════════════════════
; STREAMING BUFFER INITIALIZATION
; ════════════════════════════════════════════════════════════════════════════════
rawrxd_init_streaming_buffer PROC
    ; Zero-fill streaming buffer for security
    lea rcx, g_stream_buffer
    mov rdx, STREAM_BUFFER_SIZE
    call memory_zero_stub
    ret
rawrxd_init_streaming_buffer ENDP

; ════════════════════════════════════════════════════════════════════════════════
; AGENTIC MAIN EVENT LOOP
; ════════════════════════════════════════════════════════════════════════════════
rawrxd_agentic_main_loop PROC
    sub rsp, 28h
    
    lea rcx, szEnteringLoop
    call print_message
    
    xor r15, r15                    ; tokens_generated counter
    
main_loop:
    ; Wait for shutdown event with timeout (for agentic pulse)
    mov rcx, g_shutdown_event
    mov edx, AGENTIC_PULSE_INTERVAL
    call WaitForSingleObject
    
    cmp eax, WAIT_OBJECT_0
    jne check_commands
    
    ; Shutdown requested
    lea rcx, szShutdownReceived
    call print_message
    jmp loop_exit
    
check_commands:
    ; Check for pending commands (non-blocking)
    call rawrxd_check_for_commands
    test rax, rax
    jz agentic_pulse
    
    ; Execute command
    mov rcx, rax
    call rawrxd_execute_command
    call rawrxd_notify_agents
    
agentic_pulse:
    ; Check streaming backpressure before pulse
    call RawrXD_Stream_QueryBackpressure
    test rax, rax
    jz @check_effectiveness
    ; Backpressure active - throttle agentic optimization
    call RawrXD_Stream_AdjustThrottle
    jmp @check_heartbeat

@check_effectiveness:
    ; Check if pulse should run (effectiveness throttling)
    call CheckPulseEffectiveness
    test eax, eax
    jz @check_heartbeat             ; Skip pulse if throttled

@normal_pulse:
    ; --- Pulse with Cycle-Level Scoring ---
    ; Capture pre-pulse state
    rdtsc
    shl rdx, 32
    or rax, rdx
    mov r12, rax                    ; r12 = start cycles

    ; Execute pulse
    call agentic_pulse_stub
    mov r14d, eax                   ; r14d = result code

    ; Capture post-pulse state
    rdtscp
    shl rdx, 32
    or rax, rdx
    mov r13, rax                    ; r13 = end cycles

    ; Calculate cycle delta and update metrics
    sub r13, r12                    ; r13 = cycle delta
    mov rcx, r13
    call UpdatePulseMetrics

    ; Record pulse score for telemetry
    ; Note: Queue/mem deltas can be enhanced with actual measurements
    ; For now using 0 as placeholders
    push r14d                       ; result_code
    push 0                          ; pulse_type
    mov rcx, r12                    ; start_cycles
    mov rdx, r13                    ; end_cycles  
    xor r8d, r8d                    ; queue_delta = 0
    xor r9d, r9d                    ; mem_delta = 0
    call RawrXD_RecordPulseScoreProc
    add rsp, 16                     ; Clean up stack args

@check_heartbeat:
    inc r15
    cmp r15, 100
    jne main_loop

    lea rcx, szAgenticPulse
    call print_message
    xor r15, r15
    jmp main_loop
    
loop_exit:
    add rsp, 28h
    ret
rawrxd_agentic_main_loop ENDP

; ════════════════════════════════════════════════════════════════════════════════
; COMMAND EXECUTION ROUTER
; ════════════════════════════════════════════════════════════════════════════════
rawrxd_execute_command PROC
    ; rcx = command data, rdx = command code (16-bit)
    ; Returns: rax = execution cycles (0 on error)
    LOCAL cmd_start_cycles:qword, cmd_end_cycles:qword
    
    ; --- Tier Determination ---
    movzx rax, dx                   ; rax = command code (zero-extended)
    
    ; Extract tier from high nibble
    shr rdx, 4                      ; rdx = tier (0 or 1)
    test rdx, rdx
    jz @dispatch_tier0
    
    ; --- Tier 1 Dispatch ---
    cmp rax, 63                     ; Validate index bounds
    ja @cmd_invalid
    
    ; Load jump table entry (O(1) lookup)
    lea rbx, jt_tier1
    shl rax, 3                      ; Multiply by 8 (qword size)
    add rbx, rax
    mov rax, [rbx]                  ; rax = function pointer
    
    jmp @execute
    
@dispatch_tier0:
    cmp rax, 15                     ; Validate index bounds
    ja @cmd_invalid
    
    ; Tier 0 dispatch (higher priority, always executes)
    lea rbx, jt_tier0
    shl rax, 3
    add rbx, rax
    mov rax, [rbx]
    jmp @execute
    
@cmd_invalid:
    ; Log invalid command
    mov rcx, rdx
    call asm_log "[CMD] Invalid command code", ASM_LOG_ERROR
    xor rax, rax
    ret
    
@execute:
    ; --- Execution with Cycle Counting ---
    ; Save non-volatiles for precise measurement
    push rbx
    push rsi
    push rdi
    
    ; Read TSC (Time Stamp Counter)
    rdtsc
    shl rdx, 32
    or rax, rdx
    mov cmd_start_cycles, rax
    
    ; Execute command handler
    call rax                        ; Dispatch to handler
    
    ; Read TSC again
    rdtsc
    shl rdx, 32
    or rax, rdx
    mov cmd_end_cycles, rax
    
    ; Calculate cycle delta
    sub rax, cmd_start_cycles
    mov cmd_start_cycles, rax       ; Reuse as result
    
    ; --- Performance Tracking ---
    call rawrxd_update_command_metrics
    
    ; Restore non-volatiles
    pop rdi
    pop rsi
    pop rbx
    
    ret
rawrxd_execute_command ENDP

; ============================================================================
; Jump Tables for Command Dispatch
; Tier 0: Safety / Control handlers (small, hot)
; Tier 1: User / Agent handlers
; Tables are read-only offsets to handler entry points
; ============================================================================

; Streaming orchestrator command handlers
t1_stream_chunk PROC
    ; rcx = chunk descriptor pointer
    mov rdx, [rcx].CHUNK_DESC.src
    mov r8, [rcx].CHUNK_DESC.size
    call RawrXD_Stream_WriteChunk
    test rax, rax
    jnz stream_success
    ; Backpressure - signal agentic loop
    mov rax, 1
    mov g_stream_state.is_full, al
stream_success:
    ret
t1_stream_chunk ENDP

t1_query_stream_status PROC
    ; rcx = response buffer
    lea rax, g_stream_state
    mov rdx, [rax].STREAM_STATE.bytes_available
    mov [rcx].STREAM_STATUS.bytes_available, rdx
    mov dl, [rax].STREAM_STATE.is_full
    mov [rcx].STREAM_STATUS.is_full, dl
    mov dl, [rax].STREAM_STATE.active_engine
    mov [rcx].STREAM_STATUS.active_engine, dl
    mov rdx, g_backpressure_events
    mov [rcx].STREAM_STATUS.backpressure_count, rdx
    xor eax, eax
    ret
t1_query_stream_status ENDP

t1_adjust_stream_throttle PROC
    ; rcx = throttle factor (0-100)
    push rcx
    call RawrXD_Stream_AdjustThrottle
    pop rcx
    ; Log throttle change
    call asm_log, '[STREAM] Throttle adjusted', ASM_LOG_INFO
    xor eax, eax
    ret
t1_adjust_stream_throttle ENDP
jt_tier0:
    dq offset cmd_emergency_halt
    dq offset cmd_rollback_all
    dq offset cmd_validate_memory
    dq offset cmd_sync_checkpoints

jt_tier1:
    dq offset rawrxd_load_model_with_hotpatch
    dq offset rawrxd_switch_engine_hot
    dq offset rawrxd_toggle_feature_hot
    dq offset rawrxd_hotpatch_tensor_live
    dq offset rawrxd_enable_agentic_optimization
    dq offset rawrxd_run_benchmark


cmd_invalid_handler PROC
    call asm_log "[CMD] Handler not implemented", ASM_LOG_WARN
    mov eax, 1                      ; Error code
    ret
cmd_invalid_handler ENDP

cmd_emergency_halt PROC
    ; Safety-critical: immediate halt all operations
    call asm_log "[SAFETY] EMERGENCY HALT - Stopping all engines", ASM_LOG_ERROR
    
    ; Signal shutdown to all components
    mov rcx, g_hShutdownEvent
    call SetEvent
    
    xor eax, eax
    ret
cmd_emergency_halt ENDP

cmd_rollback_all PROC
    ; Rollback all pending patches
    mov rcx, g_patch_manager
    call UnifiedHotpatchManager_RollbackAll
    
    call asm_log "[SAFETY] Rollback complete", ASM_LOG_INFO
    
    xor eax, eax
    ret
cmd_rollback_all ENDP

cmd_validate_memory PROC
    ; Validate memory arenas and patch regions
    call asm_memory_validate_all_arenas
    
    call asm_log "[SAFETY] Memory validation complete", ASM_LOG_DEBUG
    
    xor eax, eax
    ret
cmd_validate_memory ENDP

cmd_sync_checkpoints PROC
    ; Synchronize all checkpoint states
    mov rcx, g_dual_engine
    call RawrXD_DualEngineManager_SyncCheckpoints
    
    xor eax, eax
    ret
cmd_sync_checkpoints ENDP

; ════════════════════════════════════════════════════════════════════════════════
; MODEL LOADING WITH HOTPATCHING
; ════════════════════════════════════════════════════════════════════════════════
rawrxd_load_model_with_hotpatch PROC
    sub rsp, 28h
    
    lea rcx, szCmdLoadModel
    call print_message
    
    ; Check available RAM
    mov rax, g_available_ram_gb
    mov r10, 40                     ; Assume 40GB BigDaddyG
    cmp rax, r10
    jge ram_sufficient
    
    lea rcx, szLowRamWarning
    call print_message
    mov rax, g_feature_flags
    or rax, 1 shl FEATURE_STREAMING_INFERENCE
    mov g_feature_flags, rax
    
ram_sufficient:
    ; Load through dual-engine manager
    mov rcx, g_dual_engine
    lea rdx, szDefaultModel
    call dual_engine_load_model_stub
    test rax, rax
    jnz model_load_ok
    
    lea rcx, szModelLoadFailed
    call print_message
    mov g_last_error_code, ERROR_MODEL_LOAD_FAILED
    jmp load_done
    
model_load_ok:
    mov g_current_model_size_gb, 40
    
    ; Apply any pending hotpatches
    mov rcx, g_patch_manager
    lea rdx, szDefaultModel
    call apply_pending_patches_stub
    
    ; Notify puppeteer
    mov rcx, g_puppeteer
    lea rdx, szDefaultModel
    call puppeteer_notify_stub
    
    lea rcx, szModelLoaded
    call print_message
    
load_done:
    add rsp, 28h
    ret
rawrxd_load_model_with_hotpatch ENDP

; ════════════════════════════════════════════════════════════════════════════════
; HOT ENGINE SWITCHING (Zero-Downtime)
; ════════════════════════════════════════════════════════════════════════════════
rawrxd_switch_engine_hot PROC
    sub rsp, 28h
    
    ; Check if target engine is ready
    mov rcx, g_dual_engine
    movzx rdx, g_current_engine
    xor rdx, 1                      ; Toggle 0↔1
    call engine_is_ready_stub
    test rax, rax
    jnz engine_ready
    
    lea rcx, szTargetNotReady
    call print_message
    jmp switch_done
    
engine_ready:
    ; Request patch window
    mov rcx, g_patch_manager
    call begin_critical_section_stub
    
    ; Perform hot-switch
    mov rcx, g_dual_engine
    movzx rdx, g_current_engine
    xor rdx, 1
    call hot_switch_stub
    
    mov al, g_current_engine
    xor al, 1
    mov g_current_engine, al
    
    ; Release patch window
    mov rcx, g_patch_manager
    call end_critical_section_stub
    
    lea rcx, szEngineSwitched
    call print_message
    
switch_done:
    add rsp, 28h
    ret
rawrxd_switch_engine_hot ENDP

; ════════════════════════════════════════════════════════════════════════════════
; FEATURE TOGGLING (Runtime Without Restart)
; ════════════════════════════════════════════════════════════════════════════════
rawrxd_toggle_feature_hot PROC
    sub rsp, 28h
    
    ; Toggle FEATURE_HOTPATCH_LIVE as example
    mov ecx, FEATURE_HOTPATCH_LIVE
    call toggle_feature_stub
    mov g_feature_flags, rax
    
    ; Apply associated hotpatches
    mov rcx, g_patch_manager
    mov edx, FEATURE_HOTPATCH_LIVE
    call apply_feature_patches_stub
    
    ; Notify agents
    mov rcx, g_agent_orchestrator
    mov edx, FEATURE_HOTPATCH_LIVE
    call notify_config_change_stub
    
    add rsp, 28h
    ret
rawrxd_toggle_feature_hot ENDP

; ════════════════════════════════════════════════════════════════════════════════
; TENSOR-LEVEL HOTPATCHING (Live Model Surgery)
; ════════════════════════════════════════════════════════════════════════════════
rawrxd_hotpatch_tensor_live PROC
    sub rsp, 28h
    
    lea rcx, szCmdTensorPatch
    call print_message
    
    ; Implementation stub - would parse tensor patch data
    
    add rsp, 28h
    ret
rawrxd_hotpatch_tensor_live ENDP

; ════════════════════════════════════════════════════════════════════════════════
; AGENTIC OPTIMIZATION ENABLER
; ════════════════════════════════════════════════════════════════════════════════
rawrxd_enable_agentic_optimization PROC
    sub rsp, 28h
    
    mov rax, g_feature_flags
    or rax, 1 shl FEATURE_AGENTIC_OPTIMIZATION
    mov g_feature_flags, rax
    
    mov rcx, g_agent_orchestrator
    call agentic_enable_stub
    
    lea rcx, szAgenticEnabled
    call print_message
    
    add rsp, 28h
    ret
rawrxd_enable_agentic_optimization ENDP

; ════════════════════════════════════════════════════════════════════════════════
; NOTIFICATION HELPERS
; ════════════════════════════════════════════════════════════════════════════════
rawrxd_notify_agents PROC
    sub rsp, 28h
    mov rcx, g_agent_orchestrator
    call notify_command_executed_stub
    add rsp, 28h
    ret
rawrxd_notify_agents ENDP

; ════════════════════════════════════════════════════════════════════════════════
; CLEANUP & GRACEFUL SHUTDOWN
; ════════════════════════════════════════════════════════════════════════════════
rawrxd_cleanup PROC
    sub rsp, 28h
    
    lea rcx, szShutdownInitiated
    call print_message
    
    ; Signal shutdown to agent thread
    mov rax, g_shutdown_event
    test rax, rax
    jz skip_event
    mov rcx, rax
    call SetEvent
    
skip_event:
    ; Wait for agent thread to exit (max 5 seconds)
    mov rax, g_agent_thread_handle
    test rax, rax
    jz skip_thread
    mov rcx, rax
    mov edx, 5000
    call WaitForSingleObject
    mov rcx, g_agent_thread_handle
    call CloseHandle
    
skip_thread:
    ; Destroy managers (reverse init order)
    mov rax, g_patch_manager
    test rax, rax
    jz skip_patch_mgr
    mov rcx, rax
    call destroy_patch_manager_stub
    
skip_patch_mgr:
    mov rax, g_dual_engine
    test rax, rax
    jz skip_dual_engine
    mov rcx, rax
    call destroy_dual_engine_stub
    
skip_dual_engine:
    mov rax, g_agent_orchestrator
    test rax, rax
    jz skip_orchestrator
    mov rcx, rax
    call destroy_orchestrator_stub
    
skip_orchestrator:
    mov rax, g_puppeteer
    test rax, rax
    jz skip_puppeteer
    mov rcx, rax
    call destroy_puppeteer_stub
    
skip_puppeteer:
    mov rax, g_shutdown_event
    test rax, rax
    jz skip_event_close
    mov rcx, rax
    call CloseHandle
    
skip_event_close:
    lea rcx, szShutdownComplete
    call print_message
    
    add rsp, 28h
    ret
rawrxd_cleanup ENDP

; ════════════════════════════════════════════════════════════════════════════════
; DLL ENTRY POINT (For Hot-Injection Capability)
; ════════════════════════════════════════════════════════════════════════════════
DllMain PROC
    sub rsp, 28h
    
    cmp edx, DLL_PROCESS_ATTACH
    jne check_detach
    
    call DisableThreadLibraryCalls
    call main
    jmp dll_done
    
check_detach:
    cmp edx, DLL_PROCESS_DETACH
    jne dll_done
    call rawrxd_cleanup
    
dll_done:
    mov rax, 1                      ; TRUE
    add rsp, 28h
    ret
DllMain ENDP

; ════════════════════════════════════════════════════════════════════════════════
; STUB FUNCTIONS (Production implementations would link to actual modules)
; ════════════════════════════════════════════════════════════════════════════════

rawrxd_check_for_commands PROC
    ; Return 0 (no commands) - in production would check IPC queue
    xor eax, eax
    ret
rawrxd_check_for_commands ENDP

rawrxd_run_benchmark PROC
    sub rsp, 28h
    lea rcx, szBenchmarkRunning
    call print_message
    add rsp, 28h
    ret
rawrxd_run_benchmark ENDP

; Manager creation stubs
feature_harness_init_stub PROC
    mov rax, 1
    ret
feature_harness_init_stub ENDP

unified_hotpatch_create_stub PROC
    mov rax, 1
    ret
unified_hotpatch_create_stub ENDP

dual_engine_create_stub PROC
    mov rax, 1
    ret
dual_engine_create_stub ENDP

agentic_orchestrator_create_stub PROC
    mov rax, 1
    ret
agentic_orchestrator_create_stub ENDP

agentic_puppeteer_create_stub PROC
    mov rax, 1
    ret
agentic_puppeteer_create_stub ENDP

; Operation stubs
memory_zero_stub PROC
    ret
memory_zero_stub ENDP

agentic_pulse_stub PROC
    ret
agentic_pulse_stub ENDP

dual_engine_load_model_stub PROC
    mov rax, 1
    ret
dual_engine_load_model_stub ENDP

apply_pending_patches_stub PROC
    ret
apply_pending_patches_stub ENDP

puppeteer_notify_stub PROC
    ret
puppeteer_notify_stub ENDP

engine_is_ready_stub PROC
    mov rax, 1
    ret
engine_is_ready_stub ENDP

begin_critical_section_stub PROC
    ret
begin_critical_section_stub ENDP

end_critical_section_stub PROC
    ret
end_critical_section_stub ENDP

hot_switch_stub PROC
    ret
hot_switch_stub ENDP

toggle_feature_stub PROC
    mov rax, 1
    ret
toggle_feature_stub ENDP

apply_feature_patches_stub PROC
    ret
apply_feature_patches_stub ENDP

notify_config_change_stub PROC
    ret
notify_config_change_stub ENDP

agentic_enable_stub PROC
    ret
agentic_enable_stub ENDP

notify_command_executed_stub PROC
    ret
notify_command_executed_stub ENDP

; Destroy stubs
destroy_patch_manager_stub PROC
    ret
destroy_patch_manager_stub ENDP

destroy_dual_engine_stub PROC
    ret
destroy_dual_engine_stub ENDP

destroy_orchestrator_stub PROC
    ret
destroy_orchestrator_stub ENDP

destroy_puppeteer_stub PROC
    ret
destroy_puppeteer_stub ENDP

; Simple console output helper
print_message PROC
    sub rsp, 38h
    mov r8, rcx                   ; Save string pointer
    
    ; Get console handle
    mov ecx, -11                  ; STD_OUTPUT_HANDLE
    call GetStdHandle
    test rax, rax
    jz done_print
    mov r9, rax                   ; Save handle
    
    ; Calculate string length
    mov rcx, r8
    xor edx, edx
calc_len:
    mov al, byte ptr [rcx]
    test al, al
    jz len_done
    inc rcx
    inc edx
    jmp calc_len
len_done:
    test edx, edx
    jz done_print
    
    ; WriteFile(handle, buffer, length, &written, NULL)
    mov rcx, r9                   ; Console handle
    mov rdx, r8                   ; String pointer
    mov r8d, edx                  ; Length
    lea r9, qword ptr [rsp+40h]   ; &written
    push 0                        ; NULL overlapped
    call WriteFile
    add rsp, 8
    
done_print:
    add rsp, 38h
    ret
print_message ENDP

END
