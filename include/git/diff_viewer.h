#pragma once
/**
 * @file diff_viewer.h
 * @brief Side-by-side and inline diff visualization
 * Batch 5 - Item 62: Diff viewer
 */

#include <string>
#include <vector>
#include <functional>
#include <windows.h>

namespace RawrXD::Git {

enum class DiffViewMode {
    SideBySide,
    Inline,
    Unified
};

enum class LineType {
    Context,
    Added,
    Removed,
    Modified,
    Header
};

struct DiffLine {
    LineType type;
    int oldLineNumber;
    int newLineNumber;
    std::string content;
    std::string oldContent;
    std::string newContent;
};

struct DiffBlock {
    int oldStart;
    int oldCount;
    int newStart;
    int newCount;
    std::vector<DiffLine> lines;
    bool isExpanded;
};

class DiffViewer {
public:
    DiffViewer();
    ~DiffViewer();

    // Initialization
    bool initialize(HWND parent);
    void shutdown();

    // Window management
    HWND getHandle() const;
    void resize(int width, int height);

    // Content
    void setDiff(const std::string& oldContent, const std::string& newContent);
    void setDiff(const std::vector<DiffBlock>& blocks);
    void clear();

    // View mode
    void setViewMode(DiffViewMode mode);
    DiffViewMode getViewMode() const;
    void cycleViewMode();

    // Navigation
    void nextDifference();
    void previousDifference();
    void firstDifference();
    void lastDifference();
    int getCurrentDifference() const;
    int getDifferenceCount() const;

    // Selection
    void selectLine(int line);
    void selectBlock(int block);
    void selectAll();
    void clearSelection();

    // Actions
    void acceptChange();
    void rejectChange();
    void acceptAllChanges();
    void rejectAllChanges();
    void revertChange();

    // Folding
    void collapseAll();
    void expandAll();
    void toggleBlock(int block);
    void setContextLines(int lines);

    // Rendering
    void render();
    void invalidate();

    // Configuration
    void setShowLineNumbers(bool show);
    void setShowWhitespace(bool show);
    void setWordWrap(bool wrap);
    void setIgnoreWhitespace(bool ignore);
    void setIgnoreCase(bool ignore);

    // Colors
    void setAddedColor(uint32_t color);
    void setRemovedColor(uint32_t color);
    void setModifiedColor(uint32_t color);
    void setContextColor(uint32_t color);

    // Events
    using LineClickCallback = std::function<void(int line, LineType type)>;
    using ActionCallback = std::function<void(const std::string& action)>;
    void onLineClick(LineClickCallback callback);
    void onAction(ActionCallback callback);

private:
    HWND m_hwnd{nullptr};
    HWND m_parent{nullptr};
    std::vector<DiffBlock> m_blocks;
    DiffViewMode m_mode{DiffViewMode::SideBySide};
    int m_currentDifference{0};
    int m_contextLines{3};
    bool m_showLineNumbers{true};
    bool m_showWhitespace{false};
    bool m_wordWrap{false};
    bool m_ignoreWhitespace{false};
    bool m_ignoreCase{false};

    uint32_t m_addedColor{0x90EE90};      // Light green
    uint32_t m_removedColor{0xFFB6C1};    // Light pink
    uint32_t m_modifiedColor{0xFFD700};    // Gold
    uint32_t m_contextColor{0xFFFFFF};   // White

    LineClickCallback m_lineClickCallback;
    ActionCallback m_actionCallback;

    void layout();
    void draw(HDC hdc);
    void drawSideBySide(HDC hdc);
    void drawInline(HDC hdc);
    void drawUnified(HDC hdc);
    void drawLine(HDC hdc, const DiffLine& line, int y);
    void drawLineNumber(HDC hdc, int number, int y, bool isOld);
    std::vector<DiffBlock> computeDiff(const std::string& oldContent,
                                         const std::string& newContent);
    LRESULT handleMessage(UINT msg, WPARAM wParam, LPARAM lParam);
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
};

// Global instance
DiffViewer& getDiffViewer();

} // namespace RawrXD::Git
