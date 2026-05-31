#pragma once
/**
 * @file merge_tool.h
 * @brief Three-way merge conflict resolution
 * Batch 5 - Item 63: Merge tool
 */

#include <string>
#include <vector>
#include <optional>
#include <functional>
#include <windows.h>

namespace RawrXD::Git {

enum class MergeSection {
    Base,
    Ours,
    Theirs,
    Result
};

enum class ConflictResolution {
    None,
    Ours,
    Theirs,
    Both,
    Base
};

struct ConflictBlock {
    int startLine;
    int endLine;
    std::vector<std::string> baseLines;
    std::vector<std::string> oursLines;
    std::vector<std::string> theirsLines;
    ConflictResolution resolution;
    bool isResolved;
};

struct MergeState {
    std::string baseContent;
    std::string oursContent;
    std::string theirsContent;
    std::string resultContent;
    std::vector<ConflictBlock> conflicts;
    int currentConflict;
    int totalConflicts;
    int resolvedConflicts;
};

class MergeTool {
public:
    MergeTool();
    ~MergeTool();

    // Initialization
    bool initialize(HWND parent);
    void shutdown();

    // Window management
    HWND getHandle() const;
    void resize(int width, int height);

    // Content
    void setMerge(const std::string& base, const std::string& ours,
                  const std::string& theirs);
    void setResult(const std::string& result);
    std::string getResult() const;
    void clear();

    // Conflict navigation
    void nextConflict();
    void previousConflict();
    void gotoConflict(int index);
    int getCurrentConflict() const;
    int getConflictCount() const;
    int getResolvedCount() const;
    bool hasUnresolvedConflicts() const;

    // Conflict resolution
    void acceptOurs();
    void acceptTheirs();
    void acceptBoth();
    void acceptBase();
    void markResolved();
    void markUnresolved();
    void editResult(const std::string& content);

    // Auto-resolution
    void autoResolveSimple();
    void autoResolveWhitespace();
    bool isAutoResolvable(int conflictIndex);

    // View options
    void setShowBase(bool show);
    void setShowOurs(bool show);
    void setShowTheirs(bool show);
    void setShowResult(bool show);
    void setSyncScrolling(bool sync);

    // Actions
    void saveResult();
    void abortMerge();
    bool completeMerge();

    // Events
    using ConflictChangeCallback = std::function<void(int conflictIndex)>;
    using ResolutionCallback = std::function<void(int conflictIndex, ConflictResolution resolution)>;
    using CompleteCallback = std::function<void(const std::string& result)>;
    void onConflictChanged(ConflictChangeCallback callback);
    void onConflictResolved(ResolutionCallback callback);
    void onMergeCompleted(CompleteCallback callback);

private:
    HWND m_hwnd{nullptr};
    HWND m_parent{nullptr};
    MergeState m_state;
    bool m_showBase{true};
    bool m_showOurs{true};
    bool m_showTheirs{true};
    bool m_showResult{true};
    bool m_syncScrolling{true};

    ConflictChangeCallback m_conflictCallback;
    ResolutionCallback m_resolutionCallback;
    CompleteCallback m_completeCallback;

    void layout();
    void draw(HDC hdc);
    void updateResult();
    std::vector<ConflictBlock> parseConflicts(const std::string& content);
    LRESULT handleMessage(UINT msg, WPARAM wParam, LPARAM lParam);
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
};

// Global instance
MergeTool& getMergeTool();

} // namespace RawrXD::Git
