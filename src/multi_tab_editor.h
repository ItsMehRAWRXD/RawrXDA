#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <deque>
#include <functional>
#include <optional>

// Forward declarations of core systems
namespace RawrXD {
    class LSPClient;
    // AgenticTextEdit is now an internal logic component, not a widget
    struct AgenticTextEdit; 
}

struct TextPosition {
    size_t line = 0;
    size_t column = 0;

    bool operator==(const TextPosition& other) const { return line == other.line && column == other.column; }
    bool operator!=(const TextPosition& other) const { return !(*this == other); }
    bool operator<(const TextPosition& other) const {
        if (line != other.line) return line < other.line;
        return column < other.column;
    }
};

struct SelectionRange {
    TextPosition start;
    TextPosition end;
    bool active = false;
};

// Undo/Redo Action Types
enum class EditActionType {
    Insert,
    Delete,
    Replace
};

struct EditAction {
    EditActionType type;
    TextPosition position;
    std::string text; // Text added or removed
    std::string originalText; // For replace operations
    uint64_t timestamp;
};

class EditorTab {
public:
    EditorTab();
    ~EditorTab();

    // Core Buffer Operations
    void loadFromFile(const std::string& path);
    void saveToFile(const std::string& path);
    
    // Text Manipulation
    void insertText(const std::string& text, bool recordUndo = true);
    void deleteBack(bool recordUndo = true);
    void deleteForward(bool recordUndo = true);
    void deleteSelection(bool recordUndo = true);
    void deleteRange(TextPosition start, TextPosition end, bool recordUndo = true);

    
    // Logic/State
    std::string getAllText() const;
    std::string getSelectedText() const;
    bool isModified() const { return m_isModified; }
    const std::string& getFilePath() const { return m_filePath; }
    void setFilePath(const std::string& path) { m_filePath = path; }

    // Navigation
    void setCursor(size_t line, size_t column);
    void setSelection(const TextPosition& start, const TextPosition& end);
    TextPosition getCursor() const { return m_cursor; }
    void moveCursor(int deltaLines, int deltaCols);

    // History
    void pushUndo(const EditAction& action);
    void undo();
    void redo();

    // Internal line buffer access for rendering/analysis
    const std::vector<std::string>& getLines() const { return m_lines; }

private:
    std::string m_filePath;
    std::vector<std::string> m_lines;
    TextPosition m_cursor;
    SelectionRange m_selection;
    bool m_isModified = false;
    bool m_readOnly = false;

    // Undo/Redo Stacks
    std::deque<EditAction> m_undoStack;
    std::deque<EditAction> m_redoStack;
    static const size_t MAX_UNDO_DEPTH = 1000;

    void updateModifiedState();
};

class MultiTabEditor {
public:
    explicit MultiTabEditor(void* parent = nullptr); // Parent kept for compatibility but unused
    ~MultiTabEditor();

    void initialize();
    
    // Tab Management
    size_t createNewTab();
    void openFile(const std::string& filepath); // Opens in new tab
    void closeCurrentTab();
    void switchTab(size_t index);
    void saveCurrentFile();
    void saveAllFiles();

    // Editor Operations (Proxy to Active Tab)
    void undo();
    void redo();
    
    // Search & Replace
    struct FindResult {
        size_t line;
        size_t column;
        size_t length;
        bool found;
    };
    FindResult find(const std::string& query, bool forward = true, bool caseSensitive = false);
    int replace(const std::string& query, const std::string& replacement, bool all = false);

    // State Access
    std::string getCurrentText() const;
    std::string getSelectedText() const;
    std::string getCurrentFilePath() const;
    size_t  getActiveTabIndex() const { return m_activeTabIndex; }
    size_t  getTabCount() const { return m_tabs.size(); }
    bool    idCurrentTabModified() const;

    // Integration points
    void setLSPClient(RawrXD::LSPClient* client);
    RawrXD::LSPClient* lspClient() const { return m_lspClient; }
    
    // Direct access for Agentic loops (Reverse Engineered Requirement)
    std::shared_ptr<EditorTab> getActiveTab();

private:
    std::vector<std::shared_ptr<EditorTab>> m_tabs;
    size_t m_activeTabIndex = 0;
    
    RawrXD::LSPClient* m_lspClient = nullptr;
    
    // Helper to ensure a valid tab exists
    void ensureTabExists();
};

