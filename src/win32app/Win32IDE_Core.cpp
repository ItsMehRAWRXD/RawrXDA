// ============================================================================
// Win32IDE_Core.cpp - Core Window Management Functions
// createWindow, showWindow, runMessageLoop, ~Win32IDE, onSize,
// syncEditorToGpuSurface, initializeEditorSurface, trySendToOllama,
// createReverseEngineeringMenu, handleReverseEngineering* — implemented in Win32IDE_ReverseEngineering.cpp (output to
// IDE Output tab)
// ============================================================================

#include "../../include/agentic_autonomous_config.h"
#include "../../include/benchmark_menu_widget.hpp"
#include "../../include/checkpoint_manager.h"
#include "../../include/ci_cd_settings.h"
#include "../../include/enterprise_license.h"
#include "../../include/feature_flags_runtime.h"
#include "../../include/interpretability_panel.h"
#include "../../include/license_enforcement.h"
#include "../../include/model_registry.h"
#include "../../include/multi_file_search.h"
#include "../core/enterprise_license.h"
#include "../cpu_inference_engine.h"
#include "../modules/ExtensionLoader.hpp"
#include "../modules/native_memory.hpp"
#include "../native_agent.hpp"
#include "IDEConfig.h"
#include "IDELogger.h"
#include "ModelConnection.h"
#include "RawrXD_AgentCoordinator.h"
#include "RawrXD_AutonomousAgenticPipeline.h"
#include "Win32IDE.h"
#include "Win32IDE_AgenticBrowser.h"
#include "Win32IDE_ComponentManagers.h"  // Complete types for unique_ptr<T> dtor
#include "Win32IDE_IELabels.h"
#include "enterprise_feature_manager.hpp"
#include "feature_registry_panel.h"
#include "lsp/RawrXD_LSPServer.h"
#include "multi_response_engine.h"
#include "win32_feature_adapter.h"  // Unified Feature Dispatch adapter
#include <commctrl.h>
#include <richedit.h>

#ifndef WM_DPICHANGED
#define WM_DPICHANGED 0x02E0
#endif
#include <algorithm>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include <shlobj.h>
#include <sstream>


// Menu command IDs — must match Win32IDE.cpp definitions
#ifndef IDM_FILE_NEW
#ifndef IDM_BUILD_PROJECT
#define IDM_BUILD_PROJECT 2801
#endif

#define IDM_FILE_NEW 2001
#endif
#ifndef IDM_FILE_OPEN
#define IDM_FILE_OPEN 2002
#endif
#ifndef IDM_FILE_SAVE
#define IDM_FILE_SAVE 2003
#endif
#ifndef IDM_FILE_SAVEAS
#define IDM_FILE_SAVEAS 2004
#endif
#ifndef IDM_EDIT_FIND
#define IDM_EDIT_FIND 2016
#endif
#ifndef IDM_EDIT_REPLACE
#define IDM_EDIT_REPLACE 2017
#endif

// ============================================================================
// Window Class Name
// ============================================================================
static const char* kWindowClassName = "RawrXD_IDE_MainWindow";

// Implemented in src/multi_file_search.cpp (Win32 dialog invoked by MultiFileSearchWidget::show()).
extern void MultiFileSearchWidget_ShowDialog(void* ctx);

// AI workers: process main-thread invoke queue every message (avoids queue buildup).
extern void AIWorkersProcessInvokeQueue();

// ============================================================================
// Destructor
// ============================================================================
Win32IDE::~Win32IDE()
{
    // Ensure shutdown flag is set (may already be from onDestroy)
    m_shuttingDown.store(true, std::memory_order_release);

    // If onDestroy wasn't called (abnormal exit), do the thread wait here
    if (m_activeDetachedThreads.load(std::memory_order_acquire) > 0)
    {
        for (int i = 0; i < 60 && m_activeDetachedThreads.load(std::memory_order_acquire) > 0; ++i)
        {
            Sleep(50);
        }
    }

    // Explicitly destroy objects that detached threads reference BEFORE
    // implicit member destruction order (which is reverse-declaration-order
    // and unpredictable for crash safety).
    m_subAgentManager.reset();
    m_multiResponseEngine.reset();
    // m_agenticBridge is non-owning; do not call reset() on raw pointers.
    m_agenticBridge = nullptr;
    m_agent.reset();
    m_nativeEngine.reset();
    m_modelResolver.reset();
    m_ggufLoader.reset();
    m_extensionLoader.reset();
    m_lspServer.reset();
    m_mcpServer.reset();
    m_renderer.reset();
    m_autonomousPipeline.reset();
    if (m_agentCoordinatorForPipeline)
    {
        DestroyAgentCoordinator((AgentCoordinatorHandle)m_agentCoordinatorForPipeline);
        m_agentCoordinatorForPipeline = nullptr;
    }
    m_autonomyManager.reset();

    // Null out raw pointers to externally-owned objects (deleted in main)
    m_engineManager = nullptr;
    m_codexUltimate = nullptr;

    if (m_backgroundBrush)
    {
        DeleteObject(m_backgroundBrush);
        m_backgroundBrush = nullptr;
    }
    if (m_editorFont)
    {
        DeleteObject(m_editorFont);
        m_editorFont = nullptr;
    }
}

// ============================================================================
// WindowProc - Static callback that routes to instance handleMessage
// ============================================================================
LRESULT CALLBACK Win32IDE::WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    Win32IDE* pThis = nullptr;

    if (uMsg == WM_NCCREATE)
    {
        CREATESTRUCT* cs = reinterpret_cast<CREATESTRUCT*>(lParam);
        pThis = reinterpret_cast<Win32IDE*>(cs->lpCreateParams);
        SetWindowLongPtrA(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pThis));
        pThis->m_hwndMain = hwnd;
    }
    else
    {
        pThis = reinterpret_cast<Win32IDE*>(GetWindowLongPtrA(hwnd, GWLP_USERDATA));
    }

    if (pThis)
    {
        return pThis->handleMessage(hwnd, uMsg, wParam, lParam);
    }

    return DefWindowProcA(hwnd, uMsg, wParam, lParam);
}

// Forward declaration for use in handleMessage (WM_APP+199) and showWindow
static void forceWindowToForeground(HWND hwnd);
static void runWindowVisibilityWatchdog(HWND hwnd);
static void drawLayoutDebugOverlay(HWND hwnd, HDC hdc);

static constexpr UINT_PTR IDT_VISIBILITY_WATCHDOG = 0x7D11;

static bool isLayoutDebugOverlayEnabled()
{
    static bool initialized = false;
    static bool enabled = false;
    if (!initialized)
    {
        initialized = true;
        char value[16] = {};
        DWORD n = GetEnvironmentVariableA("RAWRXD_DEBUG_LAYOUT_OVERLAY", value, (DWORD)sizeof(value));
        enabled = (n > 0 && value[0] == '1');
    }
    return enabled;
}

static void logFirstPaint()
{
    static bool logged = false;
    if (logged)
        return;
    logged = true;
    OutputDebugStringA("[Win32IDE] first_paint\n");
    std::ofstream out("ide_startup.log", std::ios::out | std::ios::app);
    if (out)
    {
        out << "first_paint\n";
        out.flush();
    }
}

static void runWindowVisibilityWatchdog(HWND hwnd)
{
    if (!hwnd || !IsWindow(hwnd))
        return;

    if (IsIconic(hwnd))
    {
        ShowWindow(hwnd, SW_RESTORE);
    }
    if (!IsWindowVisible(hwnd))
    {
        ShowWindow(hwnd, SW_SHOW);
    }

    RECT rc = {};
    GetWindowRect(hwnd, &rc);
    int width = rc.right - rc.left;
    int height = rc.bottom - rc.top;
    if (width < 200 || height < 200)
    {
        SetWindowPos(hwnd, HWND_TOP, 100, 100, 1400, 900, SWP_SHOWWINDOW);
    }

    HMONITOR monitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONULL);
    if (!monitor)
    {
        SetWindowPos(hwnd, HWND_TOP, 100, 100, 1400, 900, SWP_SHOWWINDOW);
    }

    // Keep top-level IDE discoverable without pinning always-on-top.
    BringWindowToTop(hwnd);
}

static void drawLayoutDebugOverlay(HWND hwnd, HDC hdc)
{
    if (!isLayoutDebugOverlayEnabled())
        return;
    RECT rc = {};
    GetClientRect(hwnd, &rc);
    int width = rc.right - rc.left;
    int height = rc.bottom - rc.top;

    char text[128] = {};
    snprintf(text, sizeof(text), "IDE Client Size: %d x %d", width, height);

    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, RGB(255, 80, 80));
    TextOutA(hdc, 10, 10, text, (int)strlen(text));

    if (width < 200 || height < 200)
    {
        const char* warn = "WARNING: Layout collapsed";
        SetTextColor(hdc, RGB(255, 0, 0));
        TextOutA(hdc, 10, 30, warn, (int)strlen(warn));

        HPEN pen = CreatePen(PS_SOLID, 4, RGB(255, 0, 0));
        if (pen)
        {
            HGDIOBJ oldPen = SelectObject(hdc, pen);
            HGDIOBJ oldBrush = SelectObject(hdc, GetStockObject(HOLLOW_BRUSH));
            Rectangle(hdc, 0, 0, (std::max)(width, 1), (std::max)(height, 1));
            SelectObject(hdc, oldBrush);
            SelectObject(hdc, oldPen);
            DeleteObject(pen);
        }
    }
}

// ============================================================================
// SEH wrappers — must be standalone functions without C++ objects (MSVC C2712)
typedef void (*OnCreateFn)(void* self, HWND hwnd);
typedef void (*DeferredInitFn)(void* self);

static void sehCallOnCreate(OnCreateFn fn, void* self, HWND hwnd)
{
#if defined(_MSC_VER)
    __try
    {
        fn(self, hwnd);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        char crashMsg[256];
        snprintf(crashMsg, sizeof(crashMsg),
                 "[RawrXD] SEH exception 0x%08lX caught in onCreate — window will still display.\n"
                 "Some panels may be missing.",
                 GetExceptionCode());
        OutputDebugStringA(crashMsg);
        MessageBoxA(hwnd, crashMsg, "RawrXD IDE - Startup Warning", MB_OK | MB_ICONWARNING);
    }
#else
    try
    {
        fn(self, hwnd);
    }
    catch (...)
    {
        const char* crashMsg = "[RawrXD] C++ exception caught in onCreate — window will still display.\n"
                               "Some panels may be missing.";
        OutputDebugStringA(crashMsg);
        MessageBoxA(hwnd, crashMsg, "RawrXD IDE - Startup Warning", MB_OK | MB_ICONWARNING);
    }
#endif
}

void onCreateTrampoline(void* self, HWND hwnd)
{
    static_cast<Win32IDE*>(self)->onCreate(hwnd);
}

static void sehCallDeferredInit(DeferredInitFn fn, void* self)
{
#if defined(_MSC_VER)
    __try
    {
        fn(self);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        char crashMsg[256];
        snprintf(crashMsg, sizeof(crashMsg), "[RawrXD] SEH exception 0x%08lX in deferredHeavyInit — non-fatal.\n",
                 GetExceptionCode());
        OutputDebugStringA(crashMsg);
    }
#else
    try
    {
        fn(self);
    }
    catch (...)
    {
        OutputDebugStringA("[RawrXD] C++ exception in deferredHeavyInit — non-fatal.\n");
    }
#endif
}

// SEH wrapper for background thread body — standalone function (no C++ objects
// with destructors allowed inside __try on MSVC, hence the trampoline pattern).
typedef void (*BgThreadBodyFn)(void* self);

static DWORD sehRunBgThread(BgThreadBodyFn fn, void* self)
{
#if defined(_MSC_VER)
    __try
    {
        fn(self);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        DWORD code = GetExceptionCode();
        char crashMsg[512];
        snprintf(crashMsg, sizeof(crashMsg),
                 "[RawrXD] SEH exception 0x%08lX in background init thread — non-fatal.\n"
                 "Some subsystems may be unavailable. The IDE window remains open.\n",
                 code);
        OutputDebugStringA(crashMsg);

        // Write crash log for diagnostics
        FILE* f = fopen("rawrxd_crash.log", "a");
        if (f)
        {
            fprintf(f, "BACKGROUND THREAD CRASH: Exception 0x%08lX\n", code);
            fclose(f);
        }
        return code;
    }
    return 0;
#else
    try
    {
        fn(self);
        return 0;
    }
    catch (...)
    {
        const char* crashMsg = "[RawrXD] C++ exception in background init thread — non-fatal.\n"
                               "Some subsystems may be unavailable. The IDE window remains open.\n";
        OutputDebugStringA(crashMsg);
        FILE* f = fopen("rawrxd_crash.log", "a");
        if (f)
        {
            fprintf(f, "BACKGROUND THREAD CRASH: C++ exception\n");
            fclose(f);
        }
        return 1;
    }
#endif
}

void deferredInitTrampoline(void* self)
{
    static_cast<Win32IDE*>(self)->deferredHeavyInit();
}

// handleMessage - Instance message handler
// ============================================================================
// v280 WndProc Hook — intercepts WM_CREATE/DESTROY/KEYDOWN/TIMER for
// shared-memory inference bridge (ghost text, token polling, etc.)
// Returns 1 (rax) if consumed, 0 if pass-through to normal dispatch.
extern "C" int64_t V280_UI_WndProc_Hook(void* hwnd, uint32_t uMsg, uint64_t wParam, int64_t lParam);
// v280 ghost text query for WM_PAINT overlay
extern "C" int V280_UI_IsGhostActive(void);
extern "C" int V280_UI_GetGhostText(char* buf, int buf_size);

LRESULT Win32IDE::handleMessage(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    // ── v280 SHM Bridge Hook ──
    // Must fire BEFORE normal dispatch so it can:
    //   - Intercept Tab/Esc on ghost text (WM_KEYDOWN)
    //   - Install/kill poll timer (WM_CREATE/WM_DESTROY)
    //   - Drive token polling (WM_TIMER with IDT_V280_POLL)
    //   - Trigger repaint on WM_V280_GHOST_TEXT
    int64_t v280_result = V280_UI_WndProc_Hook((void*)hwnd, (uint32_t)uMsg, (uint64_t)wParam, (int64_t)lParam);
    if (v280_result != 0)
    {
        return 0;  // Message consumed by v280 bridge
    }

    switch (uMsg)
    {
        case WM_CREATE:
            sehCallOnCreate(onCreateTrampoline, this, hwnd);
            return 0;

        case WM_SIZE:
        {
            int width = LOWORD(lParam);
            int height = HIWORD(lParam);
            if (width > 0 && height > 0)
            {
                onSize(width, height);
                // Visible line range changed — trigger recoloring for the new viewport
                onEditorContentChanged();
            }
            return 0;
        }

        case WM_DPICHANGED:
        {
            // Tier 3 (Feature 33): Full High-DPI polish — scale fonts, UI dimensions, relayout
            UINT newDpi = HIWORD(wParam);
            RECT* prc = reinterpret_cast<RECT*>(lParam);
            onDpiChanged(newDpi, prc);
            return 0;
        }

        case WM_GETMINMAXINFO:
        case WM_NCCALCSIZE:
        case WM_WINDOWPOSCHANGING:
        case WM_WINDOWPOSCHANGED:
        case WM_ACTIVATEAPP:
        case WM_ACTIVATE:
        case WM_NCACTIVATE:
        case WM_KILLFOCUS:
        case WM_IME_SETCONTEXT:
        case WM_IME_NOTIFY:
        case WM_SHOWWINDOW:
        case WM_NCPAINT:
            return DefWindowProcA(hwnd, uMsg, wParam, lParam);

        case WM_SETFOCUS:
            // Forward focus to the editor so the caret appears and keyboard input works
            if (m_hwndEditor && IsWindow(m_hwndEditor))
            {
                SetFocus(m_hwndEditor);
            }
            return 0;

        case WM_KEYDOWN:
            // Command Palette from main window (e.g. when frame has focus)
            if ((GetKeyState(VK_CONTROL) & 0x8000) && (GetKeyState(VK_SHIFT) & 0x8000) && (wParam == 'P'))
            {
                if (m_commandPaletteVisible)
                    hideCommandPalette();
                else
                    showCommandPalette();
                return 0;
            }

            // Peek overlay keyboard shortcuts
            if (isPeekOverlayActive())
            {
                bool ctrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
                bool alt = (GetKeyState(VK_MENU) & 0x8000) != 0;
                bool shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
                handlePeekOverlayKey((UINT)wParam, ctrl, alt, shift);
                return 0;
            }

            // Peek definition (Alt+F12)
            if ((GetKeyState(VK_MENU) & 0x8000) && (wParam == VK_F12))
            {
                routeCommand(IDM_LSP_GOTO_DEFINITION);
                return 0;
            }

            // Peek references (Shift+F12)
            if ((GetKeyState(VK_SHIFT) & 0x8000) && (wParam == VK_F12))
            {
                routeCommand(IDM_LSP_FIND_REFERENCES);
                return 0;
            }
            break;

        case WM_ERASEBKGND:
        {
            // Paint the background dark instead of default white
            HDC hdc = (HDC)wParam;
            RECT rc;
            GetClientRect(hwnd, &rc);
            if (!m_backgroundBrush)
            {
                m_backgroundBrush = CreateSolidBrush(RGB(30, 30, 30));
            }
            FillRect(hdc, &rc, m_backgroundBrush);
            return 1;  // We handled it
        }

        case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            if (!m_backgroundBrush)
            {
                m_backgroundBrush = CreateSolidBrush(RGB(30, 30, 30));
            }
            FillRect(hdc, &ps.rcPaint, m_backgroundBrush);

            // ── v280 Ghost Text Overlay ──
            // Render inline completion suggestion (dimmed, italic)
            if (V280_UI_IsGhostActive())
            {
                char ghost_buf[4096];
                int ghost_len = V280_UI_GetGhostText(ghost_buf, sizeof(ghost_buf));
                if (ghost_len > 0)
                {
                    // Create ghost text font (italic, same face as editor)
                    HFONT ghostFont =
                        CreateFontA(-14, 0, 0, 0, FW_NORMAL,
                                    TRUE,  // italic
                                    FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                    CLEARTYPE_QUALITY, FIXED_PITCH | FF_MODERN, "Consolas");
                    HFONT oldFont = (HFONT)SelectObject(hdc, ghostFont);

                    // Ghost text color: dimmed gray (VS Code style)
                    SetTextColor(hdc, RGB(128, 128, 128));
                    SetBkMode(hdc, TRANSPARENT);

                    // Position: after cursor (approximate — real impl uses
                    // editor caret position from Scintilla/TextBuffer)
                    RECT ghostRect = ps.rcPaint;
                    ghostRect.left += 80;  // indent from editor margin
                    ghostRect.top += 40;   // below toolbar area

                    DrawTextA(hdc, ghost_buf, ghost_len, &ghostRect, DT_LEFT | DT_TOP | DT_NOPREFIX | DT_WORDBREAK);

                    SelectObject(hdc, oldFont);
                    DeleteObject(ghostFont);
                }
            }

            drawLayoutDebugOverlay(hwnd, hdc);
            logFirstPaint();

            EndPaint(hwnd, &ps);
            return 0;
        }

        case WM_COMMAND:
            onCommand(hwnd, LOWORD(wParam), (HWND)lParam, HIWORD(wParam));
            return 0;

        case WM_NOTIFY:
        {
            NMHDR* pNMHDR = reinterpret_cast<NMHDR*>(lParam);
            if (pNMHDR)
            {
                const int idFrom = (int)pNMHDR->idFrom;
                // VS Code panel tabs (Terminal / Output / Problems / Debug Console)
                if (pNMHDR->code == TCN_SELCHANGE && (pNMHDR->hwndFrom == m_hwndPanelTabs || idFrom == 1301))
                {
                    int idx = (int)TabCtrl_GetCurSel(m_hwndPanelTabs);
                    if (idx < 0)
                        idx = 0;
                    if (idx > 3)
                        idx = 3;
                    switchPanelTab(static_cast<PanelTab>(idx));
                }
                // Tier 3 (Feature 38/39): Status bar click → language/encoding selector (use part index for reliable
                // dispatch)
                if (pNMHDR->hwndFrom == m_hwndStatusBar && pNMHDR->code == NM_CLICK)
                {
                    NMMOUSE* pNMMouse = reinterpret_cast<NMMOUSE*>(lParam);
                    handleStatusBarClick(static_cast<int>(pNMMouse->dwItemSpec));
                }
                // Output panel tab switch (Output / Errors / Debug / Find Results)
                if (pNMHDR->code == TCN_SELCHANGE && pNMHDR->hwndFrom == m_hwndOutputTabs)
                {
                    int idx = (int)TabCtrl_GetCurSel(m_hwndOutputTabs);
                    static const char* outputTabKeys[] = {"Output", "Errors", "Debug", "Find Results"};
                    if (idx >= 0 && idx < 4)
                    {
                        m_activeOutputTab = outputTabKeys[idx];
                        m_selectedOutputTab = idx;
                        for (auto& kv : m_outputWindows)
                        {
                            ShowWindow(kv.second,
                                       (kv.first == m_activeOutputTab && m_outputPanelVisible) ? SW_SHOW : SW_HIDE);
                        }
                    }
                }
                // Output panel tab switch (Output / Errors / Debug / Find Results / Problems)
                if (pNMHDR->code == TCN_SELCHANGE && pNMHDR->hwndFrom == m_hwndOutputTabs)
                {
                    int idx = (int)TabCtrl_GetCurSel(m_hwndOutputTabs);
                    static const char* outputTabKeys[] = {"Output", "Errors", "Debug", "Find Results", "Problems"};
                    if (idx >= 0 && idx < 5)
                    {
                        m_activeOutputTab = outputTabKeys[idx];
                        m_selectedOutputTab = idx;
                        for (auto& kv : m_outputWindows)
                        {
                            ShowWindow(kv.second,
                                       (kv.first == m_activeOutputTab && m_outputPanelVisible) ? SW_SHOW : SW_HIDE);
                        }
                        if (m_hwndProblemsListView)
                        {
                            ShowWindow(m_hwndProblemsListView, (idx == 4 && m_outputPanelVisible) ? SW_SHOW : SW_HIDE);
                        }
                    }
                }
                // Problems ListView double-click → goToProblem
                if ((pNMHDR->hwndFrom == m_hwndProblemsListView || idFrom == IDC_PROBLEMS_LISTVIEW) &&
                    (pNMHDR->code == NM_DBLCLK || pNMHDR->code == LVN_ITEMACTIVATE))
                {
                    int idx = (int)ListView_GetNextItem(m_hwndProblemsListView, -1, LVNI_SELECTED);
                    if (idx >= 0)
                        goToProblem(idx);
                }
                // Handle tab bar selection change
                if (pNMHDR->code == TCN_SELCHANGE && pNMHDR->hwndFrom == m_hwndTabBar)
                {
                    onTabChanged();
                }
                // Handle RichEdit scroll/change notifications for line number sync
                if (pNMHDR->hwndFrom == m_hwndEditor)
                {
                    if (pNMHDR->code == EN_VSCROLL || pNMHDR->code == EN_SELCHANGE || pNMHDR->code == EN_CHANGE)
                    {
                        updateLineNumbers();
                        // Debounce syntax coloring on content change
                        if (pNMHDR->code == EN_CHANGE && m_syntaxColoringEnabled)
                        {
                            onEditorContentChanged();
                        }
                        // Trigger AI Ghost Text on typing
                        if (pNMHDR->code == EN_CHANGE)
                        {
                            triggerGhostTextCompletion();
                        }
                        // Tier 3 (Feature 36): Mark file dirty on any content change
                        if (pNMHDR->code == EN_CHANGE)
                        {
                            markFileModified();
                        }
                        // Tier 3 (Feature 31): Update smooth caret target on selection change
                        if (pNMHDR->code == EN_SELCHANGE)
                        {
                            updateCaretTarget();
                        }
                        // Dismiss ghost text when caret moves (anchor becomes stale)
                        if (pNMHDR->code == EN_SELCHANGE && m_ghostTextVisible)
                        {
                            dismissGhostText();
                        }
                        // Update status bar cursor position
                        CHARRANGE sel;
                        SendMessage(m_hwndEditor, EM_EXGETSEL, 0, (LPARAM)&sel);
                        int line = (int)SendMessage(m_hwndEditor, EM_LINEFROMCHAR, sel.cpMin, 0);
                        int lineStart = (int)SendMessage(m_hwndEditor, EM_LINEINDEX, line, 0);
                        int col = sel.cpMin - lineStart;
                        wchar_t posBuf[64];
                        swprintf(posBuf, 64, L"Ln %d, Col %d", line + 1, col + 1);
                        if (m_hwndStatusBar)
                        {
                            SendMessage(m_hwndStatusBar, SB_SETTEXT, 1, (LPARAM)posBuf);
                        }
                        // Breadcrumb: update symbol path (File > Class > Method) on cursor move
                        if (pNMHDR->code == EN_SELCHANGE)
                        {
                            updateBreadcrumbsOnCursorMove();
                        }
                    }
                }
            }
            return 0;
        }

        case WM_HSCROLL:
        case WM_VSCROLL:
            // Forward scrollbar messages and update line numbers
            {
                // Phase 44: VoiceAutomation slider routing
                if (uMsg == WM_HSCROLL)
                {
                    extern bool Win32IDE_HandleVoiceAutomationScroll(HWND, LPARAM);
                    if (Win32IDE_HandleVoiceAutomationScroll(hwnd, lParam))
                    {
                        return 0;
                    }
                }
                LRESULT result = DefWindowProcA(hwnd, uMsg, wParam, lParam);
                updateLineNumbers();
                // Recolor visible lines on scroll
                if (m_syntaxColoringEnabled)
                {
                    onEditorContentChanged();
                }
                return result;
            }

        case WM_MOUSEWHEEL:
            // Tier 1: Smooth scroll interpolation
            if (handleTier1MouseWheel(wParam, lParam))
            {
                return 0;
            }
            break;

        case WM_TIMER:
            if (wParam == SYNTAX_COLOR_TIMER_ID)
            {
                // Handled by SyntaxColorTimerProc callback — this is a fallback
                KillTimer(hwnd, SYNTAX_COLOR_TIMER_ID);
                if (m_syntaxColoringEnabled)
                {
                    applySyntaxColoring();
                }
                return 0;
            }
            if (wParam == 8888)
            {  // GHOST_TEXT_TIMER_ID
                onGhostTextTimer();
                return 0;
            }
            if (wParam == MODEL_PROGRESS_TIMER_ID)
            {
                // Poll model progress and update the progress bar UI
                if (m_modelOperationActive.load())
                {
                    float pct = m_modelProgressPercent.load();
                    if (m_hwndModelProgressBar)
                    {
                        SendMessage(m_hwndModelProgressBar, PBM_SETPOS, (WPARAM)(int)(pct * 10.0f), 0);
                    }
                    std::string status;
                    {
                        std::lock_guard<std::mutex> lock(m_modelProgressMutex);
                        status = m_modelProgressStatus;
                    }
                    if (m_hwndModelProgressLabel && !status.empty())
                    {
                        SetWindowTextA(m_hwndModelProgressLabel, status.c_str());
                    }
                }
                else
                {
                    hideModelProgressBar();
                }
                return 0;
            }
            if (wParam == 199)
            {  // IDT_FORCE_VISIBLE — one-shot to force window visible again after init
                KillTimer(hwnd, 199);
                if (m_hwndMain && IsWindow(m_hwndMain) && !IsIconic(m_hwndMain))
                {
                    ShowWindow(m_hwndMain, SW_SHOW);
                    SetWindowPos(m_hwndMain, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
                    forceWindowToForeground(m_hwndMain);
                }
                return 0;
            }
            if (wParam == IDT_VISIBILITY_WATCHDOG)
            {
                runWindowVisibilityWatchdog(m_hwndMain ? m_hwndMain : hwnd);
                return 0;
            }
            if (wParam == 42)
            {  // IDT_STATUS_FLASH
                KillTimer(hwnd, 42);
                // Restore default status bar text
                if (m_hwndStatusBar)
                {
                    SendMessage(m_hwndStatusBar, SB_SETTEXT, 0, (LPARAM)L"Ready");
                }
                return 0;
            }
            if (wParam == 0x7C01)
            {  // VOICE_TIMER_ID (voice chat VU meter)
                onVoiceChatTimer();
                return 0;
            }
            if (wParam == 0x7C10)
            {  // VA_TIMER_ID (Phase 44: VoiceAutomation status)
                extern void Win32IDE_VoiceAutomationTimerTick();
                Win32IDE_VoiceAutomationTimerTick();
                return 0;
            }
            if (wParam == 0xDC01)
            {  // RECOVERY_TIMER_ID (Phase 45: DiskRecovery progress)
                onRecoveryTimer();
                return 0;
            }
            // Tier 3: Polish timers (caret animation, theme transition, format status)
            if (handleTier3Timer(wParam))
            {
                return 0;
            }
            // Tier 1: Critical cosmetic timers (smooth scroll, minimap, auto-update)
            if (handleTier1Timer(wParam))
            {
                return 0;
            }
            break;

        // Phase 33: Voice Chat Global Hotkeys
        case WM_HOTKEY:
            if (wParam == 0xA001)
            {  // VOICE_HOTKEY_TOGGLE_PTT
                cmdVoicePTT();
                return 0;
            }
            if (wParam == 0xA002)
            {  // VOICE_HOTKEY_TOGGLE_PANEL
                cmdVoiceTogglePanel();
                return 0;
            }
            if (wParam == 0xA003)
            {                      // VOICE_HOTKEY_STOP
                cmdVoiceRecord();  // stop recording
                return 0;
            }
            break;

        case WM_CLOSE:
            if (!m_fileModified || promptSaveChanges())
            {
                DestroyWindow(hwnd);
            }
            return 0;

        // Tier 1: Auto-update notification (WM_APP+501)
        case (WM_APP + 501):
            showUpdateNotification();
            return 0;

        // Model progress update from background thread
        case WM_APP + 300:
        {  // WM_MODEL_PROGRESS_UPDATE
            float pct = (float)wParam / 10.0f;
            if (m_hwndModelProgressBar)
            {
                SendMessage(m_hwndModelProgressBar, PBM_SETPOS, wParam, 0);
            }
            return 0;
        }
        case WM_APP + 301:
        {  // WM_MODEL_PROGRESS_DONE
            hideModelProgressBar();
            return 0;
        }

        // Visibility watchdog request (posted from watchdog worker thread)
        case WM_APP + 1:
        {
            if (m_hwndMain && IsWindow(m_hwndMain))
            {
                ShowWindow(m_hwndMain, (int)wParam);
                SetForegroundWindow(m_hwndMain);
            }
            return 0;
        }

        // Ghost Text delivery from background completion thread
        case WM_GHOST_TEXT_READY:
        {
            int cursorPos = (int)wParam;
            const char* text = reinterpret_cast<const char*>(lParam);
            onGhostTextReady(cursorPos, text);
            if (text)
                free(const_cast<char*>(text));  // Allocated with _strdup
            return 0;
        }

        case WM_DESTROY:
            KillTimer(hwnd, IDT_VISIBILITY_WATCHDOG);
            onDestroy();
            PostQuitMessage(0);
            return 0;

        case WM_CTLCOLORSTATIC:
        case WM_CTLCOLOREDIT:
        case WM_CTLCOLORLISTBOX:
        {
            // Dark mode colors for controls
            HDC hdcCtrl = (HDC)wParam;
            SetTextColor(hdcCtrl, RGB(220, 220, 220));
            SetBkColor(hdcCtrl, RGB(30, 30, 30));
            if (!m_backgroundBrush)
            {
                m_backgroundBrush = CreateSolidBrush(RGB(30, 30, 30));
            }
            return (LRESULT)m_backgroundBrush;
        }

        default:
            // Force main window visible once after startup (posted before message loop).
            // Also set a one-shot timer to force visible again ~400ms later (catches init that hides window).
            if (uMsg == WM_APP + 199)
            {
                if (m_hwndMain && IsWindow(m_hwndMain))
                {
                    if (IsIconic(m_hwndMain))
                        ShowWindow(m_hwndMain, SW_RESTORE);
                    ShowWindow(m_hwndMain, SW_SHOW);
                    SetWindowPos(m_hwndMain, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
                    forceWindowToForeground(m_hwndMain);
                    SetTimer(hwnd, 199, 400, nullptr);  // One-shot: force visible again in 400ms
                }
                return 0;
            }
            // Handle deferred heavy initialization (posted from onCreate)
            if (uMsg == WM_APP + 100)
            {
                sehCallDeferredInit(deferredInitTrampoline, this);
                return 0;
            }
            // Handle background init completion — refresh UI (Tier 5 menus enabled here after initTier5Cosmetics)
            if (uMsg == WM_APP + 101)
            {
                applyTheme();
                updateMenuEnableStates();
                InvalidateRect(hwnd, nullptr, TRUE);
                UpdateWindow(hwnd);
                return 0;
            }
            // Tier 3 (Feature 35): File changed externally → show reload toast
            if (uMsg == WM_FILE_CHANGED_EXTERNAL)
            {
                showFileChangedToast();
                return 0;
            }
            // Handle "load downloaded model" signal from background download threads
            // (HuggingFace / URL downloads complete, m_loadedModelPath already set)
            if (uMsg == WM_APP + 201)
            {
                if (!m_loadedModelPath.empty())
                {
                    std::string pathToLoad = m_loadedModelPath;
                    appendToOutput("Loading downloaded model: " + pathToLoad + "\n", "Output", OutputSeverity::Info);
                    if (loadGGUFModel(pathToLoad))
                    {
                        loadModelForInference(pathToLoad);
                        appendToOutput("Downloaded model loaded successfully!\n", "Output", OutputSeverity::Info);
                    }
                    else
                    {
                        appendToOutput("Failed to load downloaded model: " + pathToLoad + "\n", "Errors",
                                       OutputSeverity::Error);
                    }
                }
                return 0;
            }
            // RE: set binary from build output (Game Engine or other build that posted path)
            if (uMsg == WM_APP + 202)
            {
                std::string* path = reinterpret_cast<std::string*>(lParam);
                if (path && !path->empty())
                {
                    setCurrentBinaryForReverseEngineering(*path);
                    appendToOutput("[RE] Binary set from build output: " + *path + "\n", "Output",
                                   OutputSeverity::Info);
                    delete path;
                }
                return 0;
            }
            // Handle custom agent output message
            if (uMsg == WM_AGENT_OUTPUT_SAFE)
            {
                const char* text = reinterpret_cast<const char*>(lParam);
                if (text)
                {
                    onAgentOutput(text);
                    free(const_cast<char*>(text));
                }
                return 0;
            }
            // Handle Ghost Text completion delivery from background thread
            if (uMsg == WM_GHOST_TEXT_READY)
            {
                const char* completionText = reinterpret_cast<const char*>(lParam);
                onGhostTextReady((int)wParam, completionText);
                if (completionText)
                    free(const_cast<char*>(completionText));
                return 0;
            }
            // Handle Plan Executor messages
            if (uMsg == WM_PLAN_READY)
            {
                onPlanReady((int)wParam, reinterpret_cast<PlanStep*>(lParam));
                return 0;
            }
            if (uMsg == WM_PLAN_STEP_DONE)
            {
                onPlanStepDone((int)wParam, (int)lParam);
                return 0;
            }
            if (uMsg == WM_PLAN_COMPLETE)
            {
                onPlanComplete(wParam != 0);
                return 0;
            }
            // Plan step status update from background thread (live dialog update)
            if (uMsg == WM_APP + 503)
            {
                updatePlanStepInDialog((int)wParam, static_cast<PlanStepStatus>((int)lParam));
                return 0;
            }
            // Agent History replay step completion
            if (uMsg == WM_AGENT_HISTORY_REPLAY_DONE)
            {
                onReplayStepDone((int)wParam, (int)lParam);
                return 0;
            }
            // ── Native Inference Pipeline streaming messages ──
            if (uMsg == WM_NATIVE_AI_TOKEN)
            {
                onNativeAIToken(wParam, lParam);
                return 0;
            }
            if (uMsg == WM_NATIVE_AI_COMPLETE)
            {
                onNativeAIComplete(wParam, lParam);
                return 0;
            }
            if (uMsg == WM_NATIVE_AI_ERROR)
            {
                onNativeAIError();
                return 0;
            }
            if (uMsg == WM_NATIVE_AI_PROGRESS)
            {
                onNativeAIProgress();
                return 0;
            }
            break;
    }

    return DefWindowProcA(hwnd, uMsg, wParam, lParam);
}

// ============================================================================
// createWindow - Register class and create main window
// ============================================================================
bool Win32IDE::createWindow()
{
    // ====================================================================
    // Enterprise: Load external configuration before window creation
    // ====================================================================
    {
        auto& config = IDEConfig::getInstance();
        // Try workspace config, then user config, then defaults
        std::string configPath = "rawrxd.config.json";
        if (!config.loadFromFile(configPath))
        {
            // Try in exe directory
            char exePath[MAX_PATH];
            GetModuleFileNameA(nullptr, exePath, MAX_PATH);
            std::string exeDir(exePath);
            size_t lastSlash = exeDir.find_last_of("\\/");
            if (lastSlash != std::string::npos)
            {
                exeDir = exeDir.substr(0, lastSlash);
                config.loadFromFile(exeDir + "\\rawrxd.config.json");
            }
        }
        // Apply environment variable overrides (RAWRXD_* prefix)
        config.applyEnvironmentOverrides();
        // Initialize feature toggles from config
        config.applyFeatureToggles();

        // Apply config to IDE state
        m_ollamaBaseUrl = config.getString("ollama.baseUrl", "http://localhost:11434");
        m_ollamaModelOverride = config.getString("ollama.modelOverride", "");
        m_autoSaveEnabled = config.getBool("editor.autoSave", false);
        m_gpuTextEnabled = config.getBool("performance.gpuTextRendering", true);
        m_useStreamingLoader = config.getBool("performance.streamingGGUFLoad", true);
        m_useVulkanRenderer = config.getBool("performance.vulkanRenderer", false);

        // Sync agentic autonomous config (1–99x limits, QualitySpeedBalance, operation/model mode)
        {
            auto& aac = RawrXD::AgenticAutonomousConfig::instance();
            aac.setPerModelInstanceCount(config.getInt("agent.perModelInstances", 1));
            aac.setCycleAgentCounter(config.getInt("agent.cycleAgentCounter", 1));
            aac.setQualitySpeedBalanceFromString(config.getString("agent.qualitySpeedBalance", "Auto"));
            aac.setOperationModeFromString(config.getString("agent.operationMode", "Agent"));
            aac.setModelSelectionModeFromString(config.getString("agent.modelSelectionMode", "Auto"));
            int maxParallel = config.getInt("agent.maxModelsInParallel", 99);
            aac.setMaxModelsInParallel(maxParallel > 0 ? maxParallel : 99);
            std::string agenticJson = config.getString("agentic.configJson", "");
            if (!agenticJson.empty())
                aac.fromJson(agenticJson);
        }

        LOG_INFO("Configuration loaded — " + std::to_string(config.getAllKeys().size()) + " keys");
        METRICS.increment("config.loads_total");
    }

    // Load RichEdit libraries — need both for RICHEDIT_CLASSA and MSFTEDIT_CLASS
    LoadLibraryA("riched20.dll");
    LoadLibraryA("msftedit.dll");

    WNDCLASSEXA wc = {};
    wc.cbSize = sizeof(WNDCLASSEXA);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = Win32IDE::WindowProc;
    wc.hInstance = m_hInstance;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = CreateSolidBrush(RGB(30, 30, 30));
    wc.lpszClassName = kWindowClassName;
    wc.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
    wc.hIconSm = LoadIcon(nullptr, IDI_APPLICATION);

    if (!RegisterClassExA(&wc))
    {
        // Class might already be registered
        DWORD err = GetLastError();
        if (err != ERROR_CLASS_ALREADY_EXISTS)
        {
            LOG_ERROR("Failed to register window class");
            return false;
        }
    }

    // Create the main window on the primary monitor's work area so it is always visible
    int winW = 1600, winH = 1000;
    int winX = 50, winY = 50;
    HMONITOR hMon = MonitorFromPoint(POINT{0, 0}, MONITOR_DEFAULTTOPRIMARY);
    MONITORINFO mi = {sizeof(mi)};
    if (hMon && GetMonitorInfoA(hMon, &mi))
    {
        const RECT& r = mi.rcWork;
        winX = r.left + 50;
        winY = r.top + 50;
        winW = (std::min)((int)(r.right - r.left) - 100, 1600);
        winH = (std::min)((int)(r.bottom - r.top) - 100, 1000);
    }
    m_hwndMain =
        CreateWindowExA(WS_EX_APPWINDOW, kWindowClassName, "RawrXD IDE - Native Win32 AI Development Environment",
                        WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN | WS_VISIBLE, winX, winY, winW, winH, nullptr, nullptr,
                        m_hInstance, this);

    if (!m_hwndMain)
    {
        LOG_ERROR("Failed to create main window");
        return false;
    }

    ShowWindow(m_hwndMain, SW_SHOW);
    ShowWindow(m_hwndMain, SW_SHOWNORMAL);
    UpdateWindow(m_hwndMain);
    SetWindowPos(m_hwndMain, HWND_TOPMOST, winX, winY, winW, winH, SWP_SHOWWINDOW | SWP_NOACTIVATE);
    SetWindowPos(m_hwndMain, HWND_NOTOPMOST, winX, winY, winW, winH, SWP_SHOWWINDOW | SWP_NOMOVE | SWP_NOSIZE);
    BringWindowToTop(m_hwndMain);
    SetForegroundWindow(m_hwndMain);
    SetActiveWindow(m_hwndMain);
    RedrawWindow(m_hwndMain, nullptr, nullptr, RDW_ERASE | RDW_INVALIDATE | RDW_UPDATENOW);

    // Register GUI output callback so unified-command handler output goes to IDE output panel
    setIdeAppendOutput(
        [](void* ide, const char* text)
        {
            if (ide && text)
                static_cast<Win32IDE*>(ide)->appendToOutput(std::string(text), "Output", OutputSeverity::Info);
        });

    LOG_INFO("Main window created successfully");
    return true;
}

// ============================================================================
// Force window to foreground (SetForegroundWindow often fails when launched by
// another process; AttachThreadInput + BringWindowToTop works around it).
// ============================================================================
static void forceWindowToForeground(HWND hwnd)
{
    if (!hwnd || !IsWindow(hwnd))
        return;
    HWND fg = GetForegroundWindow();
    if (fg == hwnd)
        return;
    DWORD fgTid = GetWindowThreadProcessId(fg, nullptr);
    DWORD myTid = GetCurrentThreadId();
    if (fgTid != myTid && fgTid != 0)
    {
        AttachThreadInput(myTid, fgTid, TRUE);
    }
    BringWindowToTop(hwnd);
    ShowWindow(hwnd, SW_RESTORE);
    ShowWindow(hwnd, SW_SHOWNORMAL);
    SetForegroundWindow(hwnd);
    SetActiveWindow(hwnd);
    if (fgTid != myTid && fgTid != 0)
    {
        AttachThreadInput(myTid, fgTid, FALSE);
    }
}

// ============================================================================
// showWindow - Show and update the main window (always normal, never minimized)
// Ignores launcher nCmdShow so the IDE never opens minimized or disappears.
// ============================================================================
void Win32IDE::showWindow()
{
    if (!m_hwndMain)
        return;
    if (IsIconic(m_hwndMain))
        ShowWindow(m_hwndMain, SW_RESTORE);
    ShowWindow(m_hwndMain, SW_SHOWNORMAL);
    WINDOWPLACEMENT wp = {sizeof(WINDOWPLACEMENT)};
    if (GetWindowPlacement(m_hwndMain, &wp))
    {
        if (wp.showCmd != SW_SHOWNORMAL && wp.showCmd != SW_SHOW)
        {
            wp.showCmd = SW_SHOWNORMAL;
            SetWindowPlacement(m_hwndMain, &wp);
        }
    }
    UpdateWindow(m_hwndMain);
    SetWindowPos(m_hwndMain, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
    BringWindowToTop(m_hwndMain);
    SetForegroundWindow(m_hwndMain);
    SetActiveWindow(m_hwndMain);
    forceWindowToForeground(m_hwndMain);
    SetTimer(m_hwndMain, IDT_VISIBILITY_WATCHDOG, 1000, nullptr);
    FLASHWINFO fwi = {sizeof(FLASHWINFO), m_hwndMain, FLASHW_ALL | FLASHW_TIMERNOFG, 3, 0};
    FlashWindowEx(&fwi);
}

// ============================================================================
// runMessageLoop - Standard Win32 message loop with accelerators
// ============================================================================
int Win32IDE::runMessageLoop()
{
    LOG_INFO("Message loop starting");
    METRICS.increment("app.message_loop_starts");
    auto loopStart = std::chrono::high_resolution_clock::now();

    MSG msg = {};
    try
    {
        while (GetMessage(&msg, nullptr, 0, 0))
        {
            METRICS.increment("app.messages_processed");

            // Handle accelerator keys
            if (msg.message == WM_KEYDOWN)
            {
                bool ctrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
                bool shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
                bool alt = (GetKeyState(VK_MENU) & 0x8000) != 0;

                if (ctrl && shift && msg.wParam == 'P')
                {
                    if (m_commandPaletteVisible)
                    {
                        hideCommandPalette();
                    }
                    else
                    {
                        showCommandPalette();
                    }
                    continue;
                }
                if (ctrl && msg.wParam == 'N')
                {
                    routeCommandUnified(IDM_FILE_NEW, this, m_hwndMain);
                    continue;
                }
                if (ctrl && msg.wParam == 'O')
                {
                    routeCommandUnified(IDM_FILE_OPEN, this, m_hwndMain);
                    continue;
                }
                if (ctrl && msg.wParam == 'S')
                {
                    if (shift)
                        routeCommandUnified(IDM_FILE_SAVEAS, this, m_hwndMain);
                    else
                        routeCommandUnified(IDM_FILE_SAVE, this, m_hwndMain);
                    continue;
                }
                // Ctrl+Shift+L → License Creator, Ctrl+Shift+F → Feature Registry (before plain Ctrl+F)
                if (ctrl && shift && msg.wParam == 'L')
                {
                    routeCommand(3015);
                    continue;
                }
                if (ctrl && shift && msg.wParam == 'F')
                {
                    routeCommand(3016);
                    continue;
                }
                if (ctrl && msg.wParam == 'F')
                {
                    routeCommandUnified(IDM_EDIT_FIND, this, m_hwndMain);
                    continue;
                }
                if (ctrl && msg.wParam == 'H')
                {
                    routeCommandUnified(IDM_EDIT_REPLACE, this, m_hwndMain);
                    continue;
                }
                if (ctrl && msg.wParam == 'B')
                {
                    toggleSidebar();
                    continue;
                }
                if (ctrl && alt && msg.wParam == 'B')
                {
                    toggleSecondarySidebar();
                    continue;
                }
                // Ctrl+Shift+E → File Explorer (show sidebar with Explorer view)
                if (ctrl && shift && msg.wParam == 'E')
                {
                    routeCommand(IDM_VIEW_FILE_EXPLORER);
                    continue;
                }
                // Ctrl+Shift+X → Extensions view
                if (ctrl && shift && msg.wParam == 'X')
                {
                    routeCommand(2031);
                    continue;
                }
                // Ctrl+Shift+C → AI Chat panel toggle
                if (ctrl && shift && msg.wParam == 'C')
                {
                    toggleSecondarySidebar();
                    continue;
                }
                // Ctrl+Shift+A → Audit Dashboard
                if (ctrl && shift && msg.wParam == 'A')
                {
                    routeCommandUnified(IDM_AUDIT_SHOW_DASHBOARD, this, m_hwndMain);
                    continue;
                }
                // Ctrl+Shift+I → Bounded Agent Loop (tool-calling autonomous agent)
                if (ctrl && shift && msg.wParam == 'I')
                {
                    routeCommandUnified(IDM_AGENT_BOUNDED_LOOP, this, m_hwndMain);
                    continue;
                }
                // Ctrl+, → Settings (full GUI)
                if (ctrl && msg.wParam == VK_OEM_COMMA)
                {
                    this->showSettingsGUIDialog();
                    continue;
                }
                // Ctrl+= / Ctrl+- → UI Zoom In/Out, Ctrl+0 → Reset zoom
                if (ctrl && (msg.wParam == VK_OEM_PLUS || msg.wParam == 0xBB))
                {
                    // Zoom in: increase scale by 10%
                    if (m_settings.uiScalePercent == 0)
                    {
                        m_settings.uiScalePercent = MulDiv(100, m_currentDpi, 96) + 10;
                    }
                    else
                    {
                        m_settings.uiScalePercent = (std::min)(m_settings.uiScalePercent + 10, 300);
                    }
                    recreateFonts();
                    RECT rc;
                    GetClientRect(m_hwndMain, &rc);
                    onSize(rc.right, rc.bottom);
                    InvalidateRect(m_hwndMain, nullptr, TRUE);
                    saveSettings();
                    continue;
                }
                if (ctrl && (msg.wParam == VK_OEM_MINUS || msg.wParam == 0xBD))
                {
                    // Zoom out: decrease scale by 10%
                    if (m_settings.uiScalePercent == 0)
                    {
                        m_settings.uiScalePercent = (std::max)(MulDiv(100, m_currentDpi, 96) - 10, 75);
                    }
                    else
                    {
                        m_settings.uiScalePercent = (std::max)(m_settings.uiScalePercent - 10, 75);
                    }
                    recreateFonts();
                    RECT rc;
                    GetClientRect(m_hwndMain, &rc);
                    onSize(rc.right, rc.bottom);
                    InvalidateRect(m_hwndMain, nullptr, TRUE);
                    saveSettings();
                    continue;
                }
                if (ctrl && msg.wParam == '0')
                {
                    // Reset zoom to system DPI
                    m_settings.uiScalePercent = 0;
                    recreateFonts();
                    RECT rc;
                    GetClientRect(m_hwndMain, &rc);
                    onSize(rc.right, rc.bottom);
                    InvalidateRect(m_hwndMain, nullptr, TRUE);
                    saveSettings();
                    continue;
                }
            }

            TranslateMessage(&msg);
            AIWorkersProcessInvokeQueue();
            DispatchMessage(&msg);
        }
    }
    catch (const std::exception& e)
    {
        // Centralized exception handler — prevents unhandled crash
        LOG_CRITICAL(std::string("Unhandled exception in message loop: ") + e.what());
        METRICS.increment("app.unhandled_exceptions");
        MessageBoxA(nullptr, (std::string("RawrXD IDE encountered an error:\n") + e.what()).c_str(),
                    "RawrXD IDE - Critical Error", MB_ICONERROR | MB_OK);
    }
    catch (...)
    {
        LOG_CRITICAL("Unknown unhandled exception in message loop");
        METRICS.increment("app.unhandled_exceptions");
        MessageBoxA(nullptr, "RawrXD IDE encountered an unknown error.", "RawrXD IDE - Critical Error",
                    MB_ICONERROR | MB_OK);
    }

    // Log session metrics on exit
    auto loopEnd = std::chrono::high_resolution_clock::now();
    double sessionMs = std::chrono::duration<double, std::milli>(loopEnd - loopStart).count();
    METRICS.recordDuration("app.session_duration_ms", sessionMs);
    LOG_INFO("Message loop ended — session duration: " + std::to_string(sessionMs / 1000.0) + "s");
    LOG_INFO("Messages processed: " + std::to_string(METRICS.getCounter("app.messages_processed")));

    // Save configuration on exit
    IDEConfig::getInstance().saveToFile("rawrxd.config.json");

    return static_cast<int>(msg.wParam);
}

// ============================================================================
// onSize - Layout all child windows when the main window is resized
// ============================================================================
void Win32IDE::onSize(int width, int height)
{
    // LOGGING AS REQUESTED
    char logBuf[256];
    sprintf_s(logBuf, "onSize: %dx%d (Explorer: %p, Terminal: %p)", width, height, m_hwndFileExplorer,
              m_hwndPowerShellPanel);
    LOG_INFO(std::string(logBuf));

    if (width <= 0 || height <= 0)
        return;

    const int TOOLBAR_HEIGHT = dpiScale(32);
    const int STATUSBAR_HEIGHT = dpiScale(24);
    const int ACTIVITY_BAR_WIDTH = dpiScale(48);
    const int TAB_BAR_HEIGHT = dpiScale(28);

    int sidebarWidth = m_sidebarVisible ? m_sidebarWidth : 0;
    int secondarySidebarWidth = m_secondarySidebarVisible ? m_secondarySidebarWidth : 0;
    // Only reserve space for panels that actually have HWNDs created
    int panelHeight = (m_outputPanelVisible && m_hwndOutputTabs) ? m_outputTabHeight : 0;
    int powerShellHeight = (m_powerShellPanelVisible && m_hwndPowerShellPanel) ? m_powerShellPanelHeight : 0;

    int contentTop = TOOLBAR_HEIGHT;
    int contentBottom = height - STATUSBAR_HEIGHT;
    int contentHeight = contentBottom - contentTop;

    // Status bar
    if (m_hwndStatusBar)
    {
        SendMessage(m_hwndStatusBar, WM_SIZE, 0, 0);
    }

    // Toolbar
    if (m_hwndToolbar)
    {
        MoveWindow(m_hwndToolbar, 0, 0, width, TOOLBAR_HEIGHT, TRUE);
    }

    // Activity bar (far left)
    if (m_hwndActivityBar)
    {
        MoveWindow(m_hwndActivityBar, 0, contentTop, ACTIVITY_BAR_WIDTH, contentHeight, TRUE);
    }

    // Primary sidebar
    if (m_hwndSidebar && m_sidebarVisible)
    {
        MoveWindow(m_hwndSidebar, ACTIVITY_BAR_WIDTH, contentTop, sidebarWidth, contentHeight, TRUE);
    }

    // Calculate editor area
    int editorLeft = ACTIVITY_BAR_WIDTH + sidebarWidth;
    int editorRight = width - secondarySidebarWidth;
    int editorWidth = editorRight - editorLeft;
    int editorAreaHeight = contentHeight - panelHeight - powerShellHeight;

    // Tab bar (above editor)
    int tabBarBottom = contentTop;
    if (m_hwndTabBar)
    {
        MoveWindow(m_hwndTabBar, editorLeft, contentTop, editorWidth, TAB_BAR_HEIGHT, TRUE);
        tabBarBottom = contentTop + TAB_BAR_HEIGHT;
    }

    // Breadcrumb bar (below tab bar, above editor) — ESP IE labeled
    int breadcrumbBottom = tabBarBottom;
    if (m_hwndBreadcrumbs && m_settings.breadcrumbsEnabled)
    {
        MoveWindow(m_hwndBreadcrumbs, editorLeft, tabBarBottom, editorWidth, m_breadcrumbHeight, TRUE);
        breadcrumbBottom = tabBarBottom + m_breadcrumbHeight;
    }

    int editorContentHeight = editorAreaHeight - (breadcrumbBottom - contentTop);

    // Line number gutter (left of editor)
    int gutterWidth = m_hwndLineNumbers ? m_lineNumberWidth : 0;
    if (m_hwndLineNumbers)
    {
        MoveWindow(m_hwndLineNumbers, editorLeft, breadcrumbBottom, gutterWidth, editorContentHeight, TRUE);
    }

    // Editor (right of gutter)
    if (m_hwndEditor)
    {
        int minimapW = (m_minimapVisible && m_hwndMinimap) ? m_minimapWidth : 0;
        int editorX = editorLeft + gutterWidth;
        int editorW = editorWidth - gutterWidth - minimapW;
        MoveWindow(m_hwndEditor, editorX, breadcrumbBottom, editorW, editorContentHeight, TRUE);

        // Annotation overlay (same rect as editor, draws inline annotations on top)
        if (m_hwndAnnotationOverlay && IsWindow(m_hwndAnnotationOverlay))
        {
            SetWindowPos(m_hwndAnnotationOverlay, HWND_TOP, editorX, breadcrumbBottom, editorW, editorContentHeight,
                         SWP_NOACTIVATE);
        }

        // Minimap
        if (m_hwndMinimap && m_minimapVisible)
        {
            MoveWindow(m_hwndMinimap, editorRight - minimapW, breadcrumbBottom, minimapW, editorContentHeight, TRUE);
        }
    }

    // Output / Terminal panel area
    int panelTop = contentTop + editorAreaHeight;
    if (panelHeight > 0)
    {
        // Output tabs
        if (m_hwndOutputTabs)
        {
            MoveWindow(m_hwndOutputTabs, editorLeft, panelTop, editorWidth, panelHeight, TRUE);
        }
        int termTop = contentTop + editorAreaHeight;
        int tabBarH = 24;
        int termHeight = panelHeight;
        // Output tab windows (Output, Errors, Debug, Find Results)
        for (auto& kv : m_outputWindows)
        {
            if (kv.second)
            {
                MoveWindow(kv.second, editorLeft, termTop + tabBarH, editorWidth, termHeight - tabBarH, TRUE);
            }
        }
        // Problems ListView (5th tab) — same region
        if (m_hwndProblemsListView)
        {
            MoveWindow(m_hwndProblemsListView, editorLeft, termTop + tabBarH, editorWidth, termHeight - tabBarH, TRUE);
        }
        panelTop += panelHeight;
    }

    // Terminal panes — layout within the output panel area so they're visible
    if (!m_terminalPanes.empty() && panelHeight > 0)
    {
        int termTop = contentTop + editorAreaHeight;
        int termHeight = panelHeight;
        int tabBarH = 24;
        if (m_hwndOutputTabs)
        {
            // Windows already positioned above; no-op for positioning
        }
    }

    // PowerShell panel
    if (m_hwndPowerShellPanel && m_powerShellPanelVisible)
    {
        MoveWindow(m_hwndPowerShellPanel, editorLeft, panelTop, editorWidth, powerShellHeight, TRUE);
        // Also layout internal PowerShell controls
        updatePowerShellPanelLayout(editorWidth, powerShellHeight);
    }

    // Secondary sidebar (Copilot Chat / AI Panel)
    if (m_hwndSecondarySidebar && m_secondarySidebarVisible)
    {
        MoveWindow(m_hwndSecondarySidebar, editorRight, contentTop, secondarySidebarWidth, contentHeight, TRUE);
    }

    // Update line numbers after layout
    updateLineNumbers();

    // Store editor rect for GPU surface sync
    m_editorRect = {editorLeft, contentTop, editorLeft + editorWidth, contentTop + editorAreaHeight};

    Win32IDE_AgenticBrowser_Relayout();
}

// ============================================================================
// syncEditorToGpuSurface - Sync RichEdit content to GPU-accelerated overlay
// ============================================================================
void Win32IDE::syncEditorToGpuSurface()
{
    if (!m_renderer || !m_rendererReady || !m_hwndEditor)
        return;

    // The transparent renderer overlays the editor for GPU-accelerated effects.
    // If the renderer isn't initialized yet, this is a safe no-op.
    try
    {
        RECT editorRect;
        GetClientRect(m_hwndEditor, &editorRect);
        // Renderer will pick up content on next paint cycle
        InvalidateRect(m_hwndEditor, nullptr, FALSE);
    }
    catch (...)
    {
        // Swallow errors — GPU sync is optional enhancement
    }
}

// ============================================================================
// initializeEditorSurface - Set up the GPU rendering surface for the editor
// ============================================================================
void Win32IDE::initializeEditorSurface()
{
    if (!m_renderer || !m_hwndEditor)
        return;

    try
    {
        m_rendererReady = m_renderer->Initialize(m_hwndEditor);
        if (m_rendererReady)
        {
            LOG_INFO("Editor GPU surface initialized");
        }
    }
    catch (const std::exception& e)
    {
        LOG_ERROR(std::string("Editor surface init failed: ") + e.what());
        m_rendererReady = false;
    }
    catch (...)
    {
        LOG_ERROR("Editor surface init failed with unknown error");
        m_rendererReady = false;
    }
}

// ============================================================================
// getResolvedOllamaModel - Returns Ollama model tag (override, loaded path, or default)
// ============================================================================
std::string Win32IDE::getResolvedOllamaModel() const
{
    if (!m_ollamaModelOverride.empty())
        return m_ollamaModelOverride;
    if (!m_loadedModelPath.empty())
    {
        std::string filename = m_loadedModelPath;
        size_t pos = filename.find_last_of("/\\");
        if (pos != std::string::npos)
            filename = filename.substr(pos + 1);
        size_t extPos = filename.rfind(".gguf");
        if (extPos != std::string::npos)
            filename = filename.substr(0, extPos);
        return filename;
    }
    return "llama3.2:latest";
}

// ============================================================================
// trySendToOllama - Attempt to send a prompt to Ollama and get a response
// ============================================================================
bool Win32IDE::trySendToOllama(const std::string& prompt, std::string& outResponse)
{
    try
    {
        ModelConnection conn(m_ollamaBaseUrl.empty() ? "http://localhost:11434" : m_ollamaBaseUrl);

        if (!conn.checkConnection())
        {
            return false;
        }

        std::string modelTag = getResolvedOllamaModel();

        // Synchronous send for simplicity — uses sendPrompt internally
        bool gotResponse = false;
        std::string responseText;
        std::mutex mtx;
        std::condition_variable cv;

        conn.sendPrompt(
            modelTag, prompt, {},
            [&](const std::string& token)
            {
                std::lock_guard<std::mutex> lock(mtx);
                responseText += token;
            },
            [&](const std::string& error)
            {
                std::lock_guard<std::mutex> lock(mtx);
                responseText = "[Error] " + error;
                gotResponse = true;
                cv.notify_one();
            },
            [&]()
            {
                std::lock_guard<std::mutex> lock(mtx);
                gotResponse = true;
                cv.notify_one();
            });

        // Wait up to 60 seconds
        std::unique_lock<std::mutex> lock(mtx);
        cv.wait_for(lock, std::chrono::seconds(60), [&]() { return gotResponse; });

        if (!responseText.empty())
        {
            outResponse = responseText;
            return true;
        }

        return false;
    }
    catch (const std::exception& e)
    {
        outResponse = std::string("[Error] ") + e.what();
        return false;
    }
    catch (...)
    {
        outResponse = "[Error] Unknown exception in Ollama communication";
        return false;
    }
}

// ============================================================================
// Reverse Engineering methods are implemented in Win32IDE_ReverseEngineering.cpp
// ============================================================================

// ============================================================================
// onCreate - Called when WM_CREATE is received
// ============================================================================
void Win32IDE::onCreate(HWND hwnd)
{
    m_hwndMain = hwnd;

    // Initialize Common Controls
    INITCOMMONCONTROLSEX icex = {};
    icex.dwSize = sizeof(icex);
    icex.dwICC = ICC_BAR_CLASSES | ICC_TAB_CLASSES | ICC_LISTVIEW_CLASSES | ICC_TREEVIEW_CLASSES | ICC_PROGRESS_CLASS |
                 ICC_STANDARD_CLASSES;
    InitCommonControlsEx(&icex);

    // ================================================================
    // Create UI components — SEH-safe breadcrumb trail for diagnosis
    // Each step is wrapped so a crash pinpoints the exact function.
    // ================================================================

    // Optional panels (keep them alive for the lifetime of the IDE; no extra stub layers).
    // These are lightweight wrappers; heavy init is still deferred.
    if (!m_modelRegistry)
        m_modelRegistry = new ModelRegistry(hwnd);
    if (!m_interpretabilityPanel)
    {
        m_interpretabilityPanel = new InterpretabilityPanel();
        m_interpretabilityPanel->setParent(hwnd);
    }
    if (!m_checkpointManager)
        m_checkpointManager = new CheckpointManager(hwnd);
    if (!m_ciCdSettings)
        m_ciCdSettings = new CICDSettings();
    if (!m_multiFileSearch)
    {
        m_multiFileSearch = new MultiFileSearchWidget();
        // Default root: project root if set; else current working directory.
        const std::string root = m_projectRoot.empty() ? std::filesystem::current_path().string() : m_projectRoot;
        m_multiFileSearch->setProjectRoot(root);
        m_multiFileSearch->setShowCallback(&MultiFileSearchWidget_ShowDialog, m_multiFileSearch);
    }
    if (!m_benchmarkMenu)
        m_benchmarkMenu = new BenchmarkMenu(hwnd);

    if (m_modelRegistry)
    {
        m_modelRegistry->setShowCallback(
            [](void* ctx)
            {
                auto* ide = static_cast<Win32IDE*>(ctx);
                if (ide)
                    ide->showModelRegistryDialog();
            },
            this);
    }

    if (m_ciCdSettings)
    {
        m_ciCdSettings->setShowCallback(
            [](void* ctx)
            {
                auto* ide = static_cast<Win32IDE*>(ctx);
                if (ide)
                    ide->showCICDSettingsDialog();
            },
            this);
    }

    if (m_checkpointManager)
    {
        m_checkpointManager->setShowCallback(
            [](void* ctx, const std::vector<CheckpointManager::CheckpointIndex>& checkpoints)
            {
                auto* ide = static_cast<Win32IDE*>(ctx);
                if (!ide)
                    return;
                std::string msg = "[CheckpointManager] Checkpoints: " + std::to_string(checkpoints.size()) + "\n";
                for (const auto& cp : checkpoints)
                {
                    msg += "  - " + cp.checkpointId + "\n";
                }
                ide->appendToOutput(msg, "Output", Win32IDE::OutputSeverity::Info);
            },
            this);
    }

    OutputDebugStringA("[onCreate] createMenuBar...\n");
    createMenuBar(hwnd);  // ESP:m_hMenu — menus/submenus wired end-to-end
    OutputDebugStringA("[onCreate] createToolbar...\n");
    createToolbar(hwnd);

    OutputDebugStringA("[onCreate] createActivityBar...\n");
    createActivityBar(hwnd);
    OutputDebugStringA("[onCreate] createPrimarySidebar...\n");
    createPrimarySidebar(hwnd);

    OutputDebugStringA("[onCreate] createTabBar...\n");
    createTabBar(hwnd);
    OutputDebugStringA("[onCreate] createBreadcrumbBar...\n");
    createBreadcrumbBar(hwnd);  // ESP:IDC_BREADCRUMB_BAR — symbol path bar
    OutputDebugStringA("[onCreate] createLineNumberGutter...\n");
    createLineNumberGutter(hwnd);
    OutputDebugStringA("[onCreate] createEditor...\n");
    createEditor(hwnd);
    createAnnotationOverlay(hwnd);
    OutputDebugStringA("[onCreate] createTerminal...\n");
    createTerminal(hwnd);
    OutputDebugStringA("[onCreate] createEnhancedStatusBar...\n");
    createEnhancedStatusBar(hwnd);

    OutputDebugStringA("[onCreate] createOutputTabs...\n");
    createOutputTabs();
    OutputDebugStringA("[onCreate] createPowerShellPanel...\n");
    createPowerShellPanel();
    OutputDebugStringA("[onCreate] createChatPanel...\n");
    createChatPanel();

    if (m_hwndMain)
    {
        SetPropA(m_hwndMain, "RawrXD.IDE.Label", (HANDLE)RAWRXD_IDE_LABEL_MAIN_WINDOW);
        if (m_interpretabilityPanel)
            m_interpretabilityPanel->setParent(m_hwndMain);
    }

    LOG_INFO("onCreate complete — all panels created");
    OutputDebugStringA("[onCreate] all panels created OK\n");

    OutputDebugStringA("[onCreate] initSyntaxColorizer...\n");
    initSyntaxColorizer();

    OutputDebugStringA("[onCreate] initGhostText...\n");
    initGhostText();

    OutputDebugStringA("[onCreate] restoreSession...\n");
    restoreSession();

    {
        char buf[512];
        sprintf_s(buf,
                  "HWND audit: Main=%p Editor=%p Sidebar=%p "
                  "ExplorerTree=%p OutputTabs=%p PowerShellPanel=%p",
                  m_hwndMain, m_hwndEditor, m_hwndSidebar, m_hwndExplorerTree, m_hwndOutputTabs, m_hwndPowerShellPanel);
        LOG_INFO(std::string(buf));
    }

    // Apply dark theme immediately (lightweight — just sets color values + SendMessage)
    populateBuiltinThemes();  // Register all 16 built-in themes
    m_currentTheme.backgroundColor = RGB(30, 30, 30);
    m_currentTheme.textColor = RGB(212, 212, 212);
    m_currentTheme.selectionColor = RGB(38, 79, 120);
    m_currentTheme.lineNumberColor = RGB(128, 128, 128);
    if (m_backgroundBrush)
        DeleteObject(m_backgroundBrush);
    m_backgroundBrush = CreateSolidBrush(RGB(30, 30, 30));
    applyTheme();

    // Update status bar with initial state (12-part enhanced bar: Line/Col, Encoding, Language, etc.)
    if (m_hwndStatusBar)
    {
        updateEnhancedStatusBar();
        m_contextUsage.maxTokens = m_settings.aiContextWindow;
        updateContextWindowDisplay();
    }

    // Initialize backend manager and LLM router at startup so Ollama/cloud can be used
    // without requiring a local GGUF to be loaded first (see docs/AGENTIC_AND_MODEL_LOADING_AUDIT.md).
    initBackendManager();
    initLLMRouter();

    // Defer heavy init to after window is fully created
    PostMessage(hwnd, WM_APP + 100, 0, 0);
}

// ============================================================================
// deferredHeavyInit - Heavy initialization run AFTER window is fully created
// This runs outside CreateWindowExA, so SEH crashes here won't prevent
// the window from appearing.
// ============================================================================
// Static trampoline for SEH-protected background thread body.
// Cannot use lambdas inside __try (MSVC C2712), so we call through here.
// Declared as friend in Win32IDE class (external linkage) to access private members.
void bgInitBody(void* self);

// 4 MB stack for deferred init thread to avoid 0xC00000FD (STATUS_STACK_OVERFLOW)
static const DWORD kDeferredInitStackSize = 4 * 1024 * 1024;

DWORD WINAPI Win32IDE::deferredHeavyInitThreadProc(LPVOID param)
{
    Win32IDE* self = static_cast<Win32IDE*>(param);
    DetachedThreadGuard _guard(self->m_activeDetachedThreads, self->m_shuttingDown);
    if (_guard.cancelled)
        return 0;
    sehRunBgThread(bgInitBody, self);
    return 0;
}

void Win32IDE::deferredHeavyInit()
{
    // Run heavy initialization on a background thread with large stack to avoid 0xC00000FD.
    HANDLE h = CreateThread(nullptr, kDeferredInitStackSize, &Win32IDE::deferredHeavyInitThreadProc, this, 0, nullptr);
    if (h)
        CloseHandle(h);
    else
        sehRunBgThread(bgInitBody, this);
}

void bgInitBody(void* self)
{
    Win32IDE* ide = static_cast<Win32IDE*>(self);
    ide->deferredHeavyInitBody();
}

void Win32IDE::deferredHeavyInitBody()
{
    // Initialize logger under %APPDATA%\RawrXD\ide.log (fallback: RawrXD_IDE.log in cwd)
    try
    {
        std::string logPath = "RawrXD_IDE.log";
        char appData[MAX_PATH] = {};
        if (SUCCEEDED(SHGetFolderPathA(nullptr, CSIDL_APPDATA, nullptr, 0, appData)))
        {
            std::string dir = std::string(appData) + "\\RawrXD";
            CreateDirectoryA(dir.c_str(), nullptr);
            logPath = dir + "\\ide.log";
        }
        IDELogger::getInstance().initialize(logPath);
    }
    catch (...)
    {
        OutputDebugStringA("ERROR: Logger init failed\n");
    }
    if (isShuttingDown())
        return;

    // ================================================================
    // Enterprise License System — initialize FIRST (gates engine registration)
    // ================================================================
    try
    {
        auto& license = RawrXD::EnterpriseLicense::Instance();
        license.Initialize();

        auto& featureMgr = EnterpriseFeatureManager::Instance();
        featureMgr.Initialize();

        // V2 license system (61-feature manifest) + runtime enforcement
        auto& licV2 = RawrXD::License::EnterpriseLicenseV2::Instance();
        licV2.initialize();
        RawrXD::Flags::FeatureFlagsRuntime::Instance().refreshFromLicense();
        RawrXD::Enforce::LicenseEnforcer::Instance().initialize();

        OutputDebugStringA("[deferredHeavyInit] Enterprise license initialized\n");

        // Update status bar with license tier badge
        std::string tierBadge = std::string("[") + license.GetEditionName() + "]";
        PostMessage(m_hwndMain, WM_USER + 200, 0, reinterpret_cast<LPARAM>(_strdup(tierBadge.c_str())));
    }
    catch (...)
    {
        OutputDebugStringA("ERROR: Enterprise license init failed\n");
    }
    if (isShuttingDown())
        return;

    // Initialize Native CPU Inference Engine
    try
    {
        m_nativeEngine = std::make_unique<RawrXD::CPUInferenceEngine>();
        auto memPlugin = std::make_shared<RawrXD::Modules::NativeMemoryModule>();
        m_nativeEngine->RegisterMemoryPlugin(memPlugin);
        m_nativeEngineLoaded = true;
    }
    catch (...)
    {
        m_nativeEngine.reset();
        m_nativeEngineLoaded = false;
        OutputDebugStringA("ERROR: CPUInferenceEngine init failed\n");
    }
    if (isShuttingDown())
        return;

    // Initialize DirectX renderer (needs to be on UI thread ideally, but creation is OK)
    try
    {
        m_renderer = std::make_unique<TransparentRenderer>();
    }
    catch (...)
    {
        m_renderer = nullptr;
        OutputDebugStringA("ERROR: TransparentRenderer creation failed\n");
    }

    // Initialize PowerShell state
    try
    {
        initializePowerShellState();
    }
    catch (...)
    {
        OutputDebugStringA("ERROR: PowerShell init failed\n");
    }

    // Theme already applied in onCreate — skip here

    // Load code snippets
    try
    {
        loadCodeSnippets();
    }
    catch (...)
    {
        OutputDebugStringA("ERROR: Code snippets loading failed\n");
    }

    // Initialize Agent
    try
    {
        if (m_nativeEngine)
        {
            m_agent = std::make_unique<RawrXD::NativeAgent>(m_nativeEngine.get());
            m_agent->SetOutputCallback([this](const std::string& text) { postAgentOutputSafe(text); });
            m_agent->SetMaxMode(true);
            m_agent->SetDeepThink(true);
            m_agent->SetDeepResearch(true);
            m_agent->SetNoRefusal(true);
        }
    }
    catch (...)
    {
        OutputDebugStringA("ERROR: NativeAgent init failed\n");
    }

    // Initialize Extension Loader
    try
    {
        m_extensionLoader = std::make_unique<RawrXD::ExtensionLoader>();
        m_extensionLoader->Scan();
        m_extensionLoader->LoadNativeModules();
    }
    catch (...)
    {
        OutputDebugStringA("ERROR: ExtensionLoader init failed\n");
    }
    if (isShuttingDown())
        return;

    // Initialise the agentic bridge (needs m_hwndMain, which is set)
    try
    {
        initializeAgenticBridge();
    }
    catch (...)
    {
        OutputDebugStringA("ERROR: initializeAgenticBridge failed\n");
    }

    // Initialize AI/Extensions panels so menu -> show() creates real UI
    if (isShuttingDown())
        return;
    try
    {
        if (m_modelRegistry)
            m_modelRegistry->initialize();
        if (m_interpretabilityPanel)
            m_interpretabilityPanel->initialize();
        if (m_benchmarkMenu && m_hwndMain)
        {
            m_benchmarkMenu->setMainWindow(m_hwndMain);
            m_benchmarkMenu->initialize();
        }
    }
    catch (...)
    {
        OutputDebugStringA("ERROR: AI panels init failed\n");
    }

    // Initialize Ghost Text renderer (Copilot-style inline completions)
    try
    {
        initGhostText();
    }
    catch (...)
    {
        OutputDebugStringA("ERROR: initGhostText failed\n");
    }

    // Initialize Failure Detector (agent self-correction)
    try
    {
        initFailureDetector();
    }
    catch (...)
    {
        OutputDebugStringA("ERROR: initFailureDetector failed\n");
    }

    // Initialize Agent Diff Panel (Win32IDE_AgentPanel.cpp)
    try
    {
        initAgentPanel();
    }
    catch (...)
    {
        OutputDebugStringA("ERROR: initAgentPanel failed\n");
    }

    // Load persistent settings from %APPDATA%\RawrXD\settings.json
    try
    {
        loadSettings();
        applySettings();
    }
    catch (...)
    {
        OutputDebugStringA("ERROR: loadSettings/applySettings failed\n");
    }
    syncAgentModeUiFromBridge();

    if (isShuttingDown())
        return;

    // Initialize Agent History (append-only JSONL event log)
    try
    {
        initAgentHistory();
    }
    catch (...)
    {
        OutputDebugStringA("ERROR: initAgentHistory failed\n");
    }

    // Initialize Failure Intelligence — Phase 6 (classification + retry strategies)
    try
    {
        initFailureIntelligence();
    }
    catch (...)
    {
        OutputDebugStringA("ERROR: initFailureIntelligence failed\n");
    }

    // Initialize Unified Model Source Resolver (HuggingFace, Ollama blobs, HTTP, local)
    try
    {
        m_modelResolver = std::make_unique<RawrXD::ModelSourceResolver>();
        // Set cache directory for downloaded models
        m_modelResolver->SetCacheDirectory(
            m_modelResolver->GetCacheDirectory());  // Use default: %USERPROFILE%/.cache/rawrxd/models
        OutputDebugStringA("ModelSourceResolver initialized OK\n");
    }
    catch (const std::exception& e)
    {
        m_modelResolver.reset();
        OutputDebugStringA("ERROR: ModelSourceResolver init failed: ");
        OutputDebugStringA(e.what());
        OutputDebugStringA("\n");
    }
    catch (...)
    {
        m_modelResolver.reset();
        OutputDebugStringA("ERROR: ModelSourceResolver init failed (unknown)\n");
    }

    // GPU Backend Bridge — detect and initialize Vulkan compute if available
    {
        HMODULE hVulkan = LoadLibraryA("vulkan-1.dll");
        if (hVulkan)
        {
            m_gpuTextEnabled = true;
            FreeLibrary(hVulkan);
            OutputDebugStringA("GPU Backend Bridge: Vulkan ICD detected — GPU compute available\n");
            appendToOutput("[GPU] Vulkan compute backend detected and ready\n", "Output", OutputSeverity::Info);
        }
        else
        {
            m_gpuTextEnabled = false;
            OutputDebugStringA("GPU Backend Bridge: No Vulkan ICD — CPU-only mode\n");
        }
    }

    // Initialize Phase 10: Execution Governor + Safety + Replay + Confidence
    try
    {
        initPhase10();
    }
    catch (...)
    {
        OutputDebugStringA("ERROR: initPhase10 failed\n");
    }

    // Initialize MultiResponse, LSP Server, Hotpatch UI (lazy-ready)
    try
    {
        initMultiResponse();
    }
    catch (...)
    {
        OutputDebugStringA("ERROR: initMultiResponse failed\n");
    }
    try
    {
        initLSPServer();
    }
    catch (...)
    {
        OutputDebugStringA("ERROR: initLSPServer failed\n");
    }
    try
    {
        initHotpatchUI();
    }
    catch (...)
    {
        OutputDebugStringA("ERROR: initHotpatchUI failed\n");
    }

    // Initialize Phase 11: Distributed Swarm Compilation
    try
    {
        initPhase11();
    }
    catch (...)
    {
        OutputDebugStringA("ERROR: initPhase11 failed\n");
    }

    // Initialize Phase 12: Native Debugger Engine
    try
    {
        initPhase12();
    }
    catch (...)
    {
        OutputDebugStringA("ERROR: initPhase12 failed\n");
    }

    // Initialize Decompiler View (Phase 18B)
    try
    {
        initDecompilerView();
    }
    catch (...)
    {
        OutputDebugStringA("ERROR: initDecompilerView failed\n");
    }

    // Initialize Phase 33: Voice Chat Engine
    try
    {
        initVoiceChat();
        voiceLoadPreferences();
        createVoiceChatPanel(m_hwndMain);
        registerVoiceHotkeys();
        updateVoiceStatusBar();
    }
    catch (...)
    {
        OutputDebugStringA("ERROR: initVoiceChat failed\n");
    }

    // Initialize Phase 44: Voice Automation (TTS for responses)
    try
    {
        RECT rc;
        GetClientRect(m_hwndMain, &rc);
        extern void Win32IDE_CreateVoiceAutomationPanel(HWND, int, int, int, int);
        Win32IDE_CreateVoiceAutomationPanel(m_hwndMain, 0, rc.bottom - 80, rc.right, 80);
        extern void Win32IDE_AddVoiceAutomationMenu(HMENU);
        // Menu items already added in menu creation; just mark initialized
        m_voiceAutomationInitialized = true;
        OutputDebugStringA("Phase 44: VoiceAutomation panel created\n");
    }
    catch (...)
    {
        OutputDebugStringA("ERROR: VoiceAutomation init failed\n");
    }

    // Initialize Tier 3: Polish (QoL) — smooth caret, ligatures, file watcher, etc.
    try
    {
        initTier3Polish();
    }
    catch (...)
    {
        OutputDebugStringA("ERROR: initTier3Polish failed\n");
    }

    // Initialize Tier 1: Critical Cosmetics (smooth scroll, minimap, fuzzy palette, etc.)
    try
    {
        initTier1Cosmetics();
    }
    catch (...)
    {
        OutputDebugStringA("ERROR: initTier1Cosmetics failed\n");
    }

    // Initialize Phase 33: Quick-Win Systems (Shortcuts, Backups, Alerts, SLO)
    try
    {
        initQuickWinSystems();
    }
    catch (...)
    {
        OutputDebugStringA("ERROR: initQuickWinSystems failed\n");
    }

    // Initialize Phase 32B: Chain-of-Thought Multi-Model Review Engine
    try
    {
        initChainOfThought();
    }
    catch (...)
    {
        OutputDebugStringA("ERROR: initChainOfThought failed\n");
    }

    // Initialize Phase 34: Telemetry Export Subsystem
    try
    {
        initTelemetry();
    }
    catch (...)
    {
        OutputDebugStringA("ERROR: initTelemetry failed\n");
    }

    // Initialize Phase 36: Flight Recorder — persistent binary ring-buffer
    try
    {
        initFlightRecorder();
    }
    catch (...)
    {
        OutputDebugStringA("ERROR: initFlightRecorder failed\n");
    }

    // Initialize Phase 36: MCP Integration — Model Context Protocol
    try
    {
        initMCP();
    }
    catch (...)
    {
        OutputDebugStringA("ERROR: initMCP failed\n");
    }

    // Initialize Phase 29+36: VS Code Extension API + QuickJS VSIX Host
    try
    {
        initVSCodeExtensionAPI();
    }
    catch (...)
    {
        OutputDebugStringA("ERROR: initVSCodeExtensionAPI failed\n");
    }

    if (isShuttingDown())
        return;

    // Initialize Phase 43: Plugin System (Native Win32 DLL loading)
    try
    {
        initPluginSystem();
    }
    catch (...)
    {
        OutputDebugStringA("ERROR: initPluginSystem failed\n");
    }

    // Auto-start Local HTTP server (port 11435) so HTML beacon / Ghost can detect IDE
    if (!isShuttingDown())
    {
        try
        {
            startLocalServer();
        }
        catch (...)
        {
            OutputDebugStringA("ERROR: startLocalServer failed\n");
        }
    }

    // Initialize Cursor/JB-Parity Feature Modules
    if (!isShuttingDown())
    {
        try
        {
            initAllFeatureModules();
        }
        catch (...)
        {
            OutputDebugStringA("ERROR: initAllFeatureModules failed\n");
        }
    }

    // Initialize Tier 5 cosmetic features (Emoji, Telemetry Dashboard, Shortcut Editor, etc.)
    if (!isShuttingDown())
    {
        try
        {
            initTier5Cosmetics();
        }
        catch (...)
        {
            OutputDebugStringA("ERROR: initTier5Cosmetics failed\n");
        }
    }

    OutputDebugStringA("deferredHeavyInit complete (background thread)\n");

    // Notify UI thread to refresh
    if (m_hwndMain && !isShuttingDown())
    {
        PostMessage(m_hwndMain, WM_APP + 101, 0, 0);
    }
}

// ============================================================================
// onDestroy - Called when WM_DESTROY is received
// ============================================================================
void Win32IDE::onDestroy()
{
    LOG_INFO("Win32IDE::onDestroy - shutting down");

    // Signal ALL detached threads to stop touching 'this'
    m_shuttingDown.store(true, std::memory_order_release);

    // Stop visibility watchdog thread first so it cannot race on HWND usage
    // while the rest of shutdown tears down UI resources.
    stopVisibilityWatchdog();

    // Stop any in-progress inference immediately
    m_inferenceStopRequested = true;
    m_planExecutionCancelled.store(true);

    // Wait for all detached threads to notice the flag and exit (up to 3s).
    for (int i = 0; i < 60 && m_activeDetachedThreads.load(std::memory_order_acquire) > 0; ++i)
    {
        Sleep(50);
    }
    if (m_activeDetachedThreads.load(std::memory_order_acquire) > 0)
    {
        OutputDebugStringA("onDestroy: WARNING — detached threads still active after 3s\n");
        Sleep(200);  // Extra grace
    }

    // Shutdown Phase 29+36: VS Code Extension API + QuickJS VSIX Host
    shutdownVSCodeExtensionAPI();

    // Shutdown core runtime spine (signature verifier, sqlite, telemetry export).
    shutdownCoreRuntimeSpine();

    // Shutdown Tier 3: Polish (smooth caret, ligatures, file watcher)
    shutdownTier3Polish();

    // Shutdown Tier 1: Critical Cosmetics (smooth scroll, minimap, auto-update)
    shutdownTier1Cosmetics();

    // Shutdown Phase 36: MCP Integration
    shutdownMCP();

    // Shutdown Phase 36: Flight Recorder
    shutdownFlightRecorder();

    // Shutdown Phase 34: Telemetry Export
    shutdownTelemetry();

    // Shutdown Phase 44: Voice Automation
    if (m_voiceAutomationInitialized)
    {
        extern void Win32IDE_DestroyVoiceAutomationPanel();
        Win32IDE_DestroyVoiceAutomationPanel();
        m_voiceAutomationInitialized = false;
    }

    // Shutdown Phase 33: Voice Chat Engine
    voiceSavePreferences();
    unregisterVoiceHotkeys();
    shutdownVoiceChat();

    // Shutdown Phase 33: Quick-Win Systems
    shutdownQuickWinSystems();

    // Shutdown Phase 12: Native Debugger Engine
    shutdownPhase12();

    // Shutdown Phase 11: Distributed Swarm Compilation
    shutdownPhase11();

    // Shutdown Phase 10: Execution Governor + Safety + Replay + Confidence
    shutdownPhase10();

    // Shutdown ghost text renderer (kill timers, free font)
    shutdownGhostText();

    // Stop local GGUF HTTP server
    stopLocalServer();

    // Shutdown agent history (flush event buffer to disk)
    shutdownAgentHistory();

    // Shutdown backend manager (save configs)
    shutdownBackendManager();

    // Shutdown Phase 43: Plugin System (unload all DLLs)
    shutdownPlugins();

    // ========================================================================
    // CRITICAL: Stop all terminals BEFORE saving state / destroying objects.
    // Terminal threads call onOutput/onError/onFinished callbacks that capture
    // [this]. If these fire during destructor member teardown → 0xC0000005.
    // ========================================================================
    // Stop dedicated PowerShell terminal first
    if (m_dedicatedPowerShellTerminal)
    {
        m_dedicatedPowerShellTerminal->onOutput = nullptr;
        m_dedicatedPowerShellTerminal->onError = nullptr;
        m_dedicatedPowerShellTerminal->onStarted = nullptr;
        m_dedicatedPowerShellTerminal->onFinished = nullptr;
        m_dedicatedPowerShellTerminal->stop();
        m_dedicatedPowerShellTerminal.reset();
    }
    // Stop all terminal panes — clear callbacks first to prevent use-after-free
    for (auto& pane : m_terminalPanes)
    {
        if (pane.manager)
        {
            pane.manager->onOutput = nullptr;
            pane.manager->onError = nullptr;
            pane.manager->onStarted = nullptr;
            pane.manager->onFinished = nullptr;
            pane.manager->stop();
            pane.manager.reset();
        }
    }
    m_terminalPanes.clear();

    // Save settings to disk
    try
    {
        saveSettings();
    }
    catch (...)
    {
    }

    // Save full session state for next launch
    saveSession();

    // Clean up resources
    if (m_renderer)
    {
        m_renderer.reset();
    }

    // Save any unsaved state / editor config for session restore
    try
    {
        nlohmann::json session;

        // Save open file path
        if (!m_currentFile.empty())
        {
            session["lastOpenFile"] = m_currentFile;
        }

        // Save window position and size
        if (m_hwndMain)
        {
            RECT rc;
            if (GetWindowRect(m_hwndMain, &rc))
            {
                session["window"]["x"] = (int)rc.left;
                session["window"]["y"] = (int)rc.top;
                session["window"]["width"] = (int)(rc.right - rc.left);
                session["window"]["height"] = (int)(rc.bottom - rc.top);
            }
            session["window"]["maximized"] = (IsZoomed(m_hwndMain) != 0);
        }

        // Save open tabs
        nlohmann::json tabs = nlohmann::json::array();
        for (const auto& tab : m_editorTabs)
        {
            nlohmann::json t;
            t["path"] = tab.filePath;
            t["name"] = tab.displayName;
            t["modified"] = tab.modified;
            tabs.push_back(t);
        }
        session["tabs"] = tabs;

        // Save current working directory
        if (!m_currentDirectory.empty())
        {
            session["workingDirectory"] = m_currentDirectory;
        }

        // Save sidebar width / active view
        session["sidebar"]["activeView"] = static_cast<int>(m_currentSidebarView);
        session["sidebar"]["width"] = m_sidebarWidth;

        // Write session file
        std::string sessionPath = ".rawrxd/session.json";
        CreateDirectoryA(".rawrxd", nullptr);
        std::ofstream sessionFile(sessionPath);
        if (sessionFile)
        {
            sessionFile << session.dump(4);
            LOG_INFO("Session state saved to " + sessionPath);
        }
    }
    catch (const std::exception& e)
    {
        LOG_ERROR("Failed to save session state: " + std::string(e.what()));
    }

    // ========================================================================
    // PHASE 2: Tear down shared objects that detached threads may reference.
    // By doing this here (before the destructor), we ensure that even if a
    // lingering detached thread survives the 3s wait, it hits the shutdown
    // flag check before touching any of these objects.
    // ========================================================================
    try
    {
        m_subAgentManager.reset();
    }
    catch (...)
    {
    }
    try
    {
        m_multiResponseEngine.reset();
    }
    catch (...)
    {
    }
    try
    {
        // Raw pointer; just clear reference
        m_agenticBridge = nullptr;
    }
    catch (...)
    {
    }
    try
    {
        m_agent.reset();
    }
    catch (...)
    {
    }
    try
    {
        m_nativeEngine.reset();
    }
    catch (...)
    {
    }
    try
    {
        m_modelResolver.reset();
    }
    catch (...)
    {
    }
    try
    {
        m_ggufLoader.reset();
    }
    catch (...)
    {
    }
    try
    {
        m_extensionLoader.reset();
    }
    catch (...)
    {
    }
    try
    {
        m_pluginLoader.reset();
    }
    catch (...)
    {
    }
    try
    {
        m_lspServer.reset();
    }
    catch (...)
    {
    }
    try
    {
        m_mcpServer.reset();
    }
    catch (...)
    {
    }
    try
    {
        m_autonomyManager.reset();
    }
    catch (...)
    {
    }

    // Null out raw pointers to externally-owned objects
    m_engineManager = nullptr;
    m_codexUltimate = nullptr;

    // Null main window to prevent use-after-destroy in destructor or stray callbacks
    m_hwndMain = nullptr;

    OutputDebugStringA("onDestroy: all resources released\n");
}

// ============================================================================
// onCommand - WM_COMMAND dispatcher
// ============================================================================
void Win32IDE::onCommand(HWND hwnd, int id, HWND hwndCtl, UINT codeNotify)
{
    // Handle editor notifications (EN_CHANGE, EN_VSCROLL, EN_SELCHANGE come via WM_COMMAND)
    if (hwndCtl == m_hwndEditor)
    {
        if (codeNotify == EN_CHANGE || codeNotify == EN_VSCROLL || codeNotify == EN_SELCHANGE)
        {
            updateLineNumbers();
            // Debounce syntax coloring on content/scroll change
            if (m_syntaxColoringEnabled)
            {
                onEditorContentChanged();
            }
            // Update cursor position in status bar
            CHARRANGE sel;
            SendMessage(m_hwndEditor, EM_EXGETSEL, 0, (LPARAM)&sel);
            int line = (int)SendMessage(m_hwndEditor, EM_LINEFROMCHAR, sel.cpMin, 0);
            int lineStart = (int)SendMessage(m_hwndEditor, EM_LINEINDEX, line, 0);
            int col = sel.cpMin - lineStart;
            wchar_t posBuf[64];
            swprintf(posBuf, 64, L"Ln %d, Col %d", line + 1, col + 1);
            if (m_hwndStatusBar)
            {
                SendMessage(m_hwndStatusBar, SB_SETTEXT, 1, (LPARAM)posBuf);
            }
        }
        return;  // Don't route editor notifications through the command system
    }

    // First try the unified command router
    if (id == 9903)
    {  // Model progress cancel button
        cancelModelOperation();
        return;
    }
    // Copilot secondary sidebar control IDs (created in createSecondarySidebar)
    if (id == 1204)
    {
        HandleCopilotSend();
        return;
    }
    if (id == 1205)
    {
        HandleCopilotClear();
        return;
    }
    if (id == 1206)
    {
        setAgenticMode(RawrXD::AgenticMode::Plan);
        return;
    }
    if (id == 1207)
    {
        setAgenticMode(RawrXD::AgenticMode::Agent);
        return;
    }
    if (id == 1208)
    {
        setAgenticMode(RawrXD::AgenticMode::Ask);
        return;
    }

    // Agent diff panel buttons (created in initAgentPanel)
    if (id == 14003)
    {
        agentAcceptAll();
        refreshAgentDiffDisplay();
        return;
    }
    if (id == 14004)
    {
        agentRejectAll();
        refreshAgentDiffDisplay();
        return;
    }
    if (id == 14005)
    {
        onBoundedAgentLoop();
        return;
    }

    // Audit commands (9500-9506) — handle directly; SSOT handlers PostMessage to us, avoid loop
    if (id >= 9500 && id < 9600)
    {
        handleAuditCommand(id);
        return;
    }

    // ── GUI-ONLY FILE / MODEL / VIEW TOGGLES — Win32 menu IDs that must run IDE actions (dialogs, loading, toggles).
    // All other commands go through unified dispatch (Agent, Autonomy, Backend, LSP, Hotpatch, etc.).
    switch (id)
    {
        case 998:  // SCM context menu: Discard Changes
            discardChanges();
            return;
        case 1100:  // IDC_ACTIVITY_BAR
            if (!m_sidebarVisible)
            {
                toggleSidebar();
            }
            if (m_currentSidebarView == SidebarView::None)
            {
                setSidebarView(SidebarView::Explorer);
            }
            if (m_hwndSidebar && IsWindow(m_hwndSidebar))
            {
                SetFocus(m_hwndSidebar);
            }
            return;
        case 1201:  // IDC_SECONDARY_SIDEBAR_HEADER
            if (!m_secondarySidebarVisible)
            {
                toggleSecondarySidebar();
            }
            if (m_hwndCopilotChatInput && IsWindow(m_hwndCopilotChatInput))
            {
                SetFocus(m_hwndCopilotChatInput);
            }
            else if (m_hwndSecondarySidebar && IsWindow(m_hwndSecondarySidebar))
            {
                SetFocus(m_hwndSecondarySidebar);
            }
            return;
        case 5000:  // IDC_PS_PANEL_CONTAINER
            showPowerShellPanel();
            if (m_hwndPowerShellInput && IsWindow(m_hwndPowerShellInput))
            {
                SetFocus(m_hwndPowerShellInput);
            }
            else if (m_hwndPowerShellPanel && IsWindow(m_hwndPowerShellPanel))
            {
                SetFocus(m_hwndPowerShellPanel);
            }
            return;
        case 5006:  // IDC_MP_LOAD_VSCODE
        case 5007:
        {  // IDC_MP_DOWNLOAD_INSTALL
            initMarketplace();
            cmdMarketplaceShow();
            HWND hMarketplace = FindWindowW(L"RawrXD_Marketplace", nullptr);
            if (hMarketplace && IsWindow(hMarketplace))
            {
                SendMessageW(hMarketplace, WM_COMMAND, MAKEWPARAM(id, BN_CLICKED), 0);
            }
            else
            {
                appendToOutput("[Marketplace] Window not available for command id " + std::to_string(id) + "\n",
                               "Output", OutputSeverity::Warning);
            }
            return;
        }
        case 9000:  // Legacy alias used by VSCodeExtAPI bridge for hotpatch status
            handleHotpatchCommand(IDM_HOTPATCH_SHOW_STATUS);
            return;
        case 1300:  // IDC_PANEL_CONTAINER
            if (!m_panelVisible)
            {
                togglePanel();
            }
            switchPanelTab(m_activePanelTab);
            if (m_activePanelTab == PanelTab::Problems && m_hwndProblemsListView && IsWindow(m_hwndProblemsListView))
            {
                SetFocus(m_hwndProblemsListView);
            }
            else if (m_activePanelTab == PanelTab::Terminal)
            {
                TerminalPane* activePane = getActiveTerminalPane();
                if (activePane && activePane->hwnd && IsWindow(activePane->hwnd))
                {
                    SetFocus(activePane->hwnd);
                }
            }
            else if (m_activePanelTab == PanelTab::Output)
            {
                auto it = m_outputWindows.find(m_activeOutputTab);
                if (it != m_outputWindows.end() && it->second && IsWindow(it->second))
                {
                    SetFocus(it->second);
                }
            }
            else if (m_activePanelTab == PanelTab::DebugConsole && m_hwndDebugConsole && IsWindow(m_hwndDebugConsole))
            {
                SetFocus(m_hwndDebugConsole);
            }
            return;
        case 1306:  // IDC_PANEL_TOOLBAR
            if (!m_panelVisible)
            {
                togglePanel();
            }
            switchPanelTab(m_activePanelTab);
            if (m_hwndPanelTabs && IsWindow(m_hwndPanelTabs))
            {
                SetFocus(m_hwndPanelTabs);
            }
            return;
        case 1310:  // IDC_PANEL_BTN_MAXIMIZE
            maximizePanel();
            return;
        case 1311:  // IDC_PANEL_BTN_CLOSE
            if (m_panelVisible)
            {
                togglePanel();
            }
            return;
        case 1312:  // IDC_PANEL_PROBLEMS_LIST
            switchPanelTab(PanelTab::Problems);
            refreshProblemsView();
            if (m_hwndProblemsListView && IsWindow(m_hwndProblemsListView))
            {
                int idx = (int)ListView_GetNextItem(m_hwndProblemsListView, -1, LVNI_SELECTED);
                if (idx >= 0)
                {
                    goToProblem(idx);
                }
                else
                {
                    SetFocus(m_hwndProblemsListView);
                }
            }
            return;
        case 1301:
        {  // IDC_PANEL_TABS
            if (m_hwndPanelTabs && IsWindow(m_hwndPanelTabs))
            {
                int idx = (int)TabCtrl_GetCurSel(m_hwndPanelTabs);
                if (idx < 0)
                    idx = 0;
                if (idx > 3)
                    idx = 3;
                switchPanelTab(static_cast<PanelTab>(idx));
            }
            return;
        }
        case 1307:  // IDC_PANEL_BTN_NEW_TERMINAL
            switchPanelTab(PanelTab::Terminal);
            handleTerminalCommand(4001);  // Start PowerShell terminal
            return;
        case 1308:  // IDC_PANEL_BTN_SPLIT_TERMINAL
            switchPanelTab(PanelTab::Terminal);
            handleTerminalCommand(4005);  // Split terminal
            return;
        case 1309:  // IDC_PANEL_BTN_KILL_TERMINAL
            switchPanelTab(PanelTab::Terminal);
            handleTerminalCommand(IDM_TERMINAL_KILL);
            return;
        case IDC_PROBLEMS_PANEL:  // 7050
            if (!m_problemsPanelInitialized)
            {
                initProblemsPanel();
            }
            if (m_hwndProblemsPanel && IsWindow(m_hwndProblemsPanel))
            {
                ShowWindow(m_hwndProblemsPanel, SW_SHOW);
            }
            refreshProblemsView();
            if (m_hwndProblemsListView && IsWindow(m_hwndProblemsListView))
            {
                SetFocus(m_hwndProblemsListView);
            }
            return;
        case IDC_PROBLEMS_LISTVIEW:  // 7051
            if (m_hwndProblemsListView && IsWindow(m_hwndProblemsListView))
            {
                int idx = (int)ListView_GetNextItem(m_hwndProblemsListView, -1, LVNI_SELECTED);
                if (idx >= 0)
                {
                    goToProblem(idx);
                }
                else
                {
                    SetFocus(m_hwndProblemsListView);
                }
            }
            return;
        case 2114:  // IDC_DEBUGGER_MEMORY
            if (m_debuggerAttached)
            {
                updateMemoryView();
            }
            if (m_hwndDebuggerMemory && IsWindow(m_hwndDebuggerMemory))
            {
                SetFocus(m_hwndDebuggerMemory);
            }
            return;
        case 2001:
            newFile();
            if (m_hwndStatusBar)
                SendMessage(m_hwndStatusBar, SB_SETTEXT, 0, (LPARAM)L"New file created");
            return;
        case 2002:
            openFile();
            return;
        case 2003:
            if (saveFile() && m_hwndStatusBar)
                SendMessage(m_hwndStatusBar, SB_SETTEXT, 0, (LPARAM)L"File saved");
            return;
        case 2004:
            if (saveFileAs() && m_hwndStatusBar)
                SendMessage(m_hwndStatusBar, SB_SETTEXT, 0, (LPARAM)L"File saved as new name");
            return;
        case 2005:
            if (!m_fileModified || promptSaveChanges())
                PostQuitMessage(0);
            return;
        case 1030:
            openModel();
            return;
        case 1031:
            openModelFromHuggingFace();
            return;
        case 1032:
            openModelFromOllama();
            return;
        case 1033:
            openModelFromURL();
            return;
        case 1034:
            openModelUnified();
            return;
        case 1035:
            quickLoadGGUFModel();
            return;
        case 2007:  // Edit > Undo (Win32 menu ID)
            if (m_hwndEditor)
                SendMessage(m_hwndEditor, EM_UNDO, 0, 0);
            if (m_hwndStatusBar)
                SendMessage(m_hwndStatusBar, SB_SETTEXT, 0, (LPARAM)L"Undo");
            return;
        case 2008:  // Edit > Redo
            if (m_hwndEditor)
                SendMessage(m_hwndEditor, EM_REDO, 0, 0);
            if (m_hwndStatusBar)
                SendMessage(m_hwndStatusBar, SB_SETTEXT, 0, (LPARAM)L"Redo");
            return;
        case 2009:  // Edit > Cut
            if (m_hwndEditor)
            {
                SendMessage(m_hwndEditor, WM_CUT, 0, 0);
                m_fileModified = true;
            }
            if (m_hwndStatusBar)
                SendMessage(m_hwndStatusBar, SB_SETTEXT, 0, (LPARAM)L"Cut");
            return;
        case 2010:  // Edit > Copy
            if (m_hwndEditor)
                SendMessage(m_hwndEditor, WM_COPY, 0, 0);
            if (m_hwndStatusBar)
                SendMessage(m_hwndStatusBar, SB_SETTEXT, 0, (LPARAM)L"Copied");
            return;
        case 2011:  // Edit > Paste
            if (m_hwndEditor)
            {
                SendMessage(m_hwndEditor, WM_PASTE, 0, 0);
                m_fileModified = true;
            }
            if (m_hwndStatusBar)
                SendMessage(m_hwndStatusBar, SB_SETTEXT, 0, (LPARAM)L"Pasted");
            return;
        case 2026:  // View > Use Streaming Loader — toggle so model loading uses low-memory path
            m_useStreamingLoader = !m_useStreamingLoader;
            if (m_hMenu)
                CheckMenuItem(m_hMenu, 2026, MF_BYCOMMAND | (m_useStreamingLoader ? MF_CHECKED : MF_UNCHECKED));
            appendToOutput(std::string("Streaming loader ") + (m_useStreamingLoader ? "ON" : "OFF") + "\n", "Output",
                           OutputSeverity::Info);
            return;
        case 2027:  // View > Vulkan Renderer
            m_useVulkanRenderer = !m_useVulkanRenderer;
            if (m_hMenu)
                CheckMenuItem(m_hMenu, 2027, MF_BYCOMMAND | (m_useVulkanRenderer ? MF_CHECKED : MF_UNCHECKED));
            appendToOutput(std::string("Vulkan renderer ") + (m_useVulkanRenderer ? "ON" : "OFF") + "\n", "Output",
                           OutputSeverity::Info);
            persistPerformanceVulkanRendererToConfig();
            appendToOutput("[Vulkan] Saved preference performance.vulkanRenderer to rawrxd.config.json (cwd or exe dir).\n",
                           "Output", OutputSeverity::Info);
            return;
        case 502:   // Tools > Settings (IDM_TOOLS_SETTINGS)
        case 1024:  // Title bar gear (IDC_BTN_SETTINGS)
        case 1106:  // Activity bar Settings (IDC_ACTBAR_SETTINGS)
            showSettingsGUIDialog();
            return;
        case 1022:  // Title bar GH button — toggle AI Chat panel (secondary sidebar)
            toggleSecondarySidebar();
            if (m_hwndStatusBar)
                SendMessage(m_hwndStatusBar, SB_SETTEXT, 0,
                            (LPARAM)(m_secondarySidebarVisible ? L"AI Chat shown" : L"AI Chat hidden"));
            return;
        case 3007:  // View > AI Chat — toggle secondary sidebar (Ctrl+Alt+B)
        case 3009:  // View > Agent Chat (autonomous)
            toggleSecondarySidebar();
            if (m_hwndStatusBar)
                SendMessage(m_hwndStatusBar, SB_SETTEXT, 0,
                            (LPARAM)(m_secondarySidebarVisible ? L"Chat panel shown" : L"Chat panel hidden"));
            return;
        case IDM_VIEW_AGENT_PANEL:
            toggleAgentPanel();
            return;
        case IDM_SECURITY_SCAN_SECRETS:
            RunSecretsScan();
            return;
        case IDM_SECURITY_SCAN_SAST:
            RunSastScan();
            return;
        case IDM_SECURITY_SCAN_DEPENDENCIES:
            RunDependencyAudit();
            return;
        case IDM_BUILD_SOLUTION:
            runBuildInBackground(m_gitRepoPath, "");
            return;
        case IDM_BUILD_PROJECT:
            runBuildInBackground(m_gitRepoPath, "--target RawrXD-Win32IDE");
            return;
        case IDM_BUILD_CLEAN:
            runBuildInBackground(m_gitRepoPath, "--target clean");
            return;
        default:
            break;
    }

    // ── LEGACY FALLBACK — View/Tier1/Git/Monaco commands that routeToIde would loop on
    // routeCommand invokes handleViewCommand, handleTier1Command, etc. directly instead of
    // going through SSOT handlers that PostMessage same ID → infinite re-entry.
    if (routeCommand(id))
    {
        return;
    }

    // ── UNIFIED DISPATCH — The ONE AND ONLY command path ────────────────
    // All commands live in COMMAND_TABLE (command_registry.hpp).
    // If routeCommandUnified returns false, the command does NOT EXIST.
    if (routeCommandUnified(id, this, hwnd))
    {
        return;  // Dispatched via g_commandRegistry[] — identical path to CLI
    }

    // Command not found — direct user so they know the command wasn't handled
    if (m_hwndStatusBar && IsWindow(m_hwndStatusBar))
    {
        char buf[96];
        snprintf(buf, sizeof(buf), "Unknown command (id %d) — not in registry", id);
        SendMessageA(m_hwndStatusBar, SB_SETTEXTA, 0, (LPARAM)buf);
    }
#ifdef _DEBUG
    {
        char dbgBuf[128];
        snprintf(dbgBuf, sizeof(dbgBuf), "[SSOT] Unregistered WM_COMMAND: %d\n", id);
        OutputDebugStringA(dbgBuf);
    }
#endif
    DefWindowProcA(hwnd, WM_COMMAND, MAKEWPARAM(id, codeNotify), (LPARAM)hwndCtl);
}

void Win32IDE::persistPerformanceVulkanRendererToConfig()
{
    auto& cfg = IDEConfig::getInstance();
    cfg.setBool("performance.vulkanRenderer", m_useVulkanRenderer);
    if (cfg.saveToFile("rawrxd.config.json"))
        return;
    char exePath[MAX_PATH] = {};
    if (GetModuleFileNameA(nullptr, exePath, MAX_PATH) == 0)
        return;
    std::string dir(exePath);
    const size_t ls = dir.find_last_of("\\/");
    if (ls != std::string::npos)
    {
        dir = dir.substr(0, ls + 1);
        cfg.saveToFile(dir + "rawrxd.config.json");
    }
}
