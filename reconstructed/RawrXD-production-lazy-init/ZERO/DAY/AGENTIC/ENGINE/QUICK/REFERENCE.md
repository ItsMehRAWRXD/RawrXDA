# Zero-Day Agentic Engine - Quick Reference Guide

## API Overview

### Engine Lifecycle

```asm
; Create engine instance
rcx = router*
rdx = tools*
r8  = planner*
r9  = callbacks*
CALL ZeroDayAgenticEngine_Create
; Returns: rax = engine*

; Destroy engine instance  
rcx = engine*
CALL ZeroDayAgenticEngine_Destroy
```

### Mission Management

```asm
; Start mission
rcx = engine*
rdx = goal_string*
CALL ZeroDayAgenticEngine_StartMission
; Returns: rax = mission_id* or NULL

; Abort mission
rcx = engine*
CALL ZeroDayAgenticEngine_AbortMission
; Returns: rax = 1 (sent) or 0 (no mission)

; Get mission state
rcx = engine*
CALL ZeroDayAgenticEngine_GetMissionState
; Returns: rax = MISSION_STATE_*

; Get mission ID
rcx = engine*
CALL ZeroDayAgenticEngine_GetMissionId
; Returns: rax = mission_id*
```

---

## Constants

### Mission States
```asm
MISSION_STATE_IDLE      = 0     ; No mission
MISSION_STATE_RUNNING   = 1     ; Mission executing
MISSION_STATE_ABORTED   = 2     ; Mission aborted
MISSION_STATE_COMPLETE  = 3     ; Mission succeeded
MISSION_STATE_ERROR     = 4     ; Mission failed
```

### Signal Types
```asm
SIGNAL_TYPE_STREAM      = 1     ; Progress/streaming
SIGNAL_TYPE_COMPLETE    = 2     ; Mission complete
SIGNAL_TYPE_ERROR       = 3     ; Mission error
```

### Logging Levels
```asm
LOG_LEVEL_DEBUG         = 0     ; Detailed diagnostics
LOG_LEVEL_INFO          = 1     ; General info
LOG_LEVEL_WARN          = 2     ; Warnings
LOG_LEVEL_ERROR         = 3     ; Errors
ACTIVE_LOG_LEVEL        = LOG_LEVEL_DEBUG  ; Current filter
```

---

## Common Usage Patterns

### Creating an Engine with Callbacks

```asm
; Setup callback pointers
; Allocate or reference callback functions:
; - agentStream_cb: void(__fastcall)(LPCSTR message)
; - agentComplete_cb: void(__fastcall)(LPCSTR result)
; - agentError_cb: void(__fastcall)(LPCSTR error)

; Create struct with 3 QWORD slots for callbacks
; Place in r9 register

MOV rcx, router_ptr
MOV rdx, tools_ptr
MOV r8, planner_ptr
MOV r9, callbacks_struct_ptr
CALL ZeroDayAgenticEngine_Create

; Engine is ready for use
```

### Starting a Mission

```asm
; Assuming we have a valid engine
MOV rcx, engine_ptr
LEA rdx, [goal_string]  ; "Analyze customer sentiment"
CALL ZeroDayAgenticEngine_StartMission

; Check result
TEST rax, rax
JZ mission_failed        ; NULL return = error
MOV mission_id, rax      ; Save mission ID
```

### Monitoring Mission Progress

```asm
; Poll mission state
MOV rcx, engine_ptr
CALL ZeroDayAgenticEngine_GetMissionState

; Check state
CMP rax, MISSION_STATE_COMPLETE
JE mission_done
CMP rax, MISSION_STATE_ERROR
JE mission_error
CMP rax, MISSION_STATE_RUNNING
JE mission_in_progress
```

### Graceful Shutdown

```asm
; Abort if running
MOV rcx, engine_ptr
CALL ZeroDayAgenticEngine_AbortMission

; Wait for mission to terminate (implementation-specific)
; ...wait for callback or poll state...

; Destroy engine
CALL ZeroDayAgenticEngine_Destroy
```

---

## Error Handling

### Error Cases Summary

| Operation | Error | Return | Recovery |
|-----------|-------|--------|----------|
| Create | Alloc fail | NULL | Check memory |
| StartMission | NULL engine | NULL | Validate engine |
| StartMission | NULL goal | NULL | Provide goal |
| StartMission | Already running | NULL | Wait or abort |
| GetMissionState | NULL engine | IDLE | Validate engine |

### Error Checking Pattern

```asm
; After Create
TEST rax, rax
JZ handle_create_error

; After StartMission
TEST rax, rax
JZ handle_mission_error

; Always validate before use
CALL ZeroDayAgenticEngine_ValidateInstance
TEST rax, rax
JZ handle_invalid_engine
```

---

## Logging

### Structured Logging Calls

Logging is integrated at these points:
- Engine creation/destruction
- Mission start/complete/abort/error
- Execution start/success/failure
- Planner unavailable
- Allocation failures
- Invalid inputs

### Log Levels in Use

| Level | When Used | Examples |
|-------|-----------|----------|
| DEBUG | Entry/exit points | "Creating engine", "Validating instance" |
| INFO | Normal operations | "Mission started", "Execution succeeded" |
| WARN | Unexpected but handled | "Mission already running", "No planner" |
| ERROR | Failures and errors | "Allocation failed", "Execution failed" |

### Adjusting Log Level

```asm
; In zero_day_agentic_engine.asm:
ACTIVE_LOG_LEVEL = LOG_LEVEL_DEBUG      ; Debug: all messages
ACTIVE_LOG_LEVEL = LOG_LEVEL_INFO       ; Info: skip debug
ACTIVE_LOG_LEVEL = LOG_LEVEL_WARN       ; Production: warnings only
ACTIVE_LOG_LEVEL = LOG_LEVEL_ERROR      ; Minimal: errors only
```

---

## Performance Instrumentation

### Automatic Timing

The engine automatically measures:
- Mission execution duration (logged as histogram)
- Planner invocation latency
- Signal emission latency

Results are recorded via:
```asm
Metrics_RecordHistogramMission
```

### Accessing Metrics

Mission execution duration is available from the metrics subsystem:
- Metric name: "agent.mission.ms"
- Value: Duration in milliseconds
- Type: Histogram/latency

---

## Thread Safety Guarantees

### Safe Operations
✅ **Concurrent Create/Destroy**: Different engine instances  
✅ **Concurrent StartMission**: Different engine instances  
✅ **Concurrent State Queries**: GetMissionState, GetMissionId  
✅ **Concurrent Abort**: Safe from any thread  

### Not Thread-Safe
❌ **Same engine, concurrent operations**: Serialize at caller level  
❌ **Callbacks from multiple threads**: Callbacks must be thread-safe  
❌ **Destroy while mission running**: Abort first, wait for completion  

---

## Memory Management

### Allocation

The engine allocates:
- **Engine struct**: 24 bytes
- **Impl struct**: 128 bytes
- **Total**: 152 bytes per instance

### Cleanup

Destroy function frees:
- All subsystem pointers
- Implementation structure
- Engine structure
- No memory leaks (RAII guaranteed)

---

## Configuration

### Modifiable Parameters

Edit these constants in the ASM file:

```asm
; Logging
ACTIVE_LOG_LEVEL        = LOG_LEVEL_DEBUG

; Performance
THREAD_STACK_SIZE       = 65536         ; Worker thread stack

; Mission States
MISSION_STATE_IDLE      = 0
MISSION_STATE_RUNNING   = 1
; ... etc
```

### Environment Variables

For production deployment:
- Set log level via ACTIVE_LOG_LEVEL
- Adjust stack size for system resources
- Configure callback functions at runtime

---

## Debugging Tips

### Enable Full Logging
```asm
ACTIVE_LOG_LEVEL = LOG_LEVEL_DEBUG
```

### Check Mission State
```asm
CALL ZeroDayAgenticEngine_GetMissionState
; rax contains current state
```

### Validate Instance
```asm
CALL ZeroDayAgenticEngine_ValidateInstance
; rax = 1 (valid) or 0 (invalid)
```

### Common Issues

| Issue | Check | Solution |
|-------|-------|----------|
| Mission won't start | Log messages | Check goal string and engine state |
| Abort not working | Mission state | Ensure mission is RUNNING |
| Callbacks not firing | Callback pointers | Verify callbacks provided at creation |
| Memory leak | Allocation count | Ensure Destroy is called |

---

## Migration from C++

If migrating from C++ implementation:

1. **Replace function calls** - Use CALL with parameters in registers
2. **Error handling** - Check rax/NULL returns instead of exceptions
3. **Callbacks** - Register function pointers instead of std::function
4. **State checks** - Use GetMissionState instead of member access
5. **Logging** - Use built-in logging (Logger_* functions)

---

## Additional Resources

See `ZERO_DAY_AGENTIC_ENGINE_IMPROVEMENTS.md` for:
- Comprehensive improvement documentation
- Architecture details
- Testing recommendations
- Performance tuning
- Future enhancement plans

---

**Last Updated**: December 30, 2025  
**Status**: Production-Ready ✅

