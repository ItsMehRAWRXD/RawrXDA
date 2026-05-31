// ============================================================================
// SovereignGapBuffer.h — R15-Based Text Buffer for Sovereign Editor
// ============================================================================
// Zero-copy gap buffer implementation for O(1) insertions at cursor.
// Lives in R15 heap space. No std::string reallocations.
//
// Architecture:
//   [text_before_cursor | gap | text_after_cursor]
//                        ^
//                     cursor
//
// Pattern: Bump-allocated, no exceptions, fail-closed.
// ============================================================================

#pragma once

#include <cstdint>
#include <cstddef>
#include <cstring>
#include <string>
#include <string_view>
#include <vector>
#include <memory>
#include <algorithm>

namespace RawrXD {
namespace Editor {

// ============================================================================
// R15 Allocator Interface (opaque — implemented by heap manager)
// ============================================================================
struct R15Allocator {
    static void* Allocate(size_t size);
    static void  Free(void* ptr);
    static void* Reallocate(void* ptr, size_t oldSize, size_t newSize);
};

// ============================================================================
// Sovereign Gap Buffer
// ============================================================================
class SovereignGapBuffer {
public:
    explicit SovereignGapBuffer(size_t initialCapacity = 65536);
    ~SovereignGapBuffer();

    // Non-copyable (owns R15 memory)
    SovereignGapBuffer(const SovereignGapBuffer&) = delete;
    SovereignGapBuffer& operator=(const SovereignGapBuffer&) = delete;

    // Movable
    SovereignGapBuffer(SovereignGapBuffer&& other) noexcept;
    SovereignGapBuffer& operator=(SovereignGapBuffer&& other) noexcept;

    // ------------------------------------------------------------------------
    // Core editing operations (O(1) amortized)
    // ------------------------------------------------------------------------
    void Insert(const char* text, size_t len);
    void Insert(std::string_view text) { Insert(text.data(), text.size()); }
    void Delete(size_t len);                    // Delete before cursor
    void DeleteAfter(size_t len);               // Delete after cursor
    void MoveCursor(ptrdiff_t delta);             // Relative move
    void SetCursor(size_t pos);                  // Absolute position
    void MoveCursorToStart() { SetCursor(0); }
    void MoveCursorToEnd() { SetCursor(GetLength()); }

    // ------------------------------------------------------------------------
    // Content access (zero-copy views)
    // ------------------------------------------------------------------------
    std::string_view GetText() const;             // Full text (may allocate)
    std::string_view BeforeCursor() const;        // Text before gap
    std::string_view AfterCursor() const;         // Text after gap
    char GetChar(size_t pos) const;              // Character at position
    size_t GetLength() const;                     // Total text length
    size_t GetCursor() const { return cursorOffset_; }

    // ------------------------------------------------------------------------
    // Line-based access
    // ------------------------------------------------------------------------
    size_t GetLineCount() const;
    size_t GetLineStart(size_t lineNum) const;    // Byte offset of line start
    size_t GetLineLength(size_t lineNum) const;   // Length of line (no \n)
    std::string_view GetLine(size_t lineNum) const;
    size_t GetLineFromOffset(size_t offset) const; // Line number for byte offset

    // ------------------------------------------------------------------------
    // Search
    // ------------------------------------------------------------------------
    size_t Find(std::string_view pattern, size_t startPos = 0) const;
    std::vector<size_t> FindAll(std::string_view pattern) const;

    // ------------------------------------------------------------------------
    // Buffer management
    // ------------------------------------------------------------------------
    void Reserve(size_t newCapacity);
    void Compact();                               // Remove gap, shrink to fit
    void Clear();
    size_t GetCapacity() const { return capacity_; }
    size_t GetGapSize() const { return gapEnd_ - gapStart_; }

    // ------------------------------------------------------------------------
    // Diff generation (integration with DiffEngine)
    // ------------------------------------------------------------------------
    std::string ToString() const;
    void FromString(const std::string& text);

    // ------------------------------------------------------------------------
    // AI integration: apply patch directly to buffer
    // ------------------------------------------------------------------------
    bool ApplyPatch(std::string_view unifiedDiff);
    bool ApplyInsert(size_t pos, std::string_view text);
    bool ApplyDelete(size_t pos, size_t len);
    bool ApplyReplace(size_t pos, size_t oldLen, std::string_view newText);

private:
    // R15 memory block
    char* base_ = nullptr;
    size_t capacity_ = 0;

    // Gap boundaries
    size_t gapStart_ = 0;   // Start of gap (also = cursorOffset_)
    size_t gapEnd_ = 0;      // End of gap
    size_t cursorOffset_ = 0; // Logical cursor position in text

    // Internal helpers
    void EnsureGap(size_t minGapSize);
    void MoveGapTo(size_t pos);
    void ShiftGapLeft(size_t distance);
    void ShiftGapRight(size_t distance);
};

} // namespace Editor
} // namespace RawrXD
