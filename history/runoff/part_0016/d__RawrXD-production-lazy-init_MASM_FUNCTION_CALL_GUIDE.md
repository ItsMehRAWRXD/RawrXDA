# MASM Master Include - Function Call Quick Reference Guide

This guide shows how to add function calls from `masm_master_include.asm` to your MASM modules for production readiness.

## Quick Start Pattern

Once you've added `INCLUDE masm/masm_master_include.asm` to your file, you can immediately call these functions:

### Example 1: Log a Simple Message

```asm
; Setup: string pointer in RAX, level in RCX
lea rax, [rel MyMessage]          ; Pointer to message string
mov rcx, LOG_LEVEL_INFO           ; From master include (=1)
call Logger_LogStructured         ; Now available!
```

### Example 2: Record Performance Metric

```asm
; Record how long a function took
call QueryPerformanceCounter
mov r14, rax                      ; Save start time

; ... do work ...

call QueryPerformanceCounter
sub rax, r14                      ; Calculate elapsed
call Metrics_RecordLatency        ; Available from master include
```

### Example 3: Mission Execution

```asm
; Create and start an agentic mission
call ZeroDayAgenticEngine_Create  ; Create instance (rax = engine handle)
mov r13, rax                      ; Save engine handle

; Start mission
mov rcx, r13                      ; Engine handle
call ZeroDayAgenticEngine_StartMission  ; Async start

; Later: cleanup
mov rcx, r13
call ZeroDayAgenticEngine_Destroy
```

## Function Categories

### 🚀 Zero-Day Agentic Engine (NEW)

**Purpose**: High-performance autonomous execution with streaming & failure recovery

| Function | Input | Output | Notes |
|----------|-------|--------|-------|
| `ZeroDayAgenticEngine_Create` | — | RAX = handle | Must call before using engine |
| `ZeroDayAgenticEngine_Destroy` | RCX = handle | — | Cleanup (RAII) |
| `ZeroDayAgenticEngine_StartMission` | RCX = handle | RAX = mission_id | Async execution |
| `ZeroDayAgenticEngine_AbortMission` | RCX = handle | — | Graceful abort |
| `ZeroDayAgenticEngine_GetMissionState` | RCX = handle | RAX = state | IDLE/RUNNING/COMPLETE/ERROR |
| `ZeroDayAgenticEngine_LogStructured` | RCX = level, RDX = msg | — | Log with context |

**State Constants** (available from include):
- `MISSION_STATE_IDLE` = 0
- `MISSION_STATE_RUNNING` = 1
- `MISSION_STATE_COMPLETE` = 3
- `MISSION_STATE_ERROR` = 4

### 📊 Logging System

**Purpose**: Structured logging with multiple output targets

| Function | Input | Notes |
|----------|-------|-------|
| `Logger_LogStructured` | RCX=level, RDX=message | Primary logging function |
| `Logger_LogMissionStart` | RCX=mission_id | Log mission initialization |
| `Logger_LogMissionComplete` | RCX=mission_id | Log successful completion |
| `Logger_LogMissionError` | RCX=mission_id, RDX=error_code | Log failure with details |

**Log Levels** (constants from master include):
```asm
LOG_LEVEL_DEBUG     = 0    ; Detailed diagnostics
LOG_LEVEL_INFO      = 1    ; General information
LOG_LEVEL_WARN      = 2    ; Warnings
LOG_LEVEL_ERROR     = 3    ; Error conditions
```

### 📈 Metrics Collection

**Purpose**: Performance monitoring and statistics

| Function | Input | Notes |
|----------|-------|-------|
| `Metrics_RecordLatency` | RAX = latency_us | Record execution time |
| `Metrics_IncrementMissionCounter` | RCX = counter_id | Increment stat counter |
| `Metrics_RecordHistogramMission` | RCX = bucket, RAX = value | Record distribution |

### 🔗 Integration Layer

**Purpose**: Bridge with existing systems

| Function | Input | Notes |
|----------|-------|-------|
| `ZeroDayIntegration_Initialize` | — | Setup integration |
| `ZeroDayIntegration_AnalyzeComplexity` | RCX = task | Determine complexity level |
| `ZeroDayIntegration_RouteExecution` | RCX = task | Route to appropriate engine |
| `ZeroDayIntegration_IsHealthy` | — | RAX = 1 if healthy |

### 🤖 Agentic System

**Purpose**: Standard agent execution and tool management

| Function | Input | Notes |
|----------|-------|-------|
| `AgenticEngine_ExecuteTask` | RCX = task | Execute standard task |
| `AgenticEngine_CreateContext` | RCX = context_type | Create execution context |
| `ToolRegistry_InvokeToolSet` | RCX = tool_name, RDX = params | Invoke named tool |
| `UniversalModelRouter_SelectModel` | RCX = task_type | Choose best model |

## Integration Patterns

### Pattern 1: Minimal Logging (Easiest)

Add logging at function entry/exit:

```asm
PUBLIC my_function
my_function PROC FRAME
    ; Entry log
    lea rax, [rel FuncName]
    mov rcx, LOG_LEVEL_DEBUG
    call Logger_LogStructured
    
    ; Your existing code here
    ; ... (UNCHANGED)
    
    ; Exit log  
    lea rax, [rel FuncName]
    mov rcx, LOG_LEVEL_DEBUG
    call Logger_LogStructured
    
    ret
my_function ENDP

.data
FuncName db "my_function", 0
```

### Pattern 2: Performance Instrumentation

Track how long operations take:

```asm
PUBLIC expensive_operation
expensive_operation PROC FRAME
    push r14                      ; Save register
    
    call QueryPerformanceCounter  ; Start timer
    mov r14, rax
    
    ; Your expensive code here
    ; ...
    
    call QueryPerformanceCounter  ; End timer
    sub rax, r14                  ; Duration in RAX
    call Metrics_RecordLatency    ; Record it
    
    pop r14
    ret
expensive_operation ENDP
```

### Pattern 3: Mission-Based Execution

For high-level orchestration:

```asm
PUBLIC execute_mission
execute_mission PROC FRAME
    sub rsp, 32
    
    ; Log start
    lea rax, [rel MissionMsg]
    mov rcx, LOG_LEVEL_INFO
    call Logger_LogMissionStart
    
    ; Create engine
    call ZeroDayAgenticEngine_Create
    mov r13, rax  ; Save handle
    
    ; Start mission
    mov rcx, r13
    call ZeroDayAgenticEngine_StartMission  ; Async
    mov r14, rax  ; Save mission_id
    
    ; Wait for completion (in real code, use wait mechanism)
    ; ...
    
    ; Log completion
    mov rcx, r14
    call Logger_LogMissionComplete
    
    ; Cleanup
    mov rcx, r13
    call ZeroDayAgenticEngine_Destroy
    
    add rsp, 32
    ret
execute_mission ENDP

.data
MissionMsg db "Mission execution started", 0
```

### Pattern 4: Error Handling with Logging

```asm
PUBLIC safe_operation
safe_operation PROC FRAME
    ; Attempt operation
    call SomeRiskyFunction
    test rax, rax
    jnz .success
    
.error:
    ; Log error with code
    lea rax, [rel ErrorMsg]
    mov rcx, LOG_LEVEL_ERROR
    mov rdx, rax  ; Error details
    call Logger_LogMissionError
    
    mov rax, -1   ; Return error
    ret
    
.success:
    ; Log success
    lea rax, [rel SuccessMsg]
    mov rcx, LOG_LEVEL_INFO
    call Logger_LogStructured
    
    ret
safe_operation ENDP

.data
ErrorMsg db "Operation failed", 0
SuccessMsg db "Operation succeeded", 0
```

## Testing Your Integration

### Compile Test

```powershell
# Compile single file to check for errors
ml64.exe /W3 /DWIN32 /D_DEBUG your_file.asm

# Link test (if using object files)
link.exe your_file.obj /LIBPATH:. masm_modules.lib
```

### Runtime Test

Add a simple test function:

```asm
PUBLIC test_master_include
test_master_include PROC
    ; Test 1: Log a message
    lea rax, [rel TestMsg]
    mov rcx, LOG_LEVEL_INFO
    call Logger_LogStructured
    
    ; Test 2: Create an engine
    call ZeroDayAgenticEngine_Create
    test rax, rax
    jz .failed
    
    ; Test 3: Destroy it
    mov rcx, rax
    call ZeroDayAgenticEngine_Destroy
    
    mov rax, 1  ; Success
    ret
    
.failed:
    mov rax, 0
    ret
test_master_include ENDP

.data
TestMsg db "Master include test successful", 0
```

## Configuration at Runtime

All behaviors can be configured via environment variables:

```powershell
# Before running your program:
$env:LOG_LEVEL = "1"                 # INFO level
$env:ENABLE_METRICS = "1"            # Enable metrics
$env:AUTO_RETRY_ENABLED = "1"        # Auto-retry on failure
$env:MISSION_TIMEOUT_MS = "300000"   # 5 minute timeout

.\my_program.exe
```

The master include will read these automatically.

## Common Issues & Solutions

### Issue: "Undefined symbol: Logger_LogStructured"

**Cause**: File missing `INCLUDE masm/masm_master_include.asm`

**Solution**: Add the include statement at top of file:
```asm
include windows.inc
includelib kernel32.lib

INCLUDE masm/masm_master_include.asm
```

### Issue: "RAX must contain string pointer but contains address"

**Cause**: Using wrong register or not using `lea` for string address

**Solution**: Always use `lea` for address computation:
```asm
lea rax, [rel MyString]    ; ✅ Correct
mov rax, MyString          ; ❌ Wrong - just address value
```

### Issue: Function call doesn't return immediately

**Cause**: Calling async function (like `StartMission`) and expecting synchronous behavior

**Solution**: Save mission ID and wait for state change:
```asm
call ZeroDayAgenticEngine_StartMission  ; Async - returns immediately
mov r14, rax  ; Save mission_id

; Later, check state
mov rcx, r14  ; mission_id
.poll_loop:
    call ZeroDayAgenticEngine_GetMissionState
    cmp rax, MISSION_STATE_COMPLETE
    jne .poll_loop
```

## Next Steps

1. **Add the INCLUDE** to your MASM file (if not already done)
2. **Choose a pattern** from above that fits your use case
3. **Add one function call** as a test
4. **Compile and verify** it works
5. **Expand incrementally** to other functions

## Supported Architectures

- x64 (x86-64) - ✅ Primary
- x86 (32-bit) - Needs porting
- ARM64 - Future

## Additional Resources

- **Master Include File**: `masm/masm_master_include.asm` (full documentation)
- **Strategy Document**: `MASM_INTEGRATION_STRATEGY.md`
- **Progress Report**: `MASM_INTEGRATION_PROGRESS.md`
- **Build Script**: `Build-MASM-Modules.ps1`

---

**Version**: 1.0  
**Updated**: 2024-12-30  
**Status**: Production Ready
