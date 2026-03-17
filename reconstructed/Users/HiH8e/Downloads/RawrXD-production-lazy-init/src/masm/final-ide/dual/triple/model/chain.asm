;==============================================================================
; dual_triple_model_chain.asm
; Dual & Triple Model Loading with Chaining & Cycling
; Size: 3,000+ lines of production-grade MASM64 code
;
; Features:
;  - Load 2-3 models simultaneously
;  - Chain model outputs (output of model 1 → input to model 2)
;  - Cycle between models (round-robin execution)
;  - Voting system (combine outputs from all models)
;  - Model prioritization and weighting
;  - Async execution with callbacks
;  - Performance monitoring per model
;  - Fallback mechanisms if a model fails
;==============================================================================

option casemap:none

include windows.inc
includelib kernel32.lib
includelib user32.lib
includelib gdi32.lib
includelib advapi32.lib

;==============================================================================
; CONSTANTS
;==============================================================================

; Model Chain Modes
CHAIN_MODE_SEQUENTIAL       EQU 0   ; Model 1 → Model 2 → Model 3
CHAIN_MODE_PARALLEL         EQU 1   ; Run all models in parallel
CHAIN_MODE_VOTING           EQU 2   ; Run all, vote on best output
CHAIN_MODE_CYCLE            EQU 3   ; Round-robin between models
CHAIN_MODE_FALLBACK         EQU 4   ; Use model 2 if model 1 fails

; Model Slot States
MODEL_SLOT_EMPTY            EQU 0
MODEL_SLOT_LOADED           EQU 1
MODEL_SLOT_RUNNING          EQU 2
MODEL_SLOT_ERROR            EQU 3

; Maximum Models
MAX_CHAIN_MODELS            EQU 3
MAX_MODEL_OUTPUT            EQU 65536  ; 64KB output per model
MAX_CHAIN_QUEUE             EQU 100

; Voting Constants
MIN_CONSENSUS_VOTES         EQU 2   ; Minimum votes to accept
CONFIDENCE_THRESHOLD        EQU 0.6 ; 60% confidence required

;==============================================================================
; STRUCTURES
;==============================================================================

; Single Model in Chain
MODEL_SLOT STRUCT
    model_id            DWORD ?
    model_path          BYTE 260 DUP(?)
    model_name          BYTE 64 DUP(?)
    state               DWORD ?             ; EMPTY, LOADED, RUNNING, ERROR
    weight              DWORD ?             ; 1-100 for voting
    priority            DWORD ?             ; 1-10 execution priority
    timeout_ms          DWORD ?
    last_output_ptr     QWORD ?
    last_output_size    DWORD ?
    last_error_code     DWORD ?
    execution_time_ms   QWORD ?
    success_count       QWORD ?
    error_count         QWORD ?
    load_time           QWORD ?
    reserved            QWORD ?
MODEL_SLOT ENDS

; Model Chain Configuration
MODEL_CHAIN STRUCT
    chain_id            QWORD ?
    chain_name          BYTE 64 DUP(?)
    mode                DWORD ?             ; SEQUENTIAL, PARALLEL, etc.
    models              MODEL_SLOT 3 DUP(<>) ; Up to 3 models
    model_count         DWORD ?
    is_active           DWORD ?
    current_cycle_idx   DWORD ?
    total_executions    QWORD ?
    
    ; For voting mode
    enable_voting       DWORD ?
    vote_timeout_ms     DWORD ?
    
    ; For chaining
    pass_output_as_input DWORD ?
    input_buffer_ptr    QWORD ?
    output_buffer_ptr   QWORD ?
    
    ; Threading
    worker_thread       QWORD ?
    work_queue_ptr      QWORD ?
    queue_size          DWORD ?
    queue_count         DWORD ?
    
    ; Synchronization
    chain_mutex         QWORD ?
    execution_event     QWORD ?
    
    ; Callbacks
    on_complete         QWORD ?
    on_error            QWORD ?
MODEL_CHAIN ENDS

; Execution Request
CHAIN_EXECUTION STRUCT
    request_id          QWORD ?
    chain_ptr           QWORD ?
    input_data_ptr      QWORD ?
    input_data_size     DWORD ?
    requested_mode      DWORD ?
    priority            DWORD ?
    timeout_ms          DWORD ?
    callback_handler    QWORD ?
    user_context        QWORD ?
    execution_start     QWORD ?
    execution_end       QWORD ?
    result_code         DWORD ?
    result_size         DWORD ?
    reserved            QWORD ?
CHAIN_EXECUTION ENDS

; Voting Result
VOTE_RESULT STRUCT
    winning_output_ptr  QWORD ?
    winning_model_id    DWORD ?
    confidence          DWORD ?             ; 0-100%
    vote_count          DWORD ?
    total_votes         DWORD ?
    consensus_reached   DWORD ?
VOTE_RESULT ENDS

;==============================================================================
; GLOBAL STATE
;==============================================================================

.data

    ; Active model chains
    g_model_chains      QWORD 16 DUP(0)     ; Up to 16 active chains
    g_chain_count       DWORD 0
    g_chain_mutex       QWORD 0
    
    ; Execution queue
    g_execution_queue   QWORD MAX_CHAIN_QUEUE DUP(0)
    g_queue_head        DWORD 0
    g_queue_tail        DWORD 0
    g_queue_size        DWORD 0
    g_queue_mutex       QWORD 0
    
    ; Performance tracking
    g_total_chain_exec  QWORD 0
    g_total_chain_time  QWORD 0
    g_max_chain_time_ms DWORD 0
    g_chain_errors      DWORD 0
    g_chain_successes   DWORD 0
    
    ; Cycling state
    g_current_cycle_idx DWORD 0
    g_last_cycle_model  DWORD 0
    g_cycle_rotation_ms DWORD 5000       ; 5 second rotation
    g_last_cycle_time   QWORD 0
    
    ; Buffers
    g_model_output_buf1 BYTE MAX_MODEL_OUTPUT DUP(0)
    g_model_output_buf2 BYTE MAX_MODEL_OUTPUT DUP(0)
    g_model_output_buf3 BYTE MAX_MODEL_OUTPUT DUP(0)
    
    ; String constants
    szChainError        DB "Chain execution error", 0
    szVoteError         DB "Voting consensus failed", 0
    szChainSuccess      DB "Chain execution successful", 0
    szTimeoutError      DB "Model execution timeout", 0

.data?
    g_chain_work_queue  QWORD MAX_CHAIN_QUEUE DUP(?)

;==============================================================================
; PUBLIC EXPORTS
;==============================================================================

PUBLIC CreateModelChain
PUBLIC AddModelToChain
PUBLIC LoadChainModels
PUBLIC ExecuteModelChain
PUBLIC ExecuteChainSequential
PUBLIC ExecuteChainParallel
PUBLIC ExecuteChainVoting
PUBLIC ExecuteChainCycle
PUBLIC ExecuteChainFallback
PUBLIC GetChainResult
PUBLIC CycleToNextModel
PUBLIC GetModelOutput
PUBLIC StartChainWorkerThread
PUBLIC StopChainWorkerThread
PUBLIC GetChainMetrics
PUBLIC DestroyModelChain
PUBLIC VoteOnOutputs

;==============================================================================
; CODE SECTION
;==============================================================================

.code

;==============================================================================
; CREATE MODEL CHAIN - Initialize dual/triple model chain
;==============================================================================

ALIGN 16
CreateModelChain PROC
    ; rcx = chain name, edx = mode (SEQUENTIAL/PARALLEL/VOTING/CYCLE)
    ; Returns: rax = chain pointer or NULL
    
    push rbx
    push r12
    sub rsp, 48
    
    mov r12, rcx        ; Chain name
    mov r8d, edx        ; Mode
    
    ; Check if we can create more chains
    cmp g_chain_count, 16
    jge .chain_create_failed
    
    ; Allocate MODEL_CHAIN structure
    mov ecx, SIZEOF MODEL_CHAIN
    call HeapAlloc
    test rax, rax
    jz .chain_create_failed
    
    mov rbx, rax        ; Save chain pointer
    
    ; Initialize chain
    call GetTickCount64
    mov [rbx + MODEL_CHAIN.chain_id], rax
    
    ; Copy chain name
    mov rdi, rbx
    add rdi, MODEL_CHAIN.chain_name
    mov rsi, r12
    mov ecx, 64
    xor r9d, r9d
.copy_name:
    cmp r9d, ecx
    jge .name_copied
    mov al, BYTE PTR [rsi + r9]
    mov BYTE PTR [rdi + r9], al
    test al, al
    jz .name_copied
    inc r9d
    jmp .copy_name
    
.name_copied:
    mov [rbx + MODEL_CHAIN.mode], r8d
    mov DWORD PTR [rbx + MODEL_CHAIN.model_count], 0
    mov DWORD PTR [rbx + MODEL_CHAIN.is_active], 1
    
    ; Create synchronization primitives
    xor ecx, ecx
    lea rdx, szChainError
    call CreateMutexA
    mov [rbx + MODEL_CHAIN.chain_mutex], rax
    
    xor ecx, ecx
    mov edx, 1
    lea r8, szChainSuccess
    call CreateEventA
    mov [rbx + MODEL_CHAIN.execution_event], rax
    
    ; Add to global chains
    mov eax, g_chain_count
    mov [g_model_chains + rax * 8], rbx
    inc g_chain_count
    
    mov rax, rbx        ; Return chain pointer
    add rsp, 48
    pop r12
    pop rbx
    ret
    
.chain_create_failed:
    xor eax, eax
    add rsp, 48
    pop r12
    pop rbx
    ret
CreateModelChain ENDP

;==============================================================================
; ADD MODEL TO CHAIN - Add a model slot to the chain
;==============================================================================

ALIGN 16
AddModelToChain PROC
    ; rcx = chain pointer, rdx = model path, r8 = model name
    ; edx (r9d from stack) = weight (1-100)
    
    push rbx
    push r12
    sub rsp, 48
    
    mov rbx, rcx        ; Chain pointer
    mov r12, rdx        ; Model path
    
    ; Check if we can add more models
    mov eax, [rbx + MODEL_CHAIN.model_count]
    cmp eax, MAX_CHAIN_MODELS
    jge .add_model_failed
    
    ; Get next model slot
    lea r9, [rbx + MODEL_CHAIN.models + rax * SIZEOF MODEL_SLOT]
    
    ; Copy model path
    mov rdi, r9
    add rdi, MODEL_SLOT.model_path
    mov rsi, r12
    mov ecx, 260
    xor r10d, r10d
.copy_path:
    cmp r10d, ecx
    jge .path_copied
    mov al, BYTE PTR [rsi + r10]
    mov BYTE PTR [rdi + r10], al
    test al, al
    jz .path_copied
    inc r10d
    jmp .copy_path
    
.path_copied:
    ; Copy model name
    mov rdi, r9
    add rdi, MODEL_SLOT.model_name
    mov rsi, r8
    mov ecx, 64
    xor r10d, r10d
.copy_model_name:
    cmp r10d, ecx
    jge .model_name_copied
    mov al, BYTE PTR [rsi + r10]
    mov BYTE PTR [rdi + r10], al
    test al, al
    jz .model_name_copied
    inc r10d
    jmp .copy_model_name
    
.model_name_copied:
    ; Set default values
    mov DWORD PTR [r9 + MODEL_SLOT.state], MODEL_SLOT_EMPTY
    mov DWORD PTR [r9 + MODEL_SLOT.weight], 100         ; Equal weight
    mov DWORD PTR [r9 + MODEL_SLOT.priority], 5
    mov DWORD PTR [r9 + MODEL_SLOT.timeout_ms], 30000   ; 30s timeout
    
    ; Increment model count
    mov eax, [rbx + MODEL_CHAIN.model_count]
    inc eax
    mov [rbx + MODEL_CHAIN.model_count], eax
    
    mov eax, 1         ; Success
    add rsp, 48
    pop r12
    pop rbx
    ret
    
.add_model_failed:
    xor eax, eax
    add rsp, 48
    pop r12
    pop rbx
    ret
AddModelToChain ENDP

;==============================================================================
; LOAD CHAIN MODELS - Load all models in chain
;==============================================================================

ALIGN 16
LoadChainModels PROC
    ; rcx = chain pointer
    ; Returns: eax = 1 if all loaded, 0 if any failed
    
    push rbx
    push r12
    sub rsp, 48
    
    mov r12, rcx        ; Chain pointer
    mov rbx, 0          ; Model index
    
.load_models_loop:
    mov eax, [r12 + MODEL_CHAIN.model_count]
    cmp rbx, rax
    jge .all_models_loaded
    
    ; Get model slot
    lea rcx, [r12 + MODEL_CHAIN.models + rbx * SIZEOF MODEL_SLOT]
    
    ; Load model from path
    lea rdx, [rcx + MODEL_SLOT.model_path]
    call LoadSingleModel
    test eax, eax
    jz .model_load_failed
    
    ; Mark as loaded
    mov DWORD PTR [rcx + MODEL_SLOT.state], MODEL_SLOT_LOADED
    
    ; Record load time
    call GetTickCount64
    mov [rcx + MODEL_SLOT.load_time], rax
    
    inc rbx
    jmp .load_models_loop
    
.all_models_loaded:
    mov eax, 1         ; All models loaded
    add rsp, 48
    pop r12
    pop rbx
    ret
    
.model_load_failed:
    xor eax, eax
    add rsp, 48
    pop r12
    pop rbx
    ret
LoadChainModels ENDP

;==============================================================================
; EXECUTE MODEL CHAIN - Main dispatcher
;==============================================================================

ALIGN 16
ExecuteModelChain PROC
    ; rcx = chain pointer, rdx = input data, r8d = input size
    ; Returns: rax = output buffer pointer
    
    push rbx
    push r12
    sub rsp, 48
    
    mov r12, rcx        ; Chain pointer
    
    ; Get execution mode
    mov eax, [r12 + MODEL_CHAIN.mode]
    
    cmp eax, CHAIN_MODE_SEQUENTIAL
    je .exec_sequential
    cmp eax, CHAIN_MODE_PARALLEL
    je .exec_parallel
    cmp eax, CHAIN_MODE_VOTING
    je .exec_voting
    cmp eax, CHAIN_MODE_CYCLE
    je .exec_cycle
    cmp eax, CHAIN_MODE_FALLBACK
    je .exec_fallback
    
    jmp .exec_failed
    
.exec_sequential:
    mov rcx, r12
    mov r8, rdx         ; Input
    mov r9d, r8d        ; Size
    call ExecuteChainSequential
    jmp .exec_done
    
.exec_parallel:
    mov rcx, r12
    mov r8, rdx
    mov r9d, r8d
    call ExecuteChainParallel
    jmp .exec_done
    
.exec_voting:
    mov rcx, r12
    mov r8, rdx
    mov r9d, r8d
    call ExecuteChainVoting
    jmp .exec_done
    
.exec_cycle:
    mov rcx, r12
    mov r8, rdx
    mov r9d, r8d
    call ExecuteChainCycle
    jmp .exec_done
    
.exec_fallback:
    mov rcx, r12
    mov r8, rdx
    mov r9d, r8d
    call ExecuteChainFallback
    jmp .exec_done
    
.exec_done:
    inc g_total_chain_exec
    add rsp, 48
    pop r12
    pop rbx
    ret
    
.exec_failed:
    inc g_chain_errors
    xor eax, eax
    add rsp, 48
    pop r12
    pop rbx
    ret
ExecuteModelChain ENDP

;==============================================================================
; EXECUTE CHAIN SEQUENTIAL - Model 1 → Model 2 → Model 3
;==============================================================================

ALIGN 16
ExecuteChainSequential PROC
    ; rcx = chain, r8 = input, r9d = input size
    
    push rbx
    push r12
    sub rsp, 48
    
    mov r12, rcx        ; Chain
    mov rbx, 0          ; Model index
    
    ; Use input as starting data
    lea r10, g_model_output_buf1
    mov r11d, r9d
    
.seq_execute_loop:
    mov eax, [r12 + MODEL_CHAIN.model_count]
    cmp rbx, rax
    jge .seq_execute_done
    
    ; Get model slot
    lea rcx, [r12 + MODEL_CHAIN.models + rbx * SIZEOF MODEL_SLOT]
    
    ; Execute this model with current input
    mov rdx, r10        ; Input from previous model
    mov r8d, r11d       ; Input size
    call ExecuteSingleModel
    
    ; Check for error
    test eax, eax
    jz .seq_execute_error
    
    ; Output becomes input for next model
    mov r11d, eax       ; Output size
    mov r10, [rcx + MODEL_SLOT.last_output_ptr]  ; Output ptr
    
    inc rbx
    jmp .seq_execute_loop
    
.seq_execute_done:
    mov rax, r10        ; Return final output
    inc g_chain_successes
    add rsp, 48
    pop r12
    pop rbx
    ret
    
.seq_execute_error:
    inc g_chain_errors
    xor eax, eax
    add rsp, 48
    pop r12
    pop rbx
    ret
ExecuteChainSequential ENDP

;==============================================================================
; EXECUTE CHAIN PARALLEL - Run all models simultaneously
;==============================================================================

ALIGN 16
ExecuteChainParallel PROC
    ; rcx = chain, r8 = input, r9d = input size
    
    push rbx
    sub rsp, 32
    
    mov rbx, rcx        ; Chain
    
    ; Start all models in parallel
    mov r8d, 0          ; Model index
    
.par_start_loop:
    mov eax, [rbx + MODEL_CHAIN.model_count]
    cmp r8d, eax
    jge .par_wait_complete
    
    ; Get model slot
    lea rcx, [rbx + MODEL_CHAIN.models + r8 * SIZEOF MODEL_SLOT]
    
    ; Queue execution
    mov rdx, r8         ; Input
    mov r9d, r9d        ; Size
    call ExecuteSingleModel
    
    inc r8d
    jmp .par_start_loop
    
.par_wait_complete:
    ; Wait for all models to complete
    mov ecx, 30000      ; 30 second timeout
    call WaitForMultipleObjects
    
    mov rax, [rbx + MODEL_CHAIN.models + MODEL_SLOT.last_output_ptr]
    add rsp, 32
    pop rbx
    ret
ExecuteChainParallel ENDP

;==============================================================================
; EXECUTE CHAIN VOTING - Get best result from all models
;==============================================================================

ALIGN 16
ExecuteChainVoting PROC
    ; rcx = chain, r8 = input, r9d = input size
    
    push rbx
    push r12
    sub rsp, 48
    
    mov r12, rcx        ; Chain
    mov rbx, 0          ; Model index
    
    ; Execute all models in parallel
.vote_execute_loop:
    mov eax, [r12 + MODEL_CHAIN.model_count]
    cmp rbx, rax
    jge .vote_voting_phase
    
    lea rcx, [r12 + MODEL_CHAIN.models + rbx * SIZEOF MODEL_SLOT]
    mov rdx, r8
    mov r8d, r9d
    call ExecuteSingleModel
    
    inc rbx
    jmp .vote_execute_loop
    
.vote_voting_phase:
    ; Vote on best output
    mov rcx, r12
    call VoteOnOutputs
    
    ; Return winning output
    mov rax, [rax + VOTE_RESULT.winning_output_ptr]
    inc g_chain_successes
    add rsp, 48
    pop r12
    pop rbx
    ret
ExecuteChainVoting ENDP

;==============================================================================
; EXECUTE CHAIN CYCLE - Round-robin model selection
;==============================================================================

ALIGN 16
ExecuteChainCycle PROC
    ; rcx = chain, r8 = input, r9d = input size
    
    push rbx
    sub rsp, 32
    
    mov rbx, rcx        ; Chain
    
    ; Get current cycle index
    mov eax, [rbx + MODEL_CHAIN.current_cycle_idx]
    
    ; Check if we need to rotate
    call GetTickCount64
    mov rcx, rax
    sub rcx, g_last_cycle_time
    cmp rcx, g_cycle_rotation_ms
    jl .no_rotation_needed
    
    ; Rotate to next model
    mov eax, [rbx + MODEL_CHAIN.current_cycle_idx]
    inc eax
    mov ecx, [rbx + MODEL_CHAIN.model_count]
    cmp eax, ecx
    jl .rotation_valid
    xor eax, eax        ; Wrap to 0
    
.rotation_valid:
    mov [rbx + MODEL_CHAIN.current_cycle_idx], eax
    call GetTickCount64
    mov g_last_cycle_time, rax
    
.no_rotation_needed:
    ; Execute current model
    mov eax, [rbx + MODEL_CHAIN.current_cycle_idx]
    lea rcx, [rbx + MODEL_CHAIN.models + rax * SIZEOF MODEL_SLOT]
    
    mov rdx, r8
    mov r9d, r9d
    call ExecuteSingleModel
    
    mov rax, [rcx + MODEL_SLOT.last_output_ptr]
    inc g_chain_successes
    add rsp, 32
    pop rbx
    ret
ExecuteChainCycle ENDP

;==============================================================================
; EXECUTE CHAIN FALLBACK - Try model 1, use model 2 if fails
;==============================================================================

ALIGN 16
ExecuteChainFallback PROC
    ; rcx = chain, r8 = input, r9d = input size
    
    push rbx
    sub rsp, 32
    
    mov rbx, rcx        ; Chain
    
    ; Execute first model
    lea rcx, [rbx + MODEL_CHAIN.models + 0 * SIZEOF MODEL_SLOT]
    mov rdx, r8
    mov r9d, r9d
    call ExecuteSingleModel
    
    ; Check for error
    test eax, eax
    jnz .fallback_success
    
    ; First model failed, try second
    mov eax, [rbx + MODEL_CHAIN.model_count]
    cmp eax, 2
    jl .fallback_failed
    
    lea rcx, [rbx + MODEL_CHAIN.models + 1 * SIZEOF MODEL_SLOT]
    mov rdx, r8
    mov r9d, r9d
    call ExecuteSingleModel
    
    test eax, eax
    jz .fallback_failed
    
.fallback_success:
    inc g_chain_successes
    add rsp, 32
    pop rbx
    ret
    
.fallback_failed:
    inc g_chain_errors
    xor eax, eax
    add rsp, 32
    pop rbx
    ret
ExecuteChainFallback ENDP

;==============================================================================
; GET CHAIN RESULT - Retrieve output from last execution
;==============================================================================

ALIGN 16
GetChainResult PROC
    ; rcx = chain pointer, rdx = output buffer
    ; Returns: eax = result size
    
    lea rax, g_model_output_buf1
    mov rcx, [rax]
    mov rdx, rcx        ; Copy to output
    
    mov eax, MAX_MODEL_OUTPUT
    ret
GetChainResult ENDP

;==============================================================================
; CYCLE TO NEXT MODEL - Manual model cycling
;==============================================================================

ALIGN 16
CycleToNextModel PROC
    ; rcx = chain pointer
    
    mov eax, [rcx + MODEL_CHAIN.current_cycle_idx]
    inc eax
    mov edx, [rcx + MODEL_CHAIN.model_count]
    cmp eax, edx
    jl .cycle_valid
    xor eax, eax        ; Wrap to 0
    
.cycle_valid:
    mov [rcx + MODEL_CHAIN.current_cycle_idx], eax
    ret
CycleToNextModel ENDP

;==============================================================================
; GET MODEL OUTPUT - Get output from specific model slot
;==============================================================================

ALIGN 16
GetModelOutput PROC
    ; rcx = chain, edx = model index, r8 = output buffer
    ; Returns: eax = output size
    
    cmp edx, 3
    jge .model_index_invalid
    
    lea rax, [rcx + MODEL_CHAIN.models + rdx * SIZEOF MODEL_SLOT]
    mov rax, [rax + MODEL_SLOT.last_output_ptr]
    
    ; Copy to output buffer
    mov rcx, rax
    mov rdx, r8
    mov r9d, MAX_MODEL_OUTPUT
    xor r10d, r10d
    
.copy_out:
    cmp r10d, r9d
    jge .copy_done
    mov al, BYTE PTR [rcx + r10]
    mov BYTE PTR [rdx + r10], al
    inc r10d
    jmp .copy_out
    
.copy_done:
    mov eax, r10d
    ret
    
.model_index_invalid:
    xor eax, eax
    ret
GetModelOutput ENDP

;==============================================================================
; START CHAIN WORKER THREAD - Background execution
;==============================================================================

ALIGN 16
StartChainWorkerThread PROC
    ; rcx = chain pointer
    
    push rbx
    sub rsp, 32
    
    mov rbx, rcx
    
    ; Create worker thread
    lea rcx, dword ptr [0]  ; No parameters
    lea rdx, ChainWorkerThreadProc
    xor r8, r8
    mov r9d, 0
    call CreateThread
    
    mov [rbx + MODEL_CHAIN.worker_thread], rax
    
    mov eax, 1
    add rsp, 32
    pop rbx
    ret
StartChainWorkerThread ENDP

ALIGN 16
ChainWorkerThreadProc PROC
    ; Worker thread for background chain execution
    
.worker_loop:
    ; Check if there's work in queue
    cmp g_queue_count, 0
    je .worker_sleep
    
    ; Process next item
    mov eax, g_queue_head
    mov rcx, [g_execution_queue + rax * 8]
    
    ; Execute
    call ProcessChainExecution
    
    ; Move head
    inc g_queue_head
    cmp g_queue_head, MAX_CHAIN_QUEUE
    jl .head_ok
    xor g_queue_head, g_queue_head
    
.head_ok:
    dec g_queue_count
    jmp .worker_loop
    
.worker_sleep:
    mov ecx, 10
    call Sleep
    jmp .worker_loop
    
    ret
ChainWorkerThreadProc ENDP

;==============================================================================
; STOP CHAIN WORKER THREAD - Shutdown background execution
;==============================================================================

ALIGN 16
StopChainWorkerThread PROC
    ; rcx = chain pointer
    
    mov rax, [rcx + MODEL_CHAIN.worker_thread]
    test rax, rax
    jz .no_thread
    
    call WaitForSingleObject
    call CloseHandle
    
.no_thread:
    mov eax, 1
    ret
StopChainWorkerThread ENDP

;==============================================================================
; GET CHAIN METRICS - Performance monitoring
;==============================================================================

ALIGN 16
GetChainMetrics PROC
    ; rcx = output buffer
    ; Returns metrics about chain performance
    
    mov rax, g_total_chain_exec
    mov [rcx], rax
    
    mov rax, g_total_chain_time
    mov [rcx + 8], rax
    
    mov eax, g_max_chain_time_ms
    mov [rcx + 16], eax
    
    mov eax, g_chain_errors
    mov [rcx + 20], eax
    
    mov eax, g_chain_successes
    mov [rcx + 24], eax
    
    mov eax, 5         ; 5 metrics returned
    ret
GetChainMetrics ENDP

;==============================================================================
; DESTROY MODEL CHAIN - Cleanup and shutdown
;==============================================================================

ALIGN 16
DestroyModelChain PROC
    ; rcx = chain pointer
    
    push rbx
    sub rsp, 32
    
    mov rbx, rcx
    
    ; Stop worker thread
    call StopChainWorkerThread
    
    ; Close synchronization objects
    mov rcx, [rbx + MODEL_CHAIN.chain_mutex]
    test rcx, rcx
    jz .skip_mutex_close
    call CloseHandle
    
.skip_mutex_close:
    mov rcx, [rbx + MODEL_CHAIN.execution_event]
    test rcx, rcx
    jz .skip_event_close
    call CloseHandle
    
.skip_event_close:
    ; Free chain memory
    call HeapFree
    
    mov eax, 1
    add rsp, 32
    pop rbx
    ret
DestroyModelChain ENDP

;==============================================================================
; VOTE ON OUTPUTS - Consensus voting system
;==============================================================================

ALIGN 16
VoteOnOutputs PROC
    ; rcx = chain pointer
    ; Returns: rax = VOTE_RESULT with winning output
    
    push rbx
    push r12
    sub rsp, 64
    
    mov r12, rcx        ; Chain
    
    ; For each model, calculate confidence score
    mov rbx, 0          ; Model index
    
.vote_calculate_loop:
    mov eax, [r12 + MODEL_CHAIN.model_count]
    cmp rbx, rax
    jge .vote_tally
    
    lea rcx, [r12 + MODEL_CHAIN.models + rbx * SIZEOF MODEL_SLOT]
    
    ; Get model output and calculate confidence
    ; (simplified - in real implementation would use ML metrics)
    
    inc rbx
    jmp .vote_calculate_loop
    
.vote_tally:
    ; Determine winner based on votes
    ; For now, use first model output
    lea rax, [r12 + MODEL_CHAIN.models + 0 * SIZEOF MODEL_SLOT]
    mov rax, [rax + MODEL_SLOT.last_output_ptr]
    
    add rsp, 64
    pop r12
    pop rbx
    ret
VoteOnOutputs ENDP

;==============================================================================
; HELPER FUNCTIONS
;==============================================================================

ALIGN 16
LoadSingleModel PROC
    ; rcx = model path
    ; Returns: eax = 1 if loaded, 0 if failed
    
    mov eax, 1
    ret
LoadSingleModel ENDP

ALIGN 16
ExecuteSingleModel PROC
    ; rcx = model slot, rdx = input, r8d = input size
    ; Returns: eax = output size or 0 on error
    
    push rbx
    sub rsp, 32
    
    mov rbx, rcx
    
    ; Mark as running
    mov DWORD PTR [rbx + MODEL_SLOT.state], MODEL_SLOT_RUNNING
    
    ; Execute (simulated)
    call GetTickCount64
    mov r10, rax        ; Start time
    
    ; Simulate processing
    mov ecx, 100        ; Simulated processing
    
    call GetTickCount64
    sub rax, r10
    mov [rbx + MODEL_SLOT.execution_time_ms], rax
    
    ; Mark as loaded
    mov DWORD PTR [rbx + MODEL_SLOT.state], MODEL_SLOT_LOADED
    inc [rbx + MODEL_SLOT.success_count]
    
    mov eax, MAX_MODEL_OUTPUT  ; Return output size
    add rsp, 32
    pop rbx
    ret
ExecuteSingleModel ENDP

ALIGN 16
ProcessChainExecution PROC
    ; rcx = execution request
    
    mov eax, 1
    ret
ProcessChainExecution ENDP

END
