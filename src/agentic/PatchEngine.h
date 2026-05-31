// ============================================================================
// Patch Engine - Fuzzy Unified Diff Parser and Applicator
// Git-style hunks with context matching and fuzzy offset tolerance
// ============================================================================

#pragma once
#include <string>
#include <vector>
#include <functional>

namespace RawrXD {
namespace Agentic {

enum class LineType {
    Context,    // ' '
    Add,        // '+'
    Remove      // '-'
};

struct HunkLine {
    LineType type = LineType::Context;
    std::wstring text;
};

struct Hunk {
    int oldStart = 0;
    int oldCount = 0;
    int newStart = 0;
    int newCount = 0;
    std::vector<HunkLine> lines;
};

struct FilePatch {
    std::wstring oldPath;
    std::wstring newPath;
    std::vector<Hunk> hunks;
    bool isNewFile = false;
    bool isDeleted = false;
};

struct PatchSet {
    std::vector<FilePatch> files;
    std::wstring description;  // Commit message / description
};

struct PatchResult {
    bool success = false;
    std::wstring errorMessage;
    int filesPatched = 0;
    int hunksApplied = 0;
    int hunksFailed = 0;
    std::vector<std::wstring> modifiedFiles;
};

// Core API
bool ParseUnifiedDiff(const std::wstring& diff, PatchSet& out);
bool ApplyPatchSet(const PatchSet& patch, PatchResult& result, int maxFuzz = 3);
bool ApplyPatchToFile(const FilePatch& fp, int maxFuzz);

// Rollback support
bool CreateBackup(const std::wstring& path);
bool RestoreFromBackup(const std::wstring& path);
bool RemoveBackup(const std::wstring& path);

// Utility
std::wstring GetBackupPath(const std::wstring& path);
bool FileExists(const std::wstring& path);
std::vector<std::wstring> ReadFileLines(const std::wstring& path);
bool WriteFileLines(const std::wstring& path, const std::vector<std::wstring>& lines);

// Fuzzy matching
bool HunkMatchesAt(const std::vector<std::wstring>& lines, const Hunk& hunk, int startIdx);
int FindBestHunkMatch(const std::vector<std::wstring>& lines, const Hunk& hunk, int targetIdx, int maxFuzz);

} // namespace Agentic
} // namespace RawrXD
