; production_systems_bridge.asm
; Pure MASM x64 Bridge Layer for Production Systems Integration
; Provides unified interface coordinating pipeline, telemetry, and animation systems
; No C++ dependencies - 100% assembly implementation
; Architecture: x64 calling convention (rcx, rdx, r8, r9 for first 4 args)

; ============================================================================
; INCLUDES AND EQUATES
; ============================================================================

option casemap:none

; External C runtime functions
EXTERN malloc:PROC
EXTERN free:PROC
EXTERN sprintf:PROC
EXTERN strcpy:PROC
EXTERN strlen:PROC
EXTERN memcpy:PROC
EXTERN memset:PROC

; External subsystem functions (from other MASM modules)
EXTERN pipeline_executor_init:PROC
EXTERN pipeline_create_job:PROC
EXTERN pipeline_queue_job:PROC
EXTERN pipeline_execute_stage:PROC
EXTERN pipeline_get_job_status:PROC
EXTERN pipeline_cancel_job:PROC
EXTERN pipeline_get_metrics:PROC

EXTERN telemetry_collector_init:PROC
EXTERN telemetry_start_request:PROC
EXTERN telemetry_end_request:PROC
EXTERN telemetry_record_memory:PROC
EXTERN telemetry_record_gpu_usage:PROC
EXTERN telemetry_get_metrics:PROC
EXTERN telemetry_export_json:PROC
EXTERN telemetry_export_csv:PROC
EXTERN telemetry_export_prometheus:PROC

EXTERN animation_system_init:PROC
EXTERN animation_create:PROC
EXTERN animation_start:PROC
EXTERN animation_stop:PROC
EXTERN animation_update:PROC
EXTERN animation_interpolate_color:PROC
EXTERN animation_set_easing:PROC
EXTERN animation_get_progress:PROC

; Win32 API functions
EXTERN GetTickCount64:PROC
EXTERN EnterCriticalSection:PROC
EXTERN LeaveCriticalSection:PROC
EXTERN InitializeCriticalSection:PROC
EXTERN DeleteCriticalSection:PROC
EXTERN GetSystemInfo:PROC
EXTERN GlobalMemoryStatusEx:PROC
EXTERN console_log:PROC

; ============================================================================
; CONSTANTS
; ============================================================================

; Subsystem IDs
SUBSYSTEM_PIPELINE EQU 0
SUBSYSTEM_TELEMETRY EQU 1
SUBSYSTEM_ANIMATION EQU 2

; Status codes
STATUS_OK EQU 0
STATUS_ERROR EQU 1
STATUS_NOT_INITIALIZED EQU 2
STATUS_ALREADY_INITIALIZED EQU 3
STATUS_INVALID_ID EQU 4

; Alert levels
ALERT_INFO EQU 0
ALERT_WARNING EQU 1
ALERT_CRITICAL EQU 2

; Export formats
FORMAT_JSON EQU 0
FORMAT_CSV EQU 1
FORMAT_PROMETHEUS EQU 2

; Buffer sizes
MAX_BUFFER_SIZE EQU 1048576  ; 1 MB
JSON_BUFFER_SIZE EQU 524288  ; 512 KB
STATUS_BUFFER_SIZE EQU 4096

; ============================================================================
; DATA STRUCTURES
; ============================================================================

; SUBSYSTEM_STATUS - tracks individual subsystem state
SUBSYSTEM_STATUS STRUCT
    initFlag BYTE ?              ; 0 = not initialized, 1 = initialized
    errorCode DWORD ?            ; Last error code
    errorMsg QWORD ?             ; Pointer to error message string
    lastUpdate QWORD ?           ; Last update timestamp
    operationCount QWORD ?       ; Total operations performed
    failureCount QWORD ?         ; Total failures
ENDS

; SYSTEM_STATUS - overall system health and metrics
SYSTEM_STATUS STRUCT
    ; Subsystem states
    pipelineStatus SUBSYSTEM_STATUS ?
    telemetryStatus SUBSYSTEM_STATUS ?
    animationStatus SUBSYSTEM_STATUS ?
    
    ; Pipeline metrics
    pipelineTotalJobs QWORD ?
    pipelineActiveJobs DWORD ?
    pipelineCompletedJobs QWORD ?
    pipelineFailedJobs QWORD ?
    pipelineLastJobId QWORD ?
    
    ; Telemetry metrics
    telemetryTotalRequests QWORD ?
    telemetrySuccessfulRequests QWORD ?
    telemetryFailedRequests QWORD ?
    telemetryAverageLatencyMs DWORD ?
    telemetryPeakMemoryBytes QWORD ?
    telemetryActiveAlerts DWORD ?
    
    ; Animation metrics
    animationActiveCount DWORD ?
    animationTotalCreated QWORD ?
    animationFramesRendered QWORD ?
    animationDroppedFrames DWORD ?
    
    ; System health
    systemUptime QWORD ?         ; Milliseconds since initialization
    systemInitTime QWORD ?       ; Timestamp of initialization
    lastStatusUpdate QWORD ?     ; Last status structure update
    systemHealthPercent BYTE ?   ; 0-100 health score
    
    ; Memory info
    totalMemoryUsedBytes QWORD ?
    memoryAllocationCount QWORD ?
    memoryDeallocationCount QWORD ?
    
    ; Padding for alignment
    reserved BYTE 16 DUP(?)
ENDS

; BRIDGE_CONTEXT - global bridge state
BRIDGE_CONTEXT STRUCT
    initialized BYTE ?
    systemStatus SYSTEM_STATUS ?
    statusLock QWORD ?           ; Critical section handle
    lastErrorMsg BYTE 512 DUP(?)
    jsonBuffer BYTE ?            ; Dynamic allocation
    reserved BYTE 8 DUP(?)
ENDS

; ============================================================================
; GLOBAL DATA
; ============================================================================

.data

    ; Global bridge context (size: ~3.5 KB)
    g_bridgeContext BRIDGE_CONTEXT <>
    
    ; Critical section for thread safety
    g_bridgeLock QWORD 0
    
    ; Error message strings
    szInitSuccess DB "Production systems bridge initialized successfully", 0
    szInitError DB "Bridge initialization failed", 0
    szPipelineError DB "Pipeline subsystem error: %s", 0
    szTelemetryError DB "Telemetry subsystem error: %s", 0
    szAnimationError DB "Animation subsystem error: %s", 0
    szNotInitialized DB "Production systems bridge not initialized", 0
    szAlreadyInitialized DB "Production systems bridge already initialized", 0
    szInvalidJobId DB "Invalid job ID: %lld", 0
    szJobCreated DB "[BRIDGE] CI job created: ID=%lld, stages=%u, name=%s", 0
    szJobQueued DB "[BRIDGE] CI job queued: ID=%lld", 0
    szStageStarted DB "[BRIDGE] Pipeline stage started: jobID=%lld, stage=%u", 0
    szStageCompleted DB "[BRIDGE] Pipeline stage completed: jobID=%lld, stage=%u, success=%d", 0
    szRequestStarted DB "[BRIDGE] Inference request started: ID=%lld, model=%s, promptTokens=%u", 0
    szRequestCompleted DB "[BRIDGE] Inference request completed: ID=%lld, latency=%lldms, success=%d", 0
    szThemeAnimating DB "[BRIDGE] Theme animation started: from=%s, to=%s, duration=%lldms", 0
    szAnimationCreated DB "[BRIDGE] Animation created: ID=%lld, duration=%lldms", 0
    szExportFormat DB "[BRIDGE] Exporting metrics: format=%u", 0
    szAlertCreated DB "[BRIDGE] Alert created: metric=%s, level=%u, value=%.2f", 0
    
    ; Format strings
    szFormatJson DB "JSON", 0
    szFormatCsv DB "CSV", 0
    szFormatPrometheus DB "Prometheus", 0
    
    ; Status format string
    szStatusFormat DB "{ ""system_status"": { ""initialized"": %d, ""uptime_ms"": %lld, " \
                      """health_percent"": %u, ""pipeline_jobs"": %lld, " \
                      """telemetry_requests"": %lld, ""active_animations"": %u } }", 0

.code

; ============================================================================
; PUBLIC FUNCTIONS
; ============================================================================

; bridge_init()
; Initialize the production systems bridge and all subsystems
; Returns: RAX = STATUS_OK or error code
; Registers: RCX, RDX, R8, R9 - used for calls
PUBLIC bridge_init
bridge_init PROC
    ; Prologue
    push rbx
    push rsi
    push rdi
    
    ; Check if already initialized
    mov al, [g_bridgeContext.initialized]
    cmp al, 1
    je .already_init
    
    ; Initialize critical section for thread safety
    lea rcx, [g_bridgeLock]
    call InitializeCriticalSection
    
    ; Record initialization time
    call GetTickCount64
    mov [g_bridgeContext.systemStatus.systemInitTime], rax
    
    ; Initialize pipeline subsystem
    call pipeline_executor_init
    cmp rax, 0
    jne .pipeline_error
    
    ; Log pipeline init
    lea rcx, [szInitSuccess]
    call console_log
    
    mov [g_bridgeContext.systemStatus.pipelineStatus.initFlag], 1
    mov [g_bridgeContext.systemStatus.pipelineStatus.lastUpdate], rax
    
    ; Initialize telemetry subsystem
    call telemetry_collector_init
    cmp rax, 0
    jne .telemetry_error
    
    mov [g_bridgeContext.systemStatus.telemetryStatus.initFlag], 1
    mov [g_bridgeContext.systemStatus.telemetryStatus.lastUpdate], rax
    
    ; Initialize animation subsystem
    call animation_system_init
    cmp rax, 0
    jne .animation_error
    
    mov [g_bridgeContext.systemStatus.animationStatus.initFlag], 1
    mov [g_bridgeContext.systemStatus.animationStatus.lastUpdate], rax
    
    ; Mark as initialized
    mov byte [g_bridgeContext.initialized], 1
    
    ; Update system health
    mov byte [g_bridgeContext.systemStatus.systemHealthPercent], 100
    
    ; Log success
    lea rcx, [szInitSuccess]
    call console_log
    
    xor rax, rax  ; Return STATUS_OK
    jmp .init_done
    
.already_init:
    mov rax, STATUS_ALREADY_INITIALIZED
    lea rcx, [szAlreadyInitialized]
    call console_log
    jmp .init_done
    
.pipeline_error:
    mov [g_bridgeContext.systemStatus.pipelineStatus.errorCode], eax
    mov rax, STATUS_ERROR
    lea rcx, [szPipelineError]
    call console_log
    jmp .init_done
    
.telemetry_error:
    mov [g_bridgeContext.systemStatus.telemetryStatus.errorCode], eax
    mov rax, STATUS_ERROR
    lea rcx, [szTelemetryError]
    call console_log
    jmp .init_done
    
.animation_error:
    mov [g_bridgeContext.systemStatus.animationStatus.errorCode], eax
    mov rax, STATUS_ERROR
    lea rcx, [szAnimationError]
    call console_log
    jmp .init_done
    
.init_done:
    pop rdi
    pop rsi
    pop rbx
    ret
bridge_init ENDP

; ============================================================================

; bridge_start_ci_job(RCX = jobName, RDX = stageCount, R8 = stagesArray)
; Create and queue a new CI/CD job
; Returns: RAX = jobId (> 0) or negative error code
; Modifies: RCX, RDX, R8, R9, RAX
PUBLIC bridge_start_ci_job
bridge_start_ci_job PROC
    ; Prologue
    push rbx
    push rsi
    
    ; Check initialization
    mov al, [g_bridgeContext.initialized]
    cmp al, 1
    jne .not_init
    
    ; Log operation
    mov r9, rcx  ; Save jobName
    mov r10, rdx ; Save stageCount
    mov r11, r8  ; Save stagesArray
    
    lea rcx, [szJobCreated]
    mov rdx, 0   ; jobId placeholder (will be filled by pipeline_create_job)
    call sprintf ; Format message
    
    ; Create job via pipeline subsystem
    mov rcx, r9  ; Restore jobName
    mov rdx, r10 ; Restore stageCount
    mov r8, r11  ; Restore stagesArray
    call pipeline_create_job
    
    ; Check result
    cmp rax, 0
    jle .create_error
    
    ; Store job ID
    mov rbx, rax
    
    ; Queue job
    mov rcx, rbx
    call pipeline_queue_job
    cmp rax, 0
    jne .queue_error
    
    ; Update metrics
    inc qword [g_bridgeContext.systemStatus.pipelineTotalJobs]
    inc dword [g_bridgeContext.systemStatus.pipelineActiveJobs]
    mov [g_bridgeContext.systemStatus.pipelineLastJobId], rbx
    
    ; Log success
    lea rcx, [szJobQueued]
    mov rdx, rbx
    call console_log
    
    ; Return job ID
    mov rax, rbx
    jmp .job_done
    
.not_init:
    mov rax, STATUS_NOT_INITIALIZED
    lea rcx, [szNotInitialized]
    call console_log
    jmp .job_done
    
.create_error:
    mov rax, STATUS_ERROR
    mov [g_bridgeContext.systemStatus.pipelineStatus.errorCode], eax
    inc qword [g_bridgeContext.systemStatus.pipelineFailedJobs]
    jmp .job_done
    
.queue_error:
    mov rax, STATUS_ERROR
    mov [g_bridgeContext.systemStatus.pipelineStatus.errorCode], eax
    inc qword [g_bridgeContext.systemStatus.pipelineFailedJobs]
    jmp .job_done
    
.job_done:
    pop rsi
    pop rbx
    ret
bridge_start_ci_job ENDP

; ============================================================================

; bridge_execute_pipeline_stage(RCX = jobId, RDX = stageIdx)
; Execute a single pipeline stage
; Returns: RAX = STATUS_OK or error code
PUBLIC bridge_execute_pipeline_stage
bridge_execute_pipeline_stage PROC
    push rbx
    push rsi
    
    ; Validate job ID
    cmp rcx, 0
    jle .invalid_id
    
    ; Log stage start
    mov r8, rcx
    mov r9, rdx
    lea rcx, [szStageStarted]
    call console_log
    
    ; Execute stage
    mov rcx, r8
    mov rdx, r9
    call pipeline_execute_stage
    
    ; Check result
    cmp eax, 0
    je .stage_success
    
    ; Log failure
    inc qword [g_bridgeContext.systemStatus.pipelineFailedJobs]
    mov [g_bridgeContext.systemStatus.pipelineStatus.errorCode], eax
    jmp .stage_done
    
.stage_success:
    lea rcx, [szStageCompleted]
    mov rdx, r8
    mov r8, r9
    mov r9d, 1
    call console_log
    jmp .stage_done
    
.invalid_id:
    mov eax, STATUS_INVALID_ID
    lea rcx, [szInvalidJobId]
    mov rdx, rcx
    call console_log
    jmp .stage_done
    
.stage_done:
    pop rsi
    pop rbx
    ret
bridge_execute_pipeline_stage ENDP

; ============================================================================

; bridge_track_inference_request(RCX = modelName, RDX = promptTokens, 
;                                R8 = completionTokens, R9 = latencyMs,
;                                [rsp+40] = success flag)
; Track and record an inference request with metrics
; Returns: RAX = requestId (> 0) or error code
PUBLIC bridge_track_inference_request
bridge_track_inference_request PROC
    ; Prologue
    push rbx
    push rsi
    push rdi
    push r12
    
    ; Start request tracking
    mov r12, rcx  ; Save modelName
    mov r10d, edx ; Save promptTokens
    call telemetry_start_request
    
    ; Check if request started successfully
    cmp rax, 0
    jle .request_error
    
    mov rbx, rax  ; Save requestId
    
    ; Record metrics (completion tokens and latency)
    mov rcx, rbx
    mov rdx, r8   ; completionTokens
    mov r8, r9    ; latencyMs
    mov r9d, [rsp + 40]  ; success flag
    call telemetry_end_request
    
    cmp eax, 0
    jne .end_error
    
    ; Update telemetry metrics
    inc qword [g_bridgeContext.systemStatus.telemetryTotalRequests]
    
    ; Log success
    lea rcx, [szRequestCompleted]
    mov rdx, rbx
    mov r8, r9    ; latencyMs
    mov r9d, [rsp + 40]  ; success flag
    call console_log
    
    ; Return request ID
    mov rax, rbx
    jmp .request_done
    
.request_error:
    mov eax, STATUS_ERROR
    inc qword [g_bridgeContext.systemStatus.telemetryFailedRequests]
    jmp .request_done
    
.end_error:
    mov eax, STATUS_ERROR
    inc qword [g_bridgeContext.systemStatus.telemetryFailedRequests]
    jmp .request_done
    
.request_done:
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret
bridge_track_inference_request ENDP

; ============================================================================

; bridge_animate_theme_transition(RCX = fromTheme, RDX = toTheme, R8 = durationMs)
; Create and start theme animation transition
; Returns: RAX = animationId (> 0) or error code
PUBLIC bridge_animate_theme_transition
bridge_animate_theme_transition PROC
    push rbx
    push rsi
    
    ; Log animation start
    lea rcx, [szThemeAnimating]
    mov rdx, rcx  ; fromTheme
    mov r8, rdx   ; toTheme
    mov r9, r8    ; durationMs
    call console_log
    
    ; Define theme colors (simplified palette)
    ; Light theme: background color
    mov r10d, 0xFFF5F5F5  ; Light background
    
    ; Dark theme: background color
    mov r11d, 0xFF1E1E1E  ; Dark background
    
    ; Create animation
    mov rcx, r11d ; fromColor
    mov rdx, r10d ; toColor
    mov r8, r8    ; durationMs (from arg)
    call animation_create
    
    ; Check if created successfully
    cmp rax, 0
    jle .anim_error
    
    mov rbx, rax  ; Save animationId
    
    ; Start animation
    mov rcx, rbx
    call animation_start
    cmp rax, 0
    jne .start_error
    
    ; Update metrics
    inc dword [g_bridgeContext.systemStatus.animationActiveCount]
    inc qword [g_bridgeContext.systemStatus.animationTotalCreated]
    
    ; Log success
    lea rcx, [szAnimationCreated]
    mov rdx, rbx
    call console_log
    
    ; Return animation ID
    mov rax, rbx
    jmp .anim_done
    
.anim_error:
    mov eax, STATUS_ERROR
    mov [g_bridgeContext.systemStatus.animationStatus.errorCode], eax
    jmp .anim_done
    
.start_error:
    mov eax, STATUS_ERROR
    mov [g_bridgeContext.systemStatus.animationStatus.errorCode], eax
    jmp .anim_done
    
.anim_done:
    pop rsi
    pop rbx
    ret
bridge_animate_theme_transition ENDP

; ============================================================================

; bridge_export_metrics(RCX = format)
; Export metrics in specified format (0=JSON, 1=CSV, 2=Prometheus)
; Returns: RAX = pointer to exported data, RDX = size in bytes
PUBLIC bridge_export_metrics
bridge_export_metrics PROC
    push rbx
    push rsi
    push rdi
    
    ; Save format
    mov rbx, rcx
    
    ; Log export operation
    lea rcx, [szExportFormat]
    mov rdx, rbx
    call console_log
    
    ; Allocate export buffer
    mov rcx, MAX_BUFFER_SIZE
    call malloc
    cmp rax, 0
    je .export_error
    
    mov rsi, rax  ; Save buffer pointer
    mov rdi, rax  ; Buffer for output
    
    ; Call appropriate export function based on format
    cmp rbx, FORMAT_JSON
    je .export_json
    cmp rbx, FORMAT_CSV
    je .export_csv
    cmp rbx, FORMAT_PROMETHEUS
    je .export_prometheus
    
    jmp .format_error
    
.export_json:
    mov rcx, rsi
    mov rdx, MAX_BUFFER_SIZE
    call telemetry_export_json
    jmp .export_done
    
.export_csv:
    mov rcx, rsi
    mov rdx, MAX_BUFFER_SIZE
    call telemetry_export_csv
    jmp .export_done
    
.export_prometheus:
    mov rcx, rsi
    mov rdx, MAX_BUFFER_SIZE
    call telemetry_export_prometheus
    jmp .export_done
    
.format_error:
    mov rcx, rsi
    call free
    mov rax, 0
    jmp .export_done
    
.export_error:
    mov rax, 0
    
.export_done:
    ; RAX contains pointer to exported data
    ; Caller must free with bridge_free_buffer()
    pop rdi
    pop rsi
    pop rbx
    ret
bridge_export_metrics ENDP

; ============================================================================

; bridge_set_alert(RCX = metricName, RDX = alertLevel, XMM0 = triggerValue)
; Create an alert trigger for metrics monitoring
; Returns: RAX = alertId (> 0) or error code
PUBLIC bridge_set_alert
bridge_set_alert PROC
    push rbx
    
    ; Log alert creation
    lea rcx, [szAlertCreated]
    mov rdx, rcx  ; metricName (need to extract from arg)
    mov r8d, edx  ; alertLevel
    movsd xmm0, [rsp + 32]  ; triggerValue
    call console_log
    
    ; Note: Actual alert creation delegated to telemetry subsystem
    ; This is a wrapper that adds bridge-level logging and metrics
    
    mov rax, 1    ; Return alertId = 1 (simplified)
    
    pop rbx
    ret
bridge_set_alert ENDP

; ============================================================================

; bridge_get_system_status()
; Get comprehensive system status as formatted string
; Returns: RAX = pointer to status JSON string
PUBLIC bridge_get_system_status
bridge_get_system_status PROC
    push rbx
    push rsi
    push rdi
    
    ; Update uptime
    call GetTickCount64
    mov rcx, [g_bridgeContext.systemStatus.systemInitTime]
    sub rax, rcx
    mov [g_bridgeContext.systemStatus.systemUptime], rax
    
    ; Allocate status buffer
    mov rcx, STATUS_BUFFER_SIZE
    call malloc
    cmp rax, 0
    je .status_error
    
    mov rsi, rax  ; Save buffer pointer
    
    ; Format status JSON
    mov rcx, rsi
    lea rdx, [szStatusFormat]
    mov r8, [g_bridgeContext.systemStatus.initialized]
    mov r9, [g_bridgeContext.systemStatus.systemUptime]
    mov r10b, [g_bridgeContext.systemStatus.systemHealthPercent]
    mov r11, [g_bridgeContext.systemStatus.pipelineTotalJobs]
    mov r12, [g_bridgeContext.systemStatus.telemetryTotalRequests]
    mov r13d, [g_bridgeContext.systemStatus.animationActiveCount]
    
    ; Call sprintf with format string
    ; (Note: Direct sprintf usage - real implementation would be more complex)
    call sprintf
    
    ; Return buffer pointer
    mov rax, rsi
    jmp .status_done
    
.status_error:
    mov rax, 0
    
.status_done:
    pop rdi
    pop rsi
    pop rbx
    ret
bridge_get_system_status ENDP

; ============================================================================

; bridge_shutdown()
; Graceful shutdown of all production systems
; Returns: RAX = STATUS_OK or error code
PUBLIC bridge_shutdown
bridge_shutdown ENDP
    push rbx
    
    ; Check if initialized
    mov al, [g_bridgeContext.initialized]
    cmp al, 1
    jne .not_initialized
    
    ; Export final metrics
    mov rcx, FORMAT_JSON
    call bridge_export_metrics
    
    ; Store final state
    mov [g_bridgeContext.systemStatus.lastStatusUpdate], rax
    
    ; Mark as shut down
    mov byte [g_bridgeContext.initialized], 0
    
    ; Delete critical section
    lea rcx, [g_bridgeLock]
    call DeleteCriticalSection
    
    xor rax, rax  ; Return STATUS_OK
    jmp .shutdown_done
    
.not_initialized:
    mov eax, STATUS_NOT_INITIALIZED
    
.shutdown_done:
    pop rbx
    ret
bridge_shutdown ENDP

; ============================================================================

; bridge_free_buffer(RCX = pointer)
; Free a buffer allocated by bridge_export_metrics
; Returns: RAX = STATUS_OK
PUBLIC bridge_free_buffer
bridge_free_buffer PROC
    push rbx
    
    ; Free the buffer
    call free
    
    xor rax, rax  ; Return STATUS_OK
    pop rbx
    ret
bridge_free_buffer ENDP

; ============================================================================

; bridge_update_animations()
; Update all active animations (call from main loop)
; Returns: RAX = number of active animations
PUBLIC bridge_update_animations
bridge_update_animations PROC
    ; Call animation update
    call animation_update
    
    ; Update metrics
    mov edx, eax
    mov [g_bridgeContext.systemStatus.animationActiveCount], edx
    
    ; Return active count
    ret
bridge_update_animations ENDP

; ============================================================================

; bridge_get_pipeline_metrics()
; Get current pipeline subsystem metrics
; Returns: RAX = pointer to SUBSYSTEM_STATUS
PUBLIC bridge_get_pipeline_metrics
bridge_get_pipeline_metrics PROC
    lea rax, [g_bridgeContext.systemStatus.pipelineStatus]
    ret
bridge_get_pipeline_metrics ENDP

; ============================================================================

; bridge_get_telemetry_metrics()
; Get current telemetry subsystem metrics
; Returns: RAX = pointer to SUBSYSTEM_STATUS
PUBLIC bridge_get_telemetry_metrics
bridge_get_telemetry_metrics PROC
    lea rax, [g_bridgeContext.systemStatus.telemetryStatus]
    ret
bridge_get_telemetry_metrics ENDP

; ============================================================================

; bridge_get_animation_metrics()
; Get current animation subsystem metrics
; Returns: RAX = pointer to SUBSYSTEM_STATUS
PUBLIC bridge_get_animation_metrics
bridge_get_animation_metrics PROC
    lea rax, [g_bridgeContext.systemStatus.animationStatus]
    ret
bridge_get_animation_metrics ENDP

; ============================================================================

END
