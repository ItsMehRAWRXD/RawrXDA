// ============================================================================
// Win32IDE_TaskRunner.cpp — Task Runner UI (tasks.json / Run Task)
// ============================================================================
// Loads tasks from .vscode/tasks.json or .rawrxd/tasks.json, lists in dialog,
// Run executes selected task (shell command). Completes Top 25 #21 Tasks/launch.
// ============================================================================

#include "Win32IDE.h"
#include <commctrl.h>
#include <fstream>
#include <sstream>
#include <nlohmann/json.hpp>
#include <string>
#include <fstream>
#include <windows.h>
#include <vector>
#include <utility>

#ifndef IDC_TASK_LIST
#define IDC_TASK_LIST 11640
#endif
#ifndef IDC_TASK_RUN_BTN
#define IDC_TASK_RUN_BTN 11641
#endif

static HWND s_hwndTaskList = nullptr;

static LRESULT CALLBACK TaskRunnerWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    Win32IDE* ide = reinterpret_cast<Win32IDE*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
    switch (msg) {
    case WM_CREATE: {
        CREATESTRUCT* cs = reinterpret_cast<CREATESTRUCT*>(lParam);
        SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)cs->lpCreateParams);
        ide = reinterpret_cast<Win32IDE*>(cs->lpCreateParams);
        s_hwndTaskList = CreateWindowExW(0, L"LISTBOX", nullptr,
            WS_CHILD | WS_VISIBLE | LBS_NOTIFY | WS_VSCROLL,
            10, 10, 360, 220, hwnd, (HMENU)(UINT_PTR)IDC_TASK_LIST, nullptr, nullptr);
        CreateWindowW(L"BUTTON", L"Run Task", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            10, 240, 90, 28, hwnd, (HMENU)(UINT_PTR)IDC_TASK_RUN_BTN, nullptr, nullptr);
        CreateWindowW(L"BUTTON", L"Cancel", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            110, 240, 80, 28, hwnd, (HMENU)IDCANCEL, nullptr, nullptr);
        if (ide) {
            const auto& tasks = ide->getTaskRunnerTasks();
            for (const auto& t : tasks)
                SendMessageA(s_hwndTaskList, LB_ADDSTRING, 0, (LPARAM)t.label.c_str());
            if (!tasks.empty())
                SendMessage(s_hwndTaskList, LB_SETCURSEL, 0, 0);
        }
        return 0;
    }
    case WM_COMMAND:
        if (LOWORD(wParam) == IDC_TASK_RUN_BTN && ide && s_hwndTaskList) {
            int idx = (int)SendMessage(s_hwndTaskList, LB_GETCURSEL, 0, 0);
            if (idx >= 0) {
                ide->runSelectedTask(idx);
                DestroyWindow(hwnd);
            }
            return 0;
        }
        if (LOWORD(wParam) == IDCANCEL) {
            DestroyWindow(hwnd);
            return 0;
        }
        break;
    case WM_DESTROY:
        s_hwndTaskList = nullptr;
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

void Win32IDE::showTaskRunnerDialog()
{
    m_taskRunnerTasks.clear();

    std::string root = m_explorerRootPath.empty() ? "." : m_explorerRootPath;
    std::string path1 = root + "\\.vscode\\tasks.json";
    std::string path2 = root + "\\.rawrxd\\tasks.json";

    auto loadJson = [](const std::string& path, nlohmann::json& out) -> bool {
        std::ifstream f(path);
        if (!f) return false;
        try {
            std::ostringstream buf;
            buf << f.rdbuf();
            out = nlohmann::json::parse(buf.str());
            return true;
        } catch (...) { return false; }
    };

    nlohmann::json j;
    if (loadJson(path1, j) || loadJson(path2, j)) {
        if (j.contains("tasks") && j["tasks"].is_array()) {
            for (const auto& t : j["tasks"]) {
                TaskRunnerEntry e;
                e.label = t.value("label", "Unnamed");
                e.command = t.value("command", "");
                if (t.contains("args") && t["args"].is_array())
                    for (const auto& a : t["args"])
                        e.args.push_back(a.is_string() ? a.get<std::string>() : "");
                if (!e.label.empty())
                    m_taskRunnerTasks.push_back(std::move(e));
            }
        }
    }

    if (m_taskRunnerTasks.empty()) {
        m_taskRunnerTasks.push_back({"Build (default)", "cmd", {"/c", "echo Build: add .vscode/tasks.json or .rawrxd/tasks.json"}});
    }

    static bool s_registered = false;
    if (!s_registered) {
        WNDCLASSEXW wc = {};
        wc.cbSize = sizeof(wc);
        wc.lpfnWndProc = TaskRunnerWndProc;
        wc.hInstance = GetModuleHandleW(nullptr);
        wc.lpszClassName = L"RawrXD_TaskRunner";
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
        if (RegisterClassExW(&wc)) s_registered = true;
    }
    if (s_registered) {
        HWND hwnd = CreateWindowExW(0, L"RawrXD_TaskRunner", L"Run Task",
            WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
            CW_USEDEFAULT, CW_USEDEFAULT, 400, 320,
            m_hwndMain, nullptr, GetModuleHandleW(nullptr), this);
        if (hwnd)
            ShowWindow(hwnd, SW_SHOW);
    }

    std::ostringstream oss;
    oss << "=== Run Task ===\nTasks: " << m_taskRunnerTasks.size() << "\n";
    for (size_t i = 0; i < m_taskRunnerTasks.size(); ++i)
        oss << "  " << (i+1) << ". " << m_taskRunnerTasks[i].label << "\n";
    appendToOutput(oss.str(), "Tasks", OutputSeverity::Info);
}

void Win32IDE::runSelectedTask(int index)
{
    if (index < 0 || index >= (int)m_taskRunnerTasks.size()) return;
    const auto& t = m_taskRunnerTasks[index];

    std::string cmdLine = t.command;
    for (const auto& a : t.args)
        cmdLine += " \"" + a + "\"";

    appendToOutput("[Task] Running: " + t.label + "\n  " + cmdLine + "\n", "Tasks", OutputSeverity::Info);

    // ── Create pipes for stdout/stderr capture ─────────────────────────────
    SECURITY_ATTRIBUTES sa = {};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;

    HANDLE hReadPipe = nullptr, hWritePipe = nullptr;
    if (!CreatePipe(&hReadPipe, &hWritePipe, &sa, 0)) {
        appendToOutput("[Task] Failed to create output pipe.\n", "Tasks", OutputSeverity::Error);
        return;
    }
    SetHandleInformation(hReadPipe, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOA si = { sizeof(si) };
    PROCESS_INFORMATION pi = {};
    si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.hStdOutput = hWritePipe;
    si.hStdError  = hWritePipe;
    si.hStdInput  = GetStdHandle(STD_INPUT_HANDLE);
    si.wShowWindow = SW_HIDE;

    std::vector<char> cmdLineCopy(cmdLine.begin(), cmdLine.end());
    cmdLineCopy.push_back('\0');
    if (CreateProcessA(nullptr, cmdLineCopy.data(), nullptr, nullptr, TRUE, 0,
            nullptr, m_explorerRootPath.empty() ? nullptr : m_explorerRootPath.c_str(),
            &si, &pi)) {
        CloseHandle(hWritePipe);
        hWritePipe = nullptr;

        // Read all output
        std::string output;
        char buffer[4096];
        DWORD bytesRead;
        while (ReadFile(hReadPipe, buffer, sizeof(buffer) - 1, &bytesRead, nullptr) && bytesRead > 0) {
            buffer[bytesRead] = '\0';
            output += buffer;
            // Stream output to IDE panel in chunks
            if (output.size() > 2048) {
                appendToOutput(output, "Tasks", OutputSeverity::Info);
                output.clear();
            }
        }
        // Flush remaining
        if (!output.empty()) {
            appendToOutput(output, "Tasks", OutputSeverity::Info);
        }

        WaitForSingleObject(pi.hProcess, 30000);
        DWORD exitCode = 0;
        GetExitCodeProcess(pi.hProcess, &exitCode);

        std::string exitMsg = "[Task] \"" + t.label + "\" exited with code " + std::to_string(exitCode) + "\n";
        appendToOutput(exitMsg, "Tasks",
                       exitCode == 0 ? OutputSeverity::Info : OutputSeverity::Error);

        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    } else {
        CloseHandle(hWritePipe);
        DWORD err = GetLastError();
        appendToOutput("[Task] CreateProcess failed (error " + std::to_string(err) + ").\n",
                       "Tasks", OutputSeverity::Error);
    }
    if (hReadPipe) CloseHandle(hReadPipe);
}

