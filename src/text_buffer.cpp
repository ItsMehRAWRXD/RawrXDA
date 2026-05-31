#include "text_buffer.hpp"
#include <algorithm>
#include <sstream>

namespace RawrXD {

TextBuffer::TextBuffer(uint64_t id) : id_(id) {}
TextBuffer::~TextBuffer() = default;

void TextBuffer::SetText(const std::string& text) {
    std::lock_guard<std::mutex> lock(mutex_);
    originalBuffer_ = text;
    addBuffer_.clear();
    pieces_.clear();
    if (!originalBuffer_.empty())
        pieces_.push_back({true, 0, originalBuffer_.length()});
    RebuildLineIndex();
    modified_ = false;
    undoStack_.clear();
    redoStack_.clear();
}

std::string TextBuffer::GetText() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::string result;
    size_t totalSize = 0;
    for (const auto& p : pieces_) totalSize += p.length;
    result.reserve(totalSize);
    for (const auto& p : pieces_) {
        if (p.isOriginal) result.append(originalBuffer_, p.start, p.length);
        else result.append(addBuffer_, p.start, p.length);
    }
    return result;
}

std::string TextBuffer::GetTextRange(size_t start, size_t length) const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::string result;
    result.reserve(length);
    size_t currentPos = 0;
    for (const auto& p : pieces_) {
        if (currentPos + p.length <= start) { currentPos += p.length; continue; }
        size_t ps = (currentPos < start) ? start - currentPos : 0;
        size_t avail = p.length - ps;
        size_t needed = length - result.size();
        size_t toCopy = std::min(needed, avail);
        if (p.isOriginal) result.append(originalBuffer_, p.start + ps, toCopy);
        else result.append(addBuffer_, p.start + ps, toCopy);
        if (result.size() >= length) break;
        currentPos += p.length;
    }
    return result;
}

size_t TextBuffer::GetLength() const {
    std::lock_guard<std::mutex> lock(mutex_);
    size_t len = 0;
    for (const auto& p : pieces_) len += p.length;
    return len;
}

size_t TextBuffer::GetPieceIndexForOffset(size_t offset) const {
    size_t pos = 0;
    for (size_t i = 0; i < pieces_.size(); ++i) {
        if (pos + pieces_[i].length > offset) return i;
        pos += pieces_[i].length;
    }
    return pieces_.size();
}

void TextBuffer::Insert(size_t position, const std::string& text) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (text.empty()) return;
    if (!internalEdit_) {
        Edit edit;
        edit.position = position;
        edit.newText = text;
        edit.timestamp = std::chrono::steady_clock::now();
        undoStack_.push_back(edit);
        if (undoStack_.size() > MAX_UNDO) undoStack_.erase(undoStack_.begin());
        redoStack_.clear();
    }
    InternalInsert(position, text);
    modified_ = true;
    RebuildLineIndex();
}

void TextBuffer::InternalInsert(size_t position, const std::string& text) {
    size_t addStart = addBuffer_.size();
    addBuffer_ += text;
    if (pieces_.empty()) {
        pieces_.push_back({false, addStart, text.length()});
        return;
    }
    size_t pieceIdx = GetPieceIndexForOffset(position);
    size_t currentPos = 0;
    for (size_t i = 0; i < pieceIdx; ++i) currentPos += pieces_[i].length;
    size_t offsetInPiece = position - currentPos;
    if (pieceIdx >= pieces_.size()) {
        pieces_.push_back({false, addStart, text.length()});
    } else if (offsetInPiece == 0) {
        pieces_.insert(pieces_.begin() + pieceIdx, {false, addStart, text.length()});
    } else if (offsetInPiece == pieces_[pieceIdx].length) {
        pieces_.insert(pieces_.begin() + pieceIdx + 1, {false, addStart, text.length()});
    } else {
        Piece& old = pieces_[pieceIdx];
        Piece before = {old.isOriginal, old.start, offsetInPiece};
        Piece newP   = {false, addStart, text.length()};
        Piece after  = {old.isOriginal, old.start + offsetInPiece, old.length - offsetInPiece};
        pieces_[pieceIdx] = before;
        pieces_.insert(pieces_.begin() + pieceIdx + 1, newP);
        pieces_.insert(pieces_.begin() + pieceIdx + 2, after);
    }
}

void TextBuffer::Delete(size_t position, size_t length) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (length == 0) return;
    std::string oldText = GetTextRange(position, length);
    if (!internalEdit_) {
        Edit edit;
        edit.position = position;
        edit.oldText = oldText;
        edit.timestamp = std::chrono::steady_clock::now();
        undoStack_.push_back(edit);
        if (undoStack_.size() > MAX_UNDO) undoStack_.erase(undoStack_.begin());
        redoStack_.clear();
    }
    InternalDelete(position, length);
    modified_ = true;
    RebuildLineIndex();
}

void TextBuffer::InternalDelete(size_t position, size_t length) {
    // Rebuild full text, erase range, reset piece table
    std::string full;
    full.reserve(GetLength());
    for (const auto& p : pieces_) {
        if (p.isOriginal) full.append(originalBuffer_, p.start, p.length);
        else full.append(addBuffer_, p.start, p.length);
    }
    if (position >= full.size()) return;
    size_t end = std::min(position + length, full.size());
    full.erase(position, end - position);
    originalBuffer_ = std::move(full);
    addBuffer_.clear();
    pieces_.clear();
    if (!originalBuffer_.empty())
        pieces_.push_back({true, 0, originalBuffer_.size()});
}

void TextBuffer::Replace(size_t position, size_t length, const std::string& text) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::string oldText = GetTextRange(position, length);
    if (!internalEdit_) {
        Edit edit;
        edit.position = position;
        edit.oldText = oldText;
        edit.newText = text;
        edit.timestamp = std::chrono::steady_clock::now();
        undoStack_.push_back(edit);
        if (undoStack_.size() > MAX_UNDO) undoStack_.erase(undoStack_.begin());
        redoStack_.clear();
    }
    InternalDelete(position, length);
    InternalInsert(position, text);
    modified_ = true;
    RebuildLineIndex();
}

size_t TextBuffer::GetLineCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return lineIndex_.empty() ? 1 : lineIndex_.size();
}

std::string TextBuffer::GetLine(size_t lineIdx) const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (lineIdx >= lineIndex_.size()) return "";
    const auto& info = lineIndex_[lineIdx];
    return GetTextRange(info.startOffset, info.length);
}

size_t TextBuffer::GetLineStartOffset(size_t lineIdx) const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (lineIdx >= lineIndex_.size()) return 0;
    return lineIndex_[lineIdx].startOffset;
}

size_t TextBuffer::GetLineLength(size_t lineIdx) const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (lineIdx >= lineIndex_.size()) return 0;
    return lineIndex_[lineIdx].length;
}

bool TextBuffer::CanUndo() const { std::lock_guard<std::mutex> lock(mutex_); return !undoStack_.empty(); }
bool TextBuffer::CanRedo() const { std::lock_guard<std::mutex> lock(mutex_); return !redoStack_.empty(); }

void TextBuffer::Undo() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (undoStack_.empty()) return;
    Edit edit = undoStack_.back(); undoStack_.pop_back();
    internalEdit_ = true;
    if (!edit.oldText.empty() && edit.newText.empty())
        InternalInsert(edit.position, edit.oldText);
    else if (edit.oldText.empty() && !edit.newText.empty())
        InternalDelete(edit.position, edit.newText.length());
    else { InternalDelete(edit.position, edit.newText.length()); InternalInsert(edit.position, edit.oldText); }
    internalEdit_ = false;
    modified_ = !undoStack_.empty();
    RebuildLineIndex();
    redoStack_.push_back(edit);
}

void TextBuffer::Redo() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (redoStack_.empty()) return;
    Edit edit = redoStack_.back(); redoStack_.pop_back();
    internalEdit_ = true;
    if (!edit.oldText.empty() && edit.newText.empty())
        InternalDelete(edit.position, edit.oldText.length());
    else if (edit.oldText.empty() && !edit.newText.empty())
        InternalInsert(edit.position, edit.newText);
    else { InternalDelete(edit.position, edit.oldText.length()); InternalInsert(edit.position, edit.newText); }
    internalEdit_ = false;
    modified_ = true;
    RebuildLineIndex();
    undoStack_.push_back(edit);
}

void TextBuffer::ClearHistory() {
    std::lock_guard<std::mutex> lock(mutex_);
    undoStack_.clear(); redoStack_.clear();
}

std::vector<size_t> TextBuffer::FindAll(const std::string& pattern, bool caseSensitive) const {
    std::vector<size_t> results;
    std::string text = GetText();
    std::string pat = pattern;
    if (!caseSensitive) {
        std::transform(text.begin(), text.end(), text.begin(), ::tolower);
        std::transform(pat.begin(), pat.end(), pat.begin(), ::tolower);
    }
    size_t pos = 0;
    while ((pos = text.find(pat, pos)) != std::string::npos) { results.push_back(pos); ++pos; }
    return results;
}

bool TextBuffer::FindNext(const std::string& pattern, size_t startPos, size_t& foundPos) const {
    for (size_t pos : FindAll(pattern)) {
        if (pos >= startPos) { foundPos = pos; return true; }
    }
    return false;
}

std::string TextBuffer::GetContextAround(int line, int column, int contextLines) const {
    std::lock_guard<std::mutex> lock(mutex_);
    int startLine = std::max(0, line - contextLines);
    int endLine = std::min((int)lineIndex_.size() - 1, line + contextLines);
    std::string context;
    for (int i = startLine; i <= endLine; ++i)
        context += GetLine(i);
    return context;
}

void TextBuffer::RebuildLineIndex() {
    lineIndex_.clear();
    std::string fullText;
    fullText.reserve(GetLength());
    for (const auto& p : pieces_) {
        if (p.isOriginal) fullText.append(originalBuffer_, p.start, p.length);
        else fullText.append(addBuffer_, p.start, p.length);
    }
    size_t start = 0, pos = 0;
    while ((pos = fullText.find('\n', start)) != std::string::npos) {
        lineIndex_.push_back({start, pos - start + 1, false, false});
        start = pos + 1;
    }
    if (start < fullText.size())
        lineIndex_.push_back({start, fullText.size() - start, false, false});
    if (lineIndex_.empty())
        lineIndex_.push_back({0, 0, false, false});
}

void TextBuffer::NotifyChange(size_t startLine, size_t endLine) {
    if (changeHandler_) changeHandler_(startLine, endLine);
}

} // namespace RawrXD
