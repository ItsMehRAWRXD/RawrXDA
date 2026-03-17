# MASM Function Call Quick Reference

**Purpose**: Quick lookup guide for adding function calls to Priority 1 files during Phase 2

---

## Function Calling Convention

All functions use **x64 calling convention** (Windows x64):
- **rcx** = 1st parameter
- **rdx** = 2nd parameter
- **r8** = 3rd parameter
- **r9** = 4th parameter
- **stack** = 5th+ parameters
- **rax** = return value

---

## Core Functions to Add

### 1. ZeroDayAgenticEngine_Create

**Purpose**: Create engine instance (call at startup)

**Signature**:
```asm
; Input:
;   rcx = router pointer
;   rdx = tools pointer
;   r8 = planner pointer
;   r9 = callbacks pointer
; Output:
;   rax = engine handle (check != 0)
EXTERN ZeroDayAgenticEngine_Create:PROC
```

**Usage**:
```asm
MOV rcx, [g_router_ptr]
MOV rdx, [g_tools_ptr]
MOV r8, [g_planner_ptr]
MOV r9, [g_callbacks_ptr]
CALL ZeroDayAgenticEngine_Create
MOV [g_engine_handle], rax

; Check for error
CMP rax, 0
JE engine_creation_failed
```

**Where to Add**:
- main_masm.asm: initialization function (~line 50)
- agent_executor.asm: startup sequence
- agent_orchestrator_main.asm: orchestration init

**Files**: main_masm.asm, agent_executor.asm, unified_hotpatch_manager.asm

---

### 2. ZeroDayAgenticEngine_Destroy

**Purpose**: Clean up engine (call at shutdown)

**Signature**:
```asm
; Input:
;   rcx = engine handle
; Output:
;   rax = status (1 = success)
EXTERN ZeroDayAgenticEngine_Destroy:PROC
```

**Usage**:
```asm
MOV rcx, [g_engine_handle]
CALL ZeroDayAgenticEngine_Destroy

; Check success
CMP eax, 1
JNE cleanup_failed
```

**Where to Add**:
- main_masm.asm: shutdown function (~line 2050)
- agent_executor.asm: cleanup
- Any file with init/cleanup pair

**Files**: main_masm.asm, agent_executor.asm

---

### 3. Logger_LogStructured

**Purpose**: Write structured log message (use throughout)

**Signature**:
```asm
; Input:
;   rcx = message string (ANSI, NULL-terminated)
;   rdx = log level (DEBUG=0, INFO=1, WARN=2, ERROR=3)
; Output:
;   rax = number of bytes written
EXTERN Logger_LogStructured:PROC
```

**Usage**:
```asm
; Log info message
LEA rcx, [rel msg_info]
MOV rdx, LOG_LEVEL_INFO
CALL Logger_LogStructured

; Log error message
LEA rcx, [rel msg_error]
MOV rdx, LOG_LEVEL_ERROR
CALL Logger_LogStructured

; Define messages in data section
msg_info DB "Operation started successfully", 0
msg_error DB "Error: Invalid parameter", 0
```

**Log Levels**:
```asm
LOG_LEVEL_DEBUG EQU 0   ; Detailed debugging info
LOG_LEVEL_INFO  EQU 1   ; General information
LOG_LEVEL_WARN  EQU 2   ; Warning condition
LOG_LEVEL_ERROR EQU 3   ; Error condition
```

**Where to Add** (High-value locations):
- main_masm.asm: initialization/shutdown (~50+ messages)
- agentic_masm.asm: tool execution, errors (~100+ messages)
- ml_masm.asm: model loading, inference (~30+ messages)
- logging.asm: be careful with recursion
- All error paths

**Files**: ALL 9 Priority 1 files (most important)

---

### 4. QueryPerformanceCounter

**Purpose**: Get high-resolution timestamp for latency measurement

**Signature**:
```asm
; Input:
;   (none)
; Output:
;   rax = performance counter value (QWORD)
EXTERN QueryPerformanceCounter:PROC
```

**Usage**:
```asm
; At operation start
CALL QueryPerformanceCounter
MOV r14, rax                    ; Save start time

; ... do work ...

; At operation end
CALL QueryPerformanceCounter
SUB rax, r14                    ; rax = elapsed ticks
CALL Metrics_RecordLatency      ; Record timing
```

**Typical Operations to Measure**:
- Tool execution (agentic_masm.asm)
- Model loading/inference (ml_masm.asm)
- Patch application (unified_masm_hotpatch.asm)
- Memory allocation (asm_memory.asm)
- String operations (asm_string.asm)

**Files**: agentic_masm.asm, ml_masm.asm, unified_masm_hotpatch.asm, asm_memory.asm, asm_string.asm

---

### 5. Metrics_RecordLatency

**Purpose**: Record operation latency (must call QueryPerformanceCounter first)

**Signature**:
```asm
; Input:
;   rax = elapsed ticks (from QueryPerformanceCounter)
; Output:
;   (none)
EXTERN Metrics_RecordLatency:PROC
```

**Usage** (with QueryPerformanceCounter):
```asm
CALL QueryPerformanceCounter
MOV r14, rax

; ... work ...

CALL QueryPerformanceCounter
SUB rax, r14
CALL Metrics_RecordLatency      ; Record to metrics system
```

**Files**: agentic_masm.asm, ml_masm.asm, unified_masm_hotpatch.asm, asm_memory.asm, asm_string.asm

---

### 6. Metrics_RecordHistogramMission

**Purpose**: Record metrics with mission context (inference, complex ops)

**Signature**:
```asm
; Input:
;   rcx = operation identifier (string)
;   rdx = value (ticks)
;   r8 = mission ID
; Output:
;   (none)
EXTERN Metrics_RecordHistogramMission:PROC
```

**Usage**:
```asm
LEA rcx, [rel op_name]
MOV rdx, rax                    ; Elapsed ticks
MOV r8, [mission_id]
CALL Metrics_RecordHistogramMission
```

**Files**: ml_masm.asm (inference timing), agentic_masm.asm (complex ops)

---

### 7. AgenticEngine_ExecuteTask

**Purpose**: Execute agentic task (legacy integration, call in tool loop)

**Signature**:
```asm
; Input:
;   rcx = task structure pointer
; Output:
;   rax = result (0 = error, 1 = success)
EXTERN AgenticEngine_ExecuteTask:PROC
```

**Usage**:
```asm
LEA rcx, [g_task_struct]
CALL AgenticEngine_ExecuteTask

CMP eax, 0
JE task_failed

; Continue on success
```

**Files**: agentic_masm.asm (tool dispatch), agent_executor.asm

---

### 8. UniversalModelRouter_SelectModel

**Purpose**: Route to appropriate model (legacy integration)

**Signature**:
```asm
; Input:
;   rcx = model name (string)
;   rdx = query/prompt (string)
; Output:
;   rax = model handle
EXTERN UniversalModelRouter_SelectModel:PROC
```

**Usage**:
```asm
LEA rcx, [rel model_name]
LEA rdx, [rel query_prompt]
CALL UniversalModelRouter_SelectModel
MOV [model_handle], rax

CMP rax, 0
JE model_selection_failed
```

**Files**: ml_masm.asm (model initialization)

---

### 9. Metrics_IncrementMissionCounter

**Purpose**: Count missions executed

**Signature**:
```asm
; Input:
;   (none)
; Output:
;   rax = new count
EXTERN Metrics_IncrementMissionCounter:PROC
```

**Usage**:
```asm
CALL Metrics_IncrementMissionCounter
; rax now contains total mission count
```

**Files**: agent_orchestrator_main.asm (mission tracking)

---

## Logging Best Practices

### 1. Define Messages in Data Section
```asm
section .data
    msg_startup DB "Engine starting up", 0
    msg_init_fail DB "Initialization failed: %08X", 0
    msg_tool_complete DB "Tool %d completed in %d ticks", 0
```

### 2. Use Appropriate Levels
```asm
; DEBUG: Detailed info (performance measurements, intermediate values)
LEA rcx, [rel msg_debug]
MOV rdx, LOG_LEVEL_DEBUG
CALL Logger_LogStructured

; INFO: Normal operation (startup, completion, state changes)
LEA rcx, [rel msg_info]
MOV rdx, LOG_LEVEL_INFO
CALL Logger_LogStructured

; WARN: Something unexpected but handled (retry, fallback)
LEA rcx, [rel msg_warn]
MOV rdx, LOG_LEVEL_WARN
CALL Logger_LogStructured

; ERROR: Failure that stops current operation
LEA rcx, [rel msg_error]
MOV rdx, LOG_LEVEL_ERROR
CALL Logger_LogStructured
```

### 3. Log Entry/Exit Points
```asm
process_tool PROC
    ; Log entry
    LEA rcx, [rel msg_tool_entry]
    MOV rdx, LOG_LEVEL_DEBUG
    CALL Logger_LogStructured
    
    ; ... do work ...
    
    ; Log exit
    LEA rcx, [rel msg_tool_exit]
    MOV rdx, LOG_LEVEL_DEBUG
    CALL Logger_LogStructured
    
    RET
process_tool ENDP
```

### 4. Log Error Paths
```asm
; Always log when error occurs
CMP rax, 0
JNE success_path

; Log failure
LEA rcx, [rel msg_error]
MOV rdx, LOG_LEVEL_ERROR
CALL Logger_LogStructured

JMP cleanup

success_path:
    ; Log success
    LEA rcx, [rel msg_success]
    MOV rdx, LOG_LEVEL_INFO
    CALL Logger_LogStructured
    
cleanup:
    RET
```

---

## Performance Measurement Best Practices

### 1. Critical Sections Only
```asm
; GOOD: Measure critical operations
CALL QueryPerformanceCounter
MOV r14, rax
CALL ExpensiveOperation
CALL QueryPerformanceCounter
SUB rax, r14
CALL Metrics_RecordLatency

; BAD: Too granular (every line)
CALL QueryPerformanceCounter
MOV r14, rax
ADD rcx, rdx
CALL QueryPerformanceCounter
SUB rax, r14
CALL Metrics_RecordLatency

; BAD: Forgetting to record
CALL QueryPerformanceCounter
MOV r14, rax
CALL ExpensiveOperation
CALL QueryPerformanceCounter
SUB rax, r14
; Missing Metrics_RecordLatency call
```

### 2. Expected Performance Impact
- Engine creation: <10ms
- Logging per message: <1ms
- Metrics recording: <0.5ms
- Total overhead target: <5% of total execution time

---

## Common Code Patterns

### Pattern 1: Tool Execution Loop
```asm
execute_tool_loop PROC
    XOR ecx, ecx                ; Tool counter
    
tool_loop:
    ; Log tool start
    LEA rcx, [rel msg_tool_start]
    MOV rdx, LOG_LEVEL_DEBUG
    CALL Logger_LogStructured
    
    ; Measure execution
    CALL QueryPerformanceCounter
    MOV r14, rax
    
    ; Execute tool
    MOV ecx, [current_tool_id]
    CALL AgenticEngine_ExecuteTask
    
    ; Record timing
    CALL QueryPerformanceCounter
    SUB rax, r14
    CALL Metrics_RecordLatency
    
    ; Log result
    CMP eax, 0
    JE tool_failed
    
    LEA rcx, [rel msg_tool_success]
    MOV rdx, LOG_LEVEL_INFO
    CALL Logger_LogStructured
    
    ; Continue loop
    INC [tool_counter]
    CMP [tool_counter], [total_tools]
    JL tool_loop
    
    RET
    
tool_failed:
    LEA rcx, [rel msg_tool_error]
    MOV rdx, LOG_LEVEL_ERROR
    CALL Logger_LogStructured
    JMP tool_loop
execute_tool_loop ENDP
```

### Pattern 2: Initialization with Error Handling
```asm
initialize_system PROC
    ; Create engine
    MOV rcx, [g_router]
    MOV rdx, [g_tools]
    MOV r8, [g_planner]
    MOV r9, [g_callbacks]
    CALL ZeroDayAgenticEngine_Create
    
    CMP rax, 0
    JE init_error
    
    MOV [g_engine], rax
    
    ; Log success
    LEA rcx, [rel msg_init_success]
    MOV rdx, LOG_LEVEL_INFO
    CALL Logger_LogStructured
    
    MOV eax, 1
    RET
    
init_error:
    LEA rcx, [rel msg_init_error]
    MOV rdx, LOG_LEVEL_ERROR
    CALL Logger_LogStructured
    
    XOR eax, eax
    RET
initialize_system ENDP
```

### Pattern 3: Cleanup
```asm
shutdown_system PROC
    ; Log shutdown
    LEA rcx, [rel msg_shutdown]
    MOV rdx, LOG_LEVEL_INFO
    CALL Logger_LogStructured
    
    ; Destroy engine
    MOV rcx, [g_engine]
    CMP rcx, 0
    JE skip_destroy
    
    CALL ZeroDayAgenticEngine_Destroy
    
skip_destroy:
    ; Log completion
    LEA rcx, [rel msg_shutdown_complete]
    MOV rdx, LOG_LEVEL_INFO
    CALL Logger_LogStructured
    
    RET
shutdown_system ENDP
```

---

## File-Specific Guidelines

### main_masm.asm (2,100 lines)
- ✅ Add engine create at startup
- ✅ Add extensive logging (main flow, events, errors)
- ✅ Add engine destroy at shutdown
- Target: 50+ logging calls

### agentic_masm.asm (1,200 lines)
- ✅ Add logging around each tool
- ✅ Add latency measurement around tool execution
- ✅ Add AgenticEngine_ExecuteTask calls
- ✅ Add error logging
- Target: 100+ logging calls, 20+ latency measurements

### ml_masm.asm (1,800 lines)
- ✅ Add logging around model ops
- ✅ Add UniversalModelRouter_SelectModel calls
- ✅ Add inference latency measurement
- ✅ Add Metrics_RecordHistogramMission for inference
- Target: 30+ logging calls, 10+ latency measurements

### unified_masm_hotpatch.asm (900 lines)
- ✅ Add logging for patch application
- ✅ Add latency measurement around patches
- Target: 20+ logging calls, 10+ latency measurements

### logging.asm (600 lines)
- ⚠️ CAREFUL: Recursive logging possible
- ✅ Add logging only on entry/exit
- ✅ Use conditional compilation if needed
- Target: <10 logging calls (risk of recursion)

### asm_memory.asm (500 lines)
- ✅ Add logging on allocation/deallocation
- ✅ Add latency measurement
- Target: 20+ logging calls

### asm_string.asm (400 lines)
- ✅ Add logging on operation entry
- ✅ Add latency measurement
- Target: 15+ logging calls

### agent_orchestrator_main.asm (1,500 lines)
- ✅ Add logging for mission transitions
- ✅ Add engine creation/destruction
- ✅ Add metrics counting
- Target: 40+ logging calls

### unified_hotpatch_manager.asm (1,000+ lines)
- ✅ Add logging for patch coordination
- ✅ Add latency measurement
- Target: 30+ logging calls

---

## Compilation Checklist

After adding function calls to a file:

- [ ] All function calls use correct parameter registers (rcx, rdx, r8, r9)
- [ ] All functions called are declared via `include masm/masm_master_include.asm`
- [ ] All message strings are NULL-terminated
- [ ] All LEA instructions properly reference messages
- [ ] Log levels are valid (0-3)
- [ ] No infinite loops in logging
- [ ] File compiles without errors
- [ ] File compiles without unresolved symbol warnings
- [ ] No changes to existing logic (only additions)

---

## Quick Lookup Table

| Function | Files | Purpose | Calls/File |
|----------|-------|---------|-----------|
| Logger_LogStructured | ALL | Logging | 50-100 |
| QueryPerformanceCounter | 5 | Timing | 10-20 |
| Metrics_RecordLatency | 5 | Record timing | 10-20 |
| ZeroDayAgenticEngine_Create | 3 | Init | 1 |
| ZeroDayAgenticEngine_Destroy | 2 | Cleanup | 1 |
| AgenticEngine_ExecuteTask | 2 | Task exec | 5-10 |
| UniversalModelRouter_SelectModel | 1 | Model selection | 3-5 |
| Metrics_RecordHistogramMission | 2 | Complex metrics | 5 |
| Metrics_IncrementMissionCounter | 1 | Mission tracking | 5-10 |

**Total Calls to Add**: ~200-300 across all 9 files

---

**Phase 2 Reference Guide**: Use this document while editing each Priority 1 file. Each function section provides exact syntax, usage patterns, and file-specific guidance.

Last Updated: Phase 2 Planning
