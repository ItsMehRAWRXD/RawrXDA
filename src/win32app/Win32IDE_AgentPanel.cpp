// ============================================================================
// Win32IDE_AgentPanel.cpp — Multi-File Agent Edit Session (Cmd+K Experience)
// ============================================================================
// Provides the "Agent Mode" panel for RawrXD Win32IDE:
//   - Session state: list of files in working set
//   - Diff view: side-by-side proposed changes
//   - Accept/Reject: per-hunk granularity
//   - Provenance: full edit log with agent reasoning
//
// Architecture:
//   AgentEditSession  — Manages in-memory staged edits (never direct disk write)
//   AgentPanelUI      — Win32 panel rendering (diff hunks, buttons)
//   AgentEditLog      — JSON provenance log for each session
//
// Key safety invariant:
//   ALL edits are staged in memory. NOTHING touches disk until explicit Accept.
//   Accepted hunks are written individually. Rejected hunks are discarded.
//
// Pattern: PatchResult-style, no exceptions, factory results.
// Rule:    NO SOURCE FILE IS TO BE SIMPLIFIED.
// ============================================================================

#include "Win32IDE.h"
#include "IDELogger.h"
#include "IDEConfig.h"

#include "../agentic/ToolCallResult.h"
#include "../agentic/DiffEngine.h"
#include "../agentic/AgentTranscript.h"
#include "../agentic/BoundedAgentLoop.h"

#include <fstream>
#include <sstream>
#include <algorithm>
#include <filesystem>

namespace fs = std::filesystem;
using namespace RawrXD::Agent;
using namespace RawrXD::Diff;

// ============================================================================
// STRUCTURES
// ============================================================================

struct FileEdit {
    std::string path;
    std::string originalContent;
    std::string proposedContent;
    DiffResult diff;
    std::string agentReasoning;
    std::vector<std::string> toolsUsed;
    bool fullyAccepted = false;
    bool fullyRejected = false;
};

struct AgentEditLog {
    std::string sessionId;
    std::string timestamp;
    std::string userPrompt;
    std::vector<nlohmann::json> editEntries;

    void AddEntry(const FileEdit& edit, const std::string& action) {
        nlohmann::json entry;
        entry["file"] = edit.path;
        entry["action"] = action;
        entry["additions"] = edit.diff.additions;
        entry["deletions"] = edit.diff.deletions;
        entry["hunks"] = static_cast<int>(edit.diff.hunks.size());
        entry["reasoning"] = edit.agentReasoning;
        entry["tools_used"] = edit.toolsUsed;
        editEntries.push_back(entry);
    }

    nlohmann::json toJson() const {
        nlohmann::json j;
        j["session_id"] = sessionId;
        j["timestamp"] = timestamp;
        j["user_prompt"] = userPrompt;
        j["edits"] = editEntries;
        return j;
    }
};

// ============================================================================
// AGENT EDIT SESSION — in-memory edit staging
// ============================================================================

class AgentEditSession {
public:
    static constexpr size_t MAX_FILE_SIZE = 100 * 1024 * 1024; // 100 MB limit
    static constexpr size_t MAX_EDITS = 1000;  // Maximum edits per session
    
    AgentEditSession() {
        m_log.sessionId = "edit-" + std::to_string(GetTickCount64());
        m_log.timestamp = GetTimestamp();
    }

    void SetUserPrompt(const std::string& prompt) {
        m_log.userPrompt = prompt;
    }

    // ---- Propose an edit (search+replace in file) ----
    bool ProposeEdit(const std::string& path,
                     const std::string& search,
                     const std::string& replace,
                     const std::string& reasoning = "") {
        // Reject empty search string (prevents infinite loops / bad diffs)
        if (search.empty()) {
            std::cerr << "[AgentPanel] ProposeEdit: search string is empty (file: " << path << ")" << std::endl;
            return false;
        }
        
        // Reject too many edits in one session
        if (m_edits.size() >= MAX_EDITS) {
            std::cerr << "[AgentPanel] ProposeEdit: edit session limit exceeded (" << MAX_EDITS << ")" << std::endl;
            return false;
        }
        
        // Read original file with size validation
        std::ifstream file(path, std::ios::binary);
        if (!file.is_open()) {
            std::cerr << "[AgentPanel] ProposeEdit: failed to open " << path << " (error " << GetLastError() << ")" << std::endl;
            return false;
        }
        
        // Check file size before reading
        std::streamoff fileSize = 0;
        file.seekg(0, std::ios::end);
        fileSize = file.tellg();
        file.seekg(0, std::ios::beg);
        
        if (fileSize < 0 || static_cast<size_t>(fileSize) > MAX_FILE_SIZE) {
            std::cerr << "[AgentPanel] ProposeEdit: file too large (" << fileSize 
                      << " bytes, max " << MAX_FILE_SIZE << ")" << std::endl;
            file.close();
            return false;
        }
        
        std::ostringstream ss;
        ss << file.rdbuf();
        file.close();
        std::string original = ss.str();

        // Compute proposed content
        size_t pos = original.find(search);
        if (pos == std::string::npos) {
            std::cerr << "[AgentPanel] ProposeEdit: search string not found in " << path << std::endl;
            return false;
        }

        std::string proposed = original.substr(0, pos) + replace +
                               original.substr(pos + search.size());

        // Compute diff
        FileEdit edit;
        edit.path = path;
        edit.originalContent = original;
        edit.proposedContent = proposed;
        edit.diff = DiffEngine::ComputeDiff(original, proposed);
        edit.agentReasoning = reasoning;

        m_edits.push_back(edit);
        return true;
    }

    // ---- Propose a full file write ----
    bool ProposeNewFile(const std::string& path,
                        const std::string& content,
                        const std::string& reasoning = "") {
        // Validate session edit count
        if (m_edits.size() >= MAX_EDITS) {
            std::cerr << "[AgentPanel] ProposeNewFile: edit session limit exceeded (" << MAX_EDITS << ")" << std::endl;
            return false;
        }
        
        // Validate content size
        if (content.size() > MAX_FILE_SIZE) {
            std::cerr << "[AgentPanel] ProposeNewFile: content too large (" << content.size() 
                      << " bytes, max " << MAX_FILE_SIZE << ")" << std::endl;
            return false;
        }
        
        FileEdit edit;
        edit.path = path;
        edit.originalContent = ""; // New file
        edit.proposedContent = content;
        edit.diff = DiffEngine::ComputeDiff("", content);
        edit.agentReasoning = reasoning;

        m_edits.push_back(edit);
        return true;
    }

    // ---- Accept/Reject individual hunks ----
    bool AcceptHunk(size_t editIndex, size_t hunkIndex) {
        if (editIndex >= m_edits.size()) return false;
        if (hunkIndex >= m_edits[editIndex].diff.hunks.size()) return false;

        m_edits[editIndex].diff.hunks[hunkIndex].accepted = true;
        m_edits[editIndex].diff.hunks[hunkIndex].rejected = false;
        return true;
    }

    bool RejectHunk(size_t editIndex, size_t hunkIndex) {
        if (editIndex >= m_edits.size()) return false;
        if (hunkIndex >= m_edits[editIndex].diff.hunks.size()) return false;

        m_edits[editIndex].diff.hunks[hunkIndex].rejected = true;
        m_edits[editIndex].diff.hunks[hunkIndex].accepted = false;
        return true;
    }

    // ---- Accept/Reject entire file edit ----
    void AcceptAll(size_t editIndex) {
        if (editIndex >= m_edits.size()) return;
        for (auto& hunk : m_edits[editIndex].diff.hunks) {
            hunk.accepted = true;
            hunk.rejected = false;
        }
        m_edits[editIndex].fullyAccepted = true;
    }

    void RejectAll(size_t editIndex) {
        if (editIndex >= m_edits.size()) return;
        for (auto& hunk : m_edits[editIndex].diff.hunks) {
            hunk.rejected = true;
            hunk.accepted = false;
        }
        m_edits[editIndex].fullyRejected = true;
    }

    // ---- Apply accepted edits to disk ----
    int ApplyAccepted() {
        int appliedCount = 0;

        for (auto& edit : m_edits) {
            if (edit.fullyRejected) {
                m_log.AddEntry(edit, "rejected");
                continue;
            }

            // Calculate final content based on accepted hunks
            std::string finalContent;

            if (edit.fullyAccepted) {
                finalContent = edit.proposedContent;
            } else {
                // Apply only accepted hunks
                finalContent = DiffEngine::ApplyAcceptedHunks(
                    edit.originalContent, edit.diff.hunks);
            }

            // Check if anything changed
            if (finalContent == edit.originalContent) {
                m_log.AddEntry(edit, "no_change");
                continue;
            }

            // Validate final content size before write
            if (finalContent.size() > MAX_FILE_SIZE) {
                std::cerr << "[AgentPanel] ApplyAccepted: final content too large for " << edit.path 
                          << " (" << finalContent.size() << " bytes, max " << MAX_FILE_SIZE << ")" << std::endl;
                m_log.AddEntry(edit, "size_exceeded");
                continue;
            }

            // Create backup before writing
            if (fs::exists(edit.path)) {
                std::string backupPath = edit.path + ".pre_agent_bak";
                try {
                    fs::copy_file(edit.path, backupPath,
                                  fs::copy_options::overwrite_existing);
                    std::cout << "[AgentPanel] Backup created: " << backupPath << std::endl;
                } catch (const std::filesystem::filesystem_error& e) {
                    std::cerr << "[AgentPanel] WARNING: Failed to create backup for " << edit.path 
                              << ": " << e.what() << std::endl;
                    // Continue anyway; this isn't fatal
                }
            }

            // Ensure parent directories exist with error context
            try {
                fs::path parentDir = fs::path(edit.path).parent_path();
                if (!parentDir.empty() && !fs::exists(parentDir)) {
                    fs::create_directories(parentDir);
                    std::cout << "[AgentPanel] Created directory: " << parentDir.string() << std::endl;
                }
            } catch (const std::filesystem::filesystem_error& e) {
                std::cerr << "[AgentPanel] ERROR: Failed to create parent directory for " << edit.path 
                          << ": " << e.what() << std::endl;
                m_log.AddEntry(edit, "mkdir_failed");
                continue;
            }

            // Write to disk with explicit error handling
            std::ofstream outFile(edit.path, std::ios::trunc | std::ios::binary);
            if (!outFile.is_open()) {
                std::cerr << "[AgentPanel] ERROR: Failed to open " << edit.path << " for writing"
                          << " (error " << GetLastError() << ")" << std::endl;
                m_log.AddEntry(edit, "open_failed");
                continue;
            }
            
            try {
                outFile.write(finalContent.data(), finalContent.size());
                if (outFile.fail()) {
                    std::cerr << "[AgentPanel] ERROR: Write failed for " << edit.path 
                              << " (wrote " << outFile.tellp() << " of " << finalContent.size() << " bytes)" << std::endl;
                    outFile.close();
                    m_log.AddEntry(edit, "write_failed");
                    continue;
                }
                outFile.close();
                ++appliedCount;
                m_log.AddEntry(edit, "applied");
                std::cout << "[AgentPanel] Applied: " << edit.path << std::endl;
            } catch (const std::exception& e) {
                std::cerr << "[AgentPanel] ERROR: Exception writing " << edit.path 
                          << ": " << e.what() << std::endl;
                outFile.close();
                m_log.AddEntry(edit, "exception");
            }
        }

        std::cout << "[AgentPanel] ApplyAccepted complete: " << appliedCount 
                  << " of " << m_edits.size() << " applied" << std::endl;
        return appliedCount;
    }

    // ---- Accessors ----
    size_t EditCount() const { return m_edits.size(); }
    const FileEdit* GetEdit(size_t index) const {
        return (index < m_edits.size()) ? &m_edits[index] : nullptr;
    }

    const AgentEditLog& GetLog() const { return m_log; }

    bool SaveLog(const std::string& path) const {
        // Ensure parent directory exists
        try {
            fs::path logDir = fs::path(path).parent_path();
            if (!logDir.empty() && !fs::exists(logDir)) {
                fs::create_directories(logDir);
            }
        } catch (const std::filesystem::filesystem_error& e) {
            std::cerr << "[AgentPanel] Failed to create log directory: " << e.what() << std::endl;
            return false;
        }
        
        std::ofstream file(path);
        if (!file.is_open()) {
            std::cerr << "[AgentPanel] Failed to open log file for writing: " << path 
                      << " (error " << GetLastError() << ")" << std::endl;
            return false;
        }
        
        try {
            file << m_log.toJson().dump(2);
            if (file.fail()) {
                std::cerr << "[AgentPanel] Write failed for log file: " << path << std::endl;
                file.close();
                return false;
            }
            file.close();
            std::cout << "[AgentPanel] Saved edit log: " << path << std::endl;
            return true;
        } catch (const std::exception& e) {
            std::cerr << "[AgentPanel] Exception writing log file: " << e.what() << std::endl;
            file.close();
            return false;
        }
    }

    // ---- Summary for display ----
    std::string GetSummary() const {
        std::ostringstream ss;
        ss << "Agent Edit Session: " << m_edits.size() << " file(s)\n";
        for (size_t i = 0; i < m_edits.size(); ++i) {
            const auto& edit = m_edits[i];
            ss << "\n[" << (i + 1) << "] " << edit.path << "\n";
            ss << "    +" << edit.diff.additions << " -" << edit.diff.deletions
               << " (" << edit.diff.hunks.size() << " hunks)\n";

            if (!edit.agentReasoning.empty()) {
                ss << "    Reasoning: " << edit.agentReasoning.substr(0, 120) << "\n";
            }

            for (size_t h = 0; h < edit.diff.hunks.size(); ++h) {
                const auto& hunk = edit.diff.hunks[h];
                ss << "    Hunk " << (h + 1) << ": " << hunk.Header();
                if (hunk.accepted) ss << " [ACCEPTED]";
                else if (hunk.rejected) ss << " [REJECTED]";
                else ss << " [PENDING]";
                ss << "\n";
            }
        }
        return ss.str();
    }

private:
    std::string GetTimestamp() const {
        SYSTEMTIME st;
        GetSystemTime(&st);
        char buf[64];
        snprintf(buf, sizeof(buf), "%04d-%02d-%02dT%02d:%02d:%02dZ",
                 st.wYear, st.wMonth, st.wDay,
                 st.wHour, st.wMinute, st.wSecond);
        return buf;
    }

    std::vector<FileEdit> m_edits;
    AgentEditLog m_log;
};

// ============================================================================
// WIN32IDE Integration — Agent Panel Methods
// ============================================================================

// Static session pointer (owned by Win32IDE instance via opaque unique_ptr)
static std::unique_ptr<AgentEditSession> s_agentSession;
static std::unique_ptr<BoundedAgentLoop> s_agentLoop;

// ============================================================================
// Initialize agent panel — creates Win32 UI controls for diff review
// ============================================================================

// Agent panel control IDs (avoid collision with other subsystems)
#define IDC_AGENT_DIFF_PANEL     14001
#define IDC_AGENT_STATUS_LABEL   14002
#define IDC_AGENT_ACCEPT_ALL_BTN 14003
#define IDC_AGENT_REJECT_ALL_BTN 14004
#define IDC_AGENT_NEW_SESSION    14005
#define IDC_AGENT_SUMMARY_EDIT   14006

#define IDC_EDIT_REVIEW_STATUS_LABEL    14020
#define IDC_EDIT_REVIEW_LIST            14021
#define IDC_EDIT_REVIEW_PREVIEW         14022
#define IDC_EDIT_REVIEW_APPROVE_BTN     14023
#define IDC_EDIT_REVIEW_DECLINE_BTN     14024
#define IDC_EDIT_REVIEW_ACCEPT_ALL_BTN  14025
#define IDC_EDIT_REVIEW_DECLINE_ALL_BTN 14026

namespace {

const char* PendingEditSourceLabel(RawrXD::Review::EditSource source)
{
    switch (source)
    {
        case RawrXD::Review::EditSource::Agent:
            return "Agent";
        case RawrXD::Review::EditSource::GhostText:
            return "GhostText";
        case RawrXD::Review::EditSource::Lsp:
            return "LSP";
        case RawrXD::Review::EditSource::User:
            return "User";
        default:
            return "Unknown";
    }
}

const char* PendingEditStateLabel(RawrXD::Review::EditState state)
{
    switch (state)
    {
        case RawrXD::Review::EditState::Pending:
            return "PENDING";
        case RawrXD::Review::EditState::Approved:
            return "APPROVED";
        case RawrXD::Review::EditState::Applied:
            return "APPLIED";
        case RawrXD::Review::EditState::Committed:
            return "COMMITTED";
        case RawrXD::Review::EditState::Declined:
            return "DECLINED";
        case RawrXD::Review::EditState::Discarded:
            return "DISCARDED";
        case RawrXD::Review::EditState::Stale:
            return "STALE";
        case RawrXD::Review::EditState::AutoDeclined:
            return "AUTO_DECLINED";
        default:
            return "UNKNOWN";
    }
}

bool IsPendingState(RawrXD::Review::EditState state)
{
    return state == RawrXD::Review::EditState::Pending || state == RawrXD::Review::EditState::Approved ||
           state == RawrXD::Review::EditState::Stale;
}

bool QueryFileWriteTime(const std::string& filePath, FILETIME* outWriteTime)
{
    if (!outWriteTime || filePath.empty())
    {
        return false;
    }

    WIN32_FILE_ATTRIBUTE_DATA data{};
    if (!GetFileAttributesExA(filePath.c_str(), GetFileExInfoStandard, &data))
    {
        return false;
    }

    *outWriteTime = data.ftLastWriteTime;
    return true;
}

bool IsValidCharRange(const CHARRANGE& range)
{
    return range.cpMin >= 0 && range.cpMax >= range.cpMin;
}

bool RangesOverlap(const CHARRANGE& left, const CHARRANGE& right)
{
    if (!IsValidCharRange(left) || !IsValidCharRange(right))
    {
        return false;
    }
    return left.cpMin < right.cpMax && right.cpMin < left.cpMax;
}

std::string ReadWindowTextUtf8(HWND hwnd)
{
    if (!hwnd || !IsWindow(hwnd))
    {
        return {};
    }

    const int length = GetWindowTextLengthA(hwnd);
    if (length <= 0)
    {
        return {};
    }

    std::string text(static_cast<size_t>(length), '\0');
    GetWindowTextA(hwnd, text.data(), length + 1);
    return text;
}

std::string ApplyRangeReplacement(std::string original, const CHARRANGE& range, const std::string& replacement)
{
    if (!IsValidCharRange(range) || range.cpMin > static_cast<LONG>(original.size()))
    {
        return original;
    }

    const size_t start = static_cast<size_t>(range.cpMin);
    const size_t end = static_cast<size_t>((std::min)(range.cpMax, static_cast<LONG>(original.size())));
    original.replace(start, end - start, replacement);
    return original;
}

}  // namespace

LRESULT CALLBACK Win32IDE::AgentDiffPanelProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    Win32IDE* self = reinterpret_cast<Win32IDE*>(GetWindowLongPtrA(hwnd, GWLP_USERDATA));

    switch (uMsg) {
    case WM_PAINT: {
        if (!self) break;
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);

        // Fill background with dark theme color
        RECT rc;
        GetClientRect(hwnd, &rc);
        HBRUSH bgBrush = CreateSolidBrush(self->m_currentTheme.backgroundColor);
        FillRect(hdc, &rc, bgBrush);
        DeleteObject(bgBrush);

        // Render the agent diff content using existing renderAgentDiffPanel
        self->renderAgentDiffPanel(hdc, rc);

        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_ERASEBKGND:
        return 1; // Prevent flicker

    case WM_COMMAND: {
        // Forward button clicks to the main window for processing
        if (self && self->m_hwndMain) {
            PostMessage(self->m_hwndMain, WM_COMMAND, wParam, lParam);
        }
        return 0;
    }
    }

    return DefWindowProcA(hwnd, uMsg, wParam, lParam);
}

void Win32IDE::initEditReviewPanel()
{
    if (m_editReviewPanelInitialized)
        return;

    HWND hwndParent = m_hwndPanelContainer ? m_hwndPanelContainer : m_hwndMain;
    HFONT hBoldFont = CreateFontA(-14, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, ANSI_CHARSET, OUT_DEFAULT_PRECIS,
                                  CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Segoe UI");
    HFONT hNormalFont = CreateFontA(-13, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, ANSI_CHARSET, OUT_DEFAULT_PRECIS,
                                    CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Segoe UI");

    const int panelW = 780;
    const int panelH = 320;
    const int buttonY = panelH - 34;
    const int leftW = 250;
    const int previewX = leftW + 8;
    const int previewW = panelW - previewX;
    const int bodyTop = 28;
    const int bodyHeight = buttonY - bodyTop - 4;
    const int buttonW = (panelW - 25) / 4;

    m_hwndEditReviewStatusLabel = CreateWindowExA(
        0, "STATIC", " Edit Review — No pending edits", WS_CHILD | SS_LEFT | SS_CENTERIMAGE, 0, 0, panelW, 24,
        hwndParent, (HMENU)IDC_EDIT_REVIEW_STATUS_LABEL, m_hInstance, nullptr);
    if (m_hwndEditReviewStatusLabel)
        SendMessage(m_hwndEditReviewStatusLabel, WM_SETFONT, (WPARAM)hBoldFont, TRUE);

    m_hwndEditReviewList = CreateWindowExA(
        WS_EX_CLIENTEDGE, "LISTBOX", "", WS_CHILD | LBS_NOTIFY | WS_VSCROLL | LBS_NOINTEGRALHEIGHT, 0, bodyTop,
        leftW, bodyHeight, hwndParent, (HMENU)IDC_EDIT_REVIEW_LIST, m_hInstance, nullptr);
    if (m_hwndEditReviewList)
        SendMessage(m_hwndEditReviewList, WM_SETFONT, (WPARAM)hNormalFont, TRUE);

    m_hwndEditReviewPreview = CreateWindowExA(
        WS_EX_CLIENTEDGE, "EDIT", "", WS_CHILD | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL | ES_AUTOHSCROLL |
                                             WS_VSCROLL | WS_HSCROLL,
        previewX, bodyTop, previewW, bodyHeight, hwndParent, (HMENU)IDC_EDIT_REVIEW_PREVIEW, m_hInstance, nullptr);
    if (m_hwndEditReviewPreview)
        SendMessage(m_hwndEditReviewPreview, WM_SETFONT, (WPARAM)hNormalFont, TRUE);

    m_hwndEditReviewApproveBtn = CreateWindowExA(0, "BUTTON", "Approve", WS_CHILD | BS_PUSHBUTTON, 5, buttonY,
                                                 buttonW, 30, hwndParent, (HMENU)IDC_EDIT_REVIEW_APPROVE_BTN,
                                                 m_hInstance, nullptr);
    m_hwndEditReviewDeclineBtn = CreateWindowExA(0, "BUTTON", "Decline", WS_CHILD | BS_PUSHBUTTON,
                                                 10 + buttonW, buttonY, buttonW, 30, hwndParent,
                                                 (HMENU)IDC_EDIT_REVIEW_DECLINE_BTN, m_hInstance, nullptr);
    m_hwndEditReviewAcceptAllBtn = CreateWindowExA(0, "BUTTON", "Accept All", WS_CHILD | BS_PUSHBUTTON,
                                                   15 + buttonW * 2, buttonY, buttonW, 30, hwndParent,
                                                   (HMENU)IDC_EDIT_REVIEW_ACCEPT_ALL_BTN, m_hInstance, nullptr);
    m_hwndEditReviewDeclineAllBtn = CreateWindowExA(0, "BUTTON", "Decline All", WS_CHILD | BS_PUSHBUTTON,
                                                    20 + buttonW * 3, buttonY, buttonW, 30, hwndParent,
                                                    (HMENU)IDC_EDIT_REVIEW_DECLINE_ALL_BTN, m_hInstance, nullptr);

    for (HWND button : {m_hwndEditReviewApproveBtn, m_hwndEditReviewDeclineBtn, m_hwndEditReviewAcceptAllBtn,
                        m_hwndEditReviewDeclineAllBtn})
    {
        if (button)
            SendMessage(button, WM_SETFONT, (WPARAM)hNormalFont, TRUE);
    }

    for (HWND control : {m_hwndEditReviewStatusLabel, m_hwndEditReviewList, m_hwndEditReviewPreview,
                         m_hwndEditReviewApproveBtn, m_hwndEditReviewDeclineBtn, m_hwndEditReviewAcceptAllBtn,
                         m_hwndEditReviewDeclineAllBtn})
    {
        if (control)
            ShowWindow(control, SW_HIDE);
    }

    m_editReviewPanelInitialized = true;
}

std::string Win32IDE::buildPendingEditDiffPreview(const std::string& filePath, const std::string& oldText,
                                                  const std::string& newText) const
{
    const std::string oldName = filePath.empty() ? "a/pending-edit" : "a/" + filePath;
    const std::string newName = filePath.empty() ? "b/pending-edit" : "b/" + filePath;
    auto diffResult = RawrXD::Diff::DiffEngine::ComputeDiff(oldText, newText, 3);
    std::string unified = diffResult.ToUnifiedDiff(oldName, newName);
    if (unified.empty())
    {
        unified = "[no textual diff available]";
    }
    if (unified.size() > 12000)
    {
        unified.resize(12000);
        unified += "\n[diff preview truncated]";
    }
    return unified;
}

std::string Win32IDE::getPendingEditSummary() const
{
    size_t pending = 0;
    size_t stale = 0;
    size_t committed = 0;
    for (const auto& edit : m_pendingEdits)
    {
        if (edit.state == RawrXD::Review::EditState::Pending || edit.state == RawrXD::Review::EditState::Approved)
            ++pending;
        else if (edit.state == RawrXD::Review::EditState::Stale)
            ++stale;
        else if (edit.state == RawrXD::Review::EditState::Committed)
            ++committed;
    }

    std::ostringstream oss;
    oss << " Edit Review — Pending: " << pending << " | Stale: " << stale << " | Committed: " << committed;
    return oss.str();
}

void Win32IDE::rebuildPendingEditReviewList()
{
    if (!m_hwndEditReviewList)
        return;

    LRESULT previousSel = SendMessageA(m_hwndEditReviewList, LB_GETCURSEL, 0, 0);
    uint64_t previousId = 0;
    if (previousSel != LB_ERR)
    {
        previousId = static_cast<uint64_t>(SendMessageA(m_hwndEditReviewList, LB_GETITEMDATA, previousSel, 0));
    }

    SendMessageA(m_hwndEditReviewList, LB_RESETCONTENT, 0, 0);
    int selectIndex = -1;

    for (const auto& edit : m_pendingEdits)
    {
        if (!IsPendingState(edit.state))
            continue;

        std::ostringstream label;
        label << '[' << PendingEditStateLabel(edit.state) << "] [" << PendingEditSourceLabel(edit.source) << "] ";
        label << (edit.file.empty() ? "(active editor)" : edit.file);
        const int index = static_cast<int>(SendMessageA(m_hwndEditReviewList, LB_ADDSTRING, 0,
                                                        reinterpret_cast<LPARAM>(label.str().c_str())));
        if (index >= 0)
        {
            SendMessageA(m_hwndEditReviewList, LB_SETITEMDATA, index, (LPARAM)edit.id);
            if (edit.id == previousId || (selectIndex < 0 && edit.id == m_activeGhostPendingEditId))
            {
                selectIndex = index;
            }
            else if (selectIndex < 0)
            {
                selectIndex = index;
            }
        }
    }

    if (selectIndex >= 0)
    {
        SendMessageA(m_hwndEditReviewList, LB_SETCURSEL, selectIndex, 0);
    }
}

void Win32IDE::onPendingEditSelectionChanged()
{
    if (!m_hwndEditReviewPreview || !m_hwndEditReviewList)
        return;

    const LRESULT sel = SendMessageA(m_hwndEditReviewList, LB_GETCURSEL, 0, 0);
    if (sel == LB_ERR)
    {
        SetWindowTextA(m_hwndEditReviewPreview, "");
        return;
    }

    const uint64_t selectedId = static_cast<uint64_t>(SendMessageA(m_hwndEditReviewList, LB_GETITEMDATA, sel, 0));
    for (const auto& edit : m_pendingEdits)
    {
        if (edit.id == selectedId)
        {
            SetWindowTextA(m_hwndEditReviewPreview, edit.diffPreview.c_str());
            return;
        }
    }

    SetWindowTextA(m_hwndEditReviewPreview, "");
}

void Win32IDE::refreshPendingEditStaleStates()
{
    for (auto& edit : m_pendingEdits)
    {
        if (edit.state != RawrXD::Review::EditState::Pending && edit.state != RawrXD::Review::EditState::Approved)
            continue;

        if (edit.editorHandle && IsWindow(edit.editorHandle) && IsValidCharRange(edit.selectionSnapshot))
        {
            CHARRANGE current{};
            SendMessageA(edit.editorHandle, EM_EXGETSEL, 0, reinterpret_cast<LPARAM>(&current));
            if (current.cpMin != edit.selectionSnapshot.cpMin || current.cpMax != edit.selectionSnapshot.cpMax)
            {
                edit.state = RawrXD::Review::EditState::Stale;
                continue;
            }
        }

        const bool isCurrentEditorFile = !edit.file.empty() && !m_currentFile.empty() &&
                                         _stricmp(edit.file.c_str(), m_currentFile.c_str()) == 0;
        if (isCurrentEditorFile && !edit.editorHandle && !edit.oldText.empty() && getEditorText() != edit.oldText)
        {
            edit.state = RawrXD::Review::EditState::Stale;
            continue;
        }
        if (!edit.file.empty() && edit.hasFileWriteTime && !isCurrentEditorFile)
        {
            FILETIME latestWriteTime{};
            if (QueryFileWriteTime(edit.file, &latestWriteTime) &&
                CompareFileTime(&latestWriteTime, &edit.fileWriteTime) != 0)
            {
                edit.state = RawrXD::Review::EditState::Stale;
            }
        }
    }
}

void Win32IDE::refreshEditReviewPanel()
{
    if (!m_editReviewPanelInitialized)
        return;

    refreshPendingEditStaleStates();
    rebuildPendingEditReviewList();
    onPendingEditSelectionChanged();

    const std::string summary = getPendingEditSummary();
    if (m_hwndEditReviewStatusLabel)
    {
        SetWindowTextA(m_hwndEditReviewStatusLabel, summary.c_str());
    }

    const bool hasPending = std::any_of(m_pendingEdits.begin(), m_pendingEdits.end(), [](const PendingEditEntry& edit) {
        return IsPendingState(edit.state);
    });
    const int showCmd = hasPending ? SW_SHOW : SW_HIDE;

    for (HWND control : {m_hwndEditReviewStatusLabel, m_hwndEditReviewList, m_hwndEditReviewPreview,
                         m_hwndEditReviewApproveBtn, m_hwndEditReviewDeclineBtn, m_hwndEditReviewAcceptAllBtn,
                         m_hwndEditReviewDeclineAllBtn})
    {
        if (control)
            ShowWindow(control, showCmd);
    }

    const bool hasSelection = hasPending && SendMessageA(m_hwndEditReviewList, LB_GETCURSEL, 0, 0) != LB_ERR;
    if (m_hwndEditReviewApproveBtn)
        EnableWindow(m_hwndEditReviewApproveBtn, hasSelection);
    if (m_hwndEditReviewDeclineBtn)
        EnableWindow(m_hwndEditReviewDeclineBtn, hasSelection);
    if (m_hwndEditReviewAcceptAllBtn)
        EnableWindow(m_hwndEditReviewAcceptAllBtn, hasPending);
    if (m_hwndEditReviewDeclineAllBtn)
        EnableWindow(m_hwndEditReviewDeclineAllBtn, hasPending);
}

uint64_t Win32IDE::queueEditorPendingEdit(RawrXD::Review::EditSource source, RawrXD::Review::EditType type,
                                          HWND editorHandle, const CHARRANGE& oldRange, const std::string& oldText,
                                          const std::string& newText, const std::string& filePath, bool replaceAll)
{
    PendingEditEntry edit{};
    edit.id = m_nextPendingEditId++;
    edit.type = type;
    edit.file = filePath;
    edit.oldRange = oldRange;
    edit.oldText = oldText;
    edit.newText = newText;
    edit.source = source;
    edit.created = GetTickCount64();
    edit.state = RawrXD::Review::EditState::Pending;
    edit.replaceAll = replaceAll;
    edit.editorHandle = editorHandle;
    edit.selectionSnapshot = oldRange;
    if (!filePath.empty())
    {
        edit.hasFileWriteTime = QueryFileWriteTime(filePath, &edit.fileWriteTime);
    }

    for (auto& existing : m_pendingEdits)
    {
        if (!IsPendingState(existing.state))
            continue;
        if (!filePath.empty() && existing.file == filePath &&
            (!IsValidCharRange(existing.oldRange) || !IsValidCharRange(oldRange) || RangesOverlap(existing.oldRange, oldRange)))
        {
            existing.state = RawrXD::Review::EditState::Stale;
        }
    }

    if (editorHandle == m_hwndEditor && IsValidCharRange(oldRange))
    {
        const std::string beforeDoc = getEditorText();
        edit.diffPreview = buildPendingEditDiffPreview(filePath, beforeDoc, ApplyRangeReplacement(beforeDoc, oldRange, newText));
    }
    else
    {
        edit.diffPreview = buildPendingEditDiffPreview(filePath, oldText, newText);
    }

    m_pendingEdits.push_back(edit);
    if (source == RawrXD::Review::EditSource::GhostText)
    {
        m_activeGhostPendingEditId = edit.id;
    }

    appendToOutput("[ReviewGate] Queued editor edit #" + std::to_string(edit.id) + " from " +
                       std::string(PendingEditSourceLabel(source)) + "\n",
                   "Agent", OutputSeverity::Info);
    refreshEditReviewPanel();
    return edit.id;
}

uint64_t Win32IDE::queueFilePendingEdit(RawrXD::Review::EditSource source, RawrXD::Review::EditType type,
                                        const std::string& filePath, const std::string& oldText,
                                        const std::string& newText, const std::string& newPath)
{
    PendingEditEntry edit{};
    edit.id = m_nextPendingEditId++;
    edit.type = type;
    edit.file = filePath;
    edit.oldText = oldText;
    edit.newText = newText;
    edit.newPath = newPath;
    edit.source = source;
    edit.created = GetTickCount64();
    edit.state = RawrXD::Review::EditState::Pending;
    edit.hasFileWriteTime = QueryFileWriteTime(filePath, &edit.fileWriteTime);

    for (auto& existing : m_pendingEdits)
    {
        if (IsPendingState(existing.state) && !filePath.empty() && existing.file == filePath)
        {
            existing.state = RawrXD::Review::EditState::Stale;
        }
    }

    if (type == RawrXD::Review::EditType::Rename)
    {
        std::ostringstream preview;
        preview << "--- a/" << filePath << "\n+++ b/" << newPath << "\n[rename pending]";
        edit.diffPreview = preview.str();
    }
    else if (type == RawrXD::Review::EditType::Delete)
    {
        edit.diffPreview = buildPendingEditDiffPreview(filePath, oldText, std::string());
    }
    else if (type == RawrXD::Review::EditType::Create)
    {
        edit.diffPreview = buildPendingEditDiffPreview(filePath, std::string(), newText);
    }
    else
    {
        edit.diffPreview = buildPendingEditDiffPreview(filePath, oldText, newText);
    }

    m_pendingEdits.push_back(edit);
    appendToOutput("[ReviewGate] Queued file edit #" + std::to_string(edit.id) + " for " + filePath + "\n",
                   "General", OutputSeverity::Info);
    refreshEditReviewPanel();
    return edit.id;
}

void Win32IDE::queueNavigatorPendingEdit(RawrXD::Review::PendingEditRequest* request)
{
    std::unique_ptr<RawrXD::Review::PendingEditRequest> owned(request);
    if (!owned)
        return;

    std::string filePath = owned->file;
    if (filePath.empty() && owned->targetHandle == m_hwndEditor)
    {
        filePath = m_currentFile;
    }

    queueEditorPendingEdit(owned->source, owned->type, owned->targetHandle, owned->oldRange, owned->oldText,
                           owned->newText, filePath, owned->replaceAll);
}

void Win32IDE::updateGhostTextPendingEditPreview(const std::string& textToInsert)
{
    if (!m_hwndEditor || textToInsert.empty())
        return;

    CHARRANGE snapshot{};
    snapshot.cpMin = snapshot.cpMax = (m_ghostTextRequestCursorPos >= 0) ? static_cast<LONG>(m_ghostTextRequestCursorPos) : 0;
    if (IsValidCharRange(m_ghostTextSelectionSnapshot))
    {
        snapshot = m_ghostTextSelectionSnapshot;
    }

    for (auto& edit : m_pendingEdits)
    {
        if (edit.id == m_activeGhostPendingEditId && edit.source == RawrXD::Review::EditSource::GhostText &&
            IsPendingState(edit.state))
        {
            edit.newText = textToInsert;
            const std::string beforeDoc = getEditorText();
            edit.diffPreview = buildPendingEditDiffPreview(m_currentFile, beforeDoc,
                                                           ApplyRangeReplacement(beforeDoc, snapshot, textToInsert));
            refreshEditReviewPanel();
            return;
        }
    }

    queueEditorPendingEdit(RawrXD::Review::EditSource::GhostText, RawrXD::Review::EditType::Insert, m_hwndEditor,
                           snapshot, std::string(), textToInsert, m_currentFile, false);
}

void Win32IDE::clearGhostTextPendingEdit(RawrXD::Review::EditState declineState)
{
    if (m_activeGhostPendingEditId == 0)
        return;
    declinePendingEdit(m_activeGhostPendingEditId, declineState);
    m_activeGhostPendingEditId = 0;
}

bool Win32IDE::approvePendingEdit(uint64_t id)
{
    refreshPendingEditStaleStates();

    for (auto& edit : m_pendingEdits)
    {
        if (edit.id != id)
            continue;

        if (edit.state == RawrXD::Review::EditState::Stale)
        {
            edit.state = RawrXD::Review::EditState::AutoDeclined;
            refreshEditReviewPanel();
            return false;
        }

        if (edit.state != RawrXD::Review::EditState::Pending && edit.state != RawrXD::Review::EditState::Approved)
        {
            return false;
        }

        edit.approved = true;
        edit.state = RawrXD::Review::EditState::Approved;

        bool applied = false;
        if (edit.editorHandle && IsWindow(edit.editorHandle))
        {
            if (IsValidCharRange(edit.selectionSnapshot))
            {
                CHARRANGE current{};
                SendMessageA(edit.editorHandle, EM_EXGETSEL, 0, reinterpret_cast<LPARAM>(&current));
                if (current.cpMin != edit.selectionSnapshot.cpMin || current.cpMax != edit.selectionSnapshot.cpMax)
                {
                    edit.state = RawrXD::Review::EditState::AutoDeclined;
                    refreshEditReviewPanel();
                    return false;
                }
            }

            SendMessageA(edit.editorHandle, EM_SETSEL, edit.oldRange.cpMin, edit.oldRange.cpMax);
            SendMessageA(edit.editorHandle, EM_REPLACESEL, TRUE, reinterpret_cast<LPARAM>(edit.newText.c_str()));
            const LONG caret = edit.oldRange.cpMin + static_cast<LONG>(edit.newText.size());
            SendMessageA(edit.editorHandle, EM_SETSEL, caret, caret);
            SendMessageA(edit.editorHandle, EM_SCROLLCARET, 0, 0);
            applied = true;
        }
        else
        {
            switch (edit.type)
            {
                case RawrXD::Review::EditType::Rename:
                    applied = MoveFileExA(edit.file.c_str(), edit.newPath.c_str(),
                                          MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != FALSE;
                    break;
                case RawrXD::Review::EditType::Delete:
                {
                    DWORD attr = GetFileAttributesA(edit.file.c_str());
                    if (attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY))
                    {
                        SHFILEOPSTRUCTA shop = {0};
                        std::string doubleNullPath = edit.file;
                        doubleNullPath.push_back('\0');
                        shop.wFunc = FO_DELETE;
                        shop.pFrom = doubleNullPath.c_str();
                        shop.fFlags = FOF_NOCONFIRMATION | FOF_SILENT | FOF_NOERRORUI;
                        applied = SHFileOperationA(&shop) == 0;
                    }
                    else
                    {
                        applied = DeleteFileA(edit.file.c_str()) != FALSE;
                    }
                    break;
                }
                case RawrXD::Review::EditType::Create:
                {
                    if (!edit.file.empty())
                    {
                        std::error_code ec;
                        fs::create_directories(fs::path(edit.file).parent_path(), ec);
                    }
                    std::ofstream out(edit.file, std::ios::binary);
                    if (out.is_open())
                    {
                        out << edit.newText;
                        applied = true;
                    }
                    break;
                }
                case RawrXD::Review::EditType::Insert:
                case RawrXD::Review::EditType::Replace:
                default:
                {
                    const bool isCurrentEditorFile = !edit.file.empty() && !m_currentFile.empty() &&
                                                     _stricmp(edit.file.c_str(), m_currentFile.c_str()) == 0;
                    if (isCurrentEditorFile && m_hwndEditor)
                    {
                        SetWindowTextA(m_hwndEditor, edit.newText.c_str());
                        applied = true;
                    }
                    else
                    {
                        std::ofstream out(edit.file, std::ios::binary);
                        if (out.is_open())
                        {
                            out << edit.newText;
                            applied = true;
                        }
                    }
                    break;
                }
            }
        }

        if (!applied)
        {
            edit.state = RawrXD::Review::EditState::AutoDeclined;
            refreshEditReviewPanel();
            return false;
        }

        edit.state = RawrXD::Review::EditState::Applied;
        edit.state = RawrXD::Review::EditState::Committed;
        if (edit.id == m_activeGhostPendingEditId)
        {
            m_activeGhostPendingEditId = 0;
        }
        appendToOutput("[ReviewGate] Applied pending edit #" + std::to_string(edit.id) + "\n", "General",
                       OutputSeverity::Success);
        refreshEditReviewPanel();
        return true;
    }

    return false;
}

void Win32IDE::declinePendingEdit(uint64_t id, RawrXD::Review::EditState declineState)
{
    for (auto& edit : m_pendingEdits)
    {
        if (edit.id == id && IsPendingState(edit.state))
        {
            edit.state = declineState;
            if (edit.id == m_activeGhostPendingEditId)
            {
                m_activeGhostPendingEditId = 0;
            }
            appendToOutput("[ReviewGate] Declined pending edit #" + std::to_string(edit.id) + "\n", "General",
                           OutputSeverity::Info);
            break;
        }
    }
    refreshEditReviewPanel();
}

void Win32IDE::approveAllPendingEdits()
{
    std::vector<uint64_t> ids;
    for (const auto& edit : m_pendingEdits)
    {
        if (edit.state == RawrXD::Review::EditState::Pending || edit.state == RawrXD::Review::EditState::Approved)
        {
            ids.push_back(edit.id);
        }
    }
    for (uint64_t id : ids)
    {
        approvePendingEdit(id);
    }
}

void Win32IDE::declineAllPendingEdits(RawrXD::Review::EditState declineState)
{
    for (auto& edit : m_pendingEdits)
    {
        if (IsPendingState(edit.state))
        {
            edit.state = declineState;
        }
    }
    m_activeGhostPendingEditId = 0;
    refreshEditReviewPanel();
}

void Win32IDE::selectNextPendingEdit()
{
    if (!m_hwndEditReviewList)
        return;

    const LRESULT count = SendMessageA(m_hwndEditReviewList, LB_GETCOUNT, 0, 0);
    if (count <= 0)
        return;

    LRESULT current = SendMessageA(m_hwndEditReviewList, LB_GETCURSEL, 0, 0);
    if (current == LB_ERR)
        current = -1;
    const LRESULT next = (current + 1) % count;
    SendMessageA(m_hwndEditReviewList, LB_SETCURSEL, next, 0);
    onPendingEditSelectionChanged();
}

void Win32IDE::initAgentPanel() {
    if (m_agentPanelInitialized) return;

    // The agent diff panel lives inside the bottom panel container alongside
    // Terminal, Output, Problems, Debug Console. We create it as a child
    // of the panel container. When no session is active it shows an empty state.
    HWND hwndParent = m_hwndPanelContainer ? m_hwndPanelContainer : m_hwndMain;

    HFONT hBoldFont = CreateFontA(-14, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
        DEFAULT_PITCH | FF_DONTCARE, "Segoe UI");
    HFONT hNormalFont = CreateFontA(-13, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
        DEFAULT_PITCH | FF_DONTCARE, "Segoe UI");

    // Register custom window class for the diff panel (owner-drawn)
    static bool classRegistered = false;
    if (!classRegistered) {
        WNDCLASSEXA wc = {};
        wc.cbSize = sizeof(wc);
        wc.style = CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc = AgentDiffPanelProc;
        wc.hInstance = m_hInstance;
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.hbrBackground = nullptr; // We paint ourselves
        wc.lpszClassName = "RawrXD_AgentDiffPanel";
        if (RegisterClassExA(&wc)) classRegistered = true;
    }

    // ====================================================================
    // Layout: Status bar (28px) + Diff panel (fill) + Summary (60px) + Buttons (32px)
    // Initial size will be adjusted by onSize/layout
    // ====================================================================

    int panelW = 600, panelH = 300; // Initial size, will be resized by layout

    // Status label — shows agent progress or "No active session"
    m_hwndAgentStatusLabel = CreateWindowExA(
        0, "STATIC", " Agent Diff Panel — No active session",
        WS_CHILD | SS_LEFT | SS_CENTERIMAGE,
        0, 0, panelW, 26,
        hwndParent, (HMENU)IDC_AGENT_STATUS_LABEL, m_hInstance, nullptr);
    if (m_hwndAgentStatusLabel) SendMessage(m_hwndAgentStatusLabel, WM_SETFONT, (WPARAM)hBoldFont, TRUE);

    // Owner-drawn diff panel — renders hunks via renderAgentDiffPanel()
    m_hwndAgentDiffPanel = CreateWindowExA(
        WS_EX_CLIENTEDGE, "RawrXD_AgentDiffPanel", "",
        WS_CHILD | WS_VSCROLL,
        0, 26, panelW, panelH - 26 - 60 - 36,
        hwndParent, (HMENU)IDC_AGENT_DIFF_PANEL, m_hInstance, nullptr);
    if (m_hwndAgentDiffPanel) {
        SetWindowLongPtrA(m_hwndAgentDiffPanel, GWLP_USERDATA, (LONG_PTR)this);
    }

    // Summary text — read-only multiline showing session summary
    m_hwndAgentSummaryEdit = CreateWindowExA(
        WS_EX_CLIENTEDGE, "EDIT", "",
        WS_CHILD | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL | WS_VSCROLL,
        0, panelH - 60 - 36, panelW, 56,
        hwndParent, (HMENU)IDC_AGENT_SUMMARY_EDIT, m_hInstance, nullptr);
    if (m_hwndAgentSummaryEdit) SendMessage(m_hwndAgentSummaryEdit, WM_SETFONT, (WPARAM)hNormalFont, TRUE);

    // Button bar — Accept All | Reject All | New Session
    int btnW = (panelW - 20) / 3;
    m_hwndAgentAcceptAllBtn = CreateWindowExA(
        0, "BUTTON", "Accept All",
        WS_CHILD | BS_PUSHBUTTON,
        5, panelH - 34, btnW, 30,
        hwndParent, (HMENU)IDC_AGENT_ACCEPT_ALL_BTN, m_hInstance, nullptr);
    if (m_hwndAgentAcceptAllBtn) SendMessage(m_hwndAgentAcceptAllBtn, WM_SETFONT, (WPARAM)hNormalFont, TRUE);

    m_hwndAgentRejectAllBtn = CreateWindowExA(
        0, "BUTTON", "Reject All",
        WS_CHILD | BS_PUSHBUTTON,
        5 + btnW + 5, panelH - 34, btnW, 30,
        hwndParent, (HMENU)IDC_AGENT_REJECT_ALL_BTN, m_hInstance, nullptr);
    if (m_hwndAgentRejectAllBtn) SendMessage(m_hwndAgentRejectAllBtn, WM_SETFONT, (WPARAM)hNormalFont, TRUE);

    m_hwndAgentNewSessionBtn = CreateWindowExA(
        0, "BUTTON", "New Session",
        WS_CHILD | BS_PUSHBUTTON,
        5 + (btnW + 5) * 2, panelH - 34, btnW, 30,
        hwndParent, (HMENU)IDC_AGENT_NEW_SESSION, m_hInstance, nullptr);
    if (m_hwndAgentNewSessionBtn) SendMessage(m_hwndAgentNewSessionBtn, WM_SETFONT, (WPARAM)hNormalFont, TRUE);

    // All controls start hidden — shown when agent session starts or panel tab selected
    // They'll be shown by setSidebarView or a panel tab switch
    m_agentPanelInitialized = true;
    LOG_INFO("Agent diff panel initialized with Win32 controls");
    appendToOutput("[Agent] Diff panel initialized — use Ctrl+Shift+I to start a bounded agent session\n",
                   "Agent", OutputSeverity::Info);
}

// ============================================================================
// toggleAgentPanel — View > Agent Panel: show/hide agent chat panel
// ============================================================================
void Win32IDE::toggleAgentPanel() {
    toggleSecondarySidebar();
    if (m_hwndStatusBar) {
        SendMessage(m_hwndStatusBar, SB_SETTEXT, 0,
            (LPARAM)(m_secondarySidebarVisible ? "Agent panel shown" : "Agent panel hidden"));
    }
}

// ============================================================================
// Refresh the diff display — update summary + repaint diff area
// ============================================================================

void Win32IDE::refreshAgentDiffDisplay() {
    if (!m_agentPanelInitialized) return;

    // Update status label
    if (m_hwndAgentStatusLabel) {
        std::string status;
        if (s_agentSession && s_agentSession->EditCount() > 0) {
            status = " Agent Session — " + std::to_string(s_agentSession->EditCount()) + " file(s) with proposed edits";
        } else if (s_agentLoop) {
            status = " Agent Session — Running...";
        } else {
            status = " Agent Diff Panel — No active session";
        }
        SetWindowTextA(m_hwndAgentStatusLabel, status.c_str());
    }

    // Update summary text
    if (m_hwndAgentSummaryEdit) {
        std::string summary = getAgentSessionSummary();
        SetWindowTextA(m_hwndAgentSummaryEdit, summary.c_str());
    }

    // Repaint the diff area
    if (m_hwndAgentDiffPanel) {
        InvalidateRect(m_hwndAgentDiffPanel, nullptr, TRUE);
    }

    // Enable/disable buttons based on session state
    bool hasSession = s_agentSession && s_agentSession->EditCount() > 0;
    if (m_hwndAgentAcceptAllBtn) EnableWindow(m_hwndAgentAcceptAllBtn, hasSession);
    if (m_hwndAgentRejectAllBtn) EnableWindow(m_hwndAgentRejectAllBtn, hasSession);
}

// ============================================================================
// Start an agent edit session from user prompt
// ============================================================================

void Win32IDE::startAgentSession(const std::string& prompt) {
    if (!s_agentSession) {
        s_agentSession = std::make_unique<AgentEditSession>();
    }
    s_agentSession->SetUserPrompt(prompt);

    // Configure the bounded agent loop
    if (!s_agentLoop) {
        s_agentLoop = std::make_unique<BoundedAgentLoop>();

        AgentLoopConfig config;
        config.maxSteps = std::clamp(IDEConfig::getInstance().getInt("agent.cycleCount", 10), 1, 99);
        // model left empty — auto-detected from Ollama /api/tags at runtime
        config.nativeBaseUrl = m_ollamaBaseUrl.empty() ? "http://localhost:11435" : m_ollamaBaseUrl;
        config.workingDirectory = m_settings.workingDirectory;

        // Populate open files
        for (const auto& tab : m_editorTabs) {
            config.openFiles.push_back(tab.filePath);
        }

        // Configure guardrails
        ToolGuardrails guards;
        guards.allowedRoots.push_back(config.workingDirectory);
        guards.maxFileSizeBytes = 10 * 1024 * 1024;
        guards.commandTimeoutMs = static_cast<unsigned int>(IDEConfig::getInstance().getTerminalTimeoutMs(true));
        guards.requireBackupOnWrite = true;
        AgentToolHandlers::SetGuardrails(guards);

        s_agentLoop->Configure(config);

        // Progress callback → update output panel
        s_agentLoop->SetProgressCallback(
            [this](int step, int maxSteps, const std::string& status, const std::string& detail) {
                std::string msg = "[Agent Step " + std::to_string(step) + "/" +
                                  std::to_string(maxSteps) + "] " + status;
                if (!detail.empty()) {
                    msg += ": " + detail.substr(0, 200);
                }
                appendToOutput(msg, "Agent", OutputSeverity::Info);
            });

        // Complete callback → show results in diff panel
        s_agentLoop->SetCompleteCallback(
            [this](const std::string& answer, const AgentTranscript& transcript) {
                appendToOutput("[Agent] Complete: " + answer.substr(0, 500),
                               "Agent", OutputSeverity::Info);
                appendToOutput("[Agent] Steps: " + std::to_string(transcript.StepCount()) +
                               " | Tools: " + std::to_string(transcript.ToolCallCount()) +
                               " | Errors: " + std::to_string(transcript.ErrorCount()),
                               "Agent", OutputSeverity::Info);

                // Save transcript
                std::string transcriptDir = m_settings.workingDirectory + "/.rawrxd/transcripts/";
                fs::create_directories(transcriptDir);
                transcript.SaveToFile(transcriptDir + transcript.GetSessionId() + ".json");

                // Refresh the agent diff panel to show proposed edits
                refreshAgentDiffDisplay();

                // Show the agent panel controls
                if (m_hwndAgentDiffPanel)   ShowWindow(m_hwndAgentDiffPanel, SW_SHOW);
                if (m_hwndAgentStatusLabel)  ShowWindow(m_hwndAgentStatusLabel, SW_SHOW);
                if (m_hwndAgentAcceptAllBtn) ShowWindow(m_hwndAgentAcceptAllBtn, SW_SHOW);
                if (m_hwndAgentRejectAllBtn) ShowWindow(m_hwndAgentRejectAllBtn, SW_SHOW);
                if (m_hwndAgentNewSessionBtn) ShowWindow(m_hwndAgentNewSessionBtn, SW_SHOW);
                if (m_hwndAgentSummaryEdit) ShowWindow(m_hwndAgentSummaryEdit, SW_SHOW);
            });
    }

    // Fire async
    appendToOutput("[Agent] Starting session: " + prompt.substr(0, 200),
                   "Agent", OutputSeverity::Info);
    s_agentLoop->ExecuteAsync(prompt);
}

// ============================================================================
// Agent panel hunk operations (called from UI)
// ============================================================================

void Win32IDE::agentAcceptHunk(int fileIndex, int hunkIndex) {
    if (!s_agentSession) return;
    if (s_agentSession->AcceptHunk(static_cast<size_t>(fileIndex), static_cast<size_t>(hunkIndex))) {
        appendToOutput("[Agent] Accepted hunk " + std::to_string(hunkIndex + 1) +
                       " in edit " + std::to_string(fileIndex + 1),
                       "Agent", OutputSeverity::Info);
    }
}

void Win32IDE::agentRejectHunk(int fileIndex, int hunkIndex) {
    if (!s_agentSession) return;
    if (s_agentSession->RejectHunk(static_cast<size_t>(fileIndex), static_cast<size_t>(hunkIndex))) {
        appendToOutput("[Agent] Rejected hunk " + std::to_string(hunkIndex + 1) +
                       " in edit " + std::to_string(fileIndex + 1),
                       "Agent", OutputSeverity::Info);
    }
}

void Win32IDE::agentAcceptAll() {
    if (!s_agentSession) return;
    for (size_t i = 0; i < s_agentSession->EditCount(); ++i) {
        s_agentSession->AcceptAll(i);
    }
    int applied = s_agentSession->ApplyAccepted();
    appendToOutput("[Agent] Applied " + std::to_string(applied) + " file edit(s)",
                   "Agent", OutputSeverity::Info);

    // Reload affected files in editor
    // (No generic reloadFileIfOpen — use openFile which handles already-open tabs)
    // for (size_t i = 0; i < s_agentSession->EditCount(); ++i) {
    //     const auto* edit = s_agentSession->GetEdit(i);
    //     if (edit && !edit->fullyRejected) {
    //         openFile(edit->path);
    //     }
    // }

    // Save provenance log
    std::string logDir = (m_currentDirectory.empty() ? "." : m_currentDirectory) + "/.rawrxd/agent_logs/";
    fs::create_directories(logDir);
    s_agentSession->SaveLog(logDir + s_agentSession->GetLog().sessionId + ".json");

    s_agentSession.reset();
}

void Win32IDE::agentRejectAll() {
    if (!s_agentSession) return;
    for (size_t i = 0; i < s_agentSession->EditCount(); ++i) {
        s_agentSession->RejectAll(i);
    }
    appendToOutput("[Agent] All edits rejected", "Agent", OutputSeverity::Info);
    s_agentSession.reset();
}

// ============================================================================
// Agent session summary (for rendering in panel)
// ============================================================================

std::string Win32IDE::getAgentSessionSummary() const {
    if (!s_agentSession) return "No active agent session";
    return s_agentSession->GetSummary();
}

// ============================================================================
// Render diff view for agent panel (called from WM_PAINT of panel area)
// ============================================================================

void Win32IDE::renderAgentDiffPanel(HDC hdc, RECT panelRect) {
    if (!s_agentSession || s_agentSession->EditCount() == 0) {
        // Empty state
        SetTextColor(hdc, m_currentTheme.textColor);
        SetBkMode(hdc, TRANSPARENT);
        RECT textRect = panelRect;
        DrawTextA(hdc, "No pending agent edits", -1, &textRect,
                  DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        return;
    }

    int y = panelRect.top + 4;
    int lineHeight = 16; // Will be DPI-scaled in production

    SetBkMode(hdc, TRANSPARENT);

    for (size_t e = 0; e < s_agentSession->EditCount(); ++e) {
        const auto* edit = s_agentSession->GetEdit(e);
        if (!edit) continue;

        // File header
        SetTextColor(hdc, RGB(0, 200, 255)); // Cyan for filenames
        std::string header = "File: " + edit->path +
                             " (+" + std::to_string(edit->diff.additions) +
                             " -" + std::to_string(edit->diff.deletions) + ")";
        TextOutA(hdc, panelRect.left + 8, y, header.c_str(), (int)header.size());
        y += lineHeight + 2;

        // Hunks
        for (size_t h = 0; h < edit->diff.hunks.size(); ++h) {
            const auto& hunk = edit->diff.hunks[h];

            // Hunk header
            SetTextColor(hdc, RGB(128, 128, 200));
            std::string hunkHeader = "  " + hunk.Header();
            if (hunk.accepted) hunkHeader += " [ACCEPTED]";
            else if (hunk.rejected) hunkHeader += " [REJECTED]";
            TextOutA(hdc, panelRect.left + 8, y, hunkHeader.c_str(), (int)hunkHeader.size());
            y += lineHeight;

            // Diff lines (max 10 per hunk for display)
            int displayedLines = 0;
            for (const auto& dl : hunk.lines) {
                if (displayedLines >= 10) {
                    SetTextColor(hdc, RGB(128, 128, 128));
                    std::string more = "    ... (" + std::to_string(hunk.lines.size() - 10) + " more)";
                    TextOutA(hdc, panelRect.left + 8, y, more.c_str(), (int)more.size());
                    y += lineHeight;
                    break;
                }

                std::string prefix;
                switch (dl.op) {
                    case DiffOp::Equal:
                        SetTextColor(hdc, m_currentTheme.textColor);
                        prefix = "    ";
                        break;
                    case DiffOp::Insert:
                        SetTextColor(hdc, RGB(0, 200, 0));
                        prefix = "  + ";
                        break;
                    case DiffOp::Delete:
                        SetTextColor(hdc, RGB(200, 0, 0));
                        prefix = "  - ";
                        break;
                }

                std::string line = prefix + dl.text;
                if (line.size() > 120) line = line.substr(0, 120) + "...";
                TextOutA(hdc, panelRect.left + 8, y, line.c_str(), (int)line.size());
                y += lineHeight;
                ++displayedLines;
            }

            y += 2; // Small gap between hunks
        }

        y += 8; // Gap between files

        if (y > panelRect.bottom) break; // Stop if we've filled the panel
    }
}

// ============================================================================
// Bounded Agent Loop — Ctrl+Shift+I entry point
// ============================================================================

void Win32IDE::onBoundedAgentLoop() {
    LOG_INFO("onBoundedAgentLoop invoked");

    // Show a simple input dialog for the user's prompt
    // Use a Win32 in-process modal approach (no resource template needed)
    RECT parentRect{};
    GetWindowRect(m_hwndMain, &parentRect);

    const int dlgWidth = 540;
    const int dlgHeight = 180;
    const int dlgX = parentRect.left + ((parentRect.right - parentRect.left) - dlgWidth) / 2;
    const int dlgY = parentRect.top + ((parentRect.bottom - parentRect.top) - dlgHeight) / 2;

    EnableWindow(m_hwndMain, FALSE);

    HWND hwndDlg = CreateWindowExA(WS_EX_DLGMODALFRAME, "STATIC", "Bounded Agent Loop",
        WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_VISIBLE,
        dlgX, dlgY, dlgWidth, dlgHeight, m_hwndMain, nullptr, m_hInstance, nullptr);

    if (!hwndDlg) {
        EnableWindow(m_hwndMain, TRUE);
        return;
    }

    CreateWindowExA(0, "STATIC", "Enter your task (max 8 steps, tool-calling agent):",
        WS_CHILD | WS_VISIBLE, 10, 10, dlgWidth - 20, 20,
        hwndDlg, nullptr, m_hInstance, nullptr);

    HWND hwndEdit = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "",
        WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL | ES_LEFT,
        10, 35, dlgWidth - 30, 24, hwndDlg, (HMENU)2001, m_hInstance, nullptr);

    HWND hwndOk = CreateWindowExA(0, "BUTTON", "Run Agent",
        WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
        dlgWidth - 210, dlgHeight - 55, 95, 30,
        hwndDlg, (HMENU)IDOK, m_hInstance, nullptr);

    CreateWindowExA(0, "BUTTON", "Cancel",
        WS_CHILD | WS_VISIBLE,
        dlgWidth - 105, dlgHeight - 55, 85, 30,
        hwndDlg, (HMENU)IDCANCEL, m_hInstance, nullptr);

    SetFocus(hwndEdit);

    bool accepted = false;
    std::string promptText;

    MSG msg{};
    while (IsWindow(hwndDlg) && GetMessage(&msg, nullptr, 0, 0)) {
        if (msg.message == WM_COMMAND) {
            const WORD cmdId = LOWORD(msg.wParam);
            if (cmdId == IDOK) {
                char buf[2048] = {0};
                GetWindowTextA(hwndEdit, buf, sizeof(buf));
                promptText = buf;
                accepted = (promptText.length() > 0);
                DestroyWindow(hwndDlg);
                break;
            } else if (cmdId == IDCANCEL) {
                DestroyWindow(hwndDlg);
                break;
            }
        }
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    EnableWindow(m_hwndMain, TRUE);
    SetForegroundWindow(m_hwndMain);

    if (accepted) {
        appendToOutput("[BoundedAgent] Starting: " + promptText.substr(0, 200),
                       "Agent", OutputSeverity::Info);
        startAgentSession(promptText);
    }
}
