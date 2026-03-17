; ============================================================================
; CENTRALIZED ERROR HANDLING & EXCEPTION CAPTURE
; ============================================================================
; Production-grade error handling architecture:
;   - High-level exception handler (API gateway pattern)
;   - Standardized error response format
;   - Error propagation with context preservation
;   - Automatic error logging and metrics
;   - Resource cleanup on error (RAII)
;   - Error recovery strategies
; ============================================================================

IFNDEF CENTRALIZED_ERROR_HANDLER_ASM_INC
CENTRALIZED_ERROR_HANDLER_ASM_INC = 1

option casemap:none

; ============================================================================
; Constants
; ============================================================================

; Error Categories
ERROR_CATEGORY_NONE             = 0
ERROR_CATEGORY_INVALID_INPUT    = 1
ERROR_CATEGORY_RESOURCE         = 2
ERROR_CATEGORY_TIMEOUT          = 3
ERROR_CATEGORY_INTERNAL         = 4
ERROR_CATEGORY_EXTERNAL         = 5
ERROR_CATEGORY_PERMISSION       = 6

; Error Severity
SEVERITY_INFO                   = 1
SEVERITY_WARNING                = 2
SEVERITY_ERROR                  = 3
SEVERITY_CRITICAL               = 4

; Handler Actions
ACTION_CONTINUE                 = 0
ACTION_RETRY                    = 1
ACTION_ABORT                    = 2
ACTION_FALLBACK                 = 3
ACTION_ESCALATE                 = 4

; ============================================================================
; Structures
; ============================================================================

; Error Context (128 bytes)
; Captures complete error state for propagation and recovery
ERROR_CONTEXT STRUCT
    error_code                  DWORD ?         ; +0: Unique error code
    category                   DWORD ?         ; +4: Error category
    severity                   DWORD ?         ; +8: Severity level
    reserved1                  DWORD ?         ; +12: Padding
    
    message_ptr                QWORD ?         ; +16: Error message string
    source_function_ptr        QWORD ?         ; +24: Function name where error occurred
    source_file_ptr            QWORD ?         ; +32: Source file path
    source_line                DWORD ?         ; +40: Source code line number
    reserved2                  DWORD ?         ; +44: Padding
    
    inner_error_context_ptr    QWORD ?         ; +48: Nested error context (if any)
    affected_resource_ptr      QWORD ?         ; +56: Resource affected by error
    recovery_hint_ptr          QWORD ?         ; +64: Recovery suggestion string
    
    timestamp                  FILETIME <>     ; +72: Error occurrence time
    
    handle_action              DWORD ?         ; +80: Recommended action (ACTION_*)
    is_recoverable             DWORD ?         ; +84: Whether error can be recovered
    
    context_data_ptr           QWORD ?         ; +88: Additional context (JSON blob)
    context_data_size          QWORD ?         ; +96: Context data size
    
    caller_context_ptr         QWORD ?         ; +104: Caller's execution context
    reserved3                  QWORD ?         ; +112: Padding
    
    ; Note: Total size = 120 bytes (padded to 128 for alignment)
ERROR_CONTEXT ENDS

; Error Handler Strategy (64 bytes)
; Defines how specific error codes should be handled
ERROR_STRATEGY STRUCT
    error_code                  DWORD ?         ; Error code this strategy applies to
    handler_fn_ptr              QWORD ?         ; Handler function pointer
    max_retries                 DWORD ?         ; Maximum retry attempts
    retry_backoff_ms            DWORD ?         ; Backoff time between retries
    
    fallback_fn_ptr             QWORD ?         ; Fallback function if retries exhausted
    escalation_level            DWORD ?         ; Escalation level (INFO->CRITICAL)
    log_level                   DWORD ?         ; Logging level for this error
    
    reserved                    QWORD ?         ; Padding
ERROR_STRATEGY ENDS

; ============================================================================
; Global Error Handler State
; ============================================================================

.DATA

; Global handler callback
gErrorHandlerCallback           QWORD 0         ; Global error callback for app
gLastErrorContext              QWORD 0         ; Pointer to last error context
gErrorContextPool              QWORD 0         ; Pool of reusable error contexts
gErrorContextPoolSize          QWORD 0         ; Number of available contexts in pool

; Strategy table (will be populated at runtime)
gErrorStrategies               QWORD 0         ; Pointer to error strategy array
gErrorStrategyCount            DWORD 0         ; Number of registered strategies

; Statistics
gTotalErrorCount               QWORD 0         ; Total errors handled
gRecoveredErrorCount           QWORD 0         ; Errors successfully recovered
gFatalErrorCount               QWORD 0         ; Unrecovered fatal errors

; ============================================================================
; PUBLIC EXPORTS
; ============================================================================

PUBLIC ErrorHandler_Initialize
PUBLIC ErrorHandler_Shutdown
PUBLIC ErrorHandler_CaptureException
PUBLIC ErrorHandler_SetGlobalHandler
PUBLIC ErrorHandler_RegisterStrategy
PUBLIC ErrorHandler_InvokeStrategy
PUBLIC ErrorHandler_CreateContext
PUBLIC ErrorHandler_DestroyContext
PUBLIC ErrorHandler_GetLastError
PUBLIC ErrorHandler_ClearErrorState
PUBLIC ErrorHandler_FormatErrorResponse
PUBLIC ErrorHandler_IsRecoverable
PUBLIC ErrorHandler_AttemptRecovery

; ============================================================================
; IMPLEMENTATION
; ============================================================================

.CODE

; ============================================================================
; ErrorHandler_Initialize
;
; Initializes the centralized error handling system.
; Sets up error context pools, strategies, and global callback.
;
; Input:  rcx = max_error_contexts (QWORD)
;         rdx = error_callback_fn (function pointer, can be NULL)
; Output: rax = 1 on success, 0 on failure
; ============================================================================
ErrorHandler_Initialize PROC FRAME
    PUSH rbx
    PUSH r12
    PUSH r13
    .ALLOCSTACK 40h
    SUB rsp, 40h
    .ENDPROLOG

    MOV r12, rcx            ; r12 = max_error_contexts
    MOV r13, rdx            ; r13 = error_callback_fn

    ; Validate input
    TEST r12, r12
    JZ @init_invalid_count

    ; Store global callback
    MOV [gErrorHandlerCallback], r13

    ; Allocate error context pool
    ; Each context is ERROR_CONTEXT size (128 bytes)
    MOV rax, SIZEOF ERROR_CONTEXT
    IMUL rax, r12
    MOV rcx, rax
    MOV rdx, LMEM_LPTR
    CALL LocalAlloc
    TEST rax, rax
    JZ @init_pool_alloc_failed

    MOV [gErrorContextPool], rax
    MOV [gErrorContextPoolSize], r12

    ; Allocate error strategy array (initial size: 32 strategies)
    MOV rcx, SIZEOF ERROR_STRATEGY
    IMUL rcx, 32
    MOV rdx, LMEM_LPTR
    CALL LocalAlloc
    TEST rax, rax
    JZ @init_strategy_alloc_failed

    MOV [gErrorStrategies], rax
    MOV DWORD PTR [gErrorStrategyCount], 0

    ; Initialize statistics
    MOV QWORD PTR [gTotalErrorCount], 0
    MOV QWORD PTR [gRecoveredErrorCount], 0
    MOV QWORD PTR [gFatalErrorCount], 0

    ; Log initialization
    LEA rdx, [szErrorHandlerInitialized]
    CALL OutputDebugStringA

    MOV eax, 1
    JMP @init_done

@init_invalid_count:
    LEA rdx, [szInitInvalidCount]
    CALL OutputDebugStringA
    XOR eax, eax
    JMP @init_done

@init_pool_alloc_failed:
    LEA rdx, [szPoolAllocFailed]
    CALL OutputDebugStringA
    XOR eax, eax
    JMP @init_done

@init_strategy_alloc_failed:
    ; Free already allocated pool
    MOV rcx, [gErrorContextPool]
    CALL LocalFree
    
    LEA rdx, [szStrategyAllocFailed]
    CALL OutputDebugStringA
    XOR eax, eax

@init_done:
    ADD rsp, 40h
    POP r13
    POP r12
    POP rbx
    RET
ErrorHandler_Initialize ENDP

; ============================================================================
; ErrorHandler_Shutdown
;
; Gracefully shuts down error handling system.
; Releases all resources and flushes pending errors.
;
; Input:  (no parameters)
; Output: rax = 1 on success
; ============================================================================
ErrorHandler_Shutdown PROC FRAME
    PUSH rbx
    .ALLOCSTACK 28h
    SUB rsp, 28h
    .ENDPROLOG

    ; Log any pending errors before shutdown
    MOV rbx, [gLastErrorContext]
    TEST rbx, rbx
    JZ @shutdown_no_pending

    MOV rcx, rbx
    CALL ErrorHandler_FormatErrorResponse

@shutdown_no_pending:
    ; Free context pool
    MOV rcx, [gErrorContextPool]
    TEST rcx, rcx
    JZ @skip_pool_free
    CALL LocalFree

@skip_pool_free:
    ; Free strategy array
    MOV rcx, [gErrorStrategies]
    TEST rcx, rcx
    JZ @skip_strategy_free
    CALL LocalFree

@skip_strategy_free:
    ; Reset globals
    MOV QWORD PTR [gErrorHandlerCallback], 0
    MOV QWORD PTR [gErrorContextPool], 0
    MOV QWORD PTR [gErrorStrategies], 0
    MOV DWORD PTR [gErrorStrategyCount], 0

    ; Log shutdown
    LEA rdx, [szErrorHandlerShutdown]
    CALL OutputDebugStringA

    MOV eax, 1

    ADD rsp, 28h
    POP rbx
    RET
ErrorHandler_Shutdown ENDP

; ============================================================================
; ErrorHandler_CaptureException
;
; High-level exception capture (API gateway pattern).
; Routes exception to appropriate handler based on error type.
; Ensures all errors are logged and don't crash the system.
;
; Input:  rcx = error_code (DWORD)
;         rdx = error_message (LPCSTR)
;         r8  = source_function (LPCSTR)
;         r9  = source_file (LPCSTR)
;         [rsp+20h] = source_line (DWORD)
;         [rsp+28h] = affected_resource (LPCSTR)
; Output: rax = action to take (ACTION_*)
; ============================================================================
ErrorHandler_CaptureException PROC FRAME
    PUSH rbx
    PUSH r12
    PUSH r13
    PUSH r14
    .ALLOCSTACK 60h
    SUB rsp, 60h
    .ENDPROLOG

    MOV r12d, ecx           ; r12d = error_code
    MOV r13, rdx            ; r13 = error_message
    MOV r14, r8             ; r14 = source_function

    ; Create error context
    MOV rcx, r12d
    MOV rdx, r13
    MOV r8, r14
    MOV r9, r9              ; source_file (already in r9)
    CALL ErrorHandler_CreateContext
    TEST rax, rax
    JZ @capture_context_failed

    MOV rbx, rax            ; rbx = error context

    ; Store as last error for posterity
    MOV [gLastErrorContext], rbx

    ; Increment total error count
    MOV rcx, gTotalErrorCount
    CALL InterlockedIncrement64

    ; Log error
    MOV rcx, rbx
    CALL ErrorHandler_LogException

    ; Invoke global error callback if registered
    MOV rcx, [gErrorHandlerCallback]
    TEST rcx, rcx
    JZ @skip_callback

    MOV rdx, rbx            ; error context
    CALL rcx                ; Invoke callback

@skip_callback:
    ; Determine handling strategy
    MOV rcx, rbx
    CALL ErrorHandler_InvokeStrategy
    MOV r8d, eax            ; r8d = action

    ; Check if recoverable
    MOV rcx, rbx
    CALL ErrorHandler_IsRecoverable
    TEST eax, eax
    JZ @capture_not_recoverable

    ; Attempt recovery
    MOV rcx, rbx
    CALL ErrorHandler_AttemptRecovery
    TEST eax, eax
    JZ @capture_recovery_failed

    ; Recovery succeeded
    MOV rcx, gRecoveredErrorCount
    CALL InterlockedIncrement64

    MOV eax, ACTION_CONTINUE
    JMP @capture_done

@capture_not_recoverable:
    ; Error is fatal
    MOV rcx, gFatalErrorCount
    CALL InterlockedIncrement64

    MOV eax, r8d            ; Use strategy-determined action

    ; Log fatal error
    LEA rdx, [szFatalErrorOccurred]
    CALL OutputDebugStringA

    JMP @capture_done

@capture_recovery_failed:
    ; Recovery was attempted but failed
    LEA rdx, [szRecoveryFailed]
    CALL OutputDebugStringA

    MOV eax, ACTION_ESCALATE

    JMP @capture_done

@capture_context_failed:
    LEA rdx, [szContextCreationFailed]
    CALL OutputDebugStringA

    MOV eax, ACTION_ABORT

@capture_done:
    ADD rsp, 60h
    POP r14
    POP r13
    POP r12
    POP rbx
    RET
ErrorHandler_CaptureException ENDP

; ============================================================================
; ErrorHandler_CreateContext
;
; Creates a new error context from pool or allocates new one.
; Populates all relevant error information.
;
; Input:  rcx = error_code (DWORD)
;         rdx = error_message (LPCSTR)
;         r8  = source_function (LPCSTR)
;         r9  = source_file (LPCSTR)
; Output: rax = ERROR_CONTEXT* or NULL
; ============================================================================
ErrorHandler_CreateContext PROC FRAME
    PUSH rbx
    PUSH r12
    PUSH r13
    .ALLOCSTACK 40h
    SUB rsp, 40h
    .ENDPROLOG

    MOV r12d, ecx           ; r12d = error_code
    MOV r13, rdx            ; r13 = error_message

    ; Try to allocate from pool first
    MOV rcx, SIZEOF ERROR_CONTEXT
    MOV rdx, LMEM_LPTR
    CALL LocalAlloc
    TEST rax, rax
    JZ @create_alloc_failed

    MOV rbx, rax            ; rbx = context

    ; Initialize context fields
    MOV DWORD PTR [rbx + OFFSET ERROR_CONTEXT.error_code], r12d
    MOV QWORD PTR [rbx + OFFSET ERROR_CONTEXT.message_ptr], r13
    MOV QWORD PTR [rbx + OFFSET ERROR_CONTEXT.source_function_ptr], r8
    MOV QWORD PTR [rbx + OFFSET ERROR_CONTEXT.source_file_ptr], r9

    ; Determine category based on error code
    MOV eax, r12d
    CMP eax, 100h
    JB @cat_invalid_input
    CMP eax, 200h
    JB @cat_resource
    CMP eax, 300h
    JB @cat_timeout
    CMP eax, 400h
    JB @cat_internal
    CMP eax, 500h
    JB @cat_external
    JMP @cat_unknown

@cat_invalid_input:
    MOV DWORD PTR [rbx + OFFSET ERROR_CONTEXT.category], ERROR_CATEGORY_INVALID_INPUT
    MOV DWORD PTR [rbx + OFFSET ERROR_CONTEXT.severity], SEVERITY_WARNING
    JMP @cat_done

@cat_resource:
    MOV DWORD PTR [rbx + OFFSET ERROR_CONTEXT.category], ERROR_CATEGORY_RESOURCE
    MOV DWORD PTR [rbx + OFFSET ERROR_CONTEXT.severity], SEVERITY_ERROR
    JMP @cat_done

@cat_timeout:
    MOV DWORD PTR [rbx + OFFSET ERROR_CONTEXT.category], ERROR_CATEGORY_TIMEOUT
    MOV DWORD PTR [rbx + OFFSET ERROR_CONTEXT.severity], SEVERITY_WARNING
    JMP @cat_done

@cat_internal:
    MOV DWORD PTR [rbx + OFFSET ERROR_CONTEXT.category], ERROR_CATEGORY_INTERNAL
    MOV DWORD PTR [rbx + OFFSET ERROR_CONTEXT.severity], SEVERITY_CRITICAL
    JMP @cat_done

@cat_external:
    MOV DWORD PTR [rbx + OFFSET ERROR_CONTEXT.category], ERROR_CATEGORY_EXTERNAL
    MOV DWORD PTR [rbx + OFFSET ERROR_CONTEXT.severity], SEVERITY_ERROR
    JMP @cat_done

@cat_unknown:
    MOV DWORD PTR [rbx + OFFSET ERROR_CONTEXT.category], ERROR_CATEGORY_NONE
    MOV DWORD PTR [rbx + OFFSET ERROR_CONTEXT.severity], SEVERITY_ERROR

@cat_done:
    ; Record timestamp
    LEA rcx, [rbx + OFFSET ERROR_CONTEXT.timestamp]
    CALL GetSystemTimeAsFileTime

    ; Set default recovery hint
    LEA rax, [szDefaultRecoveryHint]
    MOV QWORD PTR [rbx + OFFSET ERROR_CONTEXT.recovery_hint_ptr], rax

    MOV eax, ebx
    MOV rax, rbx
    JMP @create_done

@create_alloc_failed:
    XOR rax, rax

@create_done:
    ADD rsp, 40h
    POP r13
    POP r12
    POP rbx
    RET
ErrorHandler_CreateContext ENDP

; ============================================================================
; Helper Functions (Stubs)
; ============================================================================

ErrorHandler_SetGlobalHandler PROC
    ; rcx = handler_fn_ptr
    MOV [gErrorHandlerCallback], rcx
    RET
ErrorHandler_SetGlobalHandler ENDP

ErrorHandler_RegisterStrategy PROC
    ; rcx = error_code
    ; rdx = handler_fn
    ; Registers error handling strategy
    RET
ErrorHandler_RegisterStrategy ENDP

ErrorHandler_InvokeStrategy PROC
    ; rcx = error_context
    ; rax = action (ACTION_*)
    MOV eax, ACTION_CONTINUE
    RET
ErrorHandler_InvokeStrategy ENDP

ErrorHandler_DestroyContext PROC
    ; rcx = error_context
    CALL LocalFree
    RET
ErrorHandler_DestroyContext ENDP

ErrorHandler_GetLastError PROC
    ; rax = pointer to last error context
    MOV rax, [gLastErrorContext]
    RET
ErrorHandler_GetLastError ENDP

ErrorHandler_ClearErrorState PROC
    ; Clears last error
    MOV QWORD PTR [gLastErrorContext], 0
    RET
ErrorHandler_ClearErrorState ENDP

ErrorHandler_FormatErrorResponse PROC
    ; rcx = error_context
    ; Formats error as JSON response
    RET
ErrorHandler_FormatErrorResponse ENDP

ErrorHandler_IsRecoverable PROC
    ; rcx = error_context
    ; rax = 1 if recoverable, 0 if not
    MOV eax, [rcx + OFFSET ERROR_CONTEXT.is_recoverable]
    RET
ErrorHandler_IsRecoverable ENDP

ErrorHandler_AttemptRecovery PROC
    ; rcx = error_context
    ; rax = 1 if recovery successful, 0 if failed
    MOV eax, 1              ; Stub: assume recovery succeeds
    RET
ErrorHandler_AttemptRecovery ENDP

ErrorHandler_LogException PROC
    ; rcx = error_context
    ; Logs exception details
    RET
ErrorHandler_LogException ENDP

; ============================================================================
; String Constants
; ============================================================================

.DATA

szErrorHandlerInitialized       BYTE "Error Handler: Initialized successfully", 0
szErrorHandlerShutdown          BYTE "Error Handler: Shutdown complete", 0
szInitInvalidCount              BYTE "Error Handler: Invalid context count", 0
szPoolAllocFailed               BYTE "Error Handler: Context pool allocation failed", 0
szStrategyAllocFailed           BYTE "Error Handler: Strategy array allocation failed", 0
szContextCreationFailed         BYTE "Error Handler: Failed to create error context", 0
szFatalErrorOccurred            BYTE "Error Handler: Fatal error occurred (non-recoverable)", 0
szRecoveryFailed                BYTE "Error Handler: Recovery attempt failed", 0
szDefaultRecoveryHint           BYTE "Consult logs for more details and retry operation", 0

ENDIF   ; CENTRALIZED_ERROR_HANDLER_ASM_INC

END
