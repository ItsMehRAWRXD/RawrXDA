# Real-Time Integration System - Integration Guide

**Last Updated**: December 27, 2025  
**Target**: CMake Build System Integration  
**Scope**: Connecting new real-time systems to existing RawrXD IDE

---

## 📋 Step-by-Step Integration

### Step 1: Update CMakeLists.txt

Add the new source files to your CMakeLists.txt in the `src/` directory:

```cmake
# In CMakeLists.txt - find the section where source files are listed

# Add these new source files to the target
target_sources(RawrXD-QtShell PRIVATE
    # Real-Time Integration Systems
    ${CMAKE_CURRENT_SOURCE_DIR}/src/real_time_integration_coordinator.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/real_time_terminal_pool.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/real_time_editor_integration.cpp
)

# Ensure MOC processes the new headers (if not already automated)
# Usually Qt handles this automatically with CMAKE_AUTOMOC ON
```

### Step 2: Update MainWindow.cpp

Add initialization code to your MainWindow class:

```cpp
// In src/qtapp/MainWindow.cpp - include headers
#include "real_time_integration_coordinator.hpp"
#include "real_time_terminal_pool.hpp"
#include "real_time_editor_integration.hpp"

// In MainWindow class definition (MainWindow.h)
class MainWindow : public QMainWindow {
    // ... existing members ...
    
private:
    // Add new members
    std::unique_ptr<RealTimeIntegrationCoordinator> m_coordinator;
    std::unique_ptr<RealTimeTerminalPool> m_terminalPool;
    std::unique_ptr<RealTimeEditorIntegration> m_editorIntegration;
};

// In MainWindow::MainWindow() or initialization function
void MainWindow::initializeRealTimeIntegration() {
    // Create the real-time systems
    m_terminalPool = std::make_unique<RealTimeTerminalPool>(4);  // 4 terminal slots
    m_editorIntegration = std::make_unique<RealTimeEditorIntegration>();
    m_coordinator = std::make_unique<RealTimeIntegrationCoordinator>();
    
    // Initialize the integration coordinator with all components
    m_coordinator->initialize(
        m_copilotBridge.get(),        // From existing code
        m_chatPanel,                  // From existing code
        m_multiTabEditor.get(),       // From existing code
        m_terminalPool.get(),         // New
        m_agenticEngine.get(),        // From existing code
        m_inferenceEngine.get()       // From existing code
    );
    
    // Connect editor integration to copilot bridge
    m_editorIntegration->setAgenticBridge(m_copilotBridge.get());
    
    qDebug() << "[MainWindow] Real-time integration initialized successfully";
}

// Call this from MainWindow constructor or setupUI():
// initializeRealTimeIntegration();
```

### Step 3: Wire Up Existing Components

If you have existing editor/terminal implementations, bridge them to the new systems:

```cpp
// In MainWindow or wherever you manage the UI

// Route existing editor events to new integration
void MainWindow::setupEditorConnections() {
    // If you have existing editor signals, connect them:
    // connect(m_editorTabs, &QTabWidget::currentChanged,
    //         m_editorIntegration, [this](int index) {
    //             // Set active session based on tab
    //         });
    
    // For now, load the first file when ready
    if (m_multiTabEditor) {
        m_editorIntegration->loadFile("C:\\path\\to\\first\\file.cpp");
    }
}

// Route existing terminal to new integration
void MainWindow::setupTerminalConnections() {
    // If you have existing terminal, use the new pool:
    // int termId = m_terminalPool->createTerminal(
    //     TerminalSession::PowerShell,
    //     QDir::currentPath()
    // );
}
```

### Step 4: Build and Test

```bash
cd path/to/RawrXD-production-lazy-init

# Configure with CMake
cmake -B build_masm -DCMAKE_BUILD_TYPE=Release

# Build (this will compile new source files)
cmake --build build_masm --config Release --target RawrXD-QtShell

# Run
./build_masm/bin/Release/RawrXD-QtShell.exe
```

### Step 5: Validate Integration

Test the real-time features:

1. **Terminal Test**:
   ```
   - Open application
   - Try to execute a command in terminal
   - Verify output appears in real-time
   - Check that command count increments
   ```

2. **Editor Test**:
   ```
   - Load a file via the file browser
   - Edit content (should auto-track)
   - Check edit history (Ctrl+Z should undo)
   - Save file and verify modification tracking
   ```

3. **Chat Integration Test**:
   ```
   - Send message in chat panel
   - Select code in editor
   - Ask AI to modify selected code
   - Code should be suggested and insertable
   ```

4. **Coordinator Test**:
   ```
   - Check debug output for coordinator initialization messages
   - Verify component readiness signals
   - Monitor sync cycles (every 500ms)
   ```

---

## 🔗 API Reference

### RealTimeIntegrationCoordinator

```cpp
// Key public methods
void initialize(AgenticCopilotBridge*, AIChatPanel*, MultiTabEditor*, 
                TerminalPool*, AgenticEngine*, InferenceEngine*);

void submitChatMessage(const QString& msg, int mode, const QString& model);
void requestCodeCompletion(const QString& context, const QString& prefix, const QString& file);
void executeTerminalCommand(const QString& cmd, int terminalId = -1);
void insertCodeIntoEditor(const QString& code, bool autoFormat = true);

QString getEditorContent() const;
QString getSelectedText() const;

// State queries
bool isEditorReady() const;
bool isTerminalReady(int id = -1) const;
bool isChatReady() const;
```

### RealTimeTerminalPool

```cpp
// Terminal creation & management
int createTerminal(TerminalSession::ShellType type, const QString& workDir);
bool closeTerminal(int terminalId);
bool setActiveTerminal(int terminalId);

// Command execution
void executeCommand(const QString& cmd, int terminalId = -1, bool capture = true);
bool executeCommandSync(const QString& cmd, QString& output, QString& error, 
                        int timeoutMs = 30000, int termId = -1);

// Output access
QString getTerminalOutput(int termId = -1, int lines = -1) const;
QJsonObject getPoolStatistics() const;
```

### RealTimeEditorIntegration

```cpp
// Session management
int createEditorSession(const QString& filePath);
bool closeEditorSession(int sessionId);
bool setActiveSession(int sessionId);

// File operations
bool loadFile(const QString& filePath);
bool saveFile(int sessionId = -1);

// Content access
QString getEditorContent(int sessionId = -1) const;
QString getSelectedText(int sessionId = -1) const;
int getLineCount(int sessionId = -1) const;

// Editing
void insertText(const QString& text, int sessionId = -1);
void replaceSelection(const QString& replacement, int sessionId = -1);

// Agentic features
void requestCodeCompletion(const QString& context, const QString& prefix, int id = -1);
void applyAgenticSuggestion(const QString& suggestion, int startLine, int endLine, int id = -1);
```

---

## 🎯 Usage Examples

### Example 1: Load and Edit a File

```cpp
// In your MainWindow or initialization code:

// Create editor integration
RealTimeEditorIntegration editor;

// Load a file
int sessionId = editor.createEditorSession("src/main.cpp");
editor.loadFile("src/main.cpp");

// Later: get content
QString content = editor.getEditorContent(sessionId);

// Insert text at cursor
editor.insertText("auto result = ", sessionId);

// Save
editor.saveFile(sessionId);
```

### Example 2: Execute Terminal Commands

```cpp
// Create terminal pool
RealTimeTerminalPool terminalPool(4);

// Create a terminal
int termId = terminalPool.createTerminal(
    TerminalSession::PowerShell,
    "C:\\Projects\\MyProject"
);

// Execute a command
terminalPool.executeCommand("cmake --build . --config Release", termId);

// Later: check output
QString output = terminalPool.getTerminalOutput(termId, -1);
qDebug() << "Build output:" << output;

// Get statistics
QJsonObject stats = terminalPool.getPoolStatistics();
qDebug() << "Terminal stats:" << stats;
```

### Example 3: Integrate Chat with Code

```cpp
// When user sends a message in chat asking to refactor code:

// Coordinator handles routing
coordinator->submitChatMessage(
    "Optimize this function for performance",
    AGENT_MODE_EDIT,  // Edit mode
    "gpt-4"
);

// Internally:
// 1. Message classified as CodeEdit intent
// 2. Routed to editor handler
// 3. Editor sends context to copilot bridge
// 4. Copilot generates optimized code
// 5. Code suggestion returned to chat
// 6. User can click "Insert" in chat
// 7. Code inserted into editor via coordinator
// 8. File marked as modified
```

### Example 4: Multi-Step Build Process

```cpp
// Execute build sequence
RealTimeTerminalPool pool;
int termId = pool.createTerminal(TerminalSession::PowerShell);

QStringList commands = {
    "cls",                                           // Clear
    "cmake -B build -DCMAKE_BUILD_TYPE=Release",   // Configure
    "cmake --build build --config Release",        // Build
    "ctest --output-on-failure"                    // Test
};

pool.executeBatchCommands(commands, termId);

// Monitor in real-time via signal
connect(&pool, &RealTimeTerminalPool::terminalOutput,
        this, [](int id, const QString& output) {
            qDebug() << "Build output:" << output;
        });

// Get final results
QString buildLog = pool.getTerminalOutput(termId, -1);
qDebug() << "Final build output:" << buildLog;
```

---

## 🐛 Debugging

### Enable Debug Logging

The implementation provides comprehensive logging. Check output window for:

```
[RealTimeIntegrationCoordinator] Constructing...
[RealTimeTerminalPool] Created with pool size: 4
[RealTimeEditorIntegration] Constructed...
[RealTimeIntegrationCoordinator] Initializing component connections...
[RealTimeIntegrationCoordinator] ✅ Fully initialized
```

### Common Issues

**Issue**: Components show as "not ready"
- **Solution**: Ensure `coordinator->initialize()` is called with all components

**Issue**: Terminal commands don't appear to execute
- **Solution**: Check that the active terminal is set with `setActiveTerminal()`

**Issue**: Editor content not updating
- **Solution**: Ensure `loadFile()` is called before editing, not just `createEditorSession()`

**Issue**: Memory usage growing
- **Solution**: Call `closeEditorSession()` and `closeTerminal()` for unused files/terminals

---

## 📊 Monitoring

### Check Component Status

```cpp
// In your monitoring/debug code:

qDebug() << "Editor ready:" << coordinator->isEditorReady();
qDebug() << "Terminal ready:" << coordinator->isTerminalReady();
qDebug() << "Chat ready:" << coordinator->isChatReady();
qDebug() << "Agent ready:" << coordinator->isAgentReady();

// Check statistics
QJsonObject stats = editorIntegration->getEditorStatistics();
qDebug() << "Editor stats:" << QJsonDocument(stats).toJson();

QJsonObject poolStats = terminalPool->getPoolStatistics();
qDebug() << "Terminal pool:" << QJsonDocument(poolStats).toJson();
```

### Performance Metrics

The systems emit metrics via qDebug:
- Code completion latency: `[Metrics] code_completion_latency_ms: 234 ms`
- File analysis latency: `[Metrics] file_analysis_latency_ms: 45 ms`
- Terminal output: `[Metrics] terminal_output_received: 512 bytes`

---

## 🚀 Next Phases

### Phase 2: GUI/Pane System Integration
Connect the new real-time systems to the existing pane management system:
- Terminal panes use `RealTimeTerminalPool`
- Editor panes use `RealTimeEditorIntegration`
- Chat pane uses coordinator

### Phase 3: Hotpatch Integration
Bridge the hotpatch systems:
- Editor file changes → Hotpatch trigger
- Terminal commands → Hotpatch validation
- Chat suggestions → Hotpatch application

### Phase 4: Advanced Features
- Collaborative editing support
- Remote terminal execution
- AI-powered terminal command suggestion
- Smart context injection from all panes

---

## 📞 Support

For issues or questions:
1. Check debug output for detailed logging
2. Verify all components are initialized
3. Ensure signals are properly connected
4. Test individual components in isolation
5. Check CMakeLists.txt integration

---

**Integration Checklist**:
- [ ] CMakeLists.txt updated
- [ ] Headers included in MainWindow.cpp
- [ ] Coordinator instantiated in MainWindow
- [ ] Components connected to coordinator
- [ ] Build completes without errors
- [ ] Application starts successfully
- [ ] Terminal test passes
- [ ] Editor test passes
- [ ] Chat integration test passes
- [ ] Coordinator initialization logged
- [ ] No compilation warnings
- [ ] No runtime errors in console

**Ready to integrate!**
