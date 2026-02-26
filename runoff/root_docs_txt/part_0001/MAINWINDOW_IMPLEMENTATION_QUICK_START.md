# 🚀 MainWindow Implementation Quick Start Guide

## 📋 Current Status
- **✅ 12 methods fully implemented** (8% complete)
- **📁 File:** `E:/RawrXD/src/qtapp/MainWindow.cpp` (4,986 lines)
- **🎯 Remaining:** ~138 methods to enhance
- **📊 Code Quality:** 100% error handling, 100% logging coverage

---

## 🔥 Implementation Template

Copy this template for each new method:

```cpp
void MainWindow::methodName(Parameters)
{
    // 1. LOGGING: Entry point with timestamp
    qInfo() << "[CATEGORY] methodName called at" << QDateTime::currentDateTime();
    
    // 2. VALIDATION: Check preconditions
    if (!requiredMember_) {
        qWarning() << "[CATEGORY] Required component null";
        statusBar()->showMessage(tr("Feature unavailable"), 3000);
        return;
    }
    
    // 3. INPUT VALIDATION
    if (input.isEmpty()) {
        statusBar()->showMessage(tr("Invalid input"), 2000);
        return;
    }
    
    // 4. BUSINESS LOGIC with error handling
    try {
        // Your implementation here
        // ...
        
        // 5. SUCCESS FEEDBACK
        statusBar()->showMessage(tr("Operation successful"), 3000);
        
        // 6. CONSOLE LOGGING
        if (m_hexMagConsole) {
            m_hexMagConsole->appendPlainText(
                QString("[CATEGORY] Success: %1").arg(result)
            );
        }
        
        // 7. CHAT HISTORY (optional)
        if (chatHistory_) {
            chatHistory_->addItem(tr("✅ Operation: %1").arg(summary));
        }
        
        // 8. PERSISTENCE (if state changes)
        QSettings settings("RawrXD", "QtShell");
        settings.setValue("Category/key", value);
        
        qInfo() << "[CATEGORY] methodName completed successfully";
        
    } catch (const std::exception& e) {
        // 9. ERROR HANDLING
        qCritical() << "[CATEGORY] Exception:" << e.what();
        QMessageBox::critical(this, tr("Error"), 
                            tr("Operation failed: %1").arg(e.what()));
        statusBar()->showMessage(tr("Operation failed"), 5000);
    }
}
```

---

## 🎯 Priority Implementation Order

### 🔴 PHASE 2: Critical Path (Next 20 methods)
**Why:** Core AI functionality required for MVP

#### Model & Inference (7 methods) - HIGH PRIORITY
```cpp
// 1. loadGGUFModel() - Model loading with validation
// 2. unloadGGUFModel() - Clean model unloading
// 3. runInference() - Execute inference pipeline
// 4. showInferenceResult() - Display results
// 5. showInferenceError() - Error handling
// 6. onModelLoadedChanged() - State change handler
// 7. handleBackendSelection() - Backend switching
```

#### AI/Agent Completion (8 methods)
```cpp
// 8. handleGoalSubmit() - Process agent goals
// 9. handleAgentMockProgress() - Progress tracking
// 10. updateSuggestion() - AI suggestions
// 11. appendModelChunk() - Streaming output
// 12. handleGenerationFinished() - Generation complete
// 13. onAIBackendChanged() - Backend change
// 14. setupAIBackendSwitcher() - UI setup
// 15. toggleAIChat() - Panel toggle
```

#### Task/Workflow (5 methods)
```cpp
// 16. handleTaskStatusUpdate() - Status updates
// 17. handleTaskCompleted() - Task done
// 18. handleTaskStreaming() - Streaming updates
// 19. handleWorkflowFinished() - Workflow complete
// 20. onProgressCancelled() - Cancellation
```

---

### 🟡 PHASE 3: UI Integration (Next 30 methods)
**Why:** Complete panel system

#### Toggle Methods (20 methods)
All `toggle*()` methods for subsystem visibility:
- toggleProjectExplorer
- toggleBuildSystem
- toggleVersionControl
- toggleRunDebug
- toggleProfiler
- toggleTestExplorer
- toggleDatabaseTool
- toggleDockerTool
- toggleCloudExplorer
- togglePackageManager
- ... (and 10 more)

#### UI Event Handlers (10 methods)
- onBookmarkToggled
- onAccessibilityToggled
- onWallpaperChanged
- onIconSelected
- onShortcutChanged
- ... (and 5 more)

---

### 🟢 PHASE 4: External Systems (Next 40 methods)
**Why:** Professional workflow support

- VCS integration (5 methods)
- Build system (5 methods)
- Cloud/Docker (10 methods)
- Database tools (5 methods)
- Plugin system (5 methods)
- Package management (5 methods)
- Documentation (5 methods)

---

### 🔵 PHASE 5: Editor Intelligence (Remaining ~48 methods)
**Why:** Advanced code features

- LSP integration (10 methods)
- Code lens/inlay hints (5 methods)
- Snippets/macros (5 methods)
- Collaboration (5 methods)
- Utilities (regex, diff, colors) (10 methods)
- Design tools (5 methods)
- Notebooks/markdown (5 methods)
- Time tracking/Pomodoro (3 methods)

---

## 🛠️ Development Workflow

### Step 1: Find Next Method
```powershell
# Search for stub methods
Select-String -Path "E:/RawrXD/src/qtapp/MainWindow.cpp" -Pattern "^void MainWindow::methodName"
```

### Step 2: Understand Context
- Read method signature in `MainWindow.h`
- Check member variables it might use
- Look at calling code (search for `->methodName()`)

### Step 3: Implement Using Template
- Copy template from above
- Replace placeholders with actual logic
- Add specific error cases
- Test edge cases

### Step 4: Test Implementation
```powershell
# Build the project
cd E:/RawrXD/build
cmake --build . --config Release

# Run the IDE
./RawrXD.exe
```

### Step 5: Verify Quality
✅ **Checklist:**
- [ ] Compiles without errors/warnings
- [ ] Null pointer checks present
- [ ] Error handling implemented
- [ ] Status bar feedback added
- [ ] Console logging added
- [ ] Method logs entry and exit
- [ ] User-friendly error messages
- [ ] No memory leaks (use deleteLater())

---

## 📝 Logging Patterns

### Standard Logging
```cpp
qInfo() << "[CATEGORY] Operation started";
qDebug() << "[CATEGORY] Details:" << value;
qWarning() << "[CATEGORY] Issue detected:" << issue;
qCritical() << "[CATEGORY] Critical error:" << error;
```

### Categories to Use
- `[MODEL]` - Model loading/inference
- `[TERMINAL]` - Terminal operations
- `[EDITOR]` - Editor features
- `[UI]` - UI events
- `[VCS]` - Version control
- `[BUILD]` - Build system
- `[AGENT]` - AI agent operations
- `[TASK]` - Task orchestration
- `[INIT]` - Initialization
- `[STATE]` - State management
- `[DEBUG_LOG]` - Debug console
- `[SCRIPT]` - Script execution

### Multi-Destination Logging
```cpp
// Always log to Qt console
qInfo() << "[CATEGORY] Message";

// Log to hex mag console (detailed)
if (m_hexMagConsole) {
    m_hexMagConsole->appendPlainText("[CATEGORY] Detailed message");
}

// Log to chat history (user-facing)
if (chatHistory_) {
    chatHistory_->addItem(tr("🎯 User message"));
}

// Show in status bar (immediate feedback)
statusBar()->showMessage(tr("Status update"), 3000);
```

---

## 🚨 Common Pitfalls to Avoid

### ❌ DON'T
```cpp
// No error checking
codeView_->setText(content); // Crash if codeView_ is null!

// No user feedback
runLongOperation(); // User has no idea what's happening

// No logging
executeCommand(); // Can't debug issues

// Manual memory management
QWidget* widget = new QWidget();
delete widget; // Use deleteLater() instead!

// Blocking UI
processHugeFile(); // Should use QThread
```

### ✅ DO
```cpp
// Always check nulls
if (!codeView_) {
    qWarning() << "[EDITOR] codeView_ is null";
    return;
}
codeView_->setText(content);

// Always provide feedback
statusBar()->showMessage(tr("Processing..."));
runLongOperation();
statusBar()->showMessage(tr("Complete"), 3000);

// Always log operations
qInfo() << "[CATEGORY] Executing command:" << cmd;
executeCommand();
qInfo() << "[CATEGORY] Command completed";

// Use Qt memory management
QWidget* widget = new QWidget(this); // Parent-owned
// or
QWidget* widget = new QWidget();
connect(widget, &QWidget::destroyed, []{ qDebug() << "Cleaned up"; });
widget->deleteLater(); // Safe async cleanup

// Use async for long operations
QThread* thread = new QThread(this);
Worker* worker = new Worker();
worker->moveToThread(thread);
connect(thread, &QThread::started, worker, &Worker::process);
connect(worker, &Worker::finished, thread, &QThread::quit);
thread->start();
```

---

## 📚 Quick Reference Links

### Qt Documentation
- **QMainWindow:** https://doc.qt.io/qt-6/qmainwindow.html
- **Signals/Slots:** https://doc.qt.io/qt-6/signalsandslots.html
- **QSettings:** https://doc.qt.io/qt-6/qsettings.html
- **QProcess:** https://doc.qt.io/qt-6/qprocess.html

### Project Files
- **Header:** `E:/RawrXD/src/qtapp/MainWindow.h`
- **Implementation:** `E:/RawrXD/src/qtapp/MainWindow.cpp`
- **Progress Report:** `E:/RawrXD/MAINWINDOW_STUB_IMPLEMENTATION_PROGRESS.md`
- **Executive Summary:** `E:/RawrXD/MAINWINDOW_IMPLEMENTATION_EXECUTIVE_SUMMARY.md`

### Tools
- **Build:** `cmake --build E:/RawrXD/build --config Release`
- **Run:** `E:/RawrXD/build/RawrXD.exe`
- **Search:** `Select-String -Path "path" -Pattern "pattern"`

---

## 🎓 Code Style Guidelines

### Naming Conventions
- **Private members:** `m_variableName` or `variableName_`
- **Qt widgets:** `widgetNameLabel`, `widgetNameButton`
- **Signals:** `onEventName()`, `eventNameChanged()`
- **Slots:** `handleEventName()`, `onEventName()`

### Formatting
- **Indentation:** 4 spaces (no tabs)
- **Braces:** Same line for functions, new line for classes
- **Line length:** Max 120 characters
- **Comments:** `//` for single line, `/* */` for multi-line

### Qt-Specific
- **Use `tr()` for all user-visible strings**
- **Check pointers with `if (!ptr)`**
- **Use `QPointer<T>` for optional widgets**
- **Use `connect()` for all signal-slot connections**
- **Prefer `QString` over `std::string`**
- **Use `QList<T>` over `std::vector<T>`**

---

## 💻 Example: Complete Implementation

Here's a real example from the codebase:

```cpp
void MainWindow::saveDebugLog() 
{
    qInfo() << "[DEBUG_LOG] Save requested at" << QDateTime::currentDateTime();
    
    if (!m_hexMagConsole) {
        statusBar()->showMessage(tr("Debug console not available"), 2000);
        return;
    }
    
    QString logContent = m_hexMagConsole->toPlainText();
    
    if (logContent.isEmpty()) {
        QMessageBox::information(this, tr("Save Debug Log"), 
                               tr("Debug log is empty. Nothing to save."));
        return;
    }
    
    QString defaultName = QString("rawrxd_debug_%1.log")
                         .arg(QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss"));
    
    QString filePath = QFileDialog::getSaveFileName(
        this,
        tr("Save Debug Log"),
        QDir::homePath() + "/" + defaultName,
        tr("Log Files (*.log);;Text Files (*.txt);;All Files (*.*)")  
    );
    
    if (filePath.isEmpty()) {
        statusBar()->showMessage(tr("Save cancelled"), 2000);
        return;
    }
    
    statusBar()->showMessage(tr("Saving debug log..."), 3000);
    
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::critical(this, tr("Save Failed"),
                            tr("Could not open file: %1").arg(file.errorString()));
        qWarning() << "[DEBUG_LOG] Failed to open file:" << filePath;
        return;
    }
    
    QTextStream out(&file);
    out.setCodec("UTF-8");
    out << "=== RawrXD IDE Debug Log ===\n";
    out << "Generated: " << QDateTime::currentDateTime().toString(Qt::ISODate) << "\n";
    out << "================================\n\n";
    out << logContent;
    file.close();
    
    statusBar()->showMessage(
        tr("Debug log saved: %1 KB").arg(QFileInfo(filePath).size() / 1024.0, 0, 'f', 2),
        5000
    );
    
    qInfo() << "[DEBUG_LOG] Saved to:" << filePath;
}
```

**Key Features:**
- ✅ Entry logging
- ✅ Null checks
- ✅ Empty state handling
- ✅ User dialogs
- ✅ File I/O error handling
- ✅ Status bar updates
- ✅ Success confirmation
- ✅ Exit logging

---

## 🎯 Success Metrics

After implementing each method, verify:

| Metric | Target | How to Check |
|--------|--------|--------------|
| **Lines of Code** | 30-150 lines | Line count |
| **Error Handling** | 100% | Try-catch + null checks |
| **Logging** | 3+ log statements | qInfo/qDebug/qWarning |
| **User Feedback** | 1+ status message | statusBar()->showMessage() |
| **Compilation** | Zero errors | cmake --build |
| **Testing** | Manual test | Run IDE, test feature |

---

## 📞 Need Help?

### Resources
1. **Qt Documentation:** https://doc.qt.io/
2. **C++ Reference:** https://en.cppreference.com/
3. **Project README:** `E:/RawrXD/README.md`
4. **Architecture Docs:** `E:/RawrXD/docs/`

### Debugging
```cpp
// Add temporary debug output
qDebug() << "DEBUG: Variable value:" << value;
qDebug() << "DEBUG: Pointer:" << (void*)pointer;
qDebug() << "DEBUG: Reached checkpoint 1";
```

### Common Issues
- **Null pointer crash:** Add `if (!ptr)` check
- **Signal not firing:** Check `connect()` return value
- **Widget not showing:** Check parent ownership
- **Memory leak:** Use `deleteLater()` instead of `delete`

---

**Start implementing now!** Pick a method from Phase 2, copy the template, and build something amazing! 🚀

---

*Quick Start Guide v1.0*  
*Last Updated: January 17, 2026*  
*RawrXD IDE Project*
