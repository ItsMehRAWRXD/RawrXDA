#pragma once

#include "native_ide.h"
#include "text_buffer.h"
#include "syntax_highlighter.h"

class EditorCore {
private:
    std::unique_ptr<TextBuffer> m_textBuffer;
    std::unique_ptr<SyntaxHighlighter> m_syntaxHighlighter;
    
    // Current file information
    std::wstring m_currentFilePath;
    bool m_isModified = false;
    bool m_isReadOnly = false;
    
    // Editor settings
    int m_fontSize = 14;
    std::wstring m_fontName = L"Consolas";
    bool m_showLineNumbers = true;
    bool m_wordWrap = false;
    int m_tabSize = 4;
    bool m_insertSpacesForTabs = true;
    
    // Cursor and selection
    size_t m_cursorLine = 0;
    size_t m_cursorColumn = 0;
    size_t m_selectionStartLine = 0;
    size_t m_selectionStartColumn = 0;
    size_t m_selectionEndLine = 0;
    size_t m_selectionEndColumn = 0;
    bool m_hasSelection = false;
    
    // View state
    size_t m_firstVisibleLine = 0;
    size_t m_firstVisibleColumn = 0;
    size_t m_visibleLineCount = 0;
    size_t m_visibleColumnCount = 0;
    
public:
    EditorCore();
    ~EditorCore();
    
    bool Initialize();
    void Shutdown();
    
    // File operations
    bool OpenFile(const std::wstring& filePath);
    bool SaveCurrentFile(const std::wstring& filePath = L"");
    bool SaveCurrentFileAs(const std::wstring& filePath);
    bool NewFile();
    bool CloseCurrentFile();
    
    // Text operations
    void InsertText(const std::wstring& text);
    void DeleteText(size_t startLine, size_t startColumn, size_t endLine, size_t endColumn);
    void ReplaceText(size_t startLine, size_t startColumn, size_t endLine, size_t endColumn, const std::wstring& newText);
    
    // Clipboard operations
    void Cut();
    void Copy();
    void Paste();
    
    // Undo/Redo
    void Undo();
    void Redo();
    bool CanUndo() const;
    bool CanRedo() const;
    
    // Search and replace
    struct SearchResult {
        size_t line;
        size_t column;
        size_t length;
    };
    
    std::vector<SearchResult> FindAll(const std::wstring& searchText, bool matchCase = false, bool wholeWord = false, bool useRegex = false);
    bool FindNext(const std::wstring& searchText, bool matchCase = false, bool wholeWord = false, bool useRegex = false);
    bool FindPrevious(const std::wstring& searchText, bool matchCase = false, bool wholeWord = false, bool useRegex = false);
    int ReplaceAll(const std::wstring& searchText, const std::wstring& replaceText, bool matchCase = false, bool wholeWord = false, bool useRegex = false);
    
    // Cursor and selection
    void SetCursorPosition(size_t line, size_t column);
    void GetCursorPosition(size_t& line, size_t& column) const;
    void SetSelection(size_t startLine, size_t startColumn, size_t endLine, size_t endColumn);
    void ClearSelection();
    bool HasSelection() const { return m_hasSelection; }
    std::wstring GetSelectedText() const;
    
    // View operations
    void ScrollToLine(size_t line);
    void ScrollToCursor();
    void SetFirstVisibleLine(size_t line);
    void SetVisibleArea(size_t lineCount, size_t columnCount);
    
    // Text buffer access
    size_t GetLineCount() const;
    std::wstring GetLine(size_t line) const;
    std::wstring GetText() const;
    std::wstring GetText(size_t startLine, size_t startColumn, size_t endLine, size_t endColumn) const;
    
    // File information
    const std::wstring& GetCurrentFilePath() const { return m_currentFilePath; }
    bool IsModified() const { return m_isModified; }
    bool IsReadOnly() const { return m_isReadOnly; }
    std::wstring GetFileName() const;
    std::wstring GetLanguage() const;
    
    // Settings
    void SetFontSize(int size);
    int GetFontSize() const { return m_fontSize; }
    void SetFontName(const std::wstring& name);
    const std::wstring& GetFontName() const { return m_fontName; }
    void SetShowLineNumbers(bool show);
    bool GetShowLineNumbers() const { return m_showLineNumbers; }
    void SetWordWrap(bool wrap);
    bool GetWordWrap() const { return m_wordWrap; }
    void SetTabSize(int size);
    int GetTabSize() const { return m_tabSize; }
    
    // Syntax highlighting
    void RefreshSyntaxHighlighting();
    void SetLanguage(const std::string& language);
    
    // Events
    std::function<void()> OnTextChanged;
    std::function<void()> OnCursorPositionChanged;
    std::function<void()> OnSelectionChanged;
    std::function<void(const std::wstring&)> OnFileOpened;
    std::function<void(const std::wstring&)> OnFileSaved;
    
private:
    void UpdateModifiedState(bool modified);
    void NotifyTextChanged();
    void NotifyCursorPositionChanged();
    void NotifySelectionChanged();
    
    bool LoadFileContent(const std::wstring& filePath);
    bool SaveFileContent(const std::wstring& filePath);
    
    void DetectAndSetLanguage(const std::wstring& filePath);
};