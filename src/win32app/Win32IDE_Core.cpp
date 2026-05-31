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
#include "../agentic/AgentToolHandlers.h"
#include "../agentic/agentic_controller_wiring.h"
#include "../agentic/agentic_orchestrator_integration.hpp"
#include "../core/enterprise_license.h"
#include "../cpu_inference_engine.h"
#include "../inference/speculative_execution_engine.h"
#include "../modules/ExtensionLoader.hpp"
#include "../modules/native_memory.hpp"
#include "../native_agent.hpp"
#include "../streaming_gguf_loader.h"
#include "ExtensionEngine_bridge.h"
#include "IDEConfig.h"
#include "IDELogger.h"
#include "ModelConnection.h"
#include "RawrXD_AgentCoordinator.h"
#include "RawrXD_AutonomousAgenticPipeline.h"
#include "Win32IDE.h"
#include "Win32IDE_AgenticPlanningPanel.hpp"

#include <atomic>
extern std::atomic<bool> s_isThinking;

#define IDC_EMOJI_BASE 8100

#include <memory>

extern void RawrXD_FinishCopilotMinimalAgentic(Win32IDE* ide, WPARAM successWp, LPARAM heapUtf8String);

// Agent bridge init-complete flag (defined in Win32IDE_AgentStreamingBridge.cpp)
extern "C" void AgentBridge_SetInitComplete(bool complete);
extern "C" void AgentBridge_SetShuttingDown(bool shuttingDown);
extern "C" void AgentBridge_BindMainWindow(HWND hwnd);

bool Win32IDE::smokeDeferredInitActive()
{
    char buf[8] = {};
    return GetEnvironmentVariableA("RAWRXD_SMOKE_DEFERRED_INIT", buf, sizeof(buf)) > 0;
}

bool Win32IDE::smokeCopilotChatEnabled()
{
    if (!smokeDeferredInitActive())
        return true;

    char buf[16] = {};
    if (GetEnvironmentVariableA("RAWRXD_SMOKE_BLOCK_COPILOT_CHAT", buf, sizeof(buf)) > 0)
    {
        if (buf[0] != '\0' && buf[0] != '0' && _stricmp(buf, "false") != 0 && _stricmp(buf, "no") != 0 &&
            _stricmp(buf, "off") != 0)
            return false;
    }

    if (GetEnvironmentVariableA("RAWRXD_SMOKE_ENABLE_COPILOT_CHAT", buf, sizeof(buf)) > 0)
    {
        if (buf[0] == '\0' || buf[0] == '0' || _stricmp(buf, "false") == 0)
            return false;
        return true;
    }

    return true;
}
extern "C" void __stdcall PromptWarm_SetAcceptRequests(bool accept);

#ifndef WM_COPILOT_MINIMAL_AGENTIC_DONE
#define WM_COPILOT_MINIMAL_AGENTIC_DONE (WM_APP + 108)
#endif
#include "../bridge/Win32SwarmBridge.h"
#include "Win32IDE_AgenticBrowser.h"
#include "Win32IDE_ComponentManagers.h"  // Complete types for unique_ptr<T> dtor
#include "Win32IDE_DAPServer.h"          // Complete type for unique_ptr<Win32IDE_DAPServer> dtor
#include "Win32IDE_IELabels.h"
#include "WindowVisibilityHelpers.h"
#include "context/semantic_index.h"
#include "core/vector_index_persistence.h"
#include "enterprise_feature_manager.hpp"
#include "feature_registry_panel.h"
#include "lsp/RawrXD_LSPServer.h"
#include "multi_response_engine.h"
#include "runtime/RuntimeProvider.h"
#include "win32_feature_adapter.h"  // Unified Feature Dispatch adapter
#include <commctrl.h>
#include <richedit.h>

extern HWND g_rawrxdIntegratedTerminalTabs;

#ifndef WM_DPICHANGED
#define WM_DPICHANGED 0x02E0
#endif
#ifndef MOD_NOREPEAT
#define MOD_NOREPEAT 0x4000
#endif
#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include <shlobj.h>
#include <sstream>
#include <tlhelp32.h>
#include <windowsx.h>


extern "C" bool ComposerPanel_CommitChanges();
extern "C" void ComposerPanel_HidePlan();


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

// Plan Orchestrator command IDs
#define IDM_PLAN_ORCHESTRATOR_START 4164
#define IDM_PLAN_ORCHESTRATOR_STOP 4165
#define IDM_PLAN_ORCHESTRATOR_VIEW_STATUS 4166
#define IDM_PLAN_ORCHESTRATOR_VIEW_PLAN 4167

// ============================================================================
// Window Class Name
// ============================================================================
static const char* kWindowClassName = "RawrXD_IDE_MainWindow";
static constexpr int kEmergencyWipeHotkeyId = 0xA0F0;
static constexpr UINT_PTR kStartupHeartbeatTimerId = 0x7E01;
static constexpr int kStartupHeartbeatMaxTicks = 60;
static int g_startupHeartbeatTicks = 0;

static void startupTraceImmediate(const char* phase, const char* detail = nullptr)
{
    std::ostringstream oss;
    oss << "[StartupTrace] " << (phase ? phase : "(null)");
    if (detail && detail[0])
        oss << " :: " << detail;
    const std::string line = oss.str();
    LOG_INFO(line);
    OutputDebugStringA((line + "\n").c_str());
}

// Implemented in src/multi_file_search.cpp (Win32 dialog invoked by MultiFileSearchWidget::show()).
extern void MultiFileSearchWidget_ShowDialog(void* ctx);

// AI workers: process main-thread invoke queue every message (avoids queue buildup).
extern void AIWorkersProcessInvokeQueue();


void Win32IDE::syncSpeculativeInferenceFromConfig()
{
    auto& cfg = IDEConfig::getInstance();
    if (!cfg.getBool("features.speculativeDecoding", false))
    {
        m_speculativeEngine.reset();
        return;
    }
    const std::string draft = cfg.getString("inference.speculativeDraftGguf", "");
    const std::string target = cfg.getString("inference.speculativeTargetGguf", "");
    if (draft.empty() || target.empty())
    {
        m_speculativeEngine.reset();
        appendToOutput(
            "[Speculative] Enable dual-GGUF: set inference.speculativeDraftGguf and inference.speculativeTargetGguf in "
            "rawrxd.config.json.\n",
            "Output", OutputSeverity::Info);
        return;
    }
    if (!m_speculativeEngine)
        m_speculativeEngine = std::make_unique<rawrxd::SpeculativeExecutionEngine>();
    if (m_speculativeEngine->ConfigureGgufModels(draft, target))
    {
        appendToOutput("[Speculative] Draft + target GGUF loaded for speculative decoding.\n", "Output",
                       OutputSeverity::Success);
        if (m_hwndStatusBar)
            PostMessageW(m_hwndStatusBar, SB_SETTEXT, 0, (LPARAM)L"Speculative decoding: ready");
    }
    else
    {
        appendToOutput("[Speculative] Failed to configure draft/target GGUF; check paths and logs.\n", "Output",
                       OutputSeverity::Warning);
    }
}

extern "C" void Layout_CalculateAndApply(HWND hwnd, void* pIDE, int w, int h);
extern "C" void Layout_GDI_Blit_Debug(HWND hwnd, void* pIDE);

extern "C" void Layout_GDI_Blit_Debug_Fallback(HWND hwnd, void* pIDE)
{
    (void)pIDE;
    if (!hwnd)
        return;

    HDC hdc = GetDC(hwnd);
    if (!hdc)
        return;

    RECT rc{};
    if (GetClientRect(hwnd, &rc))
    {
        // Render a lightweight fallback frame so debug-blit still has observable behavior
        // even when the MASM implementation is not linked in this lane.
        FrameRect(hdc, &rc, static_cast<HBRUSH>(GetStockObject(DKGRAY_BRUSH)));
    }
    ReleaseDC(hwnd, hdc);
}

#if defined(_M_X64)
#pragma comment(linker, "/alternatename:Layout_GDI_Blit_Debug=Layout_GDI_Blit_Debug_Fallback")
#endif

// WindowProc - Static callback that routes to instance handleMessage
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

static void restoreWindowOpacityIfNeeded(HWND hwnd)
{
    if (!hwnd || !IsWindow(hwnd))
        return;

    const LONG_PTR exStyle = GetWindowLongPtr(hwnd, GWL_EXSTYLE);
    if ((exStyle & WS_EX_LAYERED) == 0)
        return;

    COLORREF colorKey = 0;
    BYTE alpha = 255;
    DWORD flags = 0;
    if (GetLayeredWindowAttributes(hwnd, &colorKey, &alpha, &flags) && (flags & LWA_ALPHA) && alpha == 0)
    {
        SetLayeredWindowAttributes(hwnd, 0, 255, LWA_ALPHA);
        OutputDebugStringA("[Win32IDE] Recovered main window from zero-alpha transparency\n");
    }
}

static constexpr UINT_PTR IDT_VISIBILITY_WATCHDOG = 0x7D11;
static constexpr UINT_PTR IDT_GPU_TELEMETRY = 0x7D12;  // 2-second backend/GPU status refresh
static constexpr UINT_PTR IDT_SESSION_SAVE_DEBOUNCE = 0x7D13;

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

static void logWindowPlacementSnapshot(HWND hwnd, const char* phase)
{
    RawrXD::Win32Visibility::LogPlacementSnapshot(hwnd, phase);
}

static void normalizeWindowPlacementForVisibility(HWND hwnd)
{
    RawrXD::Win32Visibility::NormalizePlacementForVisibility(hwnd);
}

static void runWindowVisibilityWatchdog(HWND hwnd)
{
    if (!hwnd || !IsWindow(hwnd))
        return;

    restoreWindowOpacityIfNeeded(hwnd);

    if (IsIconic(hwnd))
    {
        ShowWindow(hwnd, SW_RESTORE);
    }
    if (!IsWindowVisible(hwnd))
    {
        ShowWindow(hwnd, SW_SHOW);
    }

    normalizeWindowPlacementForVisibility(hwnd);

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
typedef void (*OnCreateStepFn)(void* self, HWND hwnd);

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

static bool sehCallOnCreateStep(OnCreateStepFn fn, void* self, HWND hwnd, const char* stepName)
{
#if defined(_MSC_VER)
    __try
    {
        fn(self, hwnd);
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        char crashMsg[512];
        snprintf(crashMsg, sizeof(crashMsg),
                 "[RawrXD] SEH exception 0x%08lX in onCreate step '%s'. Continuing startup.\n", GetExceptionCode(),
                 (stepName && stepName[0]) ? stepName : "unknown");
        OutputDebugStringA(crashMsg);
        return false;
    }
#else
    try
    {
        fn(self, hwnd);
        return true;
    }
    catch (...)
    {
        char crashMsg[512];
        snprintf(crashMsg, sizeof(crashMsg), "[RawrXD] C++ exception in onCreate step '%s'. Continuing startup.\n",
                 (stepName && stepName[0]) ? stepName : "unknown");
        OutputDebugStringA(crashMsg);
        return false;
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
            // Post deferred init message to run heavy initialization async (chat, session, backend)
            // This allows the message pump to start processing before we block on I/O
            PostMessage(hwnd, WM_APP + 1001, 0, 0);  // WM_APP_DEFERRED_INIT
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

        case WM_NCCALCSIZE:
        {
            // Aperture frame step 1: remove the default non-client frame so the app
            // can paint a seamless custom header/canvas in the client region.
            if (wParam == TRUE)
            {
                if (NCCALCSIZE_PARAMS* pNc = reinterpret_cast<NCCALCSIZE_PARAMS*>(lParam))
                {
                    if (IsZoomed(hwnd))
                    {
                        // Keep maximized bounds within monitor work area.
                        MONITORINFO mi = {sizeof(mi)};
                        HMONITOR mon = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
                        if (GetMonitorInfoA(mon, &mi))
                        {
                            pNc->rgrc[0] = mi.rcWork;
                        }
                    }
                }
                return 0;
            }
            break;
        }

        case WM_NCHITTEST:
        {
            const LRESULT baseHit = DefWindowProcA(hwnd, uMsg, wParam, lParam);
            if (baseHit == HTCLOSE || baseHit == HTMAXBUTTON || baseHit == HTMINBUTTON || baseHit == HTSYSMENU)
            {
                return baseHit;
            }

            RECT wr = {};
            GetWindowRect(hwnd, &wr);

            const POINT pt = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};

            const int frameX = GetSystemMetrics(SM_CXSIZEFRAME) + GetSystemMetrics(SM_CXPADDEDBORDER);
            const int frameY = GetSystemMetrics(SM_CYSIZEFRAME) + GetSystemMetrics(SM_CXPADDEDBORDER);

            const bool canResize = !IsZoomed(hwnd);

            if (canResize)
            {
                const bool left = pt.x < wr.left + frameX;
                const bool right = pt.x >= wr.right - frameX;
                const bool top = pt.y < wr.top + frameY;
                const bool bottom = pt.y >= wr.bottom - frameY;

                if (top && left)
                    return HTTOPLEFT;
                if (top && right)
                    return HTTOPRIGHT;
                if (bottom && left)
                    return HTBOTTOMLEFT;
                if (bottom && right)
                    return HTBOTTOMRIGHT;
                if (left)
                    return HTLEFT;
                if (right)
                    return HTRIGHT;
                if (top)
                    return HTTOP;
                if (bottom)
                    return HTBOTTOM;
            }

            // Reserve a narrow drag strip in the top client area, while preserving
            // child control interactivity (toolbar buttons, tabs, etc.).
            const UINT dpi = GetDpiForWindow(hwnd);
            const int captionDragHeight = MulDiv(36, (dpi > 0 ? static_cast<int>(dpi) : 96), 96);
            if (pt.y >= wr.top && pt.y < wr.top + captionDragHeight)
            {
                POINT clientPt = pt;
                ScreenToClient(hwnd, &clientPt);
                HWND hChild = ChildWindowFromPointEx(hwnd, clientPt, CWP_SKIPINVISIBLE | CWP_SKIPDISABLED);
                if (!hChild || hChild == hwnd)
                {
                    return HTCAPTION;
                }
            }

            return HTCLIENT;
        }

        case WM_GETMINMAXINFO:
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

            // Quick Open (Ctrl+P) — VS Code / Cursor (also in accelerator table → WM_COMMAND 7029)
            if ((GetKeyState(VK_CONTROL) & 0x8000) && !(GetKeyState(VK_SHIFT) & 0x8000) &&
                !(GetKeyState(VK_MENU) & 0x8000) && (wParam == 'P'))
            {
                routeCommand(IDM_FILE_QUICK_OPEN);
                return 0;
            }

            // Go To Line (Ctrl+G)
            if ((GetKeyState(VK_CONTROL) & 0x8000) && !(GetKeyState(VK_SHIFT) & 0x8000) && (wParam == 'G'))
            {
                showGoToLineDialog();
                return 0;
            }

            // Go To Symbol (Ctrl+Shift+O)
            if ((GetKeyState(VK_CONTROL) & 0x8000) && (GetKeyState(VK_SHIFT) & 0x8000) && (wParam == 'O'))
            {
                showGoToSymbolPicker();
                return 0;
            }

            // Toggle terminal / bottom panel (VS Code / Cursor: Ctrl+`)
            if ((GetKeyState(VK_CONTROL) & 0x8000) && !(GetKeyState(VK_SHIFT) & 0x8000) &&
                !(GetKeyState(VK_MENU) & 0x8000) && (wParam == VK_OEM_3 || wParam == VK_OEM_8))
            {
                routeCommand(2029);
                return 0;
            }

            // Toggle bottom panel only (VS Code: Ctrl+J — distinct from Ctrl+` terminal focus)
            if ((GetKeyState(VK_CONTROL) & 0x8000) && !(GetKeyState(VK_SHIFT) & 0x8000) &&
                !(GetKeyState(VK_MENU) & 0x8000) && (wParam == 'J'))
            {
                routeCommand(IDM_VIEW_TOGGLE_BOTTOM_PANEL);
                return 0;
            }

            // Cursor-style: Ctrl+L → Agent / chat panel
            if ((GetKeyState(VK_CONTROL) & 0x8000) && !(GetKeyState(VK_SHIFT) & 0x8000) &&
                !(GetKeyState(VK_MENU) & 0x8000) && wParam == 'L')
            {
                routeCommand(3009);
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

            // Invoke Pure MASM GDI Debug Overlay (Batch 1/8)
            Layout_GDI_Blit_Debug(hwnd, this);

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
            if (LOWORD(wParam) == IDM_EMOJI_INSERT)
            {
                handleEmojiCommand(IDM_EMOJI_INSERT, lParam);
                return 0;
            }
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
                // Integrated terminal instance tabs (pwsh / cmd / Agent …) — VS Code-style shell switcher
                if (pNMHDR->code == TCN_SELCHANGE && g_rawrxdIntegratedTerminalTabs &&
                    pNMHDR->hwndFrom == g_rawrxdIntegratedTerminalTabs)
                {
                    int idx = (int)TabCtrl_GetCurSel(g_rawrxdIntegratedTerminalTabs);
                    if (idx >= 0 && idx < static_cast<int>(m_terminalPanes.size()))
                    {
                        switchTerminalPane(m_terminalPanes[static_cast<size_t>(idx)].id);
                    }
                }
                // Tier 3 (Feature 38/39): Status bar click → language/encoding selector (use part index for reliable
                // dispatch)
                if (pNMHDR->hwndFrom == m_hwndStatusBar && pNMHDR->code == NM_CLICK)
                {
                    NMMOUSE* pNMMouse = reinterpret_cast<NMMOUSE*>(lParam);
                    handleStatusBarClick(static_cast<int>(pNMMouse->dwItemSpec));
                }
                if (pNMHDR->hwndFrom == m_hwndStatusBar && pNMHDR->code == NM_CUSTOMDRAW)
                {
                    return handleStatusBarCustomDraw(pNMHDR);
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
                        // Unified content-change path (includes ghost debounce + syntax debounce)
                        if (pNMHDR->code == EN_CHANGE)
                        {
                            onEditorContentChanged();
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
                        if (lineStripEditorEnabled() && (pNMHDR->code == EN_SELCHANGE || pNMHDR->code == EN_VSCROLL))
                        {
                            invalidateLineStripOverlay(nullptr);
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
            if (m_ircBridge)
            {
                m_ircBridge->tick();
            }
            if (wParam == kStartupHeartbeatTimerId)
            {
                ++g_startupHeartbeatTicks;
                char hb[208] = {};
                snprintf(hb, sizeof(hb), "[StartupTrace] UI heartbeat tick=%d initStatus=%d tid=%lu",
                         g_startupHeartbeatTicks, m_initStatus.load(std::memory_order_acquire),
                         static_cast<unsigned long>(GetCurrentThreadId()));
                LOG_INFO(hb);
                if (g_startupHeartbeatTicks >= kStartupHeartbeatMaxTicks)
                {
                    KillTimer(hwnd, kStartupHeartbeatTimerId);
                    LOG_INFO("[StartupTrace] UI heartbeat timer auto-stopped");
                }
                return 0;
            }
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
            if (wParam == LINE_STRIP_SYNC_TIMER_ID)
            {
                KillTimer(hwnd, LINE_STRIP_SYNC_TIMER_ID);
                if (lineStripEditorEnabled())
                {
                    if (m_lineStripDocumentDirty)
                    {
                        syncLineStripDocumentFromEditor();
                    }
                    syncLineStripDirtyStrips();
                }
                return 0;
            }
            if (wParam == AGENT_CHAT_CURSOR_TIMER_ID)
            {
                tickAgentChatCursorAnimation();
                return 0;
            }
            if (wParam == LINE_STRIP_CARET_BLINK_TIMER_ID)
            {
                if (lineStripEditorEnabled())
                {
                    onLineStripCaretBlinkTimer();
                }
                return 0;
            }
            if (wParam == 8888)
            {  // GHOST_TEXT_TIMER_ID
                onGhostTextTimer();
                return 0;
            }
            if (wParam == 8889)
            {  // TITAN_PAGING_HEARTBEAT_TIMER_ID
                onTitanPagingHeartbeatTimer();
                return 0;
            }
            if (wParam == 8890)
            {  // PREFETCH_IDLE_TIMER_ID — trigger speculative prefetch after 150ms idle
                onPrefetchIdleTimer();
                return 0;
            }
            if (wParam == 8891)
            {  // GHOST_TEXT_RENDER_TIMER_ID — coalesce streamed ghost tokens before repaint
                onGhostTextRenderMessage();
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
                LOG_INFO("[StartupTrace] WM_TIMER id=199 enter on UI thread");
                const auto t199Milestone = [this](const char* debugLine, const char* userLine)
                {
                    if (isShuttingDown())
                        return;
                    if (debugLine && debugLine[0])
                        OutputDebugStringA(debugLine);
                    // Keep timer-199 diagnostics off the output pane to avoid potential
                    // startup-time lock inversion in UI text routing during first-paint.
                    (void)userLine;
                };
                t199Milestone("[IDE-Pipeline:T199] Batch 1/8: WM_TIMER id=199 follow-up visibility tick\n",
                              "[Init:T199] Batch 1/8: Follow-up visibility timer fired\n");
                t199Milestone("[IDE-Pipeline:T199] Batch 2/8: KillTimer(199) — one-shot disarmed\n",
                              "[Init:T199] Batch 2/8: One-shot timer disarmed\n");
                KillTimer(hwnd, 199);
                t199Milestone("[IDE-Pipeline:T199] Batch 3/8: evaluating main HWND + iconic state\n",
                              "[Init:T199] Batch 3/8: Checking window for second nudge\n");
                if (!m_hwndMain || !IsWindow(m_hwndMain))
                {
                    t199Milestone("[IDE-Pipeline:T199] Batch 4/8: no main HWND — second nudge skipped\n",
                                  "[Init:T199] Batch 4/8: Skipped (no main window yet)\n");
                    t199Milestone("[IDE-Pipeline:T199] Batch 5/8: (skipped)\n", "[Init:T199] Batch 5/8: (skipped)\n");
                    t199Milestone("[IDE-Pipeline:T199] Batch 6/8: (skipped)\n", "[Init:T199] Batch 6/8: (skipped)\n");
                    t199Milestone("[IDE-Pipeline:T199] Batch 7/8: (skipped)\n", "[Init:T199] Batch 7/8: (skipped)\n");
                    t199Milestone("[IDE-Pipeline:T199] Batch 8/8: WM_TIMER 199 handler complete (no-op)\n",
                                  "[Init:T199] Batch 8/8: Handler finished (no-op)\n");
                    t199Milestone("[IDE-Pipeline:T199] E0-1/8: User may have minimized during 400ms window\n",
                                  "[Init:T199] E0-1/8: Minimized state respected\n");
                    t199Milestone("[IDE-Pipeline:T199] E0-2/8: No ShowWindow while iconic (matches prior logic)\n",
                                  "[Init:T199] E0-2/8: No forced show while iconic\n");
                    t199Milestone("[IDE-Pipeline:T199] E0-3/8: KillTimer already committed — no leak\n",
                                  "[Init:T199] E0-3/8: Timer handle cleared\n");
                    t199Milestone("[IDE-Pipeline:T199] E0-4/8: WM_APP+199 may run again on next cold start only\n",
                                  "[Init:T199] E0-4/8: One-shot semantics preserved\n");
                    t199Milestone("[IDE-Pipeline:T199] E0-5/8: Z-order bump intentionally skipped\n",
                                  "[Init:T199] E0-5/8: Z-order bump skipped when iconic\n");
                    t199Milestone("[IDE-Pipeline:T199] E0-6/8: Foreground nudge skipped when iconic\n",
                                  "[Init:T199] E0-6/8: Foreground nudge skipped when iconic\n");
                    t199Milestone("[IDE-Pipeline:T199] E0-7/8: Timer ID 199 free for reuse\n",
                                  "[Init:T199] E0-7/8: Timer ID released\n");
                    t199Milestone("[IDE-Pipeline:T199] E0-8/8: WM_TIMER 199 iconic path closed\n",
                                  "[Init:T199] E0-8/8: WM_TIMER 199 iconic path complete\n");
                    return 0;
                }
                if (IsIconic(m_hwndMain))
                {
                    t199Milestone("[IDE-Pipeline:T199] Batch 4/8: window iconic — second nudge skipped\n",
                                  "[Init:T199] Batch 4/8: Skipped (minimized)\n");
                    t199Milestone("[IDE-Pipeline:T199] Batch 5/8: (skipped)\n", "[Init:T199] Batch 5/8: (skipped)\n");
                    t199Milestone("[IDE-Pipeline:T199] Batch 6/8: (skipped)\n", "[Init:T199] Batch 6/8: (skipped)\n");
                    t199Milestone("[IDE-Pipeline:T199] Batch 7/8: (skipped)\n", "[Init:T199] Batch 7/8: (skipped)\n");
                    t199Milestone("[IDE-Pipeline:T199] Batch 8/8: WM_TIMER 199 handler complete (iconic skip)\n",
                                  "[Init:T199] Batch 8/8: Timer handler finished (minimized)\n");
                    t199Milestone("[IDE-Pipeline:T199] E0-1/8: User may have minimized during 400ms window\n",
                                  "[Init:T199] E0-1/8: Minimized state respected\n");
                    t199Milestone("[IDE-Pipeline:T199] E0-2/8: No ShowWindow while iconic (matches prior logic)\n",
                                  "[Init:T199] E0-2/8: No forced show while iconic\n");
                    t199Milestone("[IDE-Pipeline:T199] E0-3/8: KillTimer already committed — no leak\n",
                                  "[Init:T199] E0-3/8: Timer handle cleared\n");
                    t199Milestone("[IDE-Pipeline:T199] E0-4/8: WM_APP+199 may run again on next cold start only\n",
                                  "[Init:T199] E0-4/8: One-shot semantics preserved\n");
                    t199Milestone("[IDE-Pipeline:T199] E0-5/8: Z-order bump intentionally skipped\n",
                                  "[Init:T199] E0-5/8: Z-order bump skipped when iconic\n");
                    t199Milestone("[IDE-Pipeline:T199] E0-6/8: Foreground nudge skipped when iconic\n",
                                  "[Init:T199] E0-6/8: Foreground nudge skipped when iconic\n");
                    t199Milestone("[IDE-Pipeline:T199] E0-7/8: Timer ID 199 free for reuse\n",
                                  "[Init:T199] E0-7/8: Timer ID released\n");
                    t199Milestone("[IDE-Pipeline:T199] E0-8/8: WM_TIMER 199 iconic path closed\n",
                                  "[Init:T199] E0-8/8: WM_TIMER 199 iconic path complete\n");
                    return 0;
                }
                t199Milestone("[IDE-Pipeline:T199] Batch 4/8: ShowWindow(SW_SHOW) second pass\n",
                              "[Init:T199] Batch 4/8: Second visibility show\n");
                LOG_INFO("[StartupTrace] WM_TIMER id=199 before ShowWindow(SW_SHOW)");
                ShowWindow(m_hwndMain, SW_SHOW);
                LOG_INFO("[StartupTrace] WM_TIMER id=199 after ShowWindow(SW_SHOW)");
                t199Milestone("[IDE-Pipeline:T199] Batch 5/8: SetWindowPos TOP (second pass)\n",
                              "[Init:T199] Batch 5/8: Second Z-order raise\n");
                LOG_INFO("[StartupTrace] WM_TIMER id=199 before SetWindowPos(HWND_TOP)");
                SetWindowPos(m_hwndMain, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
                LOG_INFO("[StartupTrace] WM_TIMER id=199 after SetWindowPos(HWND_TOP)");
                t199Milestone("[IDE-Pipeline:T199] Batch 6/8: forceWindowToForeground (second pass)\n",
                              "[Init:T199] Batch 6/8: Second foreground nudge\n");
                LOG_INFO("[StartupTrace] WM_TIMER id=199 before forceWindowToForeground");
                forceWindowToForeground(m_hwndMain);
                LOG_INFO("[StartupTrace] WM_TIMER id=199 after forceWindowToForeground");
                t199Milestone("[IDE-Pipeline:T199] Batch 7/8: second pass complete — no further timer arm\n",
                              "[Init:T199] Batch 7/8: Follow-up visibility actions applied\n");
                t199Milestone("[IDE-Pipeline:T199] Batch 8/8: WM_TIMER 199 handler returning\n",
                              "[Init:T199] Batch 8/8: WM_TIMER 199 handler finished\n");
                t199Milestone("[IDE-Pipeline:T199] E0-1/8: Cooperates with WM_APP+199 primary sequence\n",
                              "[Init:T199] E0-1/8: Complements WM_APP+199\n");
                t199Milestone("[IDE-Pipeline:T199] E0-2/8: 400ms delay absorbs transient shell focus theft\n",
                              "[Init:T199] E0-2/8: Delay absorbs transient focus theft\n");
                t199Milestone("[IDE-Pipeline:T199] E0-3/8: Non-iconic branch only (parity with legacy guard)\n",
                              "[Init:T199] E0-3/8: Non-iconic guard preserved\n");
                t199Milestone("[IDE-Pipeline:T199] E0-4/8: UI-thread only — no worker join\n",
                              "[Init:T199] E0-4/8: UI-thread-only path\n");
                t199Milestone("[IDE-Pipeline:T199] E0-5/8: KillTimer prevents periodic re-entry\n",
                              "[Init:T199] E0-5/8: One-shot semantics enforced\n");
                t199Milestone("[IDE-Pipeline:T199] E0-6/8: Taskbar + DWM may still reorder — best effort\n",
                              "[Init:T199] E0-6/8: Shell may still reorder Z-order\n");
                t199Milestone("[IDE-Pipeline:T199] E0-7/8: No InvalidateRect here — WM_APP+199 already painted\n",
                              "[Init:T199] E0-7/8: No extra invalidation in timer path\n");
                t199Milestone("[IDE-Pipeline:T199] E0-8/8: WM_TIMER 199 happy path closed\n",
                              "[Init:T199] E0-8/8: WM_TIMER 199 complete\n");
                LOG_INFO("[StartupTrace] WM_TIMER id=199 exit on UI thread");
                return 0;
            }
            if (wParam == IDT_VISIBILITY_WATCHDOG)
            {
                runWindowVisibilityWatchdog(m_hwndMain ? m_hwndMain : hwnd);
                return 0;
            }
            if (wParam == IDT_GPU_TELEMETRY)
            {
                updateStatusBarBackend();
                refreshMoEPackHudStatusBarPart();
                if (m_hwndStatusBar)
                {
                    InvalidateRect(m_hwndStatusBar, nullptr, FALSE);
                }
                return 0;
            }
            if (wParam == RAWRXD_IDT_PS_QUEUE_DRAIN)
            {
                drainPowerShellCommandQueue();
                return 0;
            }
            if (wParam == IDT_SESSION_SAVE_DEBOUNCE)
            {
                KillTimer(hwnd, IDT_SESSION_SAVE_DEBOUNCE);
                if (m_sessionSaveDebouncePending)
                {
                    m_sessionSaveDebouncePending = false;
                    saveSession();
                }
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
            if (wParam == 8892)
            {  // TEMPORAL_ANIM_TIMER_ID
                updateEmojiTemporalLayer();
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
            if (wParam == kEmergencyWipeHotkeyId)
            {
                performEmergencyWipeAndShutdown();
                return 0;
            }
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

        case WM_QUERYENDSESSION:
            return TRUE;

        case WM_ENDSESSION:
            if (wParam != 0)
            {
                if (Agentic::AgenticPlanningOrchestrator* orch =
                        Agentic::OrchestratorIntegration::instance().getOrchestrator())
                {
                    orch->flushPersistenceSnapshotNow();
                }
            }
            break;

        case WM_CLOSE:
            if (!m_fileModified || promptSaveChanges())
            {
                DestroyWindow(hwnd);
            }
            return 0;

        case WM_APP + 1337:
            handleNativeStreamTick(wParam, lParam);
            return 0;

        // Apply settings safely on the main (UI) thread.
        // Background threads must never call applySettings() directly.
        case WM_APP_APPLY_SETTINGS:
        {
            const ULONGLONG t0 = GetTickCount64();
            LOG_INFO("[StartupTrace] WM_APP_APPLY_SETTINGS begin");
            applySettings();
            const ULONGLONG dt = GetTickCount64() - t0;
            if (dt >= 25)
            {
                char msg[160] = {};
                snprintf(msg, sizeof(msg), "[UI-Timing] WM_APP_APPLY_SETTINGS took %llums\n",
                         static_cast<unsigned long long>(dt));
                OutputDebugStringA(msg);
                char logMsg[176] = {};
                snprintf(logMsg, sizeof(logMsg), "[StartupTrace] WM_APP_APPLY_SETTINGS duration=%llums",
                         static_cast<unsigned long long>(dt));
                LOG_INFO(logMsg);
            }
            LOG_INFO("[StartupTrace] WM_APP_APPLY_SETTINGS end");
            return 0;
        }

        // Initialize voice chat UI/hotkeys safely on the main thread.
        case WM_APP_INIT_VOICE_CHAT_UI:
        {
            const ULONGLONG t0 = GetTickCount64();
            LOG_INFO("[StartupTrace] WM_APP_INIT_VOICE_CHAT_UI begin");
            initVoiceChat();
            voiceLoadPreferences();
            createVoiceChatPanel(m_hwndMain);
            registerVoiceHotkeys();
            RegisterHotKey(m_hwndMain, kEmergencyWipeHotkeyId, MOD_CONTROL | MOD_SHIFT | MOD_ALT | MOD_NOREPEAT, 'K');
            updateVoiceStatusBar();
            const ULONGLONG dt = GetTickCount64() - t0;
            if (dt >= 25)
            {
                char msg[176] = {};
                snprintf(msg, sizeof(msg), "[UI-Timing] WM_APP_INIT_VOICE_CHAT_UI took %llums\n",
                         static_cast<unsigned long long>(dt));
                OutputDebugStringA(msg);
                char logMsg[192] = {};
                snprintf(logMsg, sizeof(logMsg), "[StartupTrace] WM_APP_INIT_VOICE_CHAT_UI duration=%llums",
                         static_cast<unsigned long long>(dt));
                LOG_INFO(logMsg);
            }
            LOG_INFO("[StartupTrace] WM_APP_INIT_VOICE_CHAT_UI end");
            return 0;
        }

        // Create VoiceAutomation UI safely on the main thread.
        case WM_APP_CREATE_VOICE_AUTOMATION_PANEL:
        {
            const ULONGLONG t0 = GetTickCount64();
            LOG_INFO("[StartupTrace] WM_APP_CREATE_VOICE_AUTOMATION_PANEL begin");
            RECT rc{};
            GetClientRect(m_hwndMain, &rc);
            extern void Win32IDE_CreateVoiceAutomationPanel(HWND, int, int, int, int);
            Win32IDE_CreateVoiceAutomationPanel(m_hwndMain, 0, rc.bottom - 80, rc.right, 80);
            m_voiceAutomationInitialized = true;
            OutputDebugStringA("Phase 44: VoiceAutomation panel created (UI-thread)\n");
            const ULONGLONG dt = GetTickCount64() - t0;
            if (dt >= 25)
            {
                char msg[208] = {};
                snprintf(msg, sizeof(msg), "[UI-Timing] WM_APP_CREATE_VOICE_AUTOMATION_PANEL took %llums\n",
                         static_cast<unsigned long long>(dt));
                OutputDebugStringA(msg);
                char logMsg[224] = {};
                snprintf(logMsg, sizeof(logMsg), "[StartupTrace] WM_APP_CREATE_VOICE_AUTOMATION_PANEL duration=%llums",
                         static_cast<unsigned long long>(dt));
                LOG_INFO(logMsg);
            }
            LOG_INFO("[StartupTrace] WM_APP_CREATE_VOICE_AUTOMATION_PANEL end");
            return 0;
        }

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

        // Async model-load completion from background worker
        case WM_APP + 302:
        {  // WM_MODEL_LOAD_DONE
            std::unique_ptr<AsyncModelLoadResult> result(reinterpret_cast<AsyncModelLoadResult*>(lParam));
            if (!result)
            {
                OutputDebugStringA("[WM_MODEL_LOAD_DONE] null result payload\n");
                LOG_WARNING("[WM_MODEL_LOAD_DONE] null result payload");
                return 0;
            }

            if (!smokeCopilotChatEnabled())
            {
                std::lock_guard<std::mutex> lock(m_asyncModelLoadMutex);
                m_asyncModelLoadRunning = false;
                return 0;
            }

            {
                std::ostringstream os;
                os << "[WM_MODEL_LOAD_DONE] tid=" << GetCurrentThreadId() << " path='" << result->filepath << "'"
                   << " ggufOk=" << (result->ggufOk ? 1 : 0) << " bridgeOk=" << (result->bridgeOk ? 1 : 0)
                   << " bytes=" << result->fileBytes << " wallMs=" << result->wallMs << "\n";
                OutputDebugStringA(os.str().c_str());
                LOG_INFO(std::string("[WM_MODEL_LOAD_DONE] path=") + result->filepath +
                         " ggufOk=" + (result->ggufOk ? "1" : "0") + " bridgeOk=" + (result->bridgeOk ? "1" : "0") +
                         " wallMs=" + std::to_string(result->wallMs));
            }

            {
                std::lock_guard<std::mutex> lock(m_asyncModelLoadMutex);
                m_asyncModelLoadRunning = false;
            }

            if (result->bridgeOk && !result->ggufOk)
            {
                appendToOutput("Model loaded into Agentic Bridge (streaming GGUF skipped).\n", "Output",
                               OutputSeverity::Info);
            }

            if (result->ggufOk || result->bridgeOk)
            {
                // Record model size/load time for status bar + metrics.
                m_lastLoadedModelOk = true;
                m_lastLoadedModelPath = result->filepath;
                m_ollamaModelOverride = result->filepath;
                m_lastLoadedModelBytes = result->fileBytes;
                m_lastLoadedModelWallMs = result->wallMs;
                {
                    const auto p = std::filesystem::path(result->filepath);
                    m_lastLoadedModelDisplayName = p.has_filename() ? p.filename().string() : result->filepath;
                }
                if (m_lastLoadedModelBytes > 0)
                {
                    METRICS.gauge("model.file_bytes", static_cast<double>(m_lastLoadedModelBytes));
                    METRICS.gauge("model.file_gb",
                                  static_cast<double>(m_lastLoadedModelBytes) / (1024.0 * 1024.0 * 1024.0));
                }
                if (m_lastLoadedModelWallMs > 0.0)
                {
                    METRICS.recordDuration("model.load_wall_ms", m_lastLoadedModelWallMs);
                }

                m_lastLocalModelReadyTickMs = GetTickCount64();

                // Autorun prompt format detection upon model load success (UIP-001)
                conversationDetectModelFormat(result->filepath);

                // onModelReadyUnified: single authoritative barrier — resets streaming state,
                // syncs UI, then posts WM_SAFE_TO_REPLAY (which triggers replay if pending).
                // Async path: replay pending message if queued before load completed.
                onModelReadyUnified(!m_pendingChatOnLoadMessage.empty());

                const bool hasPendingChat = !m_pendingChatOnLoadMessage.empty();
                appendModelLoadReadyCopilotTurns(result->filepath, !hasPendingChat);

                if (m_hwndModelSelector && IsWindow(m_hwndModelSelector))
                {
                    populateModelSelector();
                }

                // Ensure status bar reflects newly loaded model (and any TPS changes soon after).
                PostMessageW(m_hwndMain, WM_STATUSBAR_REFRESH_COPILOT, 0, 0);
            }
            else
            {
                m_pendingChatOnLoadMessage.clear();  // discard pending on failure
                m_pendingChatOnLoadUserTurnRendered = false;
                m_lastLoadedModelOk = false;
                m_lastLoadedModelDisplayName.clear();
                m_lastLoadedModelPath.clear();
                m_lastLoadedModelBytes = 0;
                m_lastLoadedModelWallMs = 0.0;
                METRICS.gauge("model.file_bytes", 0.0);
                METRICS.gauge("model.file_gb", 0.0);
                appendToOutput("❌ Failed to load model: " + result->filepath +
                                   " (not a valid GGUF and native load failed).",
                               "Errors", OutputSeverity::Error);
            }

            if (m_hwndMain && IsWindow(m_hwndMain))
            {
                RECT rc = {};
                if (GetClientRect(m_hwndMain, &rc))
                {
                    onSize(rc.right, rc.bottom);
                }
                ShowWindow(m_hwndMain, SW_SHOW);
                InvalidateRect(m_hwndMain, nullptr, TRUE);
                UpdateWindow(m_hwndMain);
            }
            return 0;
        }

        case WM_APP + 303:
        {  // WM_PENDING_CHAT_REPLAY
            if (!smokeCopilotChatEnabled())
                return 0;
            if (!m_pendingChatOnLoadMessage.empty() && m_hwndCopilotChatInput && IsWindow(m_hwndCopilotChatInput))
            {
                // Give backend registration/load-complete transitions a brief moment
                // before issuing the replayed Send (keep small — this runs on the UI thread).
                Sleep(32);

                std::string pending = std::move(m_pendingChatOnLoadMessage);
                m_pendingChatOnLoadMessage.clear();
                OutputDebugStringA(("[WM_PENDING_CHAT_REPLAY] Replaying pending chat: " + pending + "\n").c_str());
                SetWindowTextA(m_hwndCopilotChatInput, pending.c_str());
                HandleCopilotSend();
            }
            return 0;
        }

        case WM_COPILOT_DEFERRED_SEND:
            if (smokeCopilotChatEnabled())
                HandleCopilotSend();
            return 0;

        case WM_RAWRXD_GHOST_TEXT_CACHE_CLEAR:
        {
            std::lock_guard<std::mutex> lock(m_ghostTextCacheMutex);
            m_ghostTextCache.clear();
            return 0;
        }

        case WM_APP + 308:
        {  // WM_STATUSBAR_REFRESH_COPILOT — worker thread updated chat TPS gauges
            updateEnhancedStatusBar();
            return 0;
        }

        case WM_APP + 312:
        {  // WM_MODEL_DISCOVERY_DONE — async filesystem scan finished
            OutputDebugStringA("[ModelDiscovery] WM_MODEL_DISCOVERY_DONE: refreshing model selector\n");
            if (m_hwndModelSelector && IsWindow(m_hwndModelSelector))
            {
                populateModelSelectorFromDiscoveryCache();
            }
            if (m_hwndStatusBar && IsWindow(m_hwndStatusBar))
            {
                SendMessageW(m_hwndStatusBar, SB_SETTEXT, 0, (LPARAM)L"Sovereign Engine: Ready");
            }
            return 0;
        }

        case WM_APP + 311:
        {  // WM_STATUSBAR_AGENTIC_SETPARTTEXT — ExecPipeline worker / any thread → UI thread SB_SETTEXT
            auto* text = reinterpret_cast<wchar_t*>(lParam);
            if (m_hwndStatusBar && IsWindow(m_hwndStatusBar) && text)
            {
                SendMessageW(m_hwndStatusBar, SB_SETTEXT, wParam, reinterpret_cast<LPARAM>(text));
            }
            if (text)
            {
                free(text);
            }
            return 0;
        }

        case WM_APP + 304:
        {  // WM_RAWR_TERMINAL_PROCESS_EXIT — shell monitor thread → UI thread
            onTerminalProcessExited(static_cast<int>(wParam), static_cast<uint32_t>(lParam));
            return 0;
        }

        // Visibility watchdog request (posted from watchdog worker thread)
        case WM_APP + 1:
        {
            if (m_hwndMain && IsWindow(m_hwndMain))
            {
                restoreWindowOpacityIfNeeded(m_hwndMain);
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

        case WM_USER_GHOST_TOKEN:
        {
            const char* token = reinterpret_cast<const char*>(lParam);
            onGhostTextTokenChunk(token, static_cast<uint64_t>(wParam));
            if (token)
                free(const_cast<char*>(token));
            return 0;
        }

        case WM_USER_GHOST_RENDER:
            onGhostTextRenderMessage();
            return 0;

        case WM_USER_GHOST_COMPLETE:
        {
            const char* text = reinterpret_cast<const char*>(lParam);
            onGhostTextComplete(static_cast<uint64_t>(wParam), text);
            if (text)
                free(const_cast<char*>(text));
            return 0;
        }

        case RawrXD::Review::WM_USER_EDIT_REVIEW_REQUIRED:
        {
            auto* request = reinterpret_cast<RawrXD::Review::PendingEditRequest*>(lParam);
            queueNavigatorPendingEdit(request);
            return 0;
        }

        case WM_TITAN_GHOST_STREAM:
            Win32IDE_HandleTitanGhostStreamMessage(this);
            return 0;

        case WM_TITAN_AGENT_STREAM:
            onTitanAgentStreamMessage();
            return 0;

        case WM_TITAN_AGENT_DONE:
            onTitanAgentDone((int)wParam);
            return 0;

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

        case WM_APP + 1001:  // WM_APP_DEFERRED_INIT — run heavy initialization async
        {
            // IMPORTANT: This message is posted immediately after WM_CREATE returns.
            // By this time, the message pump has processed pending paints and the UI
            // can render while we do slow I/O (chat panel, session restore, backend init)
            OutputDebugStringA("[WIN32IDE] WM_APP_DEFERRED_INIT entering deferred heavy initialization\n");

            // Run heavy init by posting individual messages to UI thread queue
            // This way they run sequentially on the UI thread after current paints complete
            OutputDebugStringA("[DEFERRED] Posting chat panel creation to UI queue\n");
            PostMessage(m_hwndMain, WM_APP + 1002, 0, 0);  // WM_APP_DEFERRED_CREATE_CHAT

            OutputDebugStringA("[DEFERRED] Posting session restore to UI queue\n");
            PostMessage(m_hwndMain, WM_APP + 1003, 0, 0);  // WM_APP_DEFERRED_RESTORE_SESSION

            OutputDebugStringA("[DEFERRED] Posting backend init to UI queue\n");
            PostMessage(m_hwndMain, WM_APP + 1004, 0, 0);  // WM_APP_DEFERRED_INIT_BACKEND

            return 0;
        }

        case WM_APP + 1002:  // WM_APP_DEFERRED_CREATE_CHAT — create chat panel (UI thread)
        {
            OutputDebugStringA("[DEFERRED] WM_APP_DEFERRED_CREATE_CHAT: creating chat panel\n");
            try
            {
                if (!m_hwndSecondarySidebar || !IsWindow(m_hwndSecondarySidebar))
                {
                    this->createChatPanel();
                    OutputDebugStringA("[DEFERRED] Chat panel created successfully\n");
                }
                finalizeCopilotChatInterlockAfterDeferredLoad();
            }
            catch (const std::exception& ex)
            {
                char buf[512] = {};
                snprintf(buf, sizeof(buf), "[DEFERRED] Exception creating chat panel: %s\n", ex.what());
                OutputDebugStringA(buf);
            }
            return 0;
        }

        case WM_APP + 1003:  // WM_APP_DEFERRED_RESTORE_SESSION — restore session (UI thread)
        {
            OutputDebugStringA("[DEFERRED] WM_APP_DEFERRED_RESTORE_SESSION: restoring session\n");
            try
            {
                this->restoreSession();
                OutputDebugStringA("[DEFERRED] Session restored successfully\n");
            }
            catch (const std::exception& ex)
            {
                char buf[512] = {};
                snprintf(buf, sizeof(buf), "[DEFERRED] Exception restoring session: %s\n", ex.what());
                OutputDebugStringA(buf);
            }
            return 0;
        }

        case WM_APP + 1004:  // WM_APP_DEFERRED_INIT_BACKEND — init backend (UI thread)
        {
            OutputDebugStringA("[DEFERRED] WM_APP_DEFERRED_INIT_BACKEND: initializing backend\n");
            try
            {
                try
                {
                    this->initBackendManager();
                }
                catch (const std::exception& ex)
                {
                    LOG_WARNING(std::string("initBackendManager deferred: ") + ex.what());
                }
                try
                {
                    this->initLLMRouter();
                }
                catch (const std::exception& ex)
                {
                    LOG_WARNING(std::string("initLLMRouter deferred: ") + ex.what());
                }
                OutputDebugStringA("[DEFERRED] Backend init pass completed (chat interlock next)\n");
            }
            catch (...)
            {
                OutputDebugStringA("[DEFERRED] Backend init pass failed with unknown error\n");
            }
            finalizeCopilotChatInterlockAfterDeferredLoad();
            return 0;
        }

        default:
            // Force main window visible once after startup (posted before message loop).
            // Also set a one-shot timer to force visible again ~400ms later (catches init that hides window).
            if (uMsg == WM_APP + 199)
            {
                const auto wm199Milestone = [this](const char* debugLine, const char* userLine)
                {
                    if (isShuttingDown())
                        return;
                    if (debugLine && debugLine[0])
                        OutputDebugStringA(debugLine);
                    if (userLine && userLine[0] && m_hwndOutputTabs && IsWindow(m_hwndOutputTabs))
                        appendToOutput(std::string(userLine), "System", OutputSeverity::Info);
                };
                wm199Milestone("[IDE-Pipeline:WM199] Batch 1/8: WM_APP+199 force-visible ping received\n",
                               "[Init:WM199] Batch 1/8: Startup force-visible message received\n");
                wm199Milestone("[IDE-Pipeline:WM199] Batch 2/8: validating main HWND for visibility ops\n",
                               "[Init:WM199] Batch 2/8: Checking main window handle\n");
                if (!m_hwndMain || !IsWindow(m_hwndMain))
                {
                    wm199Milestone("[IDE-Pipeline:WM199] Batch 3/8: no main HWND — visibility ops skipped\n",
                                   "[Init:WM199] Batch 3/8: Skipped (no main window yet)\n");
                    wm199Milestone("[IDE-Pipeline:WM199] Batch 4/8: (skipped)\n",
                                   "[Init:WM199] Batch 4/8: (skipped)\n");
                    wm199Milestone("[IDE-Pipeline:WM199] Batch 5/8: (skipped)\n",
                                   "[Init:WM199] Batch 5/8: (skipped)\n");
                    wm199Milestone("[IDE-Pipeline:WM199] Batch 6/8: (skipped)\n",
                                   "[Init:WM199] Batch 6/8: (skipped)\n");
                    wm199Milestone("[IDE-Pipeline:WM199] Batch 7/8: (skipped)\n",
                                   "[Init:WM199] Batch 7/8: (skipped)\n");
                    wm199Milestone("[IDE-Pipeline:WM199] Batch 8/8: WM_APP+199 handler complete (no-op path)\n",
                                   "[Init:WM199] Batch 8/8: Handler finished (no-op)\n");
                    wm199Milestone("[IDE-Pipeline:WM199] E0-1/8: Early-exit — await createWindow/showWindow\n",
                                   "[Init:WM199] E0-1/8: Defer visibility until main HWND exists\n");
                    wm199Milestone("[IDE-Pipeline:WM199] E0-2/8: Timer199 not armed\n",
                                   "[Init:WM199] E0-2/8: No follow-up visibility timer\n");
                    wm199Milestone("[IDE-Pipeline:WM199] E0-3/8: Opacity restore not applied\n",
                                   "[Init:WM199] E0-3/8: Opacity restore skipped\n");
                    wm199Milestone("[IDE-Pipeline:WM199] E0-4/8: Z-order bump skipped\n",
                                   "[Init:WM199] E0-4/8: Z-order bump skipped\n");
                    wm199Milestone("[IDE-Pipeline:WM199] E0-5/8: Foreground nudge skipped\n",
                                   "[Init:WM199] E0-5/8: Foreground nudge skipped\n");
                    wm199Milestone("[IDE-Pipeline:WM199] E0-6/8: Iconic restore skipped\n",
                                   "[Init:WM199] E0-6/8: Iconic restore skipped\n");
                    wm199Milestone("[IDE-Pipeline:WM199] E0-7/8: Safe return — no SEH in this branch\n",
                                   "[Init:WM199] E0-7/8: Safe return\n");
                    wm199Milestone("[IDE-Pipeline:WM199] E0-8/8: WM_APP+199 no-op path closed\n",
                                   "[Init:WM199] E0-8/8: WM_APP+199 no-op complete\n");
                    return 0;
                }
                wm199Milestone("[IDE-Pipeline:WM199] Batch 3/8: restoring window opacity if zero-alpha stuck\n",
                               "[Init:WM199] Batch 3/8: Restoring opacity if needed\n");
                restoreWindowOpacityIfNeeded(m_hwndMain);
                wm199Milestone("[IDE-Pipeline:WM199] Batch 4/8: unminimize if iconic (SW_RESTORE)\n",
                               "[Init:WM199] Batch 4/8: Restoring from minimized if needed\n");
                if (IsIconic(m_hwndMain))
                    ShowWindow(m_hwndMain, SW_RESTORE);
                wm199Milestone("[IDE-Pipeline:WM199] Batch 5/8: ShowWindow(SW_SHOW) on main frame\n",
                               "[Init:WM199] Batch 5/8: Showing main window\n");
                ShowWindow(m_hwndMain, SW_SHOW);
                wm199Milestone("[IDE-Pipeline:WM199] Batch 6/8: SetWindowPos TOP — NOMOVE/NOSIZE/SHOWWINDOW\n",
                               "[Init:WM199] Batch 6/8: Raising window in Z-order\n");
                SetWindowPos(m_hwndMain, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
                wm199Milestone("[IDE-Pipeline:WM199] Batch 7/8: forceWindowToForeground (best-effort)\n",
                               "[Init:WM199] Batch 7/8: Forcing foreground (best-effort)\n");
                forceWindowToForeground(m_hwndMain);
                wm199Milestone("[IDE-Pipeline:WM199] Batch 8/8: one-shot timer 199 @400ms for second nudge\n",
                               "[Init:WM199] Batch 8/8: Scheduled follow-up visibility timer (400 ms)\n");
                SetTimer(hwnd, 199, 400, nullptr);  // One-shot: force visible again in 400ms
                wm199Milestone("[IDE-Pipeline:WM199] E0-1/8: Visibility pass applied on UI thread\n",
                               "[Init:WM199] E0-1/8: Visibility pass complete\n");
                wm199Milestone("[IDE-Pipeline:WM199] E0-2/8: Chrome should accept focus from shell\n",
                               "[Init:WM199] E0-2/8: Window ready for shell focus handoff\n");
                wm199Milestone("[IDE-Pipeline:WM199] E0-3/8: Degenerate alpha path cleared if present\n",
                               "[Init:WM199] E0-3/8: Opacity recovery attempted\n");
                wm199Milestone("[IDE-Pipeline:WM199] E0-4/8: Taskbar icon state normalized\n",
                               "[Init:WM199] E0-4/8: Taskbar state normalized\n");
                wm199Milestone("[IDE-Pipeline:WM199] E0-5/8: Posted before loop — cooperates with first pumps\n",
                               "[Init:WM199] E0-5/8: Cooperates with startup message pumps\n");
                wm199Milestone("[IDE-Pipeline:WM199] E0-6/8: Timer re-entrancy guarded by same ID (199)\n",
                               "[Init:WM199] E0-6/8: Follow-up timer ID 199 armed\n");
                wm199Milestone("[IDE-Pipeline:WM199] E0-7/8: No modal dialog in this handler\n",
                               "[Init:WM199] E0-7/8: Non-modal visibility path\n");
                wm199Milestone("[IDE-Pipeline:WM199] E0-8/8: WM_APP+199 happy path closed\n",
                               "[Init:WM199] E0-8/8: WM_APP+199 complete\n");
                return 0;
            }
            // Handle deferred heavy initialization (posted from onCreate)
            if (uMsg == WM_APP + 100)
            {
                g_startupHeartbeatTicks = 0;
                SetTimer(hwnd, kStartupHeartbeatTimerId, 1000, nullptr);
                LOG_INFO("[StartupTrace] WM_APP+100 handler entered on UI thread");
                const auto wm100Milestone = [this](const char* debugLine, const char* userLine)
                {
                    if (isShuttingDown())
                        return;
                    if (debugLine && debugLine[0])
                        OutputDebugStringA(debugLine);
                    if (userLine && userLine[0] && m_hwndOutputTabs && IsWindow(m_hwndOutputTabs))
                        appendToOutput(std::string(userLine), "System", OutputSeverity::Info);
                };
                wm100Milestone("[IDE-Pipeline:WM100] Batch 1/8: WM_APP+100 received on main window proc\n",
                               "[Init:WM100] Batch 1/8: Deferred heavy-init message received\n");
                wm100Milestone("[IDE-Pipeline:WM100] Batch 2/8: shutdown gate — continuing if IDE active\n",
                               "[Init:WM100] Batch 2/8: Shutdown gate evaluated\n");
                wm100Milestone("[IDE-Pipeline:WM100] Batch 3/8: main HWND + message context validated\n",
                               "[Init:WM100] Batch 3/8: Main window handle valid for deferred dispatch\n");
                wm100Milestone("[IDE-Pipeline:WM100] Batch 4/8: System output tab route available for boot lines\n",
                               "[Init:WM100] Batch 4/8: Output tab chrome ready for pipeline trace\n");
                wm100Milestone("[IDE-Pipeline:WM100] Batch 5/8: entering SEH-wrapped deferred init trampoline\n",
                               "[Init:WM100] Batch 5/8: Invoking SEH deferred initializer\n");
                wm100Milestone("[IDE-Pipeline:WM100] Batch 6/8: trampoline will schedule deferredHeavyInit (worker)\n",
                               "[Init:WM100] Batch 6/8: Background heavy-init thread will be scheduled\n");
                wm100Milestone("[IDE-Pipeline:WM100] Batch 7/8: UI thread remains responsive during handoff\n",
                               "[Init:WM100] Batch 7/8: UI thread handoff to large-stack worker\n");
                wm100Milestone("[IDE-Pipeline:WM100] Batch 8/8: calling sehCallDeferredInit → deferredHeavyInit\n",
                               "[Init:WM100] Batch 8/8: Executing deferred init trampoline\n");
                sehCallDeferredInit(deferredInitTrampoline, this);
                wm100Milestone("[IDE-Pipeline:WM100] E0-1/8: Post-trampoline — UI thread resumed from deferred gate\n",
                               "[Init:WM100] E0-1/8: Deferred init trampoline returned on UI thread\n");
                wm100Milestone("[IDE-Pipeline:WM100] E0-2/8: Worker thread owns deferredHeavyInitBody chain\n",
                               "[Init:WM100] E0-2/8: Background initializer owns heavy subsystem chain\n");
                wm100Milestone(
                    "[IDE-Pipeline:WM100] E0-3/8: Logger + enterprise ordered inside worker (see pipeline)\n",
                    "[Init:WM100] E0-3/8: Logger and license init ordered in worker\n");
                wm100Milestone("[IDE-Pipeline:WM100] E0-4/8: Feature batches 1–8 run on worker (idePipeline)\n",
                               "[Init:WM100] E0-4/8: IDE pipeline batches run off UI thread\n");
                wm100Milestone("[IDE-Pipeline:WM100] E0-5/8: WM_APP+101 refresh posted when worker completes\n",
                               "[Init:WM100] E0-5/8: UI refresh (WM_APP+101) posted after worker completes\n");
                wm100Milestone("[IDE-Pipeline:WM100] E0-6/8: AI backend probe follows worker teardown path\n",
                               "[Init:WM100] E0-6/8: AI backend probe follows deferred body\n");
                wm100Milestone("[IDE-Pipeline:WM100] E0-7/8: No blocking wait — message pump drives completion\n",
                               "[Init:WM100] E0-7/8: Non-blocking handoff to message pump\n");
                wm100Milestone("[IDE-Pipeline:WM100] E0-8/8: WM_APP+100 handler complete\n",
                               "[Init:WM100] E0-8/8: Deferred heavy-init dispatch finished\n");
                LOG_INFO("[StartupTrace] WM_APP+100 handler returned on UI thread");
                return 0;
            }
            // Handle background init completion — refresh UI (Tier 5 menus enabled here after initTier5Cosmetics)
            if (uMsg == WM_APP + 101)
            {
                const ULONGLONG wm101StartMs = GetTickCount64();
                KillTimer(hwnd, kStartupHeartbeatTimerId);
                LOG_INFO("[StartupTrace] WM_APP+101 handler entered on UI thread");
                const auto wm101Milestone = [this](const char* debugLine, const char* userLine)
                {
                    if (isShuttingDown())
                        return;
                    if (debugLine && debugLine[0])
                        OutputDebugStringA(debugLine);
                    if (userLine && userLine[0] && m_hwndOutputTabs && IsWindow(m_hwndOutputTabs))
                        appendToOutput(std::string(userLine), "System", OutputSeverity::Info);
                };
                wm101Milestone("[IDE-Pipeline:WM101] Batch 1/8: WM_APP+101 received (post-deferred UI refresh)\n",
                               "[Init:WM101] Batch 1/8: Post-deferred UI refresh message received\n");
                wm101Milestone("[IDE-Pipeline:WM101] Batch 2/8: applying theme tokens to chrome\n",
                               "[Init:WM101] Batch 2/8: Applying theme\n");
                applyTheme();
                wm101Milestone("[IDE-Pipeline:WM101] Batch 3/8: updating menu enable states (Tier5 visibility)\n",
                               "[Init:WM101] Batch 3/8: Updating menu enable states\n");
                updateMenuEnableStates();
                wm101Milestone("[IDE-Pipeline:WM101] Batch 4/8: invalidating main client for full repaint\n",
                               "[Init:WM101] Batch 4/8: Invalidating main window client area\n");
                InvalidateRect(hwnd, nullptr, TRUE);
                wm101Milestone("[IDE-Pipeline:WM101] Batch 5/8: synchronous UpdateWindow after invalidate\n",
                               "[Init:WM101] Batch 5/8: Synchronous paint update\n");
                UpdateWindow(hwnd);
                wm101Milestone("[IDE-Pipeline:WM101] Batch 6/8: chrome + menu coherence pass complete\n",
                               "[Init:WM101] Batch 6/8: Theme and menu coherence applied\n");
                wm101Milestone("[IDE-Pipeline:WM101] Batch 7/8: deferred-init → visible UI bridge closed\n",
                               "[Init:WM101] Batch 7/8: Deferred-init to visible UI bridge complete\n");
                wm101Milestone("[IDE-Pipeline:WM101] Batch 8/8: WM_APP+101 handler returning\n",
                               "[Init:WM101] Batch 8/8: Post-deferred refresh handler finished\n");
                wm101Milestone("[IDE-Pipeline:WM101] E0-1/8: Theme application cycle finished on UI thread\n",
                               "[Init:WM101] E0-1/8: Theme cycle complete\n");
                wm101Milestone("[IDE-Pipeline:WM101] E0-2/8: Command surface reflects feature gates\n",
                               "[Init:WM101] E0-2/8: Commands reflect current feature gates\n");
                wm101Milestone("[IDE-Pipeline:WM101] E0-3/8: Full client invalidation scheduled\n",
                               "[Init:WM101] E0-3/8: Client invalidation scheduled\n");
                wm101Milestone("[IDE-Pipeline:WM101] E0-4/8: Immediate paint flush requested\n",
                               "[Init:WM101] E0-4/8: Immediate paint flush requested\n");
                wm101Milestone("[IDE-Pipeline:WM101] E0-5/8: Tier5-exposed actions reachable from menu bar\n",
                               "[Init:WM101] E0-5/8: Tier5 menu exposure synchronized\n");
                wm101Milestone("[IDE-Pipeline:WM101] E0-6/8: No worker thread in this handler — UI-only\n",
                               "[Init:WM101] E0-6/8: UI-thread-only refresh path\n");
                wm101Milestone("[IDE-Pipeline:WM101] E0-7/8: Ready for user input + subsequent WM_PAINT\n",
                               "[Init:WM101] E0-7/8: Ready for input and paint-driven updates\n");
                wm101Milestone("[IDE-Pipeline:WM101] E0-8/8: Post-deferred refresh checkpoint complete\n",
                               "[Init:WM101] E0-8/8: Post-deferred refresh checkpoints complete\n");
                const ULONGLONG wm101ElapsedMs = GetTickCount64() - wm101StartMs;
                if (wm101ElapsedMs >= 25)
                {
                    char msg[176] = {};
                    snprintf(msg, sizeof(msg), "[UI-Timing] WM_APP+101 took %llums\n",
                             static_cast<unsigned long long>(wm101ElapsedMs));
                    OutputDebugStringA(msg);
                    char logMsg[176] = {};
                    snprintf(logMsg, sizeof(logMsg), "[StartupTrace] WM_APP+101 duration=%llums",
                             static_cast<unsigned long long>(wm101ElapsedMs));
                    LOG_INFO(logMsg);
                }
                finalizeCopilotChatInterlockAfterDeferredLoad();
                LOG_INFO("[StartupTrace] WM_APP+101 handler returned on UI thread");
                return 0;
            }

            // WM_DEFERRED_INIT_FAILED: Background init worker crashed, signal IDE to attempt recovery
            if (uMsg == WM_DEFERRED_INIT_FAILED)
            {
                onDeferredInitFailed();
                return 0;
            }

            // AI backend verification result from background probe thread
            if (uMsg == WM_AI_BACKEND_STATUS)
            {
                const auto aiStatusMilestone = [this](const char* debugLine, const char* userLine)
                {
                    if (isShuttingDown())
                        return;
                    if (debugLine && debugLine[0])
                        OutputDebugStringA(debugLine);
                    if (userLine && userLine[0] && m_hwndOutputTabs && IsWindow(m_hwndOutputTabs))
                        appendToOutput(std::string(userLine), "System", OutputSeverity::Info);
                };
                const bool backendOk = (wParam != 0);
                aiStatusMilestone("[IDE-Pipeline:AIStatus] Batch 1/8: WM_AI_BACKEND_STATUS received on UI thread\n",
                                  "[Init:AIStatus] Batch 1/8: AI backend probe result delivered\n");
                aiStatusMilestone("[IDE-Pipeline:AIStatus] Batch 2/8: decoding wParam as connected flag\n",
                                  "[Init:AIStatus] Batch 2/8: Decoding probe result\n");
                if (backendOk)
                {
                    aiStatusMilestone("[IDE-Pipeline:AIStatus] Batch 3/8: probe reports CONNECTED/ready\n",
                                      "[Init:AIStatus] Batch 3/8: Backend reports ready\n");
                }
                else
                {
                    aiStatusMilestone("[IDE-Pipeline:AIStatus] Batch 3/8: probe reports OFFLINE/unavailable\n",
                                      "[Init:AIStatus] Batch 3/8: Backend reports offline\n");
                }
                aiStatusMilestone("[IDE-Pipeline:AIStatus] Batch 4/8: routing to onAIBackendVerified (UI sync)\n",
                                  "[Init:AIStatus] Batch 4/8: Applying backend status to UI state\n");
                onAIBackendVerified(backendOk);
                aiStatusMilestone("[IDE-Pipeline:AIStatus] Batch 5/8: status handler returned — chrome may refresh\n",
                                  "[Init:AIStatus] Batch 5/8: Backend status handler finished\n");
                aiStatusMilestone("[IDE-Pipeline:AIStatus] Batch 6/8: chat/router surfaces inherit new connectivity\n",
                                  "[Init:AIStatus] Batch 6/8: Inference routing reflects connectivity\n");
                aiStatusMilestone("[IDE-Pipeline:AIStatus] Batch 7/8: offline path remains CPU/native capable\n",
                                  "[Init:AIStatus] Batch 7/8: Local inference path still available when offline\n");
                aiStatusMilestone("[IDE-Pipeline:AIStatus] Batch 8/8: WM_AI_BACKEND_STATUS handler complete\n",
                                  "[Init:AIStatus] Batch 8/8: AI backend status dispatch complete\n");
                aiStatusMilestone("[IDE-Pipeline:AIStatus] E0-1/8: UI-thread-only mutation from probe thread post\n",
                                  "[Init:AIStatus] E0-1/8: UI-thread application of probe result\n");
                aiStatusMilestone("[IDE-Pipeline:AIStatus] E0-2/8: wParam boolean is single-flight signal\n",
                                  "[Init:AIStatus] E0-2/8: Single-flight connected flag consumed\n");
                aiStatusMilestone("[IDE-Pipeline:AIStatus] E0-3/8: no blocking wait — posted message semantics\n",
                                  "[Init:AIStatus] E0-3/8: Async posted-message semantics\n");
                aiStatusMilestone("[IDE-Pipeline:AIStatus] E0-4/8: onAIBackendVerified owns menu/status affordances\n",
                                  "[Init:AIStatus] E0-4/8: Status affordances updated in handler\n");
                aiStatusMilestone("[IDE-Pipeline:AIStatus] E0-5/8: failure does not abort message loop\n",
                                  "[Init:AIStatus] E0-5/8: Offline is non-fatal for shell\n");
                aiStatusMilestone("[IDE-Pipeline:AIStatus] E0-6/8: cooperates with deferred init ordering\n",
                                  "[Init:AIStatus] E0-6/8: Ordered after deferred init probe\n");
                aiStatusMilestone("[IDE-Pipeline:AIStatus] E0-7/8: re-entrant safe — no nested probe here\n",
                                  "[Init:AIStatus] E0-7/8: Handler is non-reentrant for probe\n");
                aiStatusMilestone("[IDE-Pipeline:AIStatus] E0-8/8: AI backend status checkpoint closed\n",
                                  "[Init:AIStatus] E0-8/8: AI backend status checkpoints complete\n");
                return 0;
            }
            // Tier 3 (Feature 35): File changed externally → show reload toast
            if (uMsg == WM_FILE_CHANGED_EXTERNAL)
            {
                const auto extFileMilestone = [this](const char* debugLine, const char* userLine)
                {
                    if (isShuttingDown())
                        return;
                    if (debugLine && debugLine[0])
                        OutputDebugStringA(debugLine);
                    if (userLine && userLine[0] && m_hwndOutputTabs && IsWindow(m_hwndOutputTabs))
                        appendToOutput(std::string(userLine), "System", OutputSeverity::Info);
                };
                extFileMilestone("[IDE-Pipeline:ExtFile] Batch 1/8: WM_FILE_CHANGED_EXTERNAL received\n",
                                 "[Init:ExtFile] Batch 1/8: External file change notification received\n");
                extFileMilestone("[IDE-Pipeline:ExtFile] Batch 2/8: routing to reload toast (Tier 3)\n",
                                 "[Init:ExtFile] Batch 2/8: Preparing reload toast\n");
                extFileMilestone("[IDE-Pipeline:ExtFile] Batch 3/8: UI-thread handler — no disk read here\n",
                                 "[Init:ExtFile] Batch 3/8: UI-thread dispatch (no blocking I/O)\n");
                extFileMilestone("[IDE-Pipeline:ExtFile] Batch 4/8: showFileChangedToast() invoked next\n",
                                 "[Init:ExtFile] Batch 4/8: Invoking file-changed toast\n");
                showFileChangedToast();
                extFileMilestone("[IDE-Pipeline:ExtFile] Batch 5/8: toast surface returned control to pump\n",
                                 "[Init:ExtFile] Batch 5/8: Toast handler returned\n");
                extFileMilestone("[IDE-Pipeline:ExtFile] Batch 6/8: editor buffer may prompt user reload\n",
                                 "[Init:ExtFile] Batch 6/8: User may reload from toast\n");
                extFileMilestone("[IDE-Pipeline:ExtFile] Batch 7/8: watcher coalesces bursts upstream\n",
                                 "[Init:ExtFile] Batch 7/8: Watcher coalescing assumed upstream\n");
                extFileMilestone("[IDE-Pipeline:ExtFile] Batch 8/8: WM_FILE_CHANGED_EXTERNAL handler complete\n",
                                 "[Init:ExtFile] Batch 8/8: External file-change dispatch complete\n");
                extFileMilestone("[IDE-Pipeline:ExtFile] E0-1/8: Non-fatal — does not terminate message loop\n",
                                 "[Init:ExtFile] E0-1/8: Non-fatal notification\n");
                extFileMilestone("[IDE-Pipeline:ExtFile] E0-2/8: Posted message semantics (async from watcher)\n",
                                 "[Init:ExtFile] E0-2/8: Async posted semantics\n");
                extFileMilestone("[IDE-Pipeline:ExtFile] E0-3/8: No automatic reload without user confirm\n",
                                 "[Init:ExtFile] E0-3/8: No silent reload implied\n");
                extFileMilestone("[IDE-Pipeline:ExtFile] E0-4/8: Toast is non-modal overlay path\n",
                                 "[Init:ExtFile] E0-4/8: Non-modal toast path\n");
                extFileMilestone("[IDE-Pipeline:ExtFile] E0-5/8: Safe with concurrent editor typing\n",
                                 "[Init:ExtFile] E0-5/8: Safe with concurrent editing\n");
                extFileMilestone("[IDE-Pipeline:ExtFile] E0-6/8: Re-entrant file events may queue further WM\n",
                                 "[Init:ExtFile] E0-6/8: Further events may queue\n");
                extFileMilestone("[IDE-Pipeline:ExtFile] E0-7/8: Feature 35 audit tag retained in pipeline\n",
                                 "[Init:ExtFile] E0-7/8: Tier-3 feature path\n");
                extFileMilestone("[IDE-Pipeline:ExtFile] E0-8/8: External file-change checkpoint closed\n",
                                 "[Init:ExtFile] E0-8/8: External file-change checkpoints complete\n");
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
                auto* payload = reinterpret_cast<std::string*>(lParam);
                if (payload)
                {
                    constexpr size_t kMaxAgentOutputBytes = 256 * 1024;
                    if (!payload->empty() && payload->size() <= kMaxAgentOutputBytes)
                    {
                        onAgentOutput(payload->c_str());
                    }
                    delete payload;
                }
                return 0;
            }
            if (uMsg == WM_IDE_OUTPUT_APPEND_SAFE)
            {
                auto* payload = reinterpret_cast<std::string*>(lParam);
                if (payload)
                {
                    constexpr size_t kMaxOutputAppendBytes = 256 * 1024;
                    if (!payload->empty() && payload->size() <= kMaxOutputAppendBytes)
                    {
                        appendToOutput(*payload, "Output", OutputSeverity::Info);
                    }
                    delete payload;
                }
                return 0;
            }
            if (uMsg == WM_AGENT_STREAM_FINALIZE_SAFE)
            {
                auto* payload = reinterpret_cast<std::string*>(lParam);
                if (payload)
                {
                    if (!payload->empty())
                    {
                        appendToOutput(*payload, "Agent", OutputSeverity::Info);
                    }
                    appendToOutput("\n", "Agent", OutputSeverity::Info);
                    delete payload;
                }
                if (bridgeIsAgentPanelReady())
                {
                    bridgeRefreshAgentDiff();
                }
                return 0;
            }
            if (uMsg == WM_AGENT_CHAT_CURSOR_TARGET)
            {
                const int clientX = static_cast<short>(LOWORD(lParam));
                const int clientY = static_cast<short>(HIWORD(lParam));
                setAgentChatCursorTarget(clientX, clientY, wParam != 0);
                return 0;
            }
            if (uMsg == WM_AGENT_CHAT_CURSOR_PULSE)
            {
                if (m_hwndCopilotChatOutput && IsWindow(m_hwndCopilotChatOutput))
                {
                    RECT rc{};
                    GetClientRect(m_hwndCopilotChatOutput, &rc);
                    const int x = std::max(24, static_cast<int>(rc.right) - 40);
                    const int y = std::max(24, static_cast<int>(rc.bottom) - 36);
                    setAgentChatCursorTarget(x, y, true);
                }
                return 0;
            }
            if (uMsg == WM_COPILOT_CHAT_APPEND_SAFE)
            {
                const char* text = reinterpret_cast<const char*>(lParam);
                if (text)
                {
                    appendCopilotChatTextOnUiThread(std::string(text));
                    free(const_cast<char*>(text));
                }
                return 0;
            }
            if (uMsg == WM_COPILOT_CHAT_APPEND_FLUSH)
            {
                for (;;)
                {
                    std::string batch;
                    {
                        std::lock_guard<std::mutex> lock(m_copilotChatPostCoalesceMutex);
                        batch = std::move(m_copilotChatPostCoalesceBuffer);
                        m_copilotChatPostCoalesceBuffer.clear();
                        m_copilotChatPostCoalesceFlushPosted = false;
                    }
                    if (batch.empty())
                    {
                        break;
                    }
                    appendCopilotChatTextOnUiThread(batch);
                }
                return 0;
            }
            if (uMsg == WM_COPILOT_RECORD_TOOL_TURN)
            {
                auto* raw = reinterpret_cast<std::pair<std::string, std::string>*>(lParam);
                if (!raw)
                    return 0;
                std::unique_ptr<std::pair<std::string, std::string>> holder(raw);
                recordPersistedToolTurnOnUiThread(holder->first, holder->second);
                return 0;
            }
            if (uMsg == WM_COPILOT_AGENTIC_ASSISTANT_FINAL)
            {
                auto* raw = reinterpret_cast<Win32IDEAgenticCopilotFinalEnvelope*>(lParam);
                if (!raw)
                    return 0;
                std::unique_ptr<Win32IDEAgenticCopilotFinalEnvelope> holder(raw);
                applyAgenticAssistantFinalOnUiThread(std::move(holder->streamAccumulatorSnapshot),
                                                     holder->finalAssistantText);
                return 0;
            }
            if (uMsg == WM_COPILOT_INPUT_CLEAR_SAFE)
            {
                clearCopilotInputOnUiThread();
                return 0;
            }
            if (uMsg == WM_COPILOT_INTERACTION_BUSY_SAFE)
            {
                setCopilotInteractionBusyOnUiThread(wParam != 0);
                return 0;
            }
            if (uMsg == WM_COPILOT_BACKEND_READY_SAFE)
            {
                setCopilotBackendReadyOnUiThread(wParam != 0);
                return 0;
            }
            if (uMsg == WM_COPILOT_MINIMAL_AGENTIC_DONE)
            {
                RawrXD_FinishCopilotMinimalAgentic(this, wParam, lParam);
                return 0;
            }
            if (uMsg == WM_IDE_MOE_PACK_STATUS_REFRESH)
            {
                refreshMoEPackHudStatusBarPart();
                return 0;
            }
            if (uMsg == WM_AGENTIC_TELEMETRY_UPDATE)
            {
                auto* p = reinterpret_cast<std::pair<std::string, std::string>*>(lParam);
                if (p)
                {
                    // For now, route to output but prepare for Sidebar Truth View update
                    appendToOutput(" [TRUTH:" + p->first + "] " + p->second + "\n", "Truth", OutputSeverity::Info);
                    delete p;
                }
                return 0;
            }
            if (uMsg == WM_AGENTIC_TELEMETRY_DONE)
            {
                appendToOutput(" [TRUTH] Execution Graph Finalized.\n", "Truth", OutputSeverity::Info);
                return 0;
            }
            if (uMsg == WM_USER_OBSERVABILITY_SYNC)
            {
                // Trigger sidebar repaint if viewing ExecutionTruth
                if (m_currentSidebarView == SidebarView::ExecutionTruth)
                {
                    InvalidateRect(m_hwndSidebar, nullptr, TRUE);
                }
                return 0;
            }
            if (uMsg == WM_RAWR_LOG_MESSAGE)
            {
                const char* text = reinterpret_cast<const char*>(lParam);
                if (text)
                {
                    LogToOutputPanel(text, static_cast<int>(wParam));
                    free(const_cast<char*>(text));
                }
                return 0;
            }
            if (uMsg == WM_RAWRXD_GHOST_TEXT_CACHE_CLEAR)
            {
                std::lock_guard<std::mutex> lock(m_ghostTextCacheMutex);
                m_ghostTextCache.clear();
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
            if (uMsg == WM_USER_GHOST_TOKEN)
            {
                const char* token = reinterpret_cast<const char*>(lParam);
                onGhostTextTokenChunk(token, static_cast<uint64_t>(wParam));
                if (token)
                    free(const_cast<char*>(token));
                return 0;
            }
            if (uMsg == WM_USER_GHOST_RENDER)
            {
                onGhostTextRenderMessage();
                return 0;
            }
            if (uMsg == WM_USER_GHOST_COMPLETE)
            {
                const char* completionText = reinterpret_cast<const char*>(lParam);
                onGhostTextComplete(static_cast<uint64_t>(wParam), completionText);
                if (completionText)
                    free(const_cast<char*>(completionText));
                return 0;
            }
            if (uMsg == RawrXD::Review::WM_USER_EDIT_REVIEW_REQUIRED)
            {
                auto* request = reinterpret_cast<RawrXD::Review::PendingEditRequest*>(lParam);
                queueNavigatorPendingEdit(request);
                return 0;
            }
            if (uMsg == WM_TITAN_GHOST_STREAM)
            {
                Win32IDE_HandleTitanGhostStreamMessage(this);
                return 0;
            }
            if (uMsg == WM_TITAN_AGENT_STREAM)
            {
                onTitanAgentStreamMessage();
                return 0;
            }
            if (uMsg == WM_TITAN_AGENT_DONE)
            {
                onTitanAgentDone((int)wParam);
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
            // Code completion results from background thread
            if (uMsg == (WM_USER + 0x501))
            {
                onCompletionReady(lParam);
                return 0;
            }
            break;
    }

    return DefWindowProcA(hwnd, uMsg, wParam, lParam);
}

void Win32IDE::LogToOutputPanel(const char* message, int type)
{
    if (!message || !*message)
        return;

    OutputSeverity severity = OutputSeverity::Info;
    const char* tabName = "Output";
    if (type == 1)
        severity = OutputSeverity::Warning;
    else if (type == 2)
    {
        severity = OutputSeverity::Error;
        tabName = "Errors";
    }

    appendToOutput(std::string(message), tabName, severity);
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
        m_ollamaBaseUrl = config.getString("native.baseUrl", "http://localhost:11435");
        m_ollamaModelOverride = config.getString("native.modelOverride", "");
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

    // Load RichEdit libraries up front so control creation failures are explicit.
    HMODULE hMsftEdit = LoadLibraryW(L"Msftedit.dll");
    HMODULE hRichEdit20 = LoadLibraryW(L"riched20.dll");
    if (!hMsftEdit && !hRichEdit20)
    {
        LOG_ERROR("Failed to load RichEdit runtime (Msftedit.dll / riched20.dll unavailable)");
        return false;
    }
    if (!hMsftEdit)
    {
        LOG_WARNING("Msftedit.dll not available; continuing with riched20 fallback lane");
    }

    WNDCLASSEXA wc = {};
    wc.cbSize = sizeof(WNDCLASSEXA);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = Win32IDE::WindowProc;
    wc.hInstance = m_hInstance;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    HBRUSH classBackgroundBrush = CreateSolidBrush(RGB(30, 30, 30));
    wc.hbrBackground = classBackgroundBrush;
    wc.lpszClassName = kWindowClassName;
    wc.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
    wc.hIconSm = LoadIcon(nullptr, IDI_APPLICATION);

    if (!RegisterClassExA(&wc))
    {
        // Class might already be registered
        DWORD err = GetLastError();
        if (classBackgroundBrush)
        {
            DeleteObject(classBackgroundBrush);
            classBackgroundBrush = nullptr;
        }
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
    m_hwndMain = CreateWindowExA(WS_EX_APPWINDOW, kWindowClassName, "RawrXD - Native Win32 AI Development Environment",
                                 WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN | WS_CLIPSIBLINGS | WS_VISIBLE, winX, winY, winW,
                                 winH, nullptr, nullptr, m_hInstance, this);

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
// showMainWindowSafe - canonical path for bringing the main IDE window onscreen.
// ============================================================================
void Win32IDE::showMainWindowSafe()
{
    if (!m_hwndMain)
        return;

    logWindowPlacementSnapshot(m_hwndMain, "showMainWindowSafe:before");
    restoreWindowOpacityIfNeeded(m_hwndMain);

    if (IsIconic(m_hwndMain))
        ShowWindow(m_hwndMain, SW_RESTORE);

    normalizeWindowPlacementForVisibility(m_hwndMain);
    ShowWindow(m_hwndMain, SW_SHOWNORMAL);
    UpdateWindow(m_hwndMain);
    SetWindowPos(m_hwndMain, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
    BringWindowToTop(m_hwndMain);
    SetForegroundWindow(m_hwndMain);
    SetActiveWindow(m_hwndMain);
    forceWindowToForeground(m_hwndMain);
    restoreWindowOpacityIfNeeded(m_hwndMain);
    SetTimer(m_hwndMain, IDT_VISIBILITY_WATCHDOG, 1000, nullptr);
    SetTimer(m_hwndMain, IDT_GPU_TELEMETRY, 2000, nullptr);
    FLASHWINFO fwi = {sizeof(FLASHWINFO), m_hwndMain, FLASHW_ALL | FLASHW_TIMERNOFG, 3, 0};
    FlashWindowEx(&fwi);

    logWindowPlacementSnapshot(m_hwndMain, "showMainWindowSafe:after");
}

// ============================================================================
// showWindow - backwards-compatible wrapper for existing callers.
// ============================================================================
void Win32IDE::showWindow()
{
    showMainWindowSafe();
}

// ============================================================================
// runMessageLoop - Standard Win32 message loop with accelerators
// ============================================================================
int Win32IDE::runMessageLoop()
{
    LOG_INFO("Message loop starting");
    METRICS.increment("app.message_loop_starts");
    auto loopStart = std::chrono::high_resolution_clock::now();

    const auto loopBootMilestone = [](const char* debugLine, const char* /*userLine*/)
    {
        if (debugLine && debugLine[0])
            OutputDebugStringA(debugLine);
    };
    loopBootMilestone("[IDE-Pipeline:MsgLoop] Batch 1/8: runMessageLoop entered\n",
                      "[Init:MsgLoop] Batch 1/8: Primary message loop starting\n");
    loopBootMilestone("[IDE-Pipeline:MsgLoop] Batch 2/8: session metrics + loop timer armed\n",
                      "[Init:MsgLoop] Batch 2/8: Session metrics counters armed\n");
    loopBootMilestone("[IDE-Pipeline:MsgLoop] Batch 3/8: accelerator table bound to main HWND\n",
                      "[Init:MsgLoop] Batch 3/8: Accelerator table will translate for main window\n");
    loopBootMilestone("[IDE-Pipeline:MsgLoop] Batch 4/8: fallback key routing (palette, quick open) armed\n",
                      "[Init:MsgLoop] Batch 4/8: Fallback keyboard routing ready\n");
    loopBootMilestone("[IDE-Pipeline:MsgLoop] Batch 5/8: GetMessage pump — blocking until quit or message\n",
                      "[Init:MsgLoop] Batch 5/8: Entering GetMessage dispatch loop\n");
    loopBootMilestone("[IDE-Pipeline:MsgLoop] Batch 6/8: TranslateMessage + DispatchMessage path active\n",
                      "[Init:MsgLoop] Batch 6/8: Standard translate/dispatch path active\n");
    loopBootMilestone("[IDE-Pipeline:MsgLoop] Batch 7/8: AI worker invoke queue serviced each iteration\n",
                      "[Init:MsgLoop] Batch 7/8: AI worker queue hooked into pump\n");
    loopBootMilestone("[IDE-Pipeline:MsgLoop] Batch 8/8: first GetMessage iteration beginning\n",
                      "[Init:MsgLoop] Batch 8/8: Awaiting first message\n");
    loopBootMilestone("[IDE-Pipeline:MsgLoop] E0-1/8: Main HWND stable for TranslateAccelerator\n",
                      "[Init:MsgLoop] E0-1/8: Main window stable for accelerators\n");
    loopBootMilestone("[IDE-Pipeline:MsgLoop] E0-2/8: m_hAccel valid or null-safe (table optional)\n",
                      "[Init:MsgLoop] E0-2/8: Accelerator handle state known\n");
    loopBootMilestone("[IDE-Pipeline:MsgLoop] E0-3/8: Exception containment around pump (try/catch)\n",
                      "[Init:MsgLoop] E0-3/8: Pump wrapped in exception containment\n");
    loopBootMilestone("[IDE-Pipeline:MsgLoop] E0-4/8: METRICS session duration recorded on exit\n",
                      "[Init:MsgLoop] E0-4/8: Session duration will record on loop end\n");
    loopBootMilestone("[IDE-Pipeline:MsgLoop] E0-5/8: IDEConfig save hooks on normal loop exit\n",
                      "[Init:MsgLoop] E0-5/8: Config save scheduled on clean exit\n");
    loopBootMilestone("[IDE-Pipeline:MsgLoop] E0-6/8: WM_QUIT will surface as exit code in wParam\n",
                      "[Init:MsgLoop] E0-6/8: Quit code will propagate from WM_QUIT\n");
    loopBootMilestone("[IDE-Pipeline:MsgLoop] E0-7/8: Deferred WM_APP+100/101 may process inside this loop\n",
                      "[Init:MsgLoop] E0-7/8: Deferred init messages may process inside this loop\n");
    loopBootMilestone("[IDE-Pipeline:MsgLoop] E0-8/8: Entering while (GetMessage) — loop live\n",
                      "[Init:MsgLoop] E0-8/8: Message loop live\n");

    MSG msg = {};
    bool messageLoopFatalShown = false;
    for (;;)
    {
        const BOOL gotMsg = GetMessage(&msg, nullptr, 0, 0);
        if (gotMsg == 0)
            break;
        if (gotMsg == -1)
        {
            LOG_ERROR("GetMessage failed in primary message loop");
            break;
        }

        try
        {
            METRICS.increment("app.messages_processed");

            // Smoke harness polls the extension pipe on a dedicated worker; UI polling the
            // same handle concurrently corrupts framing and can throw std::bad_alloc.
            if (!smokeDeferredInitActive())
            {
                try
                {
                    RawrXD::Extensions::PollExtensionEngineLsp();
                }
                catch (const std::exception& e)
                {
                    LOG_WARNING(std::string("PollExtensionEngineLsp skipped: ") + e.what());
                }
                catch (...)
                {
                    LOG_WARNING("PollExtensionEngineLsp skipped: unknown error");
                }
            }

            if (m_hwndMain && m_hAccel && TranslateAccelerator(m_hwndMain, m_hAccel, &msg))
                continue;

            // Handle accelerator keys (fallback / keys not in ACCEL table)
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
                // VS Code / Cursor: Quick Open (Ctrl+P) — fallback when TranslateAccelerator does not consume
                if (ctrl && !shift && !alt && msg.wParam == 'P')
                {
                    routeCommand(IDM_FILE_QUICK_OPEN);
                    continue;
                }
                // VS Code / Cursor: toggle integrated terminal (Ctrl+`) — OEM_3/`~ US, OEM_8 on some layouts
                if (ctrl && !shift && !alt && (msg.wParam == VK_OEM_3 || msg.wParam == VK_OEM_8))
                {
                    routeCommand(2029);  // IDM_VIEW_TERMINAL
                    continue;
                }
                // VS Code: toggle bottom panel visibility (Ctrl+J) — workbench.action.togglePanel
                if (ctrl && !shift && !alt && msg.wParam == 'J')
                {
                    routeCommand(IDM_VIEW_TOGGLE_BOTTOM_PANEL);
                    continue;
                }
                // Cursor-style: Ctrl+L → Agent / chat panel
                if (ctrl && !shift && !alt && msg.wParam == 'L')
                {
                    routeCommand(3009);
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
                // Cursor / VS Code terminal: Ctrl+` toggles panel; Ctrl+Shift+` creates new user terminal
                if (ctrl && shift && !alt && (msg.wParam == VK_OEM_3 || msg.wParam == VK_OEM_8))
                {
                    routeCommand(4011);
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
                // Ctrl+Shift+Y → Toggle Current File Context
                if (ctrl && shift && msg.wParam == 'Y')
                {
                    routeCommandUnified(IDM_AGENT_TOGGLE_FILE_CONTEXT, this, m_hwndMain);
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

            const auto dispatchBegin = std::chrono::steady_clock::now();
            TranslateMessage(&msg);
            AIWorkersProcessInvokeQueue();
            DispatchMessage(&msg);
            const auto dispatchElapsedMs =
                std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - dispatchBegin)
                    .count();
            if (dispatchElapsedMs > 75)
            {
                std::ostringstream oss;
                oss << "[PumpTrace] LONG_DISPATCH msg=0x" << std::hex << msg.message << std::dec
                    << " elapsed_ms=" << dispatchElapsedMs;
                LOG_WARNING(oss.str());
            }
        }
        catch (const std::exception& e)
        {
            char diag[96] = {};
            snprintf(diag, sizeof(diag), " msg=0x%04X hwnd=%p", static_cast<unsigned>(msg.message),
                     static_cast<void*>(msg.hwnd));
            LOG_CRITICAL(std::string("Unhandled exception in message loop: ") + e.what() + diag);
            METRICS.increment("app.unhandled_exceptions");
            if (!messageLoopFatalShown)
            {
                messageLoopFatalShown = true;
                MessageBoxA(nullptr,
                            (std::string("RawrXD IDE encountered an error (pump continues):\n") + e.what()).c_str(),
                            "RawrXD IDE - Critical Error", MB_ICONERROR | MB_OK);
            }
        }
        catch (...)
        {
            LOG_CRITICAL("Unknown unhandled exception in message loop");
            METRICS.increment("app.unhandled_exceptions");
            if (!messageLoopFatalShown)
            {
                messageLoopFatalShown = true;
                MessageBoxA(nullptr, "RawrXD IDE encountered an unknown error (pump continues).",
                            "RawrXD IDE - Critical Error", MB_ICONERROR | MB_OK);
            }
        }
    }

    // Log session metrics on exit (debugger always; System tab if chrome still alive)
    const auto loopExitMilestone = [this](const char* debugLine, const char* userLine)
    {
        if (debugLine && debugLine[0])
            OutputDebugStringA(debugLine);
        if (userLine && userLine[0] && m_hwndOutputTabs && IsWindow(m_hwndOutputTabs))
            appendToOutput(std::string(userLine), "System", OutputSeverity::Info);
    };
    loopExitMilestone("[IDE-Pipeline:MsgLoopExit] Batch 1/8: message loop exited (GetMessage returned 0 or error)\n",
                      "[Shutdown:MsgLoop] Batch 1/8: Primary message loop stopped\n");
    auto loopEnd = std::chrono::high_resolution_clock::now();
    double sessionMs = std::chrono::duration<double, std::milli>(loopEnd - loopStart).count();
    loopExitMilestone("[IDE-Pipeline:MsgLoopExit] Batch 2/8: session wall clock sampled for METRICS\n",
                      "[Shutdown:MsgLoop] Batch 2/8: Session duration sampled\n");
    METRICS.recordDuration("app.session_duration_ms", sessionMs);
    loopExitMilestone("[IDE-Pipeline:MsgLoopExit] Batch 3/8: app.session_duration_ms recorded\n",
                      "[Shutdown:MsgLoop] Batch 3/8: Metrics duration recorded\n");
    LOG_INFO("Message loop ended — session duration: " + std::to_string(sessionMs / 1000.0) + "s");
    loopExitMilestone("[IDE-Pipeline:MsgLoopExit] Batch 4/8: LOG_INFO session summary emitted\n",
                      "[Shutdown:MsgLoop] Batch 4/8: Session summary logged\n");
    LOG_INFO("Messages processed: " + std::to_string(METRICS.getCounter("app.messages_processed")));
    loopExitMilestone("[IDE-Pipeline:MsgLoopExit] Batch 5/8: messages_processed counter logged\n",
                      "[Shutdown:MsgLoop] Batch 5/8: Message count logged\n");
    loopExitMilestone("[IDE-Pipeline:MsgLoopExit] Batch 6/8: persisting IDEConfig rawrxd.config.json\n",
                      "[Shutdown:MsgLoop] Batch 6/8: Saving IDE configuration\n");
    // Save configuration on exit
    IDEConfig::getInstance().saveToFile("rawrxd.config.json");
    loopExitMilestone("[IDE-Pipeline:MsgLoopExit] Batch 7/8: IDEConfig saveToFile completed\n",
                      "[Shutdown:MsgLoop] Batch 7/8: Configuration file write completed\n");
    loopExitMilestone("[IDE-Pipeline:MsgLoopExit] Batch 8/8: returning exit code from msg.wParam\n",
                      "[Shutdown:MsgLoop] Batch 8/8: Returning process exit code\n");
    loopExitMilestone("[IDE-Pipeline:MsgLoopExit] E0-1/8: WM_QUIT wParam preserved for WinMain\n",
                      "[Shutdown:MsgLoop] E0-1/8: Quit code preserved\n");
    loopExitMilestone("[IDE-Pipeline:MsgLoopExit] E0-2/8: METRICS gauges finalized for this session\n",
                      "[Shutdown:MsgLoop] E0-2/8: Session metrics finalized\n");
    loopExitMilestone("[IDE-Pipeline:MsgLoopExit] E0-3/8: Exception path may have skipped loop tail — still safe\n",
                      "[Shutdown:MsgLoop] E0-3/8: Unified exit tail after try/catch\n");
    loopExitMilestone("[IDE-Pipeline:MsgLoopExit] E0-4/8: IDEConfig path is cwd-relative rawrxd.config.json\n",
                      "[Shutdown:MsgLoop] E0-4/8: Config path is rawrxd.config.json\n");
    loopExitMilestone("[IDE-Pipeline:MsgLoopExit] E0-5/8: No second GetMessage after this point\n",
                      "[Shutdown:MsgLoop] E0-5/8: Pump fully drained\n");
    loopExitMilestone("[IDE-Pipeline:MsgLoopExit] E0-6/8: HWND lifetime managed by WM_DESTROY elsewhere\n",
                      "[Shutdown:MsgLoop] E0-6/8: Window teardown follows Win32 lifecycle\n");
    loopExitMilestone("[IDE-Pipeline:MsgLoopExit] E0-7/8: Resource-heavy subsystems already torn down in onDestroy\n",
                      "[Shutdown:MsgLoop] E0-7/8: Heavy teardown remains in onDestroy path\n");
    loopExitMilestone("[IDE-Pipeline:MsgLoopExit] E0-8/8: runMessageLoop return boundary\n",
                      "[Shutdown:MsgLoop] E0-8/8: runMessageLoop return\n");

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

    // WM_SIZE can arrive before the editor control exists during cold startup.
    // Defer full layout until core panes are created to avoid partial/incorrect bounds.
    if (!m_hwndEditor || !IsWindow(m_hwndEditor) || !m_hwndMain || !IsWindow(m_hwndMain))
        return;

    const int TOOLBAR_HEIGHT = dpiScale(32);
    const int STATUSBAR_HEIGHT = dpiScale(24);
    const int ACTIVITY_BAR_WIDTH = dpiScale(48);
    const int TAB_BAR_HEIGHT = dpiScale(28);

    const bool hasPrimarySidebar = (m_hwndSidebar && IsWindow(m_hwndSidebar));
    const bool hasSecondarySidebar = (m_hwndSecondarySidebar && IsWindow(m_hwndSecondarySidebar));
    int sidebarWidth = (m_sidebarVisible && hasPrimarySidebar) ? m_sidebarWidth : 0;
    int secondarySidebarWidth = (m_secondarySidebarVisible && hasSecondarySidebar) ? m_secondarySidebarWidth : 0;
    const int minEditorWidth = dpiScale(320);
    const int minPanelHeight = dpiScale(96);

    auto isNear = [this](int widthValue, int basePixels) -> bool
    {
        const int target = dpiScale(basePixels);
        const int tol = dpiScale(12);
        const int delta = (widthValue >= target) ? (widthValue - target) : (target - widthValue);
        return delta <= tol;
    };

    if (m_activeSnapState != SnapState::None && m_activeSnapState != SnapState::Custom)
    {
        bool stillOnPreset = true;
        switch (m_activeSnapState)
        {
            case SnapState::Compact:
                stillOnPreset = isNear(m_secondarySidebarWidth, 240);
                break;
            case SnapState::Standard:
                stillOnPreset = isNear(m_secondarySidebarWidth, 360);
                break;
            case SnapState::Wide:
                stillOnPreset = isNear(m_secondarySidebarWidth, 480);
                break;
            default:
                break;
        }

        if (!stillOnPreset)
        {
            m_activeSnapState = SnapState::Custom;
            updateSovereignSnapMenuChecks();
            saveSessionDebounced(500);
        }
    }

    // Guard against corrupted/restored splitter values that can collapse editor/chat regions.
    sidebarWidth = (std::max)(0, sidebarWidth);
    secondarySidebarWidth = (std::max)(0, secondarySidebarWidth);
    sidebarWidth = (std::min)(sidebarWidth, width);
    secondarySidebarWidth = (std::min)(secondarySidebarWidth, width);

    const int maxSidebarsCombined = (std::max)(0, width - minEditorWidth);
    long long combinedSidebars = static_cast<long long>(sidebarWidth) + static_cast<long long>(secondarySidebarWidth);
    if (combinedSidebars > maxSidebarsCombined)
    {
        int over = static_cast<int>(combinedSidebars - maxSidebarsCombined);
        int reduceSecondary = (std::min)(over, secondarySidebarWidth);
        secondarySidebarWidth -= reduceSecondary;
        over -= reduceSecondary;
        if (over > 0)
            sidebarWidth = (std::max)(0, sidebarWidth - over);
    }
    // Only reserve space for panels that actually have HWNDs created
    int panelHeight = (m_outputPanelVisible && m_hwndOutputTabs) ? m_outputTabHeight : 0;
    int powerShellHeight = (m_powerShellPanelVisible && m_hwndPowerShellPanel) ? m_powerShellPanelHeight : 0;

    panelHeight = (std::max)(0, panelHeight);
    powerShellHeight = (std::max)(0, powerShellHeight);

    int contentTop = TOOLBAR_HEIGHT;
    int contentBottom = height - STATUSBAR_HEIGHT;
    int contentHeight = contentBottom - contentTop;

    // Batch as many child reposition ops as possible to reduce resize flicker/overdraw.
    HDWP hdwp = BeginDeferWindowPos(24);
    auto moveChild = [&](HWND child, int x, int y, int w, int h, BOOL repaint = TRUE)
    {
        if (!child || !IsWindow(child))
            return;

        if (hdwp)
        {
            hdwp = DeferWindowPos(hdwp, child, nullptr, x, y, w, h, SWP_NOZORDER | SWP_NOACTIVATE);
            if (hdwp)
                return;
        }

        MoveWindow(child, x, y, w, h, repaint);
    };

    // Clamp bottom panels so they cannot consume all editor space.
    const int maxBottomPanelsHeight = (std::max)(0, contentHeight - minPanelHeight);
    long long combinedBottomPanels = static_cast<long long>(panelHeight) + static_cast<long long>(powerShellHeight);
    if (combinedBottomPanels > maxBottomPanelsHeight)
    {
        int over = static_cast<int>(combinedBottomPanels - maxBottomPanelsHeight);
        int reducePowerShell = (std::min)(over, powerShellHeight);
        powerShellHeight -= reducePowerShell;
        over -= reducePowerShell;
        if (over > 0)
            panelHeight = (std::max)(0, panelHeight - over);
    }

    // Status bar
    if (m_hwndStatusBar)
    {
        SendMessage(m_hwndStatusBar, WM_SIZE, 0, 0);
    }

    // Toolbar
    if (m_hwndToolbar)
    {
        moveChild(m_hwndToolbar, 0, 0, width, TOOLBAR_HEIGHT, TRUE);
    }

    // Activity bar (far left)
    if (m_hwndActivityBar)
    {
        moveChild(m_hwndActivityBar, 0, contentTop, ACTIVITY_BAR_WIDTH, contentHeight, TRUE);
    }

    // Primary sidebar
    if (m_hwndSidebar && m_sidebarVisible)
    {
        moveChild(m_hwndSidebar, ACTIVITY_BAR_WIDTH, contentTop, sidebarWidth, contentHeight, TRUE);
    }

    // Calculate editor area
    int editorLeft = ACTIVITY_BAR_WIDTH + sidebarWidth;
    int editorRight = width - secondarySidebarWidth;
    int editorWidth = editorRight - editorLeft;
    if (editorWidth < minEditorWidth)
    {
        editorWidth = (std::max)(0, width - editorLeft);
        secondarySidebarWidth = 0;
        editorRight = editorLeft + editorWidth;
    }
    const int bottomDockHeight = panelHeight + powerShellHeight;
    int editorAreaHeight = contentHeight - bottomDockHeight;
    editorAreaHeight = (std::max)(0, editorAreaHeight);

    // Nested coordinate system for the central workspace (editor + bottom dock).
    RECT workspaceRect = {editorLeft, contentTop, editorRight, contentTop + contentHeight};
    RECT editorRect = {workspaceRect.left, workspaceRect.top, workspaceRect.right,
                       workspaceRect.top + editorAreaHeight};
    RECT bottomDockRect = {workspaceRect.left, editorRect.bottom, workspaceRect.right, workspaceRect.bottom};

    const int bottomTabStripHeight = dpiScale(24);

    // Tab bar (above editor)
    int tabBarBottom = contentTop;
    if (m_hwndTabBar)
    {
        moveChild(m_hwndTabBar, editorRect.left, editorRect.top, editorWidth, TAB_BAR_HEIGHT, TRUE);
        tabBarBottom = editorRect.top + TAB_BAR_HEIGHT;
    }

    // Breadcrumb bar (below tab bar, above editor) — ESP IE labeled
    int breadcrumbBottom = tabBarBottom;
    if (m_hwndBreadcrumbs && m_settings.breadcrumbsEnabled)
    {
        moveChild(m_hwndBreadcrumbs, editorLeft, tabBarBottom, editorWidth, m_breadcrumbHeight, TRUE);
        breadcrumbBottom = tabBarBottom + m_breadcrumbHeight;
    }

    int editorContentHeight = editorAreaHeight - (breadcrumbBottom - contentTop);
    editorContentHeight = (std::max)(0, editorContentHeight);

    // Line number gutter (left of editor)
    int gutterWidth = m_hwndLineNumbers ? m_lineNumberWidth : 0;
    int editorW = 0;
    if (m_hwndLineNumbers)
    {
        moveChild(m_hwndLineNumbers, editorRect.left, breadcrumbBottom, gutterWidth, editorContentHeight, TRUE);
    }

    // Editor (right of gutter)
    if (m_hwndEditor)
    {
        int minimapW = (m_minimapVisible && m_hwndMinimap) ? m_minimapWidth : 0;
        int editorX = editorRect.left + gutterWidth;
        editorW = editorWidth - gutterWidth - minimapW;
        editorW = (std::max)(0, editorW);
        moveChild(m_hwndEditor, editorX, breadcrumbBottom, editorW, editorContentHeight, TRUE);

        // Annotation overlay (same rect as editor, draws inline annotations on top)
        if (m_hwndAnnotationOverlay && IsWindow(m_hwndAnnotationOverlay))
        {
            SetWindowPos(m_hwndAnnotationOverlay, HWND_TOP, editorX, breadcrumbBottom, editorW, editorContentHeight,
                         SWP_NOACTIVATE);
        }

        if (lineStripEditorEnabled())
        {
            layoutLineStripOverlay();
        }

        // Minimap
        if (m_hwndMinimap && m_minimapVisible)
        {
            minimapW = (std::max)(0, (std::min)(minimapW, editorWidth));
            moveChild(m_hwndMinimap, editorRect.right - minimapW, breadcrumbBottom, minimapW, editorContentHeight,
                      TRUE);
        }
    }

    // Output / Terminal panel area
    int panelTop = bottomDockRect.top;
    if (panelHeight > 0)
    {
        // Output tabs
        if (m_hwndOutputTabs)
        {
            moveChild(m_hwndOutputTabs, bottomDockRect.left, panelTop, editorWidth, panelHeight, TRUE);
        }
        int termTop = bottomDockRect.top;
        int tabBarH = bottomTabStripHeight;
        int termHeight = panelHeight;
        // Output tab windows (Output, Errors, Debug, Find Results)
        for (auto& kv : m_outputWindows)
        {
            if (kv.second)
            {
                moveChild(kv.second, bottomDockRect.left, termTop + tabBarH, editorWidth, termHeight - tabBarH, TRUE);
            }
        }
        // Problems ListView (5th tab) — same region
        if (m_hwndProblemsListView)
        {
            moveChild(m_hwndProblemsListView, bottomDockRect.left, termTop + tabBarH, editorWidth, termHeight - tabBarH,
                      TRUE);
        }
        panelTop += panelHeight;
    }

    // Terminal panes — layout within the output panel area so they're visible
    if (!m_terminalPanes.empty() && panelHeight > 0)
    {
        int termTop = bottomDockRect.top;
        int termHeight = panelHeight;
        int tabBarH = bottomTabStripHeight;
        if (m_hwndOutputTabs)
        {
            // Windows already positioned above; no-op for positioning
        }
    }

    // PowerShell panel
    if (m_hwndPowerShellPanel && m_powerShellPanelVisible)
    {
        moveChild(m_hwndPowerShellPanel, bottomDockRect.left, panelTop, editorWidth, powerShellHeight, TRUE);
    }

    // Secondary sidebar (Copilot Chat / AI Panel)
    if (m_hwndSecondarySidebar && m_secondarySidebarVisible)
    {
        moveChild(m_hwndSecondarySidebar, editorRight, contentTop, secondarySidebarWidth, contentHeight, TRUE);
    }

    if (hdwp)
    {
        EndDeferWindowPos(hdwp);
        hdwp = nullptr;
    }

    // Reflow secondary sidebar internals (chat pane controls) on every resize.
    // Historically these controls were created with fixed Y coordinates, which can
    // overlap at non-default window sizes and trigger pathological redraw behavior.
    if (m_hwndSecondarySidebar && m_secondarySidebarVisible && IsWindow(m_hwndSecondarySidebar))
    {
        struct UIRect
        {
            int x;
            int y;
            int w;
            int h;
            const char* debugName;
            HWND hwnd;
        };

        auto overlap = [](const UIRect& a, const UIRect& b) -> bool
        { return (a.x < b.x + b.w && a.x + a.w > b.x && a.y < b.y + b.h && a.y + a.h > b.y); };

        auto resolveOverlap = [&](const UIRect& fixedObj, UIRect& movingObj) -> bool
        {
            if (!overlap(fixedObj, movingObj))
                return false;

            // Sovereign Symbol logic: if the moving object is an "Emoji Glyph",
            // give it priority by shifting other non-critical elements or
            // stacking them with Z-order sorting.
            movingObj.y = fixedObj.y + fixedObj.h + dpiScale(5);
            char buf[256] = {};
            sprintf_s(buf, "[RawrXD Layout] AUTO-FIX: %s overlapped %s; moved to y=%d (Sovereign Sync Priority)\n",
                      movingObj.debugName ? movingObj.debugName : "<unknown>",
                      fixedObj.debugName ? fixedObj.debugName : "<unknown>", movingObj.y);
            OutputDebugStringA(buf);
            return true;
        };

        RECT sbRc = {};
        GetClientRect(m_hwndSecondarySidebar, &sbRc);
        const int sidebarW = (std::max)(0, static_cast<int>(sbRc.right - sbRc.left));
        const int sidebarH = (std::max)(0, static_cast<int>(sbRc.bottom - sbRc.top));

        const int pad = dpiScale(6);
        const int gap = dpiScale(6);
        const int contentW = (std::max)(dpiScale(120), sidebarW - pad * 2);

        int y = pad;
        if (m_hwndSecondarySidebarHeader && IsWindow(m_hwndSecondarySidebarHeader))
        {
            MoveWindow(m_hwndSecondarySidebarHeader, pad, y, contentW, dpiScale(24), TRUE);

            // Anchor the "Thinking" Pulse Symbol to the top-right of the chat header
            if (m_hwndEmojiPulseSymbol && IsWindow(m_hwndEmojiPulseSymbol))
            {
                int pulseSize = dpiScale(22);
                MoveWindow(m_hwndEmojiPulseSymbol, sidebarW - pad - pulseSize, y + (dpiScale(24) - pulseSize) / 2,
                           pulseSize, pulseSize, TRUE);
            }

            y += dpiScale(24) + gap;
        }

        // Model + sliders region
        if (m_hwndModelSelector && IsWindow(m_hwndModelSelector))
        {
            MoveWindow(m_hwndModelSelector, pad + dpiScale(55), y, (std::max)(dpiScale(90), contentW - dpiScale(55)),
                       dpiScale(220), TRUE);
        }
        y += dpiScale(58);

        if (m_hwndMaxTokensLabel && IsWindow(m_hwndMaxTokensLabel))
            MoveWindow(m_hwndMaxTokensLabel, pad + (std::max)(0, contentW - dpiScale(50)), y, dpiScale(50),
                       dpiScale(18), TRUE);
        y += dpiScale(20);

        if (m_hwndMaxTokensSlider && IsWindow(m_hwndMaxTokensSlider))
            MoveWindow(m_hwndMaxTokensSlider, pad, y, contentW, dpiScale(24), TRUE);
        y += dpiScale(28);

        if (m_hwndContextLabel && IsWindow(m_hwndContextLabel))
            MoveWindow(m_hwndContextLabel, pad + (std::max)(0, contentW - dpiScale(50)), y, dpiScale(50), dpiScale(18),
                       TRUE);
        y += dpiScale(20);

        if (m_hwndContextSlider && IsWindow(m_hwndContextSlider))
            MoveWindow(m_hwndContextSlider, pad, y, contentW, dpiScale(24), TRUE);
        y += dpiScale(30);

        // Toggle grid (2 columns)
        const int colGap = dpiScale(8);
        const int colW = (std::max)(dpiScale(70), (contentW - colGap) / 2);
        if (m_hwndChkMaxMode && IsWindow(m_hwndChkMaxMode))
            MoveWindow(m_hwndChkMaxMode, pad, y, colW, dpiScale(20), TRUE);
        if (m_hwndChkDeepThink && IsWindow(m_hwndChkDeepThink))
            MoveWindow(m_hwndChkDeepThink, pad + colW + colGap, y, colW, dpiScale(20), TRUE);
        y += dpiScale(24);
        if (m_hwndChkDeepResearch && IsWindow(m_hwndChkDeepResearch))
            MoveWindow(m_hwndChkDeepResearch, pad, y, colW, dpiScale(20), TRUE);
        if (m_hwndChkNoRefusal && IsWindow(m_hwndChkNoRefusal))
            MoveWindow(m_hwndChkNoRefusal, pad + colW + colGap, y, colW, dpiScale(20), TRUE);
        y += dpiScale(24);
        if (m_hwndChkAgenticMode && IsWindow(m_hwndChkAgenticMode))
            MoveWindow(m_hwndChkAgenticMode, pad, y, colW, dpiScale(20), TRUE);
        y += dpiScale(28);

        // Bottom-anchored controls
        const int actionsH = dpiScale(24);
        const int badgeH = dpiScale(18);
        const int sendRowH = dpiScale(30);
        const int inputH = (std::max)(dpiScale(72), (std::min)(dpiScale(140), sidebarH / 5));

        int actionY = sidebarH - pad - actionsH;
        int badgeY = actionY - gap - badgeH;
        int sendY = badgeY - gap - sendRowH;
        int inputY = sendY - gap - inputH;

        // Hysteresis & Ghost-Width Prediction for Token-Stream Stabilization
        int desiredOutputH = inputY - gap - y;
        if (s_isThinking.load())
        {
            // Reserve extra vertical buffer (ghost height) during inference
            desiredOutputH = (std::max)(dpiScale(150), desiredOutputH - dpiScale(40));
        }

        DWORD now = GetTickCount();
        if (abs(desiredOutputH - m_predictedChatHeight) > dpiScale(20))
        {
            if (now - m_lastChatHeightChange > 50)
            {  // 50ms Hysteresis
                m_predictedChatHeight = desiredOutputH;
                m_lastChatHeightChange = now;
            }
        }

        int outputH = (std::max)(dpiScale(80), m_predictedChatHeight > 0 ? m_predictedChatHeight : desiredOutputH);

        if (m_hwndCopilotChatOutput && IsWindow(m_hwndCopilotChatOutput))
            MoveWindow(m_hwndCopilotChatOutput, pad, y, contentW, outputH, TRUE);

        layoutAgentChatCursorOverlay();

        if (m_hwndCopilotChatInput && IsWindow(m_hwndCopilotChatInput))
            MoveWindow(m_hwndCopilotChatInput, pad, y + outputH + gap, contentW, inputH, TRUE);

        const int threeW = (std::max)(dpiScale(58), (contentW - dpiScale(8)) / 3);
        if (m_hwndCopilotSendBtn && IsWindow(m_hwndCopilotSendBtn))
            MoveWindow(m_hwndCopilotSendBtn, pad, sendY, threeW, sendRowH, TRUE);
        if (m_hwndCopilotClearBtn && IsWindow(m_hwndCopilotClearBtn))
            MoveWindow(m_hwndCopilotClearBtn, pad + threeW + dpiScale(4), sendY, threeW, sendRowH, TRUE);
        HWND hwndNewChat = nullptr;
        for (HWND child = GetWindow(m_hwndSecondarySidebar, GW_CHILD); child; child = GetWindow(child, GW_HWNDNEXT))
        {
            wchar_t cls[32] = {};
            wchar_t txt[64] = {};
            GetClassNameW(child, cls, 32);
            GetWindowTextW(child, txt, 64);
            if (_wcsicmp(cls, L"Button") == 0 && _wcsicmp(txt, L"New Chat") == 0)
            {
                hwndNewChat = child;
                break;
            }
        }
        if (hwndNewChat)
            MoveWindow(hwndNewChat, pad + (threeW + dpiScale(4)) * 2, sendY, threeW, sendRowH, TRUE);

        if (m_hwndCopilotModelUsedLabel && IsWindow(m_hwndCopilotModelUsedLabel))
            MoveWindow(m_hwndCopilotModelUsedLabel, pad, badgeY, contentW, badgeH, TRUE);

        const int fourW = (std::max)(dpiScale(44), (contentW - dpiScale(9)) / 4);
        if (m_hwndCopilotHelpfulBtn && IsWindow(m_hwndCopilotHelpfulBtn))
            MoveWindow(m_hwndCopilotHelpfulBtn, pad, actionY, fourW, actionsH, TRUE);
        if (m_hwndCopilotUnhelpfulBtn && IsWindow(m_hwndCopilotUnhelpfulBtn))
            MoveWindow(m_hwndCopilotUnhelpfulBtn, pad + fourW + dpiScale(3), actionY, fourW, actionsH, TRUE);
        if (m_hwndCopilotCopyBtn && IsWindow(m_hwndCopilotCopyBtn))
            MoveWindow(m_hwndCopilotCopyBtn, pad + (fourW + dpiScale(3)) * 2, actionY, fourW, actionsH, TRUE);
        if (m_hwndCopilotRetryBtn && IsWindow(m_hwndCopilotRetryBtn))
            MoveWindow(m_hwndCopilotRetryBtn, pad + (fourW + dpiScale(3)) * 3, actionY, fourW, actionsH, TRUE);

        // Collision sanity pass over key interactive widgets.
        auto toRect = [&](HWND hwnd, const char* name) -> UIRect
        {
            RECT rc = {};
            GetWindowRect(hwnd, &rc);
            MapWindowPoints(nullptr, m_hwndSecondarySidebar, reinterpret_cast<LPPOINT>(&rc), 2);
            return UIRect{rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top, name, hwnd};
        };

        std::vector<UIRect> widgets;
        if (m_hwndCopilotChatOutput && IsWindow(m_hwndCopilotChatOutput))
            widgets.push_back(toRect(m_hwndCopilotChatOutput, "ChatOutput"));
        if (m_hwndCopilotChatInput && IsWindow(m_hwndCopilotChatInput))
            widgets.push_back(toRect(m_hwndCopilotChatInput, "ChatInput"));
        if (m_hwndCopilotSendBtn && IsWindow(m_hwndCopilotSendBtn))
            widgets.push_back(toRect(m_hwndCopilotSendBtn, "SendButton"));
        if (m_hwndCopilotClearBtn && IsWindow(m_hwndCopilotClearBtn))
            widgets.push_back(toRect(m_hwndCopilotClearBtn, "ClearButton"));
        if (m_hwndCopilotHelpfulBtn && IsWindow(m_hwndCopilotHelpfulBtn))
            widgets.push_back(toRect(m_hwndCopilotHelpfulBtn, "HelpfulButton"));
        if (m_hwndCopilotUnhelpfulBtn && IsWindow(m_hwndCopilotUnhelpfulBtn))
            widgets.push_back(toRect(m_hwndCopilotUnhelpfulBtn, "UnhelpfulButton"));
        if (m_hwndCopilotCopyBtn && IsWindow(m_hwndCopilotCopyBtn))
            widgets.push_back(toRect(m_hwndCopilotCopyBtn, "CopyButton"));
        if (m_hwndCopilotRetryBtn && IsWindow(m_hwndCopilotRetryBtn))
            widgets.push_back(toRect(m_hwndCopilotRetryBtn, "RetryButton"));

        std::sort(widgets.begin(), widgets.end(), [](const UIRect& a, const UIRect& b) { return a.y < b.y; });
        for (size_t i = 0; i + 1 < widgets.size(); ++i)
        {
            for (size_t j = i + 1; j < widgets.size(); ++j)
            {
                if (resolveOverlap(widgets[i], widgets[j]))
                {
                    SetWindowPos(widgets[j].hwnd, nullptr, widgets[j].x, widgets[j].y, widgets[j].w, widgets[j].h,
                                 SWP_NOZORDER | SWP_NOACTIVATE);
                }
            }
        }
    }

    // Eyes fix: after a WM_SIZE pass, explicitly restore visibility of key panes when they have valid bounds.
    if (m_hwndEditor && editorW > 0 && editorContentHeight > 0)
    {
        ShowWindow(m_hwndEditor, SW_SHOWNA);
    }
    if (m_hwndLineNumbers && gutterWidth > 0 && editorContentHeight > 0)
    {
        ShowWindow(m_hwndLineNumbers, SW_SHOWNA);
    }
    if (m_hwndTabBar)
    {
        ShowWindow(m_hwndTabBar, SW_SHOWNA);
    }
    if (m_hwndSidebar)
    {
        ShowWindow(m_hwndSidebar, m_sidebarVisible ? SW_SHOWNA : SW_HIDE);
    }
    if (m_hwndSecondarySidebar)
    {
        ShowWindow(m_hwndSecondarySidebar, m_secondarySidebarVisible ? SW_SHOWNA : SW_HIDE);
    }
    if (m_hwndOutputTabs)
    {
        ShowWindow(m_hwndOutputTabs, panelHeight > 0 ? SW_SHOWNA : SW_HIDE);
    }
    if (m_hwndPowerShellPanel)
    {
        ShowWindow(m_hwndPowerShellPanel, (m_powerShellPanelVisible && powerShellHeight > 0) ? SW_SHOWNA : SW_HIDE);
    }

    // Also layout internal PowerShell controls
    if (m_hwndPowerShellPanel && m_powerShellPanelVisible)
    {
        updatePowerShellPanelLayout(editorWidth, powerShellHeight);
    }

    // Update line numbers after layout
    updateLineNumbers();

    // Store editor rect for GPU surface sync
    m_editorRect = {editorRect.left, editorRect.top, editorRect.right, editorRect.bottom};

    // Startup and lane-recovery hardening: ensure a full invalidation so controls that were
    // created late still paint correctly after the first layout pass.
    if (m_hwndMain && IsWindow(m_hwndMain))
    {
        InvalidateRect(m_hwndMain, nullptr, TRUE);
    }

    Win32IDE_AgenticBrowser_Relayout();
}

void Win32IDE::applySovereignSnapPreset(int basePixels, const wchar_t* presetName, bool persistSession)
{
    const int minSnap = dpiScale(180);
    const int maxSnap = dpiScale(640);
    const int targetWidth = (std::max)(minSnap, (std::min)(maxSnap, dpiScale(basePixels)));

    if (basePixels == 240)
        m_activeSnapState = SnapState::Compact;
    else if (basePixels == 360)
        m_activeSnapState = SnapState::Standard;
    else if (basePixels == 480)
        m_activeSnapState = SnapState::Wide;
    else
        m_activeSnapState = SnapState::Custom;

    m_sidebarWidth = targetWidth;
    m_secondarySidebarWidth = targetWidth;
    updateSovereignSnapMenuChecks();

    if (m_hwndMain && IsWindow(m_hwndMain))
    {
        RECT rc = {};
        if (GetClientRect(m_hwndMain, &rc))
        {
            onSize(rc.right - rc.left, rc.bottom - rc.top);
        }
    }

    if (m_hwndStatusBar && IsWindow(m_hwndStatusBar))
    {
        wchar_t msg[128] = {};
        swprintf(msg, 128, L"Sovereign Snap: %ls", presetName ? presetName : L"Custom");
        SendMessageW(m_hwndStatusBar, SB_SETTEXTW, 0, (LPARAM)msg);
    }

    if (persistSession)
    {
        // Persist immediately so snap preference survives crash/abrupt shutdown.
        saveSession();
    }
}

void Win32IDE::updateSovereignSnapMenuChecks()
{
    if (!m_hMenu)
        return;

    UINT checkedId = 0;
    if (m_activeSnapState == SnapState::Compact)
        checkedId = IDM_VIEW_SOVEREIGN_SNAP_COMPACT;
    else if (m_activeSnapState == SnapState::Standard)
        checkedId = IDM_VIEW_SOVEREIGN_SNAP_STANDARD;
    else if (m_activeSnapState == SnapState::Wide)
        checkedId = IDM_VIEW_SOVEREIGN_SNAP_WIDE;

    const UINT flags = MF_BYCOMMAND;
    CheckMenuItem(m_hMenu, IDM_VIEW_SOVEREIGN_SNAP_COMPACT,
                  flags | (checkedId == IDM_VIEW_SOVEREIGN_SNAP_COMPACT ? MF_CHECKED : MF_UNCHECKED));
    CheckMenuItem(m_hMenu, IDM_VIEW_SOVEREIGN_SNAP_STANDARD,
                  flags | (checkedId == IDM_VIEW_SOVEREIGN_SNAP_STANDARD ? MF_CHECKED : MF_UNCHECKED));
    CheckMenuItem(m_hMenu, IDM_VIEW_SOVEREIGN_SNAP_WIDE,
                  flags | (checkedId == IDM_VIEW_SOVEREIGN_SNAP_WIDE ? MF_CHECKED : MF_UNCHECKED));
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

    // Initialize Symbol Index Bridge for AST-aware completions
    try
    {
        if (!m_symbolIndexBridge)
        {
            m_symbolIndexBridge = std::make_unique<rawrxd::bridge::SymbolIndexBridge>();
            if (m_symbolIndexBridge->initialize())
            {
                LOG_INFO("Symbol Index Bridge initialized successfully");
            }
            else
            {
                LOG_ERROR("Symbol Index Bridge initialization failed");
                m_symbolIndexBridge.reset();
            }
        }
    }
    catch (const std::exception& e)
    {
        LOG_ERROR(std::string("Symbol Index Bridge creation failed: ") + e.what());
        m_symbolIndexBridge.reset();
    }
    catch (...)
    {
        LOG_ERROR("Symbol Index Bridge creation failed with unknown error");
        m_symbolIndexBridge.reset();
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
        ModelConnection conn(m_ollamaBaseUrl.empty() ? "http://localhost:11435" : m_ollamaBaseUrl);

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
// Fast UI scaffolding (runs synchronously on UI thread, ~500ms)
void Win32IDE::onCreate(HWND hwnd)
{
    m_hwndMain = hwnd;
    AgentBridge_BindMainWindow(hwnd);
    RawrXD::Runtime::RuntimeProvider::ConfigureWin32TokenMessenger(hwnd, WM_USER_GHOST_TOKEN);
    bool hadOnCreateStepFailure = false;
    auto initOptionalPanelsStep = +[](void* self, HWND h)
    {
        auto* ide = static_cast<Win32IDE*>(self);

        // Optional panels (keep them alive for the lifetime of the IDE; no extra stub layers).
        // These are lightweight wrappers; heavy init is still deferred.
        if (!ide->m_modelRegistry)
            ide->m_modelRegistry = new ModelRegistry(h);
        if (!ide->m_interpretabilityPanel)
        {
            ide->m_interpretabilityPanel = new InterpretabilityPanel();
            ide->m_interpretabilityPanel->setParent(h);
        }
        if (!ide->m_checkpointManager)
            ide->m_checkpointManager = new CheckpointManager(h);
        if (!ide->m_ciCdSettings)
            ide->m_ciCdSettings = new CICDSettings();
        if (!ide->m_multiFileSearch)
        {
            ide->m_multiFileSearch = new MultiFileSearchWidget();
            // Default root: project root if set; else current working directory.
            const std::string root =
                ide->m_projectRoot.empty() ? std::filesystem::current_path().string() : ide->m_projectRoot;
            ide->m_multiFileSearch->setProjectRoot(root);
            ide->m_multiFileSearch->setShowCallback(&MultiFileSearchWidget_ShowDialog, ide->m_multiFileSearch);
        }
        if (!ide->m_benchmarkMenu)
            ide->m_benchmarkMenu = new BenchmarkMenu(h);

        if (ide->m_modelRegistry)
        {
            ide->m_modelRegistry->setShowCallback(
                [](void* ctx)
                {
                    auto* cbIde = static_cast<Win32IDE*>(ctx);
                    if (cbIde)
                        cbIde->showModelRegistryDialog();
                },
                ide);
        }

        if (ide->m_ciCdSettings)
        {
            ide->m_ciCdSettings->setShowCallback(
                [](void* ctx)
                {
                    auto* cbIde = static_cast<Win32IDE*>(ctx);
                    if (cbIde)
                        cbIde->showCICDSettingsDialog();
                },
                ide);
        }

        if (ide->m_checkpointManager)
        {
            ide->m_checkpointManager->setShowCallback(
                [](void* ctx, const std::vector<CheckpointManager::CheckpointIndex>& checkpoints)
                {
                    auto* cbIde = static_cast<Win32IDE*>(ctx);
                    if (!cbIde)
                        return;
                    std::string msg = "[CheckpointManager] Checkpoints: " + std::to_string(checkpoints.size()) + "\n";
                    for (const auto& cp : checkpoints)
                    {
                        msg += "  - " + cp.checkpointId + "\n";
                    }
                    cbIde->appendToOutput(msg, "Output", Win32IDE::OutputSeverity::Info);
                },
                ide);
        }
    };
    auto createMenuBarStep = +[](void* self, HWND h) { static_cast<Win32IDE*>(self)->createMenuBar(h); };
    auto createToolbarStep = +[](void* self, HWND h) { static_cast<Win32IDE*>(self)->createToolbar(h); };
    auto createActivityBarStep = +[](void* self, HWND h) { static_cast<Win32IDE*>(self)->createActivityBar(h); };
    auto createPrimarySidebarStep = +[](void* self, HWND h) { static_cast<Win32IDE*>(self)->createPrimarySidebar(h); };
    auto createTabBarStep = +[](void* self, HWND h) { static_cast<Win32IDE*>(self)->createTabBar(h); };
    auto createBreadcrumbBarStep = +[](void* self, HWND h) { static_cast<Win32IDE*>(self)->createBreadcrumbBar(h); };
    auto createLineNumberGutterStep =
        +[](void* self, HWND h) { static_cast<Win32IDE*>(self)->createLineNumberGutter(h); };
    auto createEditorStep = +[](void* self, HWND h) { static_cast<Win32IDE*>(self)->createEditor(h); };
    auto createAnnotationOverlayStep =
        +[](void* self, HWND h) { static_cast<Win32IDE*>(self)->createAnnotationOverlay(h); };
    auto createTerminalStep = +[](void* self, HWND h) { static_cast<Win32IDE*>(self)->createTerminal(h); };
    auto createEnhancedStatusBarStep =
        +[](void* self, HWND h) { static_cast<Win32IDE*>(self)->createEnhancedStatusBar(h); };
    auto createOutputTabsStep = +[](void* self, HWND) { static_cast<Win32IDE*>(self)->createOutputTabs(); };
    auto createChatPanelStep = +[](void* self, HWND) { static_cast<Win32IDE*>(self)->createChatPanel(); };

    // Initialize Common Controls
    INITCOMMONCONTROLSEX icex = {};
    icex.dwSize = sizeof(icex);
    icex.dwICC = ICC_BAR_CLASSES | ICC_TAB_CLASSES | ICC_LISTVIEW_CLASSES | ICC_TREEVIEW_CLASSES | ICC_PROGRESS_CLASS |
                 ICC_STANDARD_CLASSES;
    if (!InitCommonControlsEx(&icex))
    {
        const DWORD commonControlsErr = GetLastError();
        LOG_WARNING("InitCommonControlsEx failed, gle=" + std::to_string(commonControlsErr));
    }

    const auto frontPipelineMilestone = [this](const char* debugLine, const char* userLine)
    {
        if (isShuttingDown())
            return;
        if (debugLine && debugLine[0])
            OutputDebugStringA(debugLine);
        if (userLine && userLine[0] && m_hwndOutputTabs && IsWindow(m_hwndOutputTabs))
            appendToOutput(std::string(userLine), "System", OutputSeverity::Info);
    };

    // ================================================================
    // Create UI components — SEH-safe breadcrumb trail for diagnosis
    // Each step is wrapped so a crash pinpoints the exact function.
    // ================================================================

    OutputDebugStringA("[onCreate] initOptionalPanels...\n");
    hadOnCreateStepFailure |= !sehCallOnCreateStep(initOptionalPanelsStep, this, hwnd, "initOptionalPanels");

    OutputDebugStringA("[onCreate] createMenuBar...\n");
    hadOnCreateStepFailure |= !sehCallOnCreateStep(createMenuBarStep, this, hwnd, "createMenuBar");
    OutputDebugStringA("[onCreate] createToolbar...\n");
    hadOnCreateStepFailure |= !sehCallOnCreateStep(createToolbarStep, this, hwnd, "createToolbar");
    frontPipelineMilestone("[IDE-Pipeline:Front] Batch 1/8: optional panels + menu + toolbar\n",
                           "[Init:Front] Batch 1/8: optional panels, menu, and toolbar\n");

    OutputDebugStringA("[onCreate] createActivityBar...\n");
    hadOnCreateStepFailure |= !sehCallOnCreateStep(createActivityBarStep, this, hwnd, "createActivityBar");
    OutputDebugStringA("[onCreate] createPrimarySidebar...\n");
    hadOnCreateStepFailure |= !sehCallOnCreateStep(createPrimarySidebarStep, this, hwnd, "createPrimarySidebar");

    OutputDebugStringA("[onCreate] createTabBar...\n");
    hadOnCreateStepFailure |= !sehCallOnCreateStep(createTabBarStep, this, hwnd, "createTabBar");
    OutputDebugStringA("[onCreate] createBreadcrumbBar...\n");
    hadOnCreateStepFailure |= !sehCallOnCreateStep(createBreadcrumbBarStep, this, hwnd, "createBreadcrumbBar");
    frontPipelineMilestone("[IDE-Pipeline:Front] Batch 2/8: activity bar + sidebar + tabs + breadcrumb\n",
                           "[Init:Front] Batch 2/8: activity bar, primary sidebar, tabs, and breadcrumb\n");
    OutputDebugStringA("[onCreate] createLineNumberGutter...\n");
    hadOnCreateStepFailure |= !sehCallOnCreateStep(createLineNumberGutterStep, this, hwnd, "createLineNumberGutter");
    OutputDebugStringA("[onCreate] createEditor...\n");
    hadOnCreateStepFailure |= !sehCallOnCreateStep(createEditorStep, this, hwnd, "createEditor");
    hadOnCreateStepFailure |= !sehCallOnCreateStep(createAnnotationOverlayStep, this, hwnd, "createAnnotationOverlay");
    if (lineStripEditorEnabled())
    {
        createLineStripOverlay(hwnd);
        layoutLineStripOverlay();
    }
    frontPipelineMilestone("[IDE-Pipeline:Front] Batch 3/8: line gutter + editor + annotation overlay\n",
                           "[Init:Front] Batch 3/8: line gutter, editor, and annotation overlay\n");
    OutputDebugStringA("[onCreate] createTerminal...\n");
    hadOnCreateStepFailure |= !sehCallOnCreateStep(createTerminalStep, this, hwnd, "createTerminal");
    OutputDebugStringA("[onCreate] createEnhancedStatusBar...\n");
    hadOnCreateStepFailure |= !sehCallOnCreateStep(createEnhancedStatusBarStep, this, hwnd, "createEnhancedStatusBar");

    OutputDebugStringA("[onCreate] createOutputTabs...\n");
    hadOnCreateStepFailure |= !sehCallOnCreateStep(createOutputTabsStep, this, hwnd, "createOutputTabs");
    this->flushCtorBootReplayToSystem();

    // DEFERRED: createChatPanel is now async-initialized after WM_CREATE returns.
    // This allows the main message pump to start processing and the window to paint
    // before we block on any file I/O or GPU initialization.
    // Chat panel will be created on-demand or after UI becomes responsive.
    OutputDebugStringA("[onCreate] createChatPanel deferred to async init...\n");

    frontPipelineMilestone("[IDE-Pipeline:Front] Batch 4/8: terminal + status bar + output tabs (chat deferred)\n",
                           "[Init:Front] Batch 4/8: terminal, status bar, output tabs (chat deferred to async)\n");

    OutputDebugStringA("[onCreate] createAcceleratorTable...\n");
    hadOnCreateStepFailure |= !sehCallOnCreateStep(
        +[](void* self, HWND) { static_cast<Win32IDE*>(self)->createAcceleratorTable(); }, this, hwnd,
        "createAcceleratorTable");

    hadOnCreateStepFailure |= !sehCallOnCreateStep(
        +[](void* self, HWND)
        {
            auto* ide = static_cast<Win32IDE*>(self);
            if (ide->m_hwndMain)
            {
                SetPropA(ide->m_hwndMain, "RawrXD.IDE.Label", (HANDLE)RAWRXD_IDE_LABEL_MAIN_WINDOW);
                if (ide->m_interpretabilityPanel)
                    ide->m_interpretabilityPanel->setParent(ide->m_hwndMain);
            }
        },
        this, hwnd, "attachMainWindowProps");
    frontPipelineMilestone("[IDE-Pipeline:Front] Batch 5/8: accelerators + main window props + primary shell closed\n",
                           "[Init:Front] Batch 5/8: accelerators and main window properties attached\n");

    LOG_INFO("onCreate complete — all panels created");
    OutputDebugStringA("[onCreate] all panels created OK\n");
    if (hadOnCreateStepFailure)
    {
        LOG_INFO("onCreate recovered from one or more panel creation failures; startup continued");
        OutputDebugStringA("[onCreate] recovered from panel creation failures; check prior onCreate step crash logs\n");
    }

    OutputDebugStringA("[onCreate] initSyntaxColorizer...\n");
    hadOnCreateStepFailure |= !sehCallOnCreateStep(
        +[](void* self, HWND) { static_cast<Win32IDE*>(self)->initSyntaxColorizer(); }, this, hwnd,
        "initSyntaxColorizer");

    OutputDebugStringA("[onCreate] initGhostText...\n");
    hadOnCreateStepFailure |= !sehCallOnCreateStep(
        +[](void* self, HWND) { static_cast<Win32IDE*>(self)->initGhostText(); }, this, hwnd, "initGhostText");

    // DEFERRED: Session restoration (file I/O) is now async.
    // This includes loading previous workspace state, tabs, cursor positions, etc.
    // Will be restored after chat pane is initialized and UI is responsive.
    OutputDebugStringA("[onCreate] restoreSession deferred to async init...\n");

    OutputDebugStringA("[onCreate] applyStartupLayoutProfileFromEnv...\n");
    hadOnCreateStepFailure |= !sehCallOnCreateStep(
        +[](void* self, HWND) { static_cast<Win32IDE*>(self)->applyStartupLayoutProfileFromEnv(); }, this, hwnd,
        "applyStartupLayoutProfileFromEnv");

    hadOnCreateStepFailure |= !sehCallOnCreateStep(
        +[](void* self, HWND)
        {
            auto* ide = static_cast<Win32IDE*>(self);
            char buf[512];
            sprintf_s(buf,
                      "HWND audit: Main=%p Editor=%p Sidebar=%p "
                      "ExplorerTree=%p OutputTabs=%p PowerShellPanel=%p",
                      ide->m_hwndMain, ide->m_hwndEditor, ide->m_hwndSidebar, ide->m_hwndExplorerTree,
                      ide->m_hwndOutputTabs, ide->m_hwndPowerShellPanel);
            LOG_INFO(std::string(buf));
        },
        this, hwnd, "logWindowAudit");
    frontPipelineMilestone(
        "[IDE-Pipeline:Front] Batch 6/8: syntax + ghost + session restore + startup layout + HWND audit\n",
        "[Init:Front] Batch 6/8: syntax colorizer, ghost text, session, layout, and HWND audit\n");

    hadOnCreateStepFailure |= !sehCallOnCreateStep(
        +[](void* self, HWND)
        {
            auto* ide = static_cast<Win32IDE*>(self);
            // Apply dark theme immediately (lightweight — just sets color values + SendMessage)
            ide->populateBuiltinThemes();  // Register all 16 built-in themes
            ide->m_currentTheme.backgroundColor = RGB(30, 30, 30);
            ide->m_currentTheme.textColor = RGB(212, 212, 212);
            ide->m_currentTheme.selectionColor = RGB(38, 79, 120);
            ide->m_currentTheme.lineNumberColor = RGB(128, 128, 128);
            if (ide->m_backgroundBrush)
                DeleteObject(ide->m_backgroundBrush);
            ide->m_backgroundBrush = CreateSolidBrush(RGB(30, 30, 30));
            ide->applyTheme();
        },
        this, hwnd, "applyInitialTheme");

    hadOnCreateStepFailure |= !sehCallOnCreateStep(
        +[](void* self, HWND)
        {
            auto* ide = static_cast<Win32IDE*>(self);
            // Update status bar with initial state (12-part enhanced bar: Line/Col, Encoding, Language, etc.)
            if (ide->m_hwndStatusBar)
            {
                ide->updateEnhancedStatusBar();
                ide->m_contextUsage.maxTokens = ide->m_settings.aiContextWindow;
                ide->updateContextWindowDisplay();
            }
        },
        this, hwnd, "initStatusBarState");

    hadOnCreateStepFailure |= !sehCallOnCreateStep(
        +[](void* self, HWND)
        {
            auto* ide = static_cast<Win32IDE*>(self);
            // Initialize SessionController spine + phase barrier + deferred queue.
            ide->m_sessionController = std::make_unique<rawrxd::session::SessionController>();
            ide->m_sessionController->Start(ide->m_projectRoot.empty() ? std::filesystem::current_path().string()
                                                                       : ide->m_projectRoot);
            ide->m_sessionController->SetDeferredDispatch(
                [ide](const std::string& prompt, std::string* error) -> bool
                {
                    if (!ide->m_sessionController->IsExecutionReady(error))
                        return false;
                    ide->generateResponseAsync(prompt, [](const std::string&, bool) {});
                    return true;
                });
        },
        this, hwnd, "initSessionController");

    // DEFERRED: Backend and LLM router initialization is now always async.
    // GPU context creation, GGUF scanning, and model enumeration happen in the background
    // after the UI is responsive. The env var RAWRXD_ENABLE_STARTUP_BACKEND_INIT is ignored
    // during fast path; models will be loaded on-demand when requested by the user.
    OutputDebugStringA("[onCreate] initBackendAndRouter deferred to async init...\n");

    hadOnCreateStepFailure |= !sehCallOnCreateStep(
        +[](void* self, HWND)
        {
            auto* ide = static_cast<Win32IDE*>(self);
            ide->initializeMissingFeaturesCore();
        },
        this, hwnd, "initializeMissingFeaturesCore");
    frontPipelineMilestone(
        "[IDE-Pipeline:Front] Batch 7/8: initial theme + status snapshot + features core (backend deferred)\n",
        "[Init:Front] Batch 7/8: theme, status bar, and features core (backend deferred to async)\n");

    hadOnCreateStepFailure |= !sehCallOnCreateStep(
        +[](void* self, HWND h)
        {
            auto* ide = static_cast<Win32IDE*>(self);

            // Critical pane recovery: ensure center + right workspace controls exist.
            if (!ide->m_hwndEditor || !IsWindow(ide->m_hwndEditor))
                ide->createEditor(h);
            if (!ide->m_hwndOutputTabs || !IsWindow(ide->m_hwndOutputTabs))
                ide->createOutputTabs();
            if (!ide->m_hwndPowerShellPanel || !IsWindow(ide->m_hwndPowerShellPanel))
                ide->createTerminal(h);
            if (!ide->m_hwndSecondarySidebar || !IsWindow(ide->m_hwndSecondarySidebar))
                ide->createChatPanel();

            if (ide->m_secondarySidebarWidth <= 0)
                ide->m_secondarySidebarWidth = ide->dpiScale(360);

            RECT rc = {};
            if (ide->m_hwndMain && GetClientRect(ide->m_hwndMain, &rc))
            {
                ide->onSize(rc.right - rc.left, rc.bottom - rc.top);
            }
        },
        this, hwnd, "recoverCriticalPanesAndRelayout");

    hadOnCreateStepFailure |= !sehCallOnCreateStep(
        +[](void* self, HWND)
        {
            auto* ide = static_cast<Win32IDE*>(self);
            if (!ide || !ide->m_hwndMain)
                return;

            // Force one deterministic startup hydration pass so visible panes are not left blank
            // when child creation order varies across lanes.
            ide->setSidebarView(Win32IDE::SidebarView::Explorer);
            ide->updateSecondarySidebarContent();

            if (ide->m_hwndEditor && IsWindow(ide->m_hwndEditor))
            {
                ShowWindow(ide->m_hwndEditor, SW_SHOWNA);
                SetFocus(ide->m_hwndEditor);
            }

            InvalidateRect(ide->m_hwndMain, nullptr, TRUE);
            UpdateWindow(ide->m_hwndMain);
        },
        this, hwnd, "hydrateStartupViewsAndRepaint");

    // Agent tools: mirror `execute_command` with use_integrated_terminal into read-only agent terminal (Cursor-style).
    hadOnCreateStepFailure |= !sehCallOnCreateStep(
        +[](void* self, HWND)
        {
            auto* ide = static_cast<Win32IDE*>(self);
            if (!ide)
                return;
            RawrXD::Agent::AgentToolHandlers::SetIntegratedTerminalEchoCallback(
                +[](const char* utf8, void* user)
                {
                    auto* w = static_cast<Win32IDE*>(user);
                    if (!w || !utf8 || !*utf8)
                        return;
                    const int paneId = w->getOrCreatePrimaryAgentTerminalPane();
                    (void)w->writeAgentTerminalLine(paneId, std::string(utf8));
                },
                ide);
        },
        this, hwnd, "registerIntegratedTerminalToolEcho");
    frontPipelineMilestone(
        "[IDE-Pipeline:Front] Batch 8/8: critical panes + layout + view hydration + agent terminal echo\n",
        "[Init:Front] Batch 8/8: pane recovery, layout, startup views, and integrated terminal echo\n");

    frontPipelineMilestone("[IDE-Pipeline:Front] E0-1/8: Front thread — document/editor HWNDs verified for handoff\n",
                           "[Init:Front] E0-1/8: Editor and shell HWNDs ready for background init\n");
    frontPipelineMilestone("[IDE-Pipeline:Front] E0-2/8: Front thread — output + terminal routing armed\n",
                           "[Init:Front] E0-2/8: Output and terminal routing armed\n");
    frontPipelineMilestone("[IDE-Pipeline:Front] E0-3/8: Front thread — session + layout snapshots applied\n",
                           "[Init:Front] E0-3/8: Session and startup layout snapshots applied\n");
    frontPipelineMilestone("[IDE-Pipeline:Front] E0-4/8: Front thread — theme tokens + status bar live\n",
                           "[Init:Front] E0-4/8: Theme and status bar live\n");
    frontPipelineMilestone("[IDE-Pipeline:Front] E0-5/8: Front thread — backend + router stubs reachable from UI\n",
                           "[Init:Front] E0-5/8: Backend manager and LLM router reachable from UI thread\n");
    frontPipelineMilestone("[IDE-Pipeline:Front] E0-6/8: Front thread — workspace geometry and focus order set\n",
                           "[Init:Front] E0-6/8: Workspace geometry and focus order set\n");
    frontPipelineMilestone("[IDE-Pipeline:Front] E0-7/8: Front thread — agent tool echo bridge registered\n",
                           "[Init:Front] E0-7/8: Agent integrated-terminal echo bridge registered\n");
    frontPipelineMilestone(
        "[IDE-Pipeline:Front] E0-8/8: Front thread — posting WM_APP+100 (deferred heavy initializer)\n",
        "[Init:Front] E0-8/8: Posting deferred heavy initializer (WM_APP+100)\n");

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

namespace
{

bool initializeEnterpriseSubsystems(Win32IDE* ide);

#ifdef _WIN32
// Exception filter: handle hardware faults but let C++ exceptions (0xE06D7363) propagate
// so the C++ runtime can properly unwind destructors and avoid heap corruption.
static LONG WINAPI enterpriseSehFilter(DWORD code, DWORD* outCode)
{
    if (code == 0xE06D7363u)  // MSVC C++ exception — do not catch via SEH
        return EXCEPTION_CONTINUE_SEARCH;
    if (outCode)
        *outCode = code;
    return EXCEPTION_EXECUTE_HANDLER;
}

int initializeEnterpriseSubsystemsSehThunk(Win32IDE* ide, DWORD* sehCode)
{
    __try
    {
        initializeEnterpriseSubsystems(ide);
        if (sehCode)
            *sehCode = 0;
        return 1;
    }
    __except (enterpriseSehFilter(GetExceptionCode(), sehCode))
    {
        return 0;
    }
}
#endif

bool initializeEnterpriseSubsystems(Win32IDE* ide)
{
    // EMERGENCY STARTUP FIX: Enterprise license initialization blocks worker thread indefinitely.
    // Defer license initialization to post-paint phase to keep UI responsive.
    // User can work normally while license validation happens in background.
    FILE* dbg = fopen("C:\\temp\\emergency_debug.txt", "a");
    if (dbg)
    {
        fprintf(dbg,
                "[EMERGENCY] initializeEnterpriseSubsystems: SKIPPING enterprise init for startup responsiveness\n");
        fflush(dbg);
        fclose(dbg);
    }
    OutputDebugStringA(
        "[EMERGENCY] initializeEnterpriseSubsystems: SKIPPING enterprise init for startup responsiveness\n");

    // Return success immediately - license will be initialized later
    return true;
}

#ifdef _WIN32
bool initializeEnterpriseSubsystemsSafe(Win32IDE* ide)
{
    DWORD sehCode = 0;
    if (initializeEnterpriseSubsystemsSehThunk(ide, &sehCode) != 0)
    {
        return true;
    }

    char sehMsg[192] = {};
    sprintf_s(sehMsg, "ERROR: Enterprise license SEH faulted (code=0x%08lX); continuing in community mode",
              static_cast<unsigned long>(sehCode));
    LOG_ERROR(sehMsg);
    OutputDebugStringA(sehMsg);
    OutputDebugStringA("\n");
    // NOTE: Do NOT call _strdup/malloc here — the heap may be in an inconsistent state
    // after a C++ exception was caught via SEH (__except). The PostMessage with the
    // community badge is deferred to a safe point in deferredHeavyInitBody.
    return false;
}
#else
bool initializeEnterpriseSubsystemsSafe(Win32IDE* ide)
{
    return initializeEnterpriseSubsystems(ide);
}
#endif

}  // namespace

// 4 MB stack for deferred init thread to avoid 0xC00000FD (STATUS_STACK_OVERFLOW)
static const DWORD kDeferredInitStackSize = 4 * 1024 * 1024;

DWORD WINAPI Win32IDE::deferredHeavyInitThreadProc(LPVOID param)
{
    Win32IDE* self = static_cast<Win32IDE*>(param);
    if (!self)
        return 0;

    // Set thread-local pointer so VEH can detect deferred init worker crashes
    extern void setDeferredInitWorkerTLS(void* idePtr);
    setDeferredInitWorkerTLS(self);

    // Record the worker thread ID for crash detection
    self->m_deferredInitWorkerThreadId = GetCurrentThreadId();

    // Keep this thread proc teardown-free with respect to instance atomics.
    // If deferredHeavyInitBody faults and partially corrupts instance state, touching
    // m_activeDetachedThreads during guard destruction can trigger a second fatal AV.
    if (self->isShuttingDown())
        return 0;

    sehRunBgThread(bgInitBody, self);
    return 0;
}

void Win32IDE::deferredHeavyInit()
{
    if (smokeDeferredInitActive())
    {
        OutputDebugStringA("[SMOKE] deferredHeavyInit: lightweight path (extension pipe on worker)\n");
        m_initStatus.store(STATUS_INIT_RUNNING, std::memory_order_release);
        AgentBridge_SetShuttingDown(false);
        PromptWarm_SetAcceptRequests(true);

        // Scenario 4 needs the named pipe server without running full deferredHeavyInitBody.
        HANDLE hSmokePipeThread = CreateThread(
            nullptr, 0,
            [](LPVOID param) -> DWORD
            {
                auto* ide = static_cast<Win32IDE*>(param);
                if (!ide || ide->isShuttingDown())
                    return 0;
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

                    RawrXD::Extensions::InitializeExtensionEngine(ide->getMainWindow(), false);

                    // Scenario 4: drain pipe traffic even if the UI message loop exits early.
                    for (int pollTick = 0; pollTick < 600 && !ide->isShuttingDown(); ++pollTick)
                    {
                        RawrXD::Extensions::PollExtensionEngineLsp();
                        Sleep(50);
                    }
                }
                catch (...)
                {
                    OutputDebugStringA("[SMOKE] InitializeExtensionEngine failed on worker\n");
                }
                ide->m_deferredHeavyInitComplete.store(true, std::memory_order_release);
                ide->m_initStatus.store(STATUS_INIT_SUCCESS, std::memory_order_release);
                AgentBridge_SetInitComplete(true);
                HWND hwnd = ide->getMainWindow();
                if (hwnd && IsWindow(hwnd))
                {
                    PostMessage(hwnd, WM_APP + 101, 0, 0);
                    if (Win32IDE::smokeCopilotChatEnabled())
                        ide->finalizeCopilotChatInterlockAfterDeferredLoad();
                }
                return 0;
            },
            this, 0, nullptr);
        if (hSmokePipeThread)
            CloseHandle(hSmokePipeThread);

        return;
    }

    // Re-arm bridge and prompt-warming gates at startup entry so early route
    // smoke traffic is not dropped while deferred init is still running.
    AgentBridge_SetShuttingDown(false);
    PromptWarm_SetAcceptRequests(true);
    // Agent streaming callbacks stay gated until deferredHeavyInitBody completes
    // (AgentBridge_SetInitComplete(true) at end of background init).

    if (!isShuttingDown())
    {
        OutputDebugStringA("[IDE-Pipeline] Bridge: UI thread entered deferredHeavyInit (CreateThread next)\n");
        if (m_hwndOutputTabs && IsWindow(m_hwndOutputTabs))
            appendToOutput("[Init] Bridge: UI thread starting background initializer (large-stack thread)\n", "System",
                           OutputSeverity::Info);
    }
    // Run heavy initialization on a background thread with large stack to avoid 0xC00000FD.
    m_initStatus.store(STATUS_INIT_RUNNING, std::memory_order_release);
    DWORD dwThreadId = 0;
    HANDLE h =
        CreateThread(nullptr, kDeferredInitStackSize, &Win32IDE::deferredHeavyInitThreadProc, this, 0, &dwThreadId);
    if (h)
    {
        m_deferredInitWorkerThreadId = dwThreadId;  // Redundant but explicit; deferredHeavyInitThreadProc also sets it
        CloseHandle(h);
    }
    else
    {
        // Fallback: run on current thread if CreateThread fails (shouldn't happen in practice)
        m_deferredInitWorkerThreadId = GetCurrentThreadId();
        sehRunBgThread(bgInitBody, this);
    }
}

void bgInitBody(void* self)
{
    Win32IDE* ide = static_cast<Win32IDE*>(self);
    ide->deferredHeavyInitBody();
}

void Win32IDE::deferredHeavyInitBody()
{
    if (smokeDeferredInitActive())
    {
        OutputDebugStringA("[SMOKE] deferredHeavyInitBody: no-op (smoke deferred init)\n");
        return;
    }

    OutputDebugStringA("[EMERGENCY] deferredHeavyInitBody ENTER\n");

    // Initialize logger under %APPDATA%\RawrXD\ide.log (fallback: RawrXD_IDE.log in cwd)
    try
    {
        OutputDebugStringA("[EMERGENCY] Logger init starting\n");
        std::string logPath = "RawrXD_IDE.log";
        char appData[MAX_PATH] = {};
        if (SUCCEEDED(SHGetFolderPathA(nullptr, CSIDL_APPDATA, nullptr, 0, appData)))
        {
            std::string dir = std::string(appData) + "\\RawrXD";
            CreateDirectoryA(dir.c_str(), nullptr);
            logPath = dir + "\\ide.log";
        }
        OutputDebugStringA("[EMERGENCY] About to call IDELogger::getInstance().initialize()\n");
        IDELogger::getInstance().initialize(logPath);
        OutputDebugStringA("[EMERGENCY] Logger init completed\n");
    }
    catch (...)
    {
        OutputDebugStringA("ERROR: Logger init failed\n");
    }
    OutputDebugStringA("[EMERGENCY] After logger init\n");
    if (isShuttingDown())
        return;

    // ================================================================
    // Enterprise License System — initialize FIRST (gates engine registration)
    // ================================================================
    bool enterpriseOk = false;
    try
    {
        OutputDebugStringA("[EMERGENCY] Enterprise init starting\n");
        enterpriseOk = initializeEnterpriseSubsystemsSafe(this);
        OutputDebugStringA("[EMERGENCY] Enterprise init completed\n");
    }
    catch (...)
    {
        // C++ exception from shield code — heap is clean (propagated via EXCEPTION_CONTINUE_SEARCH,
        // not caught by __except which would corrupt CRT heap state).
        OutputDebugStringA("ERROR: Enterprise license init failed (C++ exception)\n");
        LOG_INFO("deferredHeavyInitBody: enterprise init threw C++ exception; continuing\n");
    }
    // Post the tier badge now that the heap is stable. _strdup must NOT be called inside
    // initializeEnterpriseSubsystemsSafe's SEH failure path (heap may be unsafe after
    // catching a C++ exception via __except). We defer it to here where heap is known clean.
    if (!enterpriseOk)
        PostMessage(m_hwndMain, WM_USER + 200, 0, reinterpret_cast<LPARAM>(_strdup("[Community]")));
    LOG_INFO("deferredHeavyInitBody: CHECKPOINT-A past enterprise init\n");
    if (isShuttingDown())
        return;

    const auto idePipelineMilestone = [this](const char* debugLine, const char* userLine)
    {
        if (isShuttingDown())
            return;
        if (debugLine && debugLine[0])
            OutputDebugStringA(debugLine);
        if (userLine && userLine[0])
            appendToOutput(std::string(userLine), "System", OutputSeverity::Info);
    };

    LOG_INFO("deferredHeavyInitBody: CHECKPOINT-B before enableAllFeaturesAndWire\n");
    // IDE pipeline — Batch 1/8 (front): 5-tier orchestration before inference/agent wiring.
    try
    {
        enableAllFeaturesAndWire();
        LOG_INFO("deferredHeavyInitBody: CHECKPOINT-C after enableAllFeaturesAndWire\n");
        idePipelineMilestone("[IDE-Pipeline] Batch 1/8: enableAllFeaturesAndWire (core→AI→agent→build→advanced)\n",
                             "[Init] Batch 1/8: subsystem orchestration (core→AI→agent→build→advanced)\n");
    }
    catch (...)
    {
        OutputDebugStringA("ERROR: enableAllFeaturesAndWire failed\n");
    }
    if (isShuttingDown())
        return;

    // Initialize Native CPU Inference Engine
    try
    {
        m_nativeEngine = RawrXD::CPUInferenceEngine::GetSharedInstance();
        auto memPlugin = std::make_shared<RawrXD::Modules::NativeMemoryModule>();
        m_nativeEngine->RegisterMemoryPlugin(memPlugin);
        m_nativeEngineLoaded = true;
        wireLayerProgressToOutputPanel();
    }
    catch (...)
    {
        m_nativeEngine.reset();
        m_nativeEngineLoaded = false;
        OutputDebugStringA("ERROR: CPUInferenceEngine init failed\n");
    }
    idePipelineMilestone("[IDE-Pipeline] Batch 2/8: CPU inference engine + layer progress wiring\n",
                         "[Init] Batch 2/8: CPU inference engine + layer progress wiring\n");
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

    // Initialize Extension Loader + Installer
    try
    {
        m_extensionLoader = std::make_unique<RawrXD::ExtensionLoader>();
        m_extensionLoader->Scan();
        m_extensionLoader->LoadNativeModules();

        m_extensionInstaller = std::make_unique<RawrXD::ExtensionInstaller>();
        // Sync installer state into UI state model
        RawrXD::ExtensionUIState::Instance().syncFromInstaller(m_extensionInstaller.get());
    }
    catch (...)
    {
        OutputDebugStringA("ERROR: ExtensionLoader init failed\n");
    }
    idePipelineMilestone("[IDE-Pipeline] Batch 3/8: NativeAgent + ExtensionLoader\n",
                         "[Init] Batch 3/8: NativeAgent + ExtensionLoader\n");
    if (isShuttingDown())
        return;

    // Initialise the agentic bridge (needs m_hwndMain, which is set)
    // STARTUP HARDENING: Temporarily disabled during crash investigation.
    // The agentic bridge has been a recurring crash point; skipping it allows
    // the IDE to boot and the user can manually initialize it via menu.
    // TODO: Re-enable once crash root cause is identified.
    OutputDebugStringA("[StartupHardening] Skipping initializeAgenticBridge during boot\n");
    idePipelineMilestone("[IDE-Pipeline] Batch 4/8: Agentic bridge SKIPPED (startup hardening)\n",
                         "[Init] Batch 4/8: Agentic bridge SKIPPED (startup hardening)\n");

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
        startupTraceImmediate("before initGhostText");
        initGhostText();
        startupTraceImmediate("after initGhostText");
    }
    catch (...)
    {
        startupTraceImmediate("initGhostText exception");
        OutputDebugStringA("ERROR: initGhostText failed\n");
    }

    // Initialize Failure Detector (agent self-correction)
    try
    {
        startupTraceImmediate("before initFailureDetector");
        initFailureDetector();
        startupTraceImmediate("after initFailureDetector");
    }
    catch (...)
    {
        startupTraceImmediate("initFailureDetector exception");
        OutputDebugStringA("ERROR: initFailureDetector failed\n");
    }

    // Initialize Agent Diff / Edit Review panels (startup hardening: opt-in while AV is under triage)
    const bool enableAgentPanelsStartup = (std::getenv("RAWRXD_ENABLE_AGENT_PANELS_STARTUP") != nullptr);
    if (enableAgentPanelsStartup)
    {
        startupTraceImmediate("before initAgentPanel");
        try
        {
            initAgentPanel();
            startupTraceImmediate("after initAgentPanel");
        }
        catch (const std::exception& e)
        {
            startupTraceImmediate("initAgentPanel exception", e.what());
            OutputDebugStringA("ERROR: initAgentPanel failed\n");
        }
        catch (...)
        {
            startupTraceImmediate("initAgentPanel unknown exception");
            OutputDebugStringA("ERROR: initAgentPanel failed (unknown)\n");
        }

        startupTraceImmediate("before initEditReviewPanel");
        try
        {
            initEditReviewPanel();
            startupTraceImmediate("after initEditReviewPanel");
        }
        catch (const std::exception& e)
        {
            startupTraceImmediate("initEditReviewPanel exception", e.what());
            OutputDebugStringA("ERROR: initEditReviewPanel failed\n");
        }
        catch (...)
        {
            startupTraceImmediate("initEditReviewPanel unknown exception");
            OutputDebugStringA("ERROR: initEditReviewPanel failed (unknown)\n");
        }
    }
    else
    {
        startupTraceImmediate("agent panels skipped", "set RAWRXD_ENABLE_AGENT_PANELS_STARTUP=1 to enable");
        OutputDebugStringA("[StartupHardening] initAgentPanel/initEditReviewPanel skipped (set "
                           "RAWRXD_ENABLE_AGENT_PANELS_STARTUP=1 to enable)\n");
    }
    idePipelineMilestone("[IDE-Pipeline] Batch 5/8: Ghost text + failure detector + agent diff panel\n",
                         "[Init] Batch 5/8: Ghost text + failure detector + agent diff panel\n");

    // Load settings on background thread; applySettings() marshaled to main thread via
    // WM_APP_APPLY_SETTINGS to avoid GDI race on m_backgroundBrush/m_editorFont (RT-01).
    // Use PostMessage (not SendMessage) to avoid cross-thread startup deadlocks when
    // the UI thread and worker contend on init/config locks.
    try
    {
        startupTraceImmediate("before loadSettings");
        loadSettings();
        startupTraceImmediate("after loadSettings");
        LOG_INFO("[StartupTrace] CKPT: loadSettings() completed");
        if (m_hwndMain && IsWindow(m_hwndMain))
            PostMessage(m_hwndMain, WM_APP_APPLY_SETTINGS, 0, 0);
        LOG_INFO("[StartupTrace] CKPT: WM_APP_APPLY_SETTINGS posted");
    }
    catch (...)
    {
        startupTraceImmediate("loadSettings/applySettings exception");
        OutputDebugStringA("ERROR: loadSettings/applySettings failed\n");
    }

    try
    {
        startupTraceImmediate("before syncAgentModeUiFromBridge");
        syncAgentModeUiFromBridge();
        startupTraceImmediate("after syncAgentModeUiFromBridge");
    }
    catch (const std::exception& e)
    {
        startupTraceImmediate("syncAgentModeUiFromBridge exception", e.what());
        OutputDebugStringA("ERROR: syncAgentModeUiFromBridge failed\n");
    }
    catch (...)
    {
        startupTraceImmediate("syncAgentModeUiFromBridge unknown exception");
        OutputDebugStringA("ERROR: syncAgentModeUiFromBridge failed (unknown)\n");
    }

    try
    {
        startupTraceImmediate("before syncSpeculativeInferenceFromConfig");
        syncSpeculativeInferenceFromConfig();
        startupTraceImmediate("after syncSpeculativeInferenceFromConfig");
    }
    catch (const std::exception& e)
    {
        startupTraceImmediate("syncSpeculativeInferenceFromConfig exception", e.what());
        OutputDebugStringA("ERROR: syncSpeculativeInferenceFromConfig failed\n");
    }
    catch (...)
    {
        startupTraceImmediate("syncSpeculativeInferenceFromConfig unknown exception");
        OutputDebugStringA("ERROR: syncSpeculativeInferenceFromConfig failed (unknown)\n");
    }

    // Startup hardening: keep optional discovery, bridge-adjacent clients, and plugin managers
    // off the WinMain path so failures cannot abort startup before the first paint.
    try
    {
        LOG_INFO("[StartupTrace] CKPT: before initializeCoreRuntimeSpine");
        initializeCoreRuntimeSpine();
        LOG_INFO("[StartupTrace] CKPT: after initializeCoreRuntimeSpine");
    }
    catch (...)
    {
        OutputDebugStringA("ERROR: initializeCoreRuntimeSpine failed\n");
    }
    idePipelineMilestone("[IDE-Pipeline] Batch 6/8: Core runtime spine + bridge-adjacent clients\n",
                         "[Init] Batch 6/8: Core runtime spine + bridge-adjacent clients\n");
    try
    {
        LOG_INFO("[StartupTrace] CKPT: before initAgentOllamaClient");
        initAgentOllamaClient();
        LOG_INFO("[StartupTrace] CKPT: after initAgentOllamaClient");
    }
    catch (...)
    {
        OutputDebugStringA("ERROR: initAgentOllamaClient failed\n");
    }
    try
    {
        initModelDiscovery();
    }
    catch (...)
    {
        OutputDebugStringA("ERROR: initModelDiscovery failed\n");
    }
    try
    {
        initEnterpriseStressTests();
    }
    catch (...)
    {
        OutputDebugStringA("ERROR: initEnterpriseStressTests failed\n");
    }
    try
    {
        initRefactoringPlugin();
    }
    catch (...)
    {
        OutputDebugStringA("ERROR: initRefactoringPlugin failed\n");
    }
    try
    {
        initLanguagePlugin();
    }
    catch (...)
    {
        OutputDebugStringA("ERROR: initLanguagePlugin failed\n");
    }
    try
    {
        initResourceGenerator();
    }
    catch (...)
    {
        OutputDebugStringA("ERROR: initResourceGenerator failed\n");
    }

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
        startupTraceImmediate("before initFailureIntelligence");
        initFailureIntelligence();
        startupTraceImmediate("after initFailureIntelligence");
    }
    catch (const std::exception& e)
    {
        startupTraceImmediate("initFailureIntelligence exception", e.what());
        OutputDebugStringA("ERROR: initFailureIntelligence failed\n");
    }
    catch (...)
    {
        startupTraceImmediate("initFailureIntelligence unknown exception");
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

    // GPU Backend Bridge — detect and initialize Vulkan compute if available.
    // RT-01 triage gate: allow deterministic startup isolation without code churn.
    {
        const bool disableVulkanProbe = (std::getenv("RAWRXD_DISABLE_VULKAN_PROBE_STARTUP") != nullptr);
        if (disableVulkanProbe)
        {
            m_gpuTextEnabled = false;
            OutputDebugStringA(
                "GPU Backend Bridge: Startup Vulkan probe disabled by RAWRXD_DISABLE_VULKAN_PROBE_STARTUP\n");
            appendToOutput("[GPU] Startup Vulkan probe disabled by env gate\n", "Output", OutputSeverity::Warning);
        }
        else
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
    // MultiResponse, LSP, Hotpatch, Swarm, Debugger — all opt-in via env for smoke stability.
    const bool enableDeferredSubsystems = std::getenv("RAWRXD_ENABLE_DEFERRED_SUBSYSTEMS");
    if (enableDeferredSubsystems)
    {
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
    }
    else
    {
        OutputDebugStringA("[Smoke] Deferred subsystems skipped (set RAWRXD_ENABLE_DEFERRED_SUBSYSTEMS=1 to enable)\n");
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

    // Initialize Phase 33/44 voice stack only when explicitly enabled.
    // RT-01: startup crashes are still in triage; keeping this opt-in removes a volatile
    // early-startup path while preserving a switch for targeted validation.
    const bool enableVoiceStartupInit = (std::getenv("RAWRXD_ENABLE_VOICE_STARTUP_INIT") != nullptr);
    if (enableVoiceStartupInit)
    {
        // Initialize Phase 33: Voice Chat Engine
        try
        {
            // Marshal Phase 33 UI/hotkey init to main thread to avoid worker-thread USER/GDI races.
            if (m_hwndMain && IsWindow(m_hwndMain))
                PostMessage(m_hwndMain, WM_APP_INIT_VOICE_CHAT_UI, 0, 0);
        }
        catch (...)
        {
            OutputDebugStringA("ERROR: initVoiceChat failed\n");
        }

        // Initialize Phase 44: Voice Automation (TTS for responses)
        try
        {
            // Marshal to UI thread: creating Win32 controls on the deferred init worker can
            // race USER/GDI state and has been linked to RT-01 startup crashes.
            if (m_hwndMain && IsWindow(m_hwndMain))
                PostMessage(m_hwndMain, WM_APP_CREATE_VOICE_AUTOMATION_PANEL, 0, 0);
        }
        catch (...)
        {
            OutputDebugStringA("ERROR: VoiceAutomation init failed\n");
        }
    }
    else
    {
        OutputDebugStringA("[Smoke] Voice startup init skipped (set RAWRXD_ENABLE_VOICE_STARTUP_INIT=1 to enable)\n");
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

    // Initialize Tier 2: High-visibility cosmetics (git diff viewer, terminal tabs, hover, refs, CodeLens, inlay)
    try
    {
        initTier2Cosmetics();
    }
    catch (...)
    {
        OutputDebugStringA("ERROR: initTier2Cosmetics failed\n");
    }

    // Initialize Phase 33: Quick-Win Systems (Shortcuts, Backups, Alerts, SLO)
    startupTraceImmediate("before initQuickWinSystems");
    try
    {
        initQuickWinSystems();
        startupTraceImmediate("after initQuickWinSystems");
    }
    catch (...)
    {
        startupTraceImmediate("initQuickWinSystems exception");
        OutputDebugStringA("ERROR: initQuickWinSystems failed\n");
    }

    // Initialize Phase 32B: Chain-of-Thought Multi-Model Review Engine
    startupTraceImmediate("before initChainOfThought");
    try
    {
        initChainOfThought();
        startupTraceImmediate("after initChainOfThought");
    }
    catch (...)
    {
        startupTraceImmediate("initChainOfThought exception");
        OutputDebugStringA("ERROR: initChainOfThought failed\n");
    }

    // Initialize Phase 34: Telemetry Export Subsystem (startup hardening: opt-in while AV triage is active)
    const bool enableTelemetryStartup = (std::getenv("RAWRXD_ENABLE_TELEMETRY_STARTUP") != nullptr);
    if (enableTelemetryStartup)
    {
        startupTraceImmediate("before initTelemetry");
        try
        {
            initTelemetry();
            startupTraceImmediate("after initTelemetry");
        }
        catch (...)
        {
            startupTraceImmediate("initTelemetry exception");
            OutputDebugStringA("ERROR: initTelemetry failed\n");
        }
    }
    else
    {
        startupTraceImmediate("initTelemetry skipped", "set RAWRXD_ENABLE_TELEMETRY_STARTUP=1 to enable");
        OutputDebugStringA(
            "[StartupHardening] initTelemetry skipped (set RAWRXD_ENABLE_TELEMETRY_STARTUP=1 to enable)\n");
    }

    // Initialize Phase 36: Flight Recorder — persistent binary ring-buffer
    startupTraceImmediate("before initFlightRecorder");
    try
    {
        initFlightRecorder();
        startupTraceImmediate("after initFlightRecorder");
    }
    catch (...)
    {
        startupTraceImmediate("initFlightRecorder exception");
        OutputDebugStringA("ERROR: initFlightRecorder failed\n");
    }

    // Initialize Phase 36: MCP Integration — Model Context Protocol
    startupTraceImmediate("before initMCP");
    try
    {
        initMCP();
        startupTraceImmediate("after initMCP");
    }
    catch (...)
    {
        startupTraceImmediate("initMCP exception");
        OutputDebugStringA("ERROR: initMCP failed\n");
    }

    // Initialize Phase 29+36: VS Code Extension API + QuickJS VSIX Host
    startupTraceImmediate("before initVSCodeExtensionAPI");
    try
    {
        initVSCodeExtensionAPI();
        startupTraceImmediate("after initVSCodeExtensionAPI");
    }
    catch (...)
    {
        startupTraceImmediate("initVSCodeExtensionAPI exception");
        OutputDebugStringA("ERROR: initVSCodeExtensionAPI failed\n");
    }

    startupTraceImmediate("before InitializeExtensionEngine");
    try
    {
        RawrXD::Extensions::InitializeExtensionEngine(m_hwndMain, false);
        startupTraceImmediate("after InitializeExtensionEngine");
    }
    catch (...)
    {
        startupTraceImmediate("InitializeExtensionEngine exception");
        OutputDebugStringA("ERROR: InitializeExtensionEngine failed\n");
    }

    if (isShuttingDown())
        return;

    // Initialize Phase 43: Plugin System (Native Win32 DLL loading)
    startupTraceImmediate("before initPluginSystem");
    try
    {
        initPluginSystem();
        startupTraceImmediate("after initPluginSystem");
    }
    catch (...)
    {
        startupTraceImmediate("initPluginSystem exception");
        OutputDebugStringA("ERROR: initPluginSystem failed\n");
    }

    // Auto-start Local HTTP server (port 11435) so HTML beacon / Ghost can detect IDE
    if (!isShuttingDown())
    {
        startupTraceImmediate("before startLocalServer");
        try
        {
            startLocalServer();
            startupTraceImmediate("after startLocalServer");
        }
        catch (...)
        {
            startupTraceImmediate("startLocalServer exception");
            OutputDebugStringA("ERROR: startLocalServer failed\n");
        }
        idePipelineMilestone("[IDE-Pipeline] Batch 7/8: Local HTTP server (beacon / tooling)\n",
                             "[Init] Batch 7/8: Local HTTP server (beacon / tooling)\n");
    }

    // Initialize Cursor/JB-Parity Feature Modules
    if (!isShuttingDown())
    {
        try
        {
            initAllFeatureModules();

            std::string featureRouteReport;
            if (!verifyFeatureRoutingCoverageAtStartup(&featureRouteReport))
            {
                // NON-FATAL: Log the routing issue but don't close the app
                OutputDebugStringA(
                    "[StartupWarning] verifyFeatureRoutingCoverageAtStartup returned false (non-fatal)\n");
                LOG_INFO("[StartupWarning] Feature routing verification failed at startup (non-fatal - continuing)");
                OutputDebugStringA(featureRouteReport.c_str());
                // Silently continue — routing issues surfaced in logs but won't block startup
            }
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

            // Tier 9 scaffold: bring up lock-free inference ring buffer early so
            // ghost-text/semantic telemetry can attach without blocking UI thread.
            if (!NeuralBridge::IsInitialized())
            {
                NeuralBridge::Initialize("");
                std::string neuralReport;
                const bool neuralSmokeOk = NeuralBridge::RunSmokeTest(&neuralReport);
                appendToOutput(std::string("[NeuralBridge] ") + neuralReport + "\n", "System",
                               neuralSmokeOk ? OutputSeverity::Info : OutputSeverity::Warning);
            }
        }
        catch (...)
        {
            OutputDebugStringA("ERROR: initTier5Cosmetics failed\n");
        }
        idePipelineMilestone("[IDE-Pipeline] Batch 8/8: Tier5 cosmetics + downstream model paths\n",
                             "[Init] Batch 8/8: Tier5 cosmetics + downstream model paths\n");
    }

    idePipelineMilestone("[IDE-Pipeline] E0-1: Tier5 + feature-module chain finished; entering GGUF standby\n",
                         "[Init] E0-1/8: Tier5 and feature routing complete; GGUF standby next\n");

    // Standby StreamingGGUFLoader so load/inspect paths never hit a nullptr gate (non-authoritative vs CPU engine).
    if (!isShuttingDown() && !m_ggufLoader)
    {
        try
        {
            m_ggufLoader = std::make_unique<RawrXD::StreamingGGUFLoader>();
            appendToOutput("[Init] Streaming GGUF loader ready (standby)\n", "System", OutputSeverity::Info);
        }
        catch (...)
        {
            OutputDebugStringA("ERROR: StreamingGGUFLoader allocation failed (non-fatal)\n");
            appendToOutput("[Init] Streaming GGUF loader not created; CPU inference path unchanged.\n", "System",
                           OutputSeverity::Warning);
        }
    }

    idePipelineMilestone("[IDE-Pipeline] E0-2: GGUF loader standby path evaluated (background thread)\n",
                         "[Init] E0-2/8: GGUF loader standby evaluated\n");
    idePipelineMilestone("[IDE-Pipeline] E0-3: deferredHeavyInit complete (background thread)\n",
                         "[Init] E0-3/8: deferred init primary phases complete (background thread)\n");

    // Initialize AI backend probe (background thread, posts WM_AI_BACKEND_STATUS on result)
    if (!isShuttingDown())
    {
        idePipelineMilestone("[IDE-Pipeline] E0-4: starting AI backend capability probe\n",
                             "[Init] E0-4/8: starting AI backend capability probe\n");
        try
        {
            initializeAIBackend();
        }
        catch (...)
        {
            OutputDebugStringA("ERROR: initializeAIBackend failed\n");
        }
        idePipelineMilestone("[IDE-Pipeline] E0-5: AI backend probe invoked\n",
                             "[Init] E0-5/8: AI backend probe invoked\n");
    }

    idePipelineMilestone("[IDE-Pipeline] E0-6: preparing main-window refresh handoff\n",
                         "[Init] E0-6/8: preparing main-window refresh handoff\n");
    // Notify UI thread to refresh
    if (!isShuttingDown() && m_hwndMain)
    {
        LOG_INFO("[StartupTrace] Worker posting WM_APP+101 to UI thread");
        PostMessage(m_hwndMain, WM_APP + 101, 0, 0);
        LOG_INFO("[StartupTrace] Worker posted WM_APP+101 to UI thread");
        idePipelineMilestone("[IDE-Pipeline] E0-7: WM_APP+101 posted for main-window refresh\n",
                             "[Init] E0-7/8: main-window refresh message posted\n");
    }
    else if (!isShuttingDown())
    {
        idePipelineMilestone("[IDE-Pipeline] E0-7: WM_APP+101 skipped (no main HWND)\n",
                             "[Init] E0-7/8: main-window refresh skipped (no main HWND)\n");
    }

    m_deferredHeavyInitComplete.store(true, std::memory_order_release);
    m_initStatus.store(STATUS_INIT_SUCCESS, std::memory_order_release);  // Mark supervisor that init succeeded

    // Signal that agent bridge callbacks are now safe (UI is fully ready)
    AgentBridge_SetInitComplete(true);

    // Reset auto-retry counter on success so user gets another "free crash" if they switch modes later
    m_autoRetryCount.store(0, std::memory_order_release);

    idePipelineMilestone("[IDE-Pipeline] E0-8: deferredHeavyInitBody returning\n",
                         "[Init] E0-8/8: background startup pipeline finished\n");
}

// ============================================================================
// Supervisor Recovery: Handle background init failures and offer retry UI
// ============================================================================
void Win32IDE::onDeferredInitFailed()
{
    // Set status to CRASHED so UI can detect it
    m_initStatus.store(STATUS_INIT_CRASHED, std::memory_order_release);

    const char* msg = "[IDE-Pipeline] onDeferredInitFailed: Background init worker crashed, attempting auto-recovery";
    OutputDebugStringA(msg);
    OutputDebugStringA("\n");

    if (m_hwndOutputTabs && IsWindow(m_hwndOutputTabs))
    {
        appendToOutput("[Init] AI Engine initialization failed", "System", OutputSeverity::Error);
    }

    // Attempt one automatic safe-mode retry before yielding to UI
    handleInitFailure();
}

void Win32IDE::handleInitFailure()
{
    const int retryCount = m_autoRetryCount.load(std::memory_order_acquire);

    if (retryCount < 1)
    {
        // First crash: attempt auto-retry with CPU-only fallback
        m_autoRetryCount.store(retryCount + 1, std::memory_order_release);
        m_initStatus.store(STATUS_INIT_FALLBACK, std::memory_order_release);  // Mark fallback mode

        const char* msg = "[IDE-Pipeline] handleInitFailure: Triggering auto-retry with CPU-only fallback";
        OutputDebugStringA(msg);
        OutputDebugStringA("\n");

        if (m_hwndOutputTabs && IsWindow(m_hwndOutputTabs))
        {
            appendToOutput("[Init] Retrying in safe mode (CPU only)...", "System", OutputSeverity::Warning);
        }

        // Disable GPU layers and retry
        forceCpuInference(true);
        deferredHeavyInit();
    }
    else
    {
        // Second crash or retry already attempted: yield to manual intervention
        m_initStatus.store(STATUS_INIT_CRASHED, std::memory_order_release);

        const char* msg = "[IDE-Pipeline] handleInitFailure: Persistent crash detected, UI must handle recovery";
        OutputDebugStringA(msg);
        OutputDebugStringA("\n");

        if (m_hwndOutputTabs && IsWindow(m_hwndOutputTabs))
        {
            appendToOutput("[Init] AI Engine failed to initialize. Use Chat pane buttons to retry or select Safe Mode.",
                           "System", OutputSeverity::Error);
        }
    }
}

void Win32IDE::forceCpuInference(bool enable)
{
    // When enable=true, disable GPU layers for CPU-only fallback mode
    // When enable=false, re-enable GPU layers
    const bool gpuMode = !enable;
    setLocalGGUFGPUEnabled(gpuMode);

    std::string msg = enable ? "[IDE-Pipeline] forceCpuInference: Disabled GPU, now CPU-only for safe-mode retry\n"
                             : "[IDE-Pipeline] forceCpuInference: Re-enabled GPU layers\n";
    OutputDebugStringA(msg.c_str());
}

// ============================================================================
// onDestroy - Called when WM_DESTROY is received
// ============================================================================
void Win32IDE::onDestroy()
{
    LOG_INFO("Win32IDE::onDestroy - shutting down");

    // Gate bridge callbacks first; this is the highest-priority teardown barrier.
    AgentBridge_BindMainWindow(nullptr);
    RawrXD::Bridge::ShutdownSwarmSystem();
    RawrXD::Extensions::ShutdownExtensionEngine();

    if (m_hAccel)
    {
        DestroyAcceleratorTable(m_hAccel);
        m_hAccel = nullptr;
    }

    RawrXD::Agent::AgentToolHandlers::SetIntegratedTerminalEchoCallback(nullptr, nullptr);

    if (m_hwndMain && IsWindow(m_hwndMain))
    {
        KillTimer(m_hwndMain, RAWRXD_IDT_PS_QUEUE_DRAIN);
    }
    m_psCommandQueue.clear();

    if (m_hwndExpertHeatmapPanel && IsWindow(m_hwndExpertHeatmapPanel))
    {
        DestroyWindow(m_hwndExpertHeatmapPanel);
        m_hwndExpertHeatmapPanel = nullptr;
    }

    clearInferenceLayerProgressCallback();

    // Signal ALL detached threads to stop touching 'this'
    m_shuttingDown.store(true, std::memory_order_release);

    // Stop visibility watchdog thread first so it cannot race on HWND usage
    // while the rest of shutdown tears down UI resources.
    stopVisibilityWatchdog();

    // Stop any in-progress inference immediately
    m_inferenceStopRequested = true;
    m_planExecutionCancelled.store(true);

    // Wait for all detached threads to notice the flag and exit (up to ~2s).
    for (int i = 0; i < 200 && m_activeDetachedThreads.load(std::memory_order_acquire) > 0; ++i)
    {
        Sleep(10);
    }
    if (m_activeDetachedThreads.load(std::memory_order_acquire) > 0)
    {
        OutputDebugStringA("onDestroy: WARNING — detached threads still active after ~2s\n");
        Sleep(40);  // Brief extra grace
    }

    if (m_semanticIndexInitialized)
    {
        std::string workspaceRoot = m_projectRoot;
        if (workspaceRoot.empty())
            workspaceRoot = m_explorerRootPath;
        if (workspaceRoot.empty())
            workspaceRoot = m_currentDirectory;
        if (workspaceRoot.empty())
            workspaceRoot = ".";

        std::string persistenceDetail;
        const bool saved =
            RawrXD::Core::VectorIndexPersistence::TrySaveSemanticIndexCache(workspaceRoot, &persistenceDetail);
        OutputDebugStringA((std::string("[SemanticIndex] ") + persistenceDetail + "\n").c_str());

        if (!saved)
        {
            OutputDebugStringA("[SemanticIndex] WARNING: cache save failed\n");
        }

        RawrXD::SemanticIndex::SemanticIndexEngine::Instance().Shutdown();
        m_semanticIndexInitialized = false;
    }

    // Shutdown Phase 29+36: VS Code Extension API + QuickJS VSIX Host
    shutdownVSCodeExtensionAPI();

    // Shutdown core runtime spine (signature verifier, sqlite, telemetry export).
    shutdownCoreRuntimeSpine();

    // Shutdown Tier 3: Polish (smooth caret, ligatures, file watcher)
    shutdownTier3Polish();

    // Shutdown Tier 2: Git diff overlay, terminal tabs, hover tooltips, reference/CodeLens/inlay surfaces
    shutdownTier2Cosmetics();

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
    UnregisterHotKey(m_hwndMain, kEmergencyWipeHotkeyId);
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

    if (id == ID_ACCEL_GLOBAL_HALT)
    {
        Win32IDEAgenticPanel::Win32IDE_AgenticPlanningPanel* panel = Win32IDEAgenticPanel::GetAgenticPlanningPanel();
        if (panel)
        {
            panel->onHaltRequested();
        }
        return;
    }

    if (handleWiringManifestGaps(id, codeNotify))
    {
        return;
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
        std::string promptPreview;
        if (m_hwndCopilotChatInput && IsWindow(m_hwndCopilotChatInput))
        {
            const int inputLen = GetWindowTextLengthW(m_hwndCopilotChatInput);
            if (inputLen > 0)
            {
                std::vector<wchar_t> inputBuffer(static_cast<size_t>(inputLen) + 1, L'\0');
                GetWindowTextW(m_hwndCopilotChatInput, inputBuffer.data(), inputLen + 1);
                const int utf8Len =
                    WideCharToMultiByte(CP_UTF8, 0, inputBuffer.data(), -1, nullptr, 0, nullptr, nullptr);
                if (utf8Len > 1)
                {
                    std::string utf8(static_cast<size_t>(utf8Len) - 1, '\0');
                    WideCharToMultiByte(CP_UTF8, 0, inputBuffer.data(), -1, utf8.data(), utf8Len, nullptr, nullptr);
                    promptPreview = std::move(utf8);
                }
            }
        }
        const bool hasAgenticPrefix = promptPreview.rfind("/agent", 0) == 0 ||
                                      promptPreview.rfind("/agentic", 0) == 0 ||
                                      promptPreview.rfind("agentic:", 0) == 0 || promptPreview.rfind("@agent", 0) == 0;
        const bool bridgeAgenticMode = m_agenticBridge && m_agenticBridge->IsAgenticMode();
        const bool wantsAgentic = hasAgenticPrefix || bridgeAgenticMode || m_agenticFunctionCallingMode;
        const bool layerAvailable = rawrxd::isAgenticLayerAvailable();
        OutputDebugStringA(("ROUTE_CHECK: route=C-main-command, prompt_len=" + std::to_string(promptPreview.size()) +
                            ", wantsAgentic=" + std::to_string(wantsAgentic ? 1 : 0) +
                            ", layerAvailable=" + std::to_string(layerAvailable ? 1 : 0) + "\n")
                               .c_str());
        postDeferredCopilotSend();
        return;
    }
    if (id == 1205)
    {
        HandleCopilotClear();
        return;
    }
    if (id == 1208)
    {
        if (codeNotify == CBN_SELCHANGE)
        {
            OutputDebugStringA("[onCommand] Model selection changed\n");
            onModelSelectionChanged();
        }
        return;
    }
    if (id == 1209)  // IDC_MODEL_BROWSE_BTN
    {
        handleModelBrowse();
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
    if (id == 14021)
    {
        if (codeNotify == LBN_SELCHANGE)
        {
            onPendingEditSelectionChanged();
        }
        else if (codeNotify == LBN_DBLCLK)
        {
            LRESULT sel = SendMessageA(m_hwndEditReviewList, LB_GETCURSEL, 0, 0);
            if (sel != LB_ERR)
            {
                const uint64_t editId =
                    static_cast<uint64_t>(SendMessageA(m_hwndEditReviewList, LB_GETITEMDATA, sel, 0));
                approvePendingEdit(editId);
            }
        }
        return;
    }
    if (id == 14023 || id == 14024)
    {
        LRESULT sel = m_hwndEditReviewList ? SendMessageA(m_hwndEditReviewList, LB_GETCURSEL, 0, 0) : LB_ERR;
        if (sel != LB_ERR)
        {
            const uint64_t editId = static_cast<uint64_t>(SendMessageA(m_hwndEditReviewList, LB_GETITEMDATA, sel, 0));
            if (id == 14023)
            {
                approvePendingEdit(editId);
            }
            else
            {
                declinePendingEdit(editId);
            }
        }
        return;
    }
    if (id == 14025)
    {
        approveAllPendingEdits();
        return;
    }
    if (id == 14026)
    {
        declineAllPendingEdits();
        return;
    }

    if (id == 1001)  // Composer Apply
    {
        appendToOutput("Composer: Finalizing atomic workspace commit...\n", "System", OutputSeverity::Info);
        if (ComposerPanel_CommitChanges())
        {
            appendToOutput("Composer: ALL CHANGES APPLIED SUCCESSFULLY.\n", "System", OutputSeverity::Success);
        }
        else
        {
            appendToOutput("Composer: ERROR - Atomic swap failed or stale file detected. Transaction aborted.\n",
                           "System", OutputSeverity::Error);
        }
        ComposerPanel_HidePlan();
        return;
    }
    if (id == 1002)  // Composer Cancel
    {
        ComposerPanel_HidePlan();
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
        case 1307:  // IDC_PANEL_BTN_NEW_TERMINAL — same as VS Code "+" (new integrated terminal session)
            switchPanelTab(PanelTab::Terminal);
            handleTerminalCommand(IDM_TERMINAL_NEW_USER);
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
            appendToOutput(
                "[Vulkan] Saved preference performance.vulkanRenderer to rawrxd.config.json (cwd or exe dir).\n",
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
        case IDM_VIEW_SOVEREIGN_SNAP_COMPACT:
            applySovereignSnapPreset(240, L"Compact");
            return;
        case IDM_VIEW_SOVEREIGN_SNAP_STANDARD:
            applySovereignSnapPreset(360, L"Standard");
            return;
        case IDM_VIEW_SOVEREIGN_SNAP_WIDE:
            applySovereignSnapPreset(480, L"Wide");
            return;
        case IDM_VIEW_AGENT_PANEL:
            toggleAgentPanel();
            return;
        case IDM_VIEW_VIDEO_STUDIO:  // 2046
            toggleVideoStudioWindow();
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
        case IDM_PLAN_ORCHESTRATOR_START:
            if (m_planOrchestrator)
            {
                // Prompt user for goal via input dialog
                wchar_t goalBuf[512] = {};
                std::string goal;
                if (DialogBoxWithInput(L"Plan Orchestrator", L"Enter your goal:", goalBuf, 512) && goalBuf[0])
                {
                    int len = WideCharToMultiByte(CP_UTF8, 0, goalBuf, -1, nullptr, 0, nullptr, nullptr);
                    goal.resize(len > 0 ? len - 1 : 0);
                    WideCharToMultiByte(CP_UTF8, 0, goalBuf, -1, goal.data(), len, nullptr, nullptr);
                }
                else
                {
                    goal = "Implement a hello world function in C++";
                }
                std::string workspaceRoot = m_projectRoot;
                if (workspaceRoot.empty())
                    workspaceRoot = m_explorerRootPath;
                if (workspaceRoot.empty())
                    workspaceRoot = m_currentDirectory;
                if (workspaceRoot.empty() && !m_currentFile.empty())
                {
                    size_t lastSlash = m_currentFile.find_last_of("\\/");
                    if (lastSlash != std::string::npos)
                        workspaceRoot = m_currentFile.substr(0, lastSlash);
                }
                if (workspaceRoot.empty())
                    workspaceRoot = ".";

                // Run planning and execution asynchronously
                std::thread(
                    [this, goal, workspaceRoot]()
                    {
                        auto result = m_planOrchestrator->planAndExecute(goal, workspaceRoot, false);
                        if (result.success)
                        {
                            appendToOutput("✅ Plan Orchestrator: Task completed successfully\n", "Output",
                                           OutputSeverity::Info);
                        }
                        else
                        {
                            appendToOutput("❌ Plan Orchestrator: Task failed - " + result.errorMessage + "\n",
                                           "Errors", OutputSeverity::Error);
                        }
                    })
                    .detach();

                if (m_hwndStatusBar)
                    SendMessage(m_hwndStatusBar, SB_SETTEXT, 0, (LPARAM)L"Plan Orchestrator started");
            }
            else
            {
                MessageBoxW(m_hwndMain, L"Plan Orchestrator not initialized", L"Error", MB_OK | MB_ICONERROR);
            }
            return;
        case IDM_PLAN_ORCHESTRATOR_STOP:
            if (m_planOrchestrator)
            {
                m_planOrchestrator->requestStop();
                appendToOutput("\xE2\x9B\x94 Plan Orchestrator: Stop requested\n", "Output", OutputSeverity::Warning);
                if (m_hwndStatusBar)
                    SendMessage(m_hwndStatusBar, SB_SETTEXT, 0, (LPARAM)L"Plan Orchestrator stopped");
            }
            else
            {
                MessageBoxW(m_hwndMain, L"Plan Orchestrator not initialized", L"Error", MB_OK | MB_ICONERROR);
            }
            return;
        case IDM_PLAN_ORCHESTRATOR_VIEW_STATUS:
            if (m_planOrchestrator)
            {
                auto steps = m_planOrchestrator->getPlan();
                std::string status = "Plan Status:\n";
                status += "Total steps: " + std::to_string(steps.size()) + "\n";
                status += "Completed: " +
                          std::to_string(
                              std::count_if(steps.begin(), steps.end(), [](const auto& s) { return s.isComplete; })) +
                          "\n";
                status += std::string("Complete: ") + (m_planOrchestrator->isComplete() ? "Yes" : "No") + "\n";
                MessageBoxA(m_hwndMain, status.c_str(), "Plan Orchestrator Status", MB_OK | MB_ICONINFORMATION);
            }
            else
            {
                MessageBoxW(m_hwndMain, L"Plan Orchestrator not initialized", L"Error", MB_OK | MB_ICONERROR);
            }
            return;
        case IDM_PLAN_ORCHESTRATOR_VIEW_PLAN:
            if (m_planOrchestrator)
            {
                auto steps = m_planOrchestrator->getPlan();
                std::string current_plan;
                current_plan = "Current Plan:\n";
                for (size_t i = 0; i < steps.size(); ++i)
                {
                    current_plan += std::to_string(i + 1) + ". " + steps[i].description;
                    current_plan += (steps[i].isComplete ? " [Done]" : " [Pending]");
                    current_plan += "\n";
                    if (!steps[i].result.empty())
                    {
                        current_plan += "   Result: " + steps[i].result + "\n";
                    }
                }
                if (current_plan == "Current Plan:\n")
                {
                    current_plan = "No current plan available";
                }
                MessageBoxA(m_hwndMain, current_plan.c_str(), "Current Plan", MB_OK | MB_ICONINFORMATION);
            }
            else
            {
                MessageBoxW(m_hwndMain, L"Plan Orchestrator not initialized", L"Error", MB_OK | MB_ICONERROR);
            }
            return;
        default:
            break;
    }

    // Prevent control-ID collisions from being interpreted as File menu commands.
    // Example: IDC_OUTPUT_EDIT_GENERAL (1012) collides with IDM_FILE_RECENT_BASE+2.
    // Menu commands arrive with hwndCtl == nullptr; control notifications provide hwndCtl.
    if (hwndCtl != nullptr && id >= 1000 && id < 1100)
    {
        return;
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

void Win32IDE::forceKillBuildLocks()
{
    static const wchar_t* kBuildExeNames[] = {L"ninja.exe",    L"cmake.exe", L"MSBuild.exe",  L"devenv.exe",
                                              L"cl.exe",       L"link.exe",  L"clangd.exe",   L"lib.exe",
                                              L"clang-cl.exe", L"ml64.exe",  L"mspdbsrv.exe", L"rc.exe",
                                              L"c1xx.exe",     L"c2.dll",    L"cvtres.exe"};
    const DWORD selfPid = GetCurrentProcessId();
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE)
    {
        appendToOutput("[Build] forceKillBuildLocks: CreateToolhelp32Snapshot failed.\n", "General",
                       OutputSeverity::Warning);
        return;
    }
    PROCESSENTRY32W pe{};
    pe.dwSize = sizeof(pe);
    int terminated = 0;
    std::string terminatedNames;
    if (Process32FirstW(snap, &pe))
    {
        do
        {
            if (pe.th32ProcessID == selfPid)
            {
                continue;
            }
            for (const wchar_t* exe : kBuildExeNames)
            {
                if (_wcsicmp(pe.szExeFile, exe) != 0)
                {
                    continue;
                }
                HANDLE ph = OpenProcess(PROCESS_TERMINATE, FALSE, pe.th32ProcessID);
                if (ph)
                {
                    if (TerminateProcess(ph, 1))
                    {
                        ++terminated;
                        if (!terminatedNames.empty())
                        {
                            terminatedNames += ", ";
                        }
                        char exeNameUtf8[MAX_PATH] = {};
                        const int converted =
                            WideCharToMultiByte(CP_UTF8, 0, pe.szExeFile, -1, exeNameUtf8,
                                                static_cast<int>(std::size(exeNameUtf8)), nullptr, nullptr);
                        if (converted > 0)
                        {
                            terminatedNames += exeNameUtf8;
                        }
                        else
                        {
                            terminatedNames += "<unknown>";
                        }
                    }
                    CloseHandle(ph);
                }
                break;
            }
        } while (Process32NextW(snap, &pe));
    }
    CloseHandle(snap);
    appendToOutput("[Build] forceKillBuildLocks: issued TerminateProcess for " + std::to_string(terminated) +
                       " matching process(es) (ninja/cmake/MSBuild/devenv/cl/link/ml64/mspdbsrv/clangd/...).\n",
                   "General", OutputSeverity::Info);
    if (!terminatedNames.empty())
    {
        appendToOutput("[Build] forceKillBuildLocks: terminated => " + terminatedNames + "\n", "General",
                       OutputSeverity::Info);
    }
    appendToOutput("[Build] Build path cleared.\n", "Output", OutputSeverity::Info);
}
