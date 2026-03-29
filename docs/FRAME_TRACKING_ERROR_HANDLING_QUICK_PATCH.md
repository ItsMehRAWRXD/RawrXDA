# Frame Tracking & Error Handling - Quick Integration Patch

This file contains specific code patches to apply to `Win32IDE_Debugger.cpp` to enable frame tracking and error handling.

## Patch 1: Add Includes at Top of File

**Location:** About line 20 (after existing includes)

```cpp
#include "debugger_frame_tracker.h"
#include "debugger_error_handler.h"
```

---

## Patch 2: Update updateCallStack() Method

**Location:** Win32IDE_Debugger.cpp, method `Win32IDE::updateCallStack()` (~line 1750)

**REPLACE THIS BLOCK:**
```cpp
    // Fetch real stack frames from engine when paused
    if (m_debuggerAttached && m_debuggerPaused) {
        auto& engine = NativeDebuggerEngine::Instance();
        std::vector<NativeStackFrame> nativeFrames;
        DebugResult r = engine.walkStack(nativeFrames, 256);

        if (r.success && !nativeFrames.empty()) {
            // Convert NativeStackFrame → our UI StackFrame type
            m_callStack.clear();
            for (const auto& nf : nativeFrames) {
                StackFrame sf;
                sf.function = nf.function;
                if (sf.function.empty()) {
                    // Show address if no symbol
                    std::ostringstream addrStr;
                    addrStr << "0x" << std::hex << nf.instructionPtr;
                    sf.function = addrStr.str();
                }
                sf.file = nf.sourceFile;
                sf.line = nf.sourceLine;
                sf.locals = nf.locals;
                m_callStack.push_back(sf);
            }

            // Update debugger current position from top frame and selected frame index
            if (!m_callStack.empty()) {
                m_selectedStackFrameIndex = 0;
                const auto& top = m_callStack.front();
                if (!top.file.empty()) {
                    m_debuggerCurrentFile = top.file;
                    m_debuggerCurrentLine = top.line;
                    highlightDebuggerLine(top.file, top.line);
                }
            }
        }
    }
```

**WITH THIS CODE:**
```cpp
    // Fetch real stack frames from engine when paused
    if (m_debuggerAttached && m_debuggerPaused) {
        auto& engine = NativeDebuggerEngine::Instance();
        auto& frameTracker = DebuggerFrameTrackerInstance::instance();
        auto& errorHandler = DebuggerErrorHandlerInstance::instance();
        
        errorHandler.pushContext("updateCallStack");
        
        try {
            std::vector<NativeStackFrame> nativeFrames;
            DebugResult r = engine.walkStack(nativeFrames, 256);
            
            // Error handling for failed walkStack
            if (!r.success) {
                DebuggerError err;
                err.type = DebuggerErrorType::FrameStackWalkFailed;
                err.severity = ErrorSeverity::Error;
                err.message = "Failed to walk the stack: " + 
                    (r.error ? r.error->message : "Unknown error");
                err.isRecoverable = true;
                err.suggestedStrategy = RecoveryStrategy::Retry;
                errorHandler.reportError(err);
                errorHandler.popContext();
                return;
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
                err.suggestedStrategy = RecoveryStrategy::Fallback;
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
                
                // Try to recover invalid frames
                for (size_t i = 0; i < frameTracker.frameCount(); ++i) {
                    if (!frameTracker.getFrame(i) || 
                        frameTracker.getFrame(i)->state != FrameState::Valid) {
                        frameTracker.attemptFrameRecovery(i);
                    }
                }
            }
            
            // Update old m_callStack for backward compatibility
            m_callStack.clear();
            auto displayFrames = frameTracker.getDisplayFrames();
            for (const auto& ef : displayFrames) {
                StackFrame sf;
                sf.function = ef.function;
                sf.file = ef.file;
                sf.line = ef.line;
                sf.dapFrameId = ef.dapFrameId;
                sf.locals = ef.locals;
                m_callStack.push_back(sf);
            }
            
            // Update debugger current position from top frame
            if (!m_callStack.empty()) {
                m_selectedStackFrameIndex = 0;
                const auto& top = m_callStack.front();
                if (!top.file.empty()) {
                    m_debuggerCurrentFile = top.file;
                    m_debuggerCurrentLine = top.line;
                    highlightDebuggerLine(top.file, top.line);
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

---

## Patch 3: Update selectStackFrame() Method

**Location:** Win32IDE_Debugger.cpp, method `Win32IDE::selectStackFrame()` (~line 1830)

**REPLACE THIS:**
```cpp
void Win32IDE::selectStackFrame(int index)
{
    if (index < 0 || (size_t)index >= m_callStack.size()) return;

    m_selectedStackFrameIndex = index;
    const StackFrame& frame = m_callStack[static_cast<size_t>(index)];

    // For DAP sessions, switch the active frame context so subsequent
    // variable/watch evaluations use the correct frame.
    if (auto dapSession = getDapSession(this)) {
        if (frame.dapFrameId >= 0) {  // 0 is a valid DAP frame ID
            dapSession->currentFrameId = frame.dapFrameId;
        }
    } else {
        // Native engine: m_selectedStackFrameIndex is read directly by updateVariables()
        // via engine.getFrameLocals(frameIndex, ...) — no additional call needed.
    }

    // Navigate the editor to the selected frame's source location.
    if (!frame.file.empty() && frame.line > 0) {
        m_debuggerCurrentFile = frame.file;
        m_debuggerCurrentLine = frame.line;
        highlightDebuggerLine(frame.file, frame.line);
    }
}
```

**WITH THIS:**
```cpp
void Win32IDE::selectStackFrame(int index)
{
    auto& frameTracker = DebuggerFrameTrackerInstance::instance();
    auto& errorHandler = DebuggerErrorHandlerInstance::instance();
    
    errorHandler.pushContext("selectStackFrame");
    
    // Validate index with error reporting
    if (!errorHandler.validateIndex(static_cast<size_t>(index), frameTracker.frameCount(), 
                                     "selectStackFrame")) {
        errorHandler.popContext();
        return;
    }
    
    // Try to select the frame
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
    
    // Update backward-compatible m_callStack state
    m_selectedStackFrameIndex = index;
    
    // Navigate the editor to the selected frame's source location
    if (!frame->file.empty() && frame->line > 0) {
        m_debuggerCurrentFile = frame->file;
        m_debuggerCurrentLine = frame->line;
        highlightDebuggerLine(frame->file, frame->line);
    }
    
    // For DAP sessions, update the frame context
    if (auto dapSession = getDapSession(this)) {
        if (frame->dapFrameId >= 0) {
            dapSession->currentFrameId = frame->dapFrameId;
        }
    }
    
    // Trigger variable update for the new frame
    updateVariables();
    
    errorHandler.popContext();
}
```

---

## Patch 4: Add Frame Navigation Methods

**Location:** Win32IDE_Debugger.cpp, add these new methods near selectStackFrame()

```cpp
void Win32IDE::navigateFrameBackward()
{
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
        updateCallStack();
    } else {
        errorHandler.reportError(
            DebuggerErrorType::UnknownError,
            ErrorSeverity::Warning,
            "Frame navigation backward failed",
            "navigateFrameBackward"
        );
    }
}

void Win32IDE::navigateFrameForward()
{
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
        updateCallStack();
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

---

## Patch 5: Add Error Monitoring Method

**Location:** Win32IDE_Debugger.cpp, add this new method (can go after frame navigation methods)

```cpp
void Win32IDE::updateDebuggerErrorStatus()
{
    auto& errorHandler = DebuggerErrorHandlerInstance::instance();
    auto stats = errorHandler.getStatistics();
    
    if (stats.totalErrors == 0) {
        SetWindowTextA(m_hwndDebuggerStatus, "Ready");
        return;
    }
    
    std::ostringstream status;
    status << "Errors: " << stats.totalErrors 
           << " | Recovered: " << stats.recoveredErrors;
    
    SetWindowTextA(m_hwndDebuggerStatus, status.str().c_str());
}
```

---

## Patch 6: Update Win32IDE.h Class Declaration

**Location:** Win32IDE.h, in the debugger methods section (around line 2360)

**ADD THESE METHOD DECLARATIONS:**
```cpp
    void navigateFrameBackward();
    void navigateFrameForward();
    void updateDebuggerErrorStatus();
```

**ADD THESE STATE VARIABLES:**
```cpp
    bool m_frameTrackingEnabled = true;
    bool m_errorHandlingEnabled = true;
```

---

## Patch 7: Update CMakeLists.txt

**Location:** d:\rawrxd\CMakeLists.txt or wherever Win32IDE sources are added

**ADD THESE FILE REFERENCES:**
```cmake
    src/win32app/debugger_frame_tracker.cpp
    src/win32app/debugger_error_handler.cpp
```

---

## Verification Checklist

After applying patches:

- [ ] `debugger_frame_tracker.h` exists at `d:\rawrxd\src\win32app\debugger_frame_tracker.h`
- [ ] `debugger_frame_tracker.cpp` exists
- [ ] `debugger_error_handler.h` exists at `d:\rawrxd\src\win32app\debugger_error_handler.h`
- [ ] `debugger_error_handler.cpp` exists
- [ ] Includes added to Win32IDE_Debugger.cpp (grep for "debugger_frame_tracker.h")
- [ ] `updateCallStack()` has frame tracker and error handler code
- [ ] `selectStackFrame()` has validation code
- [ ] New frame navigation methods added
- [ ] Method declarations added to Win32IDE.h
- [ ] CMakeLists.txt updated with new .cpp files
- [ ] Project compiles without errors
- [ ] Debugger attaches and stack displays correctly
- [ ] Selecting frames navigates editor correctly
- [ ] Error status shows in status bar

---

## Rollback (if needed)

Simply revert the matching sections above and remove the includes. The system is designed to be optional.

---

## Testing Script

After integration, test with:

```cpp
// Test frame tracker
auto& tracker = DebuggerFrameTrackerInstance::instance();
auto displayFrames = tracker.getDisplayFrames();
assert(displayFrames.size() > 0);
assert(tracker.getCurrentFrameIndex() == 0);

// Test error handler
auto& errorHandler = DebuggerErrorHandlerInstance::instance();
auto stats = errorHandler.getStatistics();
assert(stats.totalErrors >= 0);

// Test frame selection
tracker.selectFrame(0);
assert(tracker.getCurrentFrameIndex() == 0);

// Test error reporting
errorHandler.reportError(DebuggerErrorType::UnknownError,
                         ErrorSeverity::Info,
                         "Test error");
assert(errorHandler.getStatistics().totalErrors > 0);
```

