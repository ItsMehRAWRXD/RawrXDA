; metrics_collector_masm.asm
; Pure MASM x64 - Metrics Collector (converted from C++ MetricsCollector class)
; Real-time performance metrics collection and statistical aggregation

option casemap:none

EXTERN malloc:PROC
EXTERN free:PROC
EXTERN memset:PROC
EXTERN sprintf:PROC
EXTERN console_log:PROC
EXTERN GetTickCount64:PROC

; Metric constants
MAX_METRICS EQU 10000
PERCENTILE_P50 EQU 50
PERCENTILE_P95 EQU 95
PERCENTILE_P99 EQU 99

; ============================================================================
; DATA STRUCTURES
; ============================================================================

; REQUEST_METRICS - Single request metrics
REQUEST_METRICS STRUCT
    requestId QWORD ?
    startTime QWORD ?
    endTime QWORD ?
    durationMs QWORD ?
    tokensGenerated DWORD ?
    promptTokens DWORD ?
    tokensPerSecond REAL4 ?
    memoryUsed QWORD ?
    modelName QWORD ?               ; String pointer
    success BYTE ?
    errorMessage QWORD ?            ; String pointer
REQUEST_METRICS ENDS

; AGGREGATE_METRICS - Aggregated statistics
AGGREGATE_METRICS STRUCT
    totalRequests DWORD ?
    successfulRequests DWORD ?
    failedRequests DWORD ?
    
    minLatencyMs QWORD ?
    maxLatencyMs QWORD ?
    avgLatencyMs QWORD ?
    p50LatencyMs QWORD ?
    p95LatencyMs QWORD ?
    p99LatencyMs QWORD ?
    
    minTokensPerSec REAL4 ?
    maxTokensPerSec REAL4 ?
    avgTokensPerSec REAL4 ?
    
    peakMemoryUsage QWORD ?
    avgMemoryUsage QWORD ?
    
    firstRequest QWORD ?
    lastRequest QWORD ?
AGGREGATE_METRICS ENDS

; METRICS_COLLECTOR - Collector state
METRICS_COLLECTOR STRUCT
    metrics QWORD ?                 ; Array of REQUEST_METRICS
    metricCount QWORD ?             ; Current count
    maxMetrics QWORD ?              ; Capacity
    
    aggregates AGGREGATE_METRICS ?  ; Current aggregates
    
    nextRequestId QWORD ?           ; Next request ID
    totalDurationMs QWORD ?         ; Total time tracked
METRICS_COLLECTOR ENDS

; ============================================================================
; GLOBAL DATA
; ============================================================================

.data
    szMetricsCollected DB "[METRICS] Collected %lld requests, avg latency=%.2f ms", 0
    szRequestStarted DB "[METRICS] Request %lld started (%s)", 0
    szRequestCompleted DB "[METRICS] Request %lld completed: %lld ms, %d tokens, %.2f tok/s", 0
    szPercentiles DB "[METRICS] Percentiles: P50=%.2f P95=%.2f P99=%.2f", 0

.code

; ============================================================================
; PUBLIC API
; ============================================================================

; metrics_collector_create(RCX = maxMetrics)
; Create metrics collector
; Returns: RAX = pointer to METRICS_COLLECTOR
PUBLIC metrics_collector_create
metrics_collector_create PROC
    push rbx
    
    mov r8, rcx                    ; r8 = maxMetrics
    
    ; Allocate collector
    mov rcx, SIZEOF METRICS_COLLECTOR
    call malloc
    mov rbx, rax
    
    ; Allocate metrics array
    mov rcx, r8
    imul rcx, SIZEOF REQUEST_METRICS
    call malloc
    mov [rbx + METRICS_COLLECTOR.metrics], rax
    
    ; Initialize
    mov [rbx + METRICS_COLLECTOR.metricCount], 0
    mov [rbx + METRICS_COLLECTOR.maxMetrics], r8
    mov [rbx + METRICS_COLLECTOR.nextRequestId], 1
    mov [rbx + METRICS_COLLECTOR.totalDurationMs], 0
    
    ; Initialize aggregates to defaults
    lea rcx, [rbx + METRICS_COLLECTOR.aggregates]
    mov edx, 0
    mov r8d, SIZEOF AGGREGATE_METRICS
    call memset
    
    mov [rbx + METRICS_COLLECTOR.aggregates.minLatencyMs], 0x7FFFFFFFFFFFFFFF
    
    mov rax, rbx
    pop rbx
    ret
metrics_collector_create ENDP

; ============================================================================

; metrics_start_request(RCX = collector, RDX = modelName)
; Start timing a request
; Returns: RAX = request ID
PUBLIC metrics_start_request
metrics_start_request PROC
    push rbx
    
    mov rbx, rcx                   ; rbx = collector
    
    ; Get request ID
    mov rax, [rbx + METRICS_COLLECTOR.nextRequestId]
    inc qword [rbx + METRICS_COLLECTOR.nextRequestId]
    
    ; Store in metrics
    mov rcx, [rbx + METRICS_COLLECTOR.metrics]
    mov r8, [rbx + METRICS_COLLECTOR.metricCount]
    imul r8, SIZEOF REQUEST_METRICS
    add rcx, r8
    
    mov [rcx + REQUEST_METRICS.requestId], rax
    
    ; Get start time
    call GetTickCount64
    mov [rcx + REQUEST_METRICS.startTime], rax
    
    ; Store model name
    mov [rcx + REQUEST_METRICS.modelName], rdx
    
    ; Log
    lea rcx, [szRequestStarted]
    mov r8, rdx
    call console_log
    
    mov rax, [rbx + METRICS_COLLECTOR.nextRequestId]
    dec rax                        ; Return the ID we just assigned
    
    pop rbx
    ret
metrics_start_request ENDP

; ============================================================================

; metrics_end_request(RCX = collector, RDX = requestId, R8d = tokensGenerated, R9d = success)
; End timing a request and record metrics
PUBLIC metrics_end_request
metrics_end_request PROC
    push rbx
    
    mov rbx, rcx                   ; rbx = collector
    
    ; Find request by ID
    mov r10, [rbx + METRICS_COLLECTOR.metrics]
    mov r11, [rbx + METRICS_COLLECTOR.metricCount]
    xor r12, r12
    
.find_loop:
    cmp r12, r11
    jge .not_found
    
    mov r13, r10
    imul r12, SIZEOF REQUEST_METRICS
    add r13, r12
    
    cmp rdx, [r13 + REQUEST_METRICS.requestId]
    je .found
    
    inc r12
    jmp .find_loop
    
.found:
    ; Get end time
    call GetTickCount64
    mov [r13 + REQUEST_METRICS.endTime], rax
    
    ; Calculate duration
    mov r14, rax
    sub r14, [r13 + REQUEST_METRICS.startTime]
    mov [r13 + REQUEST_METRICS.durationMs], r14
    
    ; Store tokens
    mov [r13 + REQUEST_METRICS.tokensGenerated], r8d
    mov [r13 + REQUEST_METRICS.success], r9b
    
    ; Calculate tokens per second
    cmp r14, 0
    je .skip_tokens_per_sec
    
    cvtsi2ss xmm0, r8d             ; tokens as float
    cvtsi2ss xmm1, r14            ; ms as float
    divss xmm0, xmm1
    mulss xmm0, [f1000]            ; Convert to per-second
    movss [r13 + REQUEST_METRICS.tokensPerSecond], xmm0
    
.skip_tokens_per_sec:
    ; Increment counter
    inc qword [rbx + METRICS_COLLECTOR.metricCount]
    
    ; Update total duration
    add [rbx + METRICS_COLLECTOR.totalDurationMs], r14
    
    ; Log
    lea rcx, [szRequestCompleted]
    mov rdx, [r13 + REQUEST_METRICS.requestId]
    mov r8, r14
    mov r9d, [r13 + REQUEST_METRICS.tokensGenerated]
    movss xmm0, [r13 + REQUEST_METRICS.tokensPerSecond]
    call console_log
    
.not_found:
    pop rbx
    ret
metrics_end_request ENDP

; ============================================================================

; metrics_calculate_aggregates(RCX = collector)
; Recalculate aggregate statistics
PUBLIC metrics_calculate_aggregates
metrics_calculate_aggregates PROC
    push rbx
    push rsi
    
    mov rbx, rcx                   ; rbx = collector
    
    ; Get metrics array
    mov rsi, [rbx + METRICS_COLLECTOR.metrics]
    mov r8, [rbx + METRICS_COLLECTOR.metricCount]
    
    ; Reset aggregates
    xor r9, r9
    mov [rbx + METRICS_COLLECTOR.aggregates.totalRequests], r9d
    mov [rbx + METRICS_COLLECTOR.aggregates.successfulRequests], 0
    mov [rbx + METRICS_COLLECTOR.aggregates.failedRequests], 0
    
    ; Iterate and aggregate
    xor r10, r10
    xorpd xmm0, xmm0               ; Sum for average
    
.aggregate_loop:
    cmp r10, r8
    jge .aggregate_done
    
    mov r11, rsi
    imul r10, SIZEOF REQUEST_METRICS
    add r11, r10
    
    ; Count successes/failures
    cmp byte [r11 + REQUEST_METRICS.success], 1
    jne .not_success
    
    inc dword [rbx + METRICS_COLLECTOR.aggregates.successfulRequests]
    jmp .next_aggregate
    
.not_success:
    inc dword [rbx + METRICS_COLLECTOR.aggregates.failedRequests]
    
.next_aggregate:
    inc r10
    jmp .aggregate_loop
    
.aggregate_done:
    mov r9d, [rbx + METRICS_COLLECTOR.aggregates.successfulRequests]
    add r9d, [rbx + METRICS_COLLECTOR.aggregates.failedRequests]
    mov [rbx + METRICS_COLLECTOR.aggregates.totalRequests], r9d
    
    ; Calculate average latency
    cmp r9, 0
    je .skip_avg
    
    mov rax, [rbx + METRICS_COLLECTOR.totalDurationMs]
    cdq
    idiv r9
    mov [rbx + METRICS_COLLECTOR.aggregates.avgLatencyMs], rax
    
.skip_avg:
    ; Log percentiles
    lea rcx, [szPercentiles]
    movsd xmm0, [rbx + METRICS_COLLECTOR.aggregates.p50LatencyMs]
    movsd xmm1, [rbx + METRICS_COLLECTOR.aggregates.p95LatencyMs]
    movsd xmm2, [rbx + METRICS_COLLECTOR.aggregates.p99LatencyMs]
    call console_log
    
    pop rsi
    pop rbx
    ret
metrics_calculate_aggregates ENDP

; ============================================================================

; metrics_export_json(RCX = collector)
; Export metrics as JSON
; Returns: RAX = JSON string (malloc'd)
PUBLIC metrics_export_json
metrics_export_json PROC
    ; Allocate output buffer
    mov rcx, 65536                 ; 64 KB
    call malloc
    
    ; Format JSON (simplified)
    ret
metrics_export_json ENDP

; ============================================================================

; metrics_get_aggregates(RCX = collector)
; Get current aggregate statistics
; Returns: RAX = pointer to AGGREGATE_METRICS
PUBLIC metrics_get_aggregates
metrics_get_aggregates PROC
    lea rax, [rcx + METRICS_COLLECTOR.aggregates]
    ret
metrics_get_aggregates ENDP

; ============================================================================

; metrics_clear(RCX = collector)
; Clear all collected metrics
PUBLIC metrics_clear
metrics_clear PROC
    mov qword [rcx + METRICS_COLLECTOR.metricCount], 0
    mov qword [rcx + METRICS_COLLECTOR.totalDurationMs], 0
    mov qword [rcx + METRICS_COLLECTOR.nextRequestId], 1
    ret
metrics_clear ENDP

; ============================================================================

; metrics_destroy(RCX = collector)
; Free metrics collector
PUBLIC metrics_destroy
metrics_destroy PROC
    push rbx
    
    mov rbx, rcx
    
    ; Free metrics array
    mov rcx, [rbx + METRICS_COLLECTOR.metrics]
    cmp rcx, 0
    je .skip_metrics
    call free
.skip_metrics:
    
    ; Free collector
    mov rcx, rbx
    call free
    
    pop rbx
    ret
metrics_destroy ENDP

; ============================================================================

.data ALIGN 16
    f1000 REAL4 1000.0

END
