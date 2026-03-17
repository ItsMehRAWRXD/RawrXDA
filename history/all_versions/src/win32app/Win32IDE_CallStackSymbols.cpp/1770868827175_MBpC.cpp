// ============================================================================
// Win32IDE_CallStackSymbols.cpp — Tier 5 Gap #44: Call Stack Symbols
// ============================================================================
//
// PURPOSE:
//   Integrate PDB symbol resolution into crash dialog and call stack display.
//   Converts raw addresses in crash dumps to function names with line numbers
//   using the existing RawrXD_PDBSymbols infrastructure.
//
// Architecture: C++20 | Win32 | No exceptions | No Qt
// Pattern:      PatchResult-compatible returns
// Rule:         NO SOURCE FILE IS TO BE SIMPLIFIED
// ============================================================================

#include "Win32IDE.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <dbghelp.h>
#include <commctrl.h>
#include <sstream>
#include <iomanip>
#include <vector>
#include <string>
#include <cstdint>

#pragma comment(lib, "dbghelp.lib")

// ============================================================================
// Resolved stack frame
// ============================================================================

struct ResolvedStackFrame {
    uint64_t    address;
    std::string moduleName;
    std::string functionName;
    std::string fileName;
    uint32_t    lineNumber;
    uint32_t    displacement;
    bool        resolved;
};

// ============================================================================
// Symbol resolution engine
// ============================================================================

static bool s_symInitialized = false;

static bool initSymbolEngine() {
    if (s_symInitialized) return true;

    HANDLE hProcess = GetCurrentProcess();
    SymSetOptions(SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS | SYMOPT_LOAD_LINES);

    if (!SymInitialize(hProcess, nullptr, TRUE)) {
        OutputDebugStringA("[CallStack] SymInitialize failed\n");
        return false;
    }

    s_symInitialized = true;
    OutputDebugStringA("[CallStack] Symbol engine initialized.\n");
    return true;
}

static ResolvedStackFrame resolveAddress(uint64_t addr) {
    ResolvedStackFrame frame{};
    frame.address = addr;
    frame.resolved = false;

    HANDLE hProcess = GetCurrentProcess();

    // Resolve module name
    IMAGEHLP_MODULE64 modInfo{};
    modInfo.SizeOfStruct = sizeof(modInfo);
    if (SymGetModuleInfo64(hProcess, addr, &modInfo)) {
        frame.moduleName = modInfo.ModuleName;
    }

    // Resolve function name
    char buffer[sizeof(SYMBOL_INFO) + MAX_SYM_NAME * sizeof(char)];
    PSYMBOL_INFO pSymbol = (PSYMBOL_INFO)buffer;
    pSymbol->SizeOfStruct = sizeof(SYMBOL_INFO);
    pSymbol->MaxNameLen = MAX_SYM_NAME;

    DWORD64 displacement64 = 0;
    if (SymFromAddr(hProcess, addr, &displacement64, pSymbol)) {
        frame.functionName = pSymbol->Name;
        frame.displacement = static_cast<uint32_t>(displacement64);
        frame.resolved = true;
    }

    // Resolve source file + line number
    IMAGEHLP_LINE64 lineInfo{};
    lineInfo.SizeOfStruct = sizeof(lineInfo);
    DWORD displacement32 = 0;
    if (SymGetLineFromAddr64(hProcess, addr, &displacement32, &lineInfo)) {
        if (lineInfo.FileName) {
            frame.fileName = lineInfo.FileName;
        }
        frame.lineNumber = lineInfo.LineNumber;
    }

    return frame;
}

// ============================================================================
// Capture current call stack
// ============================================================================

static std::vector<ResolvedStackFrame> captureCallStack(int skipFrames = 1, int maxFrames = 64) {
    std::vector<ResolvedStackFrame> frames;
    if (!initSymbolEngine()) return frames;

    void* stack[128];
    int captured = CaptureStackBackTrace(skipFrames, (maxFrames < 128 ? maxFrames : 128),
                                         stack, nullptr);

    frames.reserve(captured);
    for (int i = 0; i < captured; ++i) {
        uint64_t addr = reinterpret_cast<uint64_t>(stack[i]);
        frames.push_back(resolveAddress(addr));
    }

    return frames;
}

// ============================================================================
// Format stack frame for display
// ============================================================================

static std::string formatFrame(int index, const ResolvedStackFrame& f) {
    std::ostringstream oss;
    char addrStr[32];
    snprintf(addrStr, sizeof(addrStr), "0x%016llX", (unsigned long long)f.address);

    oss << "  #" << std::setw(2) << index << "  " << addrStr << "  ";

    if (f.resolved) {
        oss << f.functionName;
        if (f.displacement > 0) {
            oss << "+0x" << std::hex << f.displacement << std::dec;
        }
    } else {
        oss << "<unknown>";
    }

    if (!f.moduleName.empty()) {
        oss << "  [" << f.moduleName << "]";
    }

    if (!f.fileName.empty()) {
        oss << "  " << f.fileName << ":" << f.lineNumber;
    }

    oss << "\n";
    return oss.str();
}

// ============================================================================
// Crash dialog with resolved symbols
// ============================================================================

static HWND s_hwndCallStackDialog = nullptr;
static bool s_callStackClassRegistered = false;
static const wchar_t* CALLSTACK_CLASS = L"RawrXD_CallStackDialog";

static std::vector<ResolvedStackFrame> s_displayedStack;

static LRESULT CALLBACK callStackDialogProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE: {
        // Rich edit for stack trace display
        LoadLibraryW(L"Msftedit.dll");
        HWND hwndEdit = CreateWindowExW(0, L"RichEdit50W", L"",
            WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_HSCROLL |
            ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL,
            0, 0, 0, 0, hwnd, (HMENU)4001,
            GetModuleHandleW(nullptr), nullptr);

        if (hwndEdit) {
            // Dark theme
            SendMessageW(hwndEdit, EM_SETBKGNDCOLOR, 0, RGB(30, 30, 30));

            CHARFORMAT2W cf{};
            cf.cbSize = sizeof(cf);
            cf.dwMask = CFM_COLOR | CFM_FACE | CFM_SIZE;
            cf.crTextColor = RGB(220, 220, 220);
            cf.yHeight = 200; // 10pt
            wcscpy_s(cf.szFaceName, LF_FACESIZE, L"Consolas");
            SendMessageW(hwndEdit, EM_SETCHARFORMAT, SCF_ALL, (LPARAM)&cf);

            // Populate with stack trace
            std::ostringstream oss;
            oss << "=== Call Stack (Resolved Symbols) ===\n\n";
            for (int i = 0; i < (int)s_displayedStack.size(); ++i) {
                oss << formatFrame(i, s_displayedStack[i]);
            }
            oss << "\n=== " << s_displayedStack.size() << " frames ===\n";

            std::string text = oss.str();
            SetWindowTextA(hwndEdit, text.c_str());
        }

        // Copy button
        CreateWindowExW(0, L"BUTTON", L"Copy to Clipboard",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            10, 0, 140, 30, hwnd, (HMENU)4002,
            GetModuleHandleW(nullptr), nullptr);

        // Close button
        CreateWindowExW(0, L"BUTTON", L"Close",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            160, 0, 80, 30, hwnd, (HMENU)4003,
            GetModuleHandleW(nullptr), nullptr);

        return 0;
    }

    case WM_SIZE: {
        RECT rc;
        GetClientRect(hwnd, &rc);
        HWND hwndEdit = GetDlgItem(hwnd, 4001);
        if (hwndEdit) {
            MoveWindow(hwndEdit, 0, 35, rc.right, rc.bottom - 35, TRUE);
        }
        return 0;
    }

    case WM_COMMAND: {
        if (LOWORD(wParam) == 4002) {
            // Copy to clipboard
            std::ostringstream oss;
            for (int i = 0; i < (int)s_displayedStack.size(); ++i) {
                oss << formatFrame(i, s_displayedStack[i]);
            }
            std::string text = oss.str();

            if (OpenClipboard(hwnd)) {
                EmptyClipboard();
                HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, text.size() + 1);
                if (hMem) {
                    char* p = (char*)GlobalLock(hMem);
                    memcpy(p, text.c_str(), text.size() + 1);
                    GlobalUnlock(hMem);
                    SetClipboardData(CF_TEXT, hMem);
                }
                CloseClipboard();
            }
        } else if (LOWORD(wParam) == 4003) {
            DestroyWindow(hwnd);
        }
        return 0;
    }

    case WM_DESTROY:
        s_hwndCallStackDialog = nullptr;
        return 0;

    case WM_ERASEBKGND:
        return 1;
    }

    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

static bool ensureCallStackClass() {
    if (s_callStackClassRegistered) return true;

    WNDCLASSEXW wc{};
    wc.cbSize        = sizeof(wc);
    wc.style         = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc   = callStackDialogProc;
    wc.hInstance      = GetModuleHandleW(nullptr);
    wc.hCursor        = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground  = CreateSolidBrush(RGB(30, 30, 30));
    wc.lpszClassName  = CALLSTACK_CLASS;

    if (!RegisterClassExW(&wc)) return false;
    s_callStackClassRegistered = true;
    return true;
}

// ============================================================================
// Initialization
// ============================================================================

void Win32IDE::initCallStackSymbols() {
    if (m_callStackSymbolsInitialized) return;
    initSymbolEngine();
    OutputDebugStringA("[CallStack] Tier 5 — Call stack symbol resolution initialized.\n");
    m_callStackSymbolsInitialized = true;
    appendToOutput("[CallStack] PDB symbol resolution integrated into crash dialog.\n");
}

// ============================================================================
// Command Router
// ============================================================================

bool Win32IDE::handleCallStackCommand(int commandId) {
    if (!m_callStackSymbolsInitialized) initCallStackSymbols();
    switch (commandId) {
        case IDM_CALLSTACK_CAPTURE:  cmdCallStackCapture();  return true;
        case IDM_CALLSTACK_SHOW:     cmdCallStackShowDialog(); return true;
        case IDM_CALLSTACK_COPY:     cmdCallStackCopy();     return true;
        case IDM_CALLSTACK_RESOLVE:  cmdCallStackResolve();  return true;
        default: return false;
    }
}

// ============================================================================
// Capture current call stack with symbols
// ============================================================================

void Win32IDE::cmdCallStackCapture() {
    s_displayedStack = captureCallStack(2, 64);

    std::ostringstream oss;
    oss << "╔══════════════════════════════════════════════════════════════╗\n"
        << "║               CALL STACK (Symbol-Resolved)                 ║\n"
        << "╠══════════════════════════════════════════════════════════════╣\n";

    for (int i = 0; i < (int)s_displayedStack.size(); ++i) {
        auto& f = s_displayedStack[i];
        char line[256];
        if (f.resolved) {
            snprintf(line, sizeof(line), "║  #%-2d  %-40s  %-12s  ║\n",
                     i, f.functionName.c_str(), f.moduleName.c_str());
        } else {
            snprintf(line, sizeof(line), "║  #%-2d  0x%016llX                              ║\n",
                     i, (unsigned long long)f.address);
        }
        oss << line;

        if (!f.fileName.empty()) {
            snprintf(line, sizeof(line), "║        → %s:%u                               ║\n",
                     f.fileName.c_str(), f.lineNumber);
            oss << line;
        }
    }

    oss << "╠══════════════════════════════════════════════════════════════╣\n";
    char summary[128];
    snprintf(summary, sizeof(summary),
             "║  Total: %d frames, %d resolved                              ║\n",
             (int)s_displayedStack.size(),
             (int)std::count_if(s_displayedStack.begin(), s_displayedStack.end(),
                                [](const ResolvedStackFrame& f) { return f.resolved; }));
    oss << summary;
    oss << "╚══════════════════════════════════════════════════════════════╝\n";

    appendToOutput(oss.str());
}

// ============================================================================
// Show call stack dialog window
// ============================================================================

void Win32IDE::cmdCallStackShowDialog() {
    if (s_displayedStack.empty()) {
        s_displayedStack = captureCallStack(2, 64);
    }

    if (s_hwndCallStackDialog && IsWindow(s_hwndCallStackDialog)) {
        SetForegroundWindow(s_hwndCallStackDialog);
        return;
    }

    if (!ensureCallStackClass()) {
        MessageBoxW(m_hwndMain, L"Failed to register call stack dialog class.",
                    L"Call Stack Error", MB_OK | MB_ICONERROR);
        return;
    }

    s_hwndCallStackDialog = CreateWindowExW(
        WS_EX_OVERLAPPEDWINDOW,
        CALLSTACK_CLASS,
        L"RawrXD — Call Stack (Symbol Resolved)",
        WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
        CW_USEDEFAULT, CW_USEDEFAULT, 800, 500,
        m_hwndMain, nullptr,
        GetModuleHandleW(nullptr), nullptr);

    if (s_hwndCallStackDialog) {
        ShowWindow(s_hwndCallStackDialog, SW_SHOW);
        UpdateWindow(s_hwndCallStackDialog);
    }
}

// ============================================================================
// Copy call stack to clipboard
// ============================================================================

void Win32IDE::cmdCallStackCopy() {
    if (s_displayedStack.empty()) {
        s_displayedStack = captureCallStack(2, 64);
    }

    std::ostringstream oss;
    oss << "=== RawrXD Call Stack ===\n";
    for (int i = 0; i < (int)s_displayedStack.size(); ++i) {
        oss << formatFrame(i, s_displayedStack[i]);
    }

    std::string text = oss.str();

    if (OpenClipboard(m_hwndMain)) {
        EmptyClipboard();
        HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, text.size() + 1);
        if (hMem) {
            char* p = (char*)GlobalLock(hMem);
            memcpy(p, text.c_str(), text.size() + 1);
            GlobalUnlock(hMem);
            SetClipboardData(CF_TEXT, hMem);
        }
        CloseClipboard();
        appendToOutput("[CallStack] Stack trace copied to clipboard.\n");
    }
}

// ============================================================================
// Resolve a specific address
// ============================================================================

void Win32IDE::cmdCallStackResolve() {
    // Resolve a hardcoded test address (in production, prompt user)
    uint64_t testAddr = reinterpret_cast<uint64_t>(&Win32IDE::cmdCallStackResolve);
    ResolvedStackFrame f = resolveAddress(testAddr);

    std::ostringstream oss;
    oss << "[CallStack] Resolved address 0x"
        << std::hex << std::uppercase << testAddr << std::dec << ":\n";
    if (f.resolved) {
        oss << "  Function: " << f.functionName << "\n";
        oss << "  Module:   " << f.moduleName << "\n";
        if (!f.fileName.empty()) {
            oss << "  File:     " << f.fileName << ":" << f.lineNumber << "\n";
        }
    } else {
        oss << "  (unresolved)\n";
    }
    appendToOutput(oss.str());
}

#endif // _WIN32
