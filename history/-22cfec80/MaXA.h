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
#include <QMap>
#include <QPushButton>
#include <QFont>

// Core system forward declarations
class FileSystemManager;
class CommandDispatcher;
class LoggingSystem;
class ErrorHandler;

// Forward declarations
class EnhancedTextEdit;

class EnhancedTextEdit : public QPlainTextEdit {
    Q_OBJECT

public:
    explicit EnhancedTextEdit(QWidget* parent = nullptr);
    void setFilePath(const QString& filePath);
    QString getFilePath() const { return m_filePath; }

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
    bool openFile(const QString& filePath);
    bool saveFile(const QString& filePath = QString());
    bool closeFile(const QString& filePath);
    bool newFile();

    // Editor operations
    void undo();
    void redo();
    void cut();
    void copy();
    void paste();
    void selectAll();
    void find();
    void replace();
    void gotoLine(int line);

    // UI operations
    void showLineNumbers(bool show);
    void showWhitespace(bool show);
    void setTheme(const QString& theme);
    void setFont(const QFont& font);
    void setFontSize(int size);

signals:
    void fileOpened(const QString& filePath);
    void fileSaved(const QString& filePath);
    void fileClosed(const QString& filePath);
    void modificationChanged(bool modified);
    void cursorPositionChanged(int line, int column);
    void findRequested();
    void replaceRequested();

public slots:
    void onFindNext();
    void onFindPrevious();
    void onReplace();
    void onReplaceAll();

private slots:
    void onTabChanged(int index);
    void onTabCloseRequested(int index);
    void onTextChanged();
    void onCursorPositionChanged();
    void onModificationChanged(bool modified);

private:
    void setupUI();
    void setupConnections();
    void setupToolbar();
    void setupFindReplace();
    void updateStatusBar();
    EnhancedTextEdit* getCurrentEditor();
    EnhancedTextEdit* getEditor(const QString& filePath);
    int getTabIndex(const QString& filePath);
    void closeTab(int index);

    // Core systems
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
    QAction* m_zoomInAction;
    QAction* m_zoomOutAction;
    QAction* m_resetZoomAction;

    // File action menu items
    QAction* m_newFileAction;
    QAction* m_openFileAction;
    QAction* m_saveFileAction;
    QAction* m_saveAsAction;
    QAction* m_closeFileAction;

    // Edit action menu items
    QAction* m_undoAction;
    QAction* m_redoAction;
    QAction* m_cutAction;
    QAction* m_copyAction;
    QAction* m_pasteAction;

    // Search/Navigation action menu items
    QAction* m_findAction;
    QAction* m_replaceAction;
    QAction* m_gotoLineAction;
    QAction* m_selectAllAction;

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
    QString m_currentFilePath;
    bool m_showLineNumbers;
    bool m_showWhitespace;
    QString m_currentTheme;
    QFont m_currentFont;
    int m_fontSize;
};

