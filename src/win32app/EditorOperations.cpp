// ============================================================================
// EditorOperations.cpp — Implementation
// ============================================================================
#include "EditorOperations.h"
#include "RouterOperations.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <regex>

namespace RawrXD {
namespace Win32App {

void EditorOperations::discardRedoBranch(FileContext& ctx) {
    while (ctx.undoStack.size() > ctx.undoPos) {
        ctx.undoStack.pop_back();
    }
}

bool EditorOperations::applyInsertToBuffer(std::string& content, int line, int column, const std::string& text) {
    std::istringstream ss(content);
    std::ostringstream result;
    std::string currentLine;
    int currentLineNum = 1;
    bool sawAnyLine = false;
    while (std::getline(ss, currentLine)) {
        sawAnyLine = true;
        if (currentLineNum == line) {
            if (column >= 0 && column <= static_cast<int>(currentLine.length())) {
                currentLine.insert(static_cast<size_t>(column), text);
            }
        }
        result << currentLine << "\n";
        currentLineNum++;
    }
    if (!sawAnyLine && line == 1) {
        result << text << "\n";
    }
    content = result.str();
    return true;
}

bool EditorOperations::applyDeleteFromBuffer(std::string& content, int line, int column, int length,
                                            std::string* outDeleted) {
    if (length <= 0) {
        return false;
    }
    std::istringstream ss(content);
    std::ostringstream result;
    std::string currentLine;
    int currentLineNum = 1;
    bool deleted = false;
    while (std::getline(ss, currentLine)) {
        if (currentLineNum == line) {
            if (column >= 0 && column < static_cast<int>(currentLine.length())) {
                const int endPos = std::min(column + length, static_cast<int>(currentLine.length()));
                const std::string chunk = currentLine.substr(static_cast<size_t>(column),
                                                             static_cast<size_t>(endPos - column));
                if (outDeleted) {
                    *outDeleted = chunk;
                }
                if (chunk.empty()) {
                    return false;
                }
                currentLine.erase(static_cast<size_t>(column), static_cast<size_t>(endPos - column));
                deleted = true;
            } else {
                if (outDeleted) {
                    outDeleted->clear();
                }
                return false;
            }
        }
        result << currentLine << "\n";
        currentLineNum++;
    }
    content = result.str();
    return deleted;
}

bool EditorOperations::peekLineSubstring(const std::string& content, int line, int column, int length,
                                        std::string& out) {
    if (length <= 0) {
        return false;
    }
    std::istringstream ss(content);
    std::string currentLine;
    int currentLineNum = 1;
    while (std::getline(ss, currentLine)) {
        if (currentLineNum == line) {
            if (column < 0 || column > static_cast<int>(currentLine.length())) {
                return false;
            }
            const int endPos = std::min(column + length, static_cast<int>(currentLine.length()));
            out = currentLine.substr(static_cast<size_t>(column), static_cast<size_t>(endPos - column));
            return !out.empty();
        }
        currentLineNum++;
    }
    return false;
}

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

    discardRedoBranch(*ctx);
    applyInsertToBuffer(ctx->content, line, column, text);
    ctx->isDirty = true;

    EditOperation op{EditOperation::Type::Insert,
                     line,
                     column,
                     static_cast<int>(text.size()),
                     text,
                     {}};
    ctx->undoStack.push_back(op);
    ctx->undoPos = ctx->undoStack.size();

    return true;
}

bool EditorOperations::DeleteText(int fileId, int line, int column, int length) {
    std::lock_guard<std::mutex> lock(m_filesMutex);
    auto ctx = GetFileContext(fileId);
    if (!ctx || ctx->isReadOnly || length <= 0) {
        return false;
    }

    discardRedoBranch(*ctx);

    std::string erasedText;
    if (!applyDeleteFromBuffer(ctx->content, line, column, length, &erasedText) || erasedText.empty()) {
        return false;
    }

    ctx->isDirty = true;

    EditOperation op{EditOperation::Type::Delete,
                     line,
                     column,
                     static_cast<int>(erasedText.size()),
                     erasedText,
                     {}};
    ctx->undoStack.push_back(op);
    ctx->undoPos = ctx->undoStack.size();

    return true;
}

bool EditorOperations::ReplaceText(int fileId, int line, int column, int length, const std::string& replacement) {
    std::lock_guard<std::mutex> lock(m_filesMutex);
    auto ctx = GetFileContext(fileId);
    if (!ctx || ctx->isReadOnly) {
        return false;
    }

    std::string oldStr;
    if (!peekLineSubstring(ctx->content, line, column, length, oldStr)) {
        return false;
    }

    discardRedoBranch(*ctx);
    if (!applyDeleteFromBuffer(ctx->content, line, column, static_cast<int>(oldStr.size()), nullptr)) {
        return false;
    }
    applyInsertToBuffer(ctx->content, line, column, replacement);
    ctx->isDirty = true;

    EditOperation op{EditOperation::Type::Replace,
                     line,
                     column,
                     static_cast<int>(oldStr.size()),
                     oldStr,
                     replacement};
    ctx->undoStack.push_back(op);
    ctx->undoPos = ctx->undoStack.size();

    return true;
}

bool EditorOperations::SelectRange(int fileId, int startLine, int startCol, int endLine, int endCol) {
    std::lock_guard<std::mutex> lock(m_filesMutex);
    auto ctx = GetFileContext(fileId);
    if (!ctx) return false;

    // Clamp to valid line range
    std::istringstream ss(ctx->content);
    std::string line;
    std::vector<std::string> lines;
    while (std::getline(ss, line)) lines.push_back(line);
    int maxLine = static_cast<int>(lines.size());

    startLine = std::max(1, std::min(startLine, maxLine));
    endLine   = std::max(startLine, std::min(endLine, maxLine));

    // Extract selected text
    std::ostringstream sel;
    for (int i = startLine; i <= endLine; ++i) {
        const std::string& l = lines[i - 1];
        int sc = (i == startLine) ? std::min(startCol, static_cast<int>(l.size())) : 0;
        int ec = (i == endLine)   ? std::min(endCol,   static_cast<int>(l.size())) : static_cast<int>(l.size());
        sel << l.substr(sc, ec - sc);
        if (i < endLine) sel << '\n';
    }

    ctx->selection = {startLine, startCol, endLine, endCol, sel.str()};
    return true;
}

bool EditorOperations::Copy(int fileId) {
    std::lock_guard<std::mutex> lock(m_filesMutex);
    auto ctx = GetFileContext(fileId);
    if (!ctx || ctx->selection.selectedText.empty()) return false;
    return RouterOperations::Instance().SetClipboardText(ctx->selection.selectedText);
}

bool EditorOperations::Paste(int fileId, int line, int column) {
    std::string text = RouterOperations::Instance().GetClipboardText();
    if (text.empty()) return false;
    return InsertText(fileId, line, column, text);
}

bool EditorOperations::Cut(int fileId) {
    std::lock_guard<std::mutex> lock(m_filesMutex);
    auto ctx = GetFileContext(fileId);
    if (!ctx || ctx->selection.selectedText.empty()) {
        return false;
    }
    if (!RouterOperations::Instance().SetClipboardText(ctx->selection.selectedText)) {
        return false;
    }

    const auto& sel = ctx->selection;
    discardRedoBranch(*ctx);

    if (sel.startLine == sel.endLine) {
        const int len = sel.endCol - sel.startCol;
        std::string erased;
        if (!applyDeleteFromBuffer(ctx->content, sel.startLine, sel.startCol, len, &erased) || erased.empty()) {
            return false;
        }
        ctx->undoStack.push_back({EditOperation::Type::Delete,
                                  sel.startLine,
                                  sel.startCol,
                                  static_cast<int>(erased.size()),
                                  erased,
                                  {}});
        ctx->undoPos = ctx->undoStack.size();
        ctx->isDirty = true;
        ctx->selection = {};
        return true;
    }

    // Multi-line cut: rebuild content without selected region (invalidates linear undo — reset stack).
    std::istringstream ss(ctx->content);
    std::ostringstream result;
    std::string line;
    int lineNum = 1;
    while (std::getline(ss, line)) {
        if (lineNum < sel.startLine || lineNum > sel.endLine) {
            result << line << '\n';
        } else if (lineNum == sel.startLine && lineNum == sel.endLine) {
            result << line.substr(0, static_cast<size_t>(sel.startCol))
                   << line.substr(static_cast<size_t>(sel.endCol)) << '\n';
        } else if (lineNum == sel.startLine) {
            result << line.substr(0, static_cast<size_t>(sel.startCol));
        } else if (lineNum == sel.endLine) {
            result << line.substr(static_cast<size_t>(sel.endCol)) << '\n';
        }
        ++lineNum;
    }
    ctx->content = result.str();
    ctx->isDirty = true;
    ctx->selection = {};
    ctx->undoStack.clear();
    ctx->undoPos = 0;
    return true;
}

bool EditorOperations::Undo(int fileId) {
    std::lock_guard<std::mutex> lock(m_filesMutex);
    auto ctx = GetFileContext(fileId);
    if (!ctx || ctx->undoPos == 0) {
        return false;
    }

    ctx->undoPos--;
    const auto& op = ctx->undoStack[ctx->undoPos];

    if (op.type == EditOperation::Type::Insert) {
        applyDeleteFromBuffer(ctx->content, op.line, op.column, static_cast<int>(op.content.size()), nullptr);
    } else if (op.type == EditOperation::Type::Delete) {
        applyInsertToBuffer(ctx->content, op.line, op.column, op.content);
    } else if (op.type == EditOperation::Type::Replace) {
        applyDeleteFromBuffer(ctx->content, op.line, op.column, static_cast<int>(op.replacement.size()), nullptr);
        applyInsertToBuffer(ctx->content, op.line, op.column, op.content);
    }
    ctx->isDirty = true;
    return true;
}

bool EditorOperations::Redo(int fileId) {
    std::lock_guard<std::mutex> lock(m_filesMutex);
    auto ctx = GetFileContext(fileId);
    if (!ctx || ctx->undoPos >= ctx->undoStack.size()) {
        return false;
    }

    const auto& op = ctx->undoStack[ctx->undoPos];
    if (op.type == EditOperation::Type::Insert) {
        applyInsertToBuffer(ctx->content, op.line, op.column, op.content);
    } else if (op.type == EditOperation::Type::Delete) {
        applyDeleteFromBuffer(ctx->content, op.line, op.column, static_cast<int>(op.content.size()), nullptr);
    } else if (op.type == EditOperation::Type::Replace) {
        applyDeleteFromBuffer(ctx->content, op.line, op.column, static_cast<int>(op.content.size()), nullptr);
        applyInsertToBuffer(ctx->content, op.line, op.column, op.replacement);
    }
    ctx->undoPos++;
    ctx->isDirty = true;
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
    std::lock_guard<std::mutex> lock(m_filesMutex);
    auto ctx = GetFileContext(fileId);
    if (!ctx) return Selection{};
    return ctx->selection;
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
