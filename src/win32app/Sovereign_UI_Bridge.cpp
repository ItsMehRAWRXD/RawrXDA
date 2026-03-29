#include "Win32IDE_SovereignBridge.h"

#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

namespace {
std::atomic<HWND> g_streamingBeaconUiNotifyWindow{nullptr};
}

namespace RawrXD {

void setStreamingBeaconUiNotifyWindow(HWND hwnd) {
    g_streamingBeaconUiNotifyWindow.store(hwnd, std::memory_order_release);
}

HWND getStreamingBeaconUiNotifyWindow() {
    return g_streamingBeaconUiNotifyWindow.load(std::memory_order_acquire);
}

void postSovereignStreamStart() {
    HWND hwnd = getStreamingBeaconUiNotifyWindow();
    if (hwnd && IsWindow(hwnd)) {
        PostMessageA(hwnd, WM_RAWRXD_SOVEREIGN_START, 0, 0);
    }
}

void postSovereignStreamToken(const char* token, size_t length) {
    HWND hwnd = getStreamingBeaconUiNotifyWindow();
    if (!hwnd || !IsWindow(hwnd) || !token) {
        return;
    }

    const size_t actualLength = length > 0 ? length : std::strlen(token);
    std::string tokenSlice(token, actualLength);
    char* payload = _strdup(tokenSlice.c_str());
    if (!payload) {
        return;
    }

    if (!PostMessageA(hwnd, WM_RAWRXD_SOVEREIGN_TOKEN, 0,
                      reinterpret_cast<LPARAM>(payload))) {
        std::free(payload);
    }
}

void postSovereignStreamSuccess(double tps, int totalTokens, const char* schemaType) {
    HWND hwnd = getStreamingBeaconUiNotifyWindow();
    SovereignTelemetry* telemetry = new (std::nothrow) SovereignTelemetry();
    if (!telemetry) {
        return;
    }

    telemetry->harness_sentinel = RAWRXD_SOVEREIGN_TELEMETRY_WPARAM;
    telemetry->tps = tps;
    telemetry->total_tokens = totalTokens;
    if (schemaType && schemaType[0] != '\0') {
        std::strncpy(telemetry->schema_type, schemaType, sizeof(telemetry->schema_type) - 1);
        telemetry->schema_type[sizeof(telemetry->schema_type) - 1] = '\0';
    }

    if (hwnd && IsWindow(hwnd)) {
        if (!PostMessageA(hwnd, WM_RAWRXD_SOVEREIGN_SUCCESS, 0,
                          reinterpret_cast<LPARAM>(telemetry))) {
            delete telemetry;
        }
        return;
    }

    std::printf("[SOVEREIGN_SUCCESS] 0x%llX TPS=%.2f\n",
                telemetry->harness_sentinel,
                telemetry->tps);
    std::fflush(stdout);
    delete telemetry;
}

} // namespace RawrXD

void Win32IDE::wireSovereignUiBridge() {
    RawrXD::setStreamingBeaconUiNotifyWindow(m_hwndMain);
}

void Win32IDE::onSovereignStreamingStart() {
    m_sovereignStreamingGlow.store(true, std::memory_order_release);
    if (m_hwndCopilotChatOutput && IsWindow(m_hwndCopilotChatOutput)) {
        LONG_PTR exStyle = GetWindowLongPtrA(m_hwndCopilotChatOutput, GWL_EXSTYLE);
        SetWindowLongPtrA(m_hwndCopilotChatOutput, GWL_EXSTYLE, exStyle | WS_EX_CLIENTEDGE);
        SetWindowPos(m_hwndCopilotChatOutput, nullptr, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
        InvalidateRect(m_hwndCopilotChatOutput, nullptr, TRUE);
    }
    if (m_hwndStatusBar && IsWindow(m_hwndStatusBar)) {
        SendMessageA(m_hwndStatusBar, SB_SETTEXTA, 0, reinterpret_cast<LPARAM>(const_cast<char*>("Sovereign diagnostic active")));
    }
}

void Win32IDE::onSovereignStreamingToken(const char* token) {
    if (!token || !m_hwndCopilotChatOutput || !IsWindow(m_hwndCopilotChatOutput)) {
        return;
    }

    int len = GetWindowTextLengthA(m_hwndCopilotChatOutput);
    SendMessageA(m_hwndCopilotChatOutput, EM_SETSEL, len, len);
    SendMessageA(m_hwndCopilotChatOutput, EM_REPLACESEL, FALSE, reinterpret_cast<LPARAM>(token));
    SendMessageA(m_hwndCopilotChatOutput, EM_SCROLLCARET, 0, 0);
}

void Win32IDE::onSovereignStreamingSuccess(const SovereignTelemetry& telemetry) {
    m_sovereignStreamingGlow.store(false, std::memory_order_release);
    if (m_hwndCopilotChatOutput && IsWindow(m_hwndCopilotChatOutput)) {
        LONG_PTR exStyle = GetWindowLongPtrA(m_hwndCopilotChatOutput, GWL_EXSTYLE);
        SetWindowLongPtrA(m_hwndCopilotChatOutput, GWL_EXSTYLE, exStyle & ~WS_EX_CLIENTEDGE);
        SetWindowPos(m_hwndCopilotChatOutput, nullptr, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
        InvalidateRect(m_hwndCopilotChatOutput, nullptr, TRUE);
    }
    if (m_hwndStatusBar && IsWindow(m_hwndStatusBar)) {
        char statusText[160] = {};
        std::snprintf(statusText, sizeof(statusText),
                      "Sovereign Success [0x%llX] - TPS: %.2f",
                      telemetry.harness_sentinel,
                      telemetry.tps);
        SendMessageA(m_hwndStatusBar, SB_SETTEXTA, 0, reinterpret_cast<LPARAM>(statusText));
    }
}
