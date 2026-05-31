// ============================================================================
// DiffEngine.cpp — Myers Diff Algorithm Implementation
// ============================================================================
// Eugene Myers, "An O(ND) Difference Algorithm and Its Variations" (1986).
// Line-based diffing optimized for source code files.
//
// Pattern: PatchResult-style, no exceptions.
// Rule:    NO SOURCE FILE IS TO BE SIMPLIFIED.
// ============================================================================

#include "DiffEngine.h"

#include <algorithm>
#include <sstream>
#include <cmath>

using RawrXD::Diff::DiffEngine;
using RawrXD::Diff::DiffResult;
using RawrXD::Diff::DiffLine;
using RawrXD::Diff::DiffHunk;
using RawrXD::Diff::DiffOp;

// ============================================================================
// Split text into lines (preserving empty lines)
// ============================================================================

std::vector<std::string> DiffEngine::SplitLines(const std::string& text) {
    std::vector<std::string> lines;
    std::istringstream stream(text);
    std::string line;
    while (std::getline(stream, line)) {
        // Remove trailing \r
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        lines.push_back(line);
    }
    // Handle case where text ends without newline
    if (!text.empty() && text.back() != '\n') {
        // Last line already captured by getline
    } else if (!text.empty() && text.back() == '\n' && !lines.empty()) {
        // Text ends with newline — getline already handled it
    }
    return lines;
}

// ============================================================================
// Myers diff algorithm
// ============================================================================

std::vector<DiffLine> DiffEngine::MyersDiff(
    const std::vector<std::string>& oldLines,
    const std::vector<std::string>& newLines) {

    int N = static_cast<int>(oldLines.size());
    int M = static_cast<int>(newLines.size());
    int MAX = N + M;

    if (MAX == 0) return {};

    // V array: maps diagonal k → furthest reaching x endpoint
    // Using offset so negative indices work: V[k + offset]
    int offset = MAX;
    std::vector<int> V(2 * MAX + 1, 0);

    // Trace: record V snapshots for backtracking
    std::vector<std::vector<int>> trace;

    // Forward pass: find shortest edit script
    bool found = false;
    for (int d = 0; d <= MAX; ++d) {
        trace.push_back(V);

        for (int k = -d; k <= d; k += 2) {
            int x;
            if (k == -d || (k != d && V[k - 1 + offset] < V[k + 1 + offset])) {
                x = V[k + 1 + offset]; // Insertion (move down)
            } else {
                x = V[k - 1 + offset] + 1; // Deletion (move right)
            }

            int y = x - k;

            // Follow diagonal (equal elements)
            while (x < N && y < M && oldLines[x] == newLines[y]) {
                ++x;
                ++y;
            }

            V[k + offset] = x;

            if (x >= N && y >= M) {
                found = true;
                break;
            }
        }

        if (found) break;
    }

    // Backtrack to build edit script
    std::vector<DiffLine> result;

    int x = N;
    int y = M;

    // Reverse through trace to reconstruct path
    struct Edit { int prevX, prevY, x, y; };
    std::vector<Edit> edits;

    for (int d = static_cast<int>(trace.size()) - 1; d >= 0; --d) {
        const auto& v = trace[d];
        int k = x - y;

        int prevK;
        if (k == -d || (k != d && v[k - 1 + offset] < v[k + 1 + offset])) {
            prevK = k + 1; // Came from insertion
        } else {
            prevK = k - 1; // Came from deletion
        }

        int prevX = v[prevK + offset];
        int prevY = prevX - prevK;

        // Diagonal moves (equal lines)
        while (x > prevX && y > prevY) {
            --x; --y;
            edits.push_back({x, y, x + 1, y + 1});
        }

        // The actual edit
        if (d > 0) {
            if (x == prevX) {
                // Insertion: y decreased
                --y;
                edits.push_back({prevX, y, prevX, y + 1});
            } else {
                // Deletion: x decreased
                --x;
                edits.push_back({x, prevY, x + 1, prevY});
            }
        }
    }

    // Reverse edits (we built them backwards)
    std::reverse(edits.begin(), edits.end());

    // Convert edits to DiffLines
    int oldIdx = 0, newIdx = 0;
    for (const auto& edit : edits) {
        if (edit.x - edit.prevX == 1 && edit.y - edit.prevY == 1) {
            // Diagonal = equal
            DiffLine dl;
            dl.op = DiffOp::Equal;
            dl.text = oldLines[edit.prevX];
            dl.oldLineNum = edit.prevX + 1;
            dl.newLineNum = edit.prevY + 1;
            result.push_back(dl);
        } else if (edit.x == edit.prevX) {
            // Insertion
            DiffLine dl;
            dl.op = DiffOp::Insert;
            dl.text = newLines[edit.prevY];
            dl.newLineNum = edit.prevY + 1;
            result.push_back(dl);
        } else {
            // Deletion
            DiffLine dl;
            dl.op = DiffOp::Delete;
            dl.text = oldLines[edit.prevX];
            dl.oldLineNum = edit.prevX + 1;
            result.push_back(dl);
        }
    }

    return result;
}

// ============================================================================
// Group diff lines into hunks
// ============================================================================

std::vector<DiffHunk> DiffEngine::GroupIntoHunks(
    const std::vector<DiffLine>& lines, int contextLines) {

    std::vector<DiffHunk> hunks;
    if (lines.empty()) return hunks;

    // Find all change regions
    std::vector<std::pair<int, int>> changeRegions; // [start, end) indices in lines
    int regionStart = -1;

    for (int i = 0; i < static_cast<int>(lines.size()); ++i) {
        if (lines[i].op != DiffOp::Equal) {
            if (regionStart == -1) regionStart = i;
        } else {
            if (regionStart != -1) {
                changeRegions.push_back({regionStart, i});
                regionStart = -1;
            }
        }
    }
    if (regionStart != -1) {
        changeRegions.push_back({regionStart, static_cast<int>(lines.size())});
    }

    if (changeRegions.empty()) return hunks;

    // Expand regions with context and merge overlapping
    for (size_t r = 0; r < changeRegions.size(); ++r) {
        int start = std::max(0, changeRegions[r].first - contextLines);
        int end = std::min(static_cast<int>(lines.size()),
                           changeRegions[r].second + contextLines);

        // Merge with next region if they overlap
        while (r + 1 < changeRegions.size()) {
            int nextStart = std::max(0, changeRegions[r + 1].first - contextLines);
            if (nextStart <= end) {
                end = std::min(static_cast<int>(lines.size()),
                               changeRegions[r + 1].second + contextLines);
                ++r;
            } else {
                break;
            }
        }

        // Build hunk
        DiffHunk hunk;
        hunk.oldStart = 0;
        hunk.newStart = 0;

        for (int i = start; i < end; ++i) {
            hunk.lines.push_back(lines[i]);

            if (hunk.oldStart == 0 && lines[i].oldLineNum > 0) {
                hunk.oldStart = lines[i].oldLineNum;
            }
            if (hunk.newStart == 0 && lines[i].newLineNum > 0) {
                hunk.newStart = lines[i].newLineNum;
            }

            if (lines[i].op == DiffOp::Equal || lines[i].op == DiffOp::Delete) {
                hunk.oldCount++;
            }
            if (lines[i].op == DiffOp::Equal || lines[i].op == DiffOp::Insert) {
                hunk.newCount++;
            }
        }

        hunks.push_back(hunk);
    }

    return hunks;
}

// ============================================================================
// Public API: ComputeDiff
// ============================================================================

DiffResult DiffEngine::ComputeDiff(const std::string& oldText,
                                    const std::string& newText,
                                    int contextLines) {
    DiffResult result;

    auto oldLines = SplitLines(oldText);
    auto newLines = SplitLines(newText);

    result.lines = MyersDiff(oldLines, newLines);

    // Count statistics
    for (const auto& dl : result.lines) {
        switch (dl.op) {
            case DiffOp::Equal:  ++result.unchanged; break;
            case DiffOp::Insert: ++result.additions; break;
            case DiffOp::Delete: ++result.deletions; break;
        }
    }

    // Group into hunks
    result.hunks = GroupIntoHunks(result.lines, contextLines);

    return result;
}

// ============================================================================
// Unified diff output
// ============================================================================

std::string DiffResult::ToUnifiedDiff(const std::string& oldName,
                                       const std::string& newName) const {
    std::ostringstream ss;
    ss << "--- " << oldName << "\n";
    ss << "+++ " << newName << "\n";

    for (const auto& hunk : hunks) {
        ss << hunk.Header() << "\n";
        for (const auto& line : hunk.lines) {
            switch (line.op) {
                case DiffOp::Equal:  ss << " " << line.text << "\n"; break;
                case DiffOp::Insert: ss << "+" << line.text << "\n"; break;
                case DiffOp::Delete: ss << "-" << line.text << "\n"; break;
            }
        }
    }

    return ss.str();
}

// ============================================================================
// Apply a single hunk to original text
// ============================================================================

std::string DiffEngine::ApplyHunk(const std::string& originalText,
                                   const DiffHunk& hunk) {
    auto lines = SplitLines(originalText);

    // Build patched lines
    std::vector<std::string> result;

    // Copy lines before hunk
    int hunkStart = hunk.oldStart - 1; // 0-indexed
    for (int i = 0; i < hunkStart; ++i) {
        if (i < static_cast<int>(lines.size())) {
            result.push_back(lines[i]);
        }
    }

    // Apply hunk
    for (const auto& dl : hunk.lines) {
        if (dl.op == DiffOp::Equal || dl.op == DiffOp::Insert) {
            result.push_back(dl.text);
        }
        // DiffOp::Delete — skip the old line
    }

    // Copy lines after hunk
    int afterHunk = hunkStart + hunk.oldCount;
    for (int i = afterHunk; i < static_cast<int>(lines.size()); ++i) {
        result.push_back(lines[i]);
    }

    // Rejoin
    std::ostringstream ss;
    for (size_t i = 0; i < result.size(); ++i) {
        ss << result[i];
        if (i + 1 < result.size()) ss << "\n";
    }

    // Preserve trailing newline if original had one
    if (!originalText.empty() && originalText.back() == '\n') {
        ss << "\n";
    }

    return ss.str();
}

// ============================================================================
// Apply all accepted hunks (in reverse order to preserve line numbers)
// ============================================================================

std::string DiffEngine::ApplyAcceptedHunks(const std::string& originalText,
                                            const std::vector<DiffHunk>& hunks) {
    // Sort hunks by oldStart descending to apply from bottom-up
    std::vector<const DiffHunk*> accepted;
    for (const auto& h : hunks) {
        if (h.accepted && !h.rejected) {
            accepted.push_back(&h);
        }
    }

    // Sort by oldStart descending
    std::sort(accepted.begin(), accepted.end(),
              [](const DiffHunk* a, const DiffHunk* b) {
                  return a->oldStart > b->oldStart;
              });

    std::string current = originalText;
    for (const auto* hunk : accepted) {
        current = ApplyHunk(current, *hunk);
    }

    return current;
}
