#pragma once


namespace RawrXD {
    class LSPClient;
    class AgenticTextEdit;
}

class MultiTabEditor : public void {

public:
    explicit MultiTabEditor(void* parent = nullptr);
    
    // Two-phase initialization: call after QApplication exists
    void initialize();
    
public:
    void openFile(const std::string& filepath);
    void newFile();
    void saveCurrentFile();
    void undo();
    void redo();
    void find();
    void replace();

    std::string getCurrentText() const;
    std::string getSelectedText() const;
    std::string getCurrentFilePath() const;
    
    // LSP integration
    void setLSPClient(RawrXD::LSPClient* client);
    RawrXD::LSPClient* lspClient() const { return m_lspClient; }
    RawrXD::AgenticTextEdit* getCurrentEditor() const;

private:
    QTabWidget* tab_widget_;
    std::map<void*, std::string> tab_file_paths_;  // Maps editor widget to file path
    RawrXD::LSPClient* m_lspClient{};  // Shared LSP client for all tabs
};

