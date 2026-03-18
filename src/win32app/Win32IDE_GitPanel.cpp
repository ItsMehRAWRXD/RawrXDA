// Win32IDE_GitPanel.cpp - Production Git panel with staging, diff, branch picker
#include "Win32IDE.h"
#include <string>
#include <vector>
#include <cstring>
#include <sstream>

namespace {

// ── Internal: pipe-captured git execution ──────────────────────────────────
static std::string runGitCommand(const std::string& repoPath, const char* args) {
    SECURITY_ATTRIBUTES sa = {};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;

    HANDLE hReadPipe = nullptr;
    HANDLE hWritePipe = nullptr;
    if (!CreatePipe(&hReadPipe, &hWritePipe, &sa, 0)) {
        return "Failed to create pipe.";
    }

    SetHandleInformation(hReadPipe, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOA si = {};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.hStdOutput = hWritePipe;
    si.hStdError = hWritePipe;
    si.wShowWindow = SW_HIDE;

    PROCESS_INFORMATION pi = {};
    std::string cmd = std::string("git ") + args;
    char cmdBuf[2048] = {};
    strncpy_s(cmdBuf, cmd.c_str(), _TRUNCATE);

    const char* cwd = repoPath.empty() ? nullptr : repoPath.c_str();
    std::string output;
    if (CreateProcessA(nullptr, cmdBuf, nullptr, nullptr, TRUE, CREATE_NO_WINDOW, nullptr, cwd, &si, &pi)) {
        CloseHandle(hWritePipe);
        hWritePipe = nullptr;

        char buffer[4096];
        DWORD bytesRead = 0;
        constexpr DWORD kMaxChunk = static_cast<DWORD>(sizeof(buffer) - 1);
        while (ReadFile(hReadPipe, buffer, kMaxChunk, &bytesRead, nullptr) && bytesRead > 0) {
            const size_t safeBytes = (bytesRead <= kMaxChunk) ? static_cast<size_t>(bytesRead) : static_cast<size_t>(kMaxChunk);
            buffer[safeBytes] = '\0';
            output.append(buffer, safeBytes);
        }

        WaitForSingleObject(pi.hProcess, 30000);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    } else {
        output = "Failed to execute git command.\r\nEnsure git is installed and on PATH.";
    }

    if (hWritePipe) CloseHandle(hWritePipe);
    if (hReadPipe) CloseHandle(hReadPipe);
    return output;
}

// Parse "git status --porcelain" into structured entries
struct GitFileEntry {
    char indexStatus;    // X = index status
    char workTreeStatus; // Y = work-tree status
    std::string path;
    bool staged() const { return indexStatus != ' ' && indexStatus != '?'; }
};

static std::vector<GitFileEntry> parseStatusPorcelain(const std::string& raw) {
    std::vector<GitFileEntry> entries;
    std::istringstream iss(raw);
    std::string line;
    while (std::getline(iss, line)) {
        if (line.size() < 4) continue;
        GitFileEntry e;
        e.indexStatus    = line[0];
        e.workTreeStatus = line[1];
        e.path           = line.substr(3);
        entries.push_back(std::move(e));
    }
    return entries;
}

// Parse branches from "git branch -a"
static std::vector<std::string> parseBranches(const std::string& raw) {
    std::vector<std::string> branches;
    std::istringstream iss(raw);
    std::string line;
    while (std::getline(iss, line)) {
        // Remove leading "* " or "  "
        size_t start = 0;
        while (start < line.size() && (line[start] == ' ' || line[start] == '*')) ++start;
        std::string branch = line.substr(start);
        // Trim trailing whitespace
        while (!branch.empty() && (branch.back() == '\r' || branch.back() == '\n' || branch.back() == ' '))
            branch.pop_back();
        if (!branch.empty())
            branches.push_back(branch);
    }
    return branches;
}

} // namespace

// ============================================================================
// Git panel button IDs (distinct from menu IDs)
// ============================================================================
#ifndef IDM_GIT_STAGE_ALL
#define IDM_GIT_STAGE_ALL     9850
#define IDM_GIT_UNSTAGE_ALL   9851
#define IDM_GIT_DIFF          9852
#define IDM_GIT_BRANCH_PICKER 9853
#define IDM_GIT_STASH         9854
#define IDM_GIT_STASH_POP     9855
#define IDM_GIT_PULL          9856
#define IDM_GIT_STATUS        9857
#define IDM_GIT_COMMIT        9858
#define IDM_GIT_PUSH          9859
#define IDM_GIT_STAGE_FILE    9860
#define IDM_GIT_UNSTAGE_FILE  9861
#define IDM_GIT_LOG           9862
#endif

void Win32IDE::createGitPanel() {
    if (!m_hwndSidebar) return;

    // ── Branch display ──
    m_hwndGitBranch = CreateWindowExA(
        0, "STATIC", "",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        5, 5, m_sidebarWidth - 10, 18,
        m_hwndSidebar, nullptr, m_hInstance, nullptr);

    // ── Status list (multi-line read-only edit) ──
    m_hwndGitStatus = CreateWindowExA(
        WS_EX_CLIENTEDGE, "EDIT", "",
        WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL | WS_VSCROLL,
        5, 26, m_sidebarWidth - 10, 300,
        m_hwndSidebar, nullptr, m_hInstance, nullptr);

    // ── Diff view (separate read-only edit, toggled) ──
    m_hwndGitDiff = CreateWindowExA(
        WS_EX_CLIENTEDGE, "EDIT", "",
        WS_CHILD | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL | WS_VSCROLL,
        5, 330, m_sidebarWidth - 10, 200,
        m_hwndSidebar, nullptr, m_hInstance, nullptr);

    // ── Commit message input ──
    m_hwndGitCommitMsg = CreateWindowExA(
        WS_EX_CLIENTEDGE, "EDIT", "",
        WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL | WS_TABSTOP,
        5, 335, m_sidebarWidth - 10, 22,
        m_hwndSidebar, nullptr, m_hInstance, nullptr);

    // ── Action buttons row 1 ──
    int btnY = 362, btnW = 62, btnH = 22, gap = 3, x = 5;

    CreateWindowExA(0, "BUTTON", "Refresh", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        x, btnY, btnW, btnH, m_hwndSidebar, (HMENU)(UINT_PTR)IDM_GIT_STATUS, m_hInstance, nullptr);
    x += btnW + gap;

    CreateWindowExA(0, "BUTTON", "Stage All", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        x, btnY, btnW, btnH, m_hwndSidebar, (HMENU)(UINT_PTR)IDM_GIT_STAGE_ALL, m_hInstance, nullptr);
    x += btnW + gap;

    CreateWindowExA(0, "BUTTON", "Unstage", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        x, btnY, btnW, btnH, m_hwndSidebar, (HMENU)(UINT_PTR)IDM_GIT_UNSTAGE_ALL, m_hInstance, nullptr);
    x += btnW + gap;

    CreateWindowExA(0, "BUTTON", "Diff", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        x, btnY, btnW, btnH, m_hwndSidebar, (HMENU)(UINT_PTR)IDM_GIT_DIFF, m_hInstance, nullptr);

    // ── Action buttons row 2 ──
    btnY += btnH + gap;
    x = 5;

    CreateWindowExA(0, "BUTTON", "Commit", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        x, btnY, btnW, btnH, m_hwndSidebar, (HMENU)(UINT_PTR)IDM_GIT_COMMIT, m_hInstance, nullptr);
    x += btnW + gap;

    CreateWindowExA(0, "BUTTON", "Push", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        x, btnY, btnW, btnH, m_hwndSidebar, (HMENU)(UINT_PTR)IDM_GIT_PUSH, m_hInstance, nullptr);
    x += btnW + gap;

    CreateWindowExA(0, "BUTTON", "Pull", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        x, btnY, btnW, btnH, m_hwndSidebar, (HMENU)(UINT_PTR)IDM_GIT_PULL, m_hInstance, nullptr);
    x += btnW + gap;

    CreateWindowExA(0, "BUTTON", "Branch", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        x, btnY, btnW, btnH, m_hwndSidebar, (HMENU)(UINT_PTR)IDM_GIT_BRANCH_PICKER, m_hInstance, nullptr);

    // ── Action buttons row 3 (stash) ──
    btnY += btnH + gap;
    x = 5;

    CreateWindowExA(0, "BUTTON", "Stash", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        x, btnY, btnW, btnH, m_hwndSidebar, (HMENU)(UINT_PTR)IDM_GIT_STASH, m_hInstance, nullptr);
    x += btnW + gap;

    CreateWindowExA(0, "BUTTON", "Stash Pop", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        x, btnY, 80, btnH, m_hwndSidebar, (HMENU)(UINT_PTR)IDM_GIT_STASH_POP, m_hInstance, nullptr);
    x += 80 + gap;

    CreateWindowExA(0, "BUTTON", "+File", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        x, btnY, 50, btnH, m_hwndSidebar, (HMENU)(UINT_PTR)IDM_GIT_STAGE_FILE, m_hInstance, nullptr);
    x += 50 + gap;

    CreateWindowExA(0, "BUTTON", "-File", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        x, btnY, 50, btnH, m_hwndSidebar, (HMENU)(UINT_PTR)IDM_GIT_UNSTAGE_FILE, m_hInstance, nullptr);
    x += 50 + gap;

    CreateWindowExA(0, "BUTTON", "Log", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        x, btnY, 45, btnH, m_hwndSidebar, (HMENU)(UINT_PTR)IDM_GIT_LOG, m_hInstance, nullptr);

    refreshGitStatus();
}

void Win32IDE::refreshGitStatus() {
    if (!m_hwndGitStatus) return;

    std::string repo = m_gitRepoPath;
    if (repo.empty()) {
        char cwd[MAX_PATH] = {};
        GetCurrentDirectoryA(MAX_PATH, cwd);
        repo = cwd;
    }

    // Get current branch
    std::string branch = runGitCommand(repo, "rev-parse --abbrev-ref HEAD");
    // Trim whitespace
    while (!branch.empty() && (branch.back() == '\r' || branch.back() == '\n' || branch.back() == ' '))
        branch.pop_back();
    if (m_hwndGitBranch) {
        std::string branchLabel = "Branch: " + (branch.empty() ? "(detached)" : branch);
        SetWindowTextA(m_hwndGitBranch, branchLabel.c_str());
    }

    // Get porcelain status for structured display
    std::string rawStatus = runGitCommand(repo, "status --porcelain");
    auto entries = parseStatusPorcelain(rawStatus);

    // Build formatted status text
    std::string display;
    display += "=== Staged ===\r\n";
    bool anyStaged = false;
    for (const auto& e : entries) {
        if (e.staged()) {
            display += "  ";
            display += e.indexStatus;
            display += "  ";
            display += e.path;
            display += "\r\n";
            anyStaged = true;
        }
    }
    if (!anyStaged) display += "  (none)\r\n";

    display += "\r\n=== Unstaged / Untracked ===\r\n";
    bool anyUnstaged = false;
    for (const auto& e : entries) {
        if (!e.staged() || e.workTreeStatus != ' ') {
            display += "  ";
            display += e.workTreeStatus;
            display += "  ";
            display += e.path;
            display += "\r\n";
            anyUnstaged = true;
        }
    }
    if (!anyUnstaged) display += "  (clean)\r\n";

    if (display.empty()) {
        display = "No git status output.";
    }
    SetWindowTextA(m_hwndGitStatus, display.c_str());
}

void Win32IDE::handleGitPanelCommand(WORD cmdId) {
    std::string repo = m_gitRepoPath;
    if (repo.empty()) {
        char cwd[MAX_PATH] = {};
        GetCurrentDirectoryA(MAX_PATH, cwd);
        repo = cwd;
    }

    switch (cmdId) {
    case IDM_GIT_STATUS:
        refreshGitStatus();
        break;

    case IDM_GIT_STAGE_ALL:
        runGitCommand(repo, "add -A");
        refreshGitStatus();
        break;

    case IDM_GIT_UNSTAGE_ALL:
        runGitCommand(repo, "reset HEAD");
        refreshGitStatus();
        break;

    case IDM_GIT_DIFF: {
        std::string diff = runGitCommand(repo, "diff --stat");
        diff += "\r\n---\r\n";
        diff += runGitCommand(repo, "diff");
        if (m_hwndGitDiff) {
            SetWindowTextA(m_hwndGitDiff, diff.c_str());
            // Toggle visibility
            BOOL visible = IsWindowVisible(m_hwndGitDiff);
            ShowWindow(m_hwndGitDiff, visible ? SW_HIDE : SW_SHOW);
        }
        break;
    }

    case IDM_GIT_COMMIT: {
        // Get commit message from the input edit
        char msgBuf[1024] = {};
        if (m_hwndGitCommitMsg) {
            GetWindowTextA(m_hwndGitCommitMsg, msgBuf, sizeof(msgBuf));
        }
        if (strlen(msgBuf) == 0) {
            MessageBoxA(m_hwndMain, "Enter a commit message first.", "Git Commit", MB_OK | MB_ICONWARNING);
            break;
        }
        // Escape double quotes in the commit message
        std::string safeMsg;
        for (const char* p = msgBuf; *p; ++p) {
            if (*p == '"') safeMsg += "\\\"";
            else safeMsg += *p;
        }
        std::string commitCmd = "commit -m \"";
        commitCmd += safeMsg;
        commitCmd += "\"";
        std::string result = runGitCommand(repo, commitCmd.c_str());
        appendToOutput("[Git Commit] " + result, "Git", OutputSeverity::Info);
        if (m_hwndGitCommitMsg) SetWindowTextA(m_hwndGitCommitMsg, "");
        refreshGitStatus();
        break;
    }

    case IDM_GIT_PUSH: {
        std::string pushResult = runGitCommand(repo, "push");
        appendToOutput("[Git Push] " + pushResult, "Git", OutputSeverity::Info);
        if (m_hwndGitDiff) {
            SetWindowTextA(m_hwndGitDiff, ("=== Push Result ===\r\n" + pushResult).c_str());
            ShowWindow(m_hwndGitDiff, SW_SHOW);
        }
        refreshGitStatus();
        break;
    }

    case IDM_GIT_PULL: {
        std::string pullResult = runGitCommand(repo, "pull --rebase");
        appendToOutput("[Git Pull] " + pullResult, "Git", OutputSeverity::Info);
        if (m_hwndGitDiff) {
            SetWindowTextA(m_hwndGitDiff, ("=== Pull Result ===\r\n" + pullResult).c_str());
            ShowWindow(m_hwndGitDiff, SW_SHOW);
        }
        refreshGitStatus();
        break;
    }

    case IDM_GIT_BRANCH_PICKER: {
        std::string branchList = runGitCommand(repo, "branch -a");
        auto branches = parseBranches(branchList);
        if (branches.empty()) break;

        // Show branch selection popup menu
        HMENU hMenu = CreatePopupMenu();
        for (size_t i = 0; i < branches.size() && i < 50; ++i) {
            AppendMenuA(hMenu, MF_STRING, 60000 + static_cast<UINT>(i), branches[i].c_str());
        }
        POINT pt;
        GetCursorPos(&pt);
        int sel = TrackPopupMenu(hMenu, TPM_RETURNCMD | TPM_NONOTIFY, pt.x, pt.y, 0, m_hwndMain, nullptr);
        DestroyMenu(hMenu);

        if (sel >= 60000 && sel < 60000 + (int)branches.size()) {
            std::string target = branches[sel - 60000];
            // Strip "remotes/origin/" prefix if present
            const char* remotePrefix = "remotes/origin/";
            if (target.rfind(remotePrefix, 0) == 0)
                target = target.substr(strlen(remotePrefix));
            std::string checkoutCmd = "checkout " + target;
            runGitCommand(repo, checkoutCmd.c_str());
            refreshGitStatus();
        }
        break;
    }

    case IDM_GIT_STASH: {
        std::string stashResult = runGitCommand(repo, "stash");
        appendToOutput("[Git Stash] " + stashResult, "Git", OutputSeverity::Info);
        refreshGitStatus();
        break;
    }

    case IDM_GIT_STASH_POP: {
        std::string popResult = runGitCommand(repo, "stash pop");
        appendToOutput("[Git Stash Pop] " + popResult, "Git", OutputSeverity::Info);
        if (popResult.find("CONFLICT") != std::string::npos ||
            popResult.find("error") != std::string::npos) {
            MessageBoxA(m_hwndMain, popResult.c_str(), "Git Stash Pop", MB_OK | MB_ICONWARNING);
        }
        refreshGitStatus();
        break;
    }

    case IDM_GIT_STAGE_FILE: {
        // Extract selected file path from status text
        // User must have a line selected in the status edit control
        std::string targetFile;
        if (m_hwndGitStatus) {
            // Get current line from the EDIT control
            DWORD selStart = 0, selEnd = 0;
            SendMessageA(m_hwndGitStatus, EM_GETSEL, (WPARAM)&selStart, (LPARAM)&selEnd);
            int lineIdx = (int)SendMessageA(m_hwndGitStatus, EM_LINEFROMCHAR, selStart, 0);
            char lineBuf[512] = {};
            *(WORD*)lineBuf = sizeof(lineBuf) - 2;
            int lineLen = (int)SendMessageA(m_hwndGitStatus, EM_GETLINE, lineIdx, (LPARAM)lineBuf);
            if (lineLen > 0) {
                lineBuf[lineLen] = '\0';
                // Parse status line: format is "  X  path" or section headers
                std::string line(lineBuf);
                // Skip leading whitespace
                size_t start = line.find_first_not_of(" \t");
                if (start != std::string::npos && start + 1 < line.size()) {
                    // Skip status char + spaces
                    size_t pathStart = line.find_first_not_of(" \t", start + 1);
                    if (pathStart != std::string::npos) {
                        targetFile = line.substr(pathStart);
                        // Trim trailing whitespace
                        while (!targetFile.empty() && (targetFile.back() == '\r' ||
                               targetFile.back() == '\n' || targetFile.back() == ' '))
                            targetFile.pop_back();
                    }
                }
            }
        }
        if (targetFile.empty() || targetFile.find("===") == 0 ||
            targetFile == "(none)" || targetFile == "(clean)") {
            MessageBoxA(m_hwndMain, "Click on a file line in the status panel first.",
                        "Git Stage File", MB_OK | MB_ICONINFORMATION);
            break;
        }
        std::string addCmd = "add -- \"" + targetFile + "\"";
        std::string addResult = runGitCommand(repo, addCmd.c_str());
        appendToOutput("[Git Stage] " + targetFile + ": " + addResult, "Git", OutputSeverity::Info);
        refreshGitStatus();
        break;
    }

    case IDM_GIT_UNSTAGE_FILE: {
        // Same extraction as stage but runs reset
        std::string targetFile;
        if (m_hwndGitStatus) {
            DWORD selStart = 0, selEnd = 0;
            SendMessageA(m_hwndGitStatus, EM_GETSEL, (WPARAM)&selStart, (LPARAM)&selEnd);
            int lineIdx = (int)SendMessageA(m_hwndGitStatus, EM_LINEFROMCHAR, selStart, 0);
            char lineBuf[512] = {};
            *(WORD*)lineBuf = sizeof(lineBuf) - 2;
            int lineLen = (int)SendMessageA(m_hwndGitStatus, EM_GETLINE, lineIdx, (LPARAM)lineBuf);
            if (lineLen > 0) {
                lineBuf[lineLen] = '\0';
                std::string line(lineBuf);
                size_t start = line.find_first_not_of(" \t");
                if (start != std::string::npos && start + 1 < line.size()) {
                    size_t pathStart = line.find_first_not_of(" \t", start + 1);
                    if (pathStart != std::string::npos) {
                        targetFile = line.substr(pathStart);
                        while (!targetFile.empty() && (targetFile.back() == '\r' ||
                               targetFile.back() == '\n' || targetFile.back() == ' '))
                            targetFile.pop_back();
                    }
                }
            }
        }
        if (targetFile.empty() || targetFile.find("===") == 0 ||
            targetFile == "(none)" || targetFile == "(clean)") {
            MessageBoxA(m_hwndMain, "Click on a file line in the status panel first.",
                        "Git Unstage File", MB_OK | MB_ICONINFORMATION);
            break;
        }
        std::string resetCmd = "reset HEAD -- \"" + targetFile + "\"";
        std::string resetResult = runGitCommand(repo, resetCmd.c_str());
        appendToOutput("[Git Unstage] " + targetFile + ": " + resetResult, "Git", OutputSeverity::Info);
        refreshGitStatus();
        break;
    }

    case IDM_GIT_LOG: {
        std::string logResult = runGitCommand(repo, "log --oneline --graph -20");
        appendToOutput("[Git Log]\n" + logResult, "Git", OutputSeverity::Info);
        if (m_hwndGitDiff) {
            SetWindowTextA(m_hwndGitDiff, ("=== Recent Commits ===\r\n" + logResult).c_str());
            ShowWindow(m_hwndGitDiff, SW_SHOW);
        }
        break;
    }

    default:
        break;
    }
}
