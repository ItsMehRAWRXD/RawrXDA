// ============================================================================
// EditorOperations.cpp — Implementation
// ============================================================================
#include "EditorOperations.h"
#include "RouterOperations.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <regex>

namespace {
constexpr size_t kMaxEditorFileBytes = 8u * 1024u * 1024u;
constexpr size_t kMaxEditorPathBytes = 4096u;

std::vector<std::string> splitArgs(const std::string& s, char delim = '|') {
    std::vector<std::string> out;
    std::stringstream ss(s);
    std::string item;
    while (std::getline(ss, item, delim)) {
        out.push_back(item);
    }
    return out;
}

bool tryParseInt(const std::string& s, int& out) {
    try {
        size_t consumed = 0;
        int v = std::stoi(s, &consumed);
        if (consumed != s.size()) {
            return false;
        }
        out = v;
        return true;
    } catch (...) {
        return false;
    }
}
}

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

    if (filePath.empty() || filePath.size() > kMaxEditorPathBytes) {
        return false;
    }
    
    // Check if already open
    for (const auto& ctx : m_openFiles) {
        if (ctx.path == filePath) {
            return true; // Already open
        }
    }

    // Read file content
    std::ifstream file(filePath, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        return false;
    }

    const std::streamoff fileSize = file.tellg();
    if (fileSize < 0 || static_cast<size_t>(fileSize) > kMaxEditorFileBytes) {
        return false;
    }
    file.seekg(0, std::ios::beg);

    std::string content(static_cast<size_t>(fileSize), '\0');
    if (fileSize > 0) {
        file.read(content.data(), fileSize);
        if (!file) {
            return false;
        }
    }
    file.close();

    FileContext ctx;
    ctx.id = m_nextFileId++;
    ctx.path = filePath;
    ctx.content = std::move(content);
    ctx.isDirty = false;
    ctx.isReadOnly = false;

    m_openFiles.push_back(ctx);
    return true;
}

void EditorOperations::RegisterCommands() {
    auto& router = RouterOperations::Instance();

    router.RegisterCommand({"editor.openFile", "Open File", "Open file via dialog", "Editor"}, [this]() {
        std::string path;
        if (RouterOperations::Instance().OpenFileDialog(path)) {
            return OpenFile(path) ? RouterOperations::CommandResult{true, "File opened", "", 0}
                                  : RouterOperations::CommandResult{false, "Failed to open file", "OPEN_FAILED", -1};
        }
        return RouterOperations::CommandResult{false, "File open cancelled", "CANCELLED", 0};
    });

    router.RegisterCommandWithArgs({"editor.openFilePath", "Open File Path", "Open file by path argument", "Editor"},
                                   [this](const std::string& args) {
                                       if (args.empty()) {
                                           return RouterOperations::CommandResult{false, "Path argument is required", "INVALID_ARGS", -1};
                                       }
                                       return OpenFile(args) ? RouterOperations::CommandResult{true, "File opened", "", 0}
                                                             : RouterOperations::CommandResult{false, "Failed to open file", "OPEN_FAILED", -1};
                                   });

    router.RegisterCommandWithArgs({"editor.closeFile", "Close File", "Args: fileId", "Editor"},
                                   [this](const std::string& args) {
                                       int fileId = 0;
                                       if (!tryParseInt(args, fileId)) {
                                           return RouterOperations::CommandResult{false, "Invalid fileId", "INVALID_ARGS", -1};
                                       }
                                       return CloseFile(fileId) ? RouterOperations::CommandResult{true, "File closed", "", 0}
                                                                : RouterOperations::CommandResult{false, "Failed to close file", "CLOSE_FAILED", -1};
                                   });

    router.RegisterCommandWithArgs({"editor.saveFile", "Save File", "Args: fileId", "Editor"},
                                   [this](const std::string& args) {
                                       int fileId = 0;
                                       if (!tryParseInt(args, fileId)) {
                                           return RouterOperations::CommandResult{false, "Invalid fileId", "INVALID_ARGS", -1};
                                       }
                                       return SaveFile(fileId) ? RouterOperations::CommandResult{true, "File saved", "", 0}
                                                               : RouterOperations::CommandResult{false, "Failed to save file", "SAVE_FAILED", -1};
                                   });

    router.RegisterCommandWithArgs({"editor.saveFileAs", "Save File As", "Args: fileId|newPath", "Editor"},
                                   [this](const std::string& args) {
                                       const auto parts = splitArgs(args);
                                       if (parts.size() != 2) {
                                           return RouterOperations::CommandResult{false, "Expected: fileId|newPath", "INVALID_ARGS", -1};
                                       }
                                       int fileId = 0;
                                       if (!tryParseInt(parts[0], fileId)) {
                                           return RouterOperations::CommandResult{false, "Invalid fileId", "INVALID_ARGS", -1};
                                       }
                                       return SaveFileAs(fileId, parts[1])
                                                  ? RouterOperations::CommandResult{true, "File saved", "", 0}
                                                  : RouterOperations::CommandResult{false, "Save As failed", "SAVE_AS_FAILED", -1};
                                   });

    router.RegisterCommandWithArgs({"editor.insertText", "Insert Text", "Args: fileId|line|column|text", "Editor"},
                                   [this](const std::string& args) {
                                       const auto parts = splitArgs(args);
                                       if (parts.size() != 4) {
                                           return RouterOperations::CommandResult{false, "Expected: fileId|line|column|text", "INVALID_ARGS", -1};
                                       }
                                       int fileId = 0, line = 0, column = 0;
                                       if (!tryParseInt(parts[0], fileId) || !tryParseInt(parts[1], line) || !tryParseInt(parts[2], column)) {
                                           return RouterOperations::CommandResult{false, "Invalid numeric args", "INVALID_ARGS", -1};
                                       }
                                       return InsertText(fileId, line, column, parts[3])
                                                  ? RouterOperations::CommandResult{true, "Text inserted", "", 0}
                                                  : RouterOperations::CommandResult{false, "Insert failed", "INSERT_FAILED", -1};
                                   });

    router.RegisterCommandWithArgs({"editor.deleteText", "Delete Text", "Args: fileId|line|column|length", "Editor"},
                                   [this](const std::string& args) {
                                       const auto parts = splitArgs(args);
                                       if (parts.size() != 4) {
                                           return RouterOperations::CommandResult{false, "Expected: fileId|line|column|length", "INVALID_ARGS", -1};
                                       }
                                       int fileId = 0, line = 0, column = 0, length = 0;
                                       if (!tryParseInt(parts[0], fileId) || !tryParseInt(parts[1], line) ||
                                           !tryParseInt(parts[2], column) || !tryParseInt(parts[3], length)) {
                                           return RouterOperations::CommandResult{false, "Invalid numeric args", "INVALID_ARGS", -1};
                                       }
                                       return DeleteText(fileId, line, column, length)
                                                  ? RouterOperations::CommandResult{true, "Text deleted", "", 0}
                                                  : RouterOperations::CommandResult{false, "Delete failed", "DELETE_FAILED", -1};
                                   });

    router.RegisterCommandWithArgs({"editor.replaceText", "Replace Text", "Args: fileId|line|column|length|replacement", "Editor"},
                                   [this](const std::string& args) {
                                       const auto parts = splitArgs(args);
                                       if (parts.size() != 5) {
                                           return RouterOperations::CommandResult{false, "Expected: fileId|line|column|length|replacement", "INVALID_ARGS", -1};
                                       }
                                       int fileId = 0, line = 0, column = 0, length = 0;
                                       if (!tryParseInt(parts[0], fileId) || !tryParseInt(parts[1], line) ||
                                           !tryParseInt(parts[2], column) || !tryParseInt(parts[3], length)) {
                                           return RouterOperations::CommandResult{false, "Invalid numeric args", "INVALID_ARGS", -1};
                                       }
                                       return ReplaceText(fileId, line, column, length, parts[4])
                                                  ? RouterOperations::CommandResult{true, "Text replaced", "", 0}
                                                  : RouterOperations::CommandResult{false, "Replace failed", "REPLACE_FAILED", -1};
                                   });

    router.RegisterCommandWithArgs({"editor.selectRange", "Select Range", "Args: fileId|startLine|startCol|endLine|endCol", "Editor"},
                                   [this](const std::string& args) {
                                       const auto parts = splitArgs(args);
                                       if (parts.size() != 5) {
                                           return RouterOperations::CommandResult{false, "Expected: fileId|startLine|startCol|endLine|endCol", "INVALID_ARGS", -1};
                                       }
                                       int fileId = 0, sl = 0, sc = 0, el = 0, ec = 0;
                                       if (!tryParseInt(parts[0], fileId) || !tryParseInt(parts[1], sl) ||
                                           !tryParseInt(parts[2], sc) || !tryParseInt(parts[3], el) || !tryParseInt(parts[4], ec)) {
                                           return RouterOperations::CommandResult{false, "Invalid numeric args", "INVALID_ARGS", -1};
                                       }
                                       return SelectRange(fileId, sl, sc, el, ec)
                                                  ? RouterOperations::CommandResult{true, "Range selected", "", 0}
                                                  : RouterOperations::CommandResult{false, "Select failed", "SELECT_FAILED", -1};
                                   });

    router.RegisterCommandWithArgs({"editor.copy", "Copy", "Args: fileId", "Editor"}, [this](const std::string& args) {
        int fileId = 0;
        if (!tryParseInt(args, fileId)) {
            return RouterOperations::CommandResult{false, "Invalid fileId", "INVALID_ARGS", -1};
        }
        return Copy(fileId) ? RouterOperations::CommandResult{true, "Copied", "", 0}
                            : RouterOperations::CommandResult{false, "Copy failed", "COPY_FAILED", -1};
    });

    router.RegisterCommandWithArgs({"editor.cut", "Cut", "Args: fileId", "Editor"}, [this](const std::string& args) {
        int fileId = 0;
        if (!tryParseInt(args, fileId)) {
            return RouterOperations::CommandResult{false, "Invalid fileId", "INVALID_ARGS", -1};
        }
        return Cut(fileId) ? RouterOperations::CommandResult{true, "Cut", "", 0}
                           : RouterOperations::CommandResult{false, "Cut failed", "CUT_FAILED", -1};
    });

    router.RegisterCommandWithArgs({"editor.paste", "Paste", "Args: fileId|line|column", "Editor"},
                                   [this](const std::string& args) {
                                       const auto parts = splitArgs(args);
                                       if (parts.size() != 3) {
                                           return RouterOperations::CommandResult{false, "Expected: fileId|line|column", "INVALID_ARGS", -1};
                                       }
                                       int fileId = 0, line = 0, column = 0;
                                       if (!tryParseInt(parts[0], fileId) || !tryParseInt(parts[1], line) || !tryParseInt(parts[2], column)) {
                                           return RouterOperations::CommandResult{false, "Invalid numeric args", "INVALID_ARGS", -1};
                                       }
                                       return Paste(fileId, line, column)
                                                  ? RouterOperations::CommandResult{true, "Pasted", "", 0}
                                                  : RouterOperations::CommandResult{false, "Paste failed", "PASTE_FAILED", -1};
                                   });

    router.RegisterCommandWithArgs({"editor.undo", "Undo", "Args: fileId", "Editor"}, [this](const std::string& args) {
        int fileId = 0;
        if (!tryParseInt(args, fileId)) {
            return RouterOperations::CommandResult{false, "Invalid fileId", "INVALID_ARGS", -1};
        }
        return Undo(fileId) ? RouterOperations::CommandResult{true, "Undone", "", 0}
                            : RouterOperations::CommandResult{false, "Undo failed", "UNDO_FAILED", -1};
    });

    router.RegisterCommandWithArgs({"editor.redo", "Redo", "Args: fileId", "Editor"}, [this](const std::string& args) {
        int fileId = 0;
        if (!tryParseInt(args, fileId)) {
            return RouterOperations::CommandResult{false, "Invalid fileId", "INVALID_ARGS", -1};
        }
        return Redo(fileId) ? RouterOperations::CommandResult{true, "Redone", "", 0}
                            : RouterOperations::CommandResult{false, "Redo failed", "REDO_FAILED", -1};
    });

    router.RegisterCommandWithArgs({"editor.findAll", "Find All", "Args: fileId|pattern", "Editor"},
                                   [this](const std::string& args) {
                                       const auto parts = splitArgs(args);
                                       if (parts.size() != 2) {
                                           return RouterOperations::CommandResult{false, "Expected: fileId|pattern", "INVALID_ARGS", -1};
                                       }
                                       int fileId = 0;
                                       if (!tryParseInt(parts[0], fileId)) {
                                           return RouterOperations::CommandResult{false, "Invalid fileId", "INVALID_ARGS", -1};
                                       }
                                       const auto hits = FindAll(fileId, parts[1], false);
                                       return RouterOperations::CommandResult{true, "Matches: " + std::to_string(hits.size()), "", 0};
                                   });

    router.RegisterCommandWithArgs({"editor.replaceAll", "Replace All", "Args: fileId|pattern|replacement", "Editor"},
                                   [this](const std::string& args) {
                                       const auto parts = splitArgs(args);
                                       if (parts.size() != 3) {
                                           return RouterOperations::CommandResult{false, "Expected: fileId|pattern|replacement", "INVALID_ARGS", -1};
                                       }
                                       int fileId = 0;
                                       if (!tryParseInt(parts[0], fileId)) {
                                           return RouterOperations::CommandResult{false, "Invalid fileId", "INVALID_ARGS", -1};
                                       }
                                       return ReplaceAll(fileId, parts[1], parts[2], false)
                                                  ? RouterOperations::CommandResult{true, "ReplaceAll complete", "", 0}
                                                  : RouterOperations::CommandResult{false, "ReplaceAll failed", "REPLACE_ALL_FAILED", -1};
                                   });

    router.RegisterCommand({"editor.getOpenFileIds", "Get Open File IDs", "Return open file IDs", "Editor"}, [this]() {
        const auto ids = GetOpenFileIds();
        std::string msg;
        for (size_t i = 0; i < ids.size(); ++i) {
            msg += std::to_string(ids[i]);
            if (i + 1 < ids.size()) {
                msg += ",";
            }
        }
        return RouterOperations::CommandResult{true, msg.empty() ? "No open files" : msg, "", 0};
    });

    router.RegisterCommandWithArgs({"editor.getSelection", "Get Selection", "Args: fileId", "Editor"}, [this](const std::string& args) {
        int fileId = 0;
        if (!tryParseInt(args, fileId)) {
            return RouterOperations::CommandResult{false, "Invalid fileId", "INVALID_ARGS", -1};
        }
        auto sel = GetSelection(fileId);
        return RouterOperations::CommandResult{true, sel.selectedText, "", 0};
    });
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

    if (ctx->path.empty() || ctx->path.size() > kMaxEditorPathBytes) {
        return false;
    }

    std::ofstream file(ctx->path, std::ios::binary);
    if (!file.is_open()) {
        return false;
    }

    file << ctx->content;
    file.flush();
    if (!file) {
        return false;
    }
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

    if (newPath.empty() || newPath.size() > kMaxEditorPathBytes) {
        return false;
    }

    std::ofstream file(newPath, std::ios::binary);
    if (!file.is_open()) {
        return false;
    }

    file << ctx->content;
    file.flush();
    if (!file) {
        return false;
    }
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
