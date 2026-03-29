# Frame Tracking & Error Handling Integration Guide

## Overview
This document describes how to integrate the new **Frame Tracking System** and **Error Handler System** into the existing debugger code (`Win32IDE_Debugger.cpp`).

## Architecture

### Core Components
1. **DebuggerFrameTracker** - Manages call stack frames with validation, recovery, and navigation
2. **DebuggerErrorHandler** - Comprehensive error reporting, recovery, and statistics
3. **Integration Points** - Where these systems connect to existing debugger code

## Step-by-Step Integration

### Step 1: Add Includes to Win32IDE_Debugger.cpp

```cpp
#include "debugger_frame_tracker.h"
#include "debugger_error_handler.h"
```

### Step 2: Update the updateCallStack() Method

**Current Code Location**: `Win32IDE_Debugger.cpp` ~line 1750

**Current Implementation:**
```cpp
// Existing code (BEFORE integration)
if (m_debuggerAttached && m_debuggerPaused) {
    auto& engine = NativeDebuggerEngine::Instance();
    std::vector<NativeStackFrame> nativeFrames;
    DebugResult r = engine.walkStack(nativeFrames, 256);

    if (r.success && !nativeFrames.empty()) {
        m_callStack.clear();
        for (const auto& nf : nativeFrames) {
            StackFrame sf;
            // ... conversion code ...
        }
    }
}
```

**New Implementation (WITH error handling & frame tracking):**
```cpp
if (m_debuggerAttached && m_debuggerPaused) {
    auto& engine = NativeDebuggerEngine::Instance();
    auto& frameTracker = DebuggerFrameTrackerInstance::instance();
    auto& errorHandler = DebuggerErrorHandlerInstance::instance();
    
    errorHandler.pushContext("updateCallStack");
    
    try {
        std::vector<NativeStackFrame> nativeFrames;
        DebugResult r = engine.walkStack(nativeFrames, 256);
        
        if (!r.success) {
            DebuggerError err;
            err.type = DebuggerErrorType::FrameStackWalkFailed;
            err.severity = ErrorSeverity::Error;
            err.message = "Failed to walk the stack: " + (r.error ? r.error->message : "Unknown error");
            err.isRecoverable = true;
            err.suggestedStrategy = RecoveryStrategy::Retry;
            
            errorHandler.reportError(err);
            errorHandler.popContext();
            return;  // or retry
        }
        
        if (nativeFrames.empty()) {
            errorHandler.popContext();
            return;
        }
        
        // Use the new frame tracking system
        size_t frameCount = frameTracker.updateFromNativeFrames(nativeFrames);
        
        if (frameCount == 0) {
            DebuggerError err;
            err.type = DebuggerErrorType::FrameDataCorrupted;
            err.severity = ErrorSeverity::Warning;
            err.message = "Stack walk returned frames but none passed validation";
            err.isRecoverable = true;
            
            errorHandler.reportError(err);
        }
        
        // Validate the entire stack
        if (!frameTracker.validateStack()) {
            errorHandler.reportError(
                DebuggerErrorType::FrameDataCorrupted,
                ErrorSeverity::Warning,
                "Some frames failed validation; attempting recovery",
                "updateCallStack"
            );
            
            // Try to recover
            for (size_t i = 0; i < frameTracker.frameCount(); ++i) {
                frameTracker.attemptFrameRecovery(i);
            }
        }
        
        errorHandler.popContext();
        
    } catch (const std::exception& ex) {
        DebuggerError err;
        err.type = DebuggerErrorType::FrameStackWalkFailed;
        err.severity = ErrorSeverity::Critical;
        err.message = std::string("Exception during stack walk: ") + ex.what();
        err.isRecoverable = false;
        
        errorHandler.reportError(err);
        errorHandler.popContext();
    }
}
```

### Step 3: Update the UI Rendering (ListView Update)

**Location**: After frame collection in `updateCallStack()`

**New Code:**
```cpp
// Get display-ready frames from tracker
auto displayFrames = frameTracker.getDisplayFrames();
ListView_DeleteAllItems(m_hwndDebuggerStackTrace);

if (displayFrames.empty()) {
    LVITEMA lvi;
    lvi.mask = LVIF_TEXT;
    lvi.iItem = 0;
    lvi.iSubItem = 0;
    lvi.pszText = const_cast<char*>("Attach and break to inspect call stack");
    ListView_InsertItem(m_hwndDebuggerStackTrace, &lvi);
} else {
    for (size_t i = 0; i < displayFrames.size(); ++i) {
        const auto& frame = displayFrames[i];
        
        LVITEMA lvi;
        lvi.mask = LVIF_TEXT | LVIF_PARAM;  // Include frame tracking data
        lvi.iItem = static_cast<int>(i);
        lvi.iSubItem = 0;
        
        // Column 0: Function name
        lvi.pszText = const_cast<char*>(frame.function.c_str());
        lvi.lParam = static_cast<LPARAM>(i);  // Store frame index
        ListView_InsertItem(m_hwndDebuggerStackTrace, &lvi);
        
        // Column 1: File name
        lvi.iSubItem = 1;
        lvi.pszText = const_cast<char*>(frame.file.c_str());
        ListView_SetItem(m_hwndDebuggerStackTrace, &lvi);
        
        // Column 2: Line number
        lvi.iSubItem = 2;
        std::string line_str = std::to_string(frame.line);
        lvi.pszText = const_cast<char*>(line_str.c_str());
        ListView_SetItem(m_hwndDebuggerStackTrace, &lvi);
        
        // Column 3: State (if invalid, show warning)
        if (frame.state != FrameState::Valid) {
            lvi.iSubItem = 3;
            lvi.pszText = const_cast<char*>(DebuggerError::errorTypeToString(
                frame.state == FrameState::InvalidAddress
                    ? DebuggerErrorType::FrameIndexOutOfRange
                    : DebuggerErrorType::FrameDataCorrupted
            ).c_str());
            ListView_SetItem(m_hwndDebuggerStackTrace, &lvi);
        }
    }
}
```

### Step 4: Update selectStackFrame() Method

**Location**: ~line 1830

**Before:**
```cpp
void Win32IDE::selectStackFrame(int index) {
    if (index < 0 || (size_t)index >= m_callStack.size()) return;
    m_selectedStackFrameIndex = index;
    const StackFrame& frame = m_callStack[static_cast<size_t>(index)];
    // ... rest of code ...
}
```

**After (with error handling):**
```cpp
void Win32IDE::selectStackFrame(int index) {
    auto& frameTracker = DebuggerFrameTrackerInstance::instance();
    auto& errorHandler = DebuggerErrorHandlerInstance::instance();
    
    errorHandler.pushContext("selectStackFrame");
    
    // Validate index with error reports
    if (!errorHandler.validateIndex(static_cast<size_t>(index), frameTracker.frameCount(), "selectStackFrame")) {
        errorHandler.popContext();
        return;
    }
    
    if (!frameTracker.selectFrame(static_cast<size_t>(index))) {
        errorHandler.reportError(
            DebuggerErrorType::FrameIndexOutOfRange,
            ErrorSeverity::Error,
            "Failed to select frame at index " + std::to_string(index),
            "selectStackFrame"
        );
        errorHandler.popContext();
        return;
    }
    
    const auto* frame = frameTracker.getCurrentFrame();
    if (!frame) {
        errorHandler.reportError(
            DebuggerErrorType::FrameDataCorrupted,
            ErrorSeverity::Error,
            "Selected frame is null",
            "selectStackFrame"
        );
        errorHandler.popContext();
        return;
    }
    
    // Navigate to frame location if valid
    if (!frame->file.empty() && frame->line > 0) {
        m_debuggerCurrentFile = frame->file;
        m_debuggerCurrentLine = frame->line;
        highlightDebuggerLine(frame->file, frame->line);
    }
    
    m_selectedStackFrameIndex = index;
    
    // Update variables/locals from the selected frame
    updateVariablesFromFrame();
    
    errorHandler.popContext();
}
```

### Step 5: Add Frame Navigation Methods

**New methods to add to Win32IDE class:**

```cpp
void Win32IDE::navigateFrameBackward() {
    auto& frameTracker = DebuggerFrameTrackerInstance::instance();
    auto& errorHandler = DebuggerErrorHandlerInstance::instance();
    
    if (!frameTracker.canNavigateBack()) {
        errorHandler.reportError(
            DebuggerErrorType::UnknownError,
            ErrorSeverity::Info,
            "Cannot navigate to previous frame",
            "navigateFrameBackward"
        );
        return;
    }
    
    if (frameTracker.navigateFrameBack()) {
        updateCallStack();  // Refresh UI
    } else {
        errorHandler.reportError(
            DebuggerErrorType::UnknownError,
            ErrorSeverity::Warning,
            "Frame navigation backward failed",
            "navigateFrameBackward"
        );
    }
}

void Win32IDE::navigateFrameForward() {
    auto& frameTracker = DebuggerFrameTrackerInstance::instance();
    auto& errorHandler = DebuggerErrorHandlerInstance::instance();
    
    if (!frameTracker.canNavigateForward()) {
        errorHandler.reportError(
            DebuggerErrorType::UnknownError,
            ErrorSeverity::Info,
            "Cannot navigate to next frame",
            "navigateFrameForward"
        );
        return;
    }
    
    if (frameTracker.navigateFrameForward()) {
        updateCallStack();  // Refresh UI
    } else {
        errorHandler.reportError(
            DebuggerErrorType::UnknownError,
            ErrorSeverity::Warning,
            "Frame navigation forward failed",
            "navigateFrameForward"
        );
    }
}
```

### Step 6: Update updateVariables() to Use Frame Tracker

**Location**: The method that populates the variables panel

**Enhancement:**
```cpp
void Win32IDE::updateVariables() {
    auto& frameTracker = DebuggerFrameTrackerInstance::instance();
    auto& errorHandler = DebuggerErrorHandlerInstance::instance();
    
    errorHandler.pushContext("updateVariables");
    
    const auto* frame = frameTracker.getCurrentFrame();
    if (!frame) {
        errorHandler.reportError(
            DebuggerErrorType::FrameLocalsUnavailable,
            ErrorSeverity::Warning,
            "No frame selected",
            "updateVariables"
        );
        errorHandler.popContext();
        return;
    }
    
    // Get frame locals safely
    std::map<std::string, std::string> locals;
    if (!frameTracker.getFrameLocals(locals)) {
        errorHandler.reportError(
            DebuggerErrorType::FrameLocalsUnavailable,
            ErrorSeverity::Warning,
            "Failed to retrieve frame locals",
            "updateVariables"
        );
    }
    
    // Render locals to UI
    TreeView_DeleteAllItems(m_hwndDebuggerVariables);
    for (const auto& [name, value] : locals) {
        TVINSERTSTRUCTA tvi;
        tvi.hParent = TVI_ROOT;
        tvi.hInsertAfter = TVI_LAST;
        tvi.itemex.mask = TVIF_TEXT;
        tvi.itemex.pszText = const_cast<char*>(
            (name + " = " + value).c_str()
        );
        TreeView_InsertItem(m_hwndDebuggerVariables, &tvi);
    }
    
    errorHandler.popContext();
}
```

### Step 7: Add Error Monitoring UI

**Create a new error panel or status indicator:**

```cpp
void Win32IDE::updateDebuggerErrorStatus() {
    auto& errorHandler = DebuggerErrorHandlerInstance::instance();
    auto stats = errorHandler.getStatistics();
    
    // Update status bar or error panel
    std::ostringstream status;
    status << "Errors: " << stats.totalErrors 
           << " | Recovered: " << stats.recoveredErrors
           << " | Active: " << stats.unrecoveredErrors;
    
    // Send to status bar
    SetWindowTextA(m_hwndDebuggerStatus, status.str().c_str());
}
```

### Step 8: Add Diagnostics Logging

**When debugger pauses:**

```cpp
void Win32IDE::onDebuggerPaused() {
    auto& frameTracker = DebuggerFrameTrackerInstance::instance();
    auto& errorHandler = DebuggerErrorHandlerInstance::instance();
    
    // Log diagnostics for debugging
    std::string diagnostics = frameTracker.getDiagnosticsReport();
    OutputDebugStringA(diagnostics.c_str());
    
    // Also log to error handler
    if (errorHandler.getStatistics().totalErrors > 0) {
        OutputDebugStringA(errorHandler.getErrorReport().c_str());
    }
}
```

## Header Declarations

Add these to the Win32IDE class header (`Win32IDE.h`):

```cpp
// In the debwagger methods section:
void navigateFrameBackward();
void navigateFrameForward();
void updateDebuggerErrorStatus();

// Frame/error state
bool m_frameTrackingEnabled = true;
bool m_errorHandlingEnabled = true;
```

## Key Integration Points Summary

| Location | Change Type | Priority |
|----------|-------------|----------|
| `updateCallStack()` | Use new frame tracker + error handler | P0 |
| `selectStackFrame()` | Add validation + error reporting | P0 |
| `updateVariables()` | Use frame tracker's `getFrameLocals()` | P1 |
| UI Rendering | Add frame state indicators | P1 |
| Error Monitoring | Add status display for errors | P2 |
| Diagnostics | Log frame/error diagnostics | P2 |

## Testing Checklist

- [ ] Frame collection returns correct count
- [ ] Invalid frames are properly marked and handled
- [ ] Frame selection works without crashes
- [ ] Error handler logs all failures
- [ ] Error recovery strategies execute correctly
- [ ] Frame navigation (back/forward) works
- [ ] Variables display from selected frame
- [ ] Error statistics are accurate

## Configuration Options

These can be added to the debugger configuration:

```json
{
  "debugger": {
    "maxFrameHistorySize": 100,
    "frameValidationLevel": "strict",  // or "lenient"
    "errorRecoveryEnabled": true,
    "errorLoggingLevel": "warning",     // or "all", "error", "critical"
    "autoRetryOnFailure": true,
    "maxRetries": 3
  }
}
```

## Performance Considerations

- Frame tracking uses vectors (O(1) access, O(n) iteration)
- Error handler maintains bounded history (max 100 entries)
- Frame validation is done incrementally
- Frame context callbacks are optional and can be disabled
- Consider using frame tracker in a background thread for large stacks

## Future Enhancements

1. Frame cache invalidation on module load/unload
2. Async frame loading for very deep stacks
3. Frame filtering (hide internal/thunk frames)
4. Frame annotations (custom metadata per frame)
5. Frame diff tracking (what changed between pauses)
6. Remote frame debugging support (DAP integration)

