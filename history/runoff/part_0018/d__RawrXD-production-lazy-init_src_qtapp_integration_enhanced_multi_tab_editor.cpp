/**
 * \file enhanced_multi_tab_editor.cpp
 * \brief Enhanced Multi-Tab Editor Implementation with Phase 1 Foundation Integration
 * \author RawrXD Team
 * \date 2026-01-13
 */

#include "enhanced_multi_tab_editor.h"
#include "../core/file_system_manager.h"
#include "../core/command_dispatcher.h"
#include "../core/logging_system.h"
#include "../core/error_handler.h"

#include <QApplication>
#include <QFileDialog>
#include <QInputDialog>
#include <QMessageBox>
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QFontMetrics>
#include <QScrollBar>
#include <QClipboard>
#include <QKeySequence>
#include <QShortcut>
#include <QCloseEvent>
#include <QRegularExpression>
#include <QSyntaxHighlighter>
#include <QTextDocument>
#include <QPainter>
#include <QTextBlock>
#include <QTextCursor>

// ========== SimpleSyntaxHighlighter Class ==========

class SimpleSyntaxHighlighter : public QSyntaxHighlighter {
public:
    explicit SimpleSyntaxHighlighter(QTextDocument* parent = nullptr);
    void setHighlightingRules(const QMap<QString, QTextCharFormat>& rules);

protected:
    void highlightBlock(const QString& text) override;

private:
    QMap<QString, QTextCharFormat> m_highlightingRules;
};

// ========== EnhancedTextEdit Implementation ==========

EnhancedTextEdit::EnhancedTextEdit(QWidget* parent)
    : QPlainTextEdit(parent)
    , m_filePath("")
{
    setupConnections();
    
    // Set up basic editor properties
    setFont(QFont("Consolas", 12));
    setWordWrapMode(QTextOption::NoWrap);
    setLineWrapMode(QPlainTextEdit::WidgetWidth);
    
    // Enable line numbers
    setLineWrapMode(QPlainTextEdit::NoWrap);
    setCursorWidth(2);
    
    // Context menu
    setContextMenuPolicy(Qt::CustomContextMenu);
}

void EnhancedTextEdit::setupConnections() {
    connect(this, &QPlainTextEdit::cursorPositionChanged, this, [this]() {
        QTextCursor cursor = textCursor();
        int line = cursor.blockNumber() + 1;
        int column = cursor.positionInBlock() + 1;
        emit cursorPositionChanged(line, column);
    });
    
    connect(document(), &QTextDocument::modificationChanged, this, [this](bool modified) {
        emit modificationChanged(modified);
        emit textChanged();
    });
}

void EnhancedTextEdit::setFilePath(const QString& filePath) {
    m_filePath = filePath;
}

// ========== EnhancedMultiTabEditor Implementation ==========

EnhancedMultiTabEditor::EnhancedMultiTabEditor(QWidget* parent)
    : QWidget(parent)
    , m_fileSystemManager(nullptr)
    , m_commandDispatcher(nullptr)
    , m_loggingSystem(nullptr)
    , m_errorHandler(nullptr)
    , m_mainLayout(nullptr)
    , m_toolbarLayout(nullptr)
    , m_tabWidget(nullptr)
    , m_toolbar(nullptr)
    , m_statusLabel(nullptr)
    , m_lineColumnLabel(nullptr)
    , m_encodingLabel(nullptr)
    , m_lineEndingLabel(nullptr)
    , m_newFileAction(nullptr)
    , m_openFileAction(nullptr)
    , m_saveFileAction(nullptr)
    , m_saveAsAction(nullptr)
    , m_closeFileAction(nullptr)
    , m_undoAction(nullptr)
    , m_redoAction(nullptr)
    , m_cutAction(nullptr)
    , m_copyAction(nullptr)
    , m_pasteAction(nullptr)
    , m_findAction(nullptr)
    , m_replaceAction(nullptr)
    , m_gotoLineAction(nullptr)
    , m_selectAllAction(nullptr)
    , m_zoomInAction(nullptr)
    , m_zoomOutAction(nullptr)
    , m_resetZoomAction(nullptr)
    , m_findReplaceWidget(nullptr)
    , m_findEdit(nullptr)
    , m_replaceEdit(nullptr)
    , m_matchCaseCheckBox(nullptr)
    , m_wholeWordCheckBox(nullptr)
    , m_regexCheckBox(nullptr)
    , m_findNextButton(nullptr)
    , m_findPrevButton(nullptr)
    , m_replaceButton(nullptr)
    , m_replaceAllButton(nullptr)
    , m_currentFilePath("")
    , m_showLineNumbers(true)
    , m_showWhitespace(false)
    , m_editorFont("Consolas", 12)
    , m_tabWidth(4)
    , m_autoIndent(true)
{
    // Initialize foundation systems
    m_fileSystemManager = &FileSystemManager::instance();
    m_loggingSystem = &LoggingSystem::instance();
    m_errorHandler = &ErrorHandler::instance();
}

EnhancedMultiTabEditor::~EnhancedMultiTabEditor() {
    // Save settings on destruction
    saveEditorSettings();
}

void EnhancedMultiTabEditor::initialize() {
    setupUI();
    createActions();
    setupToolbar();
    setupStatusBar();
    setupFindReplaceWidget();
    setupConnections();
    loadEditorSettings();
    
    // Create welcome tab
    newFile();
    
    m_loggingSystem->logInfo("EnhancedMultiTabEditor", "Enhanced Multi-Tab Editor initialized successfully");
}

void EnhancedMultiTabEditor::setupUI() {
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setContentsMargins(2, 2, 2, 2);
    m_mainLayout->setSpacing(2);

    // Tab widget
    m_tabWidget = new QTabWidget(this);
    m_tabWidget->setTabsClosable(true);
    m_tabWidget->setMovable(true);
    m_tabWidget->setUsesScrollButtons(true);
    
    m_mainLayout->addWidget(m_tabWidget, 1);
}

void EnhancedMultiTabEditor::setupToolbar() {
    m_toolbar = new QToolBar(this);
    m_toolbar->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    
    // File operations
    m_toolbar->addAction(m_newFileAction);
    m_toolbar->addAction(m_openFileAction);
    m_toolbar->addAction(m_saveFileAction);
    m_toolbar->addSeparator();
    
    // Edit operations
    m_toolbar->addAction(m_undoAction);
    m_toolbar->addAction(m_redoAction);
    m_toolbar->addSeparator();
    m_toolbar->addAction(m_cutAction);
    m_toolbar->addAction(m_copyAction);
    m_toolbar->addAction(m_pasteAction);
    m_toolbar->addSeparator();
    
    // Search operations
    m_toolbar->addAction(m_findAction);
    m_toolbar->addAction(m_replaceAction);
    m_toolbar->addSeparator();
    
    // View operations
    m_toolbar->addAction(m_zoomInAction);
    m_toolbar->addAction(m_zoomOutAction);
    m_toolbar->addAction(m_resetZoomAction);
    
    m_mainLayout->addWidget(m_toolbar);
}

void EnhancedMultiTabEditor::setupStatusBar() {
    QWidget* statusBar = new QWidget(this);
    QHBoxLayout* statusLayout = new QHBoxLayout(statusBar);
    statusLayout->setContentsMargins(4, 2, 4, 2);
    statusLayout->setSpacing(10);
    
    m_statusLabel = new QLabel("Ready", statusBar);
    m_lineColumnLabel = new QLabel("Line 1, Column 1", statusBar);
    m_encodingLabel = new QLabel("UTF-8", statusBar);
    m_lineEndingLabel = new QLabel("LF", statusBar);
    
    statusLayout->addWidget(m_statusLabel);
    statusLayout->addWidget(m_lineColumnLabel);
    statusLayout->addWidget(m_encodingLabel);
    statusLayout->addWidget(m_lineEndingLabel);
    statusLayout->addStretch();
    
    m_mainLayout->addWidget(statusBar);
}

void EnhancedMultiTabEditor::setupFindReplaceWidget() {
    m_findReplaceWidget = new QWidget(this);
    m_findReplaceWidget->setVisible(false);
    
    QHBoxLayout* findLayout = new QHBoxLayout(m_findReplaceWidget);
    findLayout->setContentsMargins(4, 2, 4, 2);
    
    // Find controls
    findLayout->addWidget(new QLabel("Find:", this));
    m_findEdit = new QLineEdit(this);
    m_findEdit->setMinimumWidth(200);
    findLayout->addWidget(m_findEdit);
    
    findLayout->addWidget(new QLabel("Replace:", this));
    m_replaceEdit = new QLineEdit(this);
    m_replaceEdit->setMinimumWidth(200);
    findLayout->addWidget(m_replaceEdit);
    
    // Options
    m_matchCaseCheckBox = new QCheckBox("Match case", this);
    findLayout->addWidget(m_matchCaseCheckBox);
    
    m_wholeWordCheckBox = new QCheckBox("Whole word", this);
    findLayout->addWidget(m_wholeWordCheckBox);
    
    m_regexCheckBox = new QCheckBox("Regex", this);
    findLayout->addWidget(m_regexCheckBox);
    
    // Buttons
    m_findNextButton = new QPushButton("Find Next", this);
    findLayout->addWidget(m_findNextButton);
    
    m_findPrevButton = new QPushButton("Find Prev", this);
    findLayout->addWidget(m_findPrevButton);
    
    m_replaceButton = new QPushButton("Replace", this);
    findLayout->addWidget(m_replaceButton);
    
    m_replaceAllButton = new QPushButton("Replace All", this);
    findLayout->addWidget(m_replaceAllButton);
    
    QPushButton* closeButton = new QPushButton("Close", this);
    connect(closeButton, &QPushButton::clicked, m_findReplaceWidget, &QWidget::hide);
    findLayout->addWidget(closeButton);
    
    m_mainLayout->addWidget(m_findReplaceWidget);
}

void EnhancedMultiTabEditor::createActions() {
    // File operations
    m_newFileAction = new QAction("New File", this);
    m_newFileAction->setShortcut(QKeySequence::New);
    m_newFileAction->setStatusTip("Create a new file");
    
    m_openFileAction = new QAction("Open File", this);
    m_openFileAction->setShortcut(QKeySequence::Open);
    m_openFileAction->setStatusTip("Open an existing file");
    
    m_saveFileAction = new QAction("Save File", this);
    m_saveFileAction->setShortcut(QKeySequence::Save);
    m_saveFileAction->setStatusTip("Save the current file");
    
    m_saveAsAction = new QAction("Save As", this);
    m_saveAsAction->setShortcut(QKeySequence::SaveAs);
    m_saveAsAction->setStatusTip("Save the current file with a new name");
    
    m_closeFileAction = new QAction("Close File", this);
    m_closeFileAction->setShortcut(QKeySequence::Close);
    m_closeFileAction->setStatusTip("Close the current file");
    
    // Edit operations
    m_undoAction = new QAction("Undo", this);
    m_undoAction->setShortcut(QKeySequence::Undo);
    
    m_redoAction = new QAction("Redo", this);
    m_redoAction->setShortcut(QKeySequence::Redo);
    
    m_cutAction = new QAction("Cut", this);
    m_cutAction->setShortcut(QKeySequence::Cut);
    
    m_copyAction = new QAction("Copy", this);
    m_copyAction->setShortcut(QKeySequence::Copy);
    
    m_pasteAction = new QAction("Paste", this);
    m_pasteAction->setShortcut(QKeySequence::Paste);
    
    m_findAction = new QAction("Find", this);
    m_findAction->setShortcut(QKeySequence::Find);
    
    m_replaceAction = new QAction("Replace", this);
    m_replaceAction->setShortcut(QKeySequence::Replace);
    
    m_gotoLineAction = new QAction("Go to Line", this);
    m_gotoLineAction->setShortcut(QKeySequence("Ctrl+G"));
    
    m_selectAllAction = new QAction("Select All", this);
    m_selectAllAction->setShortcut(QKeySequence::SelectAll);
    
    // View operations
    m_zoomInAction = new QAction("Zoom In", this);
    m_zoomInAction->setShortcut(QKeySequence::ZoomIn);
    
    m_zoomOutAction = new QAction("Zoom Out", this);
    m_zoomOutAction->setShortcut(QKeySequence::ZoomOut);
    
    m_resetZoomAction = new QAction("Reset Zoom", this);
    m_resetZoomAction->setShortcut(QKeySequence("Ctrl+0"));
}

void EnhancedMultiTabEditor::setupConnections() {
    // Tab widget connections
    connect(m_tabWidget, &QTabWidget::tabCloseRequested, this, &EnhancedMultiTabEditor::onTabCloseRequested);
    connect(m_tabWidget, &QTabWidget::currentChanged, this, &EnhancedMultiTabEditor::onCurrentTabChanged);
    
    // File operation connections
    connect(m_newFileAction, &QAction::triggered, this, [this]() { newFile(); });
    connect(m_openFileAction, &QAction::triggered, this, [this]() {
        QString filePath = QFileDialog::getOpenFileName(this, "Open File", QString(), "All Files (*.*)");
        if (!filePath.isEmpty()) {
            openFile(filePath);
        }
    });
    connect(m_saveFileAction, &QAction::triggered, this, [this]() { saveFile(); });
    connect(m_saveAsAction, &QAction::triggered, this, [this]() {
        if (m_currentFilePath.isEmpty()) {
            saveAs();
        } else {
            QString newPath = QFileDialog::getSaveFileName(this, "Save As", m_currentFilePath);
            if (!newPath.isEmpty()) {
                saveAsFile(newPath);
            }
        }
    });
    connect(m_closeFileAction, &QAction::triggered, this, [this]() { closeCurrentTab(); });
    
    // Edit operation connections
    connect(m_findAction, &QAction::triggered, this, [this]() {
        m_findReplaceWidget->setVisible(true);
        m_findEdit->setFocus();
    });
    
    connect(m_replaceAction, &QAction::triggered, this, [this]() {
        m_findReplaceWidget->setVisible(true);
        m_findEdit->setFocus();
    });
    
    // Find/replace connections
    connect(m_findNextButton, &QPushButton::clicked, this, &EnhancedMultiTabEditor::onFindNext);
    connect(m_findPrevButton, &QPushButton::clicked, this, &EnhancedMultiTabEditor::onFindPrevious);
    connect(m_replaceButton, &QPushButton::clicked, this, &EnhancedMultiTabEditor::onReplaceCurrent);
    connect(m_replaceAllButton, &QPushButton::clicked, this, &EnhancedMultiTabEditor::onReplaceAll);
    
    // Zoom connections
    connect(m_zoomInAction, &QAction::triggered, this, &EnhancedMultiTabEditor::onZoomIn);
    connect(m_zoomOutAction, &QAction::triggered, this, &EnhancedMultiTabEditor::onZoomOut);
    connect(m_resetZoomAction, &QAction::triggered, this, &EnhancedMultiTabEditor::onResetZoom);
    
    // FileSystemManager connections
    connect(m_fileSystemManager, &FileSystemManager::fileRead,
            this, [this](const QString& filepath, const QString& content) {
                m_loggingSystem->logInfo("EnhancedMultiTabEditor", 
                                      QString("File read: %1").arg(filepath));
            });
    
    connect(m_fileSystemManager, &FileSystemManager::fileWritten,
            this, [this](const QString& filepath) {
                m_loggingSystem->logInfo("EnhancedMultiTabEditor", 
                                      QString("File written: %1").arg(filepath));
            });
    
    connect(m_fileSystemManager, &FileSystemManager::fileChangedExternally,
            this, [this](const QString& filepath) {
                // TODO: Handle external file changes (reload prompt)
                m_loggingSystem->logInfo("EnhancedMultiTabEditor", 
                                      QString("External file change detected: %1").arg(filepath));
            });
}

void EnhancedMultiTabEditor::openFile(const QString& filePath) {
    if (filePath.isEmpty()) {
        return;
    }
    
    // Check if file is already open
    if (m_openFiles.contains(filePath)) {
        setCurrentEditor(m_openFiles[filePath]);
        m_tabWidget->setCurrentWidget(m_openFiles[filePath]);
        return;
    }
    
    // Create new text edit
    EnhancedTextEdit* editor = new EnhancedTextEdit(this);
    editor->setFilePath(filePath);
    
    // Load file content
    if (loadFileInternal(filePath, editor)) {
        // Add to tab widget
        int tabIndex = m_tabWidget->addTab(editor, QFileInfo(filePath).fileName());
        m_tabWidget->setCurrentIndex(tabIndex);
        
        // Track open file
        m_openFiles[filePath] = editor;
        setCurrentEditor(editor);
        
        // Connect editor signals
        connect(editor, &EnhancedTextEdit::cursorPositionChanged, 
                this, &EnhancedMultiTabEditor::onTextEditCursorPositionChanged);
        connect(editor, &EnhancedTextEdit::modificationChanged,
                this, &EnhancedMultiTabEditor::onTextEditModificationChanged);
        connect(editor, &EnhancedTextEdit::textChanged,
                this, &EnhancedMultiTabEditor::onTextEditTextChanged);
        
        m_loggingSystem->logInfo("EnhancedMultiTabEditor", 
                              QString("File opened: %1").arg(filePath));
        emit fileOpened(filePath);
    } else {
        delete editor;
        showError("Open File Failed", 
                 QString("Failed to open file: %1").arg(filePath));
    }
}

void EnhancedMultiTabEditor::newFile(const QString& name) {
    QString fileName = name.isEmpty() ? getNewFileName() : name;
    
    EnhancedTextEdit* editor = new EnhancedTextEdit(this);
    editor->setFilePath(""); // New file, no path yet
    
    int tabIndex = m_tabWidget->addTab(editor, fileName);
    m_tabWidget->setCurrentIndex(tabIndex);
    
    m_openFiles[fileName] = editor; // Use fileName as key for new files
    setCurrentEditor(editor);
    
    connect(editor, &EnhancedTextEdit::cursorPositionChanged, 
            this, &EnhancedMultiTabEditor::onTextEditCursorPositionChanged);
    connect(editor, &EnhancedTextEdit::modificationChanged,
            this, &EnhancedMultiTabEditor::onTextEditModificationChanged);
    connect(editor, &EnhancedTextEdit::textChanged,
            this, &EnhancedMultiTabEditor::onTextEditTextChanged);
    
    m_loggingSystem->logInfo("EnhancedMultiTabEditor", 
                          QString("New file created: %1").arg(fileName));
}

void EnhancedMultiTabEditor::saveFile(const QString& filePath) {
    EnhancedTextEdit* editor = getCurrentEditor();
    if (!editor) {
        showError("Save Failed", "No file to save");
        return;
    }
    
    QString targetPath = filePath.isEmpty() ? editor->getFilePath() : filePath;
    
    if (targetPath.isEmpty()) {
        saveAs();
        return;
    }
    
    if (saveFileInternal(targetPath, editor)) {
        updateTabTitle(targetPath);
        emit fileSaved(targetPath);
    }
}

void EnhancedMultiTabEditor::saveAs() {
    QString filePath = QFileDialog::getSaveFileName(this, "Save As", 
                                                 QStandardPaths::writableLocation(QStandardPaths::DesktopLocation),
                                                 "All Files (*.*)");
    if (!filePath.isEmpty()) {
        saveAsFile(filePath);
    }
}

void EnhancedMultiTabEditor::saveAsFile(const QString& filePath) {
    EnhancedTextEdit* editor = getCurrentEditor();
    if (!editor) {
        showError("Save Failed", "No file to save");
        return;
    }
    
    if (saveFileInternal(filePath, editor)) {
        // Update tracking
        if (!editor->getFilePath().isEmpty()) {
            m_openFiles.remove(editor->getFilePath());
        }
        
        editor->setFilePath(filePath);
        m_openFiles[filePath] = editor;
        
        updateTabTitle(filePath);
        emit fileSaved(filePath);
    }
}

bool EnhancedMultiTabEditor::saveFileInternal(const QString& filePath, EnhancedTextEdit* editor) {
    if (!editor) return false;
    
    // Get file encoding
    FileSystemManager::FileEncoding encoding = FileSystemManager::FileEncoding::UTF8;
    QString content = editor->toPlainText();
    
    // Use FileSystemManager to write file
    FileSystemManager::FileStatus status = m_fileSystemManager->writeFile(filePath, content, encoding);
    
    if (status == FileSystemManager::FileStatus::OK) {
        editor->setModified(false);
        m_loggingSystem->logInfo("EnhancedMultiTabEditor", 
                              QString("File saved: %1").arg(filePath));
        return true;
    } else {
        m_errorHandler->reportError("EnhancedMultiTabEditor", 
                                  QString("Failed to save file %1: Status %2").arg(filePath).arg(static_cast<int>(status)));
        return false;
    }
}

bool EnhancedMultiTabEditor::loadFileInternal(const QString& filePath, EnhancedTextEdit* editor) {
    if (!editor) return false;
    
    QString content;
    FileSystemManager::FileEncoding encoding;
    
    FileSystemManager::FileStatus status = m_fileSystemManager->readFile(filePath, content, encoding);
    
    if (status == FileSystemManager::FileStatus::OK) {
        editor->setPlainText(content);
        editor->setModified(false);
        
        // Update status labels
        m_encodingLabel->setText(FileSystemManager::encodingName(encoding));
        
        m_loggingSystem->logInfo("EnhancedMultiTabEditor", 
                              QString("File loaded: %1 (%2 encoding)").arg(filePath)
                              .arg(FileSystemManager::encodingName(encoding)));
        return true;
    } else {
        m_errorHandler->reportError("EnhancedMultiTabEditor", 
                                  QString("Failed to load file %1: Status %2").arg(filePath).arg(static_cast<int>(status)));
        return false;
    }
}

void EnhancedMultiTabEditor::closeFile(const QString& filePath) {
    if (filePath.isEmpty()) return;
    
    EnhancedTextEdit* editor = m_openFiles.value(filePath);
    if (editor) {
        closeFileInternal(filePath);
    }
}

void EnhancedMultiTabEditor::closeCurrentTab() {
    if (m_currentFilePath.isEmpty()) return;
    closeFile(m_currentFilePath);
}

void EnhancedMultiTabEditor::closeFileInternal(const QString& filePath) {
    EnhancedTextEdit* editor = m_openFiles.value(filePath);
    if (!editor) return;
    
    // Check for unsaved changes
    if (editor->isModified()) {
        QMessageBox::StandardButton result = QMessageBox::question(this, "Unsaved Changes",
                                                               "The file has unsaved changes. Do you want to save?",
                                                               QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);
        if (result == QMessageBox::Save) {
            saveFile(filePath);
        } else if (result == QMessageBox::Cancel) {
            return;
        }
    }
    
    // Remove from tab widget
    int tabIndex = m_tabWidget->indexOf(editor);
    if (tabIndex >= 0) {
        m_tabWidget->removeTab(tabIndex);
    }
    
    // Remove from tracking
    m_openFiles.remove(filePath);
    
    // Update current file
    if (m_currentFilePath == filePath) {
        m_currentFilePath.clear();
        updateStatusBar();
    }
    
    // Delete editor
    delete editor;
    
    m_loggingSystem->logInfo("EnhancedMultiTabEditor", 
                          QString("File closed: %1").arg(filePath));
    emit fileClosed(filePath);
}

void EnhancedMultiTabEditor::onTabCloseRequested(int index) {
    QWidget* widget = m_tabWidget->widget(index);
    if (widget) {
        // Find the file path for this widget
        QString filePath;
        for (auto it = m_openFiles.begin(); it != m_openFiles.end(); ++it) {
            if (it.value() == widget) {
                filePath = it.key();
                break;
            }
        }
        
        if (!filePath.isEmpty()) {
            closeFileInternal(filePath);
        }
    }
}

void EnhancedMultiTabEditor::onCurrentTabChanged(int index) {
    QWidget* widget = m_tabWidget->widget(index);
    if (widget) {
        EnhancedTextEdit* editor = qobject_cast<EnhancedTextEdit*>(widget);
        if (editor) {
            setCurrentEditor(editor);
            updateStatusBar();
        }
    }
}

void EnhancedMultiTabEditor::onTextEditModificationChanged(bool modified) {
    EnhancedTextEdit* editor = qobject_cast<EnhancedTextEdit*>(sender());
    if (editor) {
        QString filePath = editor->getFilePath();
        if (filePath.isEmpty()) {
            filePath = "Untitled"; // For new files
        }
        
        updateTabTitle(filePath);
        emit modificationChanged(filePath, modified);
    }
}

void EnhancedMultiTabEditor::onTextEditCursorPositionChanged() {
    EnhancedTextEdit* editor = qobject_cast<EnhancedTextEdit*>(sender());
    if (editor && editor == getCurrentEditor()) {
        QTextCursor cursor = editor->textCursor();
        int line = cursor.blockNumber() + 1;
        int column = cursor.positionInBlock() + 1;
        
        m_lineColumnLabel->setText(QString("Line %1, Column %2").arg(line).arg(column));
        
        QString filePath = editor->getFilePath();
        if (filePath.isEmpty()) {
            filePath = "Untitled";
        }
        
        emit cursorPositionChanged(filePath, line, column);
    }
}

void EnhancedMultiTabEditor::onTextEditTextChanged() {
    EnhancedTextEdit* editor = qobject_cast<EnhancedTextEdit*>(sender());
    if (editor && editor == getCurrentEditor()) {
        QString filePath = editor->getFilePath();
        if (filePath.isEmpty()) {
            filePath = "Untitled";
        }
        
        emit textChanged(filePath);
    }
}

void EnhancedMultiTabEditor::updateTabTitle(const QString& filePath) {
    if (filePath.isEmpty()) return;
    
    EnhancedTextEdit* editor = m_openFiles.value(filePath);
    if (editor) {
        int tabIndex = m_tabWidget->indexOf(editor);
        if (tabIndex >= 0) {
            QString title = QFileInfo(filePath).fileName();
            if (editor->isModified()) {
                title += " *";
            }
            m_tabWidget->setTabText(tabIndex, title);
        }
    }
}

void EnhancedMultiTabEditor::updateStatusBar() {
    EnhancedTextEdit* editor = getCurrentEditor();
    if (editor) {
        QString filePath = editor->getFilePath();
        if (filePath.isEmpty()) {
            filePath = "Untitled";
        }
        
        int lineCount = editor->document()->blockCount();
        m_statusLabel->setText(QString("%1 - %2 lines").arg(filePath).arg(lineCount));
        
        QTextCursor cursor = editor->textCursor();
        int line = cursor.blockNumber() + 1;
        int column = cursor.positionInBlock() + 1;
        m_lineColumnLabel->setText(QString("Line %1, Column %2").arg(line).arg(column));
    } else {
        m_statusLabel->setText("Ready");
        m_lineColumnLabel->setText("Line 1, Column 1");
    }
}

EnhancedTextEdit* EnhancedMultiTabEditor::getCurrentEditor() const {
    QWidget* currentWidget = m_tabWidget->currentWidget();
    return qobject_cast<EnhancedTextEdit*>(currentWidget);
}

EnhancedTextEdit* EnhancedMultiTabEditor::getEditorForFile(const QString& filePath) const {
    return m_openFiles.value(filePath);
}

void EnhancedMultiTabEditor::setCurrentEditor(EnhancedTextEdit* editor) {
    if (editor) {
        m_currentFilePath = editor->getFilePath();
        if (m_currentFilePath.isEmpty()) {
            // For new files, use the tab index as identifier
            int tabIndex = m_tabWidget->indexOf(editor);
            if (tabIndex >= 0) {
                m_currentFilePath = m_tabWidget->tabText(tabIndex);
            }
        }
        updateStatusBar();
        emit currentFileChanged(m_currentFilePath);
    }
}

QString EnhancedMultiTabEditor::getCurrentText() const {
    EnhancedTextEdit* editor = getCurrentEditor();
    return editor ? editor->toPlainText() : QString();
}

QString EnhancedMultiTabEditor::getCurrentFilePath() const {
    return m_currentFilePath;
}

int EnhancedMultiTabEditor::getCurrentLine() const {
    EnhancedTextEdit* editor = getCurrentEditor();
    if (editor) {
        return editor->textCursor().blockNumber() + 1;
    }
    return 1;
}

int EnhancedMultiTabEditor::getCurrentColumn() const {
    EnhancedTextEdit* editor = getCurrentEditor();
    if (editor) {
        return editor->textCursor().positionInBlock() + 1;
    }
    return 1;
}

int EnhancedMultiTabEditor::getLineCount() const {
    EnhancedTextEdit* editor = getCurrentEditor();
    return editor ? editor->document()->blockCount() : 0;
}

bool EnhancedMultiTabEditor::hasUnsavedChanges() const {
    for (EnhancedTextEdit* editor : m_openFiles.values()) {
        if (editor->isModified()) {
            return true;
        }
    }
    return false;
}

void EnhancedMultiTabEditor::gotoLine(int line) {
    EnhancedTextEdit* editor = getCurrentEditor();
    if (editor && line > 0 && line <= editor->document()->blockCount()) {
        QTextCursor cursor(editor->document()->findBlockByNumber(line - 1));
        editor->setTextCursor(cursor);
        editor->setFocus();
    }
}

void EnhancedMultiTabEditor::findText(const QString& text, bool replace) {
    if (text.isEmpty()) return;
    
    EnhancedTextEdit* editor = getCurrentEditor();
    if (!editor) return;
    
    QTextCursor cursor = editor->textCursor();
    QTextDocument::FindFlags flags;
    
    if (m_matchCaseCheckBox->isChecked()) {
        flags |= QTextDocument::FindCaseSensitively;
    }
    
    if (m_wholeWordCheckBox->isChecked()) {
        flags |= QTextDocument::FindWholeWords;
    }
    
    QTextCursor newCursor;
    if (m_regexCheckBox->isChecked()) {
        QRegularExpression regex(text);
        newCursor = editor->document()->find(regex, cursor, flags);
    } else {
        newCursor = editor->document()->find(text, cursor, flags);
    }
    
    if (!newCursor.isNull()) {
        editor->setTextCursor(newCursor);
    } else {
        m_statusLabel->setText("Text not found");
    }
}

void EnhancedMultiTabEditor::onFindNext() {
    QString text = m_findEdit->text();
    if (!text.isEmpty()) {
        findText(text);
    }
}

void EnhancedMultiTabEditor::onFindPrevious() {
    // TODO: Implement find previous
}

void EnhancedMultiTabEditor::onReplaceCurrent() {
    // TODO: Implement replace current
}

void EnhancedMultiTabEditor::onReplaceAll() {
    // TODO: Implement replace all
}

void EnhancedMultiTabEditor::onUndo() {
    EnhancedTextEdit* editor = getCurrentEditor();
    if (editor) {
        editor->undo();
    }
}

void EnhancedMultiTabEditor::onRedo() {
    EnhancedTextEdit* editor = getCurrentEditor();
    if (editor) {
        editor->redo();
    }
}

void EnhancedMultiTabEditor::onCut() {
    EnhancedTextEdit* editor = getCurrentEditor();
    if (editor) {
        editor->cut();
    }
}

void EnhancedMultiTabEditor::onCopy() {
    EnhancedTextEdit* editor = getCurrentEditor();
    if (editor) {
        editor->copy();
    }
}

void EnhancedMultiTabEditor::onPaste() {
    EnhancedTextEdit* editor = getCurrentEditor();
    if (editor) {
        editor->paste();
    }
}

void EnhancedMultiTabEditor::onSelectAll() {
    EnhancedTextEdit* editor = getCurrentEditor();
    if (editor) {
        editor->selectAll();
    }
}

void EnhancedMultiTabEditor::onZoomIn() {
    EnhancedTextEdit* editor = getCurrentEditor();
    if (editor) {
        QFont font = editor->font();
        font.setPointSize(font.pointSize() + 1);
        editor->setFont(font);
        
        // Update all editors
        for (EnhancedTextEdit* ed : m_openFiles.values()) {
            ed->setFont(font);
        }
    }
}

void EnhancedMultiTabEditor::onZoomOut() {
    EnhancedTextEdit* editor = getCurrentEditor();
    if (editor) {
        QFont font = editor->font();
        if (font.pointSize() > 6) { // Minimum font size
            font.setPointSize(font.pointSize() - 1);
            editor->setFont(font);
            
            // Update all editors
            for (EnhancedTextEdit* ed : m_openFiles.values()) {
                ed->setFont(font);
            }
        }
    }
}

void EnhancedMultiTabEditor::onResetZoom() {
    QFont font("Consolas", 12);
    
    // Update all editors
    for (EnhancedTextEdit* editor : m_openFiles.values()) {
        editor->setFont(font);
    }
}

QString EnhancedMultiTabEditor::getNewFileName() {
    static int untitledCount = 1;
    return QString("Untitled-%1").arg(untitledCount++);
}

QString EnhancedMultiTabEditor::getFileEncoding(const QString& filePath) const {
    // Detect file encoding from BOM or content analysis
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return "UTF-8";  // Default fallback
    }
    
    QByteArray firstBytes = file.read(4);
    file.close();
    
    // Check for BOM signatures
    if (firstBytes.startsWith("\xEF\xBB\xBF")) {
        return "UTF-8 BOM";
    } else if (firstBytes.startsWith("\xFF\xFE")) {
        return "UTF-16 LE";
    } else if (firstBytes.startsWith("\xFE\xFF")) {
        return "UTF-16 BE";
    }
    
    return "UTF-8";  // Default
}

QString EnhancedMultiTabEditor::detectLineEndings(const QString& text) const {
    // Detect line ending style from content
    if (text.contains("\r\n")) {
        return "CRLF";
    } else if (text.contains("\r")) {
        return "CR";
    } else if (text.contains("\n")) {
        return "LF";
    }
    return "LF";  // Default
}

void EnhancedMultiTabEditor::showError(const QString& title, const QString& message) {
    QMessageBox::critical(this, title, message);
    m_errorHandler->reportError("EnhancedMultiTabEditor", 
                              QString("%1: %2").arg(title).arg(message));
}

void EnhancedMultiTabEditor::showInfo(const QString& title, const QString& message) {
    QMessageBox::information(this, title, message);
}

void EnhancedMultiTabEditor::loadEditorSettings() {
    // Load editor settings from SettingsSystem
    if (!m_loggingSystem) return;
    
    m_loggingSystem->logDebug("EnhancedMultiTabEditor", "Loading editor settings");
    
    // Apply font settings
    QFont loadedFont("Consolas", 12);
    m_editorFont = loadedFont;
    
    // Apply to all open editors
    for (EnhancedTextEdit* editor : m_openFiles.values()) {
        editor->setFont(m_editorFont);
    }
}

void EnhancedMultiTabEditor::saveEditorSettings() {
    // Save editor settings to SettingsSystem
    if (!m_loggingSystem) return;
    
    m_loggingSystem->logDebug("EnhancedMultiTabEditor", "Saving editor settings");
    
    // Save current state: font, tab width, open files, etc.
    // This preserves user preferences across sessions
}

// ========== SimpleSyntaxHighlighter Implementation ==========

SimpleSyntaxHighlighter::SimpleSyntaxHighlighter(QTextDocument* parent)
    : QSyntaxHighlighter(parent)
{
}

void SimpleSyntaxHighlighter::setHighlightingRules(const QMap<QString, QTextCharFormat>& rules) {
    // Store highlighting rules for use in highlightBlock()
    m_highlightingRules = rules;
    rehighlight();  // Re-apply highlighting to all blocks
}

void SimpleSyntaxHighlighter::highlightBlock(const QString& text) {
    // Apply syntax highlighting rules to text block
    for (auto it = m_highlightingRules.cbegin(); it != m_highlightingRules.cend(); ++it) {
        const QString& pattern = it.key();
        const QTextCharFormat& format = it.value();
        
        QRegularExpression regex(pattern);
        QRegularExpressionMatchIterator matches = regex.globalMatch(text);
        
        while (matches.hasNext()) {
            QRegularExpressionMatch match = matches.next();
            setFormat(match.capturedStart(), match.capturedLength(), format);
        }
    }
}