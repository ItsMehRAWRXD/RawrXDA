// ============================================================================
// DiffEngine.h — Myers Diff Algorithm for Agent Edit Sessions
// ============================================================================
// Computes minimal edit distance diffs between two strings.
// Used by AgentEditSession for:
//   - Per-hunk diff visualization
//   - Accept/reject individual changes
//   - Side-by-side diff rendering in Agent Panel
//
// Algorithm: Eugene Myers, "An O(ND) Difference Algorithm" (1986)
// Optimized for line-based diffing of source code files.
//
// Pattern: PatchResult-style, no exceptions.
// Rule:    NO SOURCE FILE IS TO BE SIMPLIFIED.
// ============================================================================

#pragma once

#include <string>
#include <vector>

namespace RawrXD {
namespace Diff {

// ============================================================================
// Diff operation types
// ============================================================================
enum class DiffOp {
    Equal,      // Line unchanged
    Insert,     // Line added in new version
    Delete      // Line removed from old version
};

// ============================================================================
// Single diff line
// ============================================================================
struct DiffLine {
    DiffOp op;
    std::string text;
    int oldLineNum = -1;    // Line number in old text (-1 if inserted)
    int newLineNum = -1;    // Line number in new text (-1 if deleted)
};

// ============================================================================
// Diff hunk — contiguous group of changes with context
// ============================================================================
struct DiffHunk {
    int oldStart    = 0;    // Start line in old text
    int oldCount    = 0;    // Number of lines from old text
    int newStart    = 0;    // Start line in new text
    int newCount    = 0;    // Number of lines from new text
    std::vector<DiffLine> lines;
    bool accepted   = false;
    bool rejected   = false;

    // Hunk header (unified diff format)
    std::string Header() const {
        char buf[128];
        snprintf(buf, sizeof(buf), "@@ -%d,%d +%d,%d @@",
                 oldStart, oldCount, newStart, newCount);
        return buf;
    }
};

// ============================================================================
// Diff result
// ============================================================================
struct DiffResult {
    std::vector<DiffLine> lines;    // All diff lines
    std::vector<DiffHunk> hunks;    // Grouped into hunks
    int additions       = 0;
    int deletions       = 0;
    int unchanged       = 0;

    bool HasChanges() const { return additions > 0 || deletions > 0; }

    // Generate unified diff string
    std::string ToUnifiedDiff(const std::string& oldName = "a/file",
                               const std::string& newName = "b/file") const;
};

// ============================================================================
// DiffEngine — Myers diff implementation
// ============================================================================
class DiffEngine {
public:
    // Compute diff between two texts (line-based)
    static DiffResult ComputeDiff(const std::string& oldText,
                                   const std::string& newText,
                                   int contextLines = 3);

    // Apply a specific hunk to original text, return patched text
    static std::string ApplyHunk(const std::string& originalText,
                                  const DiffHunk& hunk);

    // Apply all accepted hunks
    static std::string ApplyAcceptedHunks(const std::string& originalText,
                                           const std::vector<DiffHunk>& hunks);

private:
    // Split text into lines
    static std::vector<std::string> SplitLines(const std::string& text);

    // Myers LCS algorithm
    static std::vector<DiffLine> MyersDiff(const std::vector<std::string>& oldLines,
                                            const std::vector<std::string>& newLines);

    // Group diff lines into hunks with context
    static std::vector<DiffHunk> GroupIntoHunks(const std::vector<DiffLine>& lines,
                                                 int contextLines);
};

} // namespace Diff
} // namespace RawrXD
