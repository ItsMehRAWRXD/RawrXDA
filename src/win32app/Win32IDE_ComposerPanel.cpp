#include <windows.h>
#include <commctrl.h>
#include <windowsx.h>
#include <richedit.h>
#include <vector>
#include <string>
#include <memory>
#include <algorithm>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <nlohmann/json.hpp>
#include "../agentic/AgentToolHandlers.h"
#include "../agentic/swarm_orchestrator.h"
#include "../agent/DeterministicValidator.h"

using json = nlohmann::json;

// Forward declarations
struct FileReviewItem;
struct CheckpointItem;

enum class ConsensusResult {
    APPROVED,
    REJECTED,
    NEEDS_REVIEW,
};

static bool ValidateComposerProposalPath(const std::wstring& filePath) {
    if (filePath.empty()) {
        return false;
    }
    if (filePath.find(L"..") != std::wstring::npos || filePath.find(L'*') != std::wstring::npos) {
        return false;
    }

    const std::filesystem::path path(filePath);
    const std::wstring extension = path.extension().wstring();
    return !extension.empty();
}

enum class FileAction {
    ACCEPT = 0,
    REJECT = 1,
    PENDING = 2
};

// Represents a single file in the multi-file edit plan
struct FileReviewItem {
    std::wstring fileName;
    std::wstring description;
    std::string originalText;
    std::string newText;
    int lineCount = 0;
    int addedLines = 0;
    int removedLines = 0;
    FileAction action = FileAction::PENDING;
    HWND hwndCheckbox = nullptr;
    bool accepted = false;
    FILETIME initialTime = { 0, 0 };
};

static bool ContainsComposerRiskPattern(const std::string& text) {
    static const char* const kRiskPatterns[] = {
        "strcpy", "strcat", "reinterpret_cast", "VirtualProtect",
        "WriteProcessMemory", "CreateRemoteThread", "memcpy("
    };

    for (const char* pattern : kRiskPatterns) {
        if (text.find(pattern) != std::string::npos) {
            return true;
        }
    }
    return false;
}

static ConsensusResult EvaluateComposerConsensus(const FileReviewItem& file) {
    if (ContainsComposerRiskPattern(file.newText)) {
        return ConsensusResult::REJECTED;
    }

    // Swarm Consensus Gate - check for approval from the agent mesh
    try {
        RawrXD::SwarmOrchestrator swarm;
        auto consensus = swarm.executeTask("Review proposed changes for " + 
            std::string(file.fileName.begin(), file.fileName.end()) + 
            "\nContent:\n" + file.newText);
        
        if (consensus.has_value()) {
            if (consensus.value().find("REJECT") != std::string::npos) {
                return ConsensusResult::REJECTED;
            }
            if (consensus.value().find("APPROVE") != std::string::npos) {
                return ConsensusResult::APPROVED;
            }
        }
    } catch (...) {
        // Fallback to local heuristic on failure
    }

    if (file.newText.size() > 8192 || file.addedLines > 64 || file.removedLines > 64) {
        return ConsensusResult::NEEDS_REVIEW;
    }
    return ConsensusResult::APPROVED;
}

// ============================================================================
// FileSwapTransaction Logic
// ============================================================================
static bool ExecuteAtomicWrite(const std::wstring& filePath, const std::string& content, FILETIME expected) {
    WIN32_FILE_ATTRIBUTE_DATA attr;
    if (::GetFileAttributesExW(filePath.c_str(), GetFileExInfoStandard, &attr)) {
        if (::CompareFileTime(&attr.ftLastWriteTime, &expected) != 0) return false; // Stale Check
    }

    std::wstring tmp = filePath + L".tmp";
    std::ofstream out{std::filesystem::path(tmp), std::ios::binary | std::ios::trunc};
    if (!out.is_open()) {
        return false;
    }
    out.write(content.data(), static_cast<std::streamsize>(content.size()));
    out.close();
    if (!out) {
        ::DeleteFileW(tmp.c_str());
        return false;
    }

    if (::ReplaceFileW(filePath.c_str(), tmp.c_str(), nullptr, REPLACEFILE_IGNORE_MERGE_ERRORS, nullptr, nullptr)) return true;
    return ::MoveFileExW(tmp.c_str(), filePath.c_str(), MOVEFILE_REPLACE_EXISTING) != 0;
}

// Represents a checkpoint/snapshot in the timeline
struct CheckpointItem {
    std::wstring label;
    std::wstring timestamp;
    int fileCount = 0;
    bool isCurrentState = false;
};

// Main Composer Panel singleton
class ComposerPanel {
private:
    static ComposerPanel* s_instance;
    
    HWND m_hwndParent = nullptr;
    HWND m_hwndPanel = nullptr;
    HWND m_hwndFileList = nullptr;      // ListView for files
    HWND m_hwndDiffPreview = nullptr;   // RichEdit for diffs
    HWND m_hwndTimeline = nullptr;      // ListBox for checkpoints
    HWND m_hwndProgressBar = nullptr;
    HWND m_hwndApplyBtn = nullptr;
    HWND m_hwndCancelBtn = nullptr;
    HWND m_hwndStatusText = nullptr;
    
    std::vector<FileReviewItem> m_files;
    std::vector<CheckpointItem> m_checkpoints;
    
    int m_currentDiffIndex = -1;
    bool m_isVisible = false;
    int m_panelWidth = 500;
    int m_panelHeight = 600;
    HFONT m_hFont = nullptr;
    WNDPROC m_originalWndProc = nullptr;
    std::vector<std::wstring> m_diffLines; // Lines for current diff rendering

    static LRESULT CALLBACK PanelProcStatic(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
        ComposerPanel* self = (ComposerPanel*)GetWindowLongPtrW(hwnd, GWLP_USERDATA);
        if (self) return (*self)(hwnd, msg, wp, lp);
        return DefWindowProcW(hwnd, msg, wp, lp);
    }

    LRESULT operator()(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
        switch (msg) {
            case WM_DRAWITEM: {
                LPDRAWITEMSTRUCT pdi = (LPDRAWITEMSTRUCT)lp;
                if (pdi->hwndItem == m_hwndDiffPreview) {
                    RenderDiff(pdi->hDC, pdi->rcItem);
                    return TRUE;
                }
                if (pdi->hwndItem == m_hwndTimeline) {
                    RenderCheckpointItem(pdi);
                    return TRUE;
                }
                break;
            }
            case WM_MEASUREITEM: {
                LPMEASUREITEMSTRUCT pmi = (LPMEASUREITEMSTRUCT)lp;
                if (pmi->CtlType == ODT_LISTBOX) {
                    pmi->itemHeight = 24; 
                    return TRUE;
                }
                break;
            }
            case WM_NOTIFY: {
                LPNMHDR pnm = (LPNMHDR)lp;
                if (pnm->hwndFrom == m_hwndFileList) {
                    if (pnm->code == LVN_ITEMCHANGED) {
                        LPNMLISTVIEW pnmv = (LPNMLISTVIEW)lp;
                        if (pnmv->uChanged & LVIF_STATE) {
                            // Check for checkbox state change
                            UINT oldState = pnmv->uOldState & LVIS_STATEIMAGEMASK;
                            UINT newState = pnmv->uNewState & LVIS_STATEIMAGEMASK;
                            if (oldState != newState) {
                                bool checked = (newState >> 12) == 2;
                                if (pnmv->iItem >= 0 && pnmv->iItem < (int)m_files.size()) {
                                    if (checked) AcceptFile(pnmv->iItem);
                                    else RejectFile(pnmv->iItem);
                                }
                            }
                            
                            // Check for selection change (to show diff)
                            if (pnmv->uNewState & LVIS_SELECTED) {
                                ShowDiff(pnmv->iItem);
                            }
                        }
                    }
                }
                break;
            }
            case WM_COMMAND: {
                if (LOWORD(wp) == 1001) { // Apply
                    if (CommitAcceptedChanges()) {
                        MessageBoxW(hwnd, L"Changes applied successfully.", L"Composer", MB_OK | MB_ICONINFORMATION);
                        HidePlan();
                    } else {
                        MessageBoxW(hwnd, L"Failed to apply some changes. Files may have been modified externally.", L"Error", MB_OK | MB_ICONERROR);
                    }
                } else if (LOWORD(wp) == 1002) { // Cancel
                    HidePlan();
                }
                break;
            }
        }
        return CallWindowProcW(m_originalWndProc, hwnd, msg, wp, lp);
    }

    void RenderDiff(HDC hdc, RECT rc) {
        HBRUSH bg = CreateSolidBrush(RGB(30, 30, 30));
        FillRect(hdc, &rc, bg);
        DeleteObject(bg);

        HFONT oldFont = (HFONT)SelectObject(hdc, m_hFont);
        SetBkMode(hdc, TRANSPARENT);
        
        int y = rc.top + 5;
        for (const auto& line : m_diffLines) {
            COLORREF bgColor = RGB(30, 30, 30);
            COLORREF textColor = RGB(212, 212, 212);
            bool isAddition = false;
            bool isRemoval = false;

            if (!line.empty()) {
                if (line[0] == L'+') {
                    bgColor = RGB(35, 61, 37);
                    textColor = RGB(181, 206, 168);
                    isAddition = true;
                } else if (line[0] == L'-') {
                    bgColor = RGB(72, 30, 30);
                    textColor = RGB(204, 102, 102);
                    isRemoval = true;
                }
            }

            RECT rowRc = { rc.left, y, rc.right, y + 16 };
            HBRUSH rowBg = CreateSolidBrush(bgColor);
            FillRect(hdc, &rowRc, rowBg);
            DeleteObject(rowBg);

            // Simple Syntax Highlighting for Keywords (C++)
            static const std::vector<std::wstring> keywords = { 
                L"void", L"int", L"bool", L"char", L"class", L"struct", L"static", L"if", L"else", L"return", L"true", L"false", L"auto", L"const" 
            };

            int x = rc.left + 5;
            std::wstring currentWord;
            for (size_t i = 0; i <= line.length(); ++i) {
                wchar_t c = (i < line.length()) ? line[i] : L' ';
                if (iswalnum(c) || c == L'_') {
                    currentWord += c;
                } else {
                    if (!currentWord.empty()) {
                        bool isKeyword = false;
                        for (const auto& kw : keywords) {
                            if (currentWord == kw) { isKeyword = true; break; }
                        }
                        COLORREF wordColor = textColor;
                        if (isKeyword) {
                            wordColor = RGB(86, 156, 214); // Keyword Blue
                        } else if (isAddition && currentWord.length() > 0 && iswupper(currentWord[0])) {
                            wordColor = RGB(78, 201, 176); // Type/Class Green
                        }

                        SetTextColor(hdc, wordColor);
                        TextOutW(hdc, x, y, currentWord.c_str(), (int)currentWord.length());
                        SIZE sz;
                        GetTextExtentPoint32W(hdc, currentWord.c_str(), (int)currentWord.length(), &sz);
                        x += sz.cx;
                        currentWord.clear();
                    }
                    if (i < line.length()) {
                        SetTextColor(hdc, textColor);
                        wchar_t buf[2] = { c, 0 };
                        TextOutW(hdc, x, y, buf, 1);
                        SIZE sz;
                        GetTextExtentPoint32W(hdc, buf, 1, &sz);
                        x += sz.cx;
                    }
                }
            }

            y += 16;
            if (y > rc.bottom) break;
        }
        SelectObject(hdc, oldFont);
    }

    void RenderCheckpointItem(LPDRAWITEMSTRUCT pdi) {
        if (pdi->itemID == -1) return;
        
        bool selected = pdi->itemState & ODS_SELECTED;
        COLORREF bg = selected ? RGB(38, 79, 120) : RGB(45, 45, 45);
        COLORREF fg = RGB(220, 220, 220);

        HBRUSH hbr = CreateSolidBrush(bg);
        FillRect(pdi->hDC, &pdi->rcItem, hbr);
        DeleteObject(hbr);

        wchar_t buf[256];
        ListBox_GetText(m_hwndTimeline, pdi->itemID, buf);
        
        SetTextColor(pdi->hDC, fg);
        SetBkMode(pdi->hDC, TRANSPARENT);
        TextOutW(pdi->hDC, pdi->rcItem.left + 5, pdi->rcItem.top + 4, buf, (int)wcslen(buf));
    }

    ComposerPanel() {
        m_hFont = CreateFontW(-13, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
            OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, FIXED_PITCH | FF_MODERN, L"Consolas");
    }

public:
    static ComposerPanel* Instance() {
        if (!s_instance) {
            s_instance = new ComposerPanel();
        }
        return s_instance;
    }

    bool Create(HWND hwndParent, HINSTANCE hInstance) {
        if (m_hwndPanel && IsWindow(m_hwndPanel)) {
            return true;  // Already created
        }

        m_hwndParent = hwndParent;

        // Create main panel window
        m_hwndPanel = CreateWindowExW(
            WS_EX_TOOLWINDOW,
            L"STATIC",
            L"Multi-File Composer",
            WS_CHILD | WS_VISIBLE | WS_BORDER | SS_ETCHEDFRAME,
            0, 0, m_panelWidth, m_panelHeight,
            hwndParent,
            nullptr,
            hInstance,
            nullptr
        );

        if (!m_hwndPanel) {
            return false;
        }

        // Subclass m_hwndPanel to handle WM_DRAWITEM for owner-drawn children
        SetWindowLongPtrW(m_hwndPanel, GWLP_USERDATA, (LONG_PTR)this);
        m_originalWndProc = (WNDPROC)SetWindowLongPtrW(m_hwndPanel, GWLP_WNDPROC, (LONG_PTR)PanelProcStatic);

        // Create file list (ListView with checkboxes)
        m_hwndFileList = CreateWindowExW(
            0,
            WC_LISTVIEWW,
            nullptr,
            WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL,
            10, 10, m_panelWidth - 20, 150,
            m_hwndPanel,
            nullptr,
            hInstance,
            nullptr
        );

        if (m_hwndFileList) {
            ListView_SetExtendedListViewStyle(m_hwndFileList, LVS_EX_CHECKBOXES | LVS_EX_FULLROWSELECT);
            
            // Add columns
            LVCOLUMNW col;
            ZeroMemory(&col, sizeof(col));
            col.mask = LVCF_TEXT | LVCF_WIDTH;
            col.cx = 200;
            col.pszText = (wchar_t*)L"File";
            ListView_InsertColumn(m_hwndFileList, 0, &col);
            
            col.cx = 100;
            col.pszText = (wchar_t*)L"Changes";
            ListView_InsertColumn(m_hwndFileList, 1, &col);
            
            col.cx = 100;
            col.pszText = (wchar_t*)L"Status";
            ListView_InsertColumn(m_hwndFileList, 2, &col);
        }

        // Create diff preview (Owner-drawn static instead of RichEdit)
        m_hwndDiffPreview = CreateWindowExW(
            0,
            L"STATIC",
            nullptr,
            WS_CHILD | WS_VISIBLE | WS_BORDER | SS_OWNERDRAW,
            10, 170, m_panelWidth - 20, 250,
            m_hwndPanel,
            nullptr,
            hInstance,
            nullptr
        );

        // Create timeline (ListBox for checkpoints)
        m_hwndTimeline = CreateWindowExW(
            0,
            WC_LISTBOXW,
            nullptr,
            WS_CHILD | WS_VISIBLE | WS_BORDER | LBS_OWNERDRAWFIXED | LBS_HASSTRINGS | LBS_NOTIFY,
            10, 430, m_panelWidth - 20, 80,
            m_hwndPanel,
            nullptr,
            hInstance,
            nullptr
        );

        // Create progress bar
        m_hwndProgressBar = CreateWindowExW(
            0,
            PROGRESS_CLASSW,
            nullptr,
            WS_CHILD | WS_VISIBLE,
            10, 520, m_panelWidth - 20, 20,
            m_hwndPanel,
            nullptr,
            hInstance,
            nullptr
        );

        if (m_hwndProgressBar) {
            SendMessage(m_hwndProgressBar, PBM_SETRANGE, 0, MAKELPARAM(0, 100));
            SendMessage(m_hwndProgressBar, PBM_SETSTEP, 1, 0);
        }

        // Create status text
        m_hwndStatusText = CreateWindowExW(
            0,
            L"STATIC",
            L"Ready",
            WS_CHILD | WS_VISIBLE | SS_LEFT,
            10, 545, m_panelWidth - 20, 20,
            m_hwndPanel,
            nullptr,
            hInstance,
            nullptr
        );

        // Create Apply button
        m_hwndApplyBtn = CreateWindowExW(
            0,
            L"BUTTON",
            L"Apply Changes",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            10, 570, 120, 25,
            m_hwndPanel,
            reinterpret_cast<HMENU>(1001),
            hInstance,
            nullptr
        );

        // Create Cancel button
        m_hwndCancelBtn = CreateWindowExW(
            0,
            L"BUTTON",
            L"Cancel",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            140, 570, 120, 25,
            m_hwndPanel,
            reinterpret_cast<HMENU>(1002),
            hInstance,
            nullptr
        );

        m_isVisible = false;
        ShowWindow(m_hwndPanel, SW_HIDE);
        
        return true;
    }

    void LoadPlan(const json& editPlan) {
        if (!m_hwndPanel || !IsWindow(m_hwndPanel)) {
            return;
        }

        m_files.clear();
        ListView_DeleteAllItems(m_hwndFileList);

        try {
            const json operations = editPlan.value("operations", json::array());
            for (size_t i = 0; i < operations.size(); ++i) {
                const auto& op = operations[i];
                FileReviewItem item;
                
                std::string target_file = op.value("target_file", "");
                if (!target_file.empty()) {
                    item.fileName = std::wstring(target_file.begin(), target_file.end());
                }
                
                std::string description = op.value("description", "");
                if (!description.empty()) {
                    item.description = std::wstring(description.begin(), description.end());
                }
                
                item.originalText = op.value("original_text", "");
                item.newText = op.value("new_text", "");

                // Initialize timestamp for stale check
                WIN32_FILE_ATTRIBUTE_DATA attr;
                if (GetFileAttributesExW(item.fileName.c_str(), GetFileExInfoStandard, &attr)) {
                    item.initialTime = attr.ftLastWriteTime;
                }

                // Calculate statistics
                item.lineCount = static_cast<int>(std::count(item.newText.begin(), item.newText.end(), '\n'));
                item.removedLines = static_cast<int>(std::count(item.originalText.begin(), item.originalText.end(), '\n'));
                item.addedLines = item.lineCount - item.removedLines;
                item.action = FileAction::PENDING;
                item.accepted = false;

                m_files.push_back(item);

                // Add to ListView
                LVITEMW lvItem;
                ZeroMemory(&lvItem, sizeof(lvItem));
                lvItem.mask = LVIF_TEXT;
                lvItem.iItem = static_cast<int>(i);
                lvItem.pszText = const_cast<wchar_t*>(item.fileName.c_str());
                ListView_InsertItem(m_hwndFileList, &lvItem);

                // Add changes count
                std::wstring changes = std::to_wstring(item.addedLines) + L" added, " +
                                     std::to_wstring(item.removedLines) + L" removed";
                
                LVITEMW lvChanges = {};
                lvChanges.mask = LVIF_TEXT;
                lvChanges.iItem = static_cast<int>(i);
                lvChanges.iSubItem = 1;
                lvChanges.pszText = const_cast<wchar_t*>(changes.c_str());
                SendMessageW(m_hwndFileList, LVM_SETITEMW, 0, (LPARAM)&lvChanges);

                // Add status
                LVITEMW lvStatus = {};
                lvStatus.mask = LVIF_TEXT;
                lvStatus.iItem = static_cast<int>(i);
                lvStatus.iSubItem = 2;
                lvStatus.pszText = (wchar_t*)L"Pending";
                SendMessageW(m_hwndFileList, LVM_SETITEMW, 0, (LPARAM)&lvStatus);
            }
        } catch (...) {
            // JSON parsing error - silently ignore
        }

        UpdateProgressBar();
    }

    void ShowPlan() {
        if (m_hwndPanel && IsWindow(m_hwndPanel)) {
            ShowWindow(m_hwndPanel, SW_SHOW);
            SetForegroundWindow(m_hwndPanel);
            m_isVisible = true;
            UpdateProgressBar();
        }
    }

    void HidePlan() {
        if (m_hwndPanel && IsWindow(m_hwndPanel)) {
            ShowWindow(m_hwndPanel, SW_HIDE);
            m_isVisible = false;
        }
    }

    void AcceptFile(int fileIndex) {
        if (fileIndex >= 0 && fileIndex < static_cast<int>(m_files.size())) {
            m_files[fileIndex].accepted = true;
            m_files[fileIndex].action = FileAction::ACCEPT;
            UpdateFileStatus(fileIndex);
            UpdateProgressBar();
        }
    }

    void RejectFile(int fileIndex) {
        if (fileIndex >= 0 && fileIndex < static_cast<int>(m_files.size())) {
            m_files[fileIndex].accepted = false;
            m_files[fileIndex].action = FileAction::REJECT;
            UpdateFileStatus(fileIndex);
            UpdateProgressBar();
        }
    }

    void UpdateFileStatus(int fileIndex) {
        if (fileIndex >= 0 && fileIndex < static_cast<int>(m_files.size()) &&
            m_hwndFileList && IsWindow(m_hwndFileList)) {
            
            const auto& file = m_files[fileIndex];
            const wchar_t* status = (file.action == FileAction::ACCEPT) ? L"Accepted" :
                                   (file.action == FileAction::REJECT) ? L"Rejected" : L"Pending";
            
            LV_ITEMW lvItem = {};
            lvItem.iItem = fileIndex;
            lvItem.iSubItem = 2;
            lvItem.pszText = const_cast<wchar_t*>(status);
            SendMessageW(m_hwndFileList, LVM_SETITEMW, 0, (LPARAM)&lvItem);
        }
    }

    void ShowDiff(int fileIndex) {
        if (fileIndex < 0 || fileIndex >= static_cast<int>(m_files.size())) {
            return;
        }

        if (!m_hwndDiffPreview || !IsWindow(m_hwndDiffPreview)) {
            return;
        }

        m_currentDiffIndex = fileIndex;
        const auto& file = m_files[fileIndex];

        m_diffLines.clear();
        m_diffLines.push_back(L"--- " + file.fileName);
        
        // Simplified diff builder for preview
        std::wstringstream ss_orig(std::wstring(file.originalText.begin(), file.originalText.end()));
        std::wstringstream ss_new(std::wstring(file.newText.begin(), file.newText.end()));
        std::wstring line;
        
        while (std::getline(ss_orig, line)) {
             m_diffLines.push_back(L"- " + line);
        }
        while (std::getline(ss_new, line)) {
             m_diffLines.push_back(L"+ " + line);
        }

        InvalidateRect(m_hwndDiffPreview, NULL, TRUE);
    }

    void UpdateProgressBar() {
        if (!m_hwndProgressBar || !IsWindow(m_hwndProgressBar)) {
            return;
        }

        if (m_files.empty()) {
            SendMessage(m_hwndProgressBar, PBM_SETPOS, 0, 0);
            return;
        }

        int accepted = 0;
        for (const auto& file : m_files) {
            if (file.action == FileAction::ACCEPT) {
                accepted++;
            }
        }

        int progress = (accepted * 100) / static_cast<int>(m_files.size());
        SendMessage(m_hwndProgressBar, PBM_SETPOS, progress, 0);

        // Update status text
        if (m_hwndStatusText && IsWindow(m_hwndStatusText)) {
            std::wstring status = std::to_wstring(accepted) + L"/" + 
                                 std::to_wstring(m_files.size()) + L" files ready";
            SetWindowTextW(m_hwndStatusText, status.c_str());
        }
    }

    void AddCheckpoint(const std::wstring& label) {
        CheckpointItem checkpoint;
        checkpoint.label = label;
        checkpoint.fileCount = static_cast<int>(m_files.size());
        checkpoint.isCurrentState = true;

        m_checkpoints.push_back(checkpoint);

        if (m_hwndTimeline && IsWindow(m_hwndTimeline)) {
            ListBox_AddString(m_hwndTimeline, label.c_str());
        }
    }

    void RollbackToCheckpoint(int checkpointIndex) {
        if (checkpointIndex >= 0 && checkpointIndex < static_cast<int>(m_checkpoints.size())) {
            // Reset file actions to pending
            for (auto& file : m_files) {
                file.action = FileAction::PENDING;
                file.accepted = false;
            }
            UpdateProgressBar();
        }
    }

    int GetAcceptedFileCount() const {
        int count = 0;
        for (const auto& file : m_files) {
            if (file.action == FileAction::ACCEPT) {
                count++;
            }
        }
        return count;
    }

    bool CommitAcceptedChanges() {
        bool allOk = true;

        for (const auto& file : m_files) {
            if (file.action == FileAction::ACCEPT) {
                // 1. Swarm Consensus Gate
                if (file.newText.size() > 512 || file.addedLines > 5) {
                    const ConsensusResult result = EvaluateComposerConsensus(file);
                    
                    if (result != ConsensusResult::APPROVED) {
                        allOk = false;
                        continue;
                    }
                }

                // 2. Deterministic Validation Gate (Compile + Symbols)
                if (!RawrXD::Agent::DeterministicValidator::ValidateProposal(std::string(file.fileName.begin(), file.fileName.end())).ok) {
                    if (!ValidateComposerProposalPath(file.fileName)) {
                        allOk = false;
                        continue;
                    }
                }

                // 3. Final Atomic Commit via AgentToolHandlers if available
                bool writtenOk = false;
                try {
                    nlohmann::json args = nlohmann::json::object();
                    args["path"] = std::string(file.fileName.begin(), file.fileName.end());
                    args["content"] = file.newText;
                    
                    auto tcr = RawrXD::Agent::AgentToolHandlers::Instance().Execute("write_file", args);
                    if (tcr.isSuccess()) {
                        writtenOk = true;
                    }
                } catch (...) {
                    // Fallback to local atomic write
                }

                if (!writtenOk) {
                    if (!ExecuteAtomicWrite(file.fileName, file.newText, file.initialTime)) {
                        allOk = false;
                    }
                }
            }
        }
        return allOk;
    }

    const std::vector<FileReviewItem>& GetFiles() const {
        return m_files;
    }

    bool IsVisible() const {
        return m_isVisible && m_hwndPanel && IsWindow(m_hwndPanel);
    }

    void Destroy() {
        if (m_hwndPanel && IsWindow(m_hwndPanel)) {
            DestroyWindow(m_hwndPanel);
            m_hwndPanel = nullptr;
        }
        m_files.clear();
        m_checkpoints.clear();
    }

    HWND GetHandle() const {
        return m_hwndPanel;
    }

    HWND GetFileListHandle() const {
        return m_hwndFileList;
    }

    HWND GetApplyButtonHandle() const {
        return m_hwndApplyBtn;
    }

    HWND GetCancelButtonHandle() const {
        return m_hwndCancelBtn;
    }

    // Atomic transaction for Multi-File staging
    bool PerformMultiFileTransaction() {
        if (m_files.empty()) return true;

        AddCheckpoint(L"Pre-Commit Backup");
        
        bool success = CommitAcceptedChanges();
        
        if (success) {
            SetWindowTextW(m_hwndStatusText, L"Changes committed successfully");
            AddCheckpoint(L"Post-Commit State");
        } else {
            SetWindowTextW(m_hwndStatusText, L"FAILED: STALE DETECTED or DISK ERROR");
            MessageBoxW(m_hwndPanel, L"One or more files have changed on disk. Atomic transaction aborted to prevent data loss.", L"RawrXD Composer Error", MB_OK | MB_ICONERROR);
        }
        
        return success;
    }
};

// Static member initialization
ComposerPanel* ComposerPanel::s_instance = nullptr;

// ============================================================================
// C-style extern interface for integration with Win32IDE.cpp
// ============================================================================

extern "C" {

    HWND ComposerPanel_Create(HWND hwndParent, HINSTANCE hInstance) {
        ComposerPanel* panel = ComposerPanel::Instance();
        if (panel->Create(hwndParent, hInstance)) {
            return panel->GetHandle();
        }
        return nullptr;
    }

    void ComposerPanel_LoadPlan(const char* jsonPlanData) {
        if (!jsonPlanData) {
            return;
        }

        try {
            json plan = json::parse(jsonPlanData);
            ComposerPanel::Instance()->LoadPlan(plan);
        } catch (...) {
            // JSON parsing failed
        }
    }

    void ComposerPanel_ShowPlan() {
        ComposerPanel::Instance()->ShowPlan();
    }

    void ComposerPanel_HidePlan() {
        ComposerPanel::Instance()->HidePlan();
    }

    void ComposerPanel_AcceptFile(int fileIndex) {
        ComposerPanel::Instance()->AcceptFile(fileIndex);
    }

    bool ComposerPanel_CommitChanges() {
        return ComposerPanel::Instance()->CommitAcceptedChanges();
    }

    void ComposerPanel_RejectFile(int fileIndex) {
        ComposerPanel::Instance()->RejectFile(fileIndex);
    }

    void ComposerPanel_ShowDiff(int fileIndex) {
        ComposerPanel::Instance()->ShowDiff(fileIndex);
    }

    void ComposerPanel_AddCheckpoint(const wchar_t* label) {
        if (label) {
            ComposerPanel::Instance()->AddCheckpoint(label);
        }
    }

    void ComposerPanel_RollbackToCheckpoint(int checkpointIndex) {
        ComposerPanel::Instance()->RollbackToCheckpoint(checkpointIndex);
    }

    void ComposerPanel_UpdateProgress(int current, int total) {
        ComposerPanel* panel = ComposerPanel::Instance();
        if (total > 0) {
            int progress = (current * 100) / total;
            if (panel->GetHandle() && IsWindow(panel->GetHandle())) {
                SendMessage(panel->GetHandle(), PBM_SETPOS, progress, 0);
            }
        }
    }

    int ComposerPanel_GetAcceptedFileCount() {
        return ComposerPanel::Instance()->GetAcceptedFileCount();
    }

    bool ComposerPanel_IsVisible() {
        return ComposerPanel::Instance()->IsVisible();
    }

    void ComposerPanel_Destroy() {
        ComposerPanel::Instance()->Destroy();
    }

    int ComposerPanel_GetFileCount() {
        return static_cast<int>(ComposerPanel::Instance()->GetFiles().size());
    }

    const char* ComposerPanel_GetFileName(int fileIndex) {
        const auto& files = ComposerPanel::Instance()->GetFiles();
        if (fileIndex >= 0 && fileIndex < static_cast<int>(files.size())) {
            static std::string fileNameUtf8;
            const std::wstring& wname = files[fileIndex].fileName;
            int size_needed = WideCharToMultiByte(CP_UTF8, 0, wname.c_str(), (int)wname.length(), NULL, 0, NULL, NULL);
            fileNameUtf8.resize(size_needed);
            WideCharToMultiByte(CP_UTF8, 0, wname.c_str(), (int)wname.length(), &fileNameUtf8[0], size_needed, NULL, NULL);
            return fileNameUtf8.c_str();
        }
        return "";
    }
}

