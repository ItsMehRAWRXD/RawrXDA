#pragma once

#include <QWidget>
#include <QString>

class QTabWidget;

namespace RawrXD {
    class LSPClient;
    class AgenticTextEdit;
    class AICompletionProvider;
}

class MultiTabEditor : public QWidget {
    Q_OBJECT
public:
    explicit MultiTabEditor(QWidget* parent = nullptr);
    
    // Two-phase initialization: call after QApplication exists
    void initialize();
    
public slots:
    void openFile(const QString& filepath);
    void newFile();
    void saveCurrentFile();
    void undo();
    void redo();
    void find();
    void replace();

    QString getCurrentText() const;
    QString getSelectedText() const;
    QString getCurrentFilePath() const;
    
    // LSP integration
    void setLSPClient(RawrXD::LSPClient* client);
    RawrXD::LSPClient* lspClient() const { return m_lspClient; }
    RawrXD::AgenticTextEdit* getCurrentEditor() const;
    
    // AI Completion Provider integration
    void setAICompletionProvider(RawrXD::AICompletionProvider* provider);

private:
    QTabWidget* tab_widget_;
    QMap<QWidget*, QString> tab_file_paths_;  // Maps editor widget to file path
    RawrXD::LSPClient* m_lspClient{};  // Shared LSP client for all tabs
    RawrXD::AICompletionProvider* m_aiProvider{};  // Shared AI completion provider
};
