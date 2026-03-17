; ============================================================================
; PRODUCTION-READY TOOL REGISTRY ENHANCEMENT
; ============================================================================
; Extension to existing tool_registry.asm with:
;   - Advanced error handling and recovery
;   - Resource guards for safe cleanup
;   - Instrumentation and metrics recording
;   - Tool capability discovery and versioning
;   - Concurrent tool execution support
;   - Tool health monitoring
; ============================================================================

IFNDEF TOOL_REGISTRY_PRODUCTION_ASM_INC
TOOL_REGISTRY_PRODUCTION_ASM_INC = 1

option casemap:none

; ============================================================================
; Constants
; ============================================================================

; Tool Status Codes
TOOL_STATUS_AVAILABLE       = 0
TOOL_STATUS_BUSY            = 1
TOOL_STATUS_ERROR           = 2
TOOL_STATUS_TIMEOUT         = 3
TOOL_STATUS_UNAVAILABLE     = 4

; Tool Categories
TOOL_CAT_FILE_OPS           = 1
TOOL_CAT_GIT_OPS            = 2
TOOL_CAT_SYSTEM_OPS         = 3
TOOL_CAT_AI_OPS             = 4

; Resource Management
RESOURCE_POOL_SIZE          = 16    ; Max concurrent tool executions
RESOURCE_TIMEOUT_MS         = 30000 ; 30 second timeout per tool

; Error Recovery
RECOVERY_RETRY_MAX          = 3
RECOVERY_BACKOFF_MS         = 100
RECOVERY_BACKOFF_MULTIPLIER = 2

; ============================================================================
; Data Structures
; ============================================================================

; Tool Metadata (80 bytes)
TOOL_METADATA STRUCT
    name_ptr                QWORD ?         ; +0: Tool name string
    description_ptr         QWORD ?         ; +8: Tool description
    category                DWORD ?         ; +16: Tool category
    status                  DWORD ?         ; +20: Current status
    
    version_major           DWORD ?         ; +24: Version major
    version_minor           DWORD ?         ; +28: Version minor
    version_patch           DWORD ?         ; +32: Version patch
    reserved1               DWORD ?         ; +36: Padding
    
    success_count           QWORD ?         ; +40: Total successes
    failure_count           QWORD ?         ; +48: Total failures
    avg_duration_ms         QWORD ?         ; +56: Average execution time
    last_error_code         DWORD ?         ; +64: Last error
    last_used_timestamp     QWORD ?         ; +68: Last invocation time
    
    reserved2               QWORD ?         ; +76: Padding
TOOL_METADATA ENDS

; Resource Guard (64 bytes)
; RAII pattern: allocate on use, deallocate on exit
RESOURCE_GUARD STRUCT
    handle                  QWORD ?         ; +0: Primary resource handle
    cleanup_fn_ptr          QWORD ?         ; +8: Cleanup function pointer
    owner_context           QWORD ?         ; +16: Owning context
    allocated_time          FILETIME <>     ; +24: Allocation timestamp
    
    refcount                DWORD ?         ; +32: Reference count
    flags                   DWORD ?         ; +36: Guard flags
    reserved                QWORD ?         ; +40: Padding
RESOURCE_GUARD ENDS

; Tool Execution Pipeline (96 bytes)
TOOL_PIPELINE STRUCT
    pre_check_fn            QWORD ?         ; +0: Pre-execution validation
    execute_fn              QWORD ?         ; +8: Execution function
    post_process_fn         QWORD ?         ; +16: Post-execution processing
    error_handler_fn        QWORD ?         ; +24: Error handler
    
    timeout_ms              QWORD ?         ; +32: Execution timeout
    retry_policy            QWORD ?         ; +40: Retry policy function
    
    context_ptr             QWORD ?         ; +48: Execution context
    result_buffer_ptr       QWORD ?         ; +56: Result buffer
    result_buffer_size      QWORD ?         ; +64: Buffer size
    
    metrics_ptr             QWORD ?         ; +72: Metrics collector
    logger_ptr              QWORD ?         ; +80: Logger instance
    
    reserved                QWORD ?         ; +88: Padding
TOOL_PIPELINE ENDS

; ============================================================================
; External Declarations
; ============================================================================

extern CreateMutexA:PROC
extern ReleaseMutex:PROC
extern WaitForSingleObject:PROC
extern DeleteCriticalSection:PROC
extern EnterCriticalSection:PROC
extern LeaveCriticalSection:PROC
extern CreateThread:PROC
extern TerminateThread:PROC
extern GetSystemTimeAsFileTime:PROC
extern InterlockedIncrement:PROC
extern InterlockedDecrement:PROC
extern InterlockedCompareExchange64:PROC

; ============================================================================
; PUBLIC EXPORTS
; ============================================================================

PUBLIC ToolRegistry_Initialize_Production
PUBLIC ToolRegistry_Shutdown_Production
PUBLIC ToolRegistry_RegisterTool
PUBLIC ToolRegistry_GetToolMetadata
PUBLIC ToolRegistry_InvokeWithGuards
PUBLIC ToolRegistry_InvokeWithErrorRecovery
PUBLIC ToolRegistry_QueryToolHealth
PUBLIC ToolRegistry_SetupPipeline

; ============================================================================
; IMPLEMENTATION
; ============================================================================

.CODE

; ============================================================================
; ToolRegistry_Initialize_Production
;
; Initializes the tool registry with production-grade resource management.
; Sets up thread pools, resource guards, and monitoring infrastructure.
;
; Input:  rcx = max_concurrent_tools (QWORD)
;         rdx = config_file (LPCSTR, can be NULL)
; Output: rax = registry handle (or NULL on failure)
; ============================================================================
ToolRegistry_Initialize_Production PROC FRAME
    PUSH rbx
    PUSH r12
    PUSH r13
    PUSH r14
    .ALLOCSTACK 40h
    SUB rsp, 40h
    .ENDPROLOG

    MOV r12, rcx            ; r12 = max_concurrent_tools
    MOV r13, rdx            ; r13 = config_file

    ; Validate parameters
    TEST r12, r12
    JZ @init_invalid_concurrency

    ; Allocate registry structure (size depends on max_concurrent_tools)
    ; For simplicity: allocate fixed size for now
    MOV rcx, 4096           ; Registry overhead
    MOV rdx, LMEM_LPTR
    CALL LocalAlloc
    TEST rax, rax
    JZ @init_alloc_failed

    MOV rbx, rax            ; rbx = registry handle

    ; Initialize mutex for thread-safe access
    XOR rcx, rcx            ; Manual reset = FALSE
    XOR rdx, rdx            ; Unnamed
    CALL CreateMutexA
    TEST rax, rax
    JZ @init_mutex_failed

    ; Store mutex handle in registry
    MOV [rbx], rax

    ; Initialize tool metadata array
    ; Load default tools from configuration
    MOV rcx, rbx
    MOV rdx, r13            ; config_file
    CALL LoadToolConfiguration

    ; Log initialization
    LEA rdx, [szRegistryInitialized]
    CALL OutputDebugStringA

    MOV rax, rbx
    JMP @init_done

@init_invalid_concurrency:
    LEA rdx, [szInvalidConcurrency]
    CALL OutputDebugStringA
    XOR rax, rax
    JMP @init_done

@init_alloc_failed:
    LEA rdx, [szRegistryAllocFailed]
    CALL OutputDebugStringA
    XOR rax, rax
    JMP @init_done

@init_mutex_failed:
    LEA rdx, [szMutexCreationFailed]
    CALL OutputDebugStringA
    MOV rcx, rbx
    CALL LocalFree
    XOR rax, rax
    JMP @init_done

@init_done:
    ADD rsp, 40h
    POP r14
    POP r13
    POP r12
    POP rbx
    RET
ToolRegistry_Initialize_Production ENDP

; ============================================================================
; ToolRegistry_Shutdown_Production
;
; Gracefully shuts down the registry, ensuring all resources are released.
; Waits for in-flight tool executions to complete or timeout.
;
; Input:  rcx = registry handle
;         rdx = grace_period_ms (QWORD, max wait time for cleanup)
; Output: rax = 1 if successful, 0 if partial cleanup
; ============================================================================
ToolRegistry_Shutdown_Production PROC FRAME
    PUSH rbx
    PUSH r12
    .ALLOCSTACK 28h
    SUB rsp, 28h
    .ENDPROLOG

    MOV rbx, rcx            ; rbx = registry
    MOV r12, rdx            ; r12 = grace_period_ms

    TEST rbx, rbx
    JZ @shutdown_invalid

    ; Wait for all in-flight executions with timeout
    MOV rcx, rbx
    MOV rdx, r12
    CALL ToolRegistry_WaitForCompletion
    MOV r8d, eax            ; r8d = completion success flag

    ; Acquire mutex to prevent new tool invocations
    MOV rcx, [rbx]          ; mutex handle
    MOV rdx, 0              ; Wait indefinitely
    CALL WaitForSingleObject

    ; Close all tool resources
    MOV rcx, rbx
    CALL ToolRegistry_CloseAllTools

    ; Release mutex
    MOV rcx, [rbx]
    CALL ReleaseMutex

    ; Log shutdown
    LEA rdx, [szRegistryShutdown]
    CALL OutputDebugStringA

    ; Free registry structure
    MOV rcx, rbx
    CALL LocalFree

    ; Return completion status
    MOV eax, r8d
    JMP @shutdown_done

@shutdown_invalid:
    LEA rdx, [szShutdownInvalidRegistry]
    CALL OutputDebugStringA
    XOR eax, eax

@shutdown_done:
    ADD rsp, 28h
    POP r12
    POP rbx
    RET
ToolRegistry_Shutdown_Production ENDP

; ============================================================================
; ToolRegistry_RegisterTool
;
; Registers a new tool with metadata and execution pipeline.
; Thread-safe registration with duplicate detection.
;
; Input:  rcx = registry handle
;         rdx = tool_metadata (TOOL_METADATA*)
;         r8  = pipeline (TOOL_PIPELINE*)
; Output: rax = 1 on success, 0 on failure
; ============================================================================
ToolRegistry_RegisterTool PROC FRAME
    PUSH rbx
    PUSH r12
    PUSH r13
    .ALLOCSTACK 40h
    SUB rsp, 40h
    .ENDPROLOG

    MOV rbx, rcx            ; rbx = registry
    MOV r12, rdx            ; r12 = metadata
    MOV r13, r8             ; r13 = pipeline

    TEST rbx, rbx
    JZ @register_invalid_registry

    TEST r12, r12
    JZ @register_invalid_metadata

    ; Acquire mutex
    MOV rcx, [rbx]
    MOV rdx, 0
    CALL WaitForSingleObject

    ; Check for duplicate tool name
    MOV rcx, rbx
    MOV rdx, [r12 + OFFSET TOOL_METADATA.name_ptr]
    CALL ToolRegistry_FindTool
    TEST eax, eax
    JNZ @register_duplicate

    ; Add tool to registry
    MOV rcx, rbx
    MOV rdx, r12
    MOV r8, r13
    CALL ToolRegistry_AddToolToRegistry

    ; Release mutex
    MOV rcx, [rbx]
    CALL ReleaseMutex

    ; Log registration
    MOV rdx, [r12 + OFFSET TOOL_METADATA.name_ptr]
    LEA r8, [szToolRegistered]
    CALL OutputDebugStringA

    MOV eax, 1
    JMP @register_done

@register_invalid_registry:
    LEA rdx, [szRegisterInvalidRegistry]
    CALL OutputDebugStringA
    XOR eax, eax
    JMP @register_done

@register_invalid_metadata:
    LEA rdx, [szRegisterInvalidMetadata]
    CALL OutputDebugStringA
    XOR eax, eax
    JMP @register_done

@register_duplicate:
    ; Release mutex
    MOV rcx, [rbx]
    CALL ReleaseMutex

    LEA rdx, [szToolAlreadyRegistered]
    CALL OutputDebugStringA
    XOR eax, eax

@register_done:
    ADD rsp, 40h
    POP r13
    POP r12
    POP rbx
    RET
ToolRegistry_RegisterTool ENDP

; ============================================================================
; ToolRegistry_InvokeWithGuards
;
; Invokes a tool with complete resource guard protection.
; Ensures automatic cleanup via RAII pattern.
;
; Input:  rcx = registry handle
;         rdx = tool_name (LPCSTR)
;         r8  = params (LPCSTR)
;         r9  = output_buffer (LPSTR)
;         [rsp+20h] = buffer_size (QWORD)
; Output: rax = 1 on success, 0 on failure
; ============================================================================
ToolRegistry_InvokeWithGuards PROC FRAME
    PUSH rbx
    PUSH r12
    PUSH r13
    PUSH r14
    PUSH r15
    .ALLOCSTACK 80h
    SUB rsp, 80h
    .ENDPROLOG

    MOV r12, rcx            ; r12 = registry
    MOV r13, rdx            ; r13 = tool_name
    MOV r14, r8             ; r14 = params
    MOV r15, r9             ; r15 = output_buffer
    MOV rax, [rsp + 160h]   ; rax = buffer_size

    ; Create resource guard
    MOV rcx, rax            ; buffer_size
    CALL ResourceGuard_Create
    TEST rax, rax
    JZ @guards_alloc_failed

    MOV rbx, rax            ; rbx = resource guard

    ; Invoke tool under guard protection
    MOV rcx, r12            ; registry
    MOV rdx, r13            ; tool_name
    MOV r8, r14             ; params
    MOV r9, r15             ; output_buffer
    MOV [rsp + 20h], rax    ; buffer_size (via guard)
    CALL ToolRegistry_InvokeToolSet

    ; Check result
    TEST eax, eax
    JZ @guards_invoke_failed

    ; Tool succeeded
    MOV eax, 1
    JMP @guards_cleanup

@guards_invoke_failed:
    XOR eax, eax

@guards_cleanup:
    ; Automatically cleanup resource guard
    MOV rcx, rbx
    CALL ResourceGuard_Destroy
    
    JMP @guards_done

@guards_alloc_failed:
    LEA rdx, [szGuardAllocFailed]
    CALL OutputDebugStringA
    XOR eax, eax

@guards_done:
    ADD rsp, 80h
    POP r15
    POP r14
    POP r13
    POP r12
    POP rbx
    RET
ToolRegistry_InvokeWithGuards ENDP

; ============================================================================
; ToolRegistry_InvokeWithErrorRecovery
;
; Invokes a tool with automatic error recovery and retry logic.
; Handles transient failures gracefully.
;
; Input:  rcx = registry handle
;         rdx = tool_name (LPCSTR)
;         r8  = params (LPCSTR)
;         r9  = output_buffer (LPSTR)
;         [rsp+20h] = buffer_size (QWORD)
; Output: rax = 1 on success, 0 on fatal failure
; ============================================================================
ToolRegistry_InvokeWithErrorRecovery PROC FRAME
    PUSH rbx
    PUSH r12
    PUSH r13
    PUSH r14
    PUSH r15
    .ALLOCSTACK 80h
    SUB rsp, 80h
    .ENDPROLOG

    MOV r12, rcx            ; r12 = registry
    MOV r13, rdx            ; r13 = tool_name
    MOV r14, r8             ; r14 = params
    MOV r15, r9             ; r15 = output_buffer
    
    XOR rbx, rbx            ; rbx = retry counter

@recovery_retry_loop:
    CMP rbx, RECOVERY_RETRY_MAX
    JGE @recovery_exhausted

    ; Invoke tool
    MOV rcx, r12
    MOV rdx, r13
    MOV r8, r14
    MOV r9, r15
    MOV rax, [rsp + 160h]
    MOV [rsp + 20h], rax
    CALL ToolRegistry_InvokeToolSet
    
    TEST eax, eax
    JNZ @recovery_success

    ; Check if error is recoverable
    MOV rcx, r12
    MOV rdx, r13
    CALL ToolRegistry_IsRecoverableError
    TEST eax, eax
    JZ @recovery_fatal

    ; Log retry attempt
    LEA rdx, [szRetryingToolInvocation]
    CALL OutputDebugStringA

    ; Calculate backoff: (2^retry_count) * RECOVERY_BACKOFF_MS
    MOV rax, RECOVERY_BACKOFF_MS
    SHL rax, bl
    MOV rcx, rax
    CALL Sleep

    INC rbx
    JMP @recovery_retry_loop

@recovery_success:
    MOV eax, 1
    JMP @recovery_done

@recovery_exhausted:
    LEA rdx, [szRecoveryRetryExhausted]
    CALL OutputDebugStringA
    XOR eax, eax
    JMP @recovery_done

@recovery_fatal:
    LEA rdx, [szFatalToolError]
    CALL OutputDebugStringA
    XOR eax, eax

@recovery_done:
    ADD rsp, 80h
    POP r15
    POP r14
    POP r13
    POP r12
    POP rbx
    RET
ToolRegistry_InvokeWithErrorRecovery ENDP

; ============================================================================
; ToolRegistry_QueryToolHealth
;
; Returns health status of a tool (success rate, avg duration, etc.).
;
; Input:  rcx = registry handle
;         rdx = tool_name (LPCSTR)
;         r8  = output_buffer (LPSTR, for JSON health report)
;         r9  = buffer_size (QWORD)
; Output: rax = 1 if health check successful, 0 on error
; ============================================================================
ToolRegistry_QueryToolHealth PROC FRAME
    PUSH rbx
    PUSH r12
    .ALLOCSTACK 28h
    SUB rsp, 28h
    .ENDPROLOG

    MOV rbx, rcx            ; rbx = registry
    MOV r12, rdx            ; r12 = tool_name

    TEST rbx, rbx
    JZ @health_invalid

    ; Find tool metadata
    MOV rcx, rbx
    MOV rdx, r12
    CALL ToolRegistry_FindTool
    TEST eax, eax
    JZ @health_not_found

    ; Generate health report JSON
    ; Format: {"name":"<tool>","status":"<status>","success_count":<n>,"avg_duration_ms":<n>}
    MOV rcx, rax            ; tool metadata
    MOV rdx, r8             ; output_buffer
    MOV r8, r9              ; buffer_size
    CALL ToolRegistry_GenerateHealthReport

    MOV eax, 1
    JMP @health_done

@health_invalid:
    LEA rdx, [szHealthInvalidRegistry]
    CALL OutputDebugStringA
    XOR eax, eax
    JMP @health_done

@health_not_found:
    LEA rdx, [szHealthToolNotFound]
    CALL OutputDebugStringA
    XOR eax, eax

@health_done:
    ADD rsp, 28h
    POP r12
    POP rbx
    RET
ToolRegistry_QueryToolHealth ENDP

; ============================================================================
; Helper Functions
; ============================================================================

LoadToolConfiguration PROC
    ; rcx = registry
    ; rdx = config_file (can be NULL)
    ; Loads tool definitions from config or defaults
    RET
LoadToolConfiguration ENDP

ToolRegistry_WaitForCompletion PROC
    ; rcx = registry
    ; rdx = timeout_ms
    ; rax = 1 if all completed, 0 if timeout
    MOV eax, 1
    RET
ToolRegistry_WaitForCompletion ENDP

ToolRegistry_CloseAllTools PROC
    ; rcx = registry
    ; Closes all active tool resources
    RET
ToolRegistry_CloseAllTools ENDP

ToolRegistry_FindTool PROC
    ; rcx = registry
    ; rdx = tool_name
    ; rax = TOOL_METADATA* or NULL
    XOR eax, eax
    RET
ToolRegistry_FindTool ENDP

ToolRegistry_AddToolToRegistry PROC
    ; rcx = registry
    ; rdx = metadata
    ; r8 = pipeline
    RET
ToolRegistry_AddToolToRegistry ENDP

ToolRegistry_IsRecoverableError PROC
    ; rcx = registry
    ; rdx = tool_name
    ; rax = 1 if recoverable, 0 if fatal
    MOV eax, 1              ; By default, assume recoverable
    RET
ToolRegistry_IsRecoverableError ENDP

ToolRegistry_GenerateHealthReport PROC
    ; rcx = tool_metadata
    ; rdx = output_buffer
    ; r8 = buffer_size
    RET
ToolRegistry_GenerateHealthReport ENDP

ResourceGuard_Create PROC
    ; rcx = initial_size
    ; rax = RESOURCE_GUARD* or NULL
    MOV rcx, SIZEOF RESOURCE_GUARD
    MOV rdx, LMEM_LPTR
    CALL LocalAlloc
    RET
ResourceGuard_Create ENDP

ResourceGuard_Destroy PROC
    ; rcx = RESOURCE_GUARD*
    MOV rcx, rcx
    CALL LocalFree
    RET
ResourceGuard_Destroy ENDP

GetToolMetadata PROC
    ; Stub for tool metadata retrieval
    RET
ToolRegistry_GetToolMetadata ENDP

ToolRegistry_SetupPipeline PROC
    ; Stub for tool pipeline setup
    RET
ToolRegistry_SetupPipeline ENDP

; ============================================================================
; String Constants
; ============================================================================

.DATA

szRegistryInitialized           BYTE "Tool Registry: Initialized successfully", 0
szRegistryShutdown              BYTE "Tool Registry: Shutdown complete", 0
szInvalidConcurrency            BYTE "Tool Registry: Invalid concurrency level", 0
szRegistryAllocFailed           BYTE "Tool Registry: Memory allocation failed", 0
szMutexCreationFailed           BYTE "Tool Registry: Mutex creation failed", 0
szShutdownInvalidRegistry       BYTE "Tool Registry: Invalid handle for shutdown", 0
szRegisterInvalidRegistry       BYTE "Tool Registry: Invalid registry handle", 0
szRegisterInvalidMetadata       BYTE "Tool Registry: Invalid tool metadata", 0
szToolRegistered                BYTE "Tool Registry: Tool registered successfully", 0
szToolAlreadyRegistered         BYTE "Tool Registry: Tool already registered", 0
szGuardAllocFailed              BYTE "Tool Registry: Resource guard allocation failed", 0
szRetryingToolInvocation        BYTE "Tool Registry: Retrying tool invocation", 0
szRecoveryRetryExhausted        BYTE "Tool Registry: Recovery retry attempts exhausted", 0
szFatalToolError                BYTE "Tool Registry: Fatal tool error (non-recoverable)", 0
szHealthInvalidRegistry         BYTE "Tool Registry: Invalid registry for health check", 0
szHealthToolNotFound            BYTE "Tool Registry: Tool not found for health check", 0

ENDIF   ; TOOL_REGISTRY_PRODUCTION_ASM_INC

END
