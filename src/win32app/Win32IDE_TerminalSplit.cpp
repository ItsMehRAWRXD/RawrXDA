// =============================================================================
// Win32IDE Terminal Split Panes — VS Code Parity
// =============================================================================
// Implements horizontal/vertical terminal splits within the terminal panel.
// Each pane is an independent shell process with its own stdin/stdout pipes.
// Supports: split H/V, resize drag, close pane, focus cycling, pane reorder.
// =============================================================================

#include "Win32IDE.h"
#include <richedit.h>
#include <vector>
#include <string>
#include <thread>
#include <atomic>
#include <mutex>
#include <algorithm>
#include <memory>

// ─── Constants ───────────────────────────────────────────────────────────────
static constexpr int SPLITTER_SIZE = 4;
static constexpr COLORREF SPLIT_SPLITTER_CLR   = RGB(68, 68, 68);
static constexpr COLORREF SPLIT_SPLITTER_HOVER = RGB(0, 122, 204);
static constexpr COLORREF SPLIT_TERM_BG        = RGB(30, 30, 30);
static constexpr COLORREF SPLIT_TERM_FG        = RGB(204, 204, 204);

// ─── Split Direction ─────────────────────────────────────────────────────────
enum class SplitDirection {
    None,
    Horizontal,  // side by side (left | right)
    Vertical,    // top/bottom (top --- bottom)
};

// ─── Terminal Pane (split-specific, distinct from header's TerminalPane) ────
struct SplitTermPane {
    int id = 0;
    HWND hwndOutput = nullptr;     // RichEdit for terminal output
    HWND hwndInput = nullptr;      // Edit for input line
    HANDLE hProcess = nullptr;
    HANDLE hReadPipe = nullptr;
    HANDLE hWritePipe = nullptr;
    HANDLE hStdinWrite = nullptr;
    std::unique_ptr<std::thread> readerThread;
    bool running = false;
    std::string title;
    std::string shellExe;
    RECT bounds = {};              // relative to terminal panel
    float splitRatio = 0.5f;       // position of splitter (0.0 - 1.0)
    bool isFocused = false;
};

// ─── Split Pane Manager State ────────────────────────────────────────────────
struct SplitPaneManager {
    std::vector<std::unique_ptr<SplitTermPane>> panes;
    SplitDirection direction = SplitDirection::None;
    int activePaneIndex = 0;
    int nextPaneId = 1;
    HWND hwndContainer = nullptr;
    bool splitterDragging = false;
    int dragStartPos = 0;
    float splitRatio = 0.5f;
    HFONT hFont = nullptr;
    std::mutex mutex;
};

static SplitPaneManager g_splitMgr;

// ─── Forward declarations ────────────────────────────────────────────────────
static void layoutPanes(HWND hwndContainer);
static void spawnShellInPane(SplitTermPane& pane, const std::string& shell);
static void paneReaderThread(SplitTermPane* pane);
static void paneAppendOutput(SplitTermPane& pane, const std::string& text);
static LRESULT CALLBACK SplitContainerProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
static LRESULT CALLBACK PaneInputProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);

// =============================================================================
// splitTerminalHorizontal — Create a horizontal split
// =============================================================================

// splitTerminalHorizontal/Vertical defined in Win32IDE.cpp — use existing implementations

// =============================================================================
// splitTerminalImpl — Core split logic
// =============================================================================

void Win32IDE::splitTerminalImpl(bool horizontal)
{
    SplitDirection dir = horizontal ? SplitDirection::Horizontal : SplitDirection::Vertical;
    std::lock_guard<std::mutex> lock(g_splitMgr.mutex);

    // Get the terminal panel area
    HWND hwndTermPanel = m_hwndPanelContainer;
    if (!hwndTermPanel || !IsWindow(hwndTermPanel)) {
        appendToOutput("[Terminal] No terminal panel available for split\n", "Output", OutputSeverity::Warning);
        return;
    }

    // Create font if needed
    if (!g_splitMgr.hFont) {
        g_splitMgr.hFont = CreateFontA(14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, FIXED_PITCH | FF_MODERN, "Consolas");
    }

    // Register container class once
    static bool classRegistered = false;
    if (!classRegistered) {
        WNDCLASSA wc = {};
        wc.lpfnWndProc = SplitContainerProc;
        wc.hInstance = GetModuleHandle(nullptr);
        wc.lpszClassName = "RawrXD_SplitTermContainer";
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.hbrBackground = CreateSolidBrush(SPLIT_TERM_BG);
        RegisterClassA(&wc);
        classRegistered = true;
    }

    // If no panes exist yet, create the first one from the existing terminal
    if (g_splitMgr.panes.empty()) {
        auto pane = std::make_unique<SplitTermPane>();
        pane->id = g_splitMgr.nextPaneId++;
        pane->title = "Terminal 1";
        pane->shellExe = "powershell.exe";
        g_splitMgr.panes.push_back(std::move(pane));
    }

    // Create the split container if not yet created
    if (!g_splitMgr.hwndContainer) {
        RECT panelRect;
        GetClientRect(hwndTermPanel, &panelRect);

        g_splitMgr.hwndContainer = CreateWindowExA(
            0, "RawrXD_SplitTermContainer", "",
            WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN,
            0, 0, panelRect.right, panelRect.bottom,
            hwndTermPanel, nullptr, GetModuleHandle(nullptr), nullptr);
    }

    // Add a new pane
    auto newPane = std::make_unique<SplitTermPane>();
    newPane->id = g_splitMgr.nextPaneId++;
    newPane->title = "Terminal " + std::to_string(newPane->id);
    newPane->shellExe = "powershell.exe";

    g_splitMgr.direction = dir;
    g_splitMgr.splitRatio = 0.5f;
    g_splitMgr.panes.push_back(std::move(newPane));

    // Create HWND controls for all panes that don't have them yet
    for (auto& pane : g_splitMgr.panes) {
        if (!pane->hwndOutput) {
            int inputH = 22;

            // Output (RichEdit)
            pane->hwndOutput = CreateWindowExA(
                0, RICHEDIT_CLASSA, "",
                WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL,
                0, 0, 100, 100,
                g_splitMgr.hwndContainer, nullptr, GetModuleHandle(nullptr), nullptr);

            SendMessage(pane->hwndOutput, EM_SETBKGNDCOLOR, 0, SPLIT_TERM_BG);
            SendMessage(pane->hwndOutput, WM_SETFONT, (WPARAM)g_splitMgr.hFont, TRUE);

            CHARFORMAT2A cf = {};
            cf.cbSize = sizeof(cf);
            cf.dwMask = CFM_COLOR | CFM_FACE | CFM_SIZE;
            cf.crTextColor = SPLIT_TERM_FG;
            cf.yHeight = 180;
            strcpy_s(cf.szFaceName, "Consolas");
            SendMessageA(pane->hwndOutput, EM_SETCHARFORMAT, SCF_ALL, (LPARAM)&cf);

            // Input
            pane->hwndInput = CreateWindowExA(
                WS_EX_CLIENTEDGE, "EDIT", "",
                WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
                0, 0, 100, inputH,
                g_splitMgr.hwndContainer, nullptr, GetModuleHandle(nullptr), nullptr);
            SendMessage(pane->hwndInput, WM_SETFONT, (WPARAM)g_splitMgr.hFont, TRUE);

            // Subclass input for Enter key → send to shell
            WNDPROC origProc = (WNDPROC)SetWindowLongPtrA(pane->hwndInput, GWLP_WNDPROC, (LONG_PTR)PaneInputProc);
            SetPropA(pane->hwndInput, "OrigProc", (HANDLE)origProc);
            SetPropA(pane->hwndInput, "PaneId", (HANDLE)(intptr_t)pane->id);

            // Spawn shell process
            spawnShellInPane(*pane, pane->shellExe);
        }
    }

    // Layout all panes
    layoutPanes(g_splitMgr.hwndContainer);

    // Focus new pane
    g_splitMgr.activePaneIndex = (int)g_splitMgr.panes.size() - 1;
    if (g_splitMgr.panes.back()->hwndInput) {
        SetFocus(g_splitMgr.panes.back()->hwndInput);
    }

    appendToOutput(std::string("[Terminal] Split created: ") + (horizontal ? "horizontal" : "vertical") + ", " + std::to_string(g_splitMgr.panes.size()) + " panes\n", "Output", OutputSeverity::Info);
}

// =============================================================================
// closeTerminalPane — Close a specific pane
// =============================================================================

// closeTerminalPane(int) defined in Win32IDE.cpp — use existing implementation

// =============================================================================
// focusNextTerminalPane — Cycle focus between panes
// =============================================================================

void Win32IDE::focusNextTerminalPane()
{
    std::lock_guard<std::mutex> lock(g_splitMgr.mutex);
    if (g_splitMgr.panes.size() < 2) return;

    g_splitMgr.activePaneIndex = (g_splitMgr.activePaneIndex + 1) % (int)g_splitMgr.panes.size();
    auto& pane = g_splitMgr.panes[g_splitMgr.activePaneIndex];
    if (pane->hwndInput) SetFocus(pane->hwndInput);
}

// =============================================================================
// layoutPanes — Position all pane windows within the container
// =============================================================================

static void layoutPanes(HWND hwndContainer)
{
    if (!hwndContainer || g_splitMgr.panes.empty()) return;

    RECT rc;
    GetClientRect(hwndContainer, &rc);
    int w = rc.right - rc.left;
    int h = rc.bottom - rc.top;
    int inputH = 22;

    if (g_splitMgr.panes.size() == 1) {
        // Single pane — fill container
        auto& p = g_splitMgr.panes[0];
        MoveWindow(p->hwndOutput, 0, 0, w, h - inputH, TRUE);
        MoveWindow(p->hwndInput, 0, h - inputH, w, inputH, TRUE);
    }
    else if (g_splitMgr.direction == SplitDirection::Horizontal) {
        // Side by side
        int splitX = (int)(w * g_splitMgr.splitRatio);

        for (size_t i = 0; i < g_splitMgr.panes.size(); ++i) {
            auto& p = g_splitMgr.panes[i];
            int px, pw;
            if (i == 0) {
                px = 0;
                pw = splitX - SPLITTER_SIZE / 2;
            } else {
                px = splitX + SPLITTER_SIZE / 2;
                pw = w - px;
            }
            MoveWindow(p->hwndOutput, px, 0, pw, h - inputH, TRUE);
            MoveWindow(p->hwndInput, px, h - inputH, pw, inputH, TRUE);
        }
    }
    else if (g_splitMgr.direction == SplitDirection::Vertical) {
        // Top / bottom
        int splitY = (int)(h * g_splitMgr.splitRatio);

        for (size_t i = 0; i < g_splitMgr.panes.size(); ++i) {
            auto& p = g_splitMgr.panes[i];
            int py, ph;
            if (i == 0) {
                py = 0;
                ph = splitY - SPLITTER_SIZE / 2;
            } else {
                py = splitY + SPLITTER_SIZE / 2;
                ph = h - py;
            }
            MoveWindow(p->hwndOutput, 0, py, w, ph - inputH, TRUE);
            MoveWindow(p->hwndInput, 0, py + ph - inputH, w, inputH, TRUE);
        }
    }
}

// =============================================================================
// spawnShellInPane — Launch a shell process with pipes
// =============================================================================

static void spawnShellInPane(SplitTermPane& pane, const std::string& shell)
{
    SECURITY_ATTRIBUTES sa = {};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;

    // Create stdout pipe
    HANDLE hReadPipe = nullptr, hWritePipe = nullptr;
    if (!CreatePipe(&hReadPipe, &hWritePipe, &sa, 0)) return;
    SetHandleInformation(hReadPipe, HANDLE_FLAG_INHERIT, 0);

    // Create stdin pipe
    HANDLE hStdinRead = nullptr, hStdinWrite = nullptr;
    if (!CreatePipe(&hStdinRead, &hStdinWrite, &sa, 0)) {
        CloseHandle(hReadPipe);
        CloseHandle(hWritePipe);
        return;
    }
    SetHandleInformation(hStdinWrite, HANDLE_FLAG_INHERIT, 0);

    // Launch process
    STARTUPINFOA si = {};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.hStdOutput = hWritePipe;
    si.hStdError = hWritePipe;
    si.hStdInput = hStdinRead;
    si.wShowWindow = SW_HIDE;

    PROCESS_INFORMATION pi = {};
    std::string cmdLine = shell;

    BOOL ok = CreateProcessA(
        nullptr, cmdLine.data(), nullptr, nullptr, TRUE,
        CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi);

    CloseHandle(hWritePipe);
    CloseHandle(hStdinRead);

    if (!ok) {
        CloseHandle(hReadPipe);
        CloseHandle(hStdinWrite);
        paneAppendOutput(pane, "[Failed to start " + shell + "]\r\n");
        return;
    }

    CloseHandle(pi.hThread);
    pane.hProcess = pi.hProcess;
    pane.hReadPipe = hReadPipe;
    pane.hStdinWrite = hStdinWrite;
    pane.running = true;

    // Start reader thread
    pane.readerThread = std::make_unique<std::thread>(paneReaderThread, &pane);
}

// =============================================================================
// paneReaderThread — Background thread reading process stdout
// =============================================================================

static void paneReaderThread(SplitTermPane* pane)
{
    char buffer[4096];
    DWORD bytesRead;
    constexpr DWORD kMaxChunk = static_cast<DWORD>(sizeof(buffer) - 1);

    while (pane->running) {
        // Safe ReadFile into buffer - leave space for null terminator
        if (!ReadFile(pane->hReadPipe, buffer, kMaxChunk, &bytesRead, nullptr) || bytesRead == 0) {
            break;
        }

        const size_t safeBytes = (bytesRead <= kMaxChunk) ? static_cast<size_t>(bytesRead) : static_cast<size_t>(kMaxChunk);
        buffer[safeBytes] = '\0';

        // Post to UI thread
        std::string* text = new std::string(buffer, safeBytes);

        if (pane->hwndOutput && IsWindow(pane->hwndOutput)) {
            // Use WM_APP message to marshal to UI thread
            PostMessageA(pane->hwndOutput, WM_APP + 500, (WPARAM)text, 0);
        } else {
            delete text;
        }
    }

    pane->running = false;
}

// =============================================================================
// paneAppendOutput — Append text to a pane's output RichEdit
// =============================================================================

static void paneAppendOutput(SplitTermPane& pane, const std::string& text)
{
    if (!pane.hwndOutput || !IsWindow(pane.hwndOutput)) return;

    int len = GetWindowTextLengthA(pane.hwndOutput);
    SendMessageA(pane.hwndOutput, EM_SETSEL, len, len);
    SendMessageA(pane.hwndOutput, EM_REPLACESEL, FALSE, (LPARAM)text.c_str());
    SendMessageA(pane.hwndOutput, WM_VSCROLL, SB_BOTTOM, 0);
}

// =============================================================================
// SplitContainerProc — Handles splitter drag and WM_SIZE
// =============================================================================

static LRESULT CALLBACK SplitContainerProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg) {
    case WM_SIZE:
        layoutPanes(hwnd);
        return 0;

    case WM_PAINT: {
        // Draw the splitter bar
        if (g_splitMgr.panes.size() < 2) break;

        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        RECT rc;
        GetClientRect(hwnd, &rc);

        COLORREF clr = g_splitMgr.splitterDragging ? SPLIT_SPLITTER_HOVER : SPLIT_SPLITTER_CLR;
        HBRUSH brush = CreateSolidBrush(clr);

        RECT splitterRect;
        if (g_splitMgr.direction == SplitDirection::Horizontal) {
            int splitX = (int)(rc.right * g_splitMgr.splitRatio);
            splitterRect = { splitX - SPLITTER_SIZE / 2, 0, splitX + SPLITTER_SIZE / 2, rc.bottom };
        } else {
            int splitY = (int)(rc.bottom * g_splitMgr.splitRatio);
            splitterRect = { 0, splitY - SPLITTER_SIZE / 2, rc.right, splitY + SPLITTER_SIZE / 2 };
        }
        FillRect(hdc, &splitterRect, brush);
        DeleteObject(brush);

        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_LBUTTONDOWN: {
        if (g_splitMgr.panes.size() < 2) break;
        int x = LOWORD(lParam), y = HIWORD(lParam);
        RECT rc;
        GetClientRect(hwnd, &rc);

        bool onSplitter = false;
        if (g_splitMgr.direction == SplitDirection::Horizontal) {
            int splitX = (int)(rc.right * g_splitMgr.splitRatio);
            onSplitter = (x >= splitX - SPLITTER_SIZE * 2 && x <= splitX + SPLITTER_SIZE * 2);
        } else {
            int splitY = (int)(rc.bottom * g_splitMgr.splitRatio);
            onSplitter = (y >= splitY - SPLITTER_SIZE * 2 && y <= splitY + SPLITTER_SIZE * 2);
        }

        if (onSplitter) {
            g_splitMgr.splitterDragging = true;
            SetCapture(hwnd);
            SetCursor(LoadCursor(nullptr,
                g_splitMgr.direction == SplitDirection::Horizontal ? IDC_SIZEWE : IDC_SIZENS));
        }
        return 0;
    }

    case WM_MOUSEMOVE: {
        if (g_splitMgr.splitterDragging) {
            RECT rc;
            GetClientRect(hwnd, &rc);
            int x = (short)LOWORD(lParam), y = (short)HIWORD(lParam);

            if (g_splitMgr.direction == SplitDirection::Horizontal) {
                g_splitMgr.splitRatio = std::clamp((float)x / (float)rc.right, 0.15f, 0.85f);
            } else {
                g_splitMgr.splitRatio = std::clamp((float)y / (float)rc.bottom, 0.15f, 0.85f);
            }

            layoutPanes(hwnd);
            InvalidateRect(hwnd, nullptr, TRUE);
        }
        return 0;
    }

    case WM_LBUTTONUP:
        if (g_splitMgr.splitterDragging) {
            g_splitMgr.splitterDragging = false;
            ReleaseCapture();
        }
        return 0;

    case WM_SETCURSOR: {
        if (g_splitMgr.panes.size() >= 2) {
            POINT pt;
            GetCursorPos(&pt);
            ScreenToClient(hwnd, &pt);
            RECT rc;
            GetClientRect(hwnd, &rc);

            bool onSplitter = false;
            if (g_splitMgr.direction == SplitDirection::Horizontal) {
                int splitX = (int)(rc.right * g_splitMgr.splitRatio);
                onSplitter = (pt.x >= splitX - SPLITTER_SIZE * 2 && pt.x <= splitX + SPLITTER_SIZE * 2);
            } else {
                int splitY = (int)(rc.bottom * g_splitMgr.splitRatio);
                onSplitter = (pt.y >= splitY - SPLITTER_SIZE * 2 && pt.y <= splitY + SPLITTER_SIZE * 2);
            }

            if (onSplitter) {
                SetCursor(LoadCursor(nullptr,
                    g_splitMgr.direction == SplitDirection::Horizontal ? IDC_SIZEWE : IDC_SIZENS));
                return TRUE;
            }
        }
        break;
    }
    }

    return DefWindowProcA(hwnd, msg, wParam, lParam);
}

// =============================================================================
// PaneInputProc — Subclassed input field for Enter → shell stdin
// =============================================================================

static LRESULT CALLBACK PaneInputProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    WNDPROC origProc = (WNDPROC)GetPropA(hwnd, "OrigProc");

    if (msg == WM_KEYDOWN && wp == VK_RETURN) {
        char buf[2048] = {};
        GetWindowTextA(hwnd, buf, sizeof(buf));
        std::string text(buf);
        text += "\n";

        // Find which pane this input belongs to
        int paneId = (int)(intptr_t)GetPropA(hwnd, "PaneId");
        std::lock_guard<std::mutex> lock(g_splitMgr.mutex);
        for (auto& pane : g_splitMgr.panes) {
            if (pane->id == paneId && pane->hStdinWrite) {
                DWORD written;
                WriteFile(pane->hStdinWrite, text.c_str(), (DWORD)text.size(), &written, nullptr);
                break;
            }
        }

        SetWindowTextA(hwnd, "");
        return 0;
    }

    if (origProc) return CallWindowProcA(origProc, hwnd, msg, wp, lp);
    return DefWindowProcA(hwnd, msg, wp, lp);
}
