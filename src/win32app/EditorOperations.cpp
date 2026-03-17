// ============================================================================
// EditorOperations.cpp — Implementation
// ============================================================================
#include "EditorOperations.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <regex>

namespace RawrXD {
namespace Win32App {

// Singleton instance
static EditorOperations* g_editorOps = nullptr;

EditorOperations& EditorOperations::Instance() {
    if (!g_editorOps) {
        g_editorOps = new EditorOperations();
    }
    return *g_editorOps;
}

EditorOperations::EditorOperations() : m_nextFileId(1) {
}

EditorOperations::~EditorOperations() {
}

EditorOperations::FileContext* EditorOperations::GetFileContext(int fileId) {
    for (auto& ctx : m_openFiles) {
        if (ctx.id == fileId) {
            return &ctx;
        }
    }
    return nullptr;
}

const EditorOperations::FileContext* EditorOperations::GetFileContext(int fileId) const {
    for (const auto& ctx : m_openFiles) {
        if (ctx.id == fileId) {
            return &ctx;
        }
    }
    return nullptr;
}

bool EditorOperations::OpenFile(const std::string& filePath) {
    std::lock_guard<std::mutex> lock(m_filesMutex);
    
    // Check if already open
    for (const auto& ctx : m_openFiles) {
        if (ctx.path == filePath) {
            return true; // Already open
        }
    }

    // Read file content
    std::ifstream file(filePath, std::ios::binary);
    if (!file.is_open()) {
        return false;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    file.close();

    FileContext ctx;
    ctx.id = m_nextFileId++;
    ctx.path = filePath;
    ctx.content = buffer.str();
    ctx.isDirty = false;
    ctx.isReadOnly = false;

    m_openFiles.push_back(ctx);
    return true;
}

bool EditorOperations::CloseFile(int fileId) {
    std::lock_guard<std::mutex> lock(m_filesMutex);
    auto it = std::find_if(m_openFiles.begin(), m_openFiles.end(),
                          [fileId](const FileContext& ctx) { return ctx.id == fileId; });
    if (it != m_openFiles.end()) {
        m_openFiles.erase(it);
        return true;
    }
    return false;
}

bool EditorOperations::SaveFile(int fileId) {
    std::lock_guard<std::mutex> lock(m_filesMutex);
    auto ctx = GetFileContext(fileId);
    if (!ctx || ctx->isReadOnly) {
        return false;
    }

    std::ofstream file(ctx->path, std::ios::binary);
    if (!file.is_open()) {
        return false;
    }

    file << ctx->content;
    file.close();
    ctx->isDirty = false;
    return true;
}

bool EditorOperations::SaveFileAs(int fileId, const std::string& newPath) {
    std::lock_guard<std::mutex> lock(m_filesMutex);
    auto ctx = GetFileContext(fileId);
    if (!ctx) {
        return false;
    }

    std::ofstream file(newPath, std::ios::binary);
    if (!file.is_open()) {
        return false;
    }

    file << ctx->content;
    file.close();
    ctx->path = newPath;
    ctx->isDirty = false;
    return true;
}

bool EditorOperations::InsertText(int fileId, int line, int column, const std::string& text) {
    std::lock_guard<std::mutex> lock(m_filesMutex);
    auto ctx = GetFileContext(fileId);
    if (!ctx || ctx->isReadOnly) {
        return false;
    }

    // Simple implementation: split content by lines and insert
    std::istringstream ss(ctx->content);
    std::ostringstream result;
    std::string currentLine;
    int currentLineNum = 1;

    while (std::getline(ss, currentLine)) {
        if (currentLineNum == line) {
            if (column >= 0 && column <= static_cast<int>(currentLine.length())) {
                currentLine.insert(column, text);
            }
        }
        result << currentLine << "\n";
        currentLineNum++;
    }

    ctx->content = result.str();
    ctx->isDirty = true;

    // Record undo operation
    EditOperation op{EditOperation::Type::Insert, line, column, static_cast<int>(text.length())};
    ctx->undoStack.resize(ctx->undoPos + 1);
    ctx->undoStack.push_back(op);
    ctx->undoPos++;

    return true;
}

bool EditorOperations::DeleteText(int fileId, int line, int column, int length) {
    std::lock_guard<std::mutex> lock(m_filesMutex);
    auto ctx = GetFileContext(fileId);
    if (!ctx || ctx->isReadOnly || length <= 0) {
        return false;
    }

    std::istringstream ss(ctx->content);
    std::ostringstream result;
    std::string currentLine;
    int currentLineNum = 1;

    while (std::getline(ss, currentLine)) {
        if (currentLineNum == line) {
            if (column >= 0 && column < static_cast<int>(currentLine.length())) {
                int endPos = std::min(column + length, static_cast<int>(currentLine.length()));
                currentLine.erase(column, endPos - column);
            }
        }
        result << currentLine << "\n";
        currentLineNum++;
    }

    ctx->content = result.str();
    ctx->isDirty = true;

    // Record undo operation
    EditOperation op{EditOperation::Type::Delete, line, column, length};
    ctx->undoStack.resize(ctx->undoPos + 1);
    ctx->undoStack.push_back(op);
    ctx->undoPos++;

    return true;
}

bool EditorOperations::ReplaceText(int fileId, int line, int column, int length, const std::string& replacement) {
    std::lock_guard<std::mutex> lock(m_filesMutex);
    auto ctx = GetFileContext(fileId);
    if (!ctx || ctx->isReadOnly) {
        return false;
    }

    DeleteText(fileId, line, column, length);
    InsertText(fileId, line, column, replacement);
    return true;
}

bool EditorOperations::SelectRange(int fileId, int startLine, int startCol, int endLine, int endCol) {
    // TODO: Implement selection tracking
    return true;
}

bool EditorOperations::Copy(int fileId) {
    // TODO: Implement
    return true;
}

bool EditorOperations::Paste(int fileId, int line, int column) {
    // TODO: Implement
    return true;
}

bool EditorOperations::Cut(int fileId) {
    // TODO: Implement
    return true;
}

bool EditorOperations::Undo(int fileId) {
    std::lock_guard<std::mutex> lock(m_filesMutex);
    auto ctx = GetFileContext(fileId);
    if (!ctx || ctx->undoPos == 0) {
        return false;
    }

    ctx->undoPos--;
    // TODO: Implement actual undo logic
    return true;
}

bool EditorOperations::Redo(int fileId) {
    std::lock_guard<std::mutex> lock(m_filesMutex);
    auto ctx = GetFileContext(fileId);
    if (!ctx || ctx->undoPos >= ctx->undoStack.size()) {
        return false;
    }

    ctx->undoPos++;
    // TODO: Implement actual redo logic
    return true;
}

bool EditorOperations::ClearUndoStack(int fileId) {
    std::lock_guard<std::mutex> lock(m_filesMutex);
    auto ctx = GetFileContext(fileId);
    if (!ctx) {
        return false;
    }

    ctx->undoStack.clear();
    ctx->undoPos = 0;
    return true;
}

std::vector<EditorOperations::FindResult> EditorOperations::FindAll(int fileId, const std::string& pattern, bool regex) {
    std::vector<FindResult> results;
    std::lock_guard<std::mutex> lock(m_filesMutex);
    auto ctx = GetFileContext(fileId);
    if (!ctx) {
        return results;
    }

    // Simple literal search implementation
    size_t pos = 0;
    int lineNum = 1;
    while ((pos = ctx->content.find(pattern, pos)) != std::string::npos) {
        // Count newlines to find line number
        int newlines = std::count(ctx->content.begin(), ctx->content.begin() + pos, '\n');
        lineNum = newlines + 1;

        FindResult result;
        result.fileId = fileId;
        result.line = lineNum;
        result.column = pos - ctx->content.rfind('\n', pos) - 1;
        result.matchedText = pattern;
        results.push_back(result);

        pos += pattern.length();
    }

    return results;
}

bool EditorOperations::ReplaceAll(int fileId, const std::string& pattern, const std::string& replacement, bool regex) {
    std::lock_guard<std::mutex> lock(m_filesMutex);
    auto ctx = GetFileContext(fileId);
    if (!ctx || ctx->isReadOnly) {
        return false;
    }

    size_t pos = 0;
    while ((pos = ctx->content.find(pattern, pos)) != std::string::npos) {
        ctx->content.replace(pos, pattern.length(), replacement);
        pos += replacement.length();
    }

    ctx->isDirty = true;
    return true;
}

EditorOperations::LineInfo EditorOperations::GetLine(int fileId, int lineNumber) {
    std::lock_guard<std::mutex> lock(m_filesMutex);
    auto ctx = GetFileContext(fileId);
    LineInfo info{};

    if (!ctx) {
        return info;
    }

    std::istringstream ss(ctx->content);
    std::string line;
    int currentLine = 1;

    while (std::getline(ss, line)) {
        if (currentLine == lineNumber) {
            info.fileId = fileId;
            info.lineNumber = lineNumber;
            info.content = line;
            info.isDirty = ctx->isDirty;
            return info;
        }
        currentLine++;
    }

    return info;
}

bool EditorOperations::SetLine(int fileId, int lineNumber, const std::string& content) {
    std::lock_guard<std::mutex> lock(m_filesMutex);
    auto ctx = GetFileContext(fileId);
    if (!ctx || ctx->isReadOnly) {
        return false;
    }

    std::istringstream ss(ctx->content);
    std::ostringstream result;
    std::string line;
    int currentLine = 1;

    while (std::getline(ss, line)) {
        if (currentLine == lineNumber) {
            result << content;
        } else {
            result << line;
        }
        if (currentLine < lineNumber || (!ss.eof())) {
            result << "\n";
        }
        currentLine++;
    }

    ctx->content = result.str();
    ctx->isDirty = true;
    return true;
}

int EditorOperations::GetLineCount(int fileId) {
    std::lock_guard<std::mutex> lock(m_filesMutex);
    auto ctx = GetFileContext(fileId);
    if (!ctx) {
        return 0;
    }

    return std::count(ctx->content.begin(), ctx->content.end(), '\n') + 1;
}

EditorOperations::Selection EditorOperations::GetSelection(int fileId) {
    // TODO: Implement
    return Selection{};
}

EditorOperations::FileInfo EditorOperations::GetFileInfo(int fileId) {
    std::lock_guard<std::mutex> lock(m_filesMutex);
    auto ctx = GetFileContext(fileId);
    FileInfo info{};

    if (ctx) {
        info.id = ctx->id;
        info.path = ctx->path;
        info.content = ctx->content;
        info.size = ctx->content.size();
        info.isDirty = ctx->isDirty;
        info.isReadOnly = ctx->isReadOnly;
    }

    return info;
}

std::vector<int> EditorOperations::GetOpenFileIds() {
    std::lock_guard<std::mutex> lock(m_filesMutex);
    std::vector<int> ids;
    for (const auto& ctx : m_openFiles) {
        ids.push_back(ctx.id);
    }
    return ids;
}

} // namespace Win32App
} // namespace RawrXD
