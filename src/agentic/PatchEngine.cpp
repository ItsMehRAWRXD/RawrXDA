// ============================================================================
// PatchEngine.cpp - Fuzzy Unified Diff Parser and Applicator
// ============================================================================

#include "PatchEngine.h"
#include <fstream>
#include <sstream>
#include <filesystem>

namespace fs = std::filesystem;

namespace RawrXD {
namespace Agentic {

// ============================================================================
// File I/O Utilities
// ============================================================================

bool FileExists(const std::wstring& path) {
    return fs::exists(path);
}

std::vector<std::wstring> ReadFileLines(const std::wstring& path) {
    std::vector<std::wstring> lines;
    std::wifstream f(path);
    if (!f) return lines;
    
    std::wstring line;
    while (std::getline(f, line)) {
        lines.push_back(line);
    }
    return lines;
}

bool WriteFileLines(const std::wstring& path, const std::vector<std::wstring>& lines) {
    std::wofstream f(path, std::ios::trunc);
    if (!f) return false;
    
    for (const auto& line : lines) {
        f << line << L"\n";
    }
    return f.good();
}

std::wstring GetBackupPath(const std::wstring& path) {
    return path + L".rawrxd.bak";
}

bool CreateBackup(const std::wstring& path) {
    if (!FileExists(path)) return false;
    try {
        fs::copy_file(path, GetBackupPath(path), fs::copy_options::overwrite_existing);
        return true;
    } catch (...) {
        return false;
    }
}

bool RestoreFromBackup(const std::wstring& path) {
    std::wstring backup = GetBackupPath(path);
    if (!FileExists(backup)) return false;
    try {
        fs::copy_file(backup, path, fs::copy_options::overwrite_existing);
        return true;
    } catch (...) {
        return false;
    }
}

bool RemoveBackup(const std::wstring& path) {
    std::wstring backup = GetBackupPath(path);
    if (!FileExists(backup)) return true;
    try {
        fs::remove(backup);
        return true;
    } catch (...) {
        return false;
    }
}

// ============================================================================
// Unified Diff Parser
// ============================================================================

static std::vector<std::wstring> SplitLines(const std::wstring& s) {
    std::vector<std::wstring> lines;
    std::wstringstream ss(s);
    std::wstring line;
    while (std::getline(ss, line)) {
        lines.push_back(line);
    }
    return lines;
}

bool ParseUnifiedDiff(const std::wstring& diff, PatchSet& out) {
    auto lines = SplitLines(diff);
    
    FilePatch currentFile{};
    Hunk currentHunk{};
    bool inHunk = false;
    bool hasContent = false;
    
    for (size_t i = 0; i < lines.size(); ++i) {
        const std::wstring& line = lines[i];
        
        if (line.rfind(L"diff --git", 0) == 0) {
            if (!currentFile.hunks.empty()) {
                out.files.push_back(currentFile);
                currentFile = {};
            }
            hasContent = true;
        }
        else if (line.rfind(L"--- ", 0) == 0) {
            if (line.length() > 4) {
                currentFile.oldPath = line.substr(4);
                // Handle /dev/null
                if (currentFile.oldPath == L"/dev/null") {
                    currentFile.isNewFile = true;
                }
            }
        }
        else if (line.rfind(L"+++ ", 0) == 0) {
            if (line.length() > 4) {
                currentFile.newPath = line.substr(4);
                if (currentFile.newPath == L"/dev/null") {
                    currentFile.isDeleted = true;
                }
            }
        }
        else if (line.rfind(L"@@", 0) == 0) {
            if (inHunk) {
                currentFile.hunks.push_back(currentHunk);
                currentHunk = {};
            }
            
            // Parse hunk header: @@ -oldStart,oldCount +newStart,newCount @@
            swscanf_s(line.c_str(),
                L"@@ -%d,%d +%d,%d @@",
                &currentHunk.oldStart,
                &currentHunk.oldCount,
                &currentHunk.newStart,
                &currentHunk.newCount);
            
            inHunk = true;
        }
        else if (inHunk && !line.empty()) {
            HunkLine hl{};
            wchar_t prefix = line[0];
            
            if (prefix == L'+') hl.type = LineType::Add;
            else if (prefix == L'-') hl.type = LineType::Remove;
            else hl.type = LineType::Context;
            
            hl.text = line.substr(1);
            currentHunk.lines.push_back(hl);
        }
    }
    
    if (inHunk) {
        currentFile.hunks.push_back(currentHunk);
    }
    if (!currentFile.hunks.empty()) {
        out.files.push_back(currentFile);
    }
    
    return hasContent && !out.files.empty();
}

// ============================================================================
// Fuzzy Hunk Matching
// ============================================================================

bool HunkMatchesAt(const std::vector<std::wstring>& lines, const Hunk& hunk, int startIdx) {
    if (startIdx < 0 || startIdx >= (int)lines.size()) return false;
    
    int currentLine = startIdx;
    for (const auto& hl : hunk.lines) {
        if (hl.type == LineType::Context || hl.type == LineType::Remove) {
            if (currentLine >= (int)lines.size()) return false;
            if (lines[currentLine] != hl.text) return false;
            currentLine++;
        }
    }
    return true;
}

int FindBestHunkMatch(const std::vector<std::wstring>& lines, const Hunk& hunk, int targetIdx, int maxFuzz) {
    // Try exact match first
    if (HunkMatchesAt(lines, hunk, targetIdx)) {
        return targetIdx;
    }
    
    // Search outward from target
    for (int offset = 1; offset <= maxFuzz; ++offset) {
        if (HunkMatchesAt(lines, hunk, targetIdx + offset)) {
            return targetIdx + offset;
        }
        if (offset > 0 && HunkMatchesAt(lines, hunk, targetIdx - offset)) {
            return targetIdx - offset;
        }
    }
    
    return -1; // No match found
}

// ============================================================================
// Patch Application
// ============================================================================

bool ApplyPatchToFile(const FilePatch& fp, int maxFuzz) {
    // Handle new file
    if (fp.isNewFile) {
        std::vector<std::wstring> newLines;
        for (const auto& hunk : fp.hunks) {
            for (const auto& hl : hunk.lines) {
                if (hl.type == LineType::Add) {
                    newLines.push_back(hl.text);
                }
            }
        }
        return WriteFileLines(fp.newPath, newLines);
    }
    
    // Handle deleted file
    if (fp.isDeleted) {
        if (FileExists(fp.oldPath)) {
            fs::remove(fp.oldPath);
        }
        return true;
    }
    
    // Standard patch
    auto lines = ReadFileLines(fp.oldPath);
    if (lines.empty() && !FileExists(fp.oldPath)) {
        return false;
    }
    
    // Apply hunks in reverse order to maintain index integrity
    for (int i = (int)fp.hunks.size() - 1; i >= 0; --i) {
        const auto& hunk = fp.hunks[i];
        int targetIdx = hunk.oldStart - 1;
        int foundIdx = FindBestHunkMatch(lines, hunk, targetIdx, maxFuzz);
        
        if (foundIdx == -1) {
            return false; // Context mismatch even with fuzz
        }
        
        std::vector<std::wstring> newHunkLines;
        int cursor = foundIdx;
        
        for (const auto& hl : hunk.lines) {
            if (hl.type == LineType::Context) {
                newHunkLines.push_back(lines[cursor++]);
            }
            else if (hl.type == LineType::Remove) {
                cursor++; // Skip removed line
            }
            else if (hl.type == LineType::Add) {
                newHunkLines.push_back(hl.text);
            }
        }
        
        // Replace old lines with new hunk lines
        lines.erase(lines.begin() + foundIdx, lines.begin() + cursor);
        lines.insert(lines.begin() + foundIdx, newHunkLines.begin(), newHunkLines.end());
    }
    
    std::wstring outPath = fp.newPath.empty() ? fp.oldPath : fp.newPath;
    return WriteFileLines(outPath, lines);
}

bool ApplyPatchSet(const PatchSet& patch, PatchResult& result, int maxFuzz) {
    result = {};
    result.success = true;
    
    for (const auto& file : patch.files) {
        // Create backup before patching
        if (!file.isNewFile && FileExists(file.oldPath)) {
            if (!CreateBackup(file.oldPath)) {
                result.errorMessage = L"Failed to create backup for: " + file.oldPath;
                result.success = false;
                return false;
            }
        }
        
        if (ApplyPatchToFile(file, maxFuzz)) {
            result.filesPatched++;
            result.hunksApplied += (int)file.hunks.size();
            result.modifiedFiles.push_back(file.newPath.empty() ? file.oldPath : file.newPath);
        } else {
            result.hunksFailed += (int)file.hunks.size();
            result.errorMessage = L"Failed to apply patch to: " + file.oldPath;
            result.success = false;
            
            // Attempt rollback of previously patched files
            for (const auto& modified : result.modifiedFiles) {
                RestoreFromBackup(modified);
            }
            return false;
        }
    }
    
    return result.success;
}

} // namespace Agentic
} // namespace RawrXD
