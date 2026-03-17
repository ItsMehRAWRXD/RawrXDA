# Complete RawrXD IDE Audit Plan - q8-wire to Production
**Date**: December 7, 2025
**Goal**: Fix all lazy initialization, async operations, and UI freezing issues

## 🎯 Phase 1: MainWindow Staged Initialization (Priority 1)

### Files to Audit:
- `src/qtapp/MainWindow.cpp` (4421 lines)
- `src/qtapp/MainWindow.h`

### Tasks:
1. ✅ **Constructor cleanup** - Already has QTimer::singleShot pattern
2. ⚠️ **Un-comment createVSCodeLayout()** - Currently skipped (line 128)
3. ⚠️ **Enable initializeStage1()** - Currently commented out (line 134)
4. ❌ **Create initializeStage2(), initializeStage3()** - Missing
5. ❌ **Defer all Qt widget creation** - Still in constructor

### Heavy Operations to Defer:
- `setupMenuBar()` → Already called (line 117)
- `setupToolBars()` → Already called (line 120)
- `setupStatusBar()` → Already called (line 123)
- `createVSCodeLayout()` → COMMENTED OUT (line 128)
- `initializeStage1()` → COMMENTED OUT (line 134)

---

## 🎯 Phase 2: Chat Interface Async Fix (Priority 1 - FREEZE BUG)

### Files to Audit:
- `src/chat_interface.cpp`
- `include/chat_interface.h`
- `src/qtapp/ai_chat_panel.cpp`

### Tasks:
1. ❌ **Make sendMessage() async** - Currently synchronous
2. ❌ **Add QtConcurrent::run() wrapper**
3. ❌ **Add progress spinner** - Non-blocking UI
4. ❌ **Disable send button during inference**
5. ❌ **Add streaming callbacks** - Real-time responses

### Root Cause:
```cpp
// CURRENT (BLOCKS UI):
void sendMessage() {
    QString response = m_engine->generate(prompt);  // BLOCKS HERE
    displayMessage(response);
}

// FIXED (ASYNC):
void sendMessage() {
    QtConcurrent::run([this, prompt]() {
        QString response = m_engine->generate(prompt);
        QMetaObject::invokeMethod(this, [this, response]() {
            displayMessage(response);
        }, Qt::QueuedConnection);
    });
}
```

---

## 🎯 Phase 3: Inference Engine Async Loading (Priority 1)

### Files to Audit:
- `src/qtapp/inference_engine.cpp`
- `src/qtapp/inference_engine.hpp`
- `src/agentic_engine.cpp`

### Tasks:
1. ❌ **Defer model loading to worker thread**
2. ❌ **Add loadModelAsync() method**
3. ❌ **Add progress callbacks**
4. ❌ **Lazy-init GGUF loader**
5. ❌ **Lazy-init Vulkan compute**

---

## 🎯 Phase 4: LSP Client Async Operations (Priority 2)

### Files to Audit:
- `src/lsp_client.cpp`
- `include/lsp_client.h`

### Tasks:
1. ✅ **Already uses QProcess for async** - Good
2. ⚠️ **Check if sendRequest() blocks** - Needs verification
3. ❌ **Add timeout handling** - Missing
4. ❌ **Add connection pooling** - Missing

---

## 🎯 Phase 5: Settings Dialog Lazy Init (Priority 2)

### Files to Audit:
- `src/qtapp/settings_dialog.cpp`
- `src/qtapp/widgets/settings_dialog.h`
- All 7 inner widget classes

### Tasks:
1. ❌ **Defer all QGroupBox creation**
2. ❌ **Defer all QComboBox/QSpinBox creation**
3. ❌ **Add initialize() method**
4. ❌ **Move widget creation to initialize()**

---

## 🎯 Phase 6: Security Manager & Advanced Settings (Priority 2)

### Files to Audit:
- `src/qtapp/security_manager.cpp`
- `src/qtapp/checkpoint_manager.cpp`
- `src/qtapp/ci_cd_settings.cpp`
- `src/qtapp/tokenizer_selector.cpp`

### Tasks:
1. ❌ **SecurityManager::initialize()** - Defer crypto init
2. ❌ **CheckpointManager::initialize()** - Defer file I/O
3. ❌ **CI/CD Settings::initialize()** - Defer pipeline config
4. ❌ **TokenizerSelector::initialize()** - Defer vocab loading

---

## 🎯 Phase 7: Terminal & File Browser (Priority 3)

### Files to Audit:
- `src/qtapp/TerminalWidget.cpp`
- `src/file_browser.cpp`
- `src/terminal_pool.cpp`

### Tasks:
1. ⚠️ **TerminalWidget** - Check if QProcess is lazy
2. ❌ **FileBrowser** - Defer drive enumeration
3. ❌ **TerminalPool** - Defer QProcess spawning

---

## 🎯 Phase 8: Build & Test (Final)

### Tasks:
1. ❌ Copy fixed files to production repo
2. ❌ Update CMakeLists.txt (already uses main_qt.cpp)
3. ❌ Clean rebuild
4. ❌ Test chat send (no freeze)
5. ❌ Test all menus/docks visible
6. ❌ Test model loading (non-blocking)
7. ❌ Git commit all changes
8. ❌ Tag v1.0.0-stable

---

## 📊 Summary Checklist

| Component | Status | Files | Priority |
|-----------|--------|-------|----------|
| MainWindow staged init | ⚠️ Partial | MainWindow.cpp/h | P1 |
| Chat async send | ❌ Missing | chat_interface.cpp, ai_chat_panel.cpp | P1 |
| Inference async load | ❌ Missing | inference_engine.cpp | P1 |
| LSP async | ✅ Partial | lsp_client.cpp | P2 |
| Settings lazy init | ❌ Missing | settings_dialog.cpp | P2 |
| Security/Checkpoint | ❌ Missing | security_manager.cpp, checkpoint_manager.cpp | P2 |
| Terminal/Browser | ⚠️ Partial | TerminalWidget.cpp, file_browser.cpp | P3 |

**Total Items**: 38+ individual methods
**Estimated Time**: 2-3 hours systematic work
**Outcome**: Fully working, freeze-free IDE with all docks visible

---

## 🚀 Execution Order

1. **Enable MainWindow stages** (30 min) - Un-comment createVSCodeLayout, add stage2/3
2. **Fix chat async** (45 min) - QtConcurrent wrapper, progress UI
3. **Fix inference async** (45 min) - Worker thread, callbacks
4. **Fix settings lazy** (30 min) - Defer all widget creation
5. **Test & verify** (30 min) - All features working
6. **Git commit & tag** (15 min) - Ship v1.0.0-stable

---

**Say "start phase 1"** to begin systematic audit of MainWindow staged initialization.
