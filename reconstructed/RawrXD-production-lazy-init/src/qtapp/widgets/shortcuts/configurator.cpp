/**
 * @file shortcuts_configurator.cpp
 * @brief Implementation of keyboard shortcuts configurator
 */

#include "shortcuts_configurator.h"
#include <QHeaderView>
#include <QFileDialog>
#include <QMessageBox>
#include <QJsonDocument>
#include <QFile>
#include <QKeyEvent>
#include <QApplication>
#include <QStyle>

// ============================================================================
// ShortcutInfo Implementation
// ============================================================================

bool ShortcutInfo::hasConflict() const {
    return !conflictWith.isEmpty();
}

// ============================================================================
// KeySequenceRecorder Implementation
// ============================================================================

KeySequenceRecorder::KeySequenceRecorder(QWidget* parent)
    : QWidget(parent)
    , m_recordTimer(new QTimer(this))
{
    QHBoxLayout* layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    
    m_displayLabel = new QLabel(tr("Click to record shortcut..."));
    m_displayLabel->setStyleSheet(
        "QLabel {"
        "  background-color: #2d2d2d;"
        "  border: 1px solid #555;"
        "  border-radius: 3px;"
        "  padding: 5px 10px;"
        "  min-width: 200px;"
        "  font-family: monospace;"
        "}"
    );
    m_displayLabel->setAlignment(Qt::AlignCenter);
    
    m_clearButton = new QPushButton(tr("Clear"));
    m_clearButton->setFixedWidth(60);
    
    layout->addWidget(m_displayLabel, 1);
    layout->addWidget(m_clearButton);
    
    setFocusPolicy(Qt::StrongFocus);
    
    m_recordTimer->setSingleShot(true);
    m_recordTimer->setInterval(2000);  // 2 seconds timeout
    
    connect(m_clearButton, &QPushButton::clicked, this, &KeySequenceRecorder::clear);
    connect(m_recordTimer, &QTimer::timeout, this, &KeySequenceRecorder::onRecordTimeout);
}

void KeySequenceRecorder::setKeySequence(const QKeySequence& sequence) {
    m_sequence = sequence;
    updateDisplay();
}

void KeySequenceRecorder::clear() {
    m_sequence = QKeySequence();
    m_currentKeys.clear();
    stopRecording();
    updateDisplay();
    emit keySequenceChanged(m_sequence);
}

void KeySequenceRecorder::keyPressEvent(QKeyEvent* event) {
    if (!m_recording) {
        startRecording();
    }
    
    int key = event->key();
    
    // Ignore standalone modifier keys
    if (key == Qt::Key_Control || key == Qt::Key_Shift ||
        key == Qt::Key_Alt || key == Qt::Key_Meta) {
        return;
    }
    
    // Build the key combination
    int keyWithModifiers = key;
    if (event->modifiers() & Qt::ControlModifier) {
        keyWithModifiers |= Qt::CTRL;
    }
    if (event->modifiers() & Qt::ShiftModifier) {
        keyWithModifiers |= Qt::SHIFT;
    }
    if (event->modifiers() & Qt::AltModifier) {
        keyWithModifiers |= Qt::ALT;
    }
    if (event->modifiers() & Qt::MetaModifier) {
        keyWithModifiers |= Qt::META;
    }
    
    m_currentKeys.append(keyWithModifiers);
    
    // Support up to 4 chord sequences
    if (m_currentKeys.size() >= 4) {
        stopRecording();
    } else {
        m_recordTimer->start();  // Reset timeout
    }
    
    // Build QKeySequence from recorded keys
    switch (m_currentKeys.size()) {
        case 1:
            m_sequence = QKeySequence(m_currentKeys[0]);
            break;
        case 2:
            m_sequence = QKeySequence(m_currentKeys[0], m_currentKeys[1]);
            break;
        case 3:
            m_sequence = QKeySequence(m_currentKeys[0], m_currentKeys[1], m_currentKeys[2]);
            break;
        case 4:
            m_sequence = QKeySequence(m_currentKeys[0], m_currentKeys[1], 
                                      m_currentKeys[2], m_currentKeys[3]);
            break;
    }
    
    updateDisplay();
    emit keySequenceChanged(m_sequence);
    
    event->accept();
}

void KeySequenceRecorder::keyReleaseEvent(QKeyEvent* event) {
    event->accept();
}

void KeySequenceRecorder::focusInEvent(QFocusEvent* event) {
    m_displayLabel->setStyleSheet(
        "QLabel {"
        "  background-color: #3d3d3d;"
        "  border: 2px solid #0078d4;"
        "  border-radius: 3px;"
        "  padding: 5px 10px;"
        "  min-width: 200px;"
        "  font-family: monospace;"
        "}"
    );
    QWidget::focusInEvent(event);
}

void KeySequenceRecorder::focusOutEvent(QFocusEvent* event) {
    stopRecording();
    m_displayLabel->setStyleSheet(
        "QLabel {"
        "  background-color: #2d2d2d;"
        "  border: 1px solid #555;"
        "  border-radius: 3px;"
        "  padding: 5px 10px;"
        "  min-width: 200px;"
        "  font-family: monospace;"
        "}"
    );
    QWidget::focusOutEvent(event);
}

void KeySequenceRecorder::mousePressEvent(QMouseEvent* event) {
    setFocus();
    startRecording();
    QWidget::mousePressEvent(event);
}

void KeySequenceRecorder::onRecordTimeout() {
    stopRecording();
}

void KeySequenceRecorder::startRecording() {
    if (!m_recording) {
        m_recording = true;
        m_currentKeys.clear();
        m_displayLabel->setText(tr("Press keys..."));
        m_recordTimer->start();
        emit recordingStarted();
    }
}

void KeySequenceRecorder::stopRecording() {
    if (m_recording) {
        m_recording = false;
        m_recordTimer->stop();
        updateDisplay();
        emit recordingStopped();
    }
}

void KeySequenceRecorder::updateDisplay() {
    if (m_sequence.isEmpty()) {
        m_displayLabel->setText(tr("Click to record shortcut..."));
    } else {
        m_displayLabel->setText(m_sequence.toString(QKeySequence::NativeText));
    }
}

// ============================================================================
// ShortcutsConfigurator Implementation
// ============================================================================

ShortcutsConfigurator::ShortcutsConfigurator(QWidget* parent)
    : QWidget(parent)
{
    setupUI();
    setupConnections();
    populateDefaultShortcuts();
    populateTree();
}

ShortcutsConfigurator::~ShortcutsConfigurator() = default;

void ShortcutsConfigurator::setupUI() {
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(10);
    
    // Top toolbar
    QHBoxLayout* toolbarLayout = new QHBoxLayout();
    
    m_searchEdit = new QLineEdit();
    m_searchEdit->setPlaceholderText(tr("Search shortcuts..."));
    m_searchEdit->setClearButtonEnabled(true);
    
    m_categoryCombo = new QComboBox();
    m_categoryCombo->addItem(tr("All Categories"), "");
    m_categoryCombo->setMinimumWidth(150);
    
    m_showConflictsBtn = new QPushButton(tr("Conflicts"));
    m_showConflictsBtn->setCheckable(true);
    m_showConflictsBtn->setIcon(style()->standardIcon(QStyle::SP_MessageBoxWarning));
    
    m_showCustomizedBtn = new QPushButton(tr("Customized"));
    m_showCustomizedBtn->setCheckable(true);
    m_showCustomizedBtn->setIcon(style()->standardIcon(QStyle::SP_FileDialogDetailedView));
    
    toolbarLayout->addWidget(m_searchEdit, 1);
    toolbarLayout->addWidget(m_categoryCombo);
    toolbarLayout->addWidget(m_showConflictsBtn);
    toolbarLayout->addWidget(m_showCustomizedBtn);
    
    mainLayout->addLayout(toolbarLayout);
    
    // Main content splitter
    QSplitter* splitter = new QSplitter(Qt::Horizontal);
    
    // Shortcuts tree
    m_shortcutsTree = new QTreeWidget();
    m_shortcutsTree->setHeaderLabels({tr("Command"), tr("Shortcut"), tr("When"), tr("Source")});
    m_shortcutsTree->setAlternatingRowColors(true);
    m_shortcutsTree->setRootIsDecorated(true);
    m_shortcutsTree->setSortingEnabled(true);
    m_shortcutsTree->setSelectionMode(QAbstractItemView::SingleSelection);
    m_shortcutsTree->header()->setStretchLastSection(false);
    m_shortcutsTree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_shortcutsTree->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_shortcutsTree->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_shortcutsTree->header()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    
    splitter->addWidget(m_shortcutsTree);
    
    // Edit panel
    QWidget* editPanel = new QWidget();
    QVBoxLayout* editLayout = new QVBoxLayout(editPanel);
    
    m_editGroup = new QGroupBox(tr("Edit Shortcut"));
    QVBoxLayout* editGroupLayout = new QVBoxLayout(m_editGroup);
    
    m_shortcutNameLabel = new QLabel();
    m_shortcutNameLabel->setStyleSheet("font-weight: bold; font-size: 14px;");
    
    m_shortcutDescLabel = new QLabel();
    m_shortcutDescLabel->setWordWrap(true);
    m_shortcutDescLabel->setStyleSheet("color: #888;");
    
    QLabel* keyLabel = new QLabel(tr("Keybinding:"));
    m_keySequenceRecorder = new KeySequenceRecorder();
    
    m_conflictWarningLabel = new QLabel();
    m_conflictWarningLabel->setStyleSheet("color: #ff6b6b; font-weight: bold;");
    m_conflictWarningLabel->setWordWrap(true);
    m_conflictWarningLabel->hide();
    
    QLabel* conflictLabel = new QLabel(tr("Conflicts:"));
    m_conflictList = new QListWidget();
    m_conflictList->setMaximumHeight(100);
    m_conflictList->hide();
    
    QHBoxLayout* editButtonsLayout = new QHBoxLayout();
    m_resetSelectedBtn = new QPushButton(tr("Reset to Default"));
    m_applyBtn = new QPushButton(tr("Apply"));
    m_applyBtn->setDefault(true);
    editButtonsLayout->addWidget(m_resetSelectedBtn);
    editButtonsLayout->addStretch();
    editButtonsLayout->addWidget(m_applyBtn);
    
    editGroupLayout->addWidget(m_shortcutNameLabel);
    editGroupLayout->addWidget(m_shortcutDescLabel);
    editGroupLayout->addSpacing(10);
    editGroupLayout->addWidget(keyLabel);
    editGroupLayout->addWidget(m_keySequenceRecorder);
    editGroupLayout->addWidget(m_conflictWarningLabel);
    editGroupLayout->addWidget(conflictLabel);
    editGroupLayout->addWidget(m_conflictList);
    editGroupLayout->addStretch();
    editGroupLayout->addLayout(editButtonsLayout);
    
    editLayout->addWidget(m_editGroup);
    
    // Import/Export buttons
    QHBoxLayout* ioLayout = new QHBoxLayout();
    m_importBtn = new QPushButton(tr("Import..."));
    m_exportBtn = new QPushButton(tr("Export..."));
    m_resetAllBtn = new QPushButton(tr("Reset All"));
    m_resetAllBtn->setStyleSheet("color: #ff6b6b;");
    
    ioLayout->addWidget(m_importBtn);
    ioLayout->addWidget(m_exportBtn);
    ioLayout->addStretch();
    ioLayout->addWidget(m_resetAllBtn);
    
    editLayout->addLayout(ioLayout);
    
    splitter->addWidget(editPanel);
    splitter->setSizes({600, 300});
    
    mainLayout->addWidget(splitter, 1);
    
    m_editGroup->setEnabled(false);
}

void ShortcutsConfigurator::setupConnections() {
    connect(m_searchEdit, &QLineEdit::textChanged,
            this, &ShortcutsConfigurator::onSearchTextChanged);
    connect(m_categoryCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ShortcutsConfigurator::onCategoryChanged);
    connect(m_shortcutsTree, &QTreeWidget::itemClicked,
            this, &ShortcutsConfigurator::onShortcutItemSelected);
    connect(m_shortcutsTree, &QTreeWidget::itemDoubleClicked,
            this, &ShortcutsConfigurator::onShortcutDoubleClicked);
    connect(m_keySequenceRecorder, &KeySequenceRecorder::keySequenceChanged,
            this, &ShortcutsConfigurator::onKeySequenceChanged);
    connect(m_resetSelectedBtn, &QPushButton::clicked,
            this, &ShortcutsConfigurator::onResetSelectedClicked);
    connect(m_resetAllBtn, &QPushButton::clicked,
            this, &ShortcutsConfigurator::onResetAllClicked);
    connect(m_importBtn, &QPushButton::clicked,
            this, &ShortcutsConfigurator::onImportClicked);
    connect(m_exportBtn, &QPushButton::clicked,
            this, &ShortcutsConfigurator::onExportClicked);
    connect(m_applyBtn, &QPushButton::clicked,
            this, &ShortcutsConfigurator::onApplyClicked);
    connect(m_showConflictsBtn, &QPushButton::toggled,
            this, &ShortcutsConfigurator::onShowConflictsToggled);
    connect(m_showCustomizedBtn, &QPushButton::toggled,
            this, &ShortcutsConfigurator::onShowCustomizedToggled);
}

void ShortcutsConfigurator::populateDefaultShortcuts() {
    // File category
    ShortcutCategory fileCategory;
    fileCategory.id = "file";
    fileCategory.name = tr("File");
    fileCategory.icon = "📁";
    
    fileCategory.shortcuts = {
        {"file.new", tr("New File"), tr("Create a new file"), "file",
         QKeySequence::New, QKeySequence::New, QKeySequence(), ""},
        {"file.open", tr("Open File"), tr("Open an existing file"), "file",
         QKeySequence::Open, QKeySequence::Open, QKeySequence(), ""},
        {"file.save", tr("Save"), tr("Save the current file"), "file",
         QKeySequence::Save, QKeySequence::Save, QKeySequence(), "editorFocus"},
        {"file.saveAs", tr("Save As..."), tr("Save the current file with a new name"), "file",
         QKeySequence::SaveAs, QKeySequence::SaveAs, QKeySequence(), "editorFocus"},
        {"file.saveAll", tr("Save All"), tr("Save all open files"), "file",
         QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_S), QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_S), QKeySequence(), ""},
        {"file.close", tr("Close File"), tr("Close the current file"), "file",
         QKeySequence::Close, QKeySequence::Close, QKeySequence(), "editorFocus"},
        {"file.closeAll", tr("Close All"), tr("Close all open files"), "file",
         QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_W), QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_W), QKeySequence(), ""},
    };
    m_categories[fileCategory.id] = fileCategory;
    
    // Edit category
    ShortcutCategory editCategory;
    editCategory.id = "edit";
    editCategory.name = tr("Edit");
    editCategory.icon = "✏️";
    
    editCategory.shortcuts = {
        {"edit.undo", tr("Undo"), tr("Undo the last action"), "edit",
         QKeySequence::Undo, QKeySequence::Undo, QKeySequence(), "editorFocus"},
        {"edit.redo", tr("Redo"), tr("Redo the last undone action"), "edit",
         QKeySequence::Redo, QKeySequence::Redo, QKeySequence(), "editorFocus"},
        {"edit.cut", tr("Cut"), tr("Cut selection to clipboard"), "edit",
         QKeySequence::Cut, QKeySequence::Cut, QKeySequence(), "editorFocus && editorHasSelection"},
        {"edit.copy", tr("Copy"), tr("Copy selection to clipboard"), "edit",
         QKeySequence::Copy, QKeySequence::Copy, QKeySequence(), "editorFocus && editorHasSelection"},
        {"edit.paste", tr("Paste"), tr("Paste from clipboard"), "edit",
         QKeySequence::Paste, QKeySequence::Paste, QKeySequence(), "editorFocus"},
        {"edit.selectAll", tr("Select All"), tr("Select all content"), "edit",
         QKeySequence::SelectAll, QKeySequence::SelectAll, QKeySequence(), "editorFocus"},
        {"edit.find", tr("Find"), tr("Open find dialog"), "edit",
         QKeySequence::Find, QKeySequence::Find, QKeySequence(), ""},
        {"edit.replace", tr("Replace"), tr("Open find and replace dialog"), "edit",
         QKeySequence::Replace, QKeySequence::Replace, QKeySequence(), "editorFocus"},
        {"edit.findNext", tr("Find Next"), tr("Find next occurrence"), "edit",
         QKeySequence::FindNext, QKeySequence::FindNext, QKeySequence(), "findWidgetVisible"},
        {"edit.findPrevious", tr("Find Previous"), tr("Find previous occurrence"), "edit",
         QKeySequence::FindPrevious, QKeySequence::FindPrevious, QKeySequence(), "findWidgetVisible"},
    };
    m_categories[editCategory.id] = editCategory;
    
    // Editor category
    ShortcutCategory editorCategory;
    editorCategory.id = "editor";
    editorCategory.name = tr("Editor");
    editorCategory.icon = "📝";
    
    editorCategory.shortcuts = {
        {"editor.duplicateLine", tr("Duplicate Line"), tr("Duplicate the current line"), "editor",
         QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_D), QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_D), QKeySequence(), "editorTextFocus"},
        {"editor.deleteLine", tr("Delete Line"), tr("Delete the current line"), "editor",
         QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_K), QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_K), QKeySequence(), "editorTextFocus"},
        {"editor.moveLineUp", tr("Move Line Up"), tr("Move line up"), "editor",
         QKeySequence(Qt::ALT | Qt::Key_Up), QKeySequence(Qt::ALT | Qt::Key_Up), QKeySequence(), "editorTextFocus"},
        {"editor.moveLineDown", tr("Move Line Down"), tr("Move line down"), "editor",
         QKeySequence(Qt::ALT | Qt::Key_Down), QKeySequence(Qt::ALT | Qt::Key_Down), QKeySequence(), "editorTextFocus"},
        {"editor.toggleComment", tr("Toggle Comment"), tr("Toggle line comment"), "editor",
         QKeySequence(Qt::CTRL | Qt::Key_Slash), QKeySequence(Qt::CTRL | Qt::Key_Slash), QKeySequence(), "editorTextFocus"},
        {"editor.toggleBlockComment", tr("Toggle Block Comment"), tr("Toggle block comment"), "editor",
         QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_Slash), QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_Slash), QKeySequence(), "editorTextFocus"},
        {"editor.indentLine", tr("Indent Line"), tr("Indent the current line"), "editor",
         QKeySequence(Qt::CTRL | Qt::Key_BracketRight), QKeySequence(Qt::CTRL | Qt::Key_BracketRight), QKeySequence(), "editorTextFocus"},
        {"editor.outdentLine", tr("Outdent Line"), tr("Outdent the current line"), "editor",
         QKeySequence(Qt::CTRL | Qt::Key_BracketLeft), QKeySequence(Qt::CTRL | Qt::Key_BracketLeft), QKeySequence(), "editorTextFocus"},
        {"editor.format", tr("Format Document"), tr("Format the entire document"), "editor",
         QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_F), QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_F), QKeySequence(), "editorTextFocus"},
        {"editor.formatSelection", tr("Format Selection"), tr("Format selected code"), "editor",
         QKeySequence(Qt::CTRL | Qt::Key_K, Qt::CTRL | Qt::Key_F), QKeySequence(Qt::CTRL | Qt::Key_K, Qt::CTRL | Qt::Key_F), QKeySequence(), "editorTextFocus && editorHasSelection"},
    };
    m_categories[editorCategory.id] = editorCategory;
    
    // Navigation category
    ShortcutCategory navCategory;
    navCategory.id = "navigation";
    navCategory.name = tr("Navigation");
    navCategory.icon = "🧭";
    
    navCategory.shortcuts = {
        {"nav.goToLine", tr("Go to Line"), tr("Go to a specific line number"), "navigation",
         QKeySequence(Qt::CTRL | Qt::Key_G), QKeySequence(Qt::CTRL | Qt::Key_G), QKeySequence(), "editorFocus"},
        {"nav.goToFile", tr("Go to File"), tr("Quick open file"), "navigation",
         QKeySequence(Qt::CTRL | Qt::Key_P), QKeySequence(Qt::CTRL | Qt::Key_P), QKeySequence(), ""},
        {"nav.goToSymbol", tr("Go to Symbol"), tr("Go to symbol in file"), "navigation",
         QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_O), QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_O), QKeySequence(), "editorFocus"},
        {"nav.goToDefinition", tr("Go to Definition"), tr("Go to symbol definition"), "navigation",
         QKeySequence(Qt::Key_F12), QKeySequence(Qt::Key_F12), QKeySequence(), "editorTextFocus"},
        {"nav.goToDeclaration", tr("Go to Declaration"), tr("Go to symbol declaration"), "navigation",
         QKeySequence(Qt::CTRL | Qt::Key_F12), QKeySequence(Qt::CTRL | Qt::Key_F12), QKeySequence(), "editorTextFocus"},
        {"nav.peekDefinition", tr("Peek Definition"), tr("Peek symbol definition"), "navigation",
         QKeySequence(Qt::ALT | Qt::Key_F12), QKeySequence(Qt::ALT | Qt::Key_F12), QKeySequence(), "editorTextFocus"},
        {"nav.findReferences", tr("Find All References"), tr("Find all references to symbol"), "navigation",
         QKeySequence(Qt::SHIFT | Qt::Key_F12), QKeySequence(Qt::SHIFT | Qt::Key_F12), QKeySequence(), "editorTextFocus"},
        {"nav.goBack", tr("Go Back"), tr("Navigate back"), "navigation",
         QKeySequence(Qt::ALT | Qt::Key_Left), QKeySequence(Qt::ALT | Qt::Key_Left), QKeySequence(), ""},
        {"nav.goForward", tr("Go Forward"), tr("Navigate forward"), "navigation",
         QKeySequence(Qt::ALT | Qt::Key_Right), QKeySequence(Qt::ALT | Qt::Key_Right), QKeySequence(), ""},
    };
    m_categories[navCategory.id] = navCategory;
    
    // View category
    ShortcutCategory viewCategory;
    viewCategory.id = "view";
    viewCategory.name = tr("View");
    viewCategory.icon = "👁️";
    
    viewCategory.shortcuts = {
        {"view.toggleSidebar", tr("Toggle Sidebar"), tr("Show/hide the sidebar"), "view",
         QKeySequence(Qt::CTRL | Qt::Key_B), QKeySequence(Qt::CTRL | Qt::Key_B), QKeySequence(), ""},
        {"view.togglePanel", tr("Toggle Panel"), tr("Show/hide the bottom panel"), "view",
         QKeySequence(Qt::CTRL | Qt::Key_J), QKeySequence(Qt::CTRL | Qt::Key_J), QKeySequence(), ""},
        {"view.toggleFullScreen", tr("Toggle Full Screen"), tr("Toggle full screen mode"), "view",
         QKeySequence(Qt::Key_F11), QKeySequence(Qt::Key_F11), QKeySequence(), ""},
        {"view.zoomIn", tr("Zoom In"), tr("Increase zoom level"), "view",
         QKeySequence::ZoomIn, QKeySequence::ZoomIn, QKeySequence(), ""},
        {"view.zoomOut", tr("Zoom Out"), tr("Decrease zoom level"), "view",
         QKeySequence::ZoomOut, QKeySequence::ZoomOut, QKeySequence(), ""},
        {"view.resetZoom", tr("Reset Zoom"), tr("Reset zoom to default"), "view",
         QKeySequence(Qt::CTRL | Qt::Key_0), QKeySequence(Qt::CTRL | Qt::Key_0), QKeySequence(), ""},
        {"view.explorer", tr("Show Explorer"), tr("Show file explorer"), "view",
         QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_E), QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_E), QKeySequence(), ""},
        {"view.search", tr("Show Search"), tr("Show search panel"), "view",
         QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_F), QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_F), QKeySequence(), ""},
        {"view.sourceControl", tr("Show Source Control"), tr("Show source control panel"), "view",
         QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_G), QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_G), QKeySequence(), ""},
        {"view.debug", tr("Show Debug"), tr("Show debug panel"), "view",
         QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_D), QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_D), QKeySequence(), ""},
        {"view.extensions", tr("Show Extensions"), tr("Show extensions panel"), "view",
         QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_X), QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_X), QKeySequence(), ""},
    };
    m_categories[viewCategory.id] = viewCategory;
    
    // Debug category
    ShortcutCategory debugCategory;
    debugCategory.id = "debug";
    debugCategory.name = tr("Debug");
    debugCategory.icon = "🐛";
    
    debugCategory.shortcuts = {
        {"debug.start", tr("Start Debugging"), tr("Start debugging session"), "debug",
         QKeySequence(Qt::Key_F5), QKeySequence(Qt::Key_F5), QKeySequence(), ""},
        {"debug.stop", tr("Stop Debugging"), tr("Stop debugging session"), "debug",
         QKeySequence(Qt::SHIFT | Qt::Key_F5), QKeySequence(Qt::SHIFT | Qt::Key_F5), QKeySequence(), "inDebugMode"},
        {"debug.restart", tr("Restart Debugging"), tr("Restart debugging session"), "debug",
         QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_F5), QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_F5), QKeySequence(), "inDebugMode"},
        {"debug.stepOver", tr("Step Over"), tr("Step over to next line"), "debug",
         QKeySequence(Qt::Key_F10), QKeySequence(Qt::Key_F10), QKeySequence(), "inDebugMode"},
        {"debug.stepInto", tr("Step Into"), tr("Step into function"), "debug",
         QKeySequence(Qt::Key_F11), QKeySequence(Qt::Key_F11), QKeySequence(), "inDebugMode"},
        {"debug.stepOut", tr("Step Out"), tr("Step out of function"), "debug",
         QKeySequence(Qt::SHIFT | Qt::Key_F11), QKeySequence(Qt::SHIFT | Qt::Key_F11), QKeySequence(), "inDebugMode"},
        {"debug.continue", tr("Continue"), tr("Continue execution"), "debug",
         QKeySequence(Qt::Key_F5), QKeySequence(Qt::Key_F5), QKeySequence(), "inDebugMode"},
        {"debug.toggleBreakpoint", tr("Toggle Breakpoint"), tr("Toggle breakpoint on current line"), "debug",
         QKeySequence(Qt::Key_F9), QKeySequence(Qt::Key_F9), QKeySequence(), "editorTextFocus"},
    };
    m_categories[debugCategory.id] = debugCategory;
    
    // Terminal category
    ShortcutCategory terminalCategory;
    terminalCategory.id = "terminal";
    terminalCategory.name = tr("Terminal");
    terminalCategory.icon = "💻";
    
    terminalCategory.shortcuts = {
        {"terminal.new", tr("New Terminal"), tr("Create new terminal"), "terminal",
         QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_QuoteLeft), QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_QuoteLeft), QKeySequence(), ""},
        {"terminal.toggle", tr("Toggle Terminal"), tr("Show/hide terminal"), "terminal",
         QKeySequence(Qt::CTRL | Qt::Key_QuoteLeft), QKeySequence(Qt::CTRL | Qt::Key_QuoteLeft), QKeySequence(), ""},
        {"terminal.kill", tr("Kill Terminal"), tr("Kill active terminal"), "terminal",
         QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_Q), QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_Q), QKeySequence(), "terminalFocus"},
        {"terminal.clear", tr("Clear Terminal"), tr("Clear terminal output"), "terminal",
         QKeySequence(Qt::CTRL | Qt::Key_L), QKeySequence(Qt::CTRL | Qt::Key_L), QKeySequence(), "terminalFocus"},
        {"terminal.split", tr("Split Terminal"), tr("Split terminal horizontally"), "terminal",
         QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_5), QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_5), QKeySequence(), "terminalFocus"},
    };
    m_categories[terminalCategory.id] = terminalCategory;
    
    // Build all shortcuts into the main index
    for (const auto& category : m_categories) {
        m_categoryCombo->addItem(category.name, category.id);
        for (const auto& shortcut : category.shortcuts) {
            m_shortcuts[shortcut.id] = shortcut;
            if (!shortcut.currentShortcut.isEmpty()) {
                m_shortcutIndex[shortcut.currentShortcut].insert(shortcut.id);
            }
        }
    }
}

void ShortcutsConfigurator::populateTree() {
    m_shortcutsTree->clear();
    
    for (const auto& category : m_categories) {
        // Apply filters
        bool hasVisibleShortcuts = false;
        
        QTreeWidgetItem* categoryItem = new QTreeWidgetItem();
        categoryItem->setText(0, QString("%1 %2").arg(category.icon, category.name));
        categoryItem->setData(0, Qt::UserRole, category.id);
        categoryItem->setFlags(categoryItem->flags() & ~Qt::ItemIsSelectable);
        
        for (const auto& shortcut : category.shortcuts) {
            // Apply search filter
            if (!m_currentFilter.isEmpty()) {
                bool matches = shortcut.name.contains(m_currentFilter, Qt::CaseInsensitive) ||
                              shortcut.id.contains(m_currentFilter, Qt::CaseInsensitive) ||
                              shortcut.description.contains(m_currentFilter, Qt::CaseInsensitive) ||
                              shortcut.currentShortcut.toString().contains(m_currentFilter, Qt::CaseInsensitive);
                if (!matches) continue;
            }
            
            // Apply category filter
            if (!m_currentCategory.isEmpty() && shortcut.category != m_currentCategory) {
                continue;
            }
            
            // Apply customized filter
            if (m_showCustomizedOnly && !shortcut.isCustomized) {
                continue;
            }
            
            // Apply conflicts filter
            if (m_showConflictsOnly && !shortcut.hasConflict()) {
                continue;
            }
            
            hasVisibleShortcuts = true;
            
            QTreeWidgetItem* item = new QTreeWidgetItem(categoryItem);
            updateTreeItem(item, shortcut);
        }
        
        if (hasVisibleShortcuts) {
            m_shortcutsTree->addTopLevelItem(categoryItem);
            categoryItem->setExpanded(true);
        } else {
            delete categoryItem;
        }
    }
    
    highlightConflicts();
}

void ShortcutsConfigurator::updateTreeItem(QTreeWidgetItem* item, const ShortcutInfo& shortcut) {
    item->setText(0, shortcut.name);
    item->setText(1, formatKeySequence(shortcut.currentShortcut));
    item->setText(2, shortcut.whenClause);
    item->setText(3, shortcut.isCustomized ? tr("User") : tr("Default"));
    item->setData(0, Qt::UserRole, shortcut.id);
    item->setToolTip(0, shortcut.description);
    
    // Style customized shortcuts
    if (shortcut.isCustomized) {
        item->setForeground(3, QColor("#4fc3f7"));
    }
    
    // Style conflicting shortcuts
    if (shortcut.hasConflict()) {
        item->setForeground(1, QColor("#ff6b6b"));
        item->setIcon(0, style()->standardIcon(QStyle::SP_MessageBoxWarning));
    }
}

QTreeWidgetItem* ShortcutsConfigurator::findItemById(const QString& id) const {
    for (int i = 0; i < m_shortcutsTree->topLevelItemCount(); ++i) {
        QTreeWidgetItem* category = m_shortcutsTree->topLevelItem(i);
        for (int j = 0; j < category->childCount(); ++j) {
            QTreeWidgetItem* item = category->child(j);
            if (item->data(0, Qt::UserRole).toString() == id) {
                return item;
            }
        }
    }
    return nullptr;
}

void ShortcutsConfigurator::highlightConflicts() {
    auto conflicts = detectConflicts();
    
    // Clear existing conflicts
    for (auto& shortcut : m_shortcuts) {
        shortcut.conflictWith.clear();
    }
    
    // Mark conflicting shortcuts
    for (const auto& conflict : conflicts) {
        if (m_shortcuts.contains(conflict.shortcut1Id)) {
            m_shortcuts[conflict.shortcut1Id].conflictWith = conflict.shortcut2Id;
        }
        if (m_shortcuts.contains(conflict.shortcut2Id)) {
            m_shortcuts[conflict.shortcut2Id].conflictWith = conflict.shortcut1Id;
        }
    }
    
    // Update tree items
    for (int i = 0; i < m_shortcutsTree->topLevelItemCount(); ++i) {
        QTreeWidgetItem* category = m_shortcutsTree->topLevelItem(i);
        for (int j = 0; j < category->childCount(); ++j) {
            QTreeWidgetItem* item = category->child(j);
            QString id = item->data(0, Qt::UserRole).toString();
            if (m_shortcuts.contains(id)) {
                updateTreeItem(item, m_shortcuts[id]);
            }
        }
    }
}

void ShortcutsConfigurator::showConflictDialog(const QList<QString>& conflictingIds) {
    QString message = tr("The following shortcuts have the same keybinding:\n\n");
    
    for (const QString& id : conflictingIds) {
        if (m_shortcuts.contains(id)) {
            const auto& shortcut = m_shortcuts[id];
            message += QString("• %1 (%2)\n").arg(shortcut.name, shortcut.id);
        }
    }
    
    message += tr("\nWould you like to keep this binding anyway?");
    
    QMessageBox::warning(this, tr("Shortcut Conflict"), message);
}

QString ShortcutsConfigurator::formatKeySequence(const QKeySequence& sequence) const {
    if (sequence.isEmpty()) {
        return tr("(unbound)");
    }
    return sequence.toString(QKeySequence::NativeText);
}

ShortcutInfo* ShortcutsConfigurator::findShortcut(const QString& id) {
    auto it = m_shortcuts.find(id);
    return it != m_shortcuts.end() ? &it.value() : nullptr;
}

const ShortcutInfo* ShortcutsConfigurator::findShortcut(const QString& id) const {
    auto it = m_shortcuts.find(id);
    return it != m_shortcuts.end() ? &it.value() : nullptr;
}

// Slot implementations

void ShortcutsConfigurator::onSearchTextChanged(const QString& text) {
    m_currentFilter = text;
    populateTree();
}

void ShortcutsConfigurator::onCategoryChanged(int index) {
    m_currentCategory = m_categoryCombo->itemData(index).toString();
    populateTree();
}

void ShortcutsConfigurator::onShortcutItemSelected(QTreeWidgetItem* item, int column) {
    Q_UNUSED(column);
    
    if (!item || !item->parent()) {
        m_editGroup->setEnabled(false);
        m_selectedShortcutId.clear();
        return;
    }
    
    QString id = item->data(0, Qt::UserRole).toString();
    const ShortcutInfo* shortcut = findShortcut(id);
    
    if (!shortcut) {
        m_editGroup->setEnabled(false);
        return;
    }
    
    m_selectedShortcutId = id;
    m_editGroup->setEnabled(true);
    
    m_shortcutNameLabel->setText(shortcut->name);
    m_shortcutDescLabel->setText(shortcut->description);
    m_keySequenceRecorder->setKeySequence(shortcut->currentShortcut);
    
    // Check for conflicts
    QStringList conflicts = getConflictingShortcuts(shortcut->currentShortcut);
    conflicts.removeAll(id);
    
    if (!conflicts.isEmpty()) {
        m_conflictWarningLabel->setText(tr("⚠️ Conflicts with: %1").arg(conflicts.join(", ")));
        m_conflictWarningLabel->show();
        m_conflictList->clear();
        m_conflictList->addItems(conflicts);
        m_conflictList->show();
    } else {
        m_conflictWarningLabel->hide();
        m_conflictList->hide();
    }
}

void ShortcutsConfigurator::onShortcutDoubleClicked(QTreeWidgetItem* item, int column) {
    Q_UNUSED(column);
    
    if (!item || !item->parent()) return;
    
    m_keySequenceRecorder->setFocus();
}

void ShortcutsConfigurator::onKeySequenceChanged(const QKeySequence& sequence) {
    if (m_selectedShortcutId.isEmpty()) return;
    
    // Check for conflicts
    QStringList conflicts = getConflictingShortcuts(sequence);
    conflicts.removeAll(m_selectedShortcutId);
    
    if (!conflicts.isEmpty()) {
        m_conflictWarningLabel->setText(tr("⚠️ Conflicts with: %1").arg(conflicts.join(", ")));
        m_conflictWarningLabel->show();
        m_conflictList->clear();
        for (const QString& conflictId : conflicts) {
            if (m_shortcuts.contains(conflictId)) {
                m_conflictList->addItem(QString("%1 - %2")
                    .arg(m_shortcuts[conflictId].name, conflictId));
            }
        }
        m_conflictList->show();
    } else {
        m_conflictWarningLabel->hide();
        m_conflictList->hide();
    }
}

void ShortcutsConfigurator::onResetSelectedClicked() {
    if (m_selectedShortcutId.isEmpty()) return;
    
    resetShortcut(m_selectedShortcutId);
    
    // Update UI
    if (ShortcutInfo* shortcut = findShortcut(m_selectedShortcutId)) {
        m_keySequenceRecorder->setKeySequence(shortcut->currentShortcut);
    }
    
    populateTree();
}

void ShortcutsConfigurator::onResetAllClicked() {
    QMessageBox::StandardButton reply = QMessageBox::question(
        this,
        tr("Reset All Shortcuts"),
        tr("Are you sure you want to reset all keyboard shortcuts to their defaults?\n\n"
           "This action cannot be undone."),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No
    );
    
    if (reply == QMessageBox::Yes) {
        resetAllShortcuts();
        populateTree();
        
        if (!m_selectedShortcutId.isEmpty()) {
            if (ShortcutInfo* shortcut = findShortcut(m_selectedShortcutId)) {
                m_keySequenceRecorder->setKeySequence(shortcut->currentShortcut);
            }
        }
    }
}

void ShortcutsConfigurator::onImportClicked() {
    QString filePath = QFileDialog::getOpenFileName(
        this,
        tr("Import Keyboard Shortcuts"),
        QString(),
        tr("JSON Files (*.json);;All Files (*.*)")
    );
    
    if (!filePath.isEmpty()) {
        if (importShortcuts(filePath)) {
            populateTree();
            QMessageBox::information(this, tr("Import Successful"),
                tr("Keyboard shortcuts imported successfully."));
        }
    }
}

void ShortcutsConfigurator::onExportClicked() {
    QString filePath = QFileDialog::getSaveFileName(
        this,
        tr("Export Keyboard Shortcuts"),
        "keybindings.json",
        tr("JSON Files (*.json);;All Files (*.*)")
    );
    
    if (!filePath.isEmpty()) {
        if (exportShortcuts(filePath)) {
            QMessageBox::information(this, tr("Export Successful"),
                tr("Keyboard shortcuts exported to:\n%1").arg(filePath));
        }
    }
}

void ShortcutsConfigurator::onApplyClicked() {
    if (m_selectedShortcutId.isEmpty()) return;
    
    QKeySequence newSequence = m_keySequenceRecorder->keySequence();
    
    // Check conflicts one more time
    QStringList conflicts = getConflictingShortcuts(newSequence);
    conflicts.removeAll(m_selectedShortcutId);
    
    if (!conflicts.isEmpty()) {
        QMessageBox::StandardButton reply = QMessageBox::question(
            this,
            tr("Shortcut Conflict"),
            tr("This shortcut conflicts with:\n%1\n\nDo you want to assign it anyway?")
                .arg(conflicts.join("\n")),
            QMessageBox::Yes | QMessageBox::No,
            QMessageBox::No
        );
        
        if (reply != QMessageBox::Yes) {
            return;
        }
    }
    
    updateShortcut(m_selectedShortcutId, newSequence);
    populateTree();
    
    // Re-select the item
    if (QTreeWidgetItem* item = findItemById(m_selectedShortcutId)) {
        m_shortcutsTree->setCurrentItem(item);
    }
}

void ShortcutsConfigurator::onShowConflictsToggled(bool checked) {
    m_showConflictsOnly = checked;
    populateTree();
}

void ShortcutsConfigurator::onShowCustomizedToggled(bool checked) {
    m_showCustomizedOnly = checked;
    populateTree();
}

// Public methods

void ShortcutsConfigurator::loadShortcuts() {
    // Load from settings/file
    // This would typically load from QSettings or a JSON file
}

void ShortcutsConfigurator::saveShortcuts() {
    // Save to settings/file
    m_modified = false;
    emit shortcutsModified();
}

void ShortcutsConfigurator::addShortcut(const ShortcutInfo& shortcut) {
    m_shortcuts[shortcut.id] = shortcut;
    
    if (!shortcut.currentShortcut.isEmpty()) {
        m_shortcutIndex[shortcut.currentShortcut].insert(shortcut.id);
    }
    
    // Add to category
    if (m_categories.contains(shortcut.category)) {
        m_categories[shortcut.category].shortcuts.append(shortcut);
    }
    
    m_modified = true;
}

void ShortcutsConfigurator::updateShortcut(const QString& id, const QKeySequence& sequence) {
    ShortcutInfo* shortcut = findShortcut(id);
    if (!shortcut) return;
    
    QKeySequence oldSequence = shortcut->currentShortcut;
    
    // Remove from old index
    if (!oldSequence.isEmpty()) {
        m_shortcutIndex[oldSequence].remove(id);
        if (m_shortcutIndex[oldSequence].isEmpty()) {
            m_shortcutIndex.remove(oldSequence);
        }
    }
    
    // Update shortcut
    shortcut->currentShortcut = sequence;
    shortcut->isCustomized = (sequence != shortcut->defaultShortcut);
    
    // Add to new index
    if (!sequence.isEmpty()) {
        m_shortcutIndex[sequence].insert(id);
    }
    
    m_modified = true;
    emit shortcutChanged(id, oldSequence, sequence);
    
    highlightConflicts();
}

void ShortcutsConfigurator::removeShortcut(const QString& id) {
    if (!m_shortcuts.contains(id)) return;
    
    const ShortcutInfo& shortcut = m_shortcuts[id];
    
    // Remove from index
    if (!shortcut.currentShortcut.isEmpty()) {
        m_shortcutIndex[shortcut.currentShortcut].remove(id);
    }
    
    m_shortcuts.remove(id);
    m_modified = true;
}

void ShortcutsConfigurator::resetShortcut(const QString& id) {
    ShortcutInfo* shortcut = findShortcut(id);
    if (!shortcut) return;
    
    updateShortcut(id, shortcut->defaultShortcut);
    shortcut->isCustomized = false;
}

void ShortcutsConfigurator::resetAllShortcuts() {
    for (auto& shortcut : m_shortcuts) {
        if (shortcut.isCustomized) {
            // Remove from old index
            if (!shortcut.currentShortcut.isEmpty()) {
                m_shortcutIndex[shortcut.currentShortcut].remove(shortcut.id);
            }
            
            shortcut.currentShortcut = shortcut.defaultShortcut;
            shortcut.isCustomized = false;
            
            // Add to new index
            if (!shortcut.defaultShortcut.isEmpty()) {
                m_shortcutIndex[shortcut.defaultShortcut].insert(shortcut.id);
            }
        }
    }
    
    m_modified = true;
    highlightConflicts();
}

void ShortcutsConfigurator::addCategory(const ShortcutCategory& category) {
    m_categories[category.id] = category;
    m_categoryCombo->addItem(category.name, category.id);
}

QStringList ShortcutsConfigurator::categories() const {
    return m_categories.keys();
}

QList<ShortcutConflictInfo> ShortcutsConfigurator::detectConflicts() const {
    QList<ShortcutConflictInfo> conflicts;
    
    for (auto it = m_shortcutIndex.begin(); it != m_shortcutIndex.end(); ++it) {
        if (it.value().size() > 1) {
            // Found a conflict
            QList<QString> ids = it.value().values();
            
            for (int i = 0; i < ids.size() - 1; ++i) {
                for (int j = i + 1; j < ids.size(); ++j) {
                    // Check if they have the same "when" clause (true conflict)
                    const ShortcutInfo* s1 = findShortcut(ids[i]);
                    const ShortcutInfo* s2 = findShortcut(ids[j]);
                    
                    if (s1 && s2) {
                        // If both have same or overlapping when clauses, it's a real conflict
                        bool realConflict = s1->whenClause.isEmpty() || 
                                           s2->whenClause.isEmpty() ||
                                           s1->whenClause == s2->whenClause;
                        
                        if (realConflict) {
                            ShortcutConflictInfo conflict;
                            conflict.shortcut1Id = ids[i];
                            conflict.shortcut2Id = ids[j];
                            conflict.conflictingSequence = it.key();
                            conflict.resolution = tr("Change one of the shortcuts");
                            conflicts.append(conflict);
                        }
                    }
                }
            }
        }
    }
    
    return conflicts;
}

bool ShortcutsConfigurator::hasConflict(const QKeySequence& sequence, const QString& excludeId) const {
    if (sequence.isEmpty()) return false;
    
    auto it = m_shortcutIndex.find(sequence);
    if (it == m_shortcutIndex.end()) return false;
    
    QSet<QString> ids = it.value();
    if (!excludeId.isEmpty()) {
        ids.remove(excludeId);
    }
    
    return !ids.isEmpty();
}

QStringList ShortcutsConfigurator::getConflictingShortcuts(const QKeySequence& sequence) const {
    if (sequence.isEmpty()) return QStringList();
    
    auto it = m_shortcutIndex.find(sequence);
    if (it == m_shortcutIndex.end()) return QStringList();
    
    return it.value().values();
}

bool ShortcutsConfigurator::importShortcuts(const QString& filePath) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        QMessageBox::warning(this, tr("Import Error"),
            tr("Could not open file:\n%1").arg(filePath));
        return false;
    }
    
    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &error);
    file.close();
    
    if (error.error != QJsonParseError::NoError) {
        QMessageBox::warning(this, tr("Import Error"),
            tr("Invalid JSON:\n%1").arg(error.errorString()));
        return false;
    }
    
    fromJson(doc.object());
    
    int count = doc.object()["shortcuts"].toArray().size();
    emit shortcutsImported(count);
    
    return true;
}

bool ShortcutsConfigurator::exportShortcuts(const QString& filePath) const {
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        QMessageBox::warning(const_cast<ShortcutsConfigurator*>(this), 
            tr("Export Error"),
            tr("Could not write to file:\n%1").arg(filePath));
        return false;
    }
    
    QJsonDocument doc(toJson());
    file.write(doc.toJson(QJsonDocument::Indented));
    file.close();
    
    emit const_cast<ShortcutsConfigurator*>(this)->shortcutsExported(filePath);
    
    return true;
}

QJsonObject ShortcutsConfigurator::toJson() const {
    QJsonObject root;
    QJsonArray shortcuts;
    
    for (const auto& shortcut : m_shortcuts) {
        if (shortcut.isCustomized) {
            QJsonObject obj;
            obj["id"] = shortcut.id;
            obj["key"] = shortcut.currentShortcut.toString();
            if (!shortcut.whenClause.isEmpty()) {
                obj["when"] = shortcut.whenClause;
            }
            shortcuts.append(obj);
        }
    }
    
    root["shortcuts"] = shortcuts;
    root["version"] = "1.0";
    
    return root;
}

void ShortcutsConfigurator::fromJson(const QJsonObject& json) {
    QJsonArray shortcuts = json["shortcuts"].toArray();
    
    for (const QJsonValue& value : shortcuts) {
        QJsonObject obj = value.toObject();
        QString id = obj["id"].toString();
        QString key = obj["key"].toString();
        
        if (m_shortcuts.contains(id)) {
            QKeySequence sequence(key);
            updateShortcut(id, sequence);
        }
    }
}

void ShortcutsConfigurator::setSearchFilter(const QString& filter) {
    m_searchEdit->setText(filter);
}

void ShortcutsConfigurator::setCategoryFilter(const QString& category) {
    int index = m_categoryCombo->findData(category);
    if (index >= 0) {
        m_categoryCombo->setCurrentIndex(index);
    }
}

void ShortcutsConfigurator::setShowCustomizedOnly(bool show) {
    m_showCustomizedBtn->setChecked(show);
}

void ShortcutsConfigurator::setShowConflictsOnly(bool show) {
    m_showConflictsBtn->setChecked(show);
}
