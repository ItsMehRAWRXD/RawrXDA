/**
 * \file enhanced_multi_tab_editor.h
 * \brief Enhanced Multi-Tab Editor with Phase 1 Foundation Integration
 * \author RawrXD Team
 * \date 2026-01-13
 * 
 * Enhanced MultiTabEditor that integrates with:
 * - FileSystemManager for file I/O operations
 * - CommandDispatcher for command routing
 * - LoggingSystem for operation logging
 * - ErrorHandler for error management
 */

#pragma once

#include <QWidget>
#include <QTabWidget>
#include <QTextEdit>
#include <QPlainTextEdit>
#include <QToolBar>
#include <QAction>
#include <QLineEdit>
#include <QLabel>
#include <QComboBox>
#include <QSpinBox>
#include <QCheckBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSplitter>
#include <QMenu>
#include <QMenuBar>
#include <QStatusBar>
#include <QTimer>
#include <QScrollBar>
#include <QSyntaxHighlighter>
#include <QRegularExpression>
#include <QColor>
#include <QFont>

// Forward declarations
class FileSystemManager;
class CommandDispatcher;
class LoggingSystem;
class ErrorHandler;
class CodeMinimap;

class EnhancedTextEdit : public QPlainTextEdit {
    Q_OBJECT

public:
    explicit EnhancedTextEdit(QWidget* parent = nullptr);
    void setFilePath(const QString& filePath);
    QString getFilePath() const { return m_filePath; }
    bool isModified() const { return document()->isModified(); }
    void setModified(bool modified) { document()->setModified(modified); }

signals:
    void cursorPositionChanged(int line, int column);
    void modificationChanged(bool modified);
    void textChanged();
    void saveRequested(const QString& filePath);

private:
    QString m_filePath;
    void setupConnections();
};

class EnhancedMultiTabEditor : public QWidget {
    Q_OBJECT

public:
    explicit EnhancedMultiTabEditor(QWidget* parent = nullptr);
    ~EnhancedMultiTabEditor();

    // Two-phase initialization
    void initialize();

    // File operations
    void openFile(const QString& filePath);
    void newFile(const QString& name = QString());
    void saveFile(const QString& filePath = QString());
    void saveAsFile(const QString& filePath);
    void closeFile(const QString& filePath);
    void closeCurrentTab();

    // View operations
    void setShowLineNumbers(bool show);
    void setShowWhitespace(bool show);
    void setFontSize(int size);
    void setFontFamily(const QString& family);
    void setTabWidth(int width);
    void setAutoIndent(bool enabled);

    // Navigation
    void gotoLine(int line);
    void findText(const QString& text, bool replace = false);
    void findNext();
    void findPrevious();

    // Getters
    QString getCurrentText() const;
    QString getCurrentFilePath() const;
    int getCurrentLine() const;
    int getCurrentColumn() const;
    int getLineCount() const;
    bool hasUnsavedChanges() const;

signals:
    void fileOpened(const QString& filePath);
    void fileClosed(const QString& filePath);
    void fileSaved(const QString& filePath);
    void currentFileChanged(const QString& filePath);
    void textChanged(const QString& filePath);
    void cursorPositionChanged(const QString& filePath, int line, int column);
    void modificationChanged(const QString& filePath, bool modified);

public slots:
    void onTabCloseRequested(int index);
    void onCurrentTabChanged(int index);
    void onTextEditModificationChanged(bool modified);
    void onTextEditCursorPositionChanged();
    void onTextEditTextChanged();
    void onFindReplaceDialog();
    void onFindNext();
    void onFindPrevious();
    void onReplaceCurrent();
    void onReplaceAll();
    void onUndo();
    void onRedo();
    void onCut();
    void onCopy();
    void onPaste();
    void onSelectAll();
    void onZoomIn();
    void onZoomOut();
    void onResetZoom();

private:
    // Foundation system integration
    FileSystemManager* m_fileSystemManager;
    CommandDispatcher* m_commandDispatcher;
    LoggingSystem* m_loggingSystem;
    ErrorHandler* m_errorHandler;

    // UI Components
    QVBoxLayout* m_mainLayout;
    QHBoxLayout* m_toolbarLayout;
    QTabWidget* m_tabWidget;
    QToolBar* m_toolbar;
    QLabel* m_statusLabel;
    QLabel* m_lineColumnLabel;
    QLabel* m_encodingLabel;
    QLabel* m_lineEndingLabel;
    
    // Actions
    QAction* m_newFileAction;
    QAction* m_openFileAction;
    QAction* m_saveFileAction;
    QAction* m_saveAsAction;
    QAction* m_closeFileAction;
    QAction* m_undoAction;
    QAction* m_redoAction;
    QAction* m_cutAction;
    QAction* m_copyAction;
    QAction* m_pasteAction;
    QAction* m_findAction;
    QAction* m_replaceAction;
    QAction* m_gotoLineAction;
    QAction* m_selectAllAction;
    QAction* m_zoomInAction;
    QAction* m_zoomOutAction;
    QAction* m_resetZoomAction;

    // Find/Replace dialog
    QWidget* m_findReplaceWidget;
    QLineEdit* m_findEdit;
    QLineEdit* m_replaceEdit;
    QCheckBox* m_matchCaseCheckBox;
    QCheckBox* m_wholeWordCheckBox;
    QCheckBox* m_regexCheckBox;
    QPushButton* m_findNextButton;
    QPushButton* m_findPrevButton;
    QPushButton* m_replaceButton;
    QPushButton* m_replaceAllButton;

    // State
    QMap<QString, EnhancedTextEdit*> m_openFiles;
    QString m_currentFilePath;
    bool m_showLineNumbers;
    bool m_showWhitespace;
    QFont m_editorFont;
    int m_tabWidth;
    bool m_autoIndent;

    // Private methods
    void setupUI();
    void setupToolBar();
    void setupStatusBar();
    void setupFindReplaceWidget();
    void setupConnections();
    void createActions();
    void updateTabTitle(const QString& filePath);
    void updateStatusBar();
    void updateEditorSettings(EnhancedTextEdit* editor);
    void loadEditorSettings();
    void saveEditorSettings();

    // File operations
    QString getNewFileName();
    bool saveFileInternal(const QString& filePath, EnhancedTextEdit* editor);
    bool loadFileInternal(const QString& filePath, EnhancedTextEdit* editor);
    void closeFileInternal(const QString& filePath);

    // Editor management
    EnhancedTextEdit* getCurrentEditor() const;
    EnhancedTextEdit* getEditorForFile(const QString& filePath) const;
    void setCurrentEditor(EnhancedTextEdit* editor);
    void removeEditor(const QString& filePath);

    // Utility methods
    QString getFileEncoding(const QString& filePath) const;
    QString detectLineEndings(const QString& text) const;
    void showError(const QString& title, const QString& message);
    void showInfo(const QString& title, const QString& message);
};

class SimpleSyntaxHighlighter : public QSyntaxHighlighter {
    Q_OBJECT

public:
    explicit SimpleSyntaxHighlighter(QTextDocument* parent = nullptr);
    void setHighlightingRules(const QMap<QString, QTextCharFormat>& rules);

protected:
    void highlightBlock(const QString& text) override;

private:
    QMap<QRegularExpression, QTextCharFormat> m_highlightingRules;
};