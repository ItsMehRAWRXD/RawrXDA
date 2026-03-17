#pragma once
#include <vector>
#include <memory>

namespace RawrXD {

// Represents a single cursor position
struct Cursor {
    int line = 0;
    int col = 0;
    int anchorLine = 0; // For selection
    int anchorCol = 0;
};

// Manages multiple cursors for advanced editing
class CursorManager {
public:
    CursorManager();
    ~CursorManager();

    // Add a new cursor at position
    void addCursor(int line, int col);

    // Remove cursor at index
    void removeCursor(size_t index);

    // Clear all cursors except primary
    void clearSecondaryCursors();

    // Move all cursors
    void moveCursors(int dLine, int dCol, bool keepSelection = false);

    // Get cursors
    const std::vector<Cursor>& getCursors() const { return cursors; }

    // Get primary cursor
    const Cursor& getPrimaryCursor() const { return cursors.empty() ? defaultCursor : cursors[0]; }

    // Set primary cursor
    void setPrimaryCursor(int line, int col);

    // Check if any cursor has selection
    bool hasSelection() const;

    // Clear all selections
    void clearSelections();

private:
    std::vector<Cursor> cursors;
    Cursor defaultCursor;
};

} // namespace RawrXD