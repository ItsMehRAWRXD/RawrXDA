// ============================================================================
// Win32IDE_CrashReporter.cpp — Tier 5 Gap #50: Crash Reporter UI
// ============================================================================
//
// PURPOSE:
//   Provide a crash-reporting dialog with "Restart" / "Safe Mode" options,
//   stack trace display, crash log reading from rawrxd_crash.log, and
//   optional telemetry upload. Integrates with CallStackSymbols for
//   symbolicated stack traces.
//
// Architecture: C++20 | Win32 | No exceptions | No Qt
// Pattern:      PatchResult-compatible returns
// Rule:         NO SOURCE FILE IS TO BE SIMPLIFIED
// ============================================================================

#include "Win32IDE.h"
#include <richedit.h>
#include <sstream>
#include <iomanip>
#include <fstream>
#include <string>
#include <vector>
#include <chrono>
#include <ctime>
#include <commctrl.h>
#include <dbghelp.h>

#pragma comment(lib, "dbghelp.lib")

// ============================================================================
// Crash data structures
// ============================================================================

struct CrashFrame {
    DWORD64     address;
    std::string module;
    std::string function;
    std::string file;
    int         line;
};

struct CrashReport {
    std::string     timestamp;
    std::string     exceptionType;
    DWORD           exceptionCode;
    DWORD64         faultAddress;
    std::vector<CrashFrame> stackFrames;
    std::string     buildVersion;
    std::string     osVersion;
    std::string     rawLog;
    bool            isFatal;
};

static CrashReport s_currentCrash;
static std::vector<CrashReport> s_crashHistory;
static HWND s_hwndCrashDialog  = nullptr;
static HWND s_hwndCrashTrace   = nullptr;  // RichEdit for stack trace
static bool s_crashDialogClassRegistered = false;
static const wchar_t* CRASH_DIALOG_CLASS = L"RawrXD_CrashReporter";

// Crash log file path
static const char* CRASH_LOG_FILE = "rawrxd_crash.log";

// Button IDs
#define IDC_CRASH_RESTART    7601
#define IDC_CRASH_SAFEMODE   7602
#define IDC_CRASH_SEND       7603
#define IDC_CRASH_CLOSE      7604
#define IDC_CRASH_COPY       7605
#define IDC_CRASH_DETAILS    7606
#define IDC_CRASH_TRACE_EDIT 7607

// ============================================================================
// Helper: Get current timestamp string
// ============================================================================

static std::string getTimestampString() {
    auto now = std::chrono::system_clock::now();
    auto tt = std::chrono::system_clock::to_time_t(now);
    struct tm local {};
#ifdef _WIN32
    localtime_s(&local, &tt);
#else
    localtime_r(&tt, &local);
#endif
    char buf[64];
    snprintf(buf, sizeof(buf), "%04d-%02d-%02d %02d:%02d:%02d",
             local.tm_year + 1900, local.tm_mon + 1, local.tm_mday,
             local.tm_hour, local.tm_min, local.tm_sec);
    return buf;
}

// ============================================================================
// Helper: Translate exception code to string
// ============================================================================

static const char* exceptionCodeToString(DWORD code) {
    switch (code) {
        case EXCEPTION_ACCESS_VIOLATION:         return "ACCESS_VIOLATION";
        case EXCEPTION_ARRAY_BOUNDS_EXCEEDED:    return "ARRAY_BOUNDS_EXCEEDED";
        case EXCEPTION_BREAKPOINT:               return "BREAKPOINT";
        case EXCEPTION_DATATYPE_MISALIGNMENT:    return "DATATYPE_MISALIGNMENT";
        case EXCEPTION_FLT_DENORMAL_OPERAND:     return "FLT_DENORMAL_OPERAND";
        case EXCEPTION_FLT_DIVIDE_BY_ZERO:       return "FLT_DIVIDE_BY_ZERO";
        case EXCEPTION_FLT_INEXACT_RESULT:       return "FLT_INEXACT_RESULT";
        case EXCEPTION_FLT_INVALID_OPERATION:    return "FLT_INVALID_OPERATION";
        case EXCEPTION_FLT_OVERFLOW:             return "FLT_OVERFLOW";
        case EXCEPTION_FLT_STACK_CHECK:          return "FLT_STACK_CHECK";
        case EXCEPTION_FLT_UNDERFLOW:            return "FLT_UNDERFLOW";
        case EXCEPTION_ILLEGAL_INSTRUCTION:      return "ILLEGAL_INSTRUCTION";
        case EXCEPTION_IN_PAGE_ERROR:            return "IN_PAGE_ERROR";
        case EXCEPTION_INT_DIVIDE_BY_ZERO:       return "INT_DIVIDE_BY_ZERO";
        case EXCEPTION_INT_OVERFLOW:             return "INT_OVERFLOW";
        case EXCEPTION_INVALID_DISPOSITION:      return "INVALID_DISPOSITION";
        case EXCEPTION_NONCONTINUABLE_EXCEPTION: return "NONCONTINUABLE_EXCEPTION";
        case EXCEPTION_PRIV_INSTRUCTION:         return "PRIV_INSTRUCTION";
        case EXCEPTION_SINGLE_STEP:              return "SINGLE_STEP";
        case EXCEPTION_STACK_OVERFLOW:           return "STACK_OVERFLOW";
        default: return "UNKNOWN_EXCEPTION";
    }
}

// ============================================================================
// Capture stack trace using DbgHelp
// ============================================================================

static std::vector<CrashFrame> captureCurrentStackTrace(int skip = 2, int maxFrames = 32) {
    std::vector<CrashFrame> frames;

    HANDLE hProcess = GetCurrentProcess();
    SymInitialize(hProcess, nullptr, TRUE);

    void* stack[64];
    WORD frameCount = CaptureStackBackTrace(skip, (maxFrames < 64 ? maxFrames : 64), stack, nullptr);

    char symbolBuffer[sizeof(SYMBOL_INFO) + MAX_SYM_NAME * sizeof(TCHAR)];

    for (WORD i = 0; i < frameCount; ++i) {
        CrashFrame cFrame;
        cFrame.address = (DWORD64)stack[i];
        cFrame.line = 0;

        SYMBOL_INFO* sym = (SYMBOL_INFO*)symbolBuffer;
        memset(sym, 0, sizeof(SYMBOL_INFO));
        sym->SizeOfStruct = sizeof(SYMBOL_INFO);
        sym->MaxNameLen = MAX_SYM_NAME;

        DWORD64 displacement64 = 0;
        if (SymFromAddr(hProcess, cFrame.address, &displacement64, sym)) {
            cFrame.function = sym->Name;
        } else {
            cFrame.function = "<unknown>";
        }

        // Get line info
        IMAGEHLP_LINE64 lineInfo{};
        lineInfo.SizeOfStruct = sizeof(lineInfo);
        DWORD displacement32 = 0;
        if (SymGetLineFromAddr64(hProcess, cFrame.address, &displacement32, &lineInfo)) {
            cFrame.file = lineInfo.FileName;
            cFrame.line = (int)lineInfo.LineNumber;
        }

        // Get module name
        HMODULE hMod = nullptr;
        if (GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                               GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                               (LPCWSTR)stack[i], &hMod)) {
            wchar_t modPath[MAX_PATH];
            if (GetModuleFileNameW(hMod, modPath, MAX_PATH)) {
                // Extract filename only
                wchar_t* slash = wcsrchr(modPath, L'\\');
                if (slash) {
                    char modName[128];
                    WideCharToMultiByte(CP_UTF8, 0, slash + 1, -1, modName, 128, nullptr, nullptr);
                    cFrame.module = modName;
                }
            }
        }

        if (cFrame.module.empty()) cFrame.module = "<unknown>";

        frames.push_back(cFrame);
    }

    return frames;
}

// ============================================================================
// Format crash report as string
// ============================================================================

static std::string formatCrashReport(const CrashReport& report) {
    std::ostringstream oss;

    oss << "╔══════════════════════════════════════════════════════════════╗\n"
        << "║                    CRASH REPORT                            ║\n"
        << "╠══════════════════════════════════════════════════════════════╣\n"
        << "║  Time:      " << report.timestamp << "                     ║\n"
        << "║  Exception: " << report.exceptionType << "                             ║\n";

    char addrBuf[32];
    snprintf(addrBuf, sizeof(addrBuf), "0x%016llX", report.faultAddress);
    oss << "║  Address:   " << addrBuf << "                    ║\n"
        << "║  Fatal:     " << (report.isFatal ? "YES" : "NO ") << "                                         ║\n"
        << "╠══════════════════════════════════════════════════════════════╣\n"
        << "║  STACK TRACE                                               ║\n"
        << "╠══════════════════════════════════════════════════════════════╣\n";

    for (size_t i = 0; i < report.stackFrames.size(); ++i) {
        const auto& f = report.stackFrames[i];
        char frameLine[256];
        snprintf(frameLine, sizeof(frameLine),
                 "║  #%-3zu 0x%016llX %-20s %-15s\n",
                 i, f.address, f.function.c_str(), f.module.c_str());
        oss << frameLine;

        if (!f.file.empty()) {
            char fileLine[256];
            snprintf(fileLine, sizeof(fileLine),
                     "║       at %s:%d\n", f.file.c_str(), f.line);
            oss << fileLine;
        }
    }

    oss << "╚══════════════════════════════════════════════════════════════╝\n";
    return oss.str();
}

// ============================================================================
// Read crash log from disk
// ============================================================================

static std::string readCrashLog() {
    std::ifstream ifs(CRASH_LOG_FILE);
    if (!ifs.is_open()) return "[No crash log found]\n";

    std::ostringstream oss;
    oss << ifs.rdbuf();
    return oss.str();
}

// ============================================================================
// Write crash report to log
// ============================================================================

static void writeCrashLog(const CrashReport& report) {
    std::ofstream ofs(CRASH_LOG_FILE, std::ios::app);
    if (!ofs.is_open()) return;

    ofs << "=== CRASH " << report.timestamp << " ===\n"
        << "Exception: " << report.exceptionType << "\n"
        << "Code: 0x" << std::hex << report.exceptionCode << std::dec << "\n"
        << "Address: 0x" << std::hex << report.faultAddress << std::dec << "\n"
        << "Fatal: " << (report.isFatal ? "true" : "false") << "\n"
        << "Stack:\n";

    for (size_t i = 0; i < report.stackFrames.size(); ++i) {
        const auto& f = report.stackFrames[i];
        ofs << "  #" << i << " " << f.module << "!" << f.function;
        if (!f.file.empty()) {
            ofs << " at " << f.file << ":" << f.line;
        }
        ofs << " [0x" << std::hex << f.address << std::dec << "]\n";
    }

    ofs << "=== END CRASH ===\n\n";
    ofs.flush();
}

// ============================================================================
// Crash dialog window procedure
// ============================================================================

static LRESULT CALLBACK crashDialogWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE: {
        HINSTANCE hInst = GetModuleHandleW(nullptr);

        // Dark background
        HFONT hFont = CreateFontW(-14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                  DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                  CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");

        HFONT hBoldFont = CreateFontW(-16, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                                      DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                      CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");

        HFONT hMonoFont = CreateFontW(-12, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                      DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                      CLEARTYPE_QUALITY, FIXED_PITCH, L"Cascadia Mono");

        // Title
        HWND hTitle = CreateWindowExW(0, L"STATIC",
            L"\u26A0  RawrXD has encountered a problem",
            WS_CHILD | WS_VISIBLE | SS_LEFT,
            20, 15, 550, 28, hwnd, (HMENU)7610, hInst, nullptr);
        SendMessageW(hTitle, WM_SETFONT, (WPARAM)hBoldFont, TRUE);

        // Description
        HWND hDesc = CreateWindowExW(0, L"STATIC",
            L"An unexpected error occurred. You can restart normally, "
            L"restart in Safe Mode (extensions disabled), or view the crash details.",
            WS_CHILD | WS_VISIBLE | SS_LEFT,
            20, 50, 550, 40, hwnd, (HMENU)7611, hInst, nullptr);
        SendMessageW(hDesc, WM_SETFONT, (WPARAM)hFont, TRUE);

        // Stack trace RichEdit
        LoadLibraryW(L"Msftedit.dll");
        s_hwndCrashTrace = CreateWindowExW(
            WS_EX_CLIENTEDGE,
            L"RICHEDIT50W",
            L"",
            WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_HSCROLL |
            ES_MULTILINE | ES_READONLY | ES_AUTOHSCROLL | ES_AUTOVSCROLL,
            20, 100, 550, 250,
            hwnd, (HMENU)IDC_CRASH_TRACE_EDIT, hInst, nullptr);

        if (s_hwndCrashTrace) {
            SendMessageW(s_hwndCrashTrace, WM_SETFONT, (WPARAM)hMonoFont, TRUE);

            // Dark theme for RichEdit
            SendMessageW(s_hwndCrashTrace, EM_SETBKGNDCOLOR, 0, (LPARAM)RGB(30, 30, 30));

            CHARFORMAT2W cf{};
            cf.cbSize = sizeof(cf);
            cf.dwMask = CFM_COLOR | CFM_FACE | CFM_SIZE;
            cf.crTextColor = RGB(220, 220, 220);
            cf.yHeight = 180;
            wcscpy_s(cf.szFaceName, LF_FACESIZE, L"Cascadia Mono");
            SendMessageW(s_hwndCrashTrace, EM_SETCHARFORMAT, SCF_ALL, (LPARAM)&cf);

            // Populate with crash report
            std::string report = formatCrashReport(s_currentCrash);
            int len = MultiByteToWideChar(CP_UTF8, 0, report.c_str(), -1, nullptr, 0);
            std::vector<wchar_t> wBuf(len);
            MultiByteToWideChar(CP_UTF8, 0, report.c_str(), -1, wBuf.data(), len);
            SetWindowTextW(s_hwndCrashTrace, wBuf.data());
        }

        // Buttons row
        int btnY = 365;
        int btnW = 120;
        int btnH = 32;

        HWND hRestart = CreateWindowExW(0, L"BUTTON", L"\u21BB Restart",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            20, btnY, btnW, btnH, hwnd, (HMENU)IDC_CRASH_RESTART, hInst, nullptr);
        SendMessageW(hRestart, WM_SETFONT, (WPARAM)hFont, TRUE);

        HWND hSafe = CreateWindowExW(0, L"BUTTON", L"\U0001F6E1 Safe Mode",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            150, btnY, btnW, btnH, hwnd, (HMENU)IDC_CRASH_SAFEMODE, hInst, nullptr);
        SendMessageW(hSafe, WM_SETFONT, (WPARAM)hFont, TRUE);

        HWND hSend = CreateWindowExW(0, L"BUTTON", L"\U0001F4E4 Send Report",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            280, btnY, btnW, btnH, hwnd, (HMENU)IDC_CRASH_SEND, hInst, nullptr);
        SendMessageW(hSend, WM_SETFONT, (WPARAM)hFont, TRUE);

        HWND hCopy = CreateWindowExW(0, L"BUTTON", L"\U0001F4CB Copy",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            410, btnY, 80, btnH, hwnd, (HMENU)IDC_CRASH_COPY, hInst, nullptr);
        SendMessageW(hCopy, WM_SETFONT, (WPARAM)hFont, TRUE);

        HWND hClose = CreateWindowExW(0, L"BUTTON", L"Close",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            500, btnY, 70, btnH, hwnd, (HMENU)IDC_CRASH_CLOSE, hInst, nullptr);
        SendMessageW(hClose, WM_SETFONT, (WPARAM)hFont, TRUE);

        return 0;
    }

    case WM_COMMAND: {
        switch (LOWORD(wParam)) {
        case IDC_CRASH_RESTART: {
            // Restart the application
            wchar_t exePath[MAX_PATH];
            GetModuleFileNameW(nullptr, exePath, MAX_PATH);

            STARTUPINFOW si{};
            si.cb = sizeof(si);
            PROCESS_INFORMATION pi{};
            CreateProcessW(exePath, nullptr, nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi);
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);

            DestroyWindow(hwnd);
            PostQuitMessage(0);
            return 0;
        }

        case IDC_CRASH_SAFEMODE: {
            // Restart with --safe-mode flag
            wchar_t exePath[MAX_PATH];
            GetModuleFileNameW(nullptr, exePath, MAX_PATH);

            wchar_t cmdLine[MAX_PATH + 32];
            swprintf(cmdLine, MAX_PATH + 32, L"\"%s\" --safe-mode", exePath);

            STARTUPINFOW si{};
            si.cb = sizeof(si);
            PROCESS_INFORMATION pi{};
            CreateProcessW(nullptr, cmdLine, nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi);
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);

            DestroyWindow(hwnd);
            PostQuitMessage(0);
            return 0;
        }

        case IDC_CRASH_SEND: {
            // Simulate sending crash report (telemetry endpoint)
            writeCrashLog(s_currentCrash);
            MessageBoxW(hwnd,
                        L"Crash report saved to rawrxd_crash.log.\n"
                        L"Telemetry upload: (endpoint not configured)",
                        L"Report Sent", MB_OK | MB_ICONINFORMATION);
            return 0;
        }

        case IDC_CRASH_COPY: {
            // Copy stack trace to clipboard
            std::string report = formatCrashReport(s_currentCrash);
            int len = MultiByteToWideChar(CP_UTF8, 0, report.c_str(), -1, nullptr, 0);
            if (OpenClipboard(hwnd)) {
                EmptyClipboard();
                HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, len * sizeof(wchar_t));
                if (hMem) {
                    wchar_t* p = (wchar_t*)GlobalLock(hMem);
                    MultiByteToWideChar(CP_UTF8, 0, report.c_str(), -1, p, len);
                    GlobalUnlock(hMem);
                    SetClipboardData(CF_UNICODETEXT, hMem);
                }
                CloseClipboard();
            }
            return 0;
        }

        case IDC_CRASH_CLOSE:
            DestroyWindow(hwnd);
            return 0;
        }
        return 0;
    }

    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        RECT rc;
        GetClientRect(hwnd, &rc);
        HBRUSH hBrush = CreateSolidBrush(RGB(37, 37, 38));
        FillRect(hdc, &rc, hBrush);
        DeleteObject(hBrush);

        // Draw separator line above buttons
        HPEN hPen = CreatePen(PS_SOLID, 1, RGB(80, 80, 80));
        HPEN hOldPen = (HPEN)SelectObject(hdc, hPen);
        MoveToEx(hdc, 20, 355, nullptr);
        LineTo(hdc, rc.right - 20, 355);
        SelectObject(hdc, hOldPen);
        DeleteObject(hPen);

        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_CTLCOLORSTATIC: {
        HDC hdcCtrl = (HDC)wParam;
        SetTextColor(hdcCtrl, RGB(220, 220, 220));
        SetBkColor(hdcCtrl, RGB(37, 37, 38));
        static HBRUSH hBrStatic = CreateSolidBrush(RGB(37, 37, 38));
        return (LRESULT)hBrStatic;
    }

    case WM_ERASEBKGND:
        return 1;

    case WM_DESTROY:
        s_hwndCrashDialog = nullptr;
        s_hwndCrashTrace  = nullptr;
        return 0;
    }

    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

static bool ensureCrashDialogClass() {
    if (s_crashDialogClassRegistered) return true;

    WNDCLASSEXW wc{};
    wc.cbSize        = sizeof(wc);
    wc.style         = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc   = crashDialogWndProc;
    wc.hInstance      = GetModuleHandleW(nullptr);
    wc.hCursor        = LoadCursorW(nullptr, (LPCWSTR)(uintptr_t)IDC_ARROW);
    wc.hbrBackground  = CreateSolidBrush(RGB(37, 37, 38));
    wc.lpszClassName  = CRASH_DIALOG_CLASS;

    if (!RegisterClassExW(&wc)) return false;
    s_crashDialogClassRegistered = true;
    return true;
}

// ============================================================================
// Structured exception handler (to install as top-level filter)
// ============================================================================

static LONG WINAPI rawrxdUnhandledExceptionFilter(PEXCEPTION_POINTERS pExceptions) {
    if (!pExceptions || !pExceptions->ExceptionRecord) {
        return EXCEPTION_CONTINUE_SEARCH;
    }

    s_currentCrash = CrashReport{};
    s_currentCrash.timestamp     = getTimestampString();
    s_currentCrash.exceptionCode = pExceptions->ExceptionRecord->ExceptionCode;
    s_currentCrash.exceptionType = exceptionCodeToString(pExceptions->ExceptionRecord->ExceptionCode);
    s_currentCrash.faultAddress  = (DWORD64)pExceptions->ExceptionRecord->ExceptionAddress;
    s_currentCrash.isFatal       = true;
    s_currentCrash.buildVersion  = "RawrXD-Shell v1.0";
    s_currentCrash.stackFrames   = captureCurrentStackTrace(0, 32);

    // Write crash log immediately
    writeCrashLog(s_currentCrash);
    s_crashHistory.push_back(s_currentCrash);

    // Show crash dialog
    if (ensureCrashDialogClass()) {
        s_hwndCrashDialog = CreateWindowExW(
            WS_EX_TOPMOST,
            CRASH_DIALOG_CLASS,
            L"RawrXD — Crash Reporter",
            WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
            CW_USEDEFAULT, CW_USEDEFAULT, 610, 440,
            nullptr, nullptr,
            GetModuleHandleW(nullptr), nullptr);

        if (s_hwndCrashDialog) {
            ShowWindow(s_hwndCrashDialog, SW_SHOW);
            UpdateWindow(s_hwndCrashDialog);

            // Run a modal loop for the crash dialog
            MSG msg;
            while (GetMessageW(&msg, nullptr, 0, 0)) {
                TranslateMessage(&msg);
                DispatchMessageW(&msg);
                if (!IsWindow(s_hwndCrashDialog)) break;
            }
        }
    }

    return EXCEPTION_EXECUTE_HANDLER;
}

// ============================================================================
// Initialization
// ============================================================================

void Win32IDE::initCrashReporter() {
    if (m_crashReporterInitialized) return;

    // Install top-level exception filter
    SetUnhandledExceptionFilter(rawrxdUnhandledExceptionFilter);

    OutputDebugStringA("[CrashReporter] Tier 5 — Crash reporter installed.\n");
    m_crashReporterInitialized = true;
    appendToOutput("[CrashReporter] Unhandled exception filter installed.\n");
}

// ============================================================================
// Command Router
// ============================================================================

bool Win32IDE::handleCrashReporterCommand(int commandId) {
    if (!m_crashReporterInitialized) initCrashReporter();
    switch (commandId) {
        case IDM_CRASH_SHOW:      cmdCrashShow();      return true;
        case IDM_CRASH_TEST:      cmdCrashTest();      return true;
        case IDM_CRASH_LOG:       cmdCrashLog();       return true;
        case IDM_CRASH_CLEAR:     cmdCrashClear();     return true;
        case IDM_CRASH_STATS:     cmdCrashStats();     return true;
        default: return false;
    }
}

// ============================================================================
// Show crash dialog with simulated/previous crash data
// ============================================================================

void Win32IDE::cmdCrashShow() {
    if (s_hwndCrashDialog && IsWindow(s_hwndCrashDialog)) {
        SetForegroundWindow(s_hwndCrashDialog);
        return;
    }

    // If no current crash, generate a demo
    if (s_currentCrash.timestamp.empty()) {
        s_currentCrash.timestamp     = getTimestampString();
        s_currentCrash.exceptionType = "DEMO — No real crash";
        s_currentCrash.exceptionCode = 0;
        s_currentCrash.faultAddress  = 0;
        s_currentCrash.isFatal       = false;
        s_currentCrash.stackFrames   = captureCurrentStackTrace(1, 16);
    }

    if (!ensureCrashDialogClass()) {
        MessageBoxW(m_hwndMain, L"Failed to register crash dialog class.",
                    L"Crash Reporter Error", MB_OK | MB_ICONERROR);
        return;
    }

    s_hwndCrashDialog = CreateWindowExW(
        WS_EX_TOPMOST,
        CRASH_DIALOG_CLASS,
        L"RawrXD — Crash Reporter",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
        CW_USEDEFAULT, CW_USEDEFAULT, 610, 440,
        m_hwndMain, nullptr,
        GetModuleHandleW(nullptr), nullptr);

    if (s_hwndCrashDialog) {
        ShowWindow(s_hwndCrashDialog, SW_SHOW);
        UpdateWindow(s_hwndCrashDialog);
    }
}

// ============================================================================
// Simulate a test crash (non-fatal)
// ============================================================================

void Win32IDE::cmdCrashTest() {
    s_currentCrash = CrashReport{};
    s_currentCrash.timestamp     = getTimestampString();
    s_currentCrash.exceptionType = "TEST_CRASH (simulated)";
    s_currentCrash.exceptionCode = 0xDEADBEEF;
    s_currentCrash.faultAddress  = 0x00007FF6'DEADBEEF;
    s_currentCrash.isFatal       = false;
    s_currentCrash.buildVersion  = "RawrXD-Shell v1.0-dev";
    s_currentCrash.stackFrames   = captureCurrentStackTrace(1, 16);

    s_crashHistory.push_back(s_currentCrash);
    writeCrashLog(s_currentCrash);

    appendToOutput("[CrashReporter] Test crash generated:\n");
    appendToOutput(formatCrashReport(s_currentCrash));

    cmdCrashShow();
}

// ============================================================================
// Display crash log contents
// ============================================================================

void Win32IDE::cmdCrashLog() {
    std::string logContent = readCrashLog();

    std::ostringstream oss;
    oss << "╔══════════════════════════════════════════════════════════════╗\n"
        << "║                    CRASH LOG                               ║\n"
        << "╠══════════════════════════════════════════════════════════════╣\n"
        << logContent
        << "╚══════════════════════════════════════════════════════════════╝\n";

    appendToOutput(oss.str());
}

// ============================================================================
// Clear crash history
// ============================================================================

void Win32IDE::cmdCrashClear() {
    s_crashHistory.clear();
    s_currentCrash = CrashReport{};

    // Delete log file
    std::remove(CRASH_LOG_FILE);

    appendToOutput("[CrashReporter] Crash history cleared.\n");
}

// ============================================================================
// Display crash statistics
// ============================================================================

void Win32IDE::cmdCrashStats() {
    std::ostringstream oss;
    oss << "╔══════════════════════════════════════════════════════════════╗\n"
        << "║                  CRASH STATISTICS                          ║\n"
        << "╠══════════════════════════════════════════════════════════════╣\n"
        << "║  Total crashes (session): " << s_crashHistory.size() << "\n"
        << "║  Fatal crashes:           ";

    int fatalCount = 0;
    for (auto& c : s_crashHistory) {
        if (c.isFatal) ++fatalCount;
    }
    oss << fatalCount << "\n";

    // Count by exception type
    std::map<std::string, int> byType;
    for (auto& c : s_crashHistory) {
        byType[c.exceptionType]++;
    }

    if (!byType.empty()) {
        oss << "╠══════════════════════════════════════════════════════════════╣\n"
            << "║  By Exception Type:                                        ║\n";
        for (auto& [type, count] : byType) {
            char line[128];
            snprintf(line, sizeof(line), "║    %-30s  %d\n", type.c_str(), count);
            oss << line;
        }
    }

    // Log file info
    {
        std::ifstream ifs(CRASH_LOG_FILE);
        if (ifs.is_open()) {
            ifs.seekg(0, std::ios::end);
            auto fileSize = ifs.tellg();
            oss << "╠══════════════════════════════════════════════════════════════╣\n"
                << "║  Log file: rawrxd_crash.log (" << fileSize << " bytes)       ║\n";
        } else {
            oss << "╠══════════════════════════════════════════════════════════════╣\n"
                << "║  Log file: (not found)                                     ║\n";
        }
    }

    oss << "╚══════════════════════════════════════════════════════════════╝\n";
    appendToOutput(oss.str());
}
