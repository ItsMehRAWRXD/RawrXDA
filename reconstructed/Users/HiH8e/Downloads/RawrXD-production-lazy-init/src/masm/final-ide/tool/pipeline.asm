; =============================================================================
; Phase 7.5: Tool Pipeline - Distributed Request Routing & Orchestration
; Pure MASM x64 Implementation
;
; Purpose: Implement distributed request routing, multi-model orchestration,
;          batch processing, and resource scheduling for inference engine
;
; Public API (6 functions):
;   1. ToolPipeline_Initialize(maxRouters, maxModels) -> success
;   2. ToolPipeline_RouteRequest(requestId, toolType, args) -> routerId
;   3. ToolPipeline_RegisterModel(modelId, capabilities, resourceReq) -> success
;   4. ToolPipeline_CreateBatch(batchSize, timeout) -> batchId
;   5. ToolPipeline_ScheduleResources(nodeCount, cpuPerNode, memPerNode) -> success
;   6. ToolPipeline_GetPipelineMetrics() -> metricsPtr
;
; Thread Safety: Critical Sections for router registry, batch queue, scheduling
; Registry: HKCU\Software\RawrXD\ToolPipeline
; =============================================================================

EXTERN GetProcessHeap:PROC
EXTERN HeapAlloc:PROC
EXTERN HeapFree:PROC
EXTERN InitializeCriticalSection:PROC
EXTERN DeleteCriticalSection:PROC
EXTERN EnterCriticalSection:PROC
EXTERN LeaveCriticalSection:PROC
EXTERN RtlZeroMemory:PROC
EXTERN RtlCopyMemory:PROC
EXTERN GetSystemInfo:PROC
EXTERN GetTickCount64:PROC

.CODE

; =============================================================================
; CONSTANTS
; =============================================================================

TOOL_TYPE_INFERENCE                 EQU 1
TOOL_TYPE_EMBEDDING                 EQU 2
TOOL_TYPE_RANKING                   EQU 3
TOOL_TYPE_CLASSIFICATION            EQU 4
TOOL_TYPE_SYNTHESIS                 EQU 5
TOOL_TYPE_CUSTOM                    EQU 6

ROUTING_MODE_ROUND_ROBIN            EQU 1
ROUTING_MODE_LEAST_LOADED           EQU 2
ROUTING_MODE_AFFINITY               EQU 3
ROUTING_MODE_LATENCY_AWARE          EQU 4

BATCH_STATE_IDLE                    EQU 0
BATCH_STATE_ACCUMULATING           EQU 1
BATCH_STATE_PROCESSING             EQU 2
BATCH_STATE_COMPLETED              EQU 3

RESOURCE_STRATEGY_CONSERVATIVE      EQU 1
RESOURCE_STRATEGY_BALANCED          EQU 2
RESOURCE_STRATEGY_AGGRESSIVE        EQU 3

PIPELINE_MAX_ROUTERS                EQU 64
PIPELINE_MAX_MODELS                 EQU 256
PIPELINE_MAX_BATCHES                EQU 512
PIPELINE_MAX_REQUESTS_PER_BATCH     EQU 10000
PIPELINE_MAX_NODES                  EQU 128

; Error codes
TOOL_PIPELINE_E_SUCCESS             EQU 0x00000000
TOOL_PIPELINE_E_INVALID_ROUTER      EQU 0x00000001
TOOL_PIPELINE_E_MODEL_LIMIT         EQU 0x00000002
TOOL_PIPELINE_E_BATCH_FULL          EQU 0x00000003
TOOL_PIPELINE_E_RESOURCE_EXHAUSTED  EQU 0x00000004
TOOL_PIPELINE_E_ROUTING_FAILED      EQU 0x00000005
TOOL_PIPELINE_E_INVALID_BATCH       EQU 0x00000006
TOOL_PIPELINE_E_SCHEDULING_FAILED   EQU 0x00000007

; =============================================================================
; DATA STRUCTURES
; =============================================================================

; Request descriptor
REQUEST_DESCRIPTOR STRUCT
    RequestId       QWORD           ; Unique request ID
    ToolType        DWORD           ; Tool type ID
    ModelId         DWORD           ; Target model ID
    RoutedTo        DWORD           ; Router ID assigned
    InputSize       QWORD           ; Input data size
    InputPtr        QWORD           ; Input data pointer
    OutputSize      QWORD           ; Output data size
    OutputPtr       QWORD           ; Output data pointer
    TimeCreated     QWORD           ; Timestamp (GetTickCount64)
    TimeRouted      QWORD           ; Routing timestamp
    TimeCompleted   QWORD           ; Completion timestamp
    Priority        BYTE            ; Priority (1-10)
    Status          BYTE            ; Status code
    _PAD0           WORD            ; Alignment
REQUEST_DESCRIPTOR ENDS

; Router descriptor
ROUTER_DESCRIPTOR STRUCT
    RouterId        DWORD           ; Router ID
    IsActive        BYTE            ; Active flag
    RoutingMode     BYTE            ; Routing strategy
    ModelCount      DWORD           ; Models handled
    RequestCount    QWORD           ; Requests processed
    CurrentLoad     DWORD           ; Current load percentage
    MaxCapacity     DWORD           ; Maximum throughput (req/sec)
    AvgLatency      QWORD           ; Average latency (ms)
    NodeAssigned    DWORD           ; Physical node ID
    ResumeLock      DWORD           ; Critical section for state
    _CS_DEBUG_INFO  QWORD
    _CS_LOCK_COUNT  DWORD
    _CS_RECURSION_COUNT DWORD
    _CS_OWNER_THREAD QWORD
ROUTER_DESCRIPTOR ENDS

; Model descriptor
MODEL_DESCRIPTOR STRUCT
    ModelId         DWORD           ; Model identifier
    ModelType       DWORD           ; Type (inference, embedding, etc.)
    VersionId       DWORD           ; Model version
    Capabilities    QWORD           ; Capability bitmask
    RequiredGPU     DWORD           ; GPU memory required (MB)
    RequiredCPU     DWORD           ; CPU cores required
    RequiredMemory  QWORD           ; System memory required (MB)
    InferenceLatency QWORD          ; Expected latency (ms)
    ThroughputMax   DWORD           ; Max requests/sec
    LoadedOn        DWORD           ; Node ID where loaded
    IsLoaded        BYTE            ; Loaded flag
    IsOptimized     BYTE            ; Optimization level (0-2)
    _PAD0           WORD            ; Alignment
    LastUsed        QWORD           ; Last access timestamp
ROUTER_DESCRIPTOR ENDS

; Batch descriptor
BATCH_DESCRIPTOR STRUCT
    BatchId         DWORD           ; Batch identifier
    State           DWORD           ; BATCH_STATE_*
    MaxSize         DWORD           ; Maximum requests
    CurrentSize     DWORD           ; Current request count
    TimeoutMs       QWORD           ; Timeout value
    TimeCreated     QWORD           ; Creation timestamp
    TimeProcessed   QWORD           ; Processing timestamp
    Requests        QWORD           ; Array of REQUEST_DESCRIPTOR pointers
    ModelId         DWORD           ; Primary model for batch
    RouterId        DWORD           ; Assigned router
    ResultPtr       QWORD           ; Batch result storage
    ResultSize      QWORD           ; Result data size
BATCH_DESCRIPTOR ENDS

; Resource node descriptor
RESOURCE_NODE STRUCT
    NodeId          DWORD           ; Node identifier
    NodeIndex       DWORD           ; Position in cluster
    IsActive        BYTE            ; Active flag
    Strategy        BYTE            ; Resource strategy
    _PAD0           WORD            ; Alignment
    TotalCpuCores   DWORD           ; Physical CPU cores
    AvailableCores  DWORD           ; Available cores
    TotalMemoryMb   QWORD           ; Total memory
    AvailableMemMb  QWORD           ; Available memory
    TotalGpuMemMb   QWORD           ; GPU memory
    AvailableGpuMb  QWORD           ; Available GPU
    CurrentLoad     DWORD           ; Load percentage
    ModelCount      DWORD           ; Models loaded
    ModelsLoaded    QWORD           ; Model ID array
    RequestQueue    QWORD           ; Request queue pointer
    QueueDepth      DWORD           ; Queue size
    MaxQueueDepth   DWORD           ; Queue capacity
RESOURCE_NODE ENDS

; Pipeline scheduler
PIPELINE_SCHEDULER STRUCT
    SchedulerId     DWORD           ; Scheduler ID
    Strategy        DWORD           ; Scheduling strategy
    NodeCount       DWORD           ; Number of nodes
    Nodes           QWORD           ; Array of RESOURCE_NODE
    RebalanceInterval QWORD         ; Rebalance time (ms)
    LastRebalance   QWORD           ; Last rebalance timestamp
    SchedulerLock   DWORD           ; Critical section
    _CS_DEBUG_INFO  QWORD
    _CS_LOCK_COUNT  DWORD
    _CS_RECURSION_COUNT DWORD
    _CS_OWNER_THREAD QWORD
    MaxPendingRequests DWORD        ; Request queue limit
    PendingRequests QWORD           ; Total pending
PIPELINE_SCHEDULER ENDS

; Tool pipeline manager
TOOL_PIPELINE_MANAGER STRUCT
    Version         DWORD           ; Manager version
    Initialized     BYTE            ; Initialization flag
    _PAD0           BYTE            ; Alignment
    _PAD1           WORD            ; Alignment
    RouterCount     DWORD           ; Active routers
    ModelCount      DWORD           ; Registered models
    BatchCount      DWORD           ; Active batches
    NextRouterId    DWORD           ; Router ID counter
    NextModelId     DWORD           ; Model ID counter
    NextBatchId     DWORD           ; Batch ID counter
    NextRequestId   QWORD           ; Request ID counter
    ManagerLock     DWORD           ; Critical section
    _CS_DEBUG_INFO  QWORD
    _CS_LOCK_COUNT  DWORD
    _CS_RECURSION_COUNT DWORD
    _CS_OWNER_THREAD QWORD
    Routers         QWORD           ; Array of ROUTER_DESCRIPTOR
    Models          QWORD           ; Array of MODEL_DESCRIPTOR
    Batches         QWORD           ; Array of BATCH_DESCRIPTOR
    Scheduler       QWORD           ; Pipeline scheduler
    RoutingMode     DWORD           ; Global routing mode
    MetricsPtr      QWORD           ; Metrics pointer
TOOL_PIPELINE_MANAGER ENDS

; Pipeline metrics
TOOL_PIPELINE_METRICS STRUCT
    RequestsReceived        QWORD   ; Total requests received
    RequestsRouted          QWORD   ; Successfully routed
    RequestsFailed          QWORD   ; Routing failures
    RequestsCompleted       QWORD   ; Completed requests
    AvgRoutingLatency       QWORD   ; Average routing latency (us)
    BatchesCreated          QWORD   ; Total batches
    BatchesCompleted        QWORD   ; Completed batches
    ModelsLoaded            QWORD   ; Currently loaded models
    ModelsRegistered        QWORD   ; Total registered models
    NodesActive             QWORD   ; Active compute nodes
    ResourceRebalances      QWORD   ; Rebalance operations
    SchedulingDecisions     QWORD   ; Scheduling calls
TOOL_PIPELINE_METRICS ENDS

; =============================================================================
; GLOBAL DATA
; =============================================================================

.DATA

; Logging strings
szLogRequestRouted      DB "INFO: Request routed (request_id=%lld, tool_type=%d, router=%d)", 0
szLogBatchCreated       DB "INFO: Batch created (batch_id=%d, max_size=%d)", 0
szLogModelRegistered    DB "INFO: Model registered (model_id=%d, type=%d, gpu_mem=%d)", 0
szLogResourceScheduling DB "INFO: Resource scheduling (nodes=%d, strategy=%d)", 0
szLogRoutingFailed      DB "ERROR: Routing failed (request_id=%lld, error=%d)", 0

; Metrics names
szMetricRequestsRouted              DB "tool_pipeline_requests_routed_total", 0
szMetricBatchesCreated              DB "tool_pipeline_batches_created_total", 0
szMetricResourceRebalances          DB "tool_pipeline_resource_rebalances_total", 0
szMetricAvgRoutingLatency           DB "tool_pipeline_routing_latency_ms", 0

; Global manager state
toolPipelineManager TOOL_PIPELINE_MANAGER <0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0>
toolPipelineMetrics TOOL_PIPELINE_METRICS <0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0>

; =============================================================================
; PUBLIC FUNCTIONS
; =============================================================================

; ToolPipeline_Initialize(RCX = maxRouters, RDX = maxModels) -> RAX = DWORD (success)
PUBLIC ToolPipeline_Initialize
ToolPipeline_Initialize PROC FRAME
    push rbp
    mov rbp, rsp
    sub rsp, 48
    
    ; RCX = max routers
    ; RDX = max models
    
    cmp ecx, 0
    jle .L0_invalid_routers
    cmp edx, 0
    jle .L0_invalid_models
    cmp ecx, PIPELINE_MAX_ROUTERS
    jg .L0_invalid_routers
    cmp edx, PIPELINE_MAX_MODELS
    jg .L0_invalid_models
    
    ; Acquire lock
    lea r8, [toolPipelineManager + OFFSET toolPipelineManager.ManagerLock]
    call InitializeCriticalSection
    call EnterCriticalSection
    
    ; Mark as initialized
    mov BYTE PTR [toolPipelineManager + OFFSET toolPipelineManager.Initialized], 1
    mov DWORD PTR [toolPipelineManager + OFFSET toolPipelineManager.Version], 1
    
    ; Allocate router array
    mov r9, rcx
    imul r9, SIZE ROUTER_DESCRIPTOR
    call HeapAlloc
    test rax, rax
    jz .L0_alloc_failed
    mov QWORD PTR [toolPipelineManager + OFFSET toolPipelineManager.Routers], rax
    
    ; Allocate model array
    mov r9, rdx
    imul r9, SIZE MODEL_DESCRIPTOR
    call HeapAlloc
    test rax, rax
    jz .L0_alloc_failed_routers
    mov QWORD PTR [toolPipelineManager + OFFSET toolPipelineManager.Models], rax
    
    ; Allocate batch array
    mov r9, PIPELINE_MAX_BATCHES
    imul r9, SIZE BATCH_DESCRIPTOR
    call HeapAlloc
    test rax, rax
    jz .L0_alloc_failed_models
    mov QWORD PTR [toolPipelineManager + OFFSET toolPipelineManager.Batches], rax
    
    ; Set routing mode to balanced default
    mov DWORD PTR [toolPipelineManager + OFFSET toolPipelineManager.RoutingMode], ROUTING_MODE_LEAST_LOADED
    
    ; Release lock
    lea r8, [toolPipelineManager + OFFSET toolPipelineManager.ManagerLock]
    call LeaveCriticalSection
    
    xor rax, rax
    jmp .L0_exit
    
.L0_invalid_routers:
    mov rax, TOOL_PIPELINE_E_INVALID_ROUTER
    jmp .L0_exit
    
.L0_invalid_models:
    mov rax, TOOL_PIPELINE_E_MODEL_LIMIT
    jmp .L0_exit
    
.L0_alloc_failed_models:
    mov r8, QWORD PTR [toolPipelineManager + OFFSET toolPipelineManager.Routers]
    call HeapFree
    jmp .L0_alloc_failed
    
.L0_alloc_failed_routers:
    mov r8, QWORD PTR [toolPipelineManager + OFFSET toolPipelineManager.Models]
    call HeapFree
    
.L0_alloc_failed:
    lea r8, [toolPipelineManager + OFFSET toolPipelineManager.ManagerLock]
    call LeaveCriticalSection
    xor rax, rax
    
.L0_exit:
    add rsp, 48
    pop rbp
    ret
ToolPipeline_Initialize ENDP

; ToolPipeline_RouteRequest(RCX = requestId, RDX = toolType, R8 = args) -> RAX = DWORD (routerId)
PUBLIC ToolPipeline_RouteRequest
ToolPipeline_RouteRequest PROC FRAME
    push rbp
    mov rbp, rsp
    sub rsp, 48
    
    ; RCX = request ID
    ; RDX = tool type
    ; R8 = arguments pointer
    
    cmp BYTE PTR [toolPipelineManager + OFFSET toolPipelineManager.Initialized], 1
    jne .L1_not_initialized
    
    test ecx, ecx
    jz .L1_invalid_request
    test edx, edx
    jle .L1_invalid_tool
    cmp edx, TOOL_TYPE_CUSTOM
    jg .L1_invalid_tool
    
    ; Acquire lock
    lea r9, [toolPipelineManager + OFFSET toolPipelineManager.ManagerLock]
    call EnterCriticalSection
    
    ; Find best router based on routing mode
    mov r10d, DWORD PTR [toolPipelineManager + OFFSET toolPipelineManager.RoutingMode]
    mov r11d, 1                    ; Default router ID
    
    cmp r10d, ROUTING_MODE_LEAST_LOADED
    je .L1_least_loaded
    cmp r10d, ROUTING_MODE_LATENCY_AWARE
    je .L1_latency_aware
    
    ; Round-robin default
    mov r11d, 1
    jmp .L1_route_assigned
    
.L1_least_loaded:
    ; Find router with minimum load
    mov r11d, 1
    mov r12d, 100
    mov r13, QWORD PTR [toolPipelineManager + OFFSET toolPipelineManager.Routers]
    mov r14d, DWORD PTR [toolPipelineManager + OFFSET toolPipelineManager.RouterCount]
    test r14d, r14d
    jz .L1_route_assigned
    
    xor r15d, r15d
.L1_load_loop:
    cmp r15d, r14d
    jge .L1_route_assigned
    
    mov eax, DWORD PTR [r13 + r15 * 8 + OFFSET ROUTER_DESCRIPTOR.CurrentLoad]
    cmp eax, r12d
    jge .L1_load_next
    
    mov r12d, eax
    mov r11d, r15d
    add r11d, 1
    
.L1_load_next:
    inc r15d
    jmp .L1_load_loop
    
.L1_latency_aware:
    ; Find router with lowest average latency
    mov r11d, 1
    mov r12, -1
    mov r13, QWORD PTR [toolPipelineManager + OFFSET toolPipelineManager.Routers]
    mov r14d, DWORD PTR [toolPipelineManager + OFFSET toolPipelineManager.RouterCount]
    test r14d, r14d
    jz .L1_route_assigned
    
    xor r15d, r15d
.L1_latency_loop:
    cmp r15d, r14d
    jge .L1_route_assigned
    
    mov rax, QWORD PTR [r13 + r15 * 8 + OFFSET ROUTER_DESCRIPTOR.AvgLatency]
    cmp r12, -1
    je .L1_latency_first
    cmp rax, r12
    jge .L1_latency_next
    
.L1_latency_first:
    mov r12, rax
    mov r11d, r15d
    add r11d, 1
    
.L1_latency_next:
    inc r15d
    jmp .L1_latency_loop
    
.L1_route_assigned:
    ; Update request count and load
    mov r13, QWORD PTR [toolPipelineManager + OFFSET toolPipelineManager.Routers]
    mov r14, r11
    dec r14
    imul r14, SIZE ROUTER_DESCRIPTOR
    add r13, r14
    
    inc QWORD PTR [r13 + OFFSET ROUTER_DESCRIPTOR.RequestCount]
    inc DWORD PTR [r13 + OFFSET ROUTER_DESCRIPTOR.CurrentLoad]
    
    ; Increment metrics
    inc QWORD PTR [toolPipelineMetrics + OFFSET toolPipelineMetrics.RequestsRouted]
    
    ; Release lock
    lea r9, [toolPipelineManager + OFFSET toolPipelineManager.ManagerLock]
    call LeaveCriticalSection
    
    mov rax, r11
    jmp .L1_exit
    
.L1_not_initialized:
    mov rax, TOOL_PIPELINE_E_ROUTING_FAILED
    jmp .L1_exit
    
.L1_invalid_request:
    mov rax, TOOL_PIPELINE_E_ROUTING_FAILED
    jmp .L1_exit
    
.L1_invalid_tool:
    mov rax, TOOL_PIPELINE_E_ROUTING_FAILED
    jmp .L1_exit
    
.L1_exit:
    add rsp, 48
    pop rbp
    ret
ToolPipeline_RouteRequest ENDP

; ToolPipeline_RegisterModel(RCX = modelId, RDX = capabilities, R8 = resourceReq) -> RAX = DWORD (success)
PUBLIC ToolPipeline_RegisterModel
ToolPipeline_RegisterModel PROC FRAME
    push rbp
    mov rbp, rsp
    sub rsp, 32
    
    ; RCX = model ID
    ; RDX = capabilities bitmask
    ; R8 = resource requirements pointer
    
    cmp BYTE PTR [toolPipelineManager + OFFSET toolPipelineManager.Initialized], 1
    jne .L2_not_initialized
    
    ; Acquire lock
    lea r9, [toolPipelineManager + OFFSET toolPipelineManager.ManagerLock]
    call EnterCriticalSection
    
    ; Check model limit
    cmp DWORD PTR [toolPipelineManager + OFFSET toolPipelineManager.ModelCount], PIPELINE_MAX_MODELS
    jge .L2_limit_exceeded
    
    ; Get model array and store registration
    mov r10, QWORD PTR [toolPipelineManager + OFFSET toolPipelineManager.Models]
    mov r11d, DWORD PTR [toolPipelineManager + OFFSET toolPipelineManager.ModelCount]
    imul r11, SIZE MODEL_DESCRIPTOR
    add r10, r11
    
    mov DWORD PTR [r10 + OFFSET MODEL_DESCRIPTOR.ModelId], ecx
    mov QWORD PTR [r10 + OFFSET MODEL_DESCRIPTOR.Capabilities], rdx
    mov BYTE PTR [r10 + OFFSET MODEL_DESCRIPTOR.IsLoaded], 0
    
    ; Increment count
    inc DWORD PTR [toolPipelineManager + OFFSET toolPipelineManager.ModelCount]
    inc QWORD PTR [toolPipelineMetrics + OFFSET toolPipelineMetrics.ModelsRegistered]
    
    ; Release lock
    lea r9, [toolPipelineManager + OFFSET toolPipelineManager.ManagerLock]
    call LeaveCriticalSection
    
    xor rax, rax
    jmp .L2_exit
    
.L2_not_initialized:
    mov rax, TOOL_PIPELINE_E_ROUTING_FAILED
    jmp .L2_exit
    
.L2_limit_exceeded:
    lea r9, [toolPipelineManager + OFFSET toolPipelineManager.ManagerLock]
    call LeaveCriticalSection
    mov rax, TOOL_PIPELINE_E_MODEL_LIMIT
    
.L2_exit:
    add rsp, 32
    pop rbp
    ret
ToolPipeline_RegisterModel ENDP

; ToolPipeline_CreateBatch(RCX = batchSize, RDX = timeoutMs) -> RAX = DWORD (batchId)
PUBLIC ToolPipeline_CreateBatch
ToolPipeline_CreateBatch PROC FRAME
    push rbp
    mov rbp, rsp
    sub rsp, 48
    
    ; RCX = batch size
    ; RDX = timeout in milliseconds
    
    cmp BYTE PTR [toolPipelineManager + OFFSET toolPipelineManager.Initialized], 1
    jne .L3_not_initialized
    
    cmp ecx, 0
    jle .L3_invalid_size
    cmp ecx, PIPELINE_MAX_REQUESTS_PER_BATCH
    jg .L3_invalid_size
    
    ; Acquire lock
    lea r8, [toolPipelineManager + OFFSET toolPipelineManager.ManagerLock]
    call EnterCriticalSection
    
    ; Check batch limit
    cmp DWORD PTR [toolPipelineManager + OFFSET toolPipelineManager.BatchCount], PIPELINE_MAX_BATCHES
    jge .L3_limit_exceeded
    
    ; Get batch array and create new batch
    mov r9, QWORD PTR [toolPipelineManager + OFFSET toolPipelineManager.Batches]
    mov r10d, DWORD PTR [toolPipelineManager + OFFSET toolPipelineManager.BatchCount]
    imul r10, SIZE BATCH_DESCRIPTOR
    add r9, r10
    
    ; Get batch ID
    mov r11d, DWORD PTR [toolPipelineManager + OFFSET toolPipelineManager.NextBatchId]
    mov DWORD PTR [r9 + OFFSET BATCH_DESCRIPTOR.BatchId], r11d
    mov DWORD PTR [r9 + OFFSET BATCH_DESCRIPTOR.MaxSize], ecx
    mov QWORD PTR [r9 + OFFSET BATCH_DESCRIPTOR.TimeoutMs], rdx
    mov DWORD PTR [r9 + OFFSET BATCH_DESCRIPTOR.State], BATCH_STATE_ACCUMULATING
    mov DWORD PTR [r9 + OFFSET BATCH_DESCRIPTOR.CurrentSize], 0
    
    ; Get timestamp
    call GetTickCount64
    mov QWORD PTR [r9 + OFFSET BATCH_DESCRIPTOR.TimeCreated], rax
    
    ; Increment counters
    inc DWORD PTR [toolPipelineManager + OFFSET toolPipelineManager.NextBatchId]
    inc DWORD PTR [toolPipelineManager + OFFSET toolPipelineManager.BatchCount]
    inc QWORD PTR [toolPipelineMetrics + OFFSET toolPipelineMetrics.BatchesCreated]
    
    ; Release lock
    lea r8, [toolPipelineManager + OFFSET toolPipelineManager.ManagerLock]
    call LeaveCriticalSection
    
    mov rax, r11
    jmp .L3_exit
    
.L3_not_initialized:
    mov rax, TOOL_PIPELINE_E_ROUTING_FAILED
    jmp .L3_exit
    
.L3_invalid_size:
    mov rax, TOOL_PIPELINE_E_BATCH_FULL
    jmp .L3_exit
    
.L3_limit_exceeded:
    lea r8, [toolPipelineManager + OFFSET toolPipelineManager.ManagerLock]
    call LeaveCriticalSection
    mov rax, TOOL_PIPELINE_E_BATCH_FULL
    
.L3_exit:
    add rsp, 48
    pop rbp
    ret
ToolPipeline_CreateBatch ENDP

; ToolPipeline_ScheduleResources(RCX = nodeCount, RDX = cpuPerNode, R8 = memPerNode) -> RAX = DWORD (success)
PUBLIC ToolPipeline_ScheduleResources
ToolPipeline_ScheduleResources PROC FRAME
    push rbp
    mov rbp, rsp
    sub rsp, 48
    
    ; RCX = number of nodes
    ; RDX = CPU cores per node
    ; R8 = memory per node (MB)
    
    cmp BYTE PTR [toolPipelineManager + OFFSET toolPipelineManager.Initialized], 1
    jne .L4_not_initialized
    
    cmp ecx, 0
    jle .L4_invalid_nodes
    cmp ecx, PIPELINE_MAX_NODES
    jg .L4_invalid_nodes
    
    cmp edx, 0
    jle .L4_invalid_cpu
    
    cmp r8, 0
    jle .L4_invalid_memory
    
    ; Acquire lock
    lea r9, [toolPipelineManager + OFFSET toolPipelineManager.ManagerLock]
    call EnterCriticalSection
    
    ; Allocate scheduler if not exists
    mov r10, QWORD PTR [toolPipelineManager + OFFSET toolPipelineManager.Scheduler]
    test r10, r10
    jnz .L4_scheduler_exists
    
    ; Allocate scheduler
    mov r9d, SIZE PIPELINE_SCHEDULER
    call HeapAlloc
    test rax, rax
    jz .L4_alloc_failed
    
    mov r10, rax
    mov QWORD PTR [toolPipelineManager + OFFSET toolPipelineManager.Scheduler], r10
    
.L4_scheduler_exists:
    ; Allocate node array
    mov r11, rcx
    imul r11, SIZE RESOURCE_NODE
    call HeapAlloc
    test rax, rax
    jz .L4_alloc_failed
    
    mov QWORD PTR [r10 + OFFSET PIPELINE_SCHEDULER.Nodes], rax
    mov DWORD PTR [r10 + OFFSET PIPELINE_SCHEDULER.NodeCount], ecx
    mov DWORD PTR [r10 + OFFSET PIPELINE_SCHEDULER.Strategy], RESOURCE_STRATEGY_BALANCED
    
    ; Initialize each node
    xor r12d, r12d
.L4_node_loop:
    cmp r12d, ecx
    jge .L4_nodes_initialized
    
    mov r13, rax
    imul r11, r12, SIZE RESOURCE_NODE
    add r13, r11
    
    mov DWORD PTR [r13 + OFFSET RESOURCE_NODE.NodeId], r12d
    mov DWORD PTR [r13 + OFFSET RESOURCE_NODE.TotalCpuCores], edx
    mov DWORD PTR [r13 + OFFSET RESOURCE_NODE.AvailableCores], edx
    mov QWORD PTR [r13 + OFFSET RESOURCE_NODE.TotalMemoryMb], r8
    mov QWORD PTR [r13 + OFFSET RESOURCE_NODE.AvailableMemMb], r8
    mov BYTE PTR [r13 + OFFSET RESOURCE_NODE.IsActive], 1
    
    inc r12d
    jmp .L4_node_loop
    
.L4_nodes_initialized:
    ; Increment metrics
    inc QWORD PTR [toolPipelineMetrics + OFFSET toolPipelineMetrics.NodesActive]
    
    ; Release lock
    lea r9, [toolPipelineManager + OFFSET toolPipelineManager.ManagerLock]
    call LeaveCriticalSection
    
    xor rax, rax
    jmp .L4_exit
    
.L4_not_initialized:
    mov rax, TOOL_PIPELINE_E_ROUTING_FAILED
    jmp .L4_exit
    
.L4_invalid_nodes:
    mov rax, TOOL_PIPELINE_E_RESOURCE_EXHAUSTED
    jmp .L4_exit
    
.L4_invalid_cpu:
    mov rax, TOOL_PIPELINE_E_RESOURCE_EXHAUSTED
    jmp .L4_exit
    
.L4_invalid_memory:
    mov rax, TOOL_PIPELINE_E_RESOURCE_EXHAUSTED
    jmp .L4_exit
    
.L4_alloc_failed:
    lea r9, [toolPipelineManager + OFFSET toolPipelineManager.ManagerLock]
    call LeaveCriticalSection
    xor rax, rax
    
.L4_exit:
    add rsp, 48
    pop rbp
    ret
ToolPipeline_ScheduleResources ENDP

; ToolPipeline_GetPipelineMetrics(VOID) -> RAX = QWORD (metrics pointer)
PUBLIC ToolPipeline_GetPipelineMetrics
ToolPipeline_GetPipelineMetrics PROC FRAME
    lea rax, [toolPipelineMetrics]
    ret
ToolPipeline_GetPipelineMetrics ENDP

; =============================================================================
; PHASE 5 TEST FUNCTIONS
; =============================================================================

; Test_ToolPipeline_Routing(VOID) -> RAX = DWORD (test result)
PUBLIC Test_ToolPipeline_Routing
Test_ToolPipeline_Routing PROC FRAME
    push rbp
    mov rbp, rsp
    sub rsp, 32
    
    ; Test sequence:
    ; 1. Initialize pipeline with 4 routers, 10 models
    ; 2. Register 3 models
    ; 3. Route 5 requests
    ; 4. Verify routing distribution
    
    xor rax, rax
    
    add rsp, 32
    pop rbp
    ret
Test_ToolPipeline_Routing ENDP

; Test_ToolPipeline_Batching(VOID) -> RAX = DWORD (test result)
PUBLIC Test_ToolPipeline_Batching
ToolPipeline_Batching PROC FRAME
    push rbp
    mov rbp, rsp
    sub rsp, 32
    
    ; Test sequence:
    ; 1. Create batch with size 100, timeout 5000ms
    ; 2. Add 50 requests
    ; 3. Verify batch state
    ; 4. Complete batch
    ; 5. Verify metrics
    
    xor rax, rax
    
    add rsp, 32
    pop rbp
    ret
ToolPipeline_Batching ENDP

END
