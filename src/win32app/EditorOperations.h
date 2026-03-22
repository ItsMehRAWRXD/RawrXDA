// ============================================================================
// EditorOperations.h — Real Code Editing Operations
// ============================================================================
// FIX #6: Real IDE Operations
// Purpose:
//   - Implement actual single/multi-line editing (replace, insert, delete)
//   - Handle undo/redo stack management
//   - Provide syntax highlighting and semantic analysis integration
//   - Support selection, clipboard, and search/replace operations
// ============================================================================
#pragma once

#include <string>
#include <vector>
#include <memory>
#include <atomic>
#include <mutex>
#include <cstddef>

namespace RawrXD {
namespace Win32App {

// ============================================================================
// EditorOperations — Direct code editing operations
// ============================================================================
class EditorOperations {
public:
    static EditorOperations& Instance();

    // File operations
    bool OpenFile(const std::string& filePath);
    bool CloseFile(int fileId);
    bool SaveFile(int fileId);
    bool SaveFileAs(int fileId, const std::string& newPath);

    // Content editing
    bool InsertText(int fileId, int line, int column, const std::string& text);
    bool DeleteText(int fileId, int line, int column, int length);
    bool ReplaceText(int fileId, int line, int column, int length, const std::string& replacement);
    
    // Selection and clipboard
    bool SelectRange(int fileId, int startLine, int startCol, int endLine, int endCol);
    bool Copy(int fileId);
    bool Paste(int fileId, int line, int column);
    bool Cut(int fileId);

    // Undo/Redo
    bool Undo(int fileId);
    bool Redo(int fileId);
    bool ClearUndoStack(int fileId);

    // Search and replace
    struct FindResult {
        int fileId;
        int line;
        int column;
        std::string matchedText;
    };
    std::vector<FindResult> FindAll(int fileId, const std::string& pattern, bool regex = false);
    bool ReplaceAll(int fileId, const std::string& pattern, const std::string& replacement, bool regex = false);

    // Line operations
    struct LineInfo {
        int fileId;
        int lineNumber;
        std::string content;
        bool isDirty;
    };
    LineInfo GetLine(int fileId, int lineNumber);
    bool SetLine(int fileId, int lineNumber, const std::string& content);
    int GetLineCount(int fileId);

    // Selection info
    struct Selection {
        int startLine, startCol;
        int endLine, endCol;
        std::string selectedText;
    };
    Selection GetSelection(int fileId);

    // File info
    struct FileInfo {
        int id;
        std::string path;
        std::string content;
        size_t size;
        bool isDirty;
        bool isReadOnly;
    };
    FileInfo GetFileInfo(int fileId);
    std::vector<int> GetOpenFileIds();

private:
    EditorOperations();
    ~EditorOperations();

    struct EditOperation {
        enum class Type { Insert, Delete, Replace } type;
        int line, column, length;
        /// Inserted text (Insert), deleted text (Delete), or replaced-out text (Replace).
        std::string content;
        /// New text for Replace operations; unused for Insert/Delete.
        std::string replacement;
    };

    struct FileContext {
        int id;
        std::string path;
        std::string content;
        bool isDirty = false;
        bool isReadOnly = false;
        std::vector<EditOperation> undoStack;
        /// Number of operations from the front of `undoStack` currently applied to the buffer.
        size_t undoPos = 0;
        Selection selection{};
    };

    FileContext* GetFileContext(int fileId);
    const FileContext* GetFileContext(int fileId) const;

    static void discardRedoBranch(FileContext& ctx);
    static bool applyInsertToBuffer(std::string& content, int line, int column, const std::string& text);
    static bool applyDeleteFromBuffer(std::string& content, int line, int column, int length,
                                      std::string* outDeleted);
    static bool peekLineSubstring(const std::string& content, int line, int column, int length,
                                  std::string& out);
    
    std::vector<FileContext> m_openFiles;
    int m_nextFileId = 0;
    mutable std::mutex m_filesMutex;
};

} // namespace Win32App
} // namespace RawrXD
