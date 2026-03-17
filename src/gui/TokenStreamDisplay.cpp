#include "TokenStreamDisplay.hpp"
#include <sstream>
#include <iomanip>

namespace RawrXD::GUI {

// Static window class registration
static bool g_editorClassRegistered = false;
static const char* g_editorClassName = "RawrXD_TokenStreamEditorClass";

// Window proc static dispatcher
static TokenStreamDisplay* g_editorInstance = nullptr;

LRESULT CALLBACK TokenStreamDisplay::editorWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_CREATE) {
        CREATESTRUCT* pCreate = reinterpret_cast<CREATESTRUCT*>(lParam);
        g_editorInstance = reinterpret_cast<TokenStreamDisplay*>(pCreate->lpCreateParams);
        SetWindowLongPtrA(hwnd, GWLP_USERDATA, (LONG_PTR)g_editorInstance);
        return 0;
    }

    TokenStreamDisplay* pThis = reinterpret_cast<TokenStreamDisplay*>(
        GetWindowLongPtrA(hwnd, GWLP_USERDATA));

    if (pThis) {
        switch (msg) {
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            SetBkColor(hdc, RGB(30, 30, 30));   // Dark background
            SetTextColor(hdc, RGB(0, 255, 0));  // Green text (matrix style)
            TextOutA(hdc, 10, 10, pThis->getText().c_str(), pThis->getText().length());
            EndPaint(hwnd, &ps);
            return 0;
        }
        case WM_ERASEBKGND: {
            HDC hdc = (HDC)wParam;
            RECT rect;
            GetClientRect(hwnd, &rect);
            FillRect(hdc, &rect, (HBRUSH)GetStockObject(BLACK_BRUSH));
            return 1;
        }
        }
    }

    return DefWindowProcA(hwnd, msg, wParam, lParam);
}

TokenStreamDisplay::TokenStreamDisplay() {
}

TokenStreamDisplay::~TokenStreamDisplay() {
    destroy();
}

bool TokenStreamDisplay::create(HWND hParent, int x, int y, int width, int height) {
    // Register window class once
    if (!g_editorClassRegistered) {
        WNDCLASSA wc{};
        wc.lpfnWndProc = editorWndProc;
        wc.lpszClassName = g_editorClassName;
        wc.style = CS_VREDRAW | CS_HREDRAW;
        wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
        wc.hCursor = LoadCursorA(nullptr, IDC_IBEAM);

        if (!RegisterClassA(&wc)) {
            return false;
        }
        g_editorClassRegistered = true;
    }

    // Create window
    m_hEditor = CreateWindowExA(
        0,
        g_editorClassName,
        "Token Stream",
        WS_CHILD | WS_VISIBLE | WS_BORDER,
        x, y, width, height,
        hParent,
        nullptr,
        GetModuleHandleA(nullptr),
        this
    );

    if (!m_hEditor) {
        return false;
    }

    m_hParent = hParent;
    return true;
}

void TokenStreamDisplay::destroy() {
    if (m_hEditor) {
        DestroyWindow(m_hEditor);
        m_hEditor = nullptr;
    }
}

void TokenStreamDisplay::appendToken(const std::string& token) {
    m_buffer += token;
    InvalidateRect(m_hEditor, nullptr, FALSE);
    UpdateWindow(m_hEditor);
}

void TokenStreamDisplay::clear() {
    m_buffer.clear();
    InvalidateRect(m_hEditor, nullptr, TRUE);
    UpdateWindow(m_hEditor);
}

std::string TokenStreamDisplay::getText() const {
    return m_buffer;
}

void TokenStreamDisplay::scrollToEnd() {
    if (m_hEditor) {
        InvalidateRect(m_hEditor, nullptr, FALSE);
        UpdateWindow(m_hEditor);
    }
}

// ========== TelemetryDisplay ==========

static bool g_telemetryClassRegistered = false;
static const char* g_telemetryClassName = "RawrXD_TelemetryDisplayClass";

TelemetryDisplay::TelemetryDisplay() {
}

TelemetryDisplay::~TelemetryDisplay() {
    destroy();
}

bool TelemetryDisplay::create(HWND hParent, int x, int y, int width, int height) {
    // Register window class
    if (!g_telemetryClassRegistered) {
        WNDCLASSA wc{};
        wc.lpfnWndProc = DefWindowProcA;
        wc.lpszClassName = g_telemetryClassName;
        wc.hbrBackground = (HBRUSH)GetStockObject(LTGRAY_BRUSH);

        if (!RegisterClassA(&wc)) {
            return false;
        }
        g_telemetryClassRegistered = true;
    }

    // Create main status window
    m_hStatus = CreateWindowExA(
        0,
        g_telemetryClassName,
        "Inference Telemetry",
        WS_CHILD | WS_VISIBLE | WS_BORDER,
        x, y, width, height,
        hParent,
        nullptr,
        GetModuleHandleA(nullptr),
        nullptr
    );

    if (!m_hStatus) {
        return false;
    }

    // Create child label controls
    m_hTokenCount = CreateWindowExA(
        0, "STATIC", "Tokens: 0",
        WS_CHILD | WS_VISIBLE,
        5, 5, width - 10, 25,
        m_hStatus, nullptr, GetModuleHandleA(nullptr), nullptr
    );

    m_hLatency = CreateWindowExA(
        0, "STATIC", "Latency: 0ms",
        WS_CHILD | WS_VISIBLE,
        5, 35, width - 10, 25,
        m_hStatus, nullptr, GetModuleHandleA(nullptr), nullptr
    );

    m_hEngineStatus = CreateWindowExA(
        0, "STATIC", "Engine: Disconnected",
        WS_CHILD | WS_VISIBLE,
        5, 65, width - 10, 25,
        m_hStatus, nullptr, GetModuleHandleA(nullptr), nullptr
    );

    return true;
}

void TelemetryDisplay::destroy() {
    if (m_hStatus) {
        DestroyWindow(m_hStatus);
        m_hStatus = nullptr;
    }
}

void TelemetryDisplay::updateMetrics(const Metrics& metrics) {
    if (m_hTokenCount) {
        std::ostringstream oss;
        oss << "Tokens: " << metrics.totalTokens << " @ "
            << std::fixed << std::setprecision(1)
            << metrics.avgTokensPerSecond << " tok/s";
        SetWindowTextA(m_hTokenCount, oss.str().c_str());
    }

    if (m_hLatency) {
        std::ostringstream oss;
        oss << "Latency: " << metrics.totalLatencyMs << "ms";
        SetWindowTextA(m_hLatency, oss.str().c_str());
    }
}

void TelemetryDisplay::setEngineStatus(const std::string& status) {
    if (m_hEngineStatus) {
        std::string text = "Engine: " + status;
        SetWindowTextA(m_hEngineStatus, text.c_str());
    }
}

std::string TelemetryDisplay::formatMetrics(const Metrics& m) {
    std::ostringstream oss;
    oss << "Tokens: " << m.totalTokens << "\n"
        << "Latency: " << m.totalLatencyMs << "ms\n"
        << "Speed: " << std::fixed << std::setprecision(1)
        << m.avgTokensPerSecond << " tok/s\n"
        << "Engine: " << m.engineStatus;
    return oss.str();
}

}
