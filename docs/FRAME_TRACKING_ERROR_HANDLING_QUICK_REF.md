# Frame Tracking & Error Handling - Quick Reference Card

## File Locations

| Component | Header | Implementation |
|-----------|--------|-----------------|
| Frame Tracker | `src/win32app/debugger_frame_tracker.h` | `src/win32app/debugger_frame_tracker.cpp` |
| Error Handler | `src/win32app/debugger_error_handler.h` | `src/win32app/debugger_error_handler.cpp` |

## Singleton Access

```cpp
// Get instances (thread-safe, lazy initialized)
auto& frameTracker = DebuggerFrameTrackerInstance::instance();
auto& errorHandler = DebuggerErrorHandlerInstance::instance();
```

## Frame Tracker

### Essential Methods

| Method | Purpose | Returns |
|--------|---------|---------|
| `updateFromNativeFrames()` | Populate from debugger | Frame count |
| `validateStack()` | Validate all frames | bool |
| `selectFrame(index)` | Set current frame | bool |
| `getCurrentFrame()` | Get current frame ptr | const EnhancedStackFrame* |
| `getFrameLocals()` | Get vars for frame | bool |
| `frameCount()` | Get frame count | size_t |
| `navigateFrameBack()` | Go to previous frame | bool |
| `navigateFrameForward()` | Go to next frame | bool |
| `getDiagnosticsReport()` | Full diagnostics | string |

### Example: Select and Inspect Frame

```cpp
auto& tracker = DebuggerFrameTrackerInstance::instance();

if (tracker.selectFrame(0)) {
    const auto* frame = tracker.getCurrentFrame();
    if (frame) {
        qDebug() << "Function: " << frame->function.c_str();
        qDebug() << "File: " << frame->file.c_str();
        qDebug() << "Line: " << frame->line;
        
        std::map<std::string, std::string> locals;
        if (tracker.getFrameLocals(locals)) {
            for (const auto& [name, value] : locals) {
                qDebug() << name.c_str() << " = " << value.c_str();
            }
        }
    }
}
```

## Error Handler

### Essential Methods

| Method | Purpose | Returns |
|--------|---------|---------|
| `reportError()` | Log and handle error | bool (recoverable) |
| `attemptRecovery()` | Execute recovery | bool |
| `getStatistics()` | Get error stats | ErrorStats |
| `getRecentErrors()` | Last N errors | vector<DebuggerError> |
| `validateIndex()` | Check bounds | bool |
| `pushContext()` | Add context layer | void |
| `popContext()` | Remove context | void |

### Example: Report and Recover from Error

```cpp
auto& errorHandler = DebuggerErrorHandlerInstance::instance();

DebuggerError err;
err.type = DebuggerErrorType::FrameStackWalkFailed;
err.severity = ErrorSeverity::Error;
err.message = "Failed to walk the call stack";
err.isRecoverable = true;
err.suggestedStrategy = RecoveryStrategy::Retry;

if (errorHandler.reportError(err)) {
    qDebug() << "Error was handled/recovered";
} else {
    qDebug() << "Error unrecoverable";
}
```

### Example: Retry with Backoff

```cpp
auto result = errorHandler.retryWithBackoff([]() {
    DebuggerResult<std::vector<EnhancedStackFrame>> r;
    // try operation
    return r;
}, 3);  // Max 3 retries with exponential backoff
```

## Error Types (24 total)

```cpp
enum class DebuggerErrorType {
    // Adapter errors
    AdapterConnectionFailed, AdapterProcessCrashed, 
    AdapterCommunicationFailed, AdapterTimeout,
    
    // Frame errors  
    FrameStackWalkFailed, FrameIndexOutOfRange,
    FrameDataCorrupted, FrameLocalsUnavailable,
    FrameSymbolResolutionFailed,
    
    // And 15 more...
};
```

## Error Severity Levels

```cpp
enum class ErrorSeverity {
    Info = 0,        // Non-critical information
    Warning = 1,     // Degraded functionality
    Error = 2,       // Operation failed
    Critical = 3     // IDE may be compromised
};
```

## Recovery Strategies

```cpp
enum class RecoveryStrategy {
    None = 0,                // No recovery attempted
    Retry = 1,              // Retry with exponential backoff
    UseDefaultValue = 2,    // Substitute sensible default
    SkipOperation = 3,      // Skip and continue
    ReattachAdapter = 4,    // Reconnect debugger adapter
    RestartSession = 5,     // Restart debug session
    Fallback = 6            // Use fallback mechanism
};
```

## Frame State Validation

```cpp
enum class FrameState {
    Unknown = 0,                      // Not yet validated
    Valid = 1,                        // Passed validation
    InvalidAddress = 2,               // Address corruption
    SymbolResolutionFailed = 3,      // Can't resolve name
    LocalsRetrievalFailed = 4,       // Can't get variables
    Corrupted = 5                     // Data integrity failure
};
```

## Debugging with Error Handler

### Set Error Callback

```cpp
errorHandler.setErrorCallback([](const DebuggerError& err, bool recoverable) {
    OutputDebugStringA(("ERROR: " + err.message + "\n").c_str());
    if (recoverable) {
        OutputDebugStringA("  (Recoverable - will attempt recovery)\n");
    }
});
```

### Set Recovery Callback

```cpp
errorHandler.setRecoveryCallback([](DebuggerError& err) -> bool {
    if (err.type == DebuggerErrorType::AdapterTimeout) {
        // Custom logic: wait and retry
        Sleep(1000);
        return true;  // Successfully recovered
    }
    return false;  // Use default recovery
});
```

### Get Full Diagnostics

```cpp
auto& tracker = DebuggerFrameTrackerInstance::instance();
std::string report = tracker.getDiagnosticsReport();
OutputDebugStringA(report.c_str());

auto& errorHandler = DebuggerErrorHandlerInstance::instance();
report = errorHandler.getErrorReport();
OutputDebugStringA(report.c_str());
```

## Integration Points in Win32IDE_Debugger.cpp

| Method | What it Does | When to Call |
|--------|-------------|--------------|
| `updateCallStack()` | Populates frames, validates, recovers | When debugger stops |
| `selectStackFrame()` | Changes current frame, updates UI | On user click |
| `navigateFrameBackward()` | Go to previous frame in history | On back button |
| `navigateFrameForward()` | Go to next frame in history | On forward button |
| `updateDebuggerErrorStatus()` | Update status bar with error count | After error operations |

## Testing Checklist

- [ ] Frame tracker collects frames without crashes
- [ ] Corrupted frames are detected and marked
- [ ] Frame recovery attempts work
- [ ] Error handler reports all failures
- [ ] Retry strategy executes properly
- [ ] Error callbacks fire correctly
- [ ] Frame navigation preserves history
- [ ] UI updates when frame changes
- [ ] Status bar shows error count
- [ ] No memory leaks in error paths

## Performance Tips

- Frame validation is O(n) - OK for typical stacks
- Error history capped at 100 entries
- Frame history capped at 100 entries
- Callbacks are optional (no overhead if not registered)
- Use context stack for nested operations
- Error reports can be large - only create on demand

## Common Patterns

### Safe Frame Access
```cpp
if (auto* frame = frameTracker.getCurrentFrame()) {
    // Use frame safely
    qDebug() << frame->function.c_str();
}
```

### Error Context
```cpp
errorHandler.pushContext("myFunction");
try {
    // Do work
} catch (...) {
    // Error will include "myFunction" in context
}
errorHandler.popContext();
```

### Check Validity
```cpp
if (frame->isValid()) {
    // Frame is good to use
} else {
    // Try recovery
    frameTracker.attemptFrameRecovery(index);
}
```

