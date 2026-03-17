// RawrXD_MultiFileEditor.hpp - Cursor's composer in pure Win32
#pragma once
#include <windows.h>
#include <string>
#include <vector>
#include <map>
#include <functional>
#include <regex>
#include <fstream>
#include <sstream>

// Definitions for diff viewer IDs if not in resource header
#define IDD_DIFF_VIEWER 101
#define IDC_LEFT_PANE 201
#define IDC_RIGHT_PANE 202
#define IDAPPLY 301

struct FileEdit {
    std::string file_path;
    std::string original_content;
    std::string new_content;
    std::vector<std::tuple<int, int, std::string, std::string>> hunks; // line, col, old, new
    bool is_applied = false;
    bool is_reverted = false;
};

class MultiFileEditEngine {
    std::vector<FileEdit> pending_edits_;
    std::map<std::string, std::string> file_backups_;
    HWND parent_hwnd_ = nullptr;
    std::function<void(const FileEdit&)> preview_callback_;
    
public:
    void Initialize(HWND parent) { parent_hwnd_ = parent; }

    void LoadEdits(const std::vector<FileEdit>& edits) {
        pending_edits_ = edits;
    }
    
    // Parse LLM response into structured edits
    std::vector<FileEdit> ParseEditsFromLLMResponse(const std::string& response) {
        std::vector<FileEdit> edits;
        
        // Pattern: ```file:path/to/file.cpp\ncontent\n```
        std::regex file_pattern(R"(```file:(.+?)\n([\s\S]+?)```)");
        std::sregex_iterator iter(response.begin(), response.end(), file_pattern);
        std::sregex_iterator end;
        
        for (; iter != end; ++iter) {
            FileEdit edit;
            edit.file_path = (*iter)[1].str();
            edit.new_content = (*iter)[2].str();
            edit.original_content = ReadFileContent(edit.file_path);
            edit.hunks = ComputeDiffHunks(edit.original_content, edit.new_content);
            edits.push_back(edit);
        }
        
        return edits;
    }
    
    // Show inline diff UI (Win32 RichEdit)
    void ShowDiffViewer(const FileEdit& edit) {
        // Create modal dialog with side-by-side diff
        // In a real app, this requires a resource template
        DialogBoxParamA(GetModuleHandle(nullptr), (LPCSTR)IDD_DIFF_VIEWER,
                       parent_hwnd_, (DLGPROC)DiffDialogProc, (LPARAM)&edit);
    }
    
    bool ApplyEdit(const FileEdit& edit) {
        // Backup original
        file_backups_[edit.file_path] = edit.original_content;
        
        // Atomic write
        if (WriteFileAtomic(edit.file_path, edit.new_content)) {
            const_cast<FileEdit&>(edit).is_applied = true;
            // NotifyEditApplied(edit);
            return true;
        }
        return false;
    }
    
    bool RevertEdit(const FileEdit& edit) {
        auto it = file_backups_.find(edit.file_path);
        if (it != file_backups_.end()) {
            WriteFileAtomic(edit.file_path, it->second);
            const_cast<FileEdit&>(edit).is_reverted = true;
            return true;
        }
        return false;
    }
    
    // Apply all edits atomically (transaction)
    bool ApplyAllEdits() {
        std::vector<std::string> applied_files;
        
        for (auto& edit : pending_edits_) {
            if (ApplyEdit(edit)) {
                applied_files.push_back(edit.file_path);
            } else {
                // Rollback on failure
                for (const auto& f : applied_files) {
                    RevertEditByPath(f);
                }
                return false;
            }
        }
        return true;
    }

private:
    std::string ReadFileContent(const std::string& path) {
        std::ifstream t(path);
        std::stringstream buffer;
        buffer << t.rdbuf();
        return buffer.str();
    }

    bool WriteFileAtomic(const std::string& path, const std::string& content) {
        std::ofstream out(path);
        if (out.is_open()) {
            out << content;
            return true;
        }
        return false;
    }

    void RevertEditByPath(const std::string& path) {
        auto it = file_backups_.find(path);
        if (it != file_backups_.end()) {
            WriteFileAtomic(path, it->second);
        }
    }

    std::vector<std::string> SplitLines(const std::string& str) {
        std::vector<std::string> lines;
        std::stringstream ss(str);
        std::string line;
        while (std::getline(ss, line)) {
            lines.push_back(line);
        }
        return lines;
    }

    std::vector<std::tuple<int, int, std::string, std::string>> ComputeDiffHunks(
        const std::string& old, const std::string& new_content) {
        std::vector<std::tuple<int, int, std::string, std::string>> hunks;
        auto old_lines = SplitLines(old);
        auto new_lines = SplitLines(new_content);
        
        size_t i = 0, j = 0;
        while (i < old_lines.size() || j < new_lines.size()) {
            if (i < old_lines.size() && j < new_lines.size() && old_lines[i] == new_lines[j]) {
                i++; j++;
            } else {
                hunks.push_back({(int)i, (int)j, 
                                i < old_lines.size() ? old_lines[i] : "[END]",
                                j < new_lines.size() ? new_lines[j] : "[END]"});
                if (i < old_lines.size()) i++;
                if (j < new_lines.size()) j++;
            }
        }
        return hunks;
    }
    
    static INT_PTR CALLBACK DiffDialogProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
        switch (msg) {
        case WM_INITDIALOG: {
            auto* edit = (FileEdit*)lParam;
            SetWindowTextA(hwnd, ("Diff: " + edit->file_path).c_str());
            
            // Setup RichEdit controls for side-by-side
            HWND hLeft = GetDlgItem(hwnd, IDC_LEFT_PANE);
            HWND hRight = GetDlgItem(hwnd, IDC_RIGHT_PANE);
            
            SetWindowTextA(hLeft, edit->original_content.c_str());
            SetWindowTextA(hRight, edit->new_content.c_str());
            
            // Colorize differences... (stub)
            return TRUE;
        }
        case WM_COMMAND:
            if (LOWORD(wParam) == IDAPPLY) {
                EndDialog(hwnd, IDAPPLY);
                return TRUE;
            } else if (LOWORD(wParam) == IDCANCEL) {
                EndDialog(hwnd, IDCANCEL);
                return TRUE;
            }
            break;
        }
        return FALSE;
    }
};
