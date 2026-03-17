// MASM Text Editor Qt Wrapper Implementation - PRODUCTION GRADE
// Complete implementation with no placeholders

#include "masm_editor_widget.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QToolButton>
#include <QLabel>
#include <QAction>
#include <QMenu>
#include <QContextMenuEvent>
#include <QTextCursor>
#include <QRegularExpression>
#include <QFontMetrics>
#include <QMessageBox>
#include <QInputDialog>
#include <QFileDialog>
#include <QClipboard>
#include <QApplication>
#include <QScrollBar>
#include <QStandardPaths>
#include <QProcess>
#include <algorithm>

// ============================================================
// Assembly Syntax Highlighter Implementation
// ============================================================

AssemblyHighlighter::AssemblyHighlighter(QTextDocument* parent)
    : QSyntaxHighlighter(parent) {
    
    // Keyword format (blue, bold)
    keywordFormat.setForeground(QColor(86, 156, 214));
    keywordFormat.setFontWeight(QFont::Bold);
    
    // Register format (magenta)
    registerFormat.setForeground(QColor(206, 145, 120));
    registerFormat.setFontWeight(QFont::Bold);
    
    // Comment format (green, italic)
    commentFormat.setForeground(QColor(106, 153, 85));
    commentFormat.setFontItalic(true);
    
    // Label format (red)
    labelFormat.setForeground(QColor(220, 220, 170));
    labelFormat.setFontWeight(QFont::Bold);
    
    // Number format (light green)
    numberFormat.setForeground(QColor(181, 206, 168));
    
    // String format (orange)
    stringFormat.setForeground(QColor(206, 145, 120));
    
    // Directive format (purple)
    directiveFormat.setForeground(QColor(197, 134, 192));
    
    // x64 assembly instructions
    QStringList instructions;
    instructions << "mov" << "movzx" << "movsx" << "lea" << "xchg"
                 << "add" << "sub" << "mul" << "imul" << "div" << "idiv"
                 << "inc" << "dec" << "neg" << "not"
                 << "and" << "or" << "xor" << "shl" << "shr" << "sal" << "sar"
                 << "rol" << "ror" << "rcl" << "rcr"
                 << "push" << "pop" << "pushf" << "popf"
                 << "call" << "ret" << "jmp"
                 << "je" << "jne" << "jz" << "jnz" << "ja" << "jae" << "jb" << "jbe"
                 << "jg" << "jge" << "jl" << "jle" << "js" << "jns" << "jo" << "jno"
                 << "cmp" << "test"
                 << "loop" << "loope" << "loopne"
                 << "rep" << "repe" << "repne" << "repz" << "repnz"
                 << "movsb" << "movsw" << "movsd" << "movsq"
                 << "stosb" << "stosw" << "stosd" << "stosq"
                 << "lodsb" << "lodsw" << "lodsd" << "lodsq"
                 << "scasb" << "scasw" << "scasd" << "scasq"
                 << "cmpsb" << "cmpsw" << "cmpsd" << "cmpsq"
                 << "nop" << "hlt" << "int" << "syscall" << "sysret"
                 << "enter" << "leave"
                 << "cbw" << "cwd" << "cdq" << "cqo"
                 << "setc" << "setnc" << "setz" << "setnz" << "sets" << "setns"
                 << "cmovz" << "cmovnz" << "cmove" << "cmovne";
    
    for (const QString& instr : instructions) {
        HighlightingRule rule;
        rule.pattern = QRegularExpression(QString("\\b%1\\b").arg(instr),
                                          QRegularExpression::CaseInsensitiveOption);
        rule.format = keywordFormat;
        highlightingRules.push_back(rule);
    }
    
    // x64 registers
    QStringList registers;
    registers << "rax" << "rbx" << "rcx" << "rdx" << "rsi" << "rdi" << "rsp" << "rbp"
              << "r8" << "r9" << "r10" << "r11" << "r12" << "r13" << "r14" << "r15"
              << "eax" << "ebx" << "ecx" << "edx" << "esi" << "edi" << "esp" << "ebp"
              << "r8d" << "r9d" << "r10d" << "r11d" << "r12d" << "r13d" << "r14d" << "r15d"
              << "ax" << "bx" << "cx" << "dx" << "si" << "di" << "sp" << "bp"
              << "r8w" << "r9w" << "r10w" << "r11w" << "r12w" << "r13w" << "r14w" << "r15w"
              << "al" << "bl" << "cl" << "dl" << "sil" << "dil" << "spl" << "bpl"
              << "r8b" << "r9b" << "r10b" << "r11b" << "r12b" << "r13b" << "r14b" << "r15b"
              << "ah" << "bh" << "ch" << "dh"
              << "cs" << "ds" << "es" << "fs" << "gs" << "ss"
              << "rip" << "eip" << "ip" << "rflags" << "eflags" << "flags";
    
    for (const QString& reg : registers) {
        HighlightingRule rule;
        rule.pattern = QRegularExpression(QString("\\b%1\\b").arg(reg),
                                          QRegularExpression::CaseInsensitiveOption);
        rule.format = registerFormat;
        highlightingRules.push_back(rule);
    }
    
    // Assembler directives
    QStringList directives;
    directives << "\\.data" << "\\.code" << "\\.text" << "\\.bss" << "\\.section"
               << "db" << "dw" << "dd" << "dq" << "dt"
               << "resb" << "resw" << "resd" << "resq" << "rest"
               << "equ" << "times" << "incbin"
               << "proc" << "endp" << "public" << "extern" << "extrn"
               << "segment" << "ends" << "assume" << "end"
               << "byte" << "word" << "dword" << "qword" << "ptr"
               << "offset" << "sizeof" << "lengthof";
    
    for (const QString& dir : directives) {
        HighlightingRule rule;
        rule.pattern = QRegularExpression(QString("\\b%1\\b").arg(QRegularExpression::escape(dir)),
                                          QRegularExpression::CaseInsensitiveOption);
        rule.format = directiveFormat;
        highlightingRules.push_back(rule);
    }
    
    // Numbers (hex, decimal, binary)
    HighlightingRule numberRule;
    numberRule.pattern = QRegularExpression("\\b(0x[0-9a-fA-F]+|[0-9]+h|[0-9]+|[01]+b)\\b");
    numberRule.format = numberFormat;
    highlightingRules.push_back(numberRule);
    
    // Strings
    HighlightingRule stringRule;
    stringRule.pattern = QRegularExpression("\"[^\"]*\"|'[^']*'");
    stringRule.format = stringFormat;
    highlightingRules.push_back(stringRule);
}

void AssemblyHighlighter::highlightBlock(const QString& text) {
    // Apply all highlighting rules
    for (const HighlightingRule& rule : highlightingRules) {
        QRegularExpressionMatchIterator it = rule.pattern.globalMatch(text);
        while (it.hasNext()) {
            QRegularExpressionMatch match = it.next();
            setFormat(match.capturedStart(), match.capturedLength(), rule.format);
        }
    }
    
    // Comments (override everything else)
    QRegularExpression commentExpr(";.*$");
    QRegularExpressionMatchIterator commentIt = commentExpr.globalMatch(text);
    while (commentIt.hasNext()) {
        QRegularExpressionMatch match = commentIt.next();
        setFormat(match.capturedStart(), match.capturedLength(), commentFormat);
    }
    
    // Labels
    QRegularExpression labelExpr("^\\s*([a-zA-Z_][a-zA-Z0-9_]*)\\s*:");
    QRegularExpressionMatchIterator labelIt = labelExpr.globalMatch(text);
    while (labelIt.hasNext()) {
        QRegularExpressionMatch match = labelIt.next();
        setFormat(match.capturedStart(1), match.capturedLength(1), labelFormat);
    }
    
    // Local labels (.label)
    QRegularExpression localLabelExpr("\\.[a-zA-Z_][a-zA-Z0-9_]*");
    QRegularExpressionMatchIterator localIt = localLabelExpr.globalMatch(text);
    while (localIt.hasNext()) {
        QRegularExpressionMatch match = localIt.next();
        setFormat(match.capturedStart(), match.capturedLength(), labelFormat);
    }
}

// ============================================================
// EditorTabBar Implementation
// ============================================================

EditorTabBar::EditorTabBar(QWidget* parent) : QTabBar(parent) {
    setMovable(true);
    setTabsClosable(true);
    setElideMode(Qt::ElideRight);
    setDocumentMode(true);
}

void EditorTabBar::contextMenuEvent(QContextMenuEvent* event) {
    int index = tabAt(event->pos());
    if (index < 0) return;
    
    QMenu menu(this);
    
    QAction* closeAction = menu.addAction("Close Tab");
    connect(closeAction, &QAction::triggered, this, [this, index]() {
        emit tabCloseRequested(index);
    });
    
    QAction* closeOthersAction = menu.addAction("Close Other Tabs");
    connect(closeOthersAction, &QAction::triggered, this, [this, index]() {
        emit closeOthersRequested(index);
    });
    
    QAction* closeAllAction = menu.addAction("Close All Tabs");
    connect(closeAllAction, &QAction::triggered, this, [this]() {
        emit closeAllRequested();
    });
    
    menu.addSeparator();
    
    QAction* renameAction = menu.addAction("Rename Tab");
    connect(renameAction, &QAction::triggered, this, [this, index]() {
        emit tabRenameRequested(index);
    });
    
    menu.exec(event->globalPos());
}

void EditorTabBar::mouseDoubleClickEvent(QMouseEvent* event) {
    int index = tabAt(event->pos());
    if (index >= 0) {
        emit tabRenameRequested(index);
    } else {
        QTabBar::mouseDoubleClickEvent(event);
    }
}

// ============================================================
// MASMEditorWidget Implementation
// ============================================================

MASMEditorWidget::MASMEditorWidget(QWidget* parent)
    : QWidget(parent),
      nextTabNumber(1),
      caretVisible(true) {
    
    setupUI();
    setupToolbar();
    setupConnections();
    
    // Create initial tab
    newTab("Main.asm");
    
    // Setup caret blink timer
    caretTimer = new QTimer(this);
    connect(caretTimer, &QTimer::timeout, this, &MASMEditorWidget::onCaretBlink);
    caretTimer->start(500);
}

MASMEditorWidget::~MASMEditorWidget() {
    caretTimer->stop();
}

void MASMEditorWidget::setupUI() {
    mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);
    
    // Toolbar
    toolBar = new QToolBar("Editor");
    toolBar->setIconSize(QSize(16, 16));
    mainLayout->addWidget(toolBar);
    
    // Tab bar
    tabBar = new EditorTabBar();
    mainLayout->addWidget(tabBar);
    
    // Editor stack
    editorStack = new QStackedWidget();
    mainLayout->addWidget(editorStack, 1);
    
    // Status bar
    statusBar = new QStatusBar();
    statusBar->setSizeGripEnabled(false);
    mainLayout->addWidget(statusBar);
}

void MASMEditorWidget::setupToolbar() {
    // New tab
    QAction* newAction = toolBar->addAction("New");
    connect(newAction, &QAction::triggered, this, [this]() { newTab(); });
    
    // Open file
    QAction* openAction = toolBar->addAction("Open");
    connect(openAction, &QAction::triggered, this, [this]() {
        QString filePath = QFileDialog::getOpenFileName(this, "Open File",
            QString(), "Assembly Files (*.asm *.s *.inc);;All Files (*)");
        if (!filePath.isEmpty()) {
            int idx = newTab(QFileInfo(filePath).fileName());
            loadFile(filePath, idx);
        }
    });
    
    // Save
    QAction* saveAction = toolBar->addAction("Save");
    connect(saveAction, &QAction::triggered, this, [this]() {
        saveFile();
    });
    
    toolBar->addSeparator();
    
    // Undo/Redo
    QAction* undoAction = toolBar->addAction("Undo");
    connect(undoAction, &QAction::triggered, this, &MASMEditorWidget::undo);
    
    QAction* redoAction = toolBar->addAction("Redo");
    connect(redoAction, &QAction::triggered, this, &MASMEditorWidget::redo);
    
    toolBar->addSeparator();
    
    // Cut/Copy/Paste
    QAction* cutAction = toolBar->addAction("Cut");
    connect(cutAction, &QAction::triggered, this, &MASMEditorWidget::cut);
    
    QAction* copyAction = toolBar->addAction("Copy");
    connect(copyAction, &QAction::triggered, this, &MASMEditorWidget::copy);
    
    QAction* pasteAction = toolBar->addAction("Paste");
    connect(pasteAction, &QAction::triggered, this, &MASMEditorWidget::paste);
    
    toolBar->addSeparator();
    
    // Find
    QAction* findAction = toolBar->addAction("Find");
    connect(findAction, &QAction::triggered, this, [this]() {
        bool ok;
        QString text = QInputDialog::getText(this, "Find", "Search for:",
            QLineEdit::Normal, lastSearchText, &ok);
        if (ok && !text.isEmpty()) {
            find(text);
        }
    });

    toolBar->addSeparator();

    // Build (best-effort: requires toolchain in PATH)
    QAction* buildAction = toolBar->addAction("Build");
    connect(buildAction, &QAction::triggered, this, [this]() {
        buildCurrentFile();
    });

    // Run last build
    QAction* runAction = toolBar->addAction("Run");
    connect(runAction, &QAction::triggered, this, [this]() {
        runLastBuild();
    });
}

bool MASMEditorWidget::ensureCurrentFileSaved(QString& outAsmPath)
{
    const int idx = getCurrentTabIndex();
    if (idx < 0) return false;

    // If no path, force a Save As
    if (getFilePath(idx).isEmpty()) {
        if (!saveFile(QString(), idx)) return false;
    } else if (isModified(idx)) {
        // Save modifications to the same path
        if (!saveFile(getFilePath(idx), idx)) return false;
    }

    outAsmPath = getFilePath(idx);
    return !outAsmPath.isEmpty();
}

void MASMEditorWidget::buildCurrentFile()
{
    QString asmPath;
    if (!ensureCurrentFileSaved(asmPath)) {
        QMessageBox::warning(this, "Build", "Save the file before building.");
        return;
    }

    const QString ml64 = QStandardPaths::findExecutable("ml64.exe");
    const QString ml = QStandardPaths::findExecutable("ml.exe");
    const QString assembler = !ml64.isEmpty() ? ml64 : ml;

    if (assembler.isEmpty()) {
        QMessageBox::warning(this, "Build",
            "Could not find ml64.exe or ml.exe in PATH.\n\n"
            "Tip: launch RawrXD-AgenticIDE from a Visual Studio Developer Command Prompt/PowerShell, "
            "or add the MSVC tools to PATH.");
        return;
    }

    const QFileInfo asmInfo(asmPath);
    const QString outDir = asmInfo.absolutePath();
    const QString base = asmInfo.completeBaseName();
    m_lastBuiltObjPath = QDir(outDir).filePath(base + ".obj");
    m_lastBuiltExePath = QDir(outDir).filePath(base + ".exe");

    // Assemble
    QProcess proc;
    proc.setWorkingDirectory(outDir);

    QStringList args;
    // Both ml and ml64 accept /c and /Fo
    args << "/nologo" << "/c" << (QString("/Fo") + m_lastBuiltObjPath) << asmPath;
    proc.start(assembler, args);
    if (!proc.waitForFinished(-1)) {
        QMessageBox::warning(this, "Build", "Assembler did not finish.");
        return;
    }

    const QByteArray stdOut = proc.readAllStandardOutput();
    const QByteArray stdErr = proc.readAllStandardError();
    const int exitCode = proc.exitCode();
    if (exitCode != 0) {
        QMessageBox::critical(this, "Build failed",
            QString("Assembler exited with code %1.\n\n%2%3")
                .arg(exitCode)
                .arg(QString::fromLocal8Bit(stdOut))
                .arg(QString::fromLocal8Bit(stdErr)));
        statusBar->showMessage("Build failed", 4000);
        return;
    }

    statusBar->showMessage(QString("Assembled: %1").arg(QFileInfo(m_lastBuiltObjPath).fileName()), 4000);

    // Best-effort link (only works if link.exe + libs are available in environment)
    const QString linker = QStandardPaths::findExecutable("link.exe");
    if (linker.isEmpty()) {
        return;
    }

    QProcess linkProc;
    linkProc.setWorkingDirectory(outDir);
    QStringList linkArgs;
    linkArgs << "/NOLOGO" << (QString("/OUT:") + m_lastBuiltExePath)
             << "/SUBSYSTEM:CONSOLE" << "/ENTRY:main" << m_lastBuiltObjPath
             << "kernel32.lib" << "user32.lib";
    linkProc.start(linker, linkArgs);
    if (!linkProc.waitForFinished(-1)) {
        return;
    }

    const QByteArray lOut = linkProc.readAllStandardOutput();
    const QByteArray lErr = linkProc.readAllStandardError();
    const int lExit = linkProc.exitCode();
    if (lExit != 0) {
        // Don't hard-fail assemble; just inform
        QMessageBox::information(this, "Link skipped/failed",
            "Assemble succeeded, but linking failed (toolchain env may be missing).\n\n"
            "To enable linking/running, launch the IDE from a VS Developer prompt.\n\n" +
            QString::fromLocal8Bit(lOut) + QString::fromLocal8Bit(lErr));
        return;
    }

    if (QFileInfo::exists(m_lastBuiltExePath)) {
        statusBar->showMessage(QString("Built: %1").arg(QFileInfo(m_lastBuiltExePath).fileName()), 4000);
    }
}

void MASMEditorWidget::runLastBuild()
{
    if (m_lastBuiltExePath.isEmpty() || !QFileInfo::exists(m_lastBuiltExePath)) {
        QMessageBox::information(this, "Run",
            "No built executable found.\n\n"
            "Use Build first. If linking fails, launch the IDE from a VS Developer prompt so link.exe can find libraries.");
        return;
    }

    const bool ok = QProcess::startDetached(m_lastBuiltExePath, {}, QFileInfo(m_lastBuiltExePath).absolutePath());
    if (!ok) {
        QMessageBox::warning(this, "Run", "Failed to start the executable.");
        return;
    }
    statusBar->showMessage(QString("Running: %1").arg(QFileInfo(m_lastBuiltExePath).fileName()), 3000);
}

void MASMEditorWidget::setupConnections() {
    connect(tabBar, QOverload<int>::of(&QTabBar::currentChanged),
            this, &MASMEditorWidget::onTabChanged);
    connect(tabBar, &QTabBar::tabCloseRequested,
            this, &MASMEditorWidget::onTabCloseRequested);
    connect(tabBar, &EditorTabBar::tabRenameRequested,
            this, &MASMEditorWidget::onTabRenameRequested);
    connect(tabBar, &EditorTabBar::closeAllRequested,
            this, &MASMEditorWidget::closeAllTabs);
    connect(tabBar, &EditorTabBar::closeOthersRequested,
            this, &MASMEditorWidget::closeOtherTabs);
}

QTextEdit* MASMEditorWidget::createEditor() {
    QTextEdit* editor = new QTextEdit();
    
    // Set monospace font
    QFont font("Consolas", 11);
    font.setFixedPitch(true);
    editor->setFont(font);
    
    // Configure editor
    editor->setLineWrapMode(QTextEdit::NoWrap);
    editor->setTabStopDistance(QFontMetrics(font).horizontalAdvance(' ') * 4);
    editor->setAcceptRichText(false);
    
    // Dark theme colors
    QPalette p = editor->palette();
    p.setColor(QPalette::Base, QColor(30, 30, 30));
    p.setColor(QPalette::Text, QColor(212, 212, 212));
    editor->setPalette(p);
    
    // Connect signals
    connect(editor, &QTextEdit::textChanged, this, &MASMEditorWidget::onTextChanged);
    connect(editor, &QTextEdit::cursorPositionChanged, this, &MASMEditorWidget::onCursorMoved);
    
    return editor;
}

int MASMEditorWidget::newTab(const QString& name) {
    QString tabName = name.isEmpty() ?
        QString("Untitled%1.asm").arg(nextTabNumber++) : name;
    
    // Create editor
    QTextEdit* editor = createEditor();
    editors.push_back(editor);
    editorStack->addWidget(editor);
    
    // Create highlighter
    auto highlighter = std::make_unique<AssemblyHighlighter>(editor->document());
    highlighters.push_back(std::move(highlighter));
    
    // Create tab data
    TabData data;
    data.name = tabName;
    tabData.push_back(data);
    
    // Add tab
    int index = tabBar->addTab(tabName);
    tabBar->setCurrentIndex(index);
    
    emit tabCountChanged(getTabCount());
    return index;
}

void MASMEditorWidget::closeTab(int index) {
    if (index < 0 || index >= static_cast<int>(editors.size())) return;
    if (editors.size() == 1) {
        // Don't close last tab, just clear it
        editors[0]->clear();
        tabData[0] = TabData();
        tabData[0].name = "Untitled1.asm";
        tabBar->setTabText(0, tabData[0].name);
        return;
    }
    
    // Check for unsaved changes
    if (tabData[index].modified) {
        QMessageBox::StandardButton reply = QMessageBox::question(this,
            "Unsaved Changes",
            QString("Save changes to %1?").arg(tabData[index].name),
            QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);
        
        if (reply == QMessageBox::Cancel) return;
        if (reply == QMessageBox::Yes) {
            if (!saveFile(QString(), index)) return;
        }
    }
    
    // Remove editor
    editorStack->removeWidget(editors[index]);
    delete editors[index];
    editors.erase(editors.begin() + index);
    
    // Remove highlighter
    highlighters.erase(highlighters.begin() + index);
    
    // Remove data
    tabData.erase(tabData.begin() + index);
    
    // Remove tab
    tabBar->removeTab(index);
    
    emit tabCountChanged(getTabCount());
}

void MASMEditorWidget::closeAllTabs() {
    while (editors.size() > 1) {
        closeTab(editors.size() - 1);
    }
    closeTab(0);
}

void MASMEditorWidget::closeOtherTabs(int keepIndex) {
    for (int i = editors.size() - 1; i >= 0; --i) {
        if (i != keepIndex) {
            closeTab(i);
            if (keepIndex > i) --keepIndex;
        }
    }
}

bool MASMEditorWidget::switchTab(int index) {
    if (index < 0 || index >= static_cast<int>(editors.size())) return false;
    
    tabBar->setCurrentIndex(index);
    editorStack->setCurrentIndex(index);
    editors[index]->setFocus();
    
    updateStatusBar();
    emit tabChanged(index);
    return true;
}

int MASMEditorWidget::getTabCount() const {
    return static_cast<int>(editors.size());
}

int MASMEditorWidget::getCurrentTabIndex() const {
    return tabBar->currentIndex();
}

QString MASMEditorWidget::getTabName(int index) const {
    int idx = resolveIndex(index);
    if (idx < 0 || idx >= static_cast<int>(tabData.size())) return QString();
    return tabData[idx].name;
}

void MASMEditorWidget::setTabName(int index, const QString& name) {
    int idx = resolveIndex(index);
    if (idx < 0 || idx >= static_cast<int>(tabData.size())) return;
    
    tabData[idx].name = name;
    updateTabTitle(idx);
}

QString MASMEditorWidget::getContent(int index) const {
    int idx = resolveIndex(index);
    if (idx < 0 || idx >= static_cast<int>(editors.size())) return QString();
    return editors[idx]->toPlainText();
}

void MASMEditorWidget::setContent(const QString& content, int index) {
    int idx = resolveIndex(index);
    if (idx < 0 || idx >= static_cast<int>(editors.size())) return;
    
    editors[idx]->setPlainText(content);
    tabData[idx].modified = false;
    updateTabTitle(idx);
}

bool MASMEditorWidget::isModified(int index) const {
    int idx = resolveIndex(index);
    if (idx < 0 || idx >= static_cast<int>(tabData.size())) return false;
    return tabData[idx].modified;
}

void MASMEditorWidget::setModified(bool modified, int index) {
    int idx = resolveIndex(index);
    if (idx < 0 || idx >= static_cast<int>(tabData.size())) return;
    
    tabData[idx].modified = modified;
    updateTabTitle(idx);
}

bool MASMEditorWidget::loadFile(const QString& filePath, int index) {
    int idx = resolveIndex(index);
    if (idx < 0 || idx >= static_cast<int>(editors.size())) return false;
    
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::critical(this, "Error",
            QString("Could not open file: %1").arg(file.errorString()));
        return false;
    }
    
    QTextStream stream(&file);
    editors[idx]->setPlainText(stream.readAll());
    file.close();
    
    tabData[idx].filePath = filePath;
    tabData[idx].name = QFileInfo(filePath).fileName();
    tabData[idx].modified = false;
    updateTabTitle(idx);
    
    return true;
}

bool MASMEditorWidget::saveFile(const QString& filePath, int index) {
    int idx = resolveIndex(index);
    if (idx < 0 || idx >= static_cast<int>(editors.size())) return false;
    
    QString path = filePath;
    if (path.isEmpty()) {
        path = tabData[idx].filePath;
    }
    if (path.isEmpty()) {
        path = QFileDialog::getSaveFileName(this, "Save File",
            tabData[idx].name, "Assembly Files (*.asm *.s *.inc);;All Files (*)");
        if (path.isEmpty()) return false;
    }
    
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::critical(this, "Error",
            QString("Could not save file: %1").arg(file.errorString()));
        return false;
    }
    
    QTextStream stream(&file);
    stream << editors[idx]->toPlainText();
    file.close();
    
    tabData[idx].filePath = path;
    tabData[idx].name = QFileInfo(path).fileName();
    tabData[idx].modified = false;
    updateTabTitle(idx);
    
    return true;
}

QString MASMEditorWidget::getFilePath(int index) const {
    int idx = resolveIndex(index);
    if (idx < 0 || idx >= static_cast<int>(tabData.size())) return QString();
    return tabData[idx].filePath;
}

void MASMEditorWidget::undo() {
    int idx = getCurrentTabIndex();
    if (idx >= 0 && idx < static_cast<int>(editors.size())) {
        editors[idx]->undo();
    }
}

void MASMEditorWidget::redo() {
    int idx = getCurrentTabIndex();
    if (idx >= 0 && idx < static_cast<int>(editors.size())) {
        editors[idx]->redo();
    }
}

void MASMEditorWidget::cut() {
    int idx = getCurrentTabIndex();
    if (idx >= 0 && idx < static_cast<int>(editors.size())) {
        editors[idx]->cut();
    }
}

void MASMEditorWidget::copy() {
    int idx = getCurrentTabIndex();
    if (idx >= 0 && idx < static_cast<int>(editors.size())) {
        editors[idx]->copy();
    }
}

void MASMEditorWidget::paste() {
    int idx = getCurrentTabIndex();
    if (idx >= 0 && idx < static_cast<int>(editors.size())) {
        editors[idx]->paste();
    }
}

void MASMEditorWidget::selectAll() {
    int idx = getCurrentTabIndex();
    if (idx >= 0 && idx < static_cast<int>(editors.size())) {
        editors[idx]->selectAll();
    }
}

void MASMEditorWidget::find(const QString& text) {
    int idx = getCurrentTabIndex();
    if (idx < 0 || idx >= static_cast<int>(editors.size())) return;
    
    lastSearchText = text;
    if (!editors[idx]->find(text)) {
        // Wrap around
        QTextCursor cursor = editors[idx]->textCursor();
        cursor.movePosition(QTextCursor::Start);
        editors[idx]->setTextCursor(cursor);
        editors[idx]->find(text);
    }
}

void MASMEditorWidget::findNext() {
    if (!lastSearchText.isEmpty()) {
        find(lastSearchText);
    }
}

void MASMEditorWidget::replace(const QString& findText, const QString& replaceWith) {
    int idx = getCurrentTabIndex();
    if (idx < 0 || idx >= static_cast<int>(editors.size())) return;
    
    QTextCursor cursor = editors[idx]->textCursor();
    if (cursor.hasSelection() && cursor.selectedText() == findText) {
        cursor.insertText(replaceWith);
    }
    find(findText);
}

void MASMEditorWidget::goToLine(int line) {
    int idx = getCurrentTabIndex();
    if (idx < 0 || idx >= static_cast<int>(editors.size())) return;
    
    QTextCursor cursor(editors[idx]->document()->findBlockByLineNumber(line - 1));
    editors[idx]->setTextCursor(cursor);
    editors[idx]->ensureCursorVisible();
}

int MASMEditorWidget::getLineCount(int index) const {
    int idx = resolveIndex(index);
    if (idx < 0 || idx >= static_cast<int>(editors.size())) return 0;
    return editors[idx]->document()->blockCount();
}

int MASMEditorWidget::getCharCount(int index) const {
    int idx = resolveIndex(index);
    if (idx < 0 || idx >= static_cast<int>(editors.size())) return 0;
    return editors[idx]->document()->characterCount();
}

QPair<int, int> MASMEditorWidget::getCursorPosition(int index) const {
    int idx = resolveIndex(index);
    if (idx < 0 || idx >= static_cast<int>(editors.size())) return {0, 0};
    
    QTextCursor cursor = editors[idx]->textCursor();
    return {cursor.blockNumber() + 1, cursor.positionInBlock() + 1};
}

void MASMEditorWidget::onTabChanged(int index) {
    if (index >= 0 && index < static_cast<int>(editors.size())) {
        editorStack->setCurrentIndex(index);
        editors[index]->setFocus();
        updateStatusBar();
        emit tabChanged(index);
    }
}

void MASMEditorWidget::onTextChanged() {
    int idx = getCurrentTabIndex();
    if (idx >= 0 && idx < static_cast<int>(tabData.size())) {
        if (!tabData[idx].modified) {
            tabData[idx].modified = true;
            updateTabTitle(idx);
        }
        emit contentModified(idx);
    }
}

void MASMEditorWidget::onCursorMoved() {
    updateStatusBar();
    auto pos = getCursorPosition();
    emit cursorPositionChanged(pos.first, pos.second);
}

void MASMEditorWidget::onTabCloseRequested(int index) {
    closeTab(index);
}

void MASMEditorWidget::onTabRenameRequested(int index) {
    if (index < 0 || index >= static_cast<int>(tabData.size())) return;
    
    bool ok;
    QString newName = QInputDialog::getText(this, "Rename Tab",
        "New name:", QLineEdit::Normal, tabData[index].name, &ok);
    
    if (ok && !newName.isEmpty()) {
        setTabName(index, newName);
    }
}

void MASMEditorWidget::updateStatusBar() {
    auto pos = getCursorPosition();
    int lines = getLineCount();
    int chars = getCharCount();
    
    statusBar->showMessage(QString("Line %1, Column %2 | %3 lines | %4 characters")
        .arg(pos.first).arg(pos.second).arg(lines).arg(chars));
}

void MASMEditorWidget::onCaretBlink() {
    caretVisible = !caretVisible;
    int idx = getCurrentTabIndex();
    if (idx >= 0 && idx < static_cast<int>(editors.size())) {
        editors[idx]->viewport()->update();
    }
}

void MASMEditorWidget::updateTabTitle(int index) {
    if (index < 0 || index >= static_cast<int>(tabData.size())) return;
    
    QString title = tabData[index].name;
    if (tabData[index].modified) {
        title += " *";
    }
    tabBar->setTabText(index, title);
}

int MASMEditorWidget::resolveIndex(int index) const {
    if (index < 0) {
        return getCurrentTabIndex();
    }
    return index;
}
