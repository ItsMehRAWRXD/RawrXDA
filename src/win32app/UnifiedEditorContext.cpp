// UnifiedEditorContext.cpp - Implementation of centralized editor state tracking
// Part of the Unified Editor Context Runtime (UECR)

#include "UnifiedEditorContext.h"
#include <algorithm>
#include <sstream>

namespace RawrXD {

UnifiedEditorContext& UnifiedEditorContext::Get() {
    static UnifiedEditorContext instance;
    return instance;
}

void UnifiedEditorContext::Initialize(HWND hEditor) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_hEditor = hEditor;
    m_snapshot = EditorContextSnapshot();
    m_changeCounter = 0;
}

void UnifiedEditorContext::UpdateCursor(int line, int column) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    EditorPosition oldPos = m_snapshot.cursor;
    m_snapshot.cursor = {line, column};
    m_changeCounter++;
    m_snapshot.timestamp = m_changeCounter;
    
    ExtractCurrentLine();
    ExtractCurrentWord();
    
    if (oldPos != m_snapshot.cursor) {
        OnCursorMoved.emit(m_snapshot.cursor);
        NotifyContextChange();
    }
}

void UnifiedEditorContext::UpdateSelection(int startLine, int startCol, int endLine, int endCol) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    m_snapshot.selection.start = {startLine, startCol};
    m_snapshot.selection.end = {endLine, endCol};
    
    // Extract selected text
    if (!m_snapshot.fileContent.empty()) {
        std::istringstream stream(m_snapshot.fileContent);
        std::string line;
        std::string selected;
        int currentLine = 0;
        
        while (std::getline(stream, line)) {
            if (currentLine >= startLine && currentLine <= endLine) {
                if (currentLine == startLine && currentLine == endLine) {
                    // Single line selection
                    if (startCol < (int)line.length() && endCol <= (int)line.length()) {
                        selected = line.substr(startCol, endCol - startCol);
                    }
                } else if (currentLine == startLine) {
                    selected += line.substr(startCol) + "\n";
                } else if (currentLine == endLine) {
                    selected += line.substr(0, endCol);
                } else {
                    selected += line + "\n";
                }
            }
            currentLine++;
        }
        m_snapshot.selection.selectedText = selected;
    }
    
    m_changeCounter++;
    m_snapshot.timestamp = m_changeCounter;
    
    OnSelectionChanged.emit(m_snapshot.selection);
    NotifyContextChange();
}

void UnifiedEditorContext::UpdateVisibleRange(int startLine, int endLine, int totalLines) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    m_snapshot.visible.startLine = startLine;
    m_snapshot.visible.endLine = endLine;
    m_snapshot.visible.totalLines = totalLines;
    
    ExtractVisibleLines();
    
    m_changeCounter++;
    m_snapshot.timestamp = m_changeCounter;
    
    NotifyContextChange();
}

void UnifiedEditorContext::UpdateFile(const std::string& path, const std::string& content) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    m_snapshot.filePath = path;
    m_snapshot.fileContent = content;
    
    // Detect language from extension
    size_t dotPos = path.find_last_of('.');
    if (dotPos != std::string::npos) {
        std::string ext = path.substr(dotPos + 1);
        UpdateLanguage(ext);
    }
    
    ExtractCurrentLine();
    ExtractCurrentWord();
    ExtractVisibleLines();
    
    m_changeCounter++;
    m_snapshot.timestamp = m_changeCounter;
    
    OnFileChanged.emit(path);
    NotifyContextChange();
}

void UnifiedEditorContext::UpdateLanguage(const std::string& languageId) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    m_snapshot.language.languageId = languageId;
    m_snapshot.language.fileExtension = languageId;
    m_snapshot.language.isSupported = true;
    
    // Map common extensions to language IDs
    static const std::map<std::string, std::string> extToLang = {
        {"cpp", "cpp"}, {"h", "cpp"}, {"hpp", "cpp"},
        {"c", "c"},
        {"py", "python"},
        {"js", "javascript"}, {"ts", "typescript"},
        {"java", "java"},
        {"go", "go"},
        {"rs", "rust"},
        {"cs", "csharp"},
    };
    
    auto it = extToLang.find(languageId);
    if (it != extToLang.end()) {
        m_snapshot.language.languageId = it->second;
    }
    
    // Set MIME type
    static const std::map<std::string, std::string> langToMime = {
        {"cpp", "text/x-c++src"},
        {"c", "text/x-csrc"},
        {"python", "text/x-python"},
        {"javascript", "text/javascript"},
        {"typescript", "text/typescript"},
        {"java", "text/x-java"},
        {"go", "text/x-go"},
        {"rust", "text/x-rust"},
        {"csharp", "text/x-csharp"},
    };
    
    auto mimeIt = langToMime.find(m_snapshot.language.languageId);
    if (mimeIt != langToMime.end()) {
        m_snapshot.language.mimeType = mimeIt->second;
    }
}

EditorContextSnapshot UnifiedEditorContext::GetSnapshot() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_snapshot;
}

EditorPosition UnifiedEditorContext::GetCursor() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_snapshot.cursor;
}

EditorSelection UnifiedEditorContext::GetSelection() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_snapshot.selection;
}

VisibleRange UnifiedEditorContext::GetVisibleRange() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_snapshot.visible;
}

std::string UnifiedEditorContext::GetCurrentFile() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_snapshot.filePath;
}

std::string UnifiedEditorContext::GetCurrentLine() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_snapshot.currentLine;
}

std::string UnifiedEditorContext::GetCurrentWord() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_snapshot.currentWord;
}

LanguageContext UnifiedEditorContext::GetLanguage() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_snapshot.language;
}

std::string UnifiedEditorContext::GetContextAroundCursor(int linesBefore, int linesAfter) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (m_snapshot.fileContent.empty()) return "";
    
    std::istringstream stream(m_snapshot.fileContent);
    std::string line;
    std::string context;
    int currentLine = 0;
    int targetLine = m_snapshot.cursor.line;
    
    while (std::getline(stream, line)) {
        if (currentLine >= targetLine - linesBefore && currentLine <= targetLine + linesAfter) {
            if (currentLine == targetLine) {
                context += ">>> " + line + "\n";
            } else {
                context += line + "\n";
            }
        }
        currentLine++;
    }
    
    return context;
}

std::vector<std::string> UnifiedEditorContext::GetVisibleSymbols() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    std::vector<std::string> symbols;
    
    // Simple regex-like extraction of potential symbols
    for (const auto& line : m_snapshot.visibleLines) {
        // Extract words that look like identifiers
        std::string word;
        for (char c : line) {
            if (std::isalnum(c) || c == '_') {
                word += c;
            } else {
                if (word.length() > 2 && !std::isdigit(word[0])) {
                    symbols.push_back(word);
                }
                word.clear();
            }
        }
        if (word.length() > 2 && !std::isdigit(word[0])) {
            symbols.push_back(word);
        }
    }
    
    // Remove duplicates
    std::sort(symbols.begin(), symbols.end());
    symbols.erase(std::unique(symbols.begin(), symbols.end()), symbols.end());
    
    return symbols;
}

bool UnifiedEditorContext::IsCursorAtEndOfLine() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_snapshot.cursor.column >= (int)m_snapshot.currentLine.length();
}

bool UnifiedEditorContext::IsCursorAtStartOfLine() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_snapshot.cursor.column == 0;
}

// Private helper methods

void UnifiedEditorContext::ExtractCurrentLine() {
    if (m_snapshot.fileContent.empty()) {
        m_snapshot.currentLine = "";
        return;
    }
    
    std::istringstream stream(m_snapshot.fileContent);
    std::string line;
    int currentLine = 0;
    
    while (std::getline(stream, line)) {
        if (currentLine == m_snapshot.cursor.line) {
            m_snapshot.currentLine = line;
            return;
        }
        currentLine++;
    }
    
    m_snapshot.currentLine = "";
}

void UnifiedEditorContext::ExtractCurrentWord() {
    if (m_snapshot.currentLine.empty() || m_snapshot.cursor.column < 0) {
        m_snapshot.currentWord = "";
        return;
    }
    
    int col = m_snapshot.cursor.column;
    if (col >= (int)m_snapshot.currentLine.length()) {
        col = (int)m_snapshot.currentLine.length() - 1;
    }
    
    // Find word boundaries
    int start = col;
    while (start > 0 && (std::isalnum(m_snapshot.currentLine[start - 1]) || m_snapshot.currentLine[start - 1] == '_')) {
        start--;
    }
    
    int end = col;
    while (end < (int)m_snapshot.currentLine.length() && (std::isalnum(m_snapshot.currentLine[end]) || m_snapshot.currentLine[end] == '_')) {
        end++;
    }
    
    if (end > start) {
        m_snapshot.currentWord = m_snapshot.currentLine.substr(start, end - start);
    } else {
        m_snapshot.currentWord = "";
    }
}

void UnifiedEditorContext::ExtractVisibleLines() {
    m_snapshot.visibleLines.clear();
    
    if (m_snapshot.fileContent.empty()) return;
    
    std::istringstream stream(m_snapshot.fileContent);
    std::string line;
    int currentLine = 0;
    
    while (std::getline(stream, line)) {
        if (currentLine >= m_snapshot.visible.startLine && currentLine <= m_snapshot.visible.endLine) {
            m_snapshot.visibleLines.push_back(line);
        }
        currentLine++;
    }
}

void UnifiedEditorContext::NotifyContextChange() {
    // Emit the context changed signal
    OnContextChanged.emit(m_snapshot);
}

} // namespace RawrXD