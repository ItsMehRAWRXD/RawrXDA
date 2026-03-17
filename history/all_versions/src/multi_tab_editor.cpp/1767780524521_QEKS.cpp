// Multi-Tab Editor
#include "multi_tab_editor.h"
#include "agentic_text_edit.h"
#include "lsp_client.h"
#include "ai_completion_provider.h"
#include <QVBoxLayout>
#include <QTabWidget>
#include <QTextEdit>
#include <QFile>
#include <QTextStream>
#include <QFileDialog>
#include <QInputDialog>
#include <QMessageBox>
#include <QDebug>

using namespace RawrXD;

// Lightweight constructor - no widget creation
MultiTabEditor::MultiTabEditor(QWidget* parent) : QWidget(parent), tab_widget_(nullptr) {
    // Deferred to initialize() - safe to call before QApplication
}

// Two-phase init: Create Qt widgets after QApplication is running
void MultiTabEditor::initialize() {
    if (tab_widget_) return;  // Already initialized
    
    QVBoxLayout* layout = new QVBoxLayout(this);
    tab_widget_ = new QTabWidget(this);
    tab_widget_->setTabsClosable(true); // Enable tab closing
    layout->addWidget(tab_widget_);
    
    // Connect tab close signal
    connect(tab_widget_, QOverload<int>::of(&QTabWidget::tabCloseRequested),
            this, [this](int index) { 
                tab_file_paths_.remove(tab_widget_->widget(index));
                tab_widget_->removeTab(index); 
            });
    
    // Create initial empty tab
    newFile();
}

void MultiTabEditor::openFile(const QString& filepath) {
    QFile file(filepath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::warning(this, "Error", "Could not open file: " + filepath);
        return;
    }
    
    AgenticTextEdit* editor = new AgenticTextEdit(this);
    editor->initialize();
    
    QTextStream stream(&file);
    editor->setPlainText(stream.readAll());
    file.close();
    
    // Configure LSP
    editor->setDocumentUri(filepath);
    if (m_lspClient) {
        editor->setLSPClient(m_lspClient);
    }
    
    // Wire AI completion provider
    if (m_aiProvider) {
        editor->setAICompletionProvider(m_aiProvider);
    }
    
    QString filename = filepath.section('/', -1);
    tab_widget_->addTab(editor, filename);
    tab_widget_->setCurrentWidget(editor);
    
    // Store the full file path
    tab_file_paths_[editor] = filepath;
    
    qDebug() << "Opened file:" << filepath;
}

void MultiTabEditor::newFile() {
    AgenticTextEdit* editor = new AgenticTextEdit(this);
    editor->initialize();
    editor->setPlainText("// New file\n// Start coding here...");
    
    static int newFileCount = 1;
    QString tabName = "Untitled-" + QString::number(newFileCount++);
    QString tempUri = QString("untitled://%1.cpp").arg(newFileCount - 1);
    
    editor->setDocumentUri(tempUri);
    if (m_lspClient) {
        editor->setLSPClient(m_lspClient);
    }
    
    // Wire AI completion provider
    if (m_aiProvider) {
        editor->setAICompletionProvider(m_aiProvider);
    }
    
    tab_widget_->addTab(editor, tabName);
    tab_widget_->setCurrentWidget(editor);
}

void MultiTabEditor::saveCurrentFile() {
    AgenticTextEdit* currentEditor = qobject_cast<AgenticTextEdit*>(tab_widget_->currentWidget());
    if (!currentEditor) {
        QMessageBox::warning(this, "Error", "No file to save");
        return;
    }
    
    QString filePath = QFileDialog::getSaveFileName(this, "Save File");
    if (!filePath.isEmpty()) {
        QFile file(filePath);
        if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream stream(&file);
            stream << currentEditor->toPlainText();
            file.close();
            
            // Update tab name and store full path
            QString fileName = filePath.section('/', -1);
            tab_widget_->setTabText(tab_widget_->currentIndex(), fileName);
            tab_file_paths_[currentEditor] = filePath;
            
            QMessageBox::information(this, "Success", "File saved successfully");
        } else {
            QMessageBox::warning(this, "Error", "Could not save file");
        }
    }
}

void MultiTabEditor::undo() {
    AgenticTextEdit* currentEditor = qobject_cast<AgenticTextEdit*>(tab_widget_->currentWidget());
    if (currentEditor) {
        currentEditor->undo();
    }
}

void MultiTabEditor::redo() {
    AgenticTextEdit* currentEditor = qobject_cast<AgenticTextEdit*>(tab_widget_->currentWidget());
    if (currentEditor) {
        currentEditor->redo();
    }
}

void MultiTabEditor::find() {
    AgenticTextEdit* currentEditor = qobject_cast<AgenticTextEdit*>(tab_widget_->currentWidget());
    if (currentEditor) {
        QString searchText = QInputDialog::getText(this, "Find", "Enter text to find:");
        if (!searchText.isEmpty()) {
            // Simple find implementation
            QString text = currentEditor->toPlainText();
            int index = text.indexOf(searchText);
            if (index != -1) {
                QTextCursor cursor = currentEditor->textCursor();
                cursor.setPosition(index);
                cursor.setPosition(index + searchText.length(), QTextCursor::KeepAnchor);
                currentEditor->setTextCursor(cursor);
                currentEditor->setFocus();
            } else {
                QMessageBox::information(this, "Find", "Text not found");
            }
        }
    }
}

void MultiTabEditor::replace() {
    AgenticTextEdit* currentEditor = qobject_cast<AgenticTextEdit*>(tab_widget_->currentWidget());
    if (currentEditor) {
        QString searchText = QInputDialog::getText(this, "Replace", "Enter text to find:");
        if (!searchText.isEmpty()) {
            QString replaceText = QInputDialog::getText(this, "Replace", "Enter replacement text:");
            
            QString text = currentEditor->toPlainText();
            text.replace(searchText, replaceText);
            currentEditor->setPlainText(text);
            
            QMessageBox::information(this, "Replace", "Replacement completed");
        }
    }
}

QString MultiTabEditor::getCurrentText() const {
    AgenticTextEdit* currentEditor = qobject_cast<AgenticTextEdit*>(tab_widget_->currentWidget());
    if (currentEditor) {
        return currentEditor->toPlainText();
    }
    return QString();
}

QString MultiTabEditor::getSelectedText() const {
    AgenticTextEdit* currentEditor = qobject_cast<AgenticTextEdit*>(tab_widget_->currentWidget());
    if (currentEditor) {
        QTextCursor cursor = currentEditor->textCursor();
        return cursor.selectedText();
    }
    return QString();
}

QString MultiTabEditor::getCurrentFilePath() const {
    AgenticTextEdit* currentEditor = qobject_cast<AgenticTextEdit*>(tab_widget_->currentWidget());
    if (currentEditor) {
        // Return the stored full file path, or empty string if not saved yet
        return tab_file_paths_.value(currentEditor, QString());
    }
    return QString();
}

void MultiTabEditor::setLSPClient(LSPClient* client) {
    m_lspClient = client;
    
    // Wire LSP to all existing editors
    for (int i = 0; i < tab_widget_->count(); ++i) {
        AgenticTextEdit* editor = qobject_cast<AgenticTextEdit*>(tab_widget_->widget(i));
        if (editor) {
            editor->setLSPClient(client);
        }
    }
}

void MultiTabEditor::setAICompletionProvider(AICompletionProvider* provider) {
    m_aiProvider = provider;
    
    // Wire AI provider to all existing editors
    for (int i = 0; i < tab_widget_->count(); ++i) {
        AgenticTextEdit* editor = qobject_cast<AgenticTextEdit*>(tab_widget_->widget(i));
        if (editor) {
            editor->setAICompletionProvider(provider);
        }
    }
    
    qDebug() << "AI completion provider wired to all editors";
}

AgenticTextEdit* MultiTabEditor::getCurrentEditor() const {
    return qobject_cast<AgenticTextEdit*>(tab_widget_->currentWidget());
}
