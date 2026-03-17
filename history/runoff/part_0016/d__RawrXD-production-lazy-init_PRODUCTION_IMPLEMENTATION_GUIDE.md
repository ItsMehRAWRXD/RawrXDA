# Production-Ready Agentic Engine & Tool Registry Implementation Guide

## Overview

This document provides comprehensive guidance for integrating and using the production-ready implementations of:
1. **AgenticEngine_ExecuteTask_Production** - Enterprise-grade task execution
2. **ToolRegistry_InvokeToolSet_Production** - Advanced tool invocation
3. **Centralized Error Handler** - Unified exception handling

---

## Architecture Overview

### Core Components

```
┌─────────────────────────────────────────────────────────┐
│  Application Layer (Agent Coordinator, UI, APIs)        │
└────────────────┬────────────────────────────────────────┘
                 │
┌────────────────▼────────────────────────────────────────┐
│  Agentic Engine Task Execution Layer                     │
│  ├─ Parameter Validation & Parsing                       │
│  ├─ Execution Context Management                         │
│  ├─ Automatic Retry with Exponential Backoff             │
│  └─ Performance Instrumentation                          │
└────────────────┬────────────────────────────────────────┘
                 │
┌────────────────▼────────────────────────────────────────┐
│  Tool Registry & Invocation Layer                        │
│  ├─ Tool Discovery & Metadata                            │
│  ├─ Resource Guards (RAII Pattern)                       │
│  ├─ Pipeline Execution & Monitoring                      │
│  └─ Tool Health Tracking                                 │
└────────────────┬────────────────────────────────────────┘
                 │
┌────────────────▼────────────────────────────────────────┐
│  Centralized Error Handling                              │
│  ├─ Exception Capture & Categorization                   │
│  ├─ Recovery Strategies & Retry Logic                    │
│  ├─ Error Context Propagation                            │
│  └─ Metrics & Logging Integration                        │
└────────────────┬────────────────────────────────────────┘
                 │
┌────────────────▼────────────────────────────────────────┐
│  Infrastructure Layer                                    │
│  ├─ Structured Logging (DEBUG/INFO/WARNING/ERROR)       │
│  ├─ Prometheus Metrics (Counters/Gauges/Histograms)    │
│  ├─ Distributed Tracing (OpenTelemetry Ready)           │
│  ├─ Resource Management & Lifecycle                      │
│  └─ Thread-Safe Synchronization                          │
└─────────────────────────────────────────────────────────┘
```

---

## Data Structures

### ExecutionContext (128 bytes)

Captures complete execution state for task lifecycle management.

```asm
EXEC_CONTEXT STRUCT
    state               DWORD ?         ; EXEC_STATE_* constant
    error_code          DWORD ?         ; ERROR_* code
    retry_count         DWORD ?         ; Retry attempts made
    
    start_time          FILETIME <>     ; Execution start
    end_time            FILETIME <>     ; Execution end
    
    tool_name_ptr       QWORD ?         ; Tool to invoke
    params_ptr          QWORD ?         ; JSON parameters
    output_buffer_ptr   QWORD ?         ; Result buffer
    output_buffer_size  QWORD ?         ; Buffer size
    
    resource_handle     QWORD ?         ; Primary resource
    secondary_handle    QWORD ?         ; Secondary resource
    
    trace_id            BYTE 16 DUP(?) ; 128-bit trace ID
    span_id             BYTE 8 DUP(?)  ; 64-bit span ID
    
    flags               DWORD ?         ; Atomic flags
    result_size         DWORD ?         ; Result size
    
    metrics_handle      QWORD ?         ; Metrics recorder
    logger_handle       QWORD ?         ; Logger instance
EXEC_CONTEXT ENDS
```

### ErrorContext (128 bytes)

Complete error state for propagation and recovery.

```asm
ERROR_CONTEXT STRUCT
    error_code                  DWORD ?
    category                   DWORD ?
    severity                   DWORD ?
    
    message_ptr                QWORD ?
    source_function_ptr        QWORD ?
    source_file_ptr            QWORD ?
    source_line                DWORD ?
    
    inner_error_context_ptr    QWORD ?
    affected_resource_ptr      QWORD ?
    recovery_hint_ptr          QWORD ?
    
    timestamp                  FILETIME <>
    
    handle_action              DWORD ?
    is_recoverable             DWORD ?
    
    context_data_ptr           QWORD ?
    context_data_size          QWORD ?
    
    caller_context_ptr         QWORD ?
ERROR_CONTEXT ENDS
```

---

## Function Signatures

### Core Task Execution

#### AgenticEngine_ExecuteTask_Production

**Purpose**: Execute a task with full enterprise instrumentation.

**Signature**:
```asm
; Input:  rcx = engine (ZeroDayAgenticEngine*)
;         rdx = task_descriptor (LPCSTR) - JSON format
;         r8  = timeout_ms (QWORD)
; Output: rax = 1 (success) or 0 (failure)
AgenticEngine_ExecuteTask_Production PROC
```

**Parameters**:
- `engine`: Pointer to engine instance from `ZeroDayAgenticEngine_Create`
- `task_descriptor`: JSON string containing:
  ```json
  {
    "tool": "file_write",
    "params": "{\"path\": \"/tmp/test.txt\", \"content\": \"data\"}",
    "timeout_ms": 5000
  }
  ```
- `timeout_ms`: Maximum execution time in milliseconds

**Return Values**:
- `rax = 1`: Task executed successfully
- `rax = 0`: Task failed (check error context)

**Error Handling**:
```asm
; Check last error
CALL ErrorHandler_GetLastError      ; rax = ERROR_CONTEXT*
MOV eax, [rax + OFFSET ERROR_CONTEXT.error_code]
```

**Retry Behavior**:
- Automatic retry up to 3 times (MAX_RETRIES)
- Exponential backoff: (2^attempt) * 100ms
- Only retries on transient errors (timeout, resource errors)
- Logs each retry attempt at INFO level

**Instrumentation**:
- Records execution time histogram
- Tracks success/failure metrics
- Logs at key points (start, retry, complete, error)
- Propagates distributed trace context

---

### Tool Invocation

#### ToolRegistry_InvokeToolSet_Production

**Purpose**: Execute tool with instrumentation and metrics.

**Signature**:
```asm
; Input:  rcx = ToolRegistry*
;         rdx = tool_name (LPCSTR)
;         r8  = params (LPCSTR)
;         r9  = output_buffer (LPSTR)
;         [rsp+20h] = buffer_size (QWORD)
; Output: rax = 1 (success) or 0 (failure)
ToolRegistry_InvokeToolSet_Production PROC
```

**Supported Tools**:
- `file_write`: Write content to file
- `file_read`: Read file content
- `file_delete`: Delete file
- `file_exists`: Check file existence
- `git_status`: Get git status
- `git_add`: Stage files
- `git_commit`: Commit changes

**Error Codes**:
```
ERROR_OK = 0                ; Success
ERROR_INVALID_PARAM = 1     ; Invalid parameter
ERROR_ALLOC_FAILED = 2      ; Memory allocation failure
ERROR_TOOL_NOT_FOUND = 3    ; Unknown tool
ERROR_TOOL_TIMEOUT = 4      ; Tool execution timeout
ERROR_TOOL_FAILED = 5       ; Tool invocation failed
ERROR_RESOURCE_ERROR = 6    ; Resource unavailable
ERROR_CANCELLED = 7         ; Execution cancelled
```

---

### Resource Management

#### ExecutionContext_Create

**Purpose**: Allocate and initialize execution context.

**Signature**:
```asm
; Input:  rcx = tool_name (LPCSTR)
;         rdx = params (LPCSTR)
;         r8  = output_buffer (LPSTR)
;         r9  = output_buffer_size (QWORD)
; Output: rax = ExecutionContext* (or NULL)
ExecutionContext_Create PROC
```

#### ExecutionContext_Destroy

**Purpose**: Clean up execution context and all resources.

**Signature**:
```asm
; Input:  rcx = ExecutionContext*
ExecutionContext_Destroy PROC
```

---

### Error Handling

#### ErrorHandler_CaptureException

**Purpose**: High-level exception capture (API gateway pattern).

**Signature**:
```asm
; Input:  rcx = error_code (DWORD)
;         rdx = error_message (LPCSTR)
;         r8  = source_function (LPCSTR)
;         r9  = source_file (LPCSTR)
;         [rsp+20h] = source_line (DWORD)
; Output: rax = action (ACTION_CONTINUE/RETRY/ABORT/ESCALATE)
ErrorHandler_CaptureException PROC
```

**Actions**:
```
ACTION_CONTINUE = 0         ; Continue normally
ACTION_RETRY = 1            ; Retry operation
ACTION_ABORT = 2            ; Abort operation
ACTION_FALLBACK = 3         ; Use fallback path
ACTION_ESCALATE = 4         ; Escalate to higher level
```

---

## Integration Examples

### Example 1: Basic Task Execution

```asm
; Create engine
MOV rcx, router
MOV rdx, tools_registry
MOV r8, planner
MOV r9, signal_callbacks
CALL ZeroDayAgenticEngine_Create
MOV engine, rax

; Prepare task descriptor
LEA task_json, [szTaskJson]       ; JSON string
MOV timeout_ms, 5000               ; 5 second timeout

; Execute task
MOV rcx, engine
MOV rdx, task_json
MOV r8, timeout_ms
CALL AgenticEngine_ExecuteTask_Production

; Check result
TEST eax, eax
JZ task_failed

; Success - handle result
...

task_failed:
; Get error context
CALL ErrorHandler_GetLastError
MOV error_ctx, rax

; Access error details
MOV eax, [error_ctx + OFFSET ERROR_CONTEXT.error_code]
```

### Example 2: Tool Invocation with Guards

```asm
; Invoke tool with automatic resource cleanup
MOV rcx, registry
LEA rdx, [szToolName]
LEA r8, [szParams]
LEA r9, [output_buffer]
MOV QWORD PTR [rsp + 20h], buffer_size
CALL ToolRegistry_InvokeWithGuards

TEST eax, eax
JZ tool_invocation_failed

; Resource automatically cleaned up
...
```

### Example 3: Error Recovery

```asm
; Initialize error handler
MOV rcx, 16                 ; Max 16 error contexts
LEA rdx, [gErrorCallback]   ; Global error callback
CALL ErrorHandler_Initialize

; During operation, if error occurs:
; Call capture
MOV rcx, error_code
LEA rdx, [error_message]
LEA r8, [current_function]
LEA r9, [current_file]
MOV DWORD PTR [rsp + 20h], current_line
CALL ErrorHandler_CaptureException

; Action tells you what to do
; ACTION_CONTINUE: Keep going
; ACTION_RETRY: Retry operation
; ACTION_ABORT: Stop operation
; ACTION_ESCALATE: Notify upper layer
```

---

## Logging & Instrumentation

### Log Levels

```
LOG_LEVEL_DEBUG = 0       ; Detailed diagnostic information
LOG_LEVEL_INFO = 1        ; General informational messages
LOG_LEVEL_WARNING = 2     ; Warning messages
LOG_LEVEL_ERROR = 3       ; Error messages
```

### Key Log Points

1. **Task Execution**:
   - `szTaskExecutionStart`: Task begins
   - `szTaskExecutionComplete`: Task succeeds
   - `szRetryAttempt`: Retry in progress
   - `szTaskExecutionFailed`: Task fails

2. **Tool Invocation**:
   - `szToolInvocationStart`: Tool begins
   - `szToolInvocationSuccess`: Tool succeeds
   - `szToolInvocationFailed`: Tool fails

3. **Error Handling**:
   - `szFatalErrorOccurred`: Non-recoverable error
   - `szRecoveryFailed`: Recovery attempt failed
   - Error code, category, and severity logged

### Metrics Recorded

**Counters**:
- `agent.tool.invocation.count`: Total tool invocations
- `agent.tool.error.count`: Total tool errors
- `agent.task.execution.count`: Total task executions

**Histograms**:
- `agent.tool.duration.ms`: Tool execution time
- `agent.task.duration.ms`: Task execution time
- `agent.error.recovery.attempts`: Recovery attempt count

**Gauges**:
- `agent.running.tasks`: Currently executing tasks
- `agent.error.rate`: Current error rate
- `agent.tool.health`: Tool availability/health

---

## Configuration & Deployment

### Environment Variables

```
RAWR_TOOL_TIMEOUT_MS=30000           ; Default tool timeout (ms)
RAWR_MAX_RETRIES=3                   ; Max retry attempts
RAWR_LOG_LEVEL=INFO                  ; Logging level
RAWR_ENABLE_TRACING=1                ; Enable distributed tracing
RAWR_METRICS_ENABLED=1               ; Enable metrics recording
RAWR_ERROR_RECOVERY_ENABLED=1        ; Enable error recovery
```

### Configuration File (JSON)

```json
{
  "agentic_engine": {
    "max_concurrent_tasks": 10,
    "task_timeout_ms": 30000,
    "enable_tracing": true,
    "log_level": "INFO"
  },
  "tool_registry": {
    "max_concurrent_tools": 16,
    "tool_timeout_ms": 30000,
    "health_check_interval_ms": 60000
  },
  "error_handling": {
    "max_error_contexts": 256,
    "recovery_enabled": true,
    "recovery_max_retries": 3,
    "recovery_backoff_ms": 100
  },
  "instrumentation": {
    "logging_enabled": true,
    "metrics_enabled": true,
    "tracing_enabled": true,
    "trace_sample_rate": 1.0
  }
}
```

---

## Testing & Validation

### Unit Tests

1. **ExecutionContext Tests**:
   - Context creation with valid/invalid inputs
   - Resource allocation and cleanup
   - Error state management

2. **Tool Registry Tests**:
   - Tool registration and discovery
   - Invocation success and failure paths
   - Timeout handling
   - Resource guard cleanup

3. **Error Handler Tests**:
   - Exception capture and categorization
   - Error context creation
   - Recovery strategy selection
   - Retry logic with backoff

### Integration Tests

1. **End-to-End Task Execution**:
   - Create engine
   - Execute task
   - Verify metrics
   - Check error handling

2. **Tool Invocation Chain**:
   - Multiple sequential tool invocations
   - Error recovery between tools
   - Resource cleanup verification

3. **Error Scenarios**:
   - Timeout during execution
   - Invalid parameters
   - Resource exhaustion
   - Transient failures with recovery

### Performance Tests

1. **Throughput**: Tasks per second under load
2. **Latency**: P50/P95/P99 execution times
3. **Memory**: Peak memory during execution
4. **Resource Cleanup**: No leaks under error conditions

---

## Production Deployment Checklist

- [ ] Error handler initialized before engine creation
- [ ] Logging system configured and active
- [ ] Metrics collector connected
- [ ] Distributed tracing enabled (if using)
- [ ] Resource limits set (CPU, memory)
- [ ] Timeout values tuned for environment
- [ ] Retry policies tested
- [ ] Error recovery strategies registered
- [ ] Monitoring dashboards configured
- [ ] Alert rules configured for critical errors
- [ ] Load testing completed
- [ ] Disaster recovery procedures documented
- [ ] Incident response procedures documented

---

## Advanced Topics

### Distributed Tracing Integration

The ExecutionContext includes trace ID and span ID fields for OpenTelemetry integration:

```asm
; Access trace context
MOV rcx, execution_context
LEA rdx, [rcx + OFFSET EXEC_CONTEXT.trace_id]  ; 16-byte trace ID
LEA r8, [rcx + OFFSET EXEC_CONTEXT.span_id]   ; 8-byte span ID

; Propagate to downstream services
; Span ID becomes parent for child spans
```

### Custom Error Strategies

Register custom error handling strategies:

```asm
; Register strategy for specific error code
MOV rcx, error_code
LEA rdx, [custom_handler_fn]
MOV r8d, max_retries
MOV r9d, backoff_ms
CALL ErrorHandler_RegisterStrategy
```

### Resource Pool Management

The ExecutionContext supports pooling:

```asm
; Reuse contexts across multiple tasks
; Context_Destroy moves context back to pool
; Context_Create retrieves from pool if available
; Reduces allocation overhead in high-throughput scenarios
```

---

## Troubleshooting

### Common Issues

1. **Memory Leaks**:
   - Ensure ExecutionContext_Destroy is always called
   - Use resource guards for automatic cleanup
   - Monitor gTotalErrorCount vs gRecoveredErrorCount

2. **Timeout Issues**:
   - Check tool invocation time with metrics histograms
   - Adjust TOOL_TIMEOUT_MS if needed
   - Review logs for slow operations

3. **Retry Exhaustion**:
   - Check error_code to identify non-retryable errors
   - Verify recovery strategy is registered
   - Increase MAX_RETRIES if appropriate

4. **Resource Exhaustion**:
   - Monitor EXEC_CONTEXT allocations
   - Check for resource handle leaks
   - Review error metrics for patterns

---

## References

- Windows API Documentation: CreateThread, WaitForSingleObject, LocalAlloc
- MASM x64 Calling Convention: https://docs.microsoft.com/en-us/cpp/build/x64-calling-convention
- OpenTelemetry Specification: https://opentelemetry.io/
- Prometheus Metrics: https://prometheus.io/docs/instrumenting/

---

**Document Version**: 1.0
**Last Updated**: December 31, 2025
**Status**: Production Ready
