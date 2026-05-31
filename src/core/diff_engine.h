// ============================================================================
// diff_engine.h — Production Diff Algorithm (Myers + Hunt-Szymanski + Patience)
// ============================================================================
// Zero dependencies beyond C++20 + Win32. No Qt.
// Provides:
//   1. Myers O(ND) diff for line/character-level changes
//   2. Hunt-Szymanski LCS for large files (O((N+M) log (N+M)))
//   3. Patience diff for semantic code diffing
//   4. AVX2-accelerated hash comparison for binary diffing
//   5. Unified diff, side-by-side, and inline word diff output
// ============================================================================
#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>
#include <memory>
#include <functional>
#include <optional>

namespace RawrXD::Core::Diff {

// ═══════════════════════════════════════════════════════════════════════════
// Core Types
// ═══════════════════════════════════════════════════════════════════════════

enum class DiffOp : uint8_t {
    Equal     = 0,  // Unchanged
    Insert    = 1,  // Added in new
    Delete    = 2,  // Removed from old
    Replace   = 3   // Changed (delete + insert at same position)
};

struct DiffEdit {
    DiffOp      op;
    size_t      oldPos;   // Position in old text (line or char offset)
    size_t      oldLen;   // Length in old text
    size_t      newPos;   // Position in new text
    size_t      newLen;   // Length in new text
    std::string text;     // The actual text content (for inserts/replaces)
};

struct DiffHunk {
    size_t              oldStart;    // 1-based line number in old file
    size_t              oldCount;    // Number of lines from old file
    size_t              newStart;    // 1-based line number in new file
    size_t              newCount;    // Number of lines from new file
    std::vector<DiffEdit> edits;     // Line-level edits within this hunk
};

struct DiffResult {
    std::vector<DiffHunk> hunks;
    size_t                oldLineCount = 0;
    size_t                newLineCount = 0;
    size_t                insertions = 0;
    size_t                deletions = 0;
    size_t                modifications = 0;
    double                similarity = 0.0;  // 0.0-1.0, Jaccard similarity
};

// Word-level diff for inline rendering
struct WordDiff {
    DiffOp      op;
    std::string text;
};

struct InlineDiffResult {
    std::vector<WordDiff> words;
    size_t                oldWordCount = 0;
    size_t                newWordCount = 0;
};

// ═══════════════════════════════════════════════════════════════════════════
// Configuration
// ═══════════════════════════════════════════════════════════════════════════

struct DiffConfig {
    size_t contextLines = 3;        // Lines of context around changes
    size_t maxDiffSize = 10 * 1024 * 1024; // 10MB max for full diff
    bool   ignoreWhitespace = false;  // Ignore leading/trailing whitespace
    bool   ignoreCase = false;      // Case-insensitive comparison
    bool   ignoreBlankLines = false;// Skip empty lines
    bool   usePatience = true;      // Use patience diff for code
    bool   useAVX2 = true;          // Enable AVX2 acceleration if available
    size_t patienceMaxLines = 1000; // Fall back to Myers for large files
};

// ═══════════════════════════════════════════════════════════════════════════
// Diff Engine
// ═══════════════════════════════════════════════════════════════════════════

class DiffEngine {
public:
    explicit DiffEngine(const DiffConfig& config = {});
    ~DiffEngine();

    // Main entry points
    DiffResult diffLines(const std::string& oldText, const std::string& newText);
    DiffResult diffLines(std::string_view oldText, std::string_view newText);
    
    // Character-level diff (for single-line changes)
    std::vector<DiffEdit> diffChars(std::string_view oldLine, std::string_view newLine);
    
    // Word-level diff (for inline rendering)
    InlineDiffResult diffWords(std::string_view oldText, std::string_view newText);
    
    // Unified diff format output
    std::string toUnifiedDiff(const DiffResult& result,
                               const std::string& oldFileName = "a",
                               const std::string& newFileName = "b",
                               const std::string& oldTimestamp = "",
                               const std::string& newTimestamp = "");
    
    // Side-by-side diff output
    struct SideBySideLine {
        std::string oldLine;
        std::string newLine;
        DiffOp      op;
        size_t      oldLineNum;
        size_t      newLineNum;
    };
    std::vector<SideBySideLine> toSideBySide(const DiffResult& result);
    
    // HTML inline diff (for chat rendering)
    std::string toInlineHtml(const InlineDiffResult& result);
    
    // Similarity score (0.0 = completely different, 1.0 = identical)
    double calculateSimilarity(std::string_view oldText, std::string_view newText);
    
    // Binary diff (for non-text files)
    struct BinaryDiffResult {
        bool        isDifferent;
        size_t      oldSize;
        size_t      newSize;
        double      similarity;  // Based on byte-level comparison
    };
    BinaryDiffResult diffBinary(const void* oldData, size_t oldSize,
                                const void* newData, size_t newSize);

private:
    DiffConfig m_config;
    bool       m_hasAVX2;
    
    // Algorithm implementations
    std::vector<DiffEdit> myersDiff(const std::vector<std::string_view>& oldLines,
                                   const std::vector<std::string_view>& newLines);
    
    std::vector<DiffEdit> huntSzymanskiLCS(const std::vector<std::string_view>& oldLines,
                                          const std::vector<std::string_view>& newLines);
    
    std::vector<DiffEdit> patienceDiff(const std::vector<std::string_view>& oldLines,
                                       const std::vector<std::string_view>& newLines);
    
    // Helpers
    std::vector<std::string_view> splitLines(std::string_view text);
    std::vector<std::string_view> splitWords(std::string_view text);
    bool linesEqual(std::string_view a, std::string_view b) const;
    uint64_t hashLine(std::string_view line) const;
    
    // AVX2 acceleration
    bool detectAVX2() const;
    bool linesEqualAVX2(std::string_view a, std::string_view b) const;
    
    // Hunk construction
    std::vector<DiffHunk> buildHunks(const std::vector<DiffEdit>& edits,
                                     size_t oldLineCount, size_t newLineCount);
    
    // Patience diff helpers
    struct UniqueLine {
        std::string_view line;
        size_t           oldIndex;
        size_t           newIndex;
    };
    std::vector<UniqueLine> findUniqueCommonLines(
        const std::vector<std::string_view>& oldLines,
        const std::vector<std::string_view>& newLines);
};

// ═══════════════════════════════════════════════════════════════════════════
// Free Functions (convenience API)
// ═══════════════════════════════════════════════════════════════════════════

DiffResult DiffLines(const std::string& oldText, const std::string& newText,
                     const DiffConfig& config = {});

std::string UnifiedDiff(const std::string& oldText, const std::string& newText,
                        const std::string& oldFileName = "a",
                        const std::string& newFileName = "b",
                        const DiffConfig& config = {});

InlineDiffResult DiffWords(const std::string& oldText, const std::string& newText,
                           const DiffConfig& config = {});

double Similarity(const std::string& oldText, const std::string& newText,
                  const DiffConfig& config = {});

} // namespace RawrXD::Core::Diff
