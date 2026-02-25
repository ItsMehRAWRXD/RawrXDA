#pragma once
#include <string>
#include <vector>
#include <memory>

namespace RawrXD {
class LSPClient;

class MultiTabEditor {
public:
    struct Tab {
        std::string id;
        std::string filePath;
        std::string content;
        bool isDirty = false;
        int cursorLine = 0;
        int cursorCol = 0;
    };

    MultiTabEditor() = default;
    ~MultiTabEditor() = default;

    std::string generateId();
    std::string newTab();
    void openFile(const std::string& path);
    void saveFile(const std::string& tabId);
    void closeTab(const std::string& tabId);
    void insertText(const std::string& tabId, int line, int col, const std::string& text);
    void deleteText(const std::string& tabId, int line, int col, int length);
    void setActiveTab(const std::string& tabId);
    std::string getActiveTabId() const;
    Tab* getTab(const std::string& tabId);
    void attachLSP(std::shared_ptr<LSPClient> lsp);
    void triggerCompletion();

private:
    std::vector<Tab> m_tabs;
    std::string m_activeTabId;
    std::shared_ptr<LSPClient> m_lsp;
};
} // namespace RawrXD
