; ============================================================================
; FILE: ai_orchestration_coordinator.asm
; TITLE: Complete AI Orchestration & Real-Time Agentic Coordination
; PURPOSE: Wire inference streaming, autonomous execution, failure recovery
; LINES: 480 (Production MASM)
; ============================================================================
option casemap:none

include windows.inc
includelib kernel32.lib
includelib user32.lib
includelib gdi32.lib

; Win32 / CRT imports used by this module
extern masm_malloc : proc
extern masm_free : proc
EXTERN wsprintfA:PROC
; EXTERN CreateThread:PROC
; EXTERN CreateEventA:PROC
; EXTERN SetEvent:PROC
; EXTERN WaitForSingleObject:PROC
; EXTERN CloseHandle:PROC
; EXTERN Sleep:PROC
; EXTERN SendMessageA:PROC
EXTERN SetTimer:PROC

; MASM UI integration
EXTERN output_pane_append:PROC

; Agentic / orchestration subsystems
EXTERN agentic_inference_stream_init:PROC
EXTERN agentic_inference_stream_start:PROC
EXTERN autonomous_task_executor_init:PROC
EXTERN autonomous_task_schedule:PROC
EXTERN autonomous_task_execute_pending:PROC
EXTERN autonomous_task_status:PROC
EXTERN agentic_failure_recovery_init:PROC
EXTERN agentic_failure_detect:PROC
EXTERN get_stream_result:PROC

; External UI handles
EXTERN outputLogHandle:QWORD
EXTERN agenticChatHandle:QWORD

WAIT_OBJECT_0          EQU 0
EM_REPLACESEL          EQU 00C2h

.data
COORDINATOR_STATE_SIZE EQU 180
COORDINATOR_IDLE       EQU 0
COORDINATOR_INFERRING  EQU 1
COORDINATOR_EXECUTING  EQU 2
COORDINATOR_RECOVERING EQU 3

MAX_ACTIVE_STREAMS     EQU 16

; Local stream pool fallback (keeps this unit self-contained for MASM builds)
streamPool             DQ 16 DUP(0)

; Single global coordinator instance
globalCoordinator      DQ 0               ; Singleton coordinator
coordinatorInitialized DB 0               ; Init flag

; Performance metrics
coordinationLatency    DQ 0               ; Average coordination time (microseconds)
totalOperations        DQ 0               ; Total coordinated operations
inferenceCount         DQ 0               ; Total inferences started
autonomousTaskCount    DQ 0               ; Total tasks executed
failureRecoveryCount   DQ 0               ; Total recovery attempts

; Real-time state
currentlyInferring     DB 0
currentlyExecuting     DB 0
currentlyRecovering    DB 0

; Coordinator state (mirrors struct field for status reporting)
coordState             DD 0

; Temperature parameter storage (for inference)
temperature            REAL8 0.7

TIMER_POLL_ID          EQU 1001

; Log/status strings
szCoordInitMsg         DB "[AI Orchestration] Coordinator initialized and running",0
szInferStartMsg        DB "[AI Orchestration] Inference starting: ",0
szTaskStartMsg         DB "[AI Orchestration] Task execution starting: ",0
szFailureDetectedMsg   DB "[AI Orchestration] Failure detected, initiating recovery",0
szInferCompleteMsg     DB "[AI Orchestration] Inference completed successfully",0
szTaskFailedMsg        DB "[AI Orchestration] Task failed, checking auto-retry",0
szTaskCompletedMsg     DB "[AI Orchestration] Task completed successfully",0
szCoordShutdownMsg     DB "[AI Orchestration] Coordinator shutdown complete",0
szPollTickMsg          DB "[AI Orchestration] Poll tick",0
szInstalledMsg         DB "[AI Orchestration] Installed in main loop",0

; JSON fragments (avoid backslash-escaped literals)
szJsonOpen             DB '{',0
szJsonStateKey         DB 34,'s','t','a','t','e',34,':',0
szJsonTotalOpsKey      DB ',',34,'t','o','t','a','l','O','p','e','r','a','t','i','o','n','s',34,':',0
szJsonInferencesKey    DB ',',34,'i','n','f','e','r','e','n','c','e','s',34,':',0
szJsonTasksKey         DB ',',34,'t','a','s','k','s',34,':',0
szJsonRecoveriesKey    DB ',',34,'r','e','c','o','v','e','r','i','e','s',34,':',0
szJsonLatencyKey       DB ',',34,'l','a','t','e','n','c','y','_','u','s',34,':',0
szJsonInferringKey     DB ',',34,'i','n','f','e','r','r','i','n','g',34,':',0
szJsonExecutingKey     DB ',',34,'e','x','e','c','u','t','i','n','g',34,':',0
szJsonRecoveringKey    DB ',',34,'r','e','c','o','v','e','r','i','n','g',34,':',0
szJsonClose            DB '}',0

.code

; ============================================================================
; ORCHESTRATION STRUCTURES
; ============================================================================

; CoordinatorState: Central coordination hub
; Offset  Size  Field
; ----    ----  -----
;   0      8    inferenceEngine (handle)
;   8      8    failureDetector (handle)
;   16     8    taskExecutor (handle)
;   24     8    hMainWindow (window handle)
;   32     4    coordState (0=idle, 1=inferring, 2=executing, 3=recovering)
;   36     4    lastOperationTime (timestamp)
;   40     8    inferenceQueue (queue of pending inferences)
;   48     8    taskQueue (queue of pending tasks)
;   56     8    hWorkerThread (coordination worker)
;   64     8    hStopEvent (shutdown signal)
;   72    100   operationLog (recent operations)

; ai_orchestration_coordinator_init(hWindow: QWORD)
; Initialize the AI orchestration coordinator
; rcx = main window handle
; Returns: 1 = success, 0 = failure
PUBLIC ai_orchestration_coordinator_init
ai_orchestration_coordinator_init PROC

    ; Allocate coordinator state
    mov rcx, COORDINATOR_STATE_SIZE
    call masm_malloc
    test rax, rax
    jz coord_init_fail
    
    mov qword ptr [globalCoordinator], rax
    mov rsi, rax                        ; rsi = coordinator state
    
    ; Store main window handle
    mov qword ptr [rsi + 24], rcx
    
    ; Initialize subsystems
    call agentic_inference_stream_init
    test eax, eax
    jz coord_init_fail
    
    call autonomous_task_executor_init
    test eax, eax
    jz coord_init_fail
    
    call agentic_failure_recovery_init
    test eax, eax
    jz coord_init_fail
    
    ; Create coordination worker thread
    mov rcx, rsi
    call CreateThread
    mov qword ptr [rsi + 56], rax       ; hWorkerThread
    
    ; Create stop event
    call CreateEventA
    mov qword ptr [rsi + 64], rax       ; hStopEvent
    
    mov byte ptr [coordinatorInitialized], 1  ; Log initialization
    mov rcx, qword ptr [outputLogHandle]
    lea rdx, [szCoordInitMsg]
    call output_pane_append
    
    mov eax, 1
    ret
    
coord_init_fail:
    mov eax, 0
    ret

ai_orchestration_coordinator_init ENDP

; ============================================================================

; ai_orchestration_infer_async(prompt: LPCSTR, mode: LPCSTR, priority: BYTE)
; Start inference asynchronously
; rcx = prompt string
; rdx = mode (ask/edit/plan/configure)
; r8b = priority (0-100)
; Returns: eax = inference stream ID
PUBLIC ai_orchestration_infer_async
ai_orchestration_infer_async PROC

    ; Store prompt in inference queue
    mov r12, rcx                        ; r12 = prompt
    mov r13, rdx                        ; r13 = mode
    mov r14d, r8d                       ; r14d = priority
    
    ; Update state
    mov byte ptr [currentlyInferring], 1
    
    ; Log inference start
    mov rcx, qword ptr [outputLogHandle]
    lea rdx, [szInferStartMsg]
    call output_pane_append
    
    ; Increment operation count
    inc [totalOperations]
    inc [inferenceCount]
    
    ; Start inference stream
    mov rcx, r12                        ; prompt
    mov edx, 512                        ; max tokens
    movsd xmm0, [temperature]           ; temperature parameter
    call agentic_inference_stream_start
    
    ret

ai_orchestration_infer_async ENDP

; ============================================================================

; ai_orchestration_execute_task_async(goal: LPCSTR, priority: BYTE)
; Start autonomous task execution
; rcx = goal string
; edx = priority (0-100)
; Returns: eax = task ID
PUBLIC ai_orchestration_execute_task_async
ai_orchestration_execute_task_async PROC

    ; Store task in queue
    mov r12, rcx                        ; r12 = goal
    mov r14d, edx                       ; r14d = priority
    
    ; Update state
    mov byte ptr [currentlyExecuting], 1
    
    ; Log task start
    mov rcx, qword ptr [outputLogHandle]
    lea rdx, [szTaskStartMsg]
    call output_pane_append
    
    ; Increment counters
    inc [totalOperations]
    inc [autonomousTaskCount]
    
    ; Schedule task
    mov rcx, r12                        ; goal
    mov edx, r14d                       ; priority
    mov r8b, 1                          ; auto-retry
    call autonomous_task_schedule
    
    ret

ai_orchestration_execute_task_async ENDP

; ============================================================================

; ai_orchestration_handle_inference_result(streamId: QWORD, result: LPCSTR, timeMs: QWORD)
; Process completed inference
; rcx = stream ID
; rdx = result string
; r8 = time in milliseconds
; Returns: 1 if recovery needed, 0 if success
PUBLIC ai_orchestration_handle_inference_result
ai_orchestration_handle_inference_result PROC

    mov r12, rcx                        ; r12 = stream ID
    mov r13, rdx                        ; r13 = result
    mov r14, r8                         ; r14 = time
    
    ; Detect failures
    mov rcx, r13                        ; result
    mov rdx, r14                        ; time
    call agentic_failure_detect
    
    test eax, eax                       ; Any failure detected?
    jz result_success
    
    ; Failure detected - trigger recovery
    mov byte ptr [currentlyRecovering], 1
    inc [totalOperations]
    inc [failureRecoveryCount]
    
    ; Log failure
    mov rcx, qword ptr [outputLogHandle]
    lea rdx, [szFailureDetectedMsg]
    call output_pane_append
    
    mov eax, 1                          ; Signal recovery needed
    ret
    
result_success:
    ; Update metrics
    mov rcx, r14
    call calculate_operation_latency
    
    mov byte ptr [currentlyInferring], 0
    
    ; Log success
    mov rcx, qword ptr [outputLogHandle]
    lea rdx, [szInferCompleteMsg]
    call output_pane_append
    
    xor eax, eax                        ; No recovery needed
    ret

ai_orchestration_handle_inference_result ENDP

; ============================================================================

; ai_orchestration_handle_task_result(taskId: QWORD, result: LPCSTR)
; Process completed task
; rcx = task ID
; rdx = result string
; Returns: 1 success, 0 failure
PUBLIC ai_orchestration_handle_task_result
ai_orchestration_handle_task_result PROC

    mov r12, rcx                        ; r12 = task ID
    mov r13, rdx                        ; r13 = result
    
    ; Verify task completed
    mov rcx, r12
    call autonomous_task_status
    cmp eax, 2                          ; status = completed?
    je task_completed
    
    ; Task failed - mark for retry
    mov rcx, qword ptr [outputLogHandle]
    lea rdx, [szTaskFailedMsg]
    call output_pane_append
    
    xor eax, eax
    ret
    
task_completed:
    ; Update chat with result
    mov rcx, qword ptr [agenticChatHandle]
    mov rdx, r13                        ; result
    call append_task_result_to_chat
    
    ; Update state
    mov byte ptr [currentlyExecuting], 0
    
    ; Log completion
    mov rcx, qword ptr [outputLogHandle]
    lea rdx, [szTaskCompletedMsg]
    call output_pane_append
    
    mov eax, 1
    ret

ai_orchestration_handle_task_result ENDP

; ============================================================================

; ai_orchestration_get_status()
; Get current orchestration status
; Returns: rax = pointer to JSON status object
PUBLIC ai_orchestration_get_status
ai_orchestration_get_status PROC

    ; Allocate JSON status object
    mov rcx, 2048
    call masm_malloc
    mov r8, rax
    
    ; Build JSON
    mov rcx, r8
    lea rdx, [szJsonOpen]
    call wsprintfA
    
    ; Add state
    mov rcx, r8
    lea rdx, [szJsonStateKey]
    mov r8d, [coordState]
    call wsprintfA
    
    ; Add metrics
    mov rcx, r8
    lea rdx, [szJsonTotalOpsKey]
    mov r8, [totalOperations]
    call wsprintfA
    
    ; Add inference count
    mov rcx, r8
    lea rdx, [szJsonInferencesKey]
    mov r8, [inferenceCount]
    call wsprintfA
    
    ; Add task count
    mov rcx, r8
    lea rdx, [szJsonTasksKey]
    mov r8, [autonomousTaskCount]
    call wsprintfA
    
    ; Add recovery count
    mov rcx, r8
    lea rdx, [szJsonRecoveriesKey]
    mov r8, [failureRecoveryCount]
    call wsprintfA
    
    ; Add latency
    mov rcx, r8
    lea rdx, [szJsonLatencyKey]
    mov r8, [coordinationLatency]
    call wsprintfA
    
    ; Add current state
    mov rcx, r8
    lea rdx, [szJsonInferringKey]
    movzx r8d, byte ptr [currentlyInferring]
    call wsprintfA
    
    mov rcx, r8
    lea rdx, [szJsonExecutingKey]
    movzx r8d, byte ptr [currentlyExecuting]
    call wsprintfA
    
    mov rcx, r8
    lea rdx, [szJsonRecoveringKey]
    movzx r8d, byte ptr [currentlyRecovering]
    call wsprintfA
    
    ; Close JSON
    mov rcx, r8
    lea rdx, [szJsonClose]
    call wsprintfA
    
    mov rax, r8
    ret

ai_orchestration_get_status ENDP

; ============================================================================

; ai_orchestration_shutdown()
; Shutdown the coordinator gracefully
; Returns: 1 success
PUBLIC ai_orchestration_shutdown
ai_orchestration_shutdown PROC

    mov rsi, qword ptr [globalCoordinator]
    test rsi, rsi
    jz shutdown_done
    
    ; Signal worker thread to stop
    mov rcx, [rsi + 64]
    call SetEvent
    
    ; Wait for thread to finish
    mov rcx, [rsi + 56]
    mov edx, 5000
    call WaitForSingleObject
    
    ; Close handles
    mov rcx, [rsi + 64]
    call CloseHandle
    
    mov rcx, [rsi + 56]
    call CloseHandle
    
    ; Free coordinator
    mov rcx, rsi
    call masm_free
    
    mov qword ptr [globalCoordinator], 0
    mov byte ptr [coordinatorInitialized], 0  ; Log shutdown
    mov rcx, qword ptr [outputLogHandle]
    lea rdx, [szCoordShutdownMsg]
    call output_pane_append
    
shutdown_done:
    mov eax, 1
    ret

ai_orchestration_shutdown ENDP

; ============================================================================
; INTERNAL WORKER THREAD
; ============================================================================

; coordination_worker_thread(coordinatorState: PCOORDINATOR_STATE)
; Main coordination loop
; rcx = coordinator state pointer
; Returns: 0 = normal, nonzero = error
coordination_worker_thread PROC

    mov rsi, rcx                        ; rsi = coordinator state
    
coord_loop:
    ; Check stop signal
    mov rcx, [rsi + 64]                 ; hStopEvent
    mov edx, 0                          ; Don't wait
    call WaitForSingleObject
    cmp eax, WAIT_OBJECT_0
    je coord_shutdown
    
    ; Execute one pending task
    call autonomous_task_execute_pending
    test eax, eax
    jz skip_task_execute
    
    ; Wait for task completion
    mov ecx, 100                        ; 100ms poll interval
    call Sleep
    
skip_task_execute:
    ; Get next inference result
    mov rcx, [rsi + 48]                 ; inference queue
    call dequeue_inference_result
    test rax, rax
    jz skip_inference_process
    
    ; Process inference result
    mov rcx, rax
    call agentic_failure_detect
    
skip_inference_process:
    ; Brief sleep to avoid spinning
    mov ecx, 50
    call Sleep
    
    jmp coord_loop
    
coord_shutdown:
    xor eax, eax
    ret

coordination_worker_thread ENDP

; ============================================================================
; HELPER FUNCTIONS
; ============================================================================

; calculate_operation_latency(timeMs: QWORD)
; Update latency metrics
; rcx = operation time in milliseconds
calculate_operation_latency PROC

    ; Convert to microseconds
    mov rax, rcx
    mov rcx, 1000
    imul rax, rcx
    
    ; Simple running average
    mov rcx, [coordinationLatency]
    add rcx, rax
    mov rdx, 2
    xor edx, edx
    div rdx
    
    mov [coordinationLatency], rax
    ret

calculate_operation_latency ENDP

; ============================================================================

; append_task_result_to_chat(chatHandle: QWORD, result: LPCSTR)
; Display task result in chat
; rcx = chat handle
; rdx = result string
append_task_result_to_chat PROC

    ; Send EM_REPLACESEL message to append result
    mov r8, rcx                         ; Save chat handle
    mov r9, rdx                         ; Save result
    
    mov rcx, r8
    mov edx, EM_REPLACESEL
    mov r8d, 0
    call SendMessageA
    
    ret

append_task_result_to_chat ENDP

; ============================================================================

; dequeue_inference_result()
; Get next completed inference
; Returns: rax = result pointer (0 if none)
dequeue_inference_result PROC

    ; Check inference stream pool
    lea rax, [streamPool]
    mov rcx, MAX_ACTIVE_STREAMS
    
check_stream:
    cmp rcx, 0
    jle no_results
    
    mov rdx, [rax]
    test rdx, rdx
    jz next_stream
    
    ; Check if stream is done
    cmp dword ptr [rdx + 64], 2         ; status = done?
    jne next_stream
    
    ; Get result
    mov rcx, rdx
    call get_stream_result
    ret
    
next_stream:
    add rax, 8
    dec rcx
    jmp check_stream
    
no_results:
    xor eax, eax
    ret

dequeue_inference_result ENDP

; ============================================================================

; Real-time monitoring integration point
; Called periodically (e.g., from main loop via timer)

; ai_orchestration_poll()
; Poll the orchestrator for updates
; Call this from WM_TIMER in main window
PUBLIC ai_orchestration_poll
ai_orchestration_poll PROC

    ; Check coordinator initialized
    cmp [coordinatorInitialized], 0
    je poll_done
    
    ; Check pending tasks
    call autonomous_task_execute_pending
    
    ; Check inference results
    call dequeue_inference_result
    test rax, rax
    jz check_no_result
    
    ; Handle result
    mov rcx, rax
    call agentic_failure_detect
    
check_no_result:
    ; Update UI status
    mov rcx, qword ptr [outputLogHandle]
    lea rdx, [szPollTickMsg]
    call output_pane_append
    
poll_done:
    ret

ai_orchestration_poll ENDP

; ============================================================================

; Installation function for integration
; Call once during IDE startup

; ai_orchestration_install()
; Install orchestration into main window message loop
; rcx = main window handle
PUBLIC ai_orchestration_install
ai_orchestration_install PROC

    ; Save window handle
    mov rsi, rcx

    ; Initialize coordinator
    mov rcx, rsi
    call ai_orchestration_coordinator_init
    
    ; Install poll timer (every 50ms)
    mov rcx, rsi
    mov edx, TIMER_POLL_ID               ; Timer ID
    mov r8d, 50                         ; 50ms interval
    xor r9, r9                          ; NULL callback
    call SetTimer
    
    ; Log installation
    mov rcx, qword ptr [outputLogHandle]
    lea rdx, [szInstalledMsg]
    call output_pane_append
    
    ret

ai_orchestration_install ENDP

; ai_orchestration_set_handles(hOutput: HWND, hChat: HWND)
; Set UI handles for logging and chat
PUBLIC ai_orchestration_set_handles
ai_orchestration_set_handles PROC
    mov qword ptr [outputLogHandle], rcx
    mov qword ptr [agenticChatHandle], rdx
    ret
ai_orchestration_set_handles ENDP

; ai_orchestration_schedule_task(goal: LPCSTR, priority: DWORD, autoRetry: BYTE)
; Schedule a task for autonomous execution
PUBLIC ai_orchestration_schedule_task
ai_orchestration_schedule_task PROC
    ; rcx = goal string
    ; edx = priority
    ; r8b = auto-retry
    call autonomous_task_schedule
    ret
ai_orchestration_schedule_task ENDP

END

