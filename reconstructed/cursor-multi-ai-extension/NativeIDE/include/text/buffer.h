#pragma once

#include "native_ide.h"

struct UndoAction {
    enum Type {
        Insert,
        Delete,
        Replace
    };
    
    Type type;
    size_t startLine;
    size_t startColumn;
    size_t endLine;
    size_t endColumn;
    std::wstring text;
    std::wstring originalText;
    
    UndoAction(Type t, size_t sLine, size_t sCol, size_t eLine, size_t eCol, const std::wstring& txt, const std::wstring& origTxt = L"")
        : type(t), startLine(sLine), startColumn(sCol), endLine(eLine), endColumn(eCol), text(txt), originalText(origTxt) {}
};

struct TextLine {
    std::wstring content;
    std::vector<uint32_t> syntaxColors;
    bool isDirty = true;
    
    TextLine() = default;
    explicit TextLine(const std::wstring& text) : content(text) {}
};

class TextBuffer {
private:
    std::vector<TextLine> m_lines;
    std::vector<UndoAction> m_undoStack;
    std::vector<UndoAction> m_redoStack;
    
    static constexpr size_t MAX_UNDO_STACK_SIZE = 1000;
    
public:
    TextBuffer();
    ~TextBuffer();
    
    // Basic operations
    void Clear();
    void SetText(const std::wstring& text);
    std::wstring GetText() const;
    std::wstring GetText(size_t startLine, size_t startColumn, size_t endLine, size_t endColumn) const;
    
    // Line operations
    size_t GetLineCount() const;
    std::wstring GetLine(size_t line) const;
    void SetLine(size_t line, const std::wstring& content);
    void InsertLine(size_t line, const std::wstring& content = L"");
    void DeleteLine(size_t line);
    
    // Character operations
    void InsertText(size_t line, size_t column, const std::wstring& text);
    void DeleteText(size_t startLine, size_t startColumn, size_t endLine, size_t endColumn);
    void ReplaceText(size_t startLine, size_t startColumn, size_t endLine, size_t endColumn, const std::wstring& newText);
    
    // Undo/Redo
    void PushUndoAction(const UndoAction& action);
    bool Undo();
    bool Redo();
    bool CanUndo() const { return !m_undoStack.empty(); }
    bool CanRedo() const { return !m_redoStack.empty(); }
    void ClearUndoHistory();
    
    // Syntax highlighting support
    void MarkLineDirty(size_t line);
    void SetLineSyntaxColors(size_t line, const std::vector<uint32_t>& colors);
    const std::vector<uint32_t>& GetLineSyntaxColors(size_t line) const;
    bool IsLineDirty(size_t line) const;
    
    // Search operations
    struct SearchResult {
        size_t line;
        size_t column;
        size_t length;
    };
    
    std::vector<SearchResult> FindAll(const std::wstring& searchText, bool matchCase = false, bool wholeWord = false) const;
    SearchResult FindNext(const std::wstring& searchText, size_t startLine, size_t startColumn, bool matchCase = false, bool wholeWord = false) const;
    SearchResult FindPrevious(const std::wstring& searchText, size_t startLine, size_t startColumn, bool matchCase = false, bool wholeWord = false) const;
    
    // Utility functions
    bool IsValidPosition(size_t line, size_t column) const;
    size_t GetLineLength(size_t line) const;
    std::pair<size_t, size_t> GetWordBounds(size_t line, size_t column) const;
    
private:
    void EnsureLineExists(size_t line);
    void SplitLines(const std::wstring& text, std::vector<std::wstring>& outLines) const;
    void ApplyUndoAction(const UndoAction& action, bool isUndo);
    void TrimUndoStack();
    
    bool MatchesAtPosition(const std::wstring& text, const std::wstring& searchText, size_t pos, bool matchCase, bool wholeWord) const;
    bool IsWordBoundary(const std::wstring& text, size_t pos) const;
};