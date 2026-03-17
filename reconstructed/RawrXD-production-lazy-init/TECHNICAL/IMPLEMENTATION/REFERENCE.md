// ===============================================================================
// RAWRXD ARCHITECTURE - Technical Implementation Reference
// ===============================================================================

# RawrXD - Technical Architecture & Implementation Details

## 1. OrchestraManager - The Central Orchestrator

**File**: Located in `src/core/orchestra_manager.hpp`

The OrchestraManager is the **single source of truth** for all subsystem coordination:

```cpp
class OrchestraManager : public QObject {
    Q_OBJECT

    // Singleton access
    static OrchestraManager& instance();
    
    // Initialization
    bool initialize();
    void setHeadlessMode(const HeadlessConfig& config);
    
    // ============= PROJECT MANAGEMENT =============
    bool openProject(const QString& projectPath);
    bool closeProject();
    bool createProject(const QString& path, const QString& template);
    QString currentProjectPath() const;
    bool hasOpenProject() const;
    ProjectInfo getProjectInfo() const;
    
    // ============= BUILD SYSTEM =============
    void buildProject(const QString& path, const QString& config);
    void cleanBuild(const QString& path);
    void rebuildProject(const QString& path, const QString& config);
    void configureCMake(const QString& path, const QString& config);
    
    // ============= AI/INFERENCE =============
    void aiInfer(const QString& prompt);
    void aiComplete(const QString& context, const QString& language);
    void aiExplain(const QString& code);
    void aiRefactor(const QString& code, const QString& instruction);
    
    // ============= GIT OPERATIONS =============
    GitStatus gitStatus();
    void gitAdd(const QStringList& files);
    void gitCommit(const QString& message);
    void gitPush(const QString& remote, const QString& branch);
    void gitPull(const QString& remote, const QString& branch);
    void gitBranch(const QString& name);
    void gitCheckout(const QString& branch);
    QStringList gitLog(int count);
    
    // ============= TERMINAL OPERATIONS =============
    int createTerminal();
    void executeInTerminal(int terminalId, const QString& command);
    void closeTerminal(int terminalId);
    
    // ============= TESTING =============
    void discoverTests();
    void runTests(const QStringList& testIds);
    QJsonObject getTestCoverage();
    
    // ============= DIAGNOSTICS =============
    QJsonObject runDiagnostics();
    QJsonObject getSystemInfo();
    SystemStatus getSystemStatus();

signals:
    // Build signals
    void buildOutput(const QString& line, bool isError);
    void buildFinished(bool success, const QString& msg);
    void buildProgress(int percentage);
    
    // AI signals (token streaming)
    void aiInferenceToken(const QString& token);
    void aiInferenceFinished(const QString& completeResponse);
    
    // Project signals
    void projectOpened(const QString& path);
    void projectClosed();
    void projectInfoUpdated(const ProjectInfo& info);
    
    // Terminal signals
    void terminalOutput(int terminalId, const QString& output);
    void terminalClosed(int terminalId);
    
    // Git signals
    void gitStatusChanged(const GitStatus& status);
    void gitOperationFinished(bool success, const QString& msg);
    
    // Test signals
    void testDiscovered(const TestInfo& test);
    void testStarted(const QString& testId);
    void testFinished(const QString& testId, bool passed);
    
    // General signals
    void info(const QString& component, const QString& msg);
    void warning(const QString& component, const QString& msg);
    void error(const QString& component, const QString& msg);

private:
    // Subsystems
    BuildSystem* m_buildSystem;
    InferenceEngine* m_inferenceEngine;
    TerminalPool* m_terminalPool;
    GitIntegration* m_gitIntegration;
    TestRunner* m_testRunner;
    ModelRegistry* m_modelRegistry;
    ProjectManager* m_projectManager;
};
```

---

## 2. CLI Implementation Architecture

**File**: `src/cli/cli_main.cpp` (1,516 lines)

### Signal Flow in CLI

```
┌─────────────────────────────────────────────────────┐
│ User Input (stdin / batch file / single command)   │
└─────────────────┬───────────────────────────────────┘
                  │
        ┌─────────▼─────────┐
        │ parseCommandLine  │  (Handle quotes, spaces)
        └─────────┬─────────┘
                  │
        ┌─────────▼──────────┐
        │ executeCommand()   │  (Router)
        └─────────┬──────────┘
                  │
         ┌────────▼────────┐
         │ handleXxxCmd()  │  (Specific handler)
         └─────────┬───────┘
                   │
       ┌───────────▼───────────┐
       │ OrchestraManager      │  (Unified backend)
       │ .execute*()           │
       └─────────┬─────────────┘
                 │
          ┌──────▼──────┐
          │ Qt Signal   │  (Emitted by OrchestraManager)
          │ Emission    │
          └──────┬──────┘
                 │
      ┌──────────▼──────────┐
      │ CLIApp Signal       │  (Connected in connectSignals())
      │ Handler Lambdas     │
      └──────────┬──────────┘
                 │
      ┌──────────▼──────────────┐
      │ Format & Print Output   │
      │ (Colors, JSON, Plain)   │
      └──────────┬──────────────┘
                 │
      ┌──────────▼──────────────┐
      │ Return Exit Code        │
      │ (0=success, 1+=error)   │
      └─────────────────────────┘
```

### CLI Command Handler Pattern

```cpp
int CLIApp::handleBuildCommand(const QStringList& args) {
    // Parse CLI arguments
    QString config = "Debug";
    QString target = "";
    
    for (const auto& arg : args) {
        if (arg == "--config" && args.indexOf(arg) + 1 < args.size()) {
            config = args[args.indexOf(arg) + 1];
        } else if (arg == "--target") {
            target = args[args.indexOf(arg) + 1];
        }
    }
    
    // Call OrchestraManager
    auto& om = OrchestraManager::instance();
    om.buildProject(om.currentProjectPath(), config);
    
    // Format result
    if (m_verbose) {
        m_out << Colors::Cyan << "[Build]" << Colors::Reset 
              << " Configuration: " << config << Qt::endl;
    }
    
    // Connect temporary slots to capture result
    bool buildSuccess = false;
    connect(&om, &OrchestraManager::buildFinished, this, [&buildSuccess](bool success, const QString& msg) {
        buildSuccess = success;
    }, Qt::SingleShotConnection);
    
    // Return appropriate exit code
    return buildSuccess ? 0 : 1;
}
```

### Interactive REPL Implementation

```cpp
int CLIApp::runInteractive(QCoreApplication& app) {
    m_running = true;
    
    QTimer inputTimer;
    connect(&inputTimer, &QTimer::timeout, this, [this, &app]() {
        if (!m_running) {
            app.quit();
            return;
        }
        
        // Display prompt with context
        m_out << Colors::Green << "rawrxd" << Colors::Reset;
        if (OrchestraManager::instance().hasOpenProject()) {
            QString projectName = QDir(OrchestraManager::instance().currentProjectPath())
                                    .dirName();
            m_out << Colors::Blue << " [" << projectName << "]" << Colors::Reset;
        }
        m_out << "> ";
        m_out.flush();
        
        // Read and process line
        QString line = m_in.readLine();
        if (line.isNull()) {
            m_running = false;
            return;
        }
        
        QStringList args = parseCommandLine(line);
        if (!args.isEmpty()) {
            if (args[0] == "exit" || args[0] == "quit") {
                m_running = false;
            } else {
                executeCommand(args);
            }
        }
    });
    
    inputTimer.start(100);  // Poll every 100ms
    return app.exec();
}
```

### Batch File Execution

```cpp
int CLIApp::runBatchFile(const QString& filePath) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        printError(QString("Cannot open batch file: %1").arg(filePath));
        return 1;
    }
    
    // Parse commands
    QStringList commands;
    QTextStream in(&file);
    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();
        // Skip comments and empty lines
        if (!line.isEmpty() && !line.startsWith('#') && !line.startsWith("//")) {
            commands << line;
        }
    }
    file.close();
    
    // Execute each command
    int exitCode = 0;
    for (const auto& cmd : commands) {
        auto args = parseCommandLine(cmd);
        exitCode = executeCommand(args);
        
        if (m_failFast && exitCode != 0) {
            break;
        }
    }
    
    return exitCode;
}
```

---

## 3. GUI Implementation Architecture

**Files**: `src/qtapp/MainWindow_v5.h/cpp`

### Widget Organization

```
MainWindow_v5 (QMainWindow)
│
├── centralWidget() → QTabWidget
│   ├── Tab 0: Code Editor (QPlainTextEdit)
│   ├── Tab 1: Code Editor (for each open file)
│   └── ...
│
├── menuBar()
│   ├── File Menu
│   ├── Edit Menu
│   ├── View Menu
│   ├── Build Menu
│   ├── Tools Menu
│   ├── Debug Menu
│   ├── Languages Menu (auto-created by LanguageIntegration)
│   └── Help Menu
│
├── Toolbar
│   ├── New/Open/Save buttons
│   ├── Quick build
│   └── Compiler selector
│
├── statusBar()
│   ├── Current position
│   ├── File encoding
│   ├── Build status
│   └── AI model status
│
└── Dock Widgets (Left)
    ├── Activity Bar (vertical tabs)
    ├── File Browser
    ├── AI Chat Panel
    ├── AI Digestion Panel
    ├── Test Explorer
    └── Todo Manager
    
    Dock Widgets (Bottom)
    ├── Terminal
    ├── Problems Panel
    ├── Model Manager
    ├── Debugger
    └── Workflow Panel
```

### Signal Flow in GUI

```
┌─────────────────────────────────────┐
│ User Action (Click, Shortcut, etc) │
└────────────┬────────────────────────┘
             │
     ┌───────▼────────┐
     │ Qt Slot        │
     │ Handler        │
     └───────┬────────┘
             │
   ┌─────────▼─────────┐
   │ OrchestraManager  │
   │ .execute*()       │
   └─────────┬─────────┘
             │
      ┌──────▼──────┐
      │ Qt Signal   │
      │ Emission    │
      └──────┬──────┘
             │
  ┌──────────┴──────────┐
  │                     │
  ▼                     ▼
Terminal::   AIChatPanel::
appendOutput() appendToken()
  │                     │
  └──────────┬──────────┘
             │
      ┌──────▼─────────┐
      │ Visual Update  │
      │ (Immediate)    │
      └────────────────┘
```

### Menu Creation Pattern

```cpp
void MainWindow_v5::setupMenuBar() {
    // File Menu
    auto fileMenu = menuBar()->addMenu("&File");
    
    auto newAction = fileMenu->addAction("&New");
    connect(newAction, &QAction::triggered, this, &MainWindow_v5::onNewFile);
    newAction->setShortcut(QKeySequence::New);
    
    auto openAction = fileMenu->addAction("&Open");
    connect(openAction, &QAction::triggered, this, &MainWindow_v5::onOpenFile);
    openAction->setShortcut(QKeySequence::Open);
    
    fileMenu->addSeparator();
    
    auto recentMenu = fileMenu->addMenu("Recent Files");
    // Populate recent files
    
    // ... more menus ...
    
    // Language Menu (auto-created by integration)
    // LanguageIntegration::createLanguageMenu(this);
}
```

### Dock Widget Creation Pattern

```cpp
void MainWindow_v5::setupDockWidgets() {
    // File Browser (Left)
    m_fileBrowser = new QTreeWidget(this);
    auto fileBrowserDock = new QDockWidget("File Browser", this);
    fileBrowserDock->setWidget(m_fileBrowser);
    addDockWidget(Qt::LeftDockWidgetArea, fileBrowserDock);
    
    // Terminal (Bottom)
    m_terminal = new TerminalWidget(this);
    auto terminalDock = new QDockWidget("Terminal", this);
    terminalDock->setWidget(m_terminal);
    addDockWidget(Qt::BottomDockWidgetArea, terminalDock);
    
    // AI Chat Panel (Left)
    m_aiChatPanel = new AIChatPanel(this);
    auto chatDock = new QDockWidget("AI Chat", this);
    chatDock->setWidget(m_aiChatPanel);
    addDockWidget(Qt::LeftDockWidgetArea, chatDock);
    
    // Tabify for tabbed interface
    tabifyDockWidget(fileBrowserDock, chatDock);
    
    // Make terminal initially visible
    terminalDock->raise();
}
```

### Signal/Slot Connection Pattern

```cpp
void MainWindow_v5::setupUI() {
    auto& om = OrchestraManager::instance();
    
    // Build button
    connect(ui.buildButton, &QPushButton::clicked, this, [&om, this]() {
        om.buildProject(om.currentProjectPath(), "Release");
    });
    
    // Build output signal → Terminal
    connect(&om, &OrchestraManager::buildOutput, this, [this](const QString& line, bool isError) {
        m_terminal->appendOutput(line);
        if (isError) {
            m_problemsPanel->addError(line);
        }
    });
    
    // Build finished signal → Status
    connect(&om, &OrchestraManager::buildFinished, this, [this](bool success, const QString& msg) {
        statusBar()->showMessage(msg, 5000);
        if (!success) {
            QMessageBox::critical(this, "Build Failed", msg);
        }
    });
    
    // AI inference signal → Chat panel (real-time tokens)
    connect(&om, &OrchestraManager::aiInferenceToken, this, [this](const QString& token) {
        m_aiChatPanel->appendToken(token);
    });
    
    // AI finished signal → Chat panel (mark as complete)
    connect(&om, &OrchestraManager::aiInferenceFinished, this, [this](const QString& response) {
        m_aiChatPanel->finishResponse();
    });
}
```

---

## 4. Language Framework Integration Points

### 4.1 CLI Integration

**File**: `src/cli/cli_main.cpp` (handleCompileCommand section)

```cpp
int CLIApp::handleCompileCommand(const QStringList& args) {
    if (args.isEmpty()) {
        printError("Usage: compile <file> [--language <lang>]");
        return 1;
    }
    
    QString sourceFile = args[0];
    
    // Auto-detect language or use specified
    LanguageType type = LanguageFactory::detectLanguageFromFile(sourceFile);
    
    if (type == LanguageType::Unknown) {
        printError(QString("Cannot detect language for: %1").arg(sourceFile));
        return 1;
    }
    
    // Compile using language framework
    auto result = LanguageManager::instance().compileFile(sourceFile);
    
    // Output result
    if (result.success) {
        if (m_jsonOutput) {
            QJsonObject obj;
            obj["success"] = true;
            obj["output_file"] = result.outputFile;
            obj["compilation_time_ms"] = result.compilationTime;
            QJsonDocument doc(obj);
            m_out << doc.toJson();
        } else {
            printSuccess(QString("Compiled successfully: %1ms").arg(result.compilationTime));
            m_out << "Output: " << result.outputFile << Qt::endl;
        }
        return 0;
    } else {
        if (m_jsonOutput) {
            QJsonObject obj;
            obj["success"] = false;
            obj["error"] = result.errorMessage;
            QJsonDocument doc(obj);
            m_out << doc.toJson();
        } else {
            printError(result.errorMessage);
        }
        return 1;
    }
}
```

### 4.2 GUI Integration

**File**: `src/qtapp/MainWindow_v5.cpp` (Language-related slots)

```cpp
void MainWindow_v5::onCompileCurrentFile() {
    QString currentFile = m_tabWidget->currentIndex() >= 0 
        ? m_tabWidget->tabText(m_tabWidget->currentIndex())
        : "";
    
    if (currentFile.isEmpty()) {
        QMessageBox::warning(this, "Warning", "No file selected");
        return;
    }
    
    // Use language integration
    LanguageIntegration::onFileOpened(currentFile);
    
    // Show language widget
    auto widget = LanguageIntegration::languageWidget();
    if (widget) {
        widget->show();
        widget->raise();
    }
    
    // Execute compilation through language framework
    auto& lm = LanguageManager::instance();
    auto result = lm.compileFile(currentFile);
    
    // Display in terminal and problems panel
    if (result.success) {
        m_terminal->appendOutput(QString("✓ Compiled: %1ms").arg(result.compilationTime));
        statusBar()->showMessage("Compilation successful", 3000);
    } else {
        m_terminal->appendOutput("✗ Compilation failed");
        m_problemsPanel->addError(result.errorMessage);
        statusBar()->showMessage("Compilation failed", 3000);
    }
}
```

---

## 5. Build System Integration

### CMake Command Flow

```
┌─────────────────────────────────┐
│ OrchestraManager::buildProject  │
└────────────┬────────────────────┘
             │
   ┌─────────▼─────────┐
   │ BuildSystem       │
   │ .build()          │
   └────────┬──────────┘
            │
   ┌────────▼──────────┐
   │ 1. CMake Configure│
   │    (if needed)    │
   └────────┬──────────┘
            │
   ┌────────▼──────────────────────┐
   │ 2. Ninja build                │
   │    (with streaming output)    │
   └────────┬───────────────────────┘
            │
   ┌────────▼──────────────────────┐
   │ 3. Parse errors & warnings    │
   │    from build output          │
   └────────┬───────────────────────┘
            │
   ┌────────▼──────────────────────┐
   │ 4. Emit buildOutput signal    │
   │    for each line (real-time)  │
   └────────┬───────────────────────┘
            │
   ┌────────▼──────────────────────┐
   │ 5. Emit buildFinished signal  │
   │    with success/failure       │
   └───────────────────────────────┘
```

---

## 6. Model & AI Integration

### Inference Pipeline

```
User Request (CLI or GUI)
    │
    ▼
OrchestraManager::aiInfer(prompt)
    │
    ├─ Load model (if not cached)
    │  └─ Check ModelRegistry for cached model
    │
    ├─ Tokenize input
    │
    ├─ Token generation loop
    │  ├─ Generate next token
    │  ├─ Emit aiInferenceToken signal
    │  ├─ (CLI) Print token immediately
    │  ├─ (GUI) Append to chat panel in real-time
    │  └─ Continue until EOS or max tokens
    │
    ├─ Emit aiInferenceFinished signal
    │
    └─ Return complete response
```

### Model Registry Pattern

```cpp
class ModelRegistry {
    static ModelRegistry& instance();
    
    // Load model if not already loaded
    std::shared_ptr<InferenceEngine> getModel(const QString& modelPath) {
        if (m_cache.contains(modelPath)) {
            return m_cache[modelPath];
        }
        
        auto engine = std::make_shared<InferenceEngine>();
        if (!engine->loadModel(modelPath)) {
            return nullptr;
        }
        
        m_cache[modelPath] = engine;
        return engine;
    }
    
private:
    QMap<QString, std::shared_ptr<InferenceEngine>> m_cache;
};
```

---

## 7. Terminal Integration

### Terminal Pool Management

```cpp
// In OrchestraManager
class TerminalPool {
    int createTerminal() {
        int id = m_nextId++;
        m_terminals[id] = new QProcess();
        return id;
    }
    
    void executeCommand(int terminalId, const QString& command) {
        if (!m_terminals.contains(terminalId)) return;
        
        auto proc = m_terminals[terminalId];
        connect(proc, QOverload<>::of(&QProcess::readyReadStandardOutput),
                this, [this, terminalId, proc]() {
            emit terminalOutput(terminalId, QString::fromUtf8(proc->readAllStandardOutput()));
        });
        
        proc->start(command);
    }
};
```

---

## 8. Error Handling Architecture

### Unified Error Pipeline

```cpp
// All errors flow through OrchestraManager signals
void OrchestraManager::onBuildError(const QString& errorMsg) {
    // Emit buildOutput with error flag
    emit buildOutput(errorMsg, true);  // isError = true
    
    // Also emit general error signal
    emit error("BuildSystem", errorMsg);
}

// CLI catches through connected signal
connect(&om, &OrchestraManager::buildOutput, this, 
    [](const QString& line, bool isError) {
        if (isError) {
            printError(line);
        }
    });

// GUI catches through connected signal
connect(&om, &OrchestraManager::buildOutput, this,
    [this](const QString& line, bool isError) {
        if (isError) {
            m_problemsPanel->addError(line);  // Parses and categorizes
        }
        m_terminal->appendOutput(line);
    });
```

---

## 9. Configuration & Settings

### Settings Persistence Pattern

```cpp
void MainWindow_v5::saveWindowState() {
    QSettings settings("RawrXD", "IDE");
    settings.beginGroup("MainWindow");
    settings.setValue("geometry", saveGeometry());
    settings.setValue("windowState", saveState());
    settings.endGroup();
    
    settings.beginGroup("Editor");
    settings.setValue("fontSize", m_editorFontSize);
    settings.setValue("theme", m_currentTheme);
    settings.setValue("font", m_editorFont);
    settings.endGroup();
    
    settings.beginGroup("Project");
    settings.setValue("lastOpenedProject", OrchestraManager::instance().currentProjectPath());
    settings.setValue("recentProjects", m_recentProjects);
    settings.endGroup();
}

void MainWindow_v5::restoreWindowState() {
    QSettings settings("RawrXD", "IDE");
    settings.beginGroup("MainWindow");
    restoreGeometry(settings.value("geometry").toByteArray());
    restoreState(settings.value("windowState").toByteArray());
    settings.endGroup();
    // ... restore other settings
}
```

---

## 10. Performance Optimizations

### Lazy Loading

```cpp
// Only load expensive subsystems when needed
class OrchestraManager {
    InferenceEngine* inferenceEngine() {
        if (!m_inferenceEngine) {
            m_inferenceEngine = new InferenceEngine();
            m_inferenceEngine->initialize();
        }
        return m_inferenceEngine;
    }
};

// Model caching
class ModelRegistry {
    std::shared_ptr<InferenceEngine> getModel(const QString& path) {
        if (m_cache.contains(path)) {
            return m_cache[path];  // Return cached
        }
        // Load and cache
        auto model = loadModel(path);
        m_cache[path] = model;
        return model;
    }
};
```

### Async Operations

```cpp
// Long-running operations don't block UI
void OrchestraManager::buildProject(const QString& path, const QString& config) {
    QThread* buildThread = new QThread();
    
    auto buildWorker = new BuildWorker(path, config);
    buildWorker->moveToThread(buildThread);
    
    connect(buildThread, &QThread::started, buildWorker, &BuildWorker::run);
    connect(buildWorker, &BuildWorker::output, this, &OrchestraManager::buildOutput);
    connect(buildWorker, &BuildWorker::finished, this, [this]() {
        emit buildFinished(true, "Build complete");
    });
    
    buildThread->start();
}
```

---

## CONCLUSION

The RawrXD IDE architecture provides:

1. **Unified Backend** (OrchestraManager) - single point of control
2. **Dual Frontend** (CLI & GUI) - same functionality, different UX
3. **Extensible Design** - easy to add new commands/panels
4. **Production Ready** - error handling, logging, performance optimizations
5. **Language Support** - integrated 48+ language framework
6. **Async Operations** - non-blocking long operations
7. **Signal/Slot Architecture** - clean Qt event system

This design ensures that:
- New features added to OrchestraManager automatically appear in both CLI & GUI
- CLI provides programmatic access for CI/CD
- GUI provides rich visual experience for developers
- Code is maintainable and extensible for future enhancements

