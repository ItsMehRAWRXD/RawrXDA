#include "ide_completion.h"
#include <thread>
#include <mutex>
#include <queue>

namespace IDECompletion {

// Global state
static std::string g_current_model = "codellama:7b";
static std::thread g_completion_thread;
static std::mutex g_state_mutex;
static std::queue<PopupContext> g_pending_requests;
static bool g_engine_ready = false;
static bool g_thread_running = false;
static HWND g_popup_hwnd = NULL;

//==============================================================================
// COMPLETION POPUP WINDOW
//==============================================================================

LRESULT CALLBACK CompletionPopupProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            
            // Draw white background
            HBRUSH hBrush = CreateSolidBrush(RGB(240, 240, 240));
            FillRect(hdc, &ps.rcPaint, hBrush);
            DeleteObject(hBrush);
            
            // Draw border
            HPEN hPen = CreatePen(PS_SOLID, 1, RGB(100, 100, 100));
            HPEN hOldPen = (HPEN)SelectObject(hdc, hPen);
            MoveToEx(hdc, ps.rcPaint.left, ps.rcPaint.top, NULL);
            LineTo(hdc, ps.rcPaint.right, ps.rcPaint.top);
            LineTo(hdc, ps.rcPaint.right, ps.rcPaint.bottom);
            LineTo(hdc, ps.rcPaint.left, ps.rcPaint.bottom);
            LineTo(hdc, ps.rcPaint.left, ps.rcPaint.top);
            SelectObject(hdc, hOldPen);
            DeleteObject(hPen);
            
            // Draw text (would be populated from popup context)
            WCHAR szText[] = L"Suggestion from Ollama...";
            SetTextColor(hdc, RGB(0, 0, 0));
            SetBkMode(hdc, TRANSPARENT);
            TextOutW(hdc, 5, 5, szText, wcslen(szText));
            
            EndPaint(hwnd, &ps);
            break;
        }
        case WM_DESTROY:
            PostQuitMessage(0);
            break;
        default:
            return DefWindowProcW(hwnd, uMsg, wParam, lParam);
    }
    return 0;
}

//==============================================================================
// BACKGROUND COMPLETION WORKER
//==============================================================================

void CompletionWorkerThread() {
    while (g_thread_running) {
        PopupContext ctx;
        {
            std::lock_guard<std::mutex> lock(g_state_mutex);
            if (g_pending_requests.empty()) {
                // Sleep briefly to avoid busy-waiting
                Sleep(10);
                continue;
            }
            ctx = g_pending_requests.front();
            g_pending_requests.pop();
        }

        // Query Ollama API
        OllamaIntegration::CompletionRequest req;
        req.model = g_current_model;
        req.prompt = ctx.current_line;
        req.temperature = 0.7f;
        req.num_predict = 128;  // Short suggestions
        req.stream = false;

        OllamaIntegration::CompletionResponse response = OllamaIntegration::QueryCompletion(req);

        if (response.success && !response.text.empty()) {
            // Show popup with suggestion
            ShowCompletionPopup(ctx, response.text);
        }
    }
}

//==============================================================================
// PUBLIC API IMPLEMENTATION
//==============================================================================

void InitializeCompletionEngine(const std::string& default_model) {
    std::lock_guard<std::mutex> lock(g_state_mutex);

    g_current_model = default_model;

    // Check if Ollama is available
    g_engine_ready = OllamaIntegration::IsOllamaAvailable();

    if (!g_engine_ready) {
        // Try to connect later
        return;
    }

    // Start background worker thread
    g_thread_running = true;
    g_completion_thread = std::thread(CompletionWorkerThread);
}

void RequestCompletion(const PopupContext& ctx) {
    std::lock_guard<std::mutex> lock(g_state_mutex);

    if (!g_engine_ready) {
        return;
    }

    // Add to queue for background processing
    g_pending_requests.push(ctx);
}

void CancelCompletion() {
    std::lock_guard<std::mutex> lock(g_state_mutex);
    
    // Clear pending requests
    while (!g_pending_requests.empty()) {
        g_pending_requests.pop();
    }

    HideCompletionPopup();
}

void ShowCompletionPopup(const PopupContext& ctx, const std::string& suggestion) {
    // Hide existing popup
    if (g_popup_hwnd && IsWindow(g_popup_hwnd)) {
        DestroyWindow(g_popup_hwnd);
    }

    // Register window class
    static WNDCLASSW wc = {0};
    if (!wc.lpszClassName) {
        wc.lpfnWndProc = CompletionPopupProc;
        wc.hInstance = GetModuleHandleW(NULL);
        wc.lpszClassName = L"RawrXD_CompletionPopup";
        wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
        RegisterClassW(&wc);
    }

    // Create popup window
    g_popup_hwnd = CreateWindowExW(
        WS_EX_TOPMOST,
        L"RawrXD_CompletionPopup",
        L"Completion",
        WS_POPUP | WS_BORDER,
        ctx.x, ctx.y,
        300, 100,  // Width, Height
        ctx.hParentWnd,
        NULL,
        GetModuleHandleW(NULL),
        NULL);

    if (g_popup_hwnd) {
        ShowWindow(g_popup_hwnd, SW_SHOW);
        UpdateWindow(g_popup_hwnd);
    }
}

void HideCompletionPopup() {
    if (g_popup_hwnd && IsWindow(g_popup_hwnd)) {
        DestroyWindow(g_popup_hwnd);
        g_popup_hwnd = NULL;
    }
}

void SetCompletionModel(const std::string& model) {
    std::lock_guard<std::mutex> lock(g_state_mutex);
    g_current_model = model;
}

bool IsCompletionEngineReady() {
    std::lock_guard<std::mutex> lock(g_state_mutex);
    return g_engine_ready;
}

} // namespace IDECompletion
