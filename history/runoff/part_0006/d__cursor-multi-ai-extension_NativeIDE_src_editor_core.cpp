#include "editor_core.h"
#include <fstream>
#include <locale>
#include <codecvt>

EditorCore::EditorCore() {
    m_textBuffer = std::make_unique<TextBuffer>();
    m_syntaxHighlighter = std::make_unique<SyntaxHighlighter>();
}

EditorCore::~EditorCore() = default;

bool EditorCore::Initialize() {
    if (!m_textBuffer || !m_syntaxHighlighter) {
        return false;
    }
    
    // Set default language
    m_syntaxHighlighter->SetLanguage("text");
    
    return true;
}

void EditorCore::Shutdown() {
    // Clean up resources
    m_textBuffer.reset();
    m_syntaxHighlighter.reset();
}

bool EditorCore::OpenFile(const std::wstring& filePath) {
    if (!LoadFileContent(filePath)) {
        return false;
    }
    
    m_currentFilePath = filePath;
    m_isModified = false;
    m_isReadOnly = false;
    
    // Detect and set language based on file extension
    DetectAndSetLanguage(filePath);
    
    // Refresh syntax highlighting
    RefreshSyntaxHighlighting();
    
    // Reset cursor to beginning
    SetCursorPosition(0, 0);
    ClearSelection();
    
    // Notify observers
    if (OnFileOpened) {
        OnFileOpened(filePath);
    }
    
    return true;
}

bool EditorCore::SaveCurrentFile(const std::wstring& filePath) {
    std::wstring pathToSave = filePath.empty() ? m_currentFilePath : filePath;
    
    if (pathToSave.empty()) {
        return false;  // Need to specify a file path
    }
    
    if (!SaveFileContent(pathToSave)) {
        return false;
    }
    
    if (filePath.empty()) {
        // Saving to current file
        m_isModified = false;
        UpdateModifiedState(false);
    } else {
        // Save As operation
        m_currentFilePath = filePath;
        m_isModified = false;
        UpdateModifiedState(false);
        
        // Update language based on new file extension
        DetectAndSetLanguage(filePath);
        RefreshSyntaxHighlighting();
    }
    
    // Notify observers
    if (OnFileSaved) {
        OnFileSaved(pathToSave);
    }
    
    return true;
}

bool EditorCore::SaveCurrentFileAs(const std::wstring& filePath) {
    return SaveCurrentFile(filePath);
}

bool EditorCore::NewFile() {
    m_textBuffer->Clear();
    m_currentFilePath.clear();
    m_isModified = false;
    m_isReadOnly = false;
    
    // Set default language
    m_syntaxHighlighter->SetLanguage("text");
    
    // Reset cursor and selection
    SetCursorPosition(0, 0);
    ClearSelection();
    
    UpdateModifiedState(false);
    
    return true;
}

bool EditorCore::CloseCurrentFile() {
    // TODO: Prompt to save if modified
    return NewFile();
}

void EditorCore::InsertText(const std::wstring& text) {
    if (m_isReadOnly) {
        return;
    }
    
    if (m_hasSelection) {
        // Replace selected text
        ReplaceText(m_selectionStartLine, m_selectionStartColumn,
                   m_selectionEndLine, m_selectionEndColumn, text);
        
        // Update cursor position to end of inserted text
        std::vector<std::wstring> lines;
        std::wstringstream ss(text);
        std::wstring line;
        while (std::getline(ss, line, L'\n')) {
            lines.push_back(line);
        }
        
        if (lines.size() == 1) {
            SetCursorPosition(m_selectionStartLine, m_selectionStartColumn + text.length());
        } else {
            SetCursorPosition(m_selectionStartLine + lines.size() - 1, lines.back().length());
        }
        
        ClearSelection();
    } else {
        // Insert at cursor position
        m_textBuffer->InsertText(m_cursorLine, m_cursorColumn, text);
        
        // Update cursor position
        std::vector<std::wstring> lines;
        std::wstringstream ss(text);
        std::wstring line;
        while (std::getline(ss, line, L'\n')) {
            lines.push_back(line);
        }
        
        if (lines.size() == 1) {
            SetCursorPosition(m_cursorLine, m_cursorColumn + text.length());
        } else {
            SetCursorPosition(m_cursorLine + lines.size() - 1, lines.back().length());
        }
    }
    
    UpdateModifiedState(true);
    RefreshSyntaxHighlighting();
    NotifyTextChanged();
}

void EditorCore::DeleteText(size_t startLine, size_t startColumn, size_t endLine, size_t endColumn) {
    if (m_isReadOnly) {
        return;
    }
    
    m_textBuffer->DeleteText(startLine, startColumn, endLine, endColumn);
    
    SetCursorPosition(startLine, startColumn);
    ClearSelection();
    
    UpdateModifiedState(true);
    RefreshSyntaxHighlighting();
    NotifyTextChanged();
}

void EditorCore::ReplaceText(size_t startLine, size_t startColumn, size_t endLine, size_t endColumn, const std::wstring& newText) {
    if (m_isReadOnly) {
        return;
    }
    
    m_textBuffer->ReplaceText(startLine, startColumn, endLine, endColumn, newText);
    
    UpdateModifiedState(true);
    RefreshSyntaxHighlighting();
    NotifyTextChanged();
}

void EditorCore::Cut() {
    if (m_isReadOnly || !m_hasSelection) {
        return;
    }
    
    Copy();  // Copy to clipboard first
    
    // Delete selected text
    DeleteText(m_selectionStartLine, m_selectionStartColumn,
              m_selectionEndLine, m_selectionEndColumn);
}

void EditorCore::Copy() {
    if (!m_hasSelection) {
        return;
    }
    
    std::wstring selectedText = GetSelectedText();
    
    // Copy to Windows clipboard
    if (OpenClipboard(nullptr)) {
        EmptyClipboard();
        
        // Convert to multibyte string for clipboard
        std::string utf8Text = IDEUtils::WStringToString(selectedText);
        HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, utf8Text.size() + 1);
        
        if (hMem) {
            char* pMem = static_cast<char*>(GlobalLock(hMem));
            if (pMem) {
                strcpy_s(pMem, utf8Text.size() + 1, utf8Text.c_str());
                GlobalUnlock(hMem);
                SetClipboardData(CF_TEXT, hMem);
            }
        }
        
        CloseClipboard();
    }
}

void EditorCore::Paste() {
    if (m_isReadOnly) {
        return;
    }
    
    if (!IsClipboardFormatAvailable(CF_TEXT)) {
        return;
    }
    
    if (OpenClipboard(nullptr)) {
        HANDLE hData = GetClipboardData(CF_TEXT);
        if (hData) {
            char* pszText = static_cast<char*>(GlobalLock(hData));
            if (pszText) {
                std::wstring text = IDEUtils::StringToWString(pszText);
                InsertText(text);
                GlobalUnlock(hData);
            }
        }
        CloseClipboard();
    }
}

void EditorCore::Undo() {
    if (m_isReadOnly) {
        return;
    }
    
    if (m_textBuffer->Undo()) {
        ClearSelection();
        UpdateModifiedState(true);
        RefreshSyntaxHighlighting();
        NotifyTextChanged();
    }
}

void EditorCore::Redo() {
    if (m_isReadOnly) {
        return;
    }
    
    if (m_textBuffer->Redo()) {
        ClearSelection();
        UpdateModifiedState(true);
        RefreshSyntaxHighlighting();
        NotifyTextChanged();
    }
}

bool EditorCore::CanUndo() const {
    return m_textBuffer && m_textBuffer->CanUndo();
}

bool EditorCore::CanRedo() const {
    return m_textBuffer && m_textBuffer->CanRedo();
}

std::vector<EditorCore::SearchResult> EditorCore::FindAll(const std::wstring& searchText, bool matchCase, bool wholeWord, bool useRegex) {
    UNREFERENCED_PARAMETER(useRegex);  // TODO: Implement regex search
    
    std::vector<SearchResult> results;
    
    if (!m_textBuffer || searchText.empty()) {
        return results;
    }
    
    auto bufferResults = m_textBuffer->FindAll(searchText, matchCase, wholeWord);
    
    for (const auto& result : bufferResults) {
        results.push_back({result.line, result.column, result.length});
    }
    
    return results;
}

bool EditorCore::FindNext(const std::wstring& searchText, bool matchCase, bool wholeWord, bool useRegex) {
    UNREFERENCED_PARAMETER(useRegex);  // TODO: Implement regex search
    
    if (!m_textBuffer || searchText.empty()) {
        return false;
    }
    
    auto result = m_textBuffer->FindNext(searchText, m_cursorLine, m_cursorColumn, matchCase, wholeWord);
    
    if (result.line != SIZE_MAX) {
        SetCursorPosition(result.line, result.column);
        SetSelection(result.line, result.column, result.line, result.column + result.length);
        ScrollToCursor();
        return true;
    }
    
    return false;
}

bool EditorCore::FindPrevious(const std::wstring& searchText, bool matchCase, bool wholeWord, bool useRegex) {
    UNREFERENCED_PARAMETER(useRegex);  // TODO: Implement regex search
    
    if (!m_textBuffer || searchText.empty()) {
        return false;
    }
    
    auto result = m_textBuffer->FindPrevious(searchText, m_cursorLine, m_cursorColumn, matchCase, wholeWord);
    
    if (result.line != SIZE_MAX) {
        SetCursorPosition(result.line, result.column);
        SetSelection(result.line, result.column, result.line, result.column + result.length);
        ScrollToCursor();
        return true;
    }
    
    return false;
}

int EditorCore::ReplaceAll(const std::wstring& searchText, const std::wstring& replaceText, bool matchCase, bool wholeWord, bool useRegex) {
    UNREFERENCED_PARAMETER(useRegex);  // TODO: Implement regex replace
    
    if (m_isReadOnly || !m_textBuffer || searchText.empty()) {
        return 0;
    }
    
    auto results = FindAll(searchText, matchCase, wholeWord, false);
    
    if (results.empty()) {
        return 0;
    }
    
    // Replace from end to beginning to maintain correct positions
    std::sort(results.begin(), results.end(), [](const SearchResult& a, const SearchResult& b) {
        if (a.line != b.line) {
            return a.line > b.line;
        }
        return a.column > b.column;
    });
    
    for (const auto& result : results) {
        ReplaceText(result.line, result.column, result.line, result.column + result.length, replaceText);
    }
    
    return static_cast<int>(results.size());
}

void EditorCore::SetCursorPosition(size_t line, size_t column) {
    size_t lineCount = GetLineCount();
    if (line >= lineCount) {
        line = lineCount - 1;
    }
    
    size_t lineLength = m_textBuffer->GetLineLength(line);
    if (column > lineLength) {
        column = lineLength;
    }
    
    if (m_cursorLine != line || m_cursorColumn != column) {
        m_cursorLine = line;
        m_cursorColumn = column;
        NotifyCursorPositionChanged();
    }
}

void EditorCore::GetCursorPosition(size_t& line, size_t& column) const {
    line = m_cursorLine;
    column = m_cursorColumn;
}

void EditorCore::SetSelection(size_t startLine, size_t startColumn, size_t endLine, size_t endColumn) {
    m_selectionStartLine = startLine;
    m_selectionStartColumn = startColumn;
    m_selectionEndLine = endLine;
    m_selectionEndColumn = endColumn;
    m_hasSelection = true;
    
    NotifySelectionChanged();
}

void EditorCore::ClearSelection() {
    if (m_hasSelection) {
        m_hasSelection = false;
        NotifySelectionChanged();
    }
}

std::wstring EditorCore::GetSelectedText() const {
    if (!m_hasSelection || !m_textBuffer) {
        return L"";
    }
    
    return m_textBuffer->GetText(m_selectionStartLine, m_selectionStartColumn,
                                m_selectionEndLine, m_selectionEndColumn);
}

void EditorCore::ScrollToLine(size_t line) {
    m_firstVisibleLine = line;
}

void EditorCore::ScrollToCursor() {
    // Ensure cursor is visible
    if (m_cursorLine < m_firstVisibleLine) {
        m_firstVisibleLine = m_cursorLine;
    } else if (m_cursorLine >= m_firstVisibleLine + m_visibleLineCount && m_visibleLineCount > 0) {
        m_firstVisibleLine = m_cursorLine - m_visibleLineCount + 1;
    }
}

void EditorCore::SetFirstVisibleLine(size_t line) {
    m_firstVisibleLine = line;
}

void EditorCore::SetVisibleArea(size_t lineCount, size_t columnCount) {
    m_visibleLineCount = lineCount;
    m_visibleColumnCount = columnCount;
}

size_t EditorCore::GetLineCount() const {
    return m_textBuffer ? m_textBuffer->GetLineCount() : 0;
}

std::wstring EditorCore::GetLine(size_t line) const {
    return m_textBuffer ? m_textBuffer->GetLine(line) : L"";
}

std::wstring EditorCore::GetText() const {
    return m_textBuffer ? m_textBuffer->GetText() : L"";
}

std::wstring EditorCore::GetText(size_t startLine, size_t startColumn, size_t endLine, size_t endColumn) const {
    return m_textBuffer ? m_textBuffer->GetText(startLine, startColumn, endLine, endColumn) : L"";
}

std::wstring EditorCore::GetFileName() const {
    if (m_currentFilePath.empty()) {
        return L"Untitled";
    }
    
    return IDEUtils::GetFileName(m_currentFilePath);
}

std::wstring EditorCore::GetLanguage() const {
    return IDEUtils::StringToWString(m_syntaxHighlighter->GetLanguage());
}

void EditorCore::SetFontSize(int size) {
    if (size > 0 && size <= 72) {
        m_fontSize = size;
    }
}

void EditorCore::SetFontName(const std::wstring& name) {
    if (!name.empty()) {
        m_fontName = name;
    }
}

void EditorCore::SetShowLineNumbers(bool show) {
    m_showLineNumbers = show;
}

void EditorCore::SetWordWrap(bool wrap) {
    m_wordWrap = wrap;
}

void EditorCore::SetTabSize(int size) {
    if (size > 0 && size <= 16) {
        m_tabSize = size;
    }
}

void EditorCore::RefreshSyntaxHighlighting() {
    if (!m_textBuffer || !m_syntaxHighlighter) {
        return;
    }
    
    size_t lineCount = m_textBuffer->GetLineCount();
    
    for (size_t i = 0; i < lineCount; ++i) {
        if (m_textBuffer->IsLineDirty(i)) {
            std::wstring line = m_textBuffer->GetLine(i);
            auto colors = m_syntaxHighlighter->GetLineColors(line);
            m_textBuffer->SetLineSyntaxColors(i, colors);
        }
    }
}

void EditorCore::SetLanguage(const std::string& language) {
    if (m_syntaxHighlighter) {
        m_syntaxHighlighter->SetLanguage(language);
        RefreshSyntaxHighlighting();
    }
}

void EditorCore::UpdateModifiedState(bool modified) {
    if (m_isModified != modified) {
        m_isModified = modified;
        // TODO: Update UI to show modified state (asterisk in title, etc.)
    }
}

void EditorCore::NotifyTextChanged() {
    if (OnTextChanged) {
        OnTextChanged();
    }
}

void EditorCore::NotifyCursorPositionChanged() {
    if (OnCursorPositionChanged) {
        OnCursorPositionChanged();
    }
}

void EditorCore::NotifySelectionChanged() {
    if (OnSelectionChanged) {
        OnSelectionChanged();
    }
}

bool EditorCore::LoadFileContent(const std::wstring& filePath) {
    std::ifstream file(filePath, std::ios::binary);
    if (!file.is_open()) {
        return false;
    }
    
    // Read file content
    std::string content((std::istreambuf_iterator<char>(file)),
                       std::istreambuf_iterator<char>());
    file.close();
    
    // Convert to wide string (assume UTF-8)
    std::wstring wideContent = IDEUtils::StringToWString(content);
    
    // Set text buffer content
    m_textBuffer->SetText(wideContent);
    
    return true;
}

bool EditorCore::SaveFileContent(const std::wstring& filePath) {
    if (!m_textBuffer) {
        return false;
    }
    
    std::ofstream file(filePath, std::ios::binary);
    if (!file.is_open()) {
        return false;
    }
    
    // Get text content and convert to UTF-8
    std::wstring content = m_textBuffer->GetText();
    std::string utf8Content = IDEUtils::WStringToString(content);
    
    file.write(utf8Content.c_str(), utf8Content.length());
    file.close();
    
    return true;
}

void EditorCore::DetectAndSetLanguage(const std::wstring& filePath) {
    if (!m_syntaxHighlighter) {
        return;
    }
    
    std::wstring extension = IDEUtils::GetFileExtension(filePath);
    std::string language = IDEUtils::DetectLanguageFromExtension(extension);
    
    m_syntaxHighlighter->SetLanguage(language);
}