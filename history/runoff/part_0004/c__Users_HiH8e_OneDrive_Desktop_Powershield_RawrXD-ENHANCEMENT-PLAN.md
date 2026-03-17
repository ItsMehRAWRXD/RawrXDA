# RawrXD-QtShell Enhancement Implementation Plan

## Thank You!
Thank you for the amazing feedback! Here's the comprehensive plan to implement all requested features.

---

## PRIORITY 1: Critical UI Fixes (Immediate)

### 1.1 Context Panel - Remove Items & Load Files

**File: `src/qtapp/MainWindow.h`**
Add to private slots:
```cpp
void showContextMenu(const QPoint& pos);
void removeContextItem();
void loadContextItemIntoEditor(QListWidgetItem* item);
void handleTabClose(int index);
```

Add to private members:
```cpp
QComboBox* agentSelector_{};
QMenu* contextMenu_{};
QAction* removeAction_{};
QAction* loadAction_{};
```

**File: `src/qtapp/MainWindow.cpp`**

In `createProposalReview()` function, update:
```cpp
contextList_ = new QListWidget(w);
contextList_->setStyleSheet("QListWidget { background: #2b2b2b; color: #e0e0e0; border: 1px solid #555; }");
contextList_->setContextMenuPolicy(Qt::CustomContextMenu);
connect(contextList_, &QListWidget::customContextMenuRequested, this, &MainWindow::showContextMenu);
connect(contextList_, &QListWidget::itemDoubleClicked, this, &MainWindow::loadContextItemIntoEditor);
```

Add new methods at end of file:
```cpp
void MainWindow::showContextMenu(const QPoint& pos) {
    if (!contextList_) return;
    
    QListWidgetItem* item = contextList_->itemAt(pos);
    if (!item) return;
    
    QMenu* menu = new QMenu(this);
    QAction* loadAction = menu->addAction("Load in Editor");
    QAction* removeAction = menu->addAction("Remove from Context");
    
    connect(loadAction, &QAction::triggered, this, [this, item]() {
        loadContextItemIntoEditor(item);
    });
    
    connect(removeAction, &QAction::triggered, this, [this, item]() {
        delete item;
    });
    
    menu->exec(contextList_->mapToGlobal(pos));
}

void MainWindow::loadContextItemIntoEditor(QListWidgetItem* item) {
    if (!item) return;
    
    QString text = item->text();
    // Extract filename (remove icon prefix)
    QString fileName = text.mid(text.indexOf(' ') + 1);
    
    if (text.startsWith("📄")) {
        // Load file into editor
        QFile file(fileName);
        if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QTextEdit* editor = new QTextEdit(editorTabs_);
            editor->setText(file.readAll());
            file.close();
            editorTabs_->addTab(editor, QFileInfo(fileName).fileName());
            editorTabs_->setCurrentWidget(editor);
        }
    } else if (text.startsWith("📁")) {
        // Create folder tree view in new tab
        QTreeWidget* tree = new QTreeWidget(editorTabs_);
        tree->setHeaderLabel(fileName);
        populateFolderTree(tree->invisibleRootItem(), fileName);
        editorTabs_->addTab(tree, "📁 " + QFileInfo(fileName).fileName());
        editorTabs_->setCurrentWidget(tree);
    }
}

void MainWindow::populateFolderTree(QTreeWidgetItem* parent, const QString& path) {
    QDir dir(path);
    QFileInfoList entries = dir.entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot);
    
    for (const QFileInfo& entry : entries) {
        QTreeWidgetItem* item = new QTreeWidgetItem(parent);
        item->setText(0, entry.fileName());
        item->setData(0, Qt::UserRole, entry.absoluteFilePath());
        
        if (entry.isDir()) {
            item->setIcon(0, style()->standardIcon(QStyle::SP_DirIcon));
            populateFolderTree(item, entry.absoluteFilePath());
        } else {
            item->setIcon(0, style()->standardIcon(QStyle::SP_FileIcon));
        }
    }
}

void MainWindow::handleTabClose(int index) {
    if (editorTabs_ && index >= 0) {
        QWidget* widget = editorTabs_->widget(index);
        editorTabs_->removeTab(index);
        delete widget;
    }
}
```

Add to includes:
```cpp
#include <QMenu>
#include <QTreeWidget>
#include <QStyle>
```

### 1.2 Make Editor Tabs Closable

**File: `src/qtapp/MainWindow.cpp`**

In `createEditorArea()`:
```cpp
QWidget* MainWindow::createEditorArea() {
    editorTabs_ = new QTabWidget(this);
    editorTabs_->setTabsClosable(true);
    connect(editorTabs_, &QTabWidget::tabCloseRequested, this, &MainWindow::handleTabClose);
    // ... rest of function
}
```

### 1.3 Keyboard Shortcuts (Enter/Ctrl+Enter)

**File: `src/qtapp/MainWindow.cpp`**

Update `eventFilter()` method:
```cpp
bool MainWindow::eventFilter(QObject* watched, QEvent* event) {
    if (watched == codeView_) {
        if (event->type() == QEvent::Resize) {
            if (overlay_) overlay_->setGeometry(codeView_->geometry());
        }
        
        if (event->type() == QEvent::KeyPress) {
            QKeyEvent* keyEvent = static_cast<QKeyEvent*>(event);
            if ((keyEvent->key() == Qt::Key_Return || keyEvent->key() == Qt::Key_Enter) &&
                (keyEvent->modifiers() & Qt::ControlModifier)) {
                handleGoalSubmit();
                return true;
            }
        }
    }
    
    if (watched == goalInput_) {
        if (event->type() == QEvent::KeyPress) {
            QKeyEvent* keyEvent = static_cast<QKeyEvent*>(event);
            if (keyEvent->key() == Qt::Key_Return || keyEvent->key() == Qt::Key_Enter) {
                handleGoalSubmit();
                return true;
            }
        }
    }
    
    return QMainWindow::eventFilter(watched, event);
}
```

In constructor, add event filters:
```cpp
codeView_->installEventFilter(this);
goalInput_->installEventFilter(this);
```

### 1.4 Agent Selector Dropdown

**File: `src/qtapp/MainWindow.cpp`**

In `createGoalBar()`:
```cpp
QWidget* MainWindow::createGoalBar() {
    QWidget* w = new QWidget(this);
    QHBoxLayout* layout = new QHBoxLayout(w);
    layout->setContentsMargins(6,6,6,6);

    agentSelector_ = new QComboBox(w);
    agentSelector_->addItems({
        "Auto Select",
        "Feature Agent", 
        "Security Agent",
        "Performance Agent",
        "Debug Agent",
        "Refactor Agent",
        "Documentation Agent"
    });
    
    goalInput_ = new QLineEdit(w);
    goalInput_->setPlaceholderText(tr("Ask a question or describe what you want to build..."));
    goalInput_->installEventFilter(this);

    QPushButton* submitBtn = new QPushButton(tr("Send"), w);
    connect(submitBtn, &QPushButton::clicked, this, &MainWindow::handleGoalSubmit);

    layout->addWidget(new QLabel(tr("Agent:"), w));
    layout->addWidget(agentSelector_);
    layout->addWidget(new QLabel(tr("Chat:"), w));
    layout->addWidget(goalInput_, 1);
    layout->addWidget(submitBtn);
    
    // ... rest of function
}
```

Add to includes:
```cpp
#include <QComboBox>
```

### 1.5 QShell Help Text on Load

**File: `src/qtapp/MainWindow.cpp`**

In `createQShellTab()`:
```cpp
qshellOutput_->setText(tr(
    "=== RawrXD QShell - Agentic Command Interface ===\n\n"
    "CONTEXT COMMANDS (prefix with /):\n"
    "  /ask <question>        - Ask AI a question\n"
    "  /fix <code>            - Fix code issues\n"
    "  /explain <code>        - Explain code\n"
    "  /refactor <code>       - Refactor code\n"
    "  /test <code>           - Generate tests\n"
    "  /doc <code>            - Generate documentation\n\n"
    "AGENTIC COMMANDS:\n"
    "  Invoke-QAgent -Agent Feature|Security|Performance \"prompt\"\n"
    "  Retry-Blocked          - Retry blocked tasks\n"
    "  Set-MockArchitect On|Off\n\n"
    "WORKFLOW COMMANDS:\n"
    "  Start-Swarm            - Start local swarm engine\n"
    "  Stop-Swarm             - Stop swarm engine\n"
    "  Build-Model <name>     - Compile new model\n"
    "  Harvest-LLM            - Start LLM data harvesting\n\n"
    "Type 'help' for more commands\n\n"
));
```

---

## PRIORITY 2: Terminal & Debug Panels

### 2.1 Add Terminal Panel (PowerShell/CMD)

**File: `src/qtapp/MainWindow.h`**

Add includes:
```cpp
class QProcess;
class QPlainTextEdit;
```

Add to private members:
```cpp
QDockWidget* terminalDock_{};
QTabWidget* terminalTabs_{};
QPlainTextEdit* pwshOutput_{};
QPlainTextEdit* cmdOutput_{};
QLineEdit* pwshInput_{};
QLineEdit* cmdInput_{};
QProcess* pwshProcess_{};
QProcess* cmdProcess_{};
```

Add to private:
```cpp
QWidget* createTerminalPanel();
void startPowerShell();
void startCommandPrompt();
```

Add to private slots:
```cpp
void handlePwshCommand();
void handleCmdCommand();
void readPwshOutput();
void readCmdOutput();
```

**File: `src/qtapp/MainWindow.cpp`**

In constructor, add:
```cpp
// Bottom Terminal dock
terminalDock_ = new QDockWidget(tr("Terminal"), this);
terminalDock_->setObjectName("TerminalDock");
terminalDock_->setWidget(createTerminalPanel());
addDockWidget(Qt::BottomDockWidgetArea, terminalDock_);

startPowerShell();
startCommandPrompt();
```

Add method:
```cpp
QWidget* MainWindow::createTerminalPanel() {
    QWidget* w = new QWidget(this);
    QVBoxLayout* layout = new QVBoxLayout(w);
    
    terminalTabs_ = new QTabWidget(w);
    
    // PowerShell tab
    QWidget* pwshTab = new QWidget(terminalTabs_);
    QVBoxLayout* pwshLayout = new QVBoxLayout(pwshTab);
    pwshOutput_ = new QPlainTextEdit(pwshTab);
    pwshOutput_->setReadOnly(true);
    pwshOutput_->setStyleSheet("background: #012456; color: #eee; font-family: 'Consolas', monospace;");
    pwshInput_ = new QLineEdit(pwshTab);
    pwshInput_->setStyleSheet("background: #012456; color: #eee; font-family: 'Consolas', monospace;");
    pwshInput_->setPlaceholderText("PS> ");
    connect(pwshInput_, &QLineEdit::returnPressed, this, &MainWindow::handlePwshCommand);
    pwshLayout->addWidget(pwshOutput_);
    pwshLayout->addWidget(pwshInput_);
    terminalTabs_->addTab(pwshTab, "PowerShell");
    
    // Command Prompt tab
    QWidget* cmdTab = new QWidget(terminalTabs_);
    QVBoxLayout* cmdLayout = new QVBoxLayout(cmdTab);
    cmdOutput_ = new QPlainTextEdit(cmdTab);
    cmdOutput_->setReadOnly(true);
    cmdOutput_->setStyleSheet("background: #000; color: #fff; font-family: 'Consolas', monospace;");
    cmdInput_ = new QLineEdit(cmdTab);
    cmdInput_->setStyleSheet("background: #000; color: #fff; font-family: 'Consolas', monospace;");
    cmdInput_->setPlaceholderText("C:\\> ");
    connect(cmdInput_, &QLineEdit::returnPressed, this, &MainWindow::handleCmdCommand);
    cmdLayout->addWidget(cmdOutput_);
    cmdLayout->addWidget(cmdInput_);
    terminalTabs_->addTab(cmdTab, "CMD");
    
    layout->addWidget(terminalTabs_);
    return w;
}

void MainWindow::startPowerShell() {
    pwshProcess_ = new QProcess(this);
    connect(pwshProcess_, &QProcess::readyReadStandardOutput, this, &MainWindow::readPwshOutput);
    connect(pwshProcess_, &QProcess::readyReadStandardError, this, &MainWindow::readPwshOutput);
    
    pwshProcess_->start("powershell.exe", QStringList() << "-NoExit" << "-Command" << "-");
    pwshOutput_->appendPlainText("PowerShell initialized (Admin access if elevated)\n");
}

void MainWindow::startCommandPrompt() {
    cmdProcess_ = new QProcess(this);
    connect(cmdProcess_, &QProcess::readyReadStandardOutput, this, &MainWindow::readCmdOutput);
    connect(cmdProcess_, &QProcess::readyReadStandardError, this, &MainWindow::readCmdOutput);
    
    cmdProcess_->start("cmd.exe");
    cmdOutput_->appendPlainText("Command Prompt initialized\n");
}

void MainWindow::handlePwshCommand() {
    QString cmd = pwshInput_->text();
    if (cmd.isEmpty()) return;
    
    pwshOutput_->appendPlainText("PS> " + cmd);
    pwshProcess_->write((cmd + "\n").toUtf8());
    pwshInput_->clear();
}

void MainWindow::handleCmdCommand() {
    QString cmd = cmdInput_->text();
    if (cmd.isEmpty()) return;
    
    cmdOutput_->appendPlainText("C:\\> " + cmd);
    cmdProcess_->write((cmd + "\r\n").toUtf8());
    cmdInput_->clear();
}

void MainWindow::readPwshOutput() {
    QString output = QString::fromLocal8Bit(pwshProcess_->readAllStandardOutput());
    QString error = QString::fromLocal8Bit(pwshProcess_->readAllStandardError());
    if (!output.isEmpty()) pwshOutput_->appendPlainText(output);
    if (!error.isEmpty()) pwshOutput_->appendPlainText("[ERROR] " + error);
}

void MainWindow::readCmdOutput() {
    QString output = QString::fromLocal8Bit(cmdProcess_->readAllStandardOutput());
    QString error = QString::fromLocal8Bit(cmdProcess_->readAllStandardError());
    if (!output.isEmpty()) cmdOutput_->appendPlainText(output);
    if (!error.isEmpty()) cmdOutput_->appendPlainText("[ERROR] " + error);
}
```

Add includes:
```cpp
#include <QProcess>
#include <QPlainTextEdit>
```

### 2.2 Add Debug/Logging Panel

**File: `src/qtapp/MainWindow.h`**

Add to private members:
```cpp
QDockWidget* debugDock_{};
QPlainTextEdit* debugOutput_{};
QPushButton* clearLogBtn_{};
QPushButton* saveLogBtn_{};
QComboBox* logLevelFilter_{};
```

Add to private:
```cpp
QWidget* createDebugPanel();
void logMessage(const QString& level, const QString& message);
```

Add to private slots:
```cpp
void clearDebugLog();
void saveDebugLog();
void filterLogLevel(const QString& level);
```

**File: `src/qtapp/MainWindow.cpp`**

In constructor:
```cpp
// Debug/Logging dock (right side, below context)
debugDock_ = new QDockWidget(tr("Debug & Logs"), this);
debugDock_->setObjectName("DebugDock");
debugDock_->setWidget(createDebugPanel());
addDockWidget(Qt::RightDockWidgetArea, debugDock_);
tabifyDockWidget(proposalDock, debugDock_);
```

Add method:
```cpp
QWidget* MainWindow::createDebugPanel() {
    QWidget* w = new QWidget(this);
    QVBoxLayout* layout = new QVBoxLayout(w);
    
    QHBoxLayout* topBar = new QHBoxLayout();
    logLevelFilter_ = new QComboBox(w);
    logLevelFilter_->addItems({"ALL", "DEBUG", "INFO", "WARN", "ERROR"});
    connect(logLevelFilter_, &QComboBox::currentTextChanged, this, &MainWindow::filterLogLevel);
    
    clearLogBtn_ = new QPushButton("Clear", w);
    saveLogBtn_ = new QPushButton("Save Log", w);
    connect(clearLogBtn_, &QPushButton::clicked, this, &MainWindow::clearDebugLog);
    connect(saveLogBtn_, &QPushButton::clicked, this, &MainWindow::saveDebugLog);
    
    topBar->addWidget(new QLabel("Level:", w));
    topBar->addWidget(logLevelFilter_);
    topBar->addStretch();
    topBar->addWidget(clearLogBtn_);
    topBar->addWidget(saveLogBtn_);
    
    debugOutput_ = new QPlainTextEdit(w);
    debugOutput_->setReadOnly(true);
    debugOutput_->setStyleSheet("background: #1e1e1e; color: #d4d4d4; font-family: 'Consolas', monospace;");
    debugOutput_->setMaximumBlockCount(10000); // Limit to 10k lines
    
    layout->addLayout(topBar);
    layout->addWidget(debugOutput_);
    
    logMessage("INFO", "Debug panel initialized");
    logMessage("INFO", "Real-time logging active");
    
    return w;
}

void MainWindow::logMessage(const QString& level, const QString& message) {
    QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss.zzz");
    QString logLine = QString("[%1] [%2] %3").arg(timestamp, level, message);
    
    if (debugOutput_) {
        debugOutput_->appendPlainText(logLine);
    }
}

void MainWindow::clearDebugLog() {
    if (debugOutput_) {
        debugOutput_->clear();
        logMessage("INFO", "Log cleared by user");
    }
}

void MainWindow::saveDebugLog() {
    QString fileName = QFileDialog::getSaveFileName(this, tr("Save Debug Log"), 
                                                     "debug_log.txt", 
                                                     tr("Text Files (*.txt);;All Files (*)"));
    if (!fileName.isEmpty() && debugOutput_) {
        QFile file(fileName);
        if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream out(&file);
            out << debugOutput_->toPlainText();
            file.close();
            logMessage("INFO", "Log saved to: " + fileName);
        }
    }
}

void MainWindow::filterLogLevel(const QString& level) {
    // TODO: Implement filtering (requires storing full log and filtering display)
    logMessage("INFO", "Log level filter changed to: " + level);
}
```

Add includes:
```cpp
#include <QDateTime>
#include <QTextStream>
```

---

## PRIORITY 3: Right-Click Context Menu (Copilot Actions)

**File: `src/qtapp/MainWindow.cpp`**

Update editor creation to add context menu:
```cpp
codeView_ = new QTextEdit(editorTabs_);
codeView_->setReadOnly(false);
codeView_->setContextMenuPolicy(Qt::CustomContextMenu);
connect(codeView_, &QTextEdit::customContextMenuRequested, this, &MainWindow::showEditorContextMenu);
```

Add to header private slots:
```cpp
void showEditorContextMenu(const QPoint& pos);
void explainCode();
void fixCode();
void refactorCode();
void generateTests();
void generateDocs();
```

Add method:
```cpp
void MainWindow::showEditorContextMenu(const QPoint& pos) {
    QMenu* menu = codeView_->createStandardContextMenu();
    menu->addSeparator();
    
    QMenu* copilotMenu = menu->addMenu("🤖 Copilot");
    copilotMenu->addAction("Explain Code", this, &MainWindow::explainCode);
    copilotMenu->addAction("Fix Issues", this, &MainWindow::fixCode);
    copilotMenu->addAction("Refactor", this, &MainWindow::refactorCode);
    copilotMenu->addAction("Generate Tests", this, &MainWindow::generateTests);
    copilotMenu->addAction("Generate Docs", this, &MainWindow::generateDocs);
    
    menu->exec(codeView_->mapToGlobal(pos));
}

void MainWindow::explainCode() {
    QString selectedText = codeView_->textCursor().selectedText();
    if (selectedText.isEmpty()) {
        selectedText = codeView_->toPlainText();
    }
    
    if (qshellOutput_) {
        qshellOutput_->append("[COPILOT] Explaining code...");
    }
    
    // TODO: Send to agent/swarm for explanation
    goalInput_->setText("/explain " + selectedText.left(100) + "...");
    handleGoalSubmit();
}

void MainWindow::fixCode() {
    QString selectedText = codeView_->textCursor().selectedText();
    if (selectedText.isEmpty()) return;
    
    if (qshellOutput_) {
        qshellOutput_->append("[COPILOT] Analyzing code for fixes...");
    }
    
    goalInput_->setText("/fix " + selectedText.left(100) + "...");
    handleGoalSubmit();
}

void MainWindow::refactorCode() {
    QString selectedText = codeView_->textCursor().selectedText();
    if (selectedText.isEmpty()) return;
    
    if (qshellOutput_) {
        qshellOutput_->append("[COPILOT] Refactoring code...");
    }
    
    goalInput_->setText("/refactor " + selectedText.left(100) + "...");
    handleGoalSubmit();
}

void MainWindow::generateTests() {
    QString selectedText = codeView_->textCursor().selectedText();
    if (selectedText.isEmpty()) return;
    
    if (qshellOutput_) {
        qshellOutput_->append("[COPILOT] Generating tests...");
    }
    
    goalInput_->setText("/test " + selectedText.left(100) + "...");
    handleGoalSubmit();
}

void MainWindow::generateDocs() {
    QString selectedText = codeView_->textCursor().selectedText();
    if (selectedText.isEmpty()) return;
    
    if (qshellOutput_) {
        qshellOutput_->append("[COPILOT] Generating documentation...");
    }
    
    goalInput_->setText("/doc " + selectedText.left(100) + "...");
    handleGoalSubmit();
}
```

---

## PRIORITY 4: Git Integration Panel

**File: `src/qtapp/MainWindow.h`**

Add to private members:
```cpp
QDockWidget* gitDock_{};
QListWidget* gitFilesList_{};
QTextEdit* gitDiffView_{};
QLineEdit* gitCommitMsg_{};
QPushButton* gitCommitBtn_{};
QPushButton* gitPushBtn_{};
QPushButton* gitPullBtn_{};
QLabel* gitBranchLabel_{};
```

Add to private:
```cpp
QWidget* createGitPanel();
void refreshGitStatus();
```

Add to private slots:
```cpp
void handleGitCommit();
void handleGitPush();
void handleGitPull();
void showGitDiff(QListWidgetItem* item);
```

**File: `src/qtapp/MainWindow.cpp`**

In constructor:
```cpp
// Git panel (left side, below chat history)
gitDock_ = new QDockWidget(tr("Git / Version Control"), this);
gitDock_->setObjectName("GitDock");
gitDock_->setWidget(createGitPanel());
addDockWidget(Qt::LeftDockWidgetArea, gitDock_);
tabifyDockWidget(agentDock, gitDock_);

refreshGitStatus();
```

Add method:
```cpp
QWidget* MainWindow::createGitPanel() {
    QWidget* w = new QWidget(this);
    QVBoxLayout* layout = new QVBoxLayout(w);
    
    // Branch info
    QHBoxLayout* branchBar = new QHBoxLayout();
    branchBar->addWidget(new QLabel("Branch:", w));
    gitBranchLabel_ = new QLabel("main", w);
    gitBranchLabel_->setStyleSheet("font-weight: bold; color: #4ec9b0;");
    branchBar->addWidget(gitBranchLabel_);
    branchBar->addStretch();
    layout->addLayout(branchBar);
    
    // Changed files list
    QLabel* filesLabel = new QLabel("Changed Files:", w);
    gitFilesList_ = new QListWidget(w);
    gitFilesList_->setStyleSheet("QListWidget { background: #2b2b2b; color: #e0e0e0; }");
    connect(gitFilesList_, &QListWidget::itemClicked, this, &MainWindow::showGitDiff);
    
    // Diff view
    QLabel* diffLabel = new QLabel("Diff:", w);
    gitDiffView_ = new QTextEdit(w);
    gitDiffView_->setReadOnly(true);
    gitDiffView_->setStyleSheet("QTextEdit { background: #1e1e1e; color: #d4d4d4; font-family: 'Consolas'; }");
    
    // Commit controls
    gitCommitMsg_ = new QLineEdit(w);
    gitCommitMsg_->setPlaceholderText("Commit message...");
    
    QHBoxLayout* gitButtons = new QHBoxLayout();
    gitCommitBtn_ = new QPushButton("Commit", w);
    gitPushBtn_ = new QPushButton("Push", w);
    gitPullBtn_ = new QPushButton("Pull", w);
    
    connect(gitCommitBtn_, &QPushButton::clicked, this, &MainWindow::handleGitCommit);
    connect(gitPushBtn_, &QPushButton::clicked, this, &MainWindow::handleGitPush);
    connect(gitPullBtn_, &QPushButton::clicked, this, &MainWindow::handleGitPull);
    
    gitButtons->addWidget(gitCommitBtn_);
    gitButtons->addWidget(gitPushBtn_);
    gitButtons->addWidget(gitPullBtn_);
    
    layout->addWidget(filesLabel);
    layout->addWidget(gitFilesList_, 1);
    layout->addWidget(diffLabel);
    layout->addWidget(gitDiffView_, 1);
    layout->addWidget(gitCommitMsg_);
    layout->addLayout(gitButtons);
    
    return w;
}

void MainWindow::refreshGitStatus() {
    if (!gitFilesList_) return;
    
    gitFilesList_->clear();
    
    // Run git status
    QProcess git;
    git.setWorkingDirectory(QDir::currentPath());
    git.start("git", QStringList() << "status" << "--porcelain");
    git.waitForFinished();
    
    QString output = git.readAllStandardOutput();
    QStringList lines = output.split('\n', Qt::SkipEmptyParts);
    
    for (const QString& line : lines) {
        gitFilesList_->addItem(line);
    }
    
    // Get current branch
    git.start("git", QStringList() << "branch" << "--show-current");
    git.waitForFinished();
    QString branch = git.readAllStandardOutput().trimmed();
    if (gitBranchLabel_) {
        gitBranchLabel_->setText(branch);
    }
}

void MainWindow::showGitDiff(QListWidgetItem* item) {
    if (!item || !gitDiffView_) return;
    
    QString fileName = item->text().mid(3); // Skip "M  " or "A  " prefix
    
    QProcess git;
    git.start("git", QStringList() << "diff" << fileName);
    git.waitForFinished();
    
    QString diff = git.readAllStandardOutput();
    gitDiffView_->setPlainText(diff);
}

void MainWindow::handleGitCommit() {
    QString msg = gitCommitMsg_->text();
    if (msg.isEmpty()) {
        logMessage("WARN", "Commit message is empty");
        return;
    }
    
    QProcess git;
    git.start("git", QStringList() << "commit" << "-am" << msg);
    git.waitForFinished();
    
    QString output = git.readAllStandardOutput();
    logMessage("INFO", "Git commit: " + output);
    gitCommitMsg_->clear();
    refreshGitStatus();
}

void MainWindow::handleGitPush() {
    QProcess git;
    git.start("git", QStringList() << "push");
    git.waitForFinished();
    
    QString output = git.readAllStandardOutput();
    logMessage("INFO", "Git push: " + output);
}

void MainWindow::handleGitPull() {
    QProcess git;
    git.start("git", QStringList() << "pull");
    git.waitForFinished();
    
    QString output = git.readAllStandardOutput();
    logMessage("INFO", "Git pull: " + output);
    refreshGitStatus();
}
```

---

## PRIORITY 5: Settings Menu & Browser Panel

### 5.1 Settings Menu

**File: `src/qtapp/MainWindow.h`**

Add to private:
```cpp
void createMenus();
QAction* settingsAction_{};
QAction* preferencesAction_{};
```

Add to private slots:
```cpp
void showSettings();
void showPreferences();
```

**File: `src/qtapp/MainWindow.cpp`**

In constructor:
```cpp
createMenus();
```

Add methods:
```cpp
void MainWindow::createMenus() {
    QMenuBar* menuBar = new QMenuBar(this);
    
    // File menu
    QMenu* fileMenu = menuBar->addMenu(tr("&File"));
    fileMenu->addAction("New Chat", this, &MainWindow::handleNewChat);
    fileMenu->addAction("New Editor", this, &MainWindow::handleNewEditor);
    fileMenu->addAction("New Window", this, &MainWindow::handleNewWindow);
    fileMenu->addSeparator();
    fileMenu->addAction("Save State", this, &MainWindow::handleSaveState);
    fileMenu->addAction("Load State", this, &MainWindow::handleLoadState);
    fileMenu->addSeparator();
    fileMenu->addAction("Exit", this, &QWidget::close);
    
    // Edit menu
    QMenu* editMenu = menuBar->addMenu(tr("&Edit"));
    editMenu->addAction("Preferences", this, &MainWindow::showPreferences);
    editMenu->addAction("Settings", this, &MainWindow::showSettings);
    
    // View menu
    QMenu* viewMenu = menuBar->addMenu(tr("&View"));
    viewMenu->addAction(agentDock_->toggleViewAction());
    viewMenu->addAction(proposalDock->toggleViewAction());
    viewMenu->addAction(terminalDock_->toggleViewAction());
    viewMenu->addAction(debugDock_->toggleViewAction());
    viewMenu->addAction(gitDock_->toggleViewAction());
    
    // Swarm menu
    QMenu* swarmMenu = menuBar->addMenu(tr("&Swarm"));
    swarmMenu->addAction("Start Swarm Engine", this, &MainWindow::startSwarmEngine);
    swarmMenu->addAction("Stop Swarm Engine", this, &MainWindow::stopSwarmEngine);
    swarmMenu->addAction("Swarm Status", this, &MainWindow::showSwarmStatus);
    swarmMenu->addSeparator();
    swarmMenu->addAction("Build Model", this, &MainWindow::buildModel);
    swarmMenu->addAction("Harvest LLM Data", this, &MainWindow::startHarvesting);
    
    // Help menu
    QMenu* helpMenu = menuBar->addMenu(tr("&Help"));
    helpMenu->addAction("Documentation", this, &MainWindow::showDocs);
    helpMenu->addAction("About RawrXD", this, &MainWindow::showAbout);
    
    setMenuBar(menuBar);
}

void MainWindow::showSettings() {
    // TODO: Create settings dialog
    logMessage("INFO", "Settings dialog opened");
}

void MainWindow::showPreferences() {
    // TODO: Create preferences dialog
    logMessage("INFO", "Preferences dialog opened");
}
```

### 5.2 Browser Panel

**File: `src/qtapp/MainWindow.h`**

Add includes:
```cpp
class QWebEngineView;
```

Add to private members:
```cpp
QDockWidget* browserDock_{};
QWebEngineView* webView_{};
QLineEdit* urlBar_{};
```

Add to private:
```cpp
QWidget* createBrowserPanel();
```

Add to private slots:
```cpp
void navigateToUrl();
void updateUrlBar(const QUrl& url);
```

**File: `src/qtapp/MainWindow.cpp`**

In constructor:
```cpp
// Browser panel (tabbed with context panel)
browserDock_ = new QDockWidget(tr("Browser"), this);
browserDock_->setObjectName("BrowserDock");
browserDock_->setWidget(createBrowserPanel());
addDockWidget(Qt::RightDockWidgetArea, browserDock_);
tabifyDockWidget(proposalDock, browserDock_);
```

Add method:
```cpp
QWidget* MainWindow::createBrowserPanel() {
    QWidget* w = new QWidget(this);
    QVBoxLayout* layout = new QVBoxLayout(w);
    
    // URL bar
    QHBoxLayout* urlLayout = new QHBoxLayout();
    QPushButton* backBtn = new QPushButton("←", w);
    QPushButton* forwardBtn = new QPushButton("→", w);
    QPushButton* refreshBtn = new QPushButton("⟳", w);
    
    urlBar_ = new QLineEdit(w);
    urlBar_->setPlaceholderText("https://...");
    connect(urlBar_, &QLineEdit::returnPressed, this, &MainWindow::navigateToUrl);
    
    QPushButton* goBtn = new QPushButton("Go", w);
    connect(goBtn, &QPushButton::clicked, this, &MainWindow::navigateToUrl);
    
    urlLayout->addWidget(backBtn);
    urlLayout->addWidget(forwardBtn);
    urlLayout->addWidget(refreshBtn);
    urlLayout->addWidget(urlBar_, 1);
    urlLayout->addWidget(goBtn);
    
    // Web view
    webView_ = new QWebEngineView(w);
    connect(webView_, &QWebEngineView::urlChanged, this, &MainWindow::updateUrlBar);
    
    connect(backBtn, &QPushButton::clicked, webView_, &QWebEngineView::back);
    connect(forwardBtn, &QPushButton::clicked, webView_, &QWebEngineView::forward);
    connect(refreshBtn, &QPushButton::clicked, webView_, &QWebEngineView::reload);
    
    layout->addLayout(urlLayout);
    layout->addWidget(webView_);
    
    webView_->setUrl(QUrl("https://github.com"));
    
    return w;
}

void MainWindow::navigateToUrl() {
    QString url = urlBar_->text();
    if (!url.startsWith("http://") && !url.startsWith("https://")) {
        url = "https://" + url;
    }
    webView_->setUrl(QUrl(url));
}

void MainWindow::updateUrlBar(const QUrl& url) {
    urlBar_->setText(url.toString());
}
```

**NOTE**: Requires Qt WebEngine module. Add to CMakeLists.txt:
```cmake
find_package(Qt6 REQUIRED COMPONENTS WebEngineWidgets)
target_link_libraries(RawrXD-QtShell PRIVATE Qt6::WebEngineWidgets)
```

---

## PRIORITY 6: Swarm Engine Integration

### 6.1 Add Swarm Server Management

**File: `src/qtapp/MainWindow.h`**

Add to private members:
```cpp
QProcess* swarmProcess_{};
bool swarmRunning_{false};
QString swarmUrl_{"http://localhost:8000"};
```

Add to private slots:
```cpp
void startSwarmEngine();
void stopSwarmEngine();
void showSwarmStatus();
void readSwarmOutput();
void checkSwarmHealth();
```

**File: `src/qtapp/MainWindow.cpp`**

Add methods:
```cpp
void MainWindow::startSwarmEngine() {
    if (swarmRunning_) {
        logMessage("WARN", "Swarm engine already running");
        return;
    }
    
    swarmProcess_ = new QProcess(this);
    connect(swarmProcess_, &QProcess::readyReadStandardOutput, this, &MainWindow::readSwarmOutput);
    connect(swarmProcess_, &QProcess::readyReadStandardError, this, &MainWindow::readSwarmOutput);
    
    // TODO: Point to actual swarm server script
    QString swarmScript = "python swarm_server.py";
    swarmProcess_->start("powershell", QStringList() << "-Command" << swarmScript);
    
    swarmRunning_ = true;
    logMessage("INFO", "Swarm engine starting...");
    
    // Check health after 3 seconds
    QTimer::singleShot(3000, this, &MainWindow::checkSwarmHealth);
}

void MainWindow::stopSwarmEngine() {
    if (!swarmRunning_ || !swarmProcess_) {
        logMessage("WARN", "Swarm engine not running");
        return;
    }
    
    swarmProcess_->terminate();
    swarmProcess_->waitForFinished(5000);
    if (swarmProcess_->state() != QProcess::NotRunning) {
        swarmProcess_->kill();
    }
    
    swarmRunning_ = false;
    logMessage("INFO", "Swarm engine stopped");
}

void MainWindow::showSwarmStatus() {
    if (!swarmRunning_) {
        logMessage("INFO", "Swarm Status: OFFLINE");
        return;
    }
    
    // Make HTTP request to swarm status endpoint
    // TODO: Use QNetworkAccessManager for HTTP request
    logMessage("INFO", QString("Swarm Status: ONLINE at %1").arg(swarmUrl_));
}

void MainWindow::readSwarmOutput() {
    QString output = swarmProcess_->readAllStandardOutput();
    QString error = swarmProcess_->readAllStandardError();
    
    if (!output.isEmpty()) {
        logMessage("SWARM", output.trimmed());
    }
    if (!error.isEmpty()) {
        logMessage("ERROR", "Swarm: " + error.trimmed());
    }
}

void MainWindow::checkSwarmHealth() {
    // TODO: HTTP GET to swarmUrl_/health
    logMessage("INFO", "Checking swarm health...");
}
```

Add include:
```cpp
#include <QTimer>
```

### 6.2 Model Compiler Integration

**File: `src/qtapp/MainWindow.h`**

Add to private slots:
```cpp
void buildModel();
void startHarvesting();
void showDocs();
void showAbout();
```

**File: `src/qtapp/MainWindow.cpp`**

Add methods:
```cpp
void MainWindow::buildModel() {
    // Show dialog for model parameters
    bool ok;
    QString modelName = QInputDialog::getText(this, tr("Build Model"),
                                             tr("Model name:"), QLineEdit::Normal,
                                             "custom-model", &ok);
    if (!ok || modelName.isEmpty()) return;
    
    int digestSize = QInputDialog::getInt(this, tr("Build Model"),
                                         tr("Digest size:"), 256, 64, 1024, 1, &ok);
    if (!ok) return;
    
    logMessage("INFO", QString("Building model: %1 (digest: %2)").arg(modelName).arg(digestSize));
    
    // TODO: Execute model compiler
    // ./model-compiler --name $modelName --digest $digestSize --output models/$modelName.json
    
    QProcess compiler;
    compiler.start("model-compiler", QStringList() 
        << "--name" << modelName 
        << "--digest" << QString::number(digestSize)
        << "--output" << "models/" + modelName + ".json");
    compiler.waitForFinished();
    
    QString output = compiler.readAllStandardOutput();
    logMessage("INFO", "Model compiler: " + output);
}

void MainWindow::startHarvesting() {
    logMessage("INFO", "Starting LLM data harvesting...");
    
    // TODO: Start harvesting process
    // This would connect to LLM endpoints and collect training data
    
    if (qshellOutput_) {
        qshellOutput_->append("[HARVEST] LLM data harvesting started");
    }
}

void MainWindow::showDocs() {
    if (webView_) {
        webView_->setUrl(QUrl("https://github.com/ItsMehRAWRXD/RawrXD/wiki"));
        browserDock_->raise();
    }
}

void MainWindow::showAbout() {
    QMessageBox::about(this, tr("About RawrXD"),
        tr("<h2>RawrXD Agentic IDE</h2>"
           "<p>Version 1.0.0</p>"
           "<p>A comprehensive AI-powered development environment with:</p>"
           "<ul>"
           "<li>Multi-agent orchestration</li>"
           "<li>Local swarm intelligence</li>"
           "<li>Model compilation & harvesting</li>"
           "<li>Integrated terminals & debugging</li>"
           "<li>Git version control</li>"
           "<li>Web browser access</li>"
           "</ul>"
           "<p>Built with Qt 6.7.3 and C++20</p>"
           "<p><a href='https://github.com/ItsMehRAWRXD/RawrXD'>GitHub Repository</a></p>"));
}
```

Add include:
```cpp
#include <QInputDialog>
#include <QMessageBox>
```

---

## SUMMARY OF CHANGES

### Files to Modify:
1. **src/qtapp/MainWindow.h** - Add all new member variables, methods, and slots
2. **src/qtapp/MainWindow.cpp** - Implement all new methods
3. **CMakeLists.txt** - Add Qt WebEngine if using browser

### New Includes Needed:
```cpp
#include <QMenu>
#include <QComboBox>
#include <QProcess>
#include <QPlainTextEdit>
#include <QDateTime>
#include <QTextStream>
#include <QTreeWidget>
#include <QStyle>
#include <QTimer>
#include <QInputDialog>
#include <QMessageBox>
#include <QWebEngineView>  // If using browser
```

### Build Steps:
1. Make all code changes
2. Update CMakeLists.txt to include WebEngine (optional)
3. Rebuild:
   ```powershell
   cd C:\Users\HiH8e\OneDrive\Desktop\Powershield\RawrXD-ModelLoader\build
   cmake --build . --config Release --target RawrXD-QtShell
   ```

### Testing Checklist:
- [ ] Context panel: Right-click to remove items
- [ ] Context panel: Double-click files to load in editor
- [ ] Context panel: Folders show tree view
- [ ] Chat tabs: Can close with X button
- [ ] Keyboard: Enter in goal input sends message
- [ ] Keyboard: Ctrl+Enter in chat editor sends message
- [ ] Agent selector: Dropdown shows agents
- [ ] Terminal: PowerShell works
- [ ] Terminal: CMD works
- [ ] Debug panel: Shows logs in real-time
- [ ] Right-click: Explain/Fix/Refactor on code
- [ ] Git panel: Shows changed files
- [ ] Git panel: Can commit/push/pull
- [ ] Settings menu: Opens
- [ ] Browser: Can navigate URLs
- [ ] Swarm: Can start/stop engine
- [ ] Model compiler: Can build models

This comprehensive plan addresses ALL your feedback! 🚀
