#ifndef RAWRXD_TEXT_BUFFER_HPP
#define RAWRXD_TEXT_BUFFER_HPP

#include <string>
#include <vector>
#include <mutex>
#include <functional>
#include <chrono>
#include <algorithm>

namespace RawrXD {

struct LineInfo {
    size_t startOffset;
    size_t length;
    bool hasBreakpoint;
    bool isBookmarked;
};

struct Edit {
    size_t position;
    std::string oldText;
    std::string newText;
    std::chrono::steady_clock::time_point timestamp;
};

struct Piece {
    bool isOriginal;
    size_t start;
    size_t length;
};

class TextBuffer {
public:
    explicit TextBuffer(uint64_t id);
    ~TextBuffer();

    void SetText(const std::string& text);
    std::string GetText() const;
    std::string GetTextRange(size_t start, size_t length) const;
    size_t GetLength() const;

    size_t GetLineCount() const;
    std::string GetLine(size_t lineIndex) const;
    size_t GetLineStartOffset(size_t lineIndex) const;
    size_t GetLineLength(size_t lineIndex) const;

    void Insert(size_t position, const std::string& text);
    void Delete(size_t position, size_t length);
    void Replace(size_t position, size_t length, const std::string& text);

    bool CanUndo() const;
    bool CanRedo() const;
    void Undo();
    void Redo();
    void ClearHistory();

    bool IsModified() const { return modified_; }
    void SetModified(bool modified) { modified_ = modified; }
    uint64_t GetID() const { return id_; }
    std::string GetPath() const { return path_; }
    void SetPath(const std::string& path) { path_ = path; }

    std::vector<size_t> FindAll(const std::string& pattern, bool caseSensitive = true) const;
    bool FindNext(const std::string& pattern, size_t startPos, size_t& foundPos) const;
    std::string GetContextAround(int line, int column, int contextLines) const;

    using ChangeHandler = std::function<void(size_t startLine, size_t endLine)>;
    void OnChanged(ChangeHandler handler) { changeHandler_ = handler; }
    std::mutex& GetMutex() { return mutex_; }

private:
    void InternalInsert(size_t position, const std::string& text);
    void InternalDelete(size_t position, size_t length);
    void RebuildLineIndex();
    void NotifyChange(size_t startLine, size_t endLine);
    size_t GetPieceIndexForOffset(size_t offset) const;

    uint64_t id_;
    std::string path_;
    std::string originalBuffer_;
    std::string addBuffer_;
    std::vector<Piece> pieces_;
    std::vector<LineInfo> lineIndex_;
    std::vector<Edit> undoStack_;
    std::vector<Edit> redoStack_;
    mutable std::mutex mutex_;
    bool modified_ = false;
    bool internalEdit_ = false;
    ChangeHandler changeHandler_;
    static constexpr size_t MAX_UNDO = 1000;
};

} // namespace RawrXD
#endif // RAWRXD_TEXT_BUFFER_HPP
