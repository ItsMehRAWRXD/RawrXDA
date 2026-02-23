// ============================================================================
// streaming_token_progress.cpp — Pure Win32 Native Streaming Token Progress
// ============================================================================
// Real-time inference progress: animated bar, tok/s rate, ETA,
// elapsed time. Gradient-style progress fill. Dark theme.
//
// Pattern: PatchResult-style structured results, no exceptions
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED
// ============================================================================

#include "streaming_token_progress.h"
#include <cstdio>
#include <cstring>
#include <algorithm>

// ============================================================================
// Constants
// ============================================================================

static constexpr COLORREF BG_COLOR       = RGB(30, 30, 35);
static constexpr COLORREF BORDER_CLR     = RGB(60, 60, 65);
static constexpr COLORREF TEXT_CLR       = RGB(220, 220, 220);
static constexpr COLORREF LABEL_CLR     = RGB(140, 140, 140);
static constexpr COLORREF PROGRESS_BG    = RGB(30, 30, 30);
static constexpr COLORREF PROGRESS_FG1   = RGB(78, 201, 176);    // Teal
static constexpr COLORREF PROGRESS_FG2   = RGB(86, 156, 214);    // Blue
static constexpr COLORREF METRICS_CLR    = RGB(86, 156, 214);
static constexpr COLORREF SUCCESS_CLR    = RGB(92, 184, 92);
static constexpr COLORREF ACTIVE_CLR     = RGB(255, 200, 50);

static const wchar_t* PROGRESS_CLASS = L"RawrXD_StreamingTokenProgress";
bool StreamingTokenProgressBar::s_classRegistered = false;

#define IDT_METRICS 6001

// ============================================================================
// WndProc
// ============================================================================

LRESULT CALLBACK StreamingTokenProgressBar::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    StreamingTokenProgressBar* self = nullptr;
    if (msg == WM_NCCREATE) {
        auto cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
        self = reinterpret_cast<StreamingTokenProgressBar*>(cs->lpCreateParams);
        SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    } else {
        self = reinterpret_cast<StreamingTokenProgressBar*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
    }
    if (!self) return DefWindowProcW(hwnd, msg, wParam, lParam);

    switch (msg) {
    case WM_TIMER:
        if (wParam == IDT_METRICS) {
            self->updateMetrics();
            InvalidateRect(hwnd, nullptr, FALSE);
        }
        return 0;

    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        RECT rc;
        GetClientRect(hwnd, &rc);
        int w = rc.right - rc.left;
        int h = rc.bottom - rc.top;

        if (w != self->m_bufW || h != self->m_bufH) {
            if (self->m_backBuf) DeleteObject(self->m_backBuf);
            if (self->m_backDC) DeleteDC(self->m_backDC);
            self->m_backDC = CreateCompatibleDC(hdc);
            self->m_backBuf = CreateCompatibleBitmap(hdc, w, h);
            SelectObject(self->m_backDC, self->m_backBuf);
            self->m_bufW = w;
            self->m_bufH = h;
        }
        self->paint(self->m_backDC, rc);
        BitBlt(hdc, 0, 0, w, h, self->m_backDC, 0, 0, SRCCOPY);
        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_ERASEBKGND:
        return 1;

    default:
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
}

// ============================================================================
// Constructor / Destructor
// ============================================================================

StreamingTokenProgressBar::StreamingTokenProgressBar(HWND parent) {
    createWindow(parent);
    OutputDebugStringA("[StreamingTokenProgressBar] Initialized\n");
}

StreamingTokenProgressBar::~StreamingTokenProgressBar() {
    if (m_timerId) KillTimer(m_hwnd, m_timerId);
    if (m_backBuf) DeleteObject(m_backBuf);
    if (m_backDC) DeleteDC(m_backDC);
    if (m_fontStatus) DeleteObject(m_fontStatus);
    if (m_fontMetrics) DeleteObject(m_fontMetrics);
    if (m_hwnd) DestroyWindow(m_hwnd);
}

void StreamingTokenProgressBar::createWindow(HWND parent) {
    HINSTANCE hInst = (HINSTANCE)GetWindowLongPtr(parent, GWLP_HINSTANCE);
    if (!hInst) hInst = GetModuleHandle(nullptr);

    if (!s_classRegistered) {
        WNDCLASSEXW wc = {};
        wc.cbSize = sizeof(wc);
        wc.style = CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc = WndProc;
        wc.hInstance = hInst;
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.hbrBackground = CreateSolidBrush(BG_COLOR);
        wc.lpszClassName = PROGRESS_CLASS;
        RegisterClassExW(&wc);
        s_classRegistered = true;
    }

    m_hwnd = CreateWindowExW(0, PROGRESS_CLASS, nullptr,
        WS_CHILD | WS_CLIPCHILDREN, 0, 0, 400, 60, parent, nullptr, hInst, this);

    m_fontStatus = CreateFontW(-12, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
    m_fontMetrics = CreateFontW(-10, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, FIXED_PITCH, L"Consolas");
}

// ============================================================================
// Lifecycle
// ============================================================================

void StreamingTokenProgressBar::startGeneration(int estimatedTokens) {
    m_isGenerating = true;
    m_totalTokens = 0;
    m_estimatedTokens = estimatedTokens;
    m_startTick = GetTickCount();
    m_tokensPerSecond = 0.0;

    wcscpy_s(m_statusText, L"\x26A1 Generating tokens...");
    m_metricsText[0] = L'\0';

    m_timerId = SetTimer(m_hwnd, IDT_METRICS, 100, nullptr);

    if (m_pfnStarted) m_pfnStarted(m_cbUserdata);

    char log[128];
    sprintf_s(log, "[StreamingTokenProgressBar] Started, estimated=%d\n", estimatedTokens);
    OutputDebugStringA(log);

    InvalidateRect(m_hwnd, nullptr, FALSE);
}

void StreamingTokenProgressBar::onTokenGenerated(const wchar_t* token) {
    if (!m_isGenerating) return;

    m_totalTokens++;

    // Calculate rate
    DWORD elapsed = GetTickCount() - m_startTick;
    if (elapsed > 0) {
        m_tokensPerSecond = (m_totalTokens * 1000.0) / elapsed;
    }

    if (m_pfnToken) m_pfnToken(token, m_totalTokens, m_cbUserdata);

    // Log every 10 tokens
    if (m_totalTokens % 10 == 0) {
        char log[128];
        sprintf_s(log, "[StreamingTokenProgressBar] Tokens: %d, Rate: %.2f tok/s\n",
            m_totalTokens, m_tokensPerSecond);
        OutputDebugStringA(log);
    }

    InvalidateRect(m_hwnd, nullptr, FALSE);
}

void StreamingTokenProgressBar::completeGeneration() {
    if (!m_isGenerating) return;

    DWORD elapsed = GetTickCount() - m_startTick;
    if (elapsed > 0) {
        m_tokensPerSecond = (m_totalTokens * 1000.0) / elapsed;
    }

    m_isGenerating = false;
    if (m_timerId) { KillTimer(m_hwnd, m_timerId); m_timerId = 0; }

    // Format completion message
    int sec = elapsed / 1000;
    int ms = elapsed % 1000;
    if (sec >= 60) {
        int min = sec / 60;
        sec = sec % 60;
        swprintf_s(m_statusText, L"\x2713 Generated %d tokens in %dm %ds (%.2f tok/s)",
            m_totalTokens, min, sec, m_tokensPerSecond);
    } else {
        swprintf_s(m_statusText, L"\x2713 Generated %d tokens in %d.%ds (%.2f tok/s)",
            m_totalTokens, sec, ms / 100, m_tokensPerSecond);
    }

    char log[256];
    sprintf_s(log, "[StreamingTokenProgressBar] Completed: %d tokens, %.2f tok/s\n",
        m_totalTokens, m_tokensPerSecond);
    OutputDebugStringA(log);

    if (m_pfnCompleted) m_pfnCompleted(m_totalTokens, m_tokensPerSecond, m_cbUserdata);

    InvalidateRect(m_hwnd, nullptr, FALSE);
}

void StreamingTokenProgressBar::reset() {
    m_isGenerating = false;
    m_totalTokens = 0;
    m_estimatedTokens = 0;
    m_tokensPerSecond = 0.0;
    if (m_timerId) { KillTimer(m_hwnd, m_timerId); m_timerId = 0; }
    wcscpy_s(m_statusText, L"Ready");
    m_metricsText[0] = L'\0';
    InvalidateRect(m_hwnd, nullptr, FALSE);
    OutputDebugStringA("[StreamingTokenProgressBar] Reset\n");
}

// ============================================================================
// Metrics Update (100ms timer)
// ============================================================================

void StreamingTokenProgressBar::updateMetrics() {
    if (!m_isGenerating) return;

    DWORD elapsed = GetTickCount() - m_startTick;
    m_metricsText[0] = L'\0';

    if (m_showTokenRate) {
        wchar_t rateBuf[64];
        swprintf_s(rateBuf, L"\x26A1 %.2f tok/s  ", m_tokensPerSecond);
        wcscat_s(m_metricsText, rateBuf);
    }

    if (m_showElapsedTime) {
        int sec = elapsed / 1000;
        int ms = elapsed % 1000;
        wchar_t timeBuf[64];
        if (sec >= 60) {
            swprintf_s(timeBuf, L"\x23F1 %dm %ds  ", sec / 60, sec % 60);
        } else {
            swprintf_s(timeBuf, L"\x23F1 %d.%ds  ", sec, ms / 100);
        }
        wcscat_s(m_metricsText, timeBuf);
    }

    if (m_estimatedTokens > 0 && m_tokensPerSecond > 0.0) {
        int remaining = m_estimatedTokens - m_totalTokens;
        if (remaining > 0) {
            double remSec = remaining / m_tokensPerSecond;
            wchar_t etaBuf[64];
            swprintf_s(etaBuf, L"\xD83D\xDCCA ETA: %.1fs", remSec); /* U+1F4CA chart */
            wcscat_s(m_metricsText, etaBuf);
        }
    }
}

// ============================================================================
// Paint
// ============================================================================

void StreamingTokenProgressBar::paint(HDC hdc, const RECT& rc) {
    int w = rc.right - rc.left;
    int h = rc.bottom - rc.top;

    HBRUSH bgBr = CreateSolidBrush(BG_COLOR);
    FillRect(hdc, &rc, bgBr);
    DeleteObject(bgBr);

    SetBkMode(hdc, TRANSPARENT);

    // Status label
    SelectObject(hdc, m_fontStatus);
    COLORREF statusClr = m_isGenerating ? ACTIVE_CLR :
        (m_totalTokens > 0 && !m_isGenerating ? SUCCESS_CLR : LABEL_CLR);
    SetTextColor(hdc, statusClr);
    RECT statusRect = { 5, 2, w - 5, 18 };
    DrawTextW(hdc, m_statusText, -1, &statusRect, DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS);

    // Progress bar
    int barY = 20;
    int barH = 16;
    paintProgressBar(hdc, 5, barY, w - 10, barH);

    // Metrics label
    if (m_metricsText[0]) {
        SelectObject(hdc, m_fontMetrics);
        SetTextColor(hdc, METRICS_CLR);
        RECT metricsRect = { 5, barY + barH + 3, w - 5, h - 2 };
        DrawTextW(hdc, m_metricsText, -1, &metricsRect, DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS);
    }
}

void StreamingTokenProgressBar::paintProgressBar(HDC hdc, int x, int y, int w, int h) {
    // Background
    RECT bgRect = { x, y, x + w, y + h };
    HBRUSH bgBr = CreateSolidBrush(PROGRESS_BG);
    FillRect(hdc, &bgRect, bgBr);
    DeleteObject(bgBr);

    // Border
    HPEN pen = CreatePen(PS_SOLID, 1, BORDER_CLR);
    HPEN oldPen = (HPEN)SelectObject(hdc, pen);
    HBRUSH nullBr = (HBRUSH)GetStockObject(NULL_BRUSH);
    SelectObject(hdc, nullBr);
    Rectangle(hdc, x, y, x + w, y + h);
    SelectObject(hdc, oldPen);
    DeleteObject(pen);

    // Calculate fill
    double ratio = 0.0;
    if (m_estimatedTokens > 0) {
        ratio = (double)m_totalTokens / m_estimatedTokens;
        if (ratio > 1.0) ratio = 1.0;
    } else if (m_isGenerating) {
        // Indeterminate: pulse animation based on time
        DWORD elapsed = GetTickCount() - m_startTick;
        double phase = (elapsed % 2000) / 2000.0;
        ratio = 0.3 + 0.2 * sin(phase * 3.14159265 * 2);
    } else if (m_totalTokens > 0) {
        ratio = 1.0;  // Completed
    }

    if (ratio > 0.0) {
        int fillW = (int)(ratio * (w - 2));
        if (fillW < 1) fillW = 1;

        // Gradient-like fill: use two colors, left half teal, right half blue
        int halfW = fillW / 2;
        if (halfW > 0) {
            RECT leftFill = { x + 1, y + 1, x + 1 + halfW, y + h - 1 };
            HBRUSH leftBr = CreateSolidBrush(PROGRESS_FG1);
            FillRect(hdc, &leftFill, leftBr);
            DeleteObject(leftBr);
        }
        if (fillW > halfW) {
            RECT rightFill = { x + 1 + halfW, y + 1, x + 1 + fillW, y + h - 1 };
            HBRUSH rightBr = CreateSolidBrush(PROGRESS_FG2);
            FillRect(hdc, &rightFill, rightBr);
            DeleteObject(rightBr);
        }
    }

    // Token count text in center
    wchar_t tokenBuf[64];
    if (m_estimatedTokens > 0) {
        int pct = m_estimatedTokens > 0 ? (m_totalTokens * 100 / m_estimatedTokens) : 0;
        swprintf_s(tokenBuf, L"%d / %d tokens (%d%%)", m_totalTokens, m_estimatedTokens, pct);
    } else {
        swprintf_s(tokenBuf, L"%d tokens", m_totalTokens);
    }
    SelectObject(hdc, m_fontMetrics);
    SetTextColor(hdc, TEXT_CLR);
    RECT textRect = { x, y, x + w, y + h };
    DrawTextW(hdc, tokenBuf, -1, &textRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
}

// ============================================================================
// Public Methods
// ============================================================================

void StreamingTokenProgressBar::setCallbacks(PFN_GENERATION_STARTED startedFn,
    PFN_TOKEN_RECEIVED tokenFn, PFN_GENERATION_COMPLETED completedFn, void* userdata) {
    m_pfnStarted = startedFn;
    m_pfnToken = tokenFn;
    m_pfnCompleted = completedFn;
    m_cbUserdata = userdata;
}

void StreamingTokenProgressBar::show() { if (m_hwnd) ShowWindow(m_hwnd, SW_SHOW); }
void StreamingTokenProgressBar::hide() { if (m_hwnd) ShowWindow(m_hwnd, SW_HIDE); }

void StreamingTokenProgressBar::resize(int x, int y, int w, int h) {
    if (m_hwnd) MoveWindow(m_hwnd, x, y, w, h, TRUE);
}

// ============================================================================
// C API
// ============================================================================

extern "C" {

StreamingTokenProgressBar* StreamingProgress_Create(HWND parent) {
    if (!parent) return nullptr;
    return new StreamingTokenProgressBar(parent);
}

void StreamingProgress_Start(StreamingTokenProgressBar* p, int estimatedTokens) {
    if (p) p->startGeneration(estimatedTokens);
}

void StreamingProgress_OnToken(StreamingTokenProgressBar* p, const wchar_t* token) {
    if (p) p->onTokenGenerated(token);
}

void StreamingProgress_Complete(StreamingTokenProgressBar* p) {
    if (p) p->completeGeneration();
}

void StreamingProgress_Reset(StreamingTokenProgressBar* p) {
    if (p) p->reset();
}

void StreamingProgress_SetCallbacks(StreamingTokenProgressBar* p,
    PFN_GENERATION_STARTED startedFn, PFN_TOKEN_RECEIVED tokenFn,
    PFN_GENERATION_COMPLETED completedFn, void* userdata) {
    if (p) p->setCallbacks(startedFn, tokenFn, completedFn, userdata);
}

void StreamingProgress_Show(StreamingTokenProgressBar* p) { if (p) p->show(); }
void StreamingProgress_Hide(StreamingTokenProgressBar* p) { if (p) p->hide(); }

void StreamingProgress_Resize(StreamingTokenProgressBar* p, int x, int y, int w, int h) {
    if (p) p->resize(x, y, w, h);
}

void StreamingProgress_Destroy(StreamingTokenProgressBar* p) {
    delete p;
}

}
