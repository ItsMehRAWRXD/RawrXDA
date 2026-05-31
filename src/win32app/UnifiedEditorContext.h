// UnifiedEditorContext.h - Centralized editor state tracking
// Part of the Unified Editor Context Runtime (UECR)
// Tracks cursor, selection, visible range, and language context

#pragma once
#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <mutex>
#include "RawrXD_SignalSlot.h"

namespace RawrXD {

// Editor position (line, column)
struct EditorPosition {
    int line = 0;
    int column = 0;
    
    bool operator==(const EditorPosition& other) const {
        return line == other.line && column == other.column;
    }
    bool operator!=(const EditorPosition& other) const {
        return !(*this == other);
    }
};

// Selection range
struct EditorSelection {
    EditorPosition start;
    EditorPosition end;
    std::string selectedText;
    bool isEmpty() const { return start == end; }
};

// Visible range in editor
struct VisibleRange {
    int startLine = 0;
    int endLine = 0;
    int totalLines = 0;
};

// Language context
struct LanguageContext {
    std::string languageId;      // "cpp", "python", "javascript", etc.
    std::string fileExtension;
    std::string mimeType;
    bool isSupported = false;
};

// Editor context snapshot
struct EditorContextSnapshot {
    std::string filePath;
    std::string fileContent;
    EditorPosition cursor;
    EditorSelection selection;
    VisibleRange visible;
    LanguageContext language;
    std::string currentLine;
    std::string currentWord;
    std::vector<std::string> visibleLines;
    int timestamp = 0;
};

// Unified Editor Context - single source of truth for editor state
class UnifiedEditorContext {
public:
    static UnifiedEditorContext& Get();
    
    // Initialize with editor window handle
    void Initialize(HWND hEditor);
    
    // Update methods (called by editor window proc)
    void UpdateCursor(int line, int column);
    void UpdateSelection(int startLine, int startCol, int endLine, int endCol);
    void UpdateVisibleRange(int startLine, int endLine, int totalLines);
    void UpdateFile(const std::string& path, const std::string& content);
    void UpdateLanguage(const std::string& languageId);
    
    // Get current snapshot
    EditorContextSnapshot GetSnapshot() const;
    
    // Get specific context
    EditorPosition GetCursor() const;
    EditorSelection GetSelection() const;
    VisibleRange GetVisibleRange() const;
    std::string GetCurrentFile() const;
    std::string GetCurrentLine() const;
    std::string GetCurrentWord() const;
    LanguageContext GetLanguage() const;
    
    // Context queries
    std::string GetContextAroundCursor(int linesBefore, int linesAfter) const;
    std::vector<std::string> GetVisibleSymbols() const;
    bool IsCursorAtEndOfLine() const;
    bool IsCursorAtStartOfLine() const;
    
    // Signals for context changes
    Signal<const EditorContextSnapshot&> OnContextChanged;
    Signal<const EditorPosition&> OnCursorMoved;
    Signal<const EditorSelection&> OnSelectionChanged;
    Signal<const std::string&> OnFileChanged;
    
private:
    UnifiedEditorContext() = default;
    ~UnifiedEditorContext() = default;
    
    UnifiedEditorContext(const UnifiedEditorContext&) = delete;
    UnifiedEditorContext& operator=(const UnifiedEditorContext&) = delete;
    
    mutable std::mutex m_mutex;
    HWND m_hEditor = nullptr;
    
    // State
    EditorContextSnapshot m_snapshot;
    int m_changeCounter = 0;
    
    // Helper methods
    void ExtractCurrentLine();
    void ExtractCurrentWord();
    void ExtractVisibleLines();
    void NotifyContextChange();
};

} // namespace RawrXD