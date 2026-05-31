// ============================================================================
// SovereignGapBuffer.cpp — R15 Gap Buffer Implementation
// ============================================================================
// O(1) amortized insertions via gap buffer technique.
// All memory lives in R15 heap space.
//
// Rule: NO EXCEPTIONS. Fail-closed on allocation failure.
// ============================================================================

#include "SovereignGapBuffer.h"
#include <windows.h>

namespace RawrXD {
namespace Editor {

// ============================================================================
// R15Allocator — Windows VirtualAlloc backend
// ============================================================================
void* R15Allocator::Allocate(size_t size) {
    if (size == 0) return nullptr;
    void* ptr = VirtualAlloc(nullptr, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    return ptr; // nullptr on failure — caller must check
}

void R15Allocator::Free(void* ptr) {
    if (ptr) VirtualFree(ptr, 0, MEM_RELEASE);
}

void* R15Allocator::Reallocate(void* ptr, size_t oldSize, size_t newSize) {
    if (newSize == 0) { Free(ptr); return nullptr; }
    void* newPtr = Allocate(newSize);
    if (!newPtr) return nullptr;
    if (ptr && oldSize > 0) {
        std::memcpy(newPtr, ptr, (std::min)(oldSize, newSize));
        Free(ptr);
    }
    return newPtr;
}

// ============================================================================
// SovereignGapBuffer
// ============================================================================

SovereignGapBuffer::SovereignGapBuffer(size_t initialCapacity) {
    if (initialCapacity < 16) initialCapacity = 16;
    base_ = static_cast<char*>(R15Allocator::Allocate(initialCapacity));
    if (!base_) return; // Fail-closed: base_ stays null, all ops become no-ops
    capacity_ = initialCapacity;
    gapStart_ = 0;
    gapEnd_ = initialCapacity;
    cursorOffset_ = 0;
}

SovereignGapBuffer::~SovereignGapBuffer() {
    R15Allocator::Free(base_);
}

SovereignGapBuffer::SovereignGapBuffer(SovereignGapBuffer&& other) noexcept
    : base_(other.base_)
    , capacity_(other.capacity_)
    , gapStart_(other.gapStart_)
    , gapEnd_(other.gapEnd_)
    , cursorOffset_(other.cursorOffset_)
{
    other.base_ = nullptr;
    other.capacity_ = 0;
    other.gapStart_ = other.gapEnd_ = other.cursorOffset_ = 0;
}

SovereignGapBuffer& SovereignGapBuffer::operator=(SovereignGapBuffer&& other) noexcept {
    if (this != &other) {
        R15Allocator::Free(base_);
        base_ = other.base_;
        capacity_ = other.capacity_;
        gapStart_ = other.gapStart_;
        gapEnd_ = other.gapEnd_;
        cursorOffset_ = other.cursorOffset_;
        other.base_ = nullptr;
        other.capacity_ = 0;
        other.gapStart_ = other.gapEnd_ = other.cursorOffset_ = 0;
    }
    return *this;
}

// ------------------------------------------------------------------------
// Core editing
// ------------------------------------------------------------------------

void SovereignGapBuffer::Insert(const char* text, size_t len) {
    if (!base_ || len == 0 || !text) return;
    EnsureGap(len);
    std::memcpy(base_ + gapStart_, text, len);
    gapStart_ += len;
    cursorOffset_ = gapStart_;
}

void SovereignGapBuffer::Delete(size_t len) {
    if (!base_ || len == 0) return;
    if (len > cursorOffset_) len = cursorOffset_;
    gapStart_ -= len;
    cursorOffset_ = gapStart_;
}

void SovereignGapBuffer::DeleteAfter(size_t len) {
    if (!base_ || len == 0) return;
    size_t afterSize = capacity_ - gapEnd_;
    if (len > afterSize) len = afterSize;
    gapEnd_ += len;
}

void SovereignGapBuffer::MoveCursor(ptrdiff_t delta) {
    if (!base_) return;
    ptrdiff_t newPos = static_cast<ptrdiff_t>(cursorOffset_) + delta;
    if (newPos < 0) newPos = 0;
    SetCursor(static_cast<size_t>(newPos));
}

void SovereignGapBuffer::SetCursor(size_t pos) {
    if (!base_) return;
    size_t textLen = GetLength();
    if (pos > textLen) pos = textLen;
    MoveGapTo(pos);
    cursorOffset_ = pos;
}

// ------------------------------------------------------------------------
// Content access
// ------------------------------------------------------------------------

std::string_view SovereignGapBuffer::BeforeCursor() const {
    if (!base_) return {};
    return std::string_view(base_, gapStart_);
}

std::string_view SovereignGapBuffer::AfterCursor() const {
    if (!base_) return {};
    return std::string_view(base_ + gapEnd_, capacity_ - gapEnd_);
}

std::string_view SovereignGapBuffer::GetText() const {
    if (!base_) return {};
    // Note: This returns a view that skips the gap. Not contiguous.
    // For contiguous access, use ToString().
    return std::string_view(base_, gapStart_);
}

char SovereignGapBuffer::GetChar(size_t pos) const {
    if (!base_) return '\0';
    if (pos >= GetLength()) return '\0';
    if (pos < gapStart_) return base_[pos];
    return base_[gapEnd_ + (pos - gapStart_)];
}

size_t SovereignGapBuffer::GetLength() const {
    if (!base_) return 0;
    return gapStart_ + (capacity_ - gapEnd_);
}

// ------------------------------------------------------------------------
// Line-based access
// ------------------------------------------------------------------------

size_t SovereignGapBuffer::GetLineCount() const {
    if (!base_) return 0;
    size_t count = 1;
    for (size_t i = 0; i < gapStart_; ++i) {
        if (base_[i] == '\n') ++count;
    }
    for (size_t i = gapEnd_; i < capacity_; ++i) {
        if (base_[i] == '\n') ++count;
    }
    return count;
}

size_t SovereignGapBuffer::GetLineStart(size_t lineNum) const {
    if (!base_ || lineNum == 0) return 0;
    size_t currentLine = 0;
    for (size_t i = 0; i < gapStart_; ++i) {
        if (base_[i] == '\n') {
            ++currentLine;
            if (currentLine == lineNum) return i + 1;
        }
    }
    for (size_t i = gapEnd_; i < capacity_; ++i) {
        if (base_[i] == '\n') {
            ++currentLine;
            if (currentLine == lineNum) return i + 1;
        }
    }
    return GetLength();
}

size_t SovereignGapBuffer::GetLineLength(size_t lineNum) const {
    size_t start = GetLineStart(lineNum);
    size_t end = GetLineStart(lineNum + 1);
    if (end > start && (end - 1) < GetLength() && GetChar(end - 1) == '\n') {
        return end - start - 1;
    }
    return end - start;
}

std::string_view SovereignGapBuffer::GetLine(size_t lineNum) const {
    size_t start = GetLineStart(lineNum);
    size_t len = GetLineLength(lineNum);
    if (start + len <= gapStart_) {
        return std::string_view(base_ + start, len);
    }
    // Crosses gap — caller must handle or use ToString()
    return std::string_view();
}

size_t SovereignGapBuffer::GetLineFromOffset(size_t offset) const {
    if (!base_) return 0;
    size_t line = 0;
    for (size_t i = 0; i < offset && i < gapStart_; ++i) {
        if (base_[i] == '\n') ++line;
    }
    size_t adjustedOffset = offset > gapStart_ ? offset + (gapEnd_ - gapStart_) : offset;
    for (size_t i = gapEnd_; i < adjustedOffset && i < capacity_; ++i) {
        if (base_[i] == '\n') ++line;
    }
    return line;
}

// ------------------------------------------------------------------------
// Search
// ------------------------------------------------------------------------

size_t SovereignGapBuffer::Find(std::string_view pattern, size_t startPos) const {
    if (!base_ || pattern.empty()) return std::string::npos;
    std::string text = ToString();
    size_t pos = text.find(pattern, startPos);
    return pos;
}

std::vector<size_t> SovereignGapBuffer::FindAll(std::string_view pattern) const {
    std::vector<size_t> results;
    if (!base_ || pattern.empty()) return results;
    std::string text = ToString();
    size_t pos = 0;
    while ((pos = text.find(pattern, pos)) != std::string::npos) {
        results.push_back(pos);
        ++pos;
    }
    return results;
}

// ------------------------------------------------------------------------
// Buffer management
// ------------------------------------------------------------------------

void SovereignGapBuffer::EnsureGap(size_t minGapSize) {
    if (!base_) return;
    size_t currentGap = gapEnd_ - gapStart_;
    if (currentGap >= minGapSize) return;

    size_t textLen = GetLength();
    size_t newCapacity = capacity_ * 2;
    while (newCapacity - textLen < minGapSize) {
        newCapacity *= 2;
    }

    char* newBase = static_cast<char*>(R15Allocator::Allocate(newCapacity));
    if (!newBase) return; // Fail-closed

    // Copy before-gap
    if (gapStart_ > 0) {
        std::memcpy(newBase, base_, gapStart_);
    }
    // Copy after-gap to end of new buffer
    size_t afterSize = capacity_ - gapEnd_;
    if (afterSize > 0) {
        std::memcpy(newBase + newCapacity - afterSize, base_ + gapEnd_, afterSize);
    }

    R15Allocator::Free(base_);
    base_ = newBase;
    gapEnd_ = newCapacity - afterSize;
    capacity_ = newCapacity;
}

void SovereignGapBuffer::MoveGapTo(size_t pos) {
    if (!base_ || pos == gapStart_) return;
    if (pos < gapStart_) {
        ShiftGapLeft(gapStart_ - pos);
    } else {
        ShiftGapRight(pos - gapStart_);
    }
}

void SovereignGapBuffer::ShiftGapLeft(size_t distance) {
    if (!base_ || distance == 0) return;
    std::memmove(base_ + gapStart_ - distance, base_ + gapEnd_ - distance, distance);
    gapStart_ -= distance;
    gapEnd_ -= distance;
}

void SovereignGapBuffer::ShiftGapRight(size_t distance) {
    if (!base_ || distance == 0) return;
    std::memmove(base_ + gapStart_, base_ + gapEnd_, distance);
    gapStart_ += distance;
    gapEnd_ += distance;
}

void SovereignGapBuffer::Reserve(size_t newCapacity) {
    if (!base_ || newCapacity <= capacity_) return;
    EnsureGap(newCapacity - GetLength());
}

void SovereignGapBuffer::Compact() {
    if (!base_) return;
    size_t textLen = GetLength();
    if (textLen == 0) {
        Clear();
        return;
    }
    char* newBase = static_cast<char*>(R15Allocator::Allocate(textLen));
    if (!newBase) return;
    std::memcpy(newBase, base_, gapStart_);
    std::memcpy(newBase + gapStart_, base_ + gapEnd_, capacity_ - gapEnd_);
    R15Allocator::Free(base_);
    base_ = newBase;
    capacity_ = textLen;
    gapStart_ = textLen;
    gapEnd_ = textLen;
    cursorOffset_ = textLen;
}

void SovereignGapBuffer::Clear() {
    if (!base_) return;
    gapStart_ = 0;
    gapEnd_ = capacity_;
    cursorOffset_ = 0;
}

// ------------------------------------------------------------------------
// String conversion
// ------------------------------------------------------------------------

std::string SovereignGapBuffer::ToString() const {
    if (!base_) return {};
    std::string result;
    result.reserve(GetLength());
    result.append(base_, gapStart_);
    result.append(base_ + gapEnd_, capacity_ - gapEnd_);
    return result;
}

void SovereignGapBuffer::FromString(const std::string& text) {
    if (!base_) return;
    Clear();
    Insert(text);
}

// ------------------------------------------------------------------------
// Patch application
// ------------------------------------------------------------------------

bool SovereignGapBuffer::ApplyInsert(size_t pos, std::string_view text) {
    if (!base_) return false;
    SetCursor(pos);
    Insert(text.data(), text.size());
    return true;
}

bool SovereignGapBuffer::ApplyDelete(size_t pos, size_t len) {
    if (!base_) return false;
    SetCursor(pos + len);
    Delete(len);
    return true;
}

bool SovereignGapBuffer::ApplyReplace(size_t pos, size_t oldLen, std::string_view newText) {
    if (!base_) return false;
    SetCursor(pos + oldLen);
    Delete(oldLen);
    Insert(newText.data(), newText.size());
    return true;
}

bool SovereignGapBuffer::ApplyPatch(std::string_view unifiedDiff) {
    // Delegate to DiffEngine for parsing, then apply hunks
    // This is a stub for integration — full implementation would parse
    // unified diff format and call ApplyReplace per hunk.
    (void)unifiedDiff;
    return false; // TODO: integrate with DiffEngine::ComputeDiff
}

} // namespace Editor
} // namespace RawrXD
