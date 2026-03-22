#include <array>
#include <atomic>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include "../core/ssot_handlers.h"

namespace {
constexpr uint32_t WM_V280_GHOST_TEXT = 0x0400u + 280u;
std::array<char, 4096> g_v280GhostText = {};
std::atomic<bool> g_v280GhostActive{false};
}

extern "C" int64_t V280_UI_WndProc_Hook(void* hwnd, uint32_t uMsg, uint64_t wParam, int64_t lParam) {
    (void)hwnd;

    // Gate v280 shared-memory bridge by SSOT full beacon heartbeat.
    if (!isBeaconFullActive()) {
        return 0;
    }

    if (uMsg == WM_V280_GHOST_TEXT) {
        if (wParam == 0 || lParam == 0) {
            g_v280GhostText.fill(0);
            g_v280GhostActive.store(false, std::memory_order_release);
            return 1;
        }
        const char* incoming = reinterpret_cast<const char*>(lParam);
        if (incoming && incoming[0] != '\0') {
            std::snprintf(g_v280GhostText.data(), g_v280GhostText.size(), "%s", incoming);
            g_v280GhostActive.store(true, std::memory_order_release);
            return 1;
        }
        return 0;
    }

    if (uMsg == 0x0100u /* WM_KEYDOWN */ && g_v280GhostActive.load(std::memory_order_acquire)) {
        if (wParam == 0x1Bu /* VK_ESCAPE */ || wParam == 0x09u /* VK_TAB */) {
            g_v280GhostText.fill(0);
            g_v280GhostActive.store(false, std::memory_order_release);
            return 1;
        }
    }

    if (uMsg == 0x0002u /* WM_DESTROY */) {
        g_v280GhostText.fill(0);
        g_v280GhostActive.store(false, std::memory_order_release);
    }

    return 0;
}

extern "C" int V280_UI_IsGhostActive(void) {
    return g_v280GhostActive.load(std::memory_order_acquire) ? 1 : 0;
}

extern "C" int V280_UI_GetGhostText(char* buf, int buf_size) {
    if (!buf || buf_size <= 0) {
        return 0;
    }
    buf[0] = '\0';
    if (!g_v280GhostActive.load(std::memory_order_acquire)) {
        return 0;
    }
    std::snprintf(buf, static_cast<size_t>(buf_size), "%s", g_v280GhostText.data());
    return static_cast<int>(std::strlen(buf));
}

extern "C" void asm_quadbuf_shutdown(void) {}
extern "C" void asm_spengine_shutdown(void) {}
