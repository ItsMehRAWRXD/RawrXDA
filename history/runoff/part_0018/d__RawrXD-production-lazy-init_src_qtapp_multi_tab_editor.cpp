/**
 * @file multi_tab_editor.cpp
 * @brief Implementation of multi-tab code editor with minimap support
 *
 * Provides tab-based code editing with language support, LSP integration,
 * and optional minimap visualization.
 *
 * @author RawrXD Team
 * @version 1.0.0
 */

#include "multi_tab_editor.h"

#include <QTabWidget>
#include <QFile>
#include <QTextEdit>
#include <QTextDocument>
#include <QTextBlock>
#include <QTextCursor>
#include <QInputDialog>
#include <QVBoxLayout>
#include <QMessageBox>
#include <QDebug>
#include <QFileInfo>

#include "editor_with_minimap.h"
#include "code_minimap.h"
#include "lsp_client.h"

MultiTabEditor::MultiTabEditor(QWidget* parent)
    : QWidget(parent)
    , tab_widget_(nullptr)
    , m_lspClient(nullptr)
    , m_minimapEnabled(true)
{
    // Lightweight constructor - defer creation
    setWindowTitle("RawrXD Code Editor");
}

void MultiTabEditor::initialize()
{
    if (tab_widget_) {
        return;  // Already initialized
    }

    QVBoxLayout* layout = new QVBoxLayout(this);
    
    // Create tab widget
    tab_widget_ = new QTabWidget(this);
    tab_widget_->setTabsClosable(true);
    tab_widget_->setMovable(true);
    
    layout->addWidget(tab_widget_);
    setLayout(layout);

    qDebug() << "[MultiTabEditor] Initialized with tab support";
}

/**
 * @brief Open a file in a new tab
 */
void MultiTabEditor::openFile(const QString& filepath)
{
    if (!tab_widget_) {
        initialize();
    }

    QFileInfo fileInfo(filepath);
    if (!fileInfo.exists()) {
        QMessageBox::warning(this, "Error", "File does not exist: " + filepath);
        return;
    }

    // Check if already open
    for (int i = 0; i < tab_widget_->count(); ++i) {
        if (tab_file_paths_.value(tab_widget_->widget(i)) == filepath) {
            tab_widget_->setCurrentIndex(i);
            qDebug() << "[MultiTabEditor] File already open:" << filepath;
            return;
        }
    }

    // Read file content
    QFile file(filepath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::warning(this, "Error", "Cannot open file: " + file.errorString());
        return;
    }

    QString content = QString::fromUtf8(file.readAll());
    file.close();

    // Create editor widget
    RawrXD::EditorWithMinimap* editor = new RawrXD::EditorWithMinimap(this);
    editor->editor()->setPlainText(content);
    editor->setMinimapEnabled(m_minimapEnabled);

    // Connect LSP if available
    if (m_lspClient) {
        m_lspClient->openDocument(filepath, "plaintext", content);
    }

    // Add to tab widget
    int index = tab_widget_->addTab(editor, fileInfo.fileName());
    tab_widget_->setCurrentIndex(index);
    tab_file_paths_[editor] = filepath;

    qDebug() << "[MultiTabEditor] Opened file:" << filepath;
}

/**
 * @brief Create a new empty file tab
 */
void MultiTabEditor::newFile()
{
    if (!tab_widget_) {
        initialize();
    }

    RawrXD::EditorWithMinimap* editor = new RawrXD::EditorWithMinimap(this);
    editor->setMinimapEnabled(m_minimapEnabled);

    int index = tab_widget_->addTab(editor, "Untitled");
    tab_widget_->setCurrentIndex(index);
    tab_file_paths_[editor] = "";

    qDebug() << "[MultiTabEditor] Created new untitled file";
}

/**
 * @brief Save the current file
 */
void MultiTabEditor::saveCurrentFile()
{
    QWidget* current = tab_widget_->currentWidget();
    if (!current) {
        return;
    }

    QString filepath = tab_file_paths_.value(current);
    if (filepath.isEmpty()) {
        qWarning() << "[MultiTabEditor] Cannot save untitled file";
        return;
    }

    RawrXD::EditorWithMinimap* editor = qobject_cast<RawrXD::EditorWithMinimap*>(current);
    if (!editor) {
        return;
    }

    QString content = editor->editor()->toPlainText();
    
    QFile file(filepath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, "Error", "Cannot save file: " + file.errorString());
        return;
    }

    file.write(content.toUtf8());
    file.close();

    qDebug() << "[MultiTabEditor] Saved file:" << filepath;
}

/**
 * @brief Undo last action in current editor
 */
void MultiTabEditor::undo()
{
    RawrXD::EditorWithMinimap* editor = qobject_cast<RawrXD::EditorWithMinimap*>(tab_widget_->currentWidget());
    if (editor) {
        editor->editor()->undo();
    }
}

/**
 * @brief Redo last undone action in current editor
 */
void MultiTabEditor::redo()
{
    RawrXD::EditorWithMinimap* editor = qobject_cast<RawrXD::EditorWithMinimap*>(tab_widget_->currentWidget());
    if (editor) {
        editor->editor()->redo();
    }
}

/**
 * @brief Open find dialog in current editor
 */
void MultiTabEditor::find()
{
    RawrXD::EditorWithMinimap* editor = qobject_cast<RawrXD::EditorWithMinimap*>(tab_widget_->currentWidget());
    if (editor) {
        editor->editor()->find("searchText", QTextDocument::FindCaseSensitively);
    }
}

/**
 * @brief Open find/replace dialog in current editor
 */
void MultiTabEditor::replace()
{
    RawrXD::EditorWithMinimap* editor = qobject_cast<RawrXD::EditorWithMinimap*>(tab_widget_->currentWidget());
    if (editor) {
        bool findOk = false;
        QString findText = QInputDialog::getText(this, "Find", "Find text:", QLineEdit::Normal, QString(), &findOk);
        if (!findOk || findText.isEmpty()) {
            return;
        }

        bool replaceOk = false;
        QString replaceText = QInputDialog::getText(this, "Replace", "Replace with:", QLineEdit::Normal, QString(), &replaceOk);
        if (!replaceOk) {
            return;
        }

        QPlainTextEdit* textEditor = editor->editor();
        if (!textEditor) {
            return;
        }

        textEditor->moveCursor(QTextCursor::Start);
        while (textEditor->find(findText)) {
            QTextCursor cursor = textEditor->textCursor();
            cursor.insertText(replaceText);
        }
    }
}

/**
 * @brief Get the text content of the current editor tab
 * @return Plain text content, empty string if no tab open
 */
QString MultiTabEditor::getCurrentText() const
{
    if (!tab_widget_) {
        return "";
    }

    RawrXD::EditorWithMinimap* editor = qobject_cast<RawrXD::EditorWithMinimap*>(tab_widget_->currentWidget());
    if (!editor) {
        return "";
    }

    return editor->editor()->toPlainText();
}

/**
 * @brief Get the selected text in the current editor
 * @return Selected text, empty string if nothing selected
 */
QString MultiTabEditor::getSelectedText() const
{
    if (!tab_widget_) {
        return "";
    }

    RawrXD::EditorWithMinimap* editor = qobject_cast<RawrXD::EditorWithMinimap*>(tab_widget_->currentWidget());
    if (!editor) {
        return "";
    }

    return editor->editor()->textCursor().selectedText();
}

/**
 * @brief Get the file path of the current tab
 * @return File path, empty string if untitled
 */
QString MultiTabEditor::getCurrentFilePath() const
{
    if (!tab_widget_) {
        return "";
    }

    QWidget* current = tab_widget_->currentWidget();
    if (!current) {
        return "";
    }

    return tab_file_paths_.value(current, "");
}

/**
 * @brief Set the LSP client for language server support
 */
void MultiTabEditor::setLSPClient(RawrXD::LSPClient* client)
{
    m_lspClient = client;
    qDebug() << "[MultiTabEditor] LSP client set";
}

/**
 * @brief Get the current editor widget
 */
RawrXD::AgenticTextEdit* MultiTabEditor::getCurrentEditor() const
{
    if (!tab_widget_) {
        return nullptr;
    }

    // EditorWithMinimap wraps AgenticTextEdit, need to extract it
    RawrXD::EditorWithMinimap* editor = qobject_cast<RawrXD::EditorWithMinimap*>(tab_widget_->currentWidget());
    if (!editor) {
        return nullptr;
    }

    // Assuming AgenticTextEdit is the underlying editor type
    // This would need to be cast appropriately based on the actual implementation
    return nullptr; // Placeholder - need to check actual type
}

/**
 * @brief Enable/disable minimap on all tabs
 */
void MultiTabEditor::setMinimapEnabled(bool enabled)
{
    m_minimapEnabled = enabled;

    if (!tab_widget_) {
        return;
    }

    for (int i = 0; i < tab_widget_->count(); ++i) {
        RawrXD::EditorWithMinimap* editor = qobject_cast<RawrXD::EditorWithMinimap*>(tab_widget_->widget(i));
        if (editor) {
            editor->setMinimapEnabled(enabled);
        }
    }

    qDebug() << "[MultiTabEditor] Minimap" << (enabled ? "enabled" : "disabled");
}

/**
 * @brief Get the number of open tabs
 */
int MultiTabEditor::getTabCount() const
{
    return tab_widget_ ? tab_widget_->count() : 0;
}

/**
 * @brief Get the line count of the current editor
 */
int MultiTabEditor::getLineCount() const
{
    RawrXD::EditorWithMinimap* editor = qobject_cast<RawrXD::EditorWithMinimap*>(
        tab_widget_ ? tab_widget_->currentWidget() : nullptr
    );
    if (!editor) {
        return 0;
    }

    return editor->editor()->document()->blockCount();
}

/**
 * @brief Get a specific line from the current editor
 * @param lineNumber 0-based line number
 */
QString MultiTabEditor::getLine(int lineNumber) const
{
    RawrXD::EditorWithMinimap* editor = qobject_cast<RawrXD::EditorWithMinimap*>(
        tab_widget_ ? tab_widget_->currentWidget() : nullptr
    );
    if (!editor) {
        return "";
    }

    QTextDocument* doc = editor->editor()->document();
    if (lineNumber < 0 || lineNumber >= doc->blockCount()) {
        return "";
    }

    return doc->findBlockByNumber(lineNumber).text();
}

/**
 * @brief Set the text content of the current editor
 */
void MultiTabEditor::setText(const QString& text)
{
    RawrXD::EditorWithMinimap* editor = qobject_cast<RawrXD::EditorWithMinimap*>(
        tab_widget_ ? tab_widget_->currentWidget() : nullptr
    );
    if (editor) {
        editor->editor()->setPlainText(text);
    }
}

/**
 * @brief Close a specific tab by index
 */
void MultiTabEditor::closeTab(int index)
{
    if (!tab_widget_ || index < 0 || index >= tab_widget_->count()) {
        return;
    }

    QWidget* widget = tab_widget_->widget(index);
    tab_file_paths_.remove(widget);
    tab_widget_->removeTab(index);
    
    qDebug() << "[MultiTabEditor] Closed tab at index" << index;
}

/**
 * @brief Close all tabs
 */
void MultiTabEditor::closeAllTabs()
{
    if (!tab_widget_) {
        return;
    }

    tab_widget_->clear();
    tab_file_paths_.clear();
    
    qDebug() << "[MultiTabEditor] Closed all tabs";
}

/**
 * @brief Get the index of the tab with a given file path
 */
int MultiTabEditor::getTabIndexByPath(const QString& filepath) const
{
    if (!tab_widget_) {
        return -1;
    }

    for (int i = 0; i < tab_widget_->count(); ++i) {
        if (tab_file_paths_.value(tab_widget_->widget(i)) == filepath) {
            return i;
        }
    }

    return -1;
}

/**
 * @brief Switch to the tab with a given file path
 */
void MultiTabEditor::switchToTab(const QString& filepath)
{
    int index = getTabIndexByPath(filepath);
    if (index >= 0 && tab_widget_) {
        tab_widget_->setCurrentIndex(index);
    }
}
