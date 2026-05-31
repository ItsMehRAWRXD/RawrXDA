// ============================================================================
// Win32IDE_TestExplorerTree.cpp — Tier 5 Gap #42: Test Explorer Tree
// ============================================================================
//
// PURPOSE:
//   Provides a VS Code-style Test Explorer tree view showing discovered tests
//   with pass/fail/pending icons.  Parses test output from the gauntlet
//   runner and external test frameworks, presenting results in a TreeView
//   control with color-coded icons.
//
// Architecture: C++20 | Win32 | No exceptions | No Qt
// Pattern:      PatchResult-compatible returns
// Rule:         NO SOURCE FILE IS TO BE SIMPLIFIED
// ============================================================================

#include "Win32IDE.h"
#include <commctrl.h>
#include <sstream>
#include <iomanip>
#include <vector>
#include <string>
#include <regex>
#include <filesystem>
#include <algorithm>

// SCAFFOLD_032: Test explorer tree population


// ============================================================================
// Test node data model
// ============================================================================

enum class TestStatus { Unknown, Running, Passed, Failed, Skipped };

struct TestNode {
    std::string name;
    std::string suite;
    TestStatus  status;
    double      durationMs;
    std::string errorMessage;
    std::vector<TestNode> children;
    HTREEITEM   hTreeItem;

    TestNode() : status(TestStatus::Unknown), durationMs(0.0), hTreeItem(nullptr) {}
};

static std::vector<TestNode> s_testNodes;
static HWND s_hwndTestExplorer = nullptr;
static HWND s_hwndTestTree     = nullptr;
static bool s_testExplorerClassRegistered = false;
static const wchar_t* TEST_EXPLORER_CLASS = L"RawrXD_TestExplorer";

struct ProcessRunResult {
    bool launched = false;
    bool timedOut = false;
    DWORD exitCode = static_cast<DWORD>(-1);
    DWORD launchError = 0;
    std::string output;
};

struct ScriptTestCase {
    std::string name;
    std::string scriptPath;
    std::vector<std::string> arguments;
    DWORD timeoutMs = 60000;
};

static std::string trimString(const std::string& value) {
    const size_t first = value.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return {};
    const size_t last = value.find_last_not_of(" \t\r\n");
    return value.substr(first, last - first + 1);
}

static std::string summarizeOutputLine(const std::string& output) {
    std::istringstream stream(output);
    std::string line;
    while (std::getline(stream, line)) {
        line = trimString(line);
        if (line.empty()) continue;
        if (line.size() > 160) {
            return line.substr(0, 157) + "...";
        }
        return line;
    }
    return {};
}

static std::string quoteProcessArg(const std::string& value) {
    if (value.empty()) return "\"\"";
    const bool needsQuotes = value.find_first_of(" \t\"") != std::string::npos;
    if (!needsQuotes) return value;

    std::string escaped;
    escaped.reserve(value.size() + 2);
    escaped.push_back('"');
    for (char ch : value) {
        if (ch == '"') escaped += "\\\"";
        else escaped.push_back(ch);
    }
    escaped.push_back('"');
    return escaped;
}

static void appendSyntheticResult(std::ostringstream& output,
                                  const char* status,
                                  const std::string& name,
                                  double durationMs,
                                  const std::string& detail = {}) {
    output << '[' << status << "] " << name;
    if (!detail.empty()) {
        output << " -> " << detail;
    }
    if (durationMs > 0.0) {
        output << " (" << std::fixed << std::setprecision(2) << durationMs << " ms)";
    }
    output << '\n';
}

static ProcessRunResult runCapturedProcess(const std::string& commandLine,
                                           const std::string& workDir,
                                           DWORD timeoutMs) {
    ProcessRunResult result;

    SECURITY_ATTRIBUTES sa = {};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;

    HANDLE hRead = nullptr;
    HANDLE hWrite = nullptr;
    if (!CreatePipe(&hRead, &hWrite, &sa, 0)) {
        result.launchError = GetLastError();
        return result;
    }

    SetHandleInformation(hRead, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOA si = {};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdOutput = hWrite;
    si.hStdError = hWrite;

    PROCESS_INFORMATION pi = {};
    std::vector<char> commandBuffer(commandLine.begin(), commandLine.end());
    commandBuffer.push_back('\0');

    if (!CreateProcessA(nullptr,
                        commandBuffer.data(),
                        nullptr,
                        nullptr,
                        TRUE,
                        CREATE_NO_WINDOW,
                        nullptr,
                        workDir.empty() ? nullptr : workDir.c_str(),
                        &si,
                        &pi)) {
        result.launchError = GetLastError();
        CloseHandle(hWrite);
        CloseHandle(hRead);
        return result;
    }

    result.launched = true;
    CloseHandle(hWrite);
    hWrite = nullptr;

    const ULONGLONG startTick = GetTickCount64();
    char buffer[4096] = {};

    while (true) {
        DWORD available = 0;
        if (PeekNamedPipe(hRead, nullptr, 0, nullptr, &available, nullptr) && available > 0) {
            DWORD bytesRead = 0;
            const DWORD toRead = (available < sizeof(buffer) - 1) ? available : static_cast<DWORD>(sizeof(buffer) - 1);
            if (ReadFile(hRead, buffer, toRead, &bytesRead, nullptr) && bytesRead > 0) {
                buffer[bytesRead] = '\0';
                result.output.append(buffer, bytesRead);
            }
        }

        const DWORD waitResult = WaitForSingleObject(pi.hProcess, 50);
        if (waitResult == WAIT_OBJECT_0) {
            break;
        }

        if (timeoutMs > 0 && (GetTickCount64() - startTick) >= timeoutMs) {
            result.timedOut = true;
            TerminateProcess(pi.hProcess, WAIT_TIMEOUT);
            break;
        }
    }

    while (true) {
        DWORD bytesRead = 0;
        if (!ReadFile(hRead, buffer, static_cast<DWORD>(sizeof(buffer) - 1), &bytesRead, nullptr) || bytesRead == 0) {
            break;
        }
        buffer[bytesRead] = '\0';
        result.output.append(buffer, bytesRead);
    }

    DWORD exitCode = 0;
    if (GetExitCodeProcess(pi.hProcess, &exitCode)) {
        result.exitCode = exitCode;
    }

    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    CloseHandle(hRead);

    return result;
}

static std::string resolvePreferredWin32IDEExe(const std::string& workDir) {
    const std::filesystem::path workspace(workDir);
    const std::filesystem::path parent = workspace.has_parent_path() ? workspace.parent_path() : workspace;
    const std::vector<std::filesystem::path> candidates = {
        workspace / "build" / "bin" / "Release" / "RawrXD-Win32IDE.exe",
        workspace / "build" / "bin" / "RawrXD-Win32IDE.exe",
        workspace / "build" / "RawrXD-Win32IDE.exe",
        workspace / "build-ninja" / "bin" / "RawrXD-Win32IDE.exe",
        workspace / "build-ninja" / "RawrXD-Win32IDE.exe",
        workspace / "build" / "dist" / "RawrXD-Win32IDE.exe",
        parent / "rawrxd" / "build" / "bin" / "Release" / "RawrXD-Win32IDE.exe",
        parent / "rawrxd" / "build" / "bin" / "RawrXD-Win32IDE.exe",
        parent / "rawrxd" / "build-ninja" / "bin" / "RawrXD-Win32IDE.exe",
        parent / "rxdn_strict" / "bin" / "RawrXD-Win32IDE.exe",
        parent / "rxdn" / "bin" / "RawrXD-Win32IDE.exe",
        parent / "rxdn_ninja" / "bin" / "RawrXD-Win32IDE.exe",
        std::filesystem::path("D:/rawrxd/build/bin/Release/RawrXD-Win32IDE.exe"),
        std::filesystem::path("D:/rawrxd/build/bin/RawrXD-Win32IDE.exe"),
        std::filesystem::path("D:/rawrxd/build-ninja/bin/RawrXD-Win32IDE.exe"),
        std::filesystem::path("D:/rxdn/bin/RawrXD-Win32IDE.exe"),
        std::filesystem::path("D:/rxdn_ninja/bin/RawrXD-Win32IDE.exe")
    };

    for (const auto& candidate : candidates) {
        if (std::filesystem::exists(candidate)) {
            return candidate.string();
        }
    }

    return {};
}

static std::vector<ScriptTestCase> discoverQaScripts(const std::string& workDir, const std::string& exePath) {
    std::vector<ScriptTestCase> scripts;
    if (exePath.empty()) return scripts;

    const std::filesystem::path workspace(workDir);
    const std::filesystem::path parent = workspace.has_parent_path() ? workspace.parent_path() : workspace;
    std::vector<std::filesystem::path> roots = {
        workspace / "scripts",
        parent / "scripts",
        parent / "rawrxd" / "scripts",
        std::filesystem::path("D:/scripts")
    };

    if (const char* envRoot = std::getenv("RAWRXD_QA_SCRIPTS_ROOT")) {
        if (envRoot[0] != '\0') {
            roots.emplace_back(envRoot);
        }
    }

    std::sort(roots.begin(), roots.end());
    roots.erase(std::unique(roots.begin(), roots.end()), roots.end());

    auto addScript = [&](const char* displayName,
                         const char* scriptName,
                         std::vector<std::string> args,
                         DWORD timeoutMs) {
        for (const auto& root : roots) {
            const std::filesystem::path scriptPath = root / scriptName;
            if (!std::filesystem::exists(scriptPath)) continue;

            scripts.push_back(ScriptTestCase{
                displayName,
                scriptPath.string(),
                std::move(args),
                timeoutMs,
            });
            break;
        }
    };

    addScript("QA Smoke: Runtime Launch",
              "runtime_smoke_check.ps1",
              {"-ExePath", exePath, "-ProcessName", "RawrXD-Win32IDE"},
              45000);

    addScript("QA Smoke: Chat Pane Strict",
              "smoke_test_chat_pane.ps1",
              {"-ExePath", exePath, "-RequireModelResponse", "-RequireIdeModelLoadPath", "-FailOnWarnings", "-KillOnExit"},
              90000);

    addScript("QA Smoke: Chat Routes",
              "smoke_test_chat_routes.ps1",
              {"-ExePath", exePath, "-FullModelMode", "-KillOnExit"},
              90000);

    addScript("QA Smoke: All Routes",
              "smoke_test_all_routes.ps1",
              {"-ExePath", exePath, "-KillOnExit"},
              120000);

    addScript("QA Contract: Debug + Swarm",
              "contract_test_debug_swarm.ps1",
              {"-ExePath", exePath, "-KillOnExit"},
              90000);

    addScript("QA Contract: Router + Hybrid",
              "contract_test_router_hybrid.ps1",
              {"-ExePath", exePath, "-KillOnExit"},
              90000);

    addScript("QA Contract: Dual + MultiResponse",
              "contract_test_dual_multiresponse.ps1",
              {"-ExePath", exePath, "-KillOnExit"},
              90000);

    return scripts;
}

// ============================================================================
// Parse test output (supports [PASS], [FAIL], [SKIP] format)
// ============================================================================

static void parseTestOutput(const std::string& output, std::vector<TestNode>& nodes) {
    nodes.clear();

    // Parse lines like:
    //   [PASS] Test 1: Memory hotpatch apply (2.34 ms)
    //   [FAIL] Test 2: Byte search pattern → detail (1.23 ms)
    //   [SKIP] Test 3: GPU compute

    std::istringstream stream(output);
    std::string line;
    TestNode currentSuite;
    currentSuite.name = "All Tests";
    currentSuite.status = TestStatus::Passed;

    while (std::getline(stream, line)) {
        // Check for [PASS], [FAIL], [SKIP]
        TestStatus status = TestStatus::Unknown;
        size_t statusEnd = 0;

        if (line.find("[PASS]") != std::string::npos) {
            status = TestStatus::Passed;
            statusEnd = line.find("[PASS]") + 6;
        } else if (line.find("[FAIL]") != std::string::npos) {
            status = TestStatus::Failed;
            statusEnd = line.find("[FAIL]") + 6;
            currentSuite.status = TestStatus::Failed;
        } else if (line.find("[SKIP]") != std::string::npos) {
            status = TestStatus::Skipped;
            statusEnd = line.find("[SKIP]") + 6;
        } else if (line.find("PASS") != std::string::npos && line.find("Test") != std::string::npos) {
            status = TestStatus::Passed;
            statusEnd = line.find("PASS") + 4;
        } else if (line.find("FAIL") != std::string::npos && line.find("Test") != std::string::npos) {
            status = TestStatus::Failed;
            statusEnd = line.find("FAIL") + 4;
            currentSuite.status = TestStatus::Failed;
        }

        if (status == TestStatus::Unknown) continue;

        TestNode node;
        node.status = status;

        // Extract test name (everything after status marker, before timing)
        std::string rest = line.substr(statusEnd);
        // Trim leading spaces and colons
        size_t nameStart = rest.find_first_not_of(" :\t");
        if (nameStart != std::string::npos)
            rest = rest.substr(nameStart);

        // Extract duration if present: (X.XX ms)
        size_t timeStart = rest.rfind('(');
        size_t timeEnd   = rest.rfind(')');
        if (timeStart != std::string::npos && timeEnd != std::string::npos && timeEnd > timeStart) {
            std::string timeStr = rest.substr(timeStart + 1, timeEnd - timeStart - 1);
            // Parse "2.34 ms"
            double ms = 0;
            if (sscanf(timeStr.c_str(), "%lf", &ms) == 1) {
                node.durationMs = ms;
            }
            rest = rest.substr(0, timeStart);
        }

        // Trim trailing whitespace
        while (!rest.empty() && (rest.back() == ' ' || rest.back() == '\t'))
            rest.pop_back();

        // Check for error detail after →
        size_t arrowPos = rest.find("\xe2\x86\x92"); // UTF-8 →
        if (arrowPos == std::string::npos) arrowPos = rest.find("->");
        if (arrowPos != std::string::npos) {
            node.errorMessage = rest.substr(arrowPos + (rest[arrowPos] == '-' ? 2 : 3));
            rest = rest.substr(0, arrowPos);
            while (!rest.empty() && rest.back() == ' ') rest.pop_back();
        }

        node.name = rest;
        currentSuite.children.push_back(node);
    }

    nodes.push_back(currentSuite);
}

// ============================================================================
// Status icon text (Unicode symbols)
// ============================================================================

static const wchar_t* testStatusIcon(TestStatus s) {
    switch (s) {
        case TestStatus::Passed:  return L"\u2714";  // ✔
        case TestStatus::Failed:  return L"\u2718";  // ✘
        case TestStatus::Skipped: return L"\u25CB";  // ○
        case TestStatus::Running: return L"\u25B6";  // ▶
        default:                  return L"\u25A1";  // □
    }
}

// ============================================================================
// Populate TreeView from TestNodes
// ============================================================================

static void populateTestTree(HWND hwndTree, const std::vector<TestNode>& nodes, HTREEITEM parent = TVI_ROOT) {
    for (size_t i = 0; i < nodes.size(); ++i) {
        const TestNode& n = nodes[i];

        // Build display text: "✔ TestName (2.34ms)"
        wchar_t display[256] = {};
        wchar_t nameW[200] = {};
        MultiByteToWideChar(CP_UTF8, 0, n.name.c_str(), -1, nameW, 199);

        if (n.durationMs > 0) {
            swprintf(display, 256, L"%s %s (%.2f ms)", testStatusIcon(n.status), nameW, n.durationMs);
        } else {
            swprintf(display, 256, L"%s %s", testStatusIcon(n.status), nameW);
        }

        TVINSERTSTRUCTW tvis{};
        tvis.hParent      = parent;
        tvis.hInsertAfter = TVI_LAST;
        tvis.item.mask    = TVIF_TEXT | TVIF_PARAM;
        tvis.item.pszText = display;
        tvis.item.lParam  = (LPARAM)i;

        HTREEITEM hItem = (HTREEITEM)SendMessageW(hwndTree, TVM_INSERTITEMW, 0, (LPARAM)&tvis);

        // Recurse into children
        if (!n.children.empty()) {
            populateTestTree(hwndTree, n.children, hItem);
            SendMessageW(hwndTree, TVM_EXPAND, TVE_EXPAND, (LPARAM)hItem);
        }
    }
}

// ============================================================================
// Window Procedure
// ============================================================================

#define IDC_TE_RUN_BTN     3001
#define IDC_TE_REFRESH_BTN 3002
#define IDC_TE_COLLAPSE_BTN 3003

static LRESULT CALLBACK testExplorerWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE: {
        // Toolbar
        CreateWindowExW(0, L"BUTTON", L"\u25B6 Run All",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            10, 5, 80, 28, hwnd, (HMENU)IDC_TE_RUN_BTN,
            GetModuleHandleW(nullptr), nullptr);

        CreateWindowExW(0, L"BUTTON", L"\u21BB Refresh",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            95, 5, 80, 28, hwnd, (HMENU)IDC_TE_REFRESH_BTN,
            GetModuleHandleW(nullptr), nullptr);

        CreateWindowExW(0, L"BUTTON", L"\u25BC Collapse",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            180, 5, 80, 28, hwnd, (HMENU)IDC_TE_COLLAPSE_BTN,
            GetModuleHandleW(nullptr), nullptr);

        // TreeView
        s_hwndTestTree = CreateWindowExW(0, WC_TREEVIEWW, L"",
            WS_CHILD | WS_VISIBLE | WS_BORDER |
            TVS_HASLINES | TVS_LINESATROOT | TVS_HASBUTTONS | TVS_SHOWSELALWAYS,
            0, 38, 0, 0, hwnd, (HMENU)3010,
            GetModuleHandleW(nullptr), nullptr);

        if (s_hwndTestTree) {
            // Dark theme colors
            SendMessageW(s_hwndTestTree, TVM_SETBKCOLOR, 0, RGB(30, 30, 30));
            SendMessageW(s_hwndTestTree, TVM_SETTEXTCOLOR, 0, RGB(220, 220, 220));

            // Populate with current test data
            populateTestTree(s_hwndTestTree, s_testNodes);
        }
        return 0;
    }

    case WM_SIZE: {
        RECT rc;
        GetClientRect(hwnd, &rc);
        if (s_hwndTestTree)
            MoveWindow(s_hwndTestTree, 0, 38, rc.right, rc.bottom - 38, TRUE);
        return 0;
    }

    case WM_COMMAND: {
        int wmId = LOWORD(wParam);
        if (wmId == IDC_TE_RUN_BTN) {
            // Post run command to parent IDE
            PostMessageW(GetParent(hwnd), WM_COMMAND, Win32IDE::IDM_TESTEXPLORER_RUN, 0);
        } else if (wmId == IDC_TE_REFRESH_BTN) {
            PostMessageW(GetParent(hwnd), WM_COMMAND, Win32IDE::IDM_TESTEXPLORER_REFRESH, 0);
        } else if (wmId == IDC_TE_COLLAPSE_BTN) {
            // Collapse all
            HTREEITEM hRoot = (HTREEITEM)SendMessageW(s_hwndTestTree, TVM_GETNEXTITEM, TVGN_ROOT, 0);
            while (hRoot) {
                SendMessageW(s_hwndTestTree, TVM_EXPAND, TVE_COLLAPSE, (LPARAM)hRoot);
                hRoot = (HTREEITEM)SendMessageW(s_hwndTestTree, TVM_GETNEXTITEM, TVGN_NEXT, (LPARAM)hRoot);
            }
        }
        return 0;
    }

    case WM_NOTIFY: {
        NMHDR* nmh = (NMHDR*)lParam;
        if (nmh && nmh->code == NM_CUSTOMDRAW && nmh->hwndFrom == s_hwndTestTree) {
            LPNMTVCUSTOMDRAW lpcd = (LPNMTVCUSTOMDRAW)lParam;
            switch (lpcd->nmcd.dwDrawStage) {
                case CDDS_PREPAINT:
                    return CDRF_NOTIFYITEMDRAW;
                case CDDS_ITEMPREPAINT: {
                    // Get item text to determine color
                    wchar_t text[256] = {};
                    TVITEMW tvi{};
                    tvi.mask      = TVIF_TEXT;
                    tvi.hItem     = (HTREEITEM)lpcd->nmcd.dwItemSpec;
                    tvi.pszText   = text;
                    tvi.cchTextMax = 255;
                    SendMessageW(s_hwndTestTree, TVM_GETITEMW, 0, (LPARAM)&tvi);

                    // Color based on status icon
                    if (wcsstr(text, L"\u2714")) {
                        lpcd->clrText = RGB(50, 205, 50);  // Green for pass
                    } else if (wcsstr(text, L"\u2718")) {
                        lpcd->clrText = RGB(255, 80, 80);  // Red for fail
                    } else if (wcsstr(text, L"\u25CB")) {
                        lpcd->clrText = RGB(180, 180, 50); // Yellow for skipped
                    } else if (wcsstr(text, L"\u25B6")) {
                        lpcd->clrText = RGB(80, 180, 255); // Blue for running
                    }
                    lpcd->clrTextBk = RGB(30, 30, 30);
                    return CDRF_DODEFAULT;
                }
                default:
                    return CDRF_DODEFAULT;
            }
        }
        break;
    }

    case WM_ERASEBKGND:
        return 1;

    case WM_DESTROY:
        s_hwndTestExplorer = nullptr;
        s_hwndTestTree     = nullptr;
        return 0;
    }

    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

// ============================================================================
// Ensure window class
// ============================================================================

static bool ensureTestExplorerClass() {
    if (s_testExplorerClassRegistered) return true;

    WNDCLASSEXW wc{};
    wc.cbSize        = sizeof(wc);
    wc.style         = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc   = testExplorerWndProc;
    wc.cbWndExtra    = sizeof(void*);
    wc.hInstance      = GetModuleHandleW(nullptr);
    wc.hCursor        = LoadCursorW(nullptr, (LPCWSTR)(uintptr_t)IDC_ARROW);
    wc.hbrBackground  = CreateSolidBrush(RGB(30, 30, 30));
    wc.lpszClassName  = TEST_EXPLORER_CLASS;

    if (!RegisterClassExW(&wc)) return false;
    s_testExplorerClassRegistered = true;
    return true;
}

// ============================================================================
// Initialization
// ============================================================================

void Win32IDE::initTestExplorer() {
    if (m_testExplorerInitialized) return;
    OutputDebugStringA("[TestExplorer] Tier 5 — Test Explorer Tree initialized.\n");
    m_testExplorerInitialized = true;
    appendToOutput("[TestExplorer] Test Explorer ready. Run tests to populate tree.\n");
}

// ============================================================================
// Command Router
// ============================================================================

bool Win32IDE::handleTestExplorerCommand(int commandId) {
    if (!m_testExplorerInitialized) initTestExplorer();
    switch (commandId) {
        case IDM_TESTEXPLORER_SHOW:      cmdTestExplorerShow();    return true;
        case IDM_TESTEXPLORER_RUN:       cmdTestExplorerRun();     return true;
        case IDM_TESTEXPLORER_REFRESH:   cmdTestExplorerRefresh(); return true;
        case IDM_TESTEXPLORER_FILTER:    cmdTestExplorerFilter();  return true;
        default: return false;
    }
}

// ============================================================================
// Show Test Explorer Window
// ============================================================================

void Win32IDE::cmdTestExplorerShow() {
    if (s_hwndTestExplorer && IsWindow(s_hwndTestExplorer)) {
        SetForegroundWindow(s_hwndTestExplorer);
        return;
    }

    if (!ensureTestExplorerClass()) {
        MessageBoxW(getMainWindow(), L"Failed to register test explorer class.",
                    L"Test Explorer Error", MB_OK | MB_ICONERROR);
        return;
    }

    s_hwndTestExplorer = CreateWindowExW(
        WS_EX_OVERLAPPEDWINDOW,
        TEST_EXPLORER_CLASS,
        L"RawrXD — Test Explorer",
        WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
        CW_USEDEFAULT, CW_USEDEFAULT, 450, 600,
        getMainWindow(), nullptr,
        GetModuleHandleW(nullptr), nullptr);

    if (s_hwndTestExplorer) {
        ShowWindow(s_hwndTestExplorer, SW_SHOW);
        UpdateWindow(s_hwndTestExplorer);
    }
}

// ============================================================================
// Run Tests (delegates to gauntlet, parses output)
// ============================================================================

void Win32IDE::cmdTestExplorerRun() {
    appendToOutput("[TestExplorer] Running test suite...\n");

    // ── Real test discovery: find test executables in workspace ─────────
    std::string workDir;
    {
        if (!m_projectRoot.empty()) {
            workDir = m_projectRoot;
        } else if (!m_currentDirectory.empty()) {
            workDir = m_currentDirectory;
        } else {
            char cwd[MAX_PATH] = {};
            GetCurrentDirectoryA(MAX_PATH, cwd);
            workDir = cwd;
        }
    }

    // Search common build output directories for test executables
    std::vector<std::string> searchDirs = {
        workDir + "\\build\\tests",
        workDir + "\\build\\bin",
        workDir + "\\build\\bin\\Release",
        workDir + "\\build\\Debug",
        workDir + "\\build\\Release",
        workDir + "\\build-ninja\\bin",
        workDir + "\\build\\monolithic\\bin",
        workDir + "\\build\\win32ide\\bin",
        workDir + "\\build",
        workDir + "\\out\\test",
        workDir + "\\bin",
        "D:\\rawrxd\\build\\bin",
        "D:\\rawrxd\\build\\bin\\Release",
        "D:\\rawrxd\\build-ninja\\bin",
        "D:\\rxdn\\bin",
        "D:\\rxdn_ninja\\bin",
    };

    std::vector<std::string> testExes;
    for (const auto& dir : searchDirs) {
        if (!std::filesystem::exists(dir)) continue;
        for (const auto& entry : std::filesystem::directory_iterator(dir)) {
            if (!entry.is_regular_file()) continue;
            std::string name = entry.path().filename().string();
            std::string ext = entry.path().extension().string();
            if (ext != ".exe") continue;
            // Match common test executable patterns
            std::string lower = name;
            std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
            if (lower.find("test") != std::string::npos ||
                lower.find("spec") != std::string::npos ||
                lower.find("check") != std::string::npos ||
                lower.find("gauntlet") != std::string::npos ||
                lower.find("selftest") != std::string::npos) {
                testExes.push_back(entry.path().string());
            }
        }
    }

    // Also check for CTest (CMake) and pytest
    bool hasCTest = std::filesystem::exists(workDir + "\\build\\CTestTestfile.cmake");
    bool hasPytest = std::filesystem::exists(workDir + "\\pytest.ini") ||
                     std::filesystem::exists(workDir + "\\setup.cfg") ||
                     std::filesystem::exists(workDir + "\\pyproject.toml");

    const std::string preferredExe = resolvePreferredWin32IDEExe(workDir);
    const std::vector<ScriptTestCase> qaScripts = discoverQaScripts(workDir, preferredExe);

    std::ostringstream combinedOutput;

    if (testExes.empty() && !hasCTest && !hasPytest && qaScripts.empty()) {
        appendToOutput("[TestExplorer] No test executables found. Checked:\n");
        for (const auto& d : searchDirs) {
            appendToOutput("  " + d + "\n");
        }
        if (preferredExe.empty()) {
            appendToOutput("[TestExplorer] No Win32IDE binary found for smoke/contract validation.\n");
        }
        appendToOutput("[TestExplorer] Build your project with test targets to populate.\n");

        // Generate a diagnostic report as the test output
        combinedOutput << "[SKIP] No test executables discovered in build directories\n";
    } else {
        // ── Run CTest if available ──────────────────────────────────────
        if (hasCTest) {
            appendToOutput("[TestExplorer] Running CTest...\n");
            std::string ctestCmd = "cd /d \"" + workDir + "\\build\" && ctest --output-on-failure 2>&1";

            SECURITY_ATTRIBUTES sa = {};
            sa.nLength = sizeof(sa);
            sa.bInheritHandle = TRUE;
            HANDLE hRead = nullptr, hWrite = nullptr;
            if (!CreatePipe(&hRead, &hWrite, &sa, 0)) {
                combinedOutput << "[FAIL] CTest -> CreatePipe failed (error " << GetLastError() << ")\n";
            } else {
            SetHandleInformation(hRead, HANDLE_FLAG_INHERIT, 0);

            STARTUPINFOA si = { sizeof(si) };
            si.dwFlags = STARTF_USESTDHANDLES;
            si.hStdOutput = hWrite;
            si.hStdError = hWrite;
            PROCESS_INFORMATION pi = {};
            char cmdBuf[2048];
            snprintf(cmdBuf, sizeof(cmdBuf), "cmd.exe /C %s", ctestCmd.c_str());

            if (CreateProcessA(nullptr, cmdBuf, nullptr, nullptr, TRUE,
                              CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) {
                CloseHandle(hWrite); hWrite = nullptr;
                char buf[4096];
                DWORD bytesRead;
                constexpr DWORD kMaxChunk = static_cast<DWORD>(sizeof(buf) - 1);
                while (ReadFile(hRead, buf, kMaxChunk, &bytesRead, nullptr) && bytesRead > 0) {
                    const size_t safeBytes = (bytesRead <= kMaxChunk) ? static_cast<size_t>(bytesRead) : static_cast<size_t>(kMaxChunk);
                    buf[safeBytes] = '\0';
                    combinedOutput.write(buf, static_cast<std::streamsize>(safeBytes));
                }
                WaitForSingleObject(pi.hProcess, 30000);
                CloseHandle(pi.hProcess);
                CloseHandle(pi.hThread);
            }
            }
            if (hWrite) CloseHandle(hWrite);
            if (hRead) CloseHandle(hRead);
        }

        // ── Run pytest if available ─────────────────────────────────────
        if (hasPytest) {
            appendToOutput("[TestExplorer] Running pytest...\n");
            std::string pytestCmd = "python -m pytest --tb=short -q 2>&1";

            SECURITY_ATTRIBUTES sa = {};
            sa.nLength = sizeof(sa);
            sa.bInheritHandle = TRUE;
            HANDLE hRead = nullptr, hWrite = nullptr;
            if (!CreatePipe(&hRead, &hWrite, &sa, 0)) {
                combinedOutput << "[FAIL] pytest -> CreatePipe failed (error " << GetLastError() << ")\n";
            } else {
            SetHandleInformation(hRead, HANDLE_FLAG_INHERIT, 0);

            STARTUPINFOA si = { sizeof(si) };
            si.dwFlags = STARTF_USESTDHANDLES;
            si.hStdOutput = hWrite;
            si.hStdError = hWrite;
            PROCESS_INFORMATION pi = {};
            char cmdBuf[2048];
            snprintf(cmdBuf, sizeof(cmdBuf), "cmd.exe /C %s", pytestCmd.c_str());

            if (CreateProcessA(nullptr, cmdBuf, nullptr, nullptr, TRUE,
                              CREATE_NO_WINDOW, nullptr, workDir.c_str(), &si, &pi)) {
                CloseHandle(hWrite); hWrite = nullptr;
                char buf[4096];
                DWORD bytesRead;
                constexpr DWORD kMaxChunk = static_cast<DWORD>(sizeof(buf) - 1);
                while (ReadFile(hRead, buf, kMaxChunk, &bytesRead, nullptr) && bytesRead > 0) {
                    const size_t safeBytes = (bytesRead <= kMaxChunk) ? static_cast<size_t>(bytesRead) : static_cast<size_t>(kMaxChunk);
                    buf[safeBytes] = '\0';
                    combinedOutput.write(buf, static_cast<std::streamsize>(safeBytes));
                }
                WaitForSingleObject(pi.hProcess, 60000);
                CloseHandle(pi.hProcess);
                CloseHandle(pi.hThread);
            }
            }
            if (hWrite) CloseHandle(hWrite);
            if (hRead) CloseHandle(hRead);
        }

        // ── Run discovered test executables ─────────────────────────────
        for (const auto& exe : testExes) {
            appendToOutput("[TestExplorer] Running: " + exe + "\n");

            SECURITY_ATTRIBUTES sa = {};
            sa.nLength = sizeof(sa);
            sa.bInheritHandle = TRUE;
            HANDLE hRead = nullptr, hWrite = nullptr;
            if (!CreatePipe(&hRead, &hWrite, &sa, 0)) {
                combinedOutput << "[FAIL] " << std::filesystem::path(exe).filename().string()
                               << " -> CreatePipe failed (error " << GetLastError() << ")\n";
                continue;
            }
            SetHandleInformation(hRead, HANDLE_FLAG_INHERIT, 0);

            STARTUPINFOA si = { sizeof(si) };
            si.dwFlags = STARTF_USESTDHANDLES;
            si.hStdOutput = hWrite;
            si.hStdError = hWrite;
            PROCESS_INFORMATION pi = {};

            char cmdBuf[MAX_PATH + 2];
            snprintf(cmdBuf, sizeof(cmdBuf), "\"%s\"", exe.c_str());

            std::string exeDir = std::filesystem::path(exe).parent_path().string();

            if (CreateProcessA(nullptr, cmdBuf, nullptr, nullptr, TRUE,
                              CREATE_NO_WINDOW, nullptr, exeDir.c_str(), &si, &pi)) {
                CloseHandle(hWrite); hWrite = nullptr;
                char buf[4096];
                DWORD bytesRead;
                constexpr DWORD kMaxChunk = static_cast<DWORD>(sizeof(buf) - 1);
                while (ReadFile(hRead, buf, kMaxChunk, &bytesRead, nullptr) && bytesRead > 0) {
                    const size_t safeBytes = (bytesRead <= kMaxChunk) ? static_cast<size_t>(bytesRead) : static_cast<size_t>(kMaxChunk);
                    buf[safeBytes] = '\0';
                    combinedOutput.write(buf, static_cast<std::streamsize>(safeBytes));
                }
                DWORD waitResult = WaitForSingleObject(pi.hProcess, 60000);
                DWORD exitCode = 0;
                GetExitCodeProcess(pi.hProcess, &exitCode);
                CloseHandle(pi.hProcess);
                CloseHandle(pi.hThread);

                if (waitResult == WAIT_TIMEOUT) {
                    combinedOutput << "[FAIL] " << std::filesystem::path(exe).filename().string()
                                   << " -> Timed out after 60 seconds\n";
                }
            } else {
                combinedOutput << "[FAIL] " << std::filesystem::path(exe).filename().string()
                               << " -> Failed to launch (error " << GetLastError() << ")\n";
            }
            if (hWrite) CloseHandle(hWrite);
            if (hRead) CloseHandle(hRead);
        }

        // ── Run curated QA smoke and contract scripts ───────────────────
        for (const auto& qaScript : qaScripts) {
            appendToOutput("[TestExplorer] Running QA script: " + qaScript.name + "\n");

            std::ostringstream command;
            command << "powershell.exe -NoProfile -ExecutionPolicy Bypass -File "
                    << quoteProcessArg(qaScript.scriptPath);
            for (const auto& arg : qaScript.arguments) {
                command << ' ' << quoteProcessArg(arg);
            }

            const ULONGLONG startTick = GetTickCount64();
            ProcessRunResult result = runCapturedProcess(command.str(), workDir, qaScript.timeoutMs);
            const double durationMs = static_cast<double>(GetTickCount64() - startTick);

            if (!result.launched) {
                std::ostringstream detail;
                detail << "launch failed (error " << result.launchError << ')';
                appendSyntheticResult(combinedOutput, "FAIL", qaScript.name, durationMs, detail.str());
                continue;
            }

            if (result.timedOut) {
                appendSyntheticResult(combinedOutput,
                                      "FAIL",
                                      qaScript.name,
                                      durationMs,
                                      "timed out after " + std::to_string(qaScript.timeoutMs / 1000) + " seconds");
                continue;
            }

            const std::string detail = summarizeOutputLine(result.output);
            if (result.exitCode == 0) {
                appendSyntheticResult(combinedOutput, "PASS", qaScript.name, durationMs, detail);
            } else {
                std::ostringstream failureDetail;
                failureDetail << "exit " << result.exitCode;
                if (!detail.empty()) {
                    failureDetail << ": " << detail;
                }
                appendSyntheticResult(combinedOutput, "FAIL", qaScript.name, durationMs, failureDetail.str());
            }
        }
    }

    // ── Parse combined output and populate tree ─────────────────────────
    parseTestOutput(combinedOutput.str(), s_testNodes);

    // Refresh the tree view
    if (s_hwndTestTree && IsWindow(s_hwndTestTree)) {
        SendMessageW(s_hwndTestTree, TVM_DELETEITEM, 0, (LPARAM)TVI_ROOT);
        populateTestTree(s_hwndTestTree, s_testNodes);
    }

    // Count results
    int pass = 0, fail = 0, skip = 0;
    if (!s_testNodes.empty()) {
        for (auto& child : s_testNodes[0].children) {
            if (child.status == TestStatus::Passed)  ++pass;
            else if (child.status == TestStatus::Failed) ++fail;
            else if (child.status == TestStatus::Skipped) ++skip;
        }
    }

    std::ostringstream oss;
    oss << "[TestExplorer] Results: " << pass << " passed, "
        << fail << " failed, " << skip << " skipped\n";
    appendToOutput(oss.str());
}

// ============================================================================
// Refresh (re-parse last output)
// ============================================================================

void Win32IDE::cmdTestExplorerRefresh() {
    if (s_hwndTestTree && IsWindow(s_hwndTestTree)) {
        SendMessageW(s_hwndTestTree, TVM_DELETEITEM, 0, (LPARAM)TVI_ROOT);
        populateTestTree(s_hwndTestTree, s_testNodes);
    }
    appendToOutput("[TestExplorer] Tree refreshed.\n");
}

// ============================================================================
// Filter (show failures only)
// ============================================================================

void Win32IDE::cmdTestExplorerFilter() {
    appendToOutput("[TestExplorer] Filtering to failures only...\n");

    if (s_hwndTestTree && IsWindow(s_hwndTestTree)) {
        SendMessageW(s_hwndTestTree, TVM_DELETEITEM, 0, (LPARAM)TVI_ROOT);

        // Build filtered nodes
        std::vector<TestNode> filtered;
        TestNode root;
        root.name   = "Failed Tests";
        root.status = TestStatus::Failed;

        if (!s_testNodes.empty()) {
            for (auto& child : s_testNodes[0].children) {
                if (child.status == TestStatus::Failed) {
                    root.children.push_back(child);
                }
            }
        }

        filtered.push_back(root);
        populateTestTree(s_hwndTestTree, filtered);
    }
}
