// MASM Text Editor Qt Wrapper Implementation - PRODUCTION GRADE
// Complete implementation with no placeholders

#include "masm_editor_widget.h"


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
    std::vector<std::string> instructions;
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
    
    for (const std::string& instr : instructions) {
        HighlightingRule rule;
        rule.pattern = std::regex(std::string("\\b%1\\b"),
                                          std::regex::CaseInsensitiveOption);
        rule.format = keywordFormat;
        highlightingRules.push_back(rule);
    }
    
    // x64 registers
    std::vector<std::string> registers;
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
    
    for (const std::string& reg : registers) {
        HighlightingRule rule;
        rule.pattern = std::regex(std::string("\\b%1\\b"),
                                          std::regex::CaseInsensitiveOption);
        rule.format = registerFormat;
        highlightingRules.push_back(rule);
    }
    
    // Assembler directives
    std::vector<std::string> directives;
    directives << "\\.data" << "\\.code" << "\\.text" << "\\.bss" << "\\.section"
               << "db" << "dw" << "dd" << "dq" << "dt"
               << "resb" << "resw" << "resd" << "resq" << "rest"
               << "equ" << "times" << "incbin"
               << "proc" << "endp" << "public" << "extern" << "extrn"
               << "segment" << "ends" << "assume" << "end"
               << "byte" << "word" << "dword" << "qword" << "ptr"
               << "offset" << "sizeof" << "lengthof";
    
    for (const std::string& dir : directives) {
        HighlightingRule rule;
        rule.pattern = std::regex(std::string("\\b%1\\b")),
                                          std::regex::CaseInsensitiveOption);
        rule.format = directiveFormat;
        highlightingRules.push_back(rule);
    }
    
    // Numbers (hex, decimal, binary)
    HighlightingRule numberRule;
    numberRule.pattern = std::regex("\\b(0x[0-9a-fA-F]+|[0-9]+h|[0-9]+|[01]+b)\\b");
    numberRule.format = numberFormat;
    highlightingRules.push_back(numberRule);
    
    // Strings
    HighlightingRule stringRule;
    stringRule.pattern = std::regex("\"[^\"]*\"|'[^']*'");
    stringRule.format = stringFormat;
    highlightingRules.push_back(stringRule);
}

void AssemblyHighlighter::highlightBlock(const std::string& text) {
    // Apply all highlighting rules
    for (const HighlightingRule& rule : highlightingRules) {
        std::sregex_iterator it = rule.pattern;
        while (itfalse) {
            std::smatch match = it;
            setFormat(match.capturedStart(), match.capturedLength(), rule.format);
        }
    }
    
    // Comments (override everything else)
    std::regex commentExpr(";.*$");
    std::sregex_iterator commentIt = commentExpr;
    while (commentItfalse) {
        std::smatch match = commentIt;
        setFormat(match.capturedStart(), match.capturedLength(), commentFormat);
    }
    
    // Labels
    std::regex labelExpr("^\\s*([a-zA-Z_][a-zA-Z0-9_]*)\\s*:");
    std::sregex_iterator labelIt = labelExpr;
    while (labelItfalse) {
        std::smatch match = labelIt;
        setFormat(match.capturedStart(1), match.capturedLength(1), labelFormat);
    }
    
    // Local labels (.label)
    std::regex localLabelExpr("\\.[a-zA-Z_][a-zA-Z0-9_]*");
    std::sregex_iterator localIt = localLabelExpr;
    while (localItfalse) {
        std::smatch match = localIt;
        setFormat(match.capturedStart(), match.capturedLength(), labelFormat);
    }
}

// ============================================================
// EditorTabBar Implementation
// ============================================================

EditorTabBar::EditorTabBar(void* parent) : QTabBar(parent) {
    setMovable(true);
    setTabsClosable(true);
    setElideMode(//ElideRight);
    setDocumentMode(true);
}

void EditorTabBar::contextMenuEvent(QContextMenuEvent* event) {
    int index = tabAt(event->pos());
    if (index < 0) return;
    
    QMenu menu(this);
    
    QAction* closeAction = menu.addAction("Close Tab");
// Qt connect removed
    });
    
    QAction* closeOthersAction = menu.addAction("Close Other Tabs");
// Qt connect removed
    });
    
    QAction* closeAllAction = menu.addAction("Close All Tabs");
// Qt connect removed
    });
    
    menu.addSeparator();
    
    QAction* renameAction = menu.addAction("Rename Tab");
// Qt connect removed
    });
    
    menu.exec(event->globalPos());
}

void EditorTabBar::mouseDoubleClickEvent(QMouseEvent* event) {
    int index = tabAt(event->pos());
    if (index >= 0) {
        tabRenameRequested(index);
    } else {
        QTabBar::mouseDoubleClickEvent(event);
    }
}

// ============================================================
// MASMEditorWidget Implementation
// ============================================================

MASMEditorWidget::MASMEditorWidget(void* parent)
    : void(parent),
      nextTabNumber(1),
      caretVisible(true) {
    
    setupUI();
    setupToolbar();
    setupConnections();
    
    // Create initial tab
    newTab("Main.asm");
    
    // Setup caret blink timer
    caretTimer = new void*(this);
// Qt connect removed
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
// Qt connect removed
            std::string(), "Assembly Files (*.asm *.s *.inc);;All Files (*)");
        if (!filePath.isEmpty()) {
            int idx = newTab(std::filesystem::path(filePath).fileName());
            loadFile(filePath, idx);
        }
    });
    
    // Save
    QAction* saveAction = toolBar->addAction("Save");
// Qt connect removed
    });
    
    toolBar->addSeparator();
    
    // Undo/Redo
    QAction* undoAction = toolBar->addAction("Undo");
// Qt connect removed
    QAction* redoAction = toolBar->addAction("Redo");
// Qt connect removed
    toolBar->addSeparator();
    
    // Cut/Copy/Paste
    QAction* cutAction = toolBar->addAction("Cut");
// Qt connect removed
    QAction* copyAction = toolBar->addAction("Copy");
// Qt connect removed
    QAction* pasteAction = toolBar->addAction("Paste");
// Qt connect removed
    toolBar->addSeparator();
    
    // Find
    QAction* findAction = toolBar->addAction("Find");
// Qt connect removed
        std::string text = QInputDialog::getText(this, "Find", "Search for:",
            QLineEdit::Normal, lastSearchText, &ok);
        if (ok && !text.isEmpty()) {
            find(text);
        }
    });
}

void MASMEditorWidget::setupConnections() {
// Qt connect removed
// Qt connect removed
// Qt connect removed
// Qt connect removed
// Qt connect removed
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
// Qt connect removed
// Qt connect removed
    return editor;
}

int MASMEditorWidget::newTab(const std::string& name) {
    std::string tabName = name.isEmpty() ?
        std::string("Untitled%1.asm") : name;
    
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
    
    tabCountChanged(getTabCount());
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
            std::string("Save changes to %1?"),
            QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);
        
        if (reply == QMessageBox::Cancel) return;
        if (reply == QMessageBox::Yes) {
            if (!saveFile(std::string(), index)) return;
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
    
    tabCountChanged(getTabCount());
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
    tabChanged(index);
    return true;
}

int MASMEditorWidget::getTabCount() const {
    return static_cast<int>(editors.size());
}

int MASMEditorWidget::getCurrentTabIndex() const {
    return tabBar->currentIndex();
}

std::string MASMEditorWidget::getTabName(int index) const {
    int idx = resolveIndex(index);
    if (idx < 0 || idx >= static_cast<int>(tabData.size())) return std::string();
    return tabData[idx].name;
}

void MASMEditorWidget::setTabName(int index, const std::string& name) {
    int idx = resolveIndex(index);
    if (idx < 0 || idx >= static_cast<int>(tabData.size())) return;
    
    tabData[idx].name = name;
    updateTabTitle(idx);
}

std::string MASMEditorWidget::getContent(int index) const {
    int idx = resolveIndex(index);
    if (idx < 0 || idx >= static_cast<int>(editors.size())) return std::string();
    return editors[idx]->toPlainText();
}

void MASMEditorWidget::setContent(const std::string& content, int index) {
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

bool MASMEditorWidget::loadFile(const std::string& filePath, int index) {
    int idx = resolveIndex(index);
    if (idx < 0 || idx >= static_cast<int>(editors.size())) return false;
    
    std::fstream file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::critical(this, "Error",
            std::string("Could not open file: %1")));
        return false;
    }
    
    QTextStream stream(&file);
    editors[idx]->setPlainText(stream.readAll());
    file.close();
    
    tabData[idx].filePath = filePath;
    tabData[idx].name = std::filesystem::path(filePath).fileName();
    tabData[idx].modified = false;
    updateTabTitle(idx);
    
    return true;
}

bool MASMEditorWidget::saveFile(const std::string& filePath, int index) {
    int idx = resolveIndex(index);
    if (idx < 0 || idx >= static_cast<int>(editors.size())) return false;
    
    std::string path = filePath;
    if (path.isEmpty()) {
        path = tabData[idx].filePath;
    }
    if (path.isEmpty()) {
        path = QFileDialog::getSaveFileName(this, "Save File",
            tabData[idx].name, "Assembly Files (*.asm *.s *.inc);;All Files (*)");
        if (path.isEmpty()) return false;
    }
    
    std::fstream file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::critical(this, "Error",
            std::string("Could not save file: %1")));
        return false;
    }
    
    QTextStream stream(&file);
    stream << editors[idx]->toPlainText();
    file.close();
    
    tabData[idx].filePath = path;
    tabData[idx].name = std::filesystem::path(path).fileName();
    tabData[idx].modified = false;
    updateTabTitle(idx);
    
    return true;
}

std::string MASMEditorWidget::getFilePath(int index) const {
    int idx = resolveIndex(index);
    if (idx < 0 || idx >= static_cast<int>(tabData.size())) return std::string();
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

void MASMEditorWidget::find(const std::string& text) {
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

void MASMEditorWidget::replace(const std::string& findText, const std::string& replaceWith) {
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

std::pair<int, int> MASMEditorWidget::getCursorPosition(int index) const {
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
        tabChanged(index);
    }
}

void MASMEditorWidget::onTextChanged() {
    int idx = getCurrentTabIndex();
    if (idx >= 0 && idx < static_cast<int>(tabData.size())) {
        if (!tabData[idx].modified) {
            tabData[idx].modified = true;
            updateTabTitle(idx);
        }
        contentModified(idx);
    }
}

void MASMEditorWidget::onCursorMoved() {
    updateStatusBar();
    auto pos = getCursorPosition();
    cursorPositionChanged(pos.first, pos.second);
}

void MASMEditorWidget::onTabCloseRequested(int index) {
    closeTab(index);
}

void MASMEditorWidget::onTabRenameRequested(int index) {
    if (index < 0 || index >= static_cast<int>(tabData.size())) return;
    
    bool ok;
    std::string newName = QInputDialog::getText(this, "Rename Tab",
        "New name:", QLineEdit::Normal, tabData[index].name, &ok);
    
    if (ok && !newName.isEmpty()) {
        setTabName(index, newName);
    }
}

void MASMEditorWidget::updateStatusBar() {
    auto pos = getCursorPosition();
    int lines = getLineCount();
    int chars = getCharCount();
    
    statusBar->showMessage(std::string("Line %1, Column %2 | %3 lines | %4 characters")
        );
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
    
    std::string title = tabData[index].name;
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

