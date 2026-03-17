# 🔥 Phase 2 Integration - Final TODO List

**Status**: 90% Complete  
**Remaining**: Signal emissions in AgenticEngine  

---

## ✅ COMPLETED

- [x] Created all 5 Phase 2 widget implementations (11 source files)
- [x] Created 5 documentation files (QUICKSTART, VISUAL_GUIDE, etc.)
- [x] Integrated streaming token progress bar into ChatInterface
- [x] Created DiffDock widget for Day 2 deliverable
- [x] Updated chat_interface.h with progress bar members
- [x] Updated MainWindow_v5.h with Phase 2 declarations
- [x] Added Phase 2 includes to MainWindow_v5.cpp
- [x] Implemented `initializePhase2Polish()` function
- [x] Implemented `onRefactorSuggested()` function
- [x] Implemented `showModelDownloadDialog()` function
- [x] Added helper function `hasTelemetryPreference()`
- [x] Added `tokenGenerated` and `refactorSuggested` signals to AgenticEngine header
- [x] Updated CMakeLists.txt with Phase 2 UI files
- [x] Connected all Phase 2 widgets to MainWindow signals

---

## ⏳ REMAINING TASKS (Critical Path)

### 1. Emit `tokenGenerated` Signal in AgenticEngine

**File**: `src/agentic_engine.cpp` or `src/qtapp/inference_engine.cpp`  
**Location**: Streaming inference loop (where tokens are decoded)

**What to Add**:
```cpp
// Inside the token generation loop, after each token is decoded:
emit tokenGenerated(1);  // Emits signal to update progress bar
```

**Full Context Example**:
```cpp
QString AgenticEngine::generateTokenizedResponse(const QString& message) {
    QString response;
    
    // ... existing setup code ...
    
    while (!finished) {
        std::string token = m_inferenceEngine->generateNextToken();
        response += QString::fromStdString(token);
        
        emit tokenGenerated(1);  // 👈 ADD THIS LINE
        
        // ... rest of loop ...
    }
    
    return response;
}
```

**How to Find**:
```powershell
# Search for the generation loop:
Get-Content "D:\temp\RawrXD-agentic-ide-production\RawrXD-ModelLoader\src\agentic_engine.cpp" | Select-String -Pattern "while|for.*token|generate.*Token" -Context 5

# Or search in inference_engine.cpp:
Get-Content "D:\temp\RawrXD-agentic-ide-production\RawrXD-ModelLoader\src\qtapp\inference_engine.cpp" | Select-String -Pattern "while|for.*token|decode" -Context 5
```

---

### 2. Emit `refactorSuggested` Signal After `/refactor` Command

**File**: `src/chat_interface.cpp` or `src/agentic_engine.cpp`  
**Location**: Command handler for `/refactor` command

**What to Add**:
```cpp
// After processing /refactor command and getting AI response:
QString original = /* extract original code from editor */;
QString suggested = aiResponse;  // The refactored code from AI

emit refactorSuggested(original, suggested);  // 👈 ADD THIS LINE
```

**Full Context Example**:
```cpp
void ChatInterface::executeAgentCommand(const QString& command) {
    if (command.startsWith("/refactor")) {
        // Get current editor selection
        QString selectedCode = getEditorSelection();
        
        // Send to AI for refactoring
        QString prompt = "Refactor this code: " + command.mid(9);  // Remove "/refactor "
        QString refactoredCode = m_agenticEngine->generateCode(prompt);
        
        // Emit signal to show diff dock
        emit m_agenticEngine->refactorSuggested(selectedCode, refactoredCode);  // 👈 ADD THIS
        
        // Also show in chat
        appendMessage("AI", "Refactoring suggestion ready. Check the Diff Preview dock.");
    }
}
```

**How to Find**:
```powershell
# Search for /refactor command handling:
Get-Content "D:\temp\RawrXD-agentic-ide-production\RawrXD-ModelLoader\src\chat_interface.cpp" | Select-String -Pattern "/refactor|command.*refactor" -Context 10

# Or in agentic_engine.cpp:
Get-Content "D:\temp\RawrXD-agentic-ide-production\RawrXD-ModelLoader\src\agentic_engine.cpp" | Select-String -Pattern "refactor" -Context 10
```

---

### 3. Connect InferenceEngine to Backend Selector (Optional - Day 3+)

**File**: `src/qtapp/MainWindow_v5.cpp`  
**Location**: Inside `initializePhase2Polish()`, in the backend selector connection

**Current State**: Logs the backend change  
**Needed**: Actually reconfigure InferenceEngine

**What to Add**:
```cpp
connect(m_backendSelector, &RawrXD::GPUBackendSelector::backendChanged,
        this, [this](RawrXD::ComputeBackend backend) {
    QString backendName;
    switch (backend) {
        case RawrXD::ComputeBackend::CUDA: backendName = "CUDA"; break;
        case RawrXD::ComputeBackend::Vulkan: backendName = "Vulkan"; break;
        case RawrXD::ComputeBackend::CPU: backendName = "CPU"; break;
        default: backendName = "Auto"; break;
    }
    
    qDebug() << "[MainWindow] Backend switched to:" << backendName;
    statusBar()->showMessage("✓ Backend: " + backendName, 3000);
    
    // 👇 ADD THIS: Reconfigure inference engine
    if (m_inferenceEngine) {
        m_inferenceEngine->setComputeBackend(backend);
        m_inferenceEngine->reinitialize();  // Reload model with new backend
    }
});
```

---

## 🧪 Quick Testing Guide

### Test Token Progress Bar

**Option A: Mock Signal Emission**
```cpp
// Add to MainWindow_v5.cpp, inside initializePhase2Polish() at the end:
QTimer::singleShot(5000, this, [this]() {
    qDebug() << "[MainWindow] Testing token progress bar...";
    for (int i = 0; i < 100; ++i) {
        QTimer::singleShot(i * 50, [this]() {
            emit m_agenticEngine->tokenGenerated(1);
        });
    }
});
```

**Option B: Actual Inference**
1. Load a tiny model (phi-2 or tinyllama)
2. Type a message in chat
3. Watch progress bar appear at top of chat panel

---

### Test Diff Preview Dock

**Option A: Manual Trigger**
```cpp
// Add to MainWindow_v5.cpp, inside initializePhase2Polish() at the end:
QTimer::singleShot(7000, this, [this]() {
    QString original = 
        "void calculateTotal() {\n"
        "    int sum = 0;\n"
        "    for(int i=0; i<10; i++) sum+=i;\n"
        "    return sum;\n"
        "}";
    
    QString suggested = 
        "int calculateTotal() {\n"
        "    return std::accumulate(\n"
        "        std::begin(range), std::end(range), 0);\n"
        "}";
    
    emit m_agenticEngine->refactorSuggested(original, suggested);
});
```

**Option B: Actual Refactor Command**
1. Select some code in the editor
2. Type `/refactor simplify this function` in chat
3. Wait for AI response
4. Diff dock should appear on right side

---

### Test GPU Backend Selector

1. Launch RawrXD-AgenticIDE
2. Look for "Backend:" dropdown in toolbar
3. Select different backend (CUDA/Vulkan/CPU)
4. Check debug console for: `[MainWindow] Backend switched to: <name>`
5. (Once InferenceEngine integration is done) Verify model reloads

---

### Test Model Downloader

**Force Trigger** (even if models exist):
```cpp
// In MainWindow_v5.cpp, replace the auto-download check with:
QTimer::singleShot(1500, this, [this]() {
    // Force show dialog for testing
    showModelDownloadDialog();
});
```

**Normal Flow**:
1. Delete all `.gguf` files from your models directory
2. Launch RawrXD-AgenticIDE
3. After 1.5 seconds, dialog should appear
4. Select a model and click Download
5. Watch progress bar
6. After completion, model should appear in chat dropdown

---

### Test Telemetry Opt-In

**Force Show** (even if preference exists):
```cpp
// In MainWindow_v5.cpp, replace telemetry check with:
QTimer::singleShot(2500, this, [this]() {
    // Clear preference for testing
    QSettings("RawrXD", "AgenticIDE").remove("telemetry/enabled");
    
    RawrXD::TelemetryOptInDialog* dialog = new RawrXD::TelemetryOptInDialog(this);
    // ... rest of dialog code ...
});
```

**Normal Flow**:
1. First launch of RawrXD-AgenticIDE
2. After 2.5 seconds, dialog appears
3. Click "Enable" or "Disable"
4. Preference is saved
5. Second launch: dialog does NOT appear

---

## 📂 File Locations Quick Reference

| Component | Header | Implementation |
|-----------|--------|----------------|
| MainWindow | `src/qtapp/MainWindow_v5.h` | `src/qtapp/MainWindow_v5.cpp` |
| ChatInterface | `include/chat_interface.h` | `src/chat_interface.cpp` |
| AgenticEngine | `include/agentic_engine.h` | `src/agentic_engine.cpp` |
| InferenceEngine | `src/qtapp/inference_engine.hpp` | `src/qtapp/inference_engine.cpp` |
| DiffDock | `src/ui/diff_dock.h` | `src/ui/diff_dock.cpp` |
| GPU Backend Selector | `src/ui/gpu_backend_selector.h` | `src/ui/gpu_backend_selector.cpp` |
| Model Downloader | `src/ui/auto_model_downloader.h` | `src/ui/auto_model_downloader.cpp` |
| Telemetry Dialog | `src/ui/telemetry_optin_dialog.h` | `src/ui/telemetry_optin_dialog.cpp` |

---

## 🔍 Debugging Tips

### Progress Bar Not Showing

**Check**:
1. Is `tokenGenerated` signal being emitted? (Add `qDebug() << "Token!";`)
2. Is signal connected? (Check Phase 2 initialization logs)
3. Is progress bar created? (Check `m_tokenProgress != nullptr`)
4. Is progress bar visible? (It auto-hides after 1s, type message faster!)

### Diff Dock Not Appearing

**Check**:
1. Is `refactorSuggested` signal being emitted?
2. Is dock created in `initializePhase2Polish()`?
3. Is dock hidden by default? (Check `m_diffPreviewDock->isHidden()`)
4. Is signal connection working? (Add debug log in lambda)

### Backend Selector Dropdown Empty

**Check**:
1. Is `GPUBackendSelector::initialize()` being called?
2. Are backends being detected? (Add debug logs in backend detection)
3. Is CUDA/Vulkan installed on system?
4. Does widget have proper parent (MainWindow)?

---

## 🚀 Build and Run

```powershell
# Navigate to project
cd D:\temp\RawrXD-agentic-ide-production\RawrXD-ModelLoader

# Clean build (recommended)
rm -r build -Force -ErrorAction SilentlyContinue
mkdir build
cd build

# Configure with CMake
cmake .. -G "Visual Studio 17 2022" -A x64

# Build Release
cmake --build . --config Release

# Run
.\bin\Release\RawrXD-AgenticIDE.exe
```

**Expected Output**:
```
[MainWindow] 🎨 Initializing Phase 2 Polish Features...
  ✓ Diff Preview Dock initialized
  ✓ Token progress connected to AgenticEngine
  ✓ GPU Backend Selector initialized
  ✓ Auto Model Download scheduled
  ✓ Telemetry Opt-In scheduled
[MainWindow] ✅ Phase 2 Polish Features initialized
```

---

## 📞 Need Help?

**Common Issues**:
- **Signal not emitting**: Check if object is valid (`!= nullptr`)
- **Slot not called**: Verify signal signature matches slot signature exactly
- **Widget not showing**: Check if `hide()` is being called somewhere
- **Build errors**: Ensure all new files are in CMakeLists.txt

**Debug Pattern**:
```cpp
qDebug() << "[ComponentName] Event happened:" << details;
```

**Enable MOC Debugging**:
```cmake
set(CMAKE_AUTOMOC_MOC_OPTIONS "-d")  # Verbose MOC output
```

---

**Last Updated**: 2025-01-XX  
**Status**: Ready for final signal emissions  
**Blockers**: None - all infrastructure complete
