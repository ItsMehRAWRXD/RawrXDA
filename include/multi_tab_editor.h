<<<<<<< HEAD
#pragma once

// ============================================================================
// MultiTabEditor — C++20, Win32. No Qt. (QWidget, QTabWidget, QString removed)
// ============================================================================

#include <map>
#include <memory>
#include <string>

namespace RawrXD {
class LSPClient;
class AgenticTextEdit;
class AICompletionProvider;
}

class MultiTabEditor {
public:
    explicit MultiTabEditor(void* parent = nullptr);

    void initialize();

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

    void setLSPClient(RawrXD::LSPClient* client);
    RawrXD::LSPClient* lspClient() const { return m_lspClient; }

    void setAICompletionProvider(RawrXD::AICompletionProvider* provider);
    RawrXD::AICompletionProvider* aiCompletionProvider() const { return m_aiProvider; }

    RawrXD::AgenticTextEdit* getCurrentEditor() const;

private:
    void* tab_widget_ = nullptr;  // Win32 tab control or custom
    std::map<void*, std::string> tab_file_paths_;  // editor handle -> file path
    RawrXD::LSPClient* m_lspClient = nullptr;
    RawrXD::AICompletionProvider* m_aiProvider = nullptr;
};
=======
#pragma once

// ============================================================================
// MultiTabEditor — C++20, Win32. No Qt. (QWidget, QTabWidget, QString removed)
// ============================================================================

#include <map>
#include <memory>
#include <string>

namespace RawrXD {
class LSPClient;
class AgenticTextEdit;
class AICompletionProvider;
}

class MultiTabEditor {
public:
    explicit MultiTabEditor(void* parent = nullptr);

    void initialize();

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

    void setLSPClient(RawrXD::LSPClient* client);
    RawrXD::LSPClient* lspClient() const { return m_lspClient; }

    void setAICompletionProvider(RawrXD::AICompletionProvider* provider);
    RawrXD::AICompletionProvider* aiCompletionProvider() const { return m_aiProvider; }

    RawrXD::AgenticTextEdit* getCurrentEditor() const;

private:
    void* tab_widget_ = nullptr;  // Win32 tab control or custom
    std::map<void*, std::string> tab_file_paths_;  // editor handle -> file path
    RawrXD::LSPClient* m_lspClient = nullptr;
    RawrXD::AICompletionProvider* m_aiProvider = nullptr;
};
>>>>>>> origin/main
