#include <cstdint>
#include <cstring>
#include <windows.h>

namespace {
bool g_v280GhostActive = false;
char g_v280GhostText[1024] = {};
} // namespace

extern "C" int64_t V280_UI_WndProc_Hook(void* hwnd, uint32_t uMsg, uint64_t wParam, int64_t lParam) {
    (void)hwnd;
    (void)wParam;
    if (uMsg == WM_SETTEXT && lParam != 0) {
        const char* text = reinterpret_cast<const char*>(lParam);
        std::strncpy(g_v280GhostText, text, sizeof(g_v280GhostText) - 1);
        g_v280GhostText[sizeof(g_v280GhostText) - 1] = '\0';
        g_v280GhostActive = g_v280GhostText[0] != '\0';
    } else if (uMsg == WM_KILLFOCUS || uMsg == WM_DESTROY) {
        g_v280GhostText[0] = '\0';
        g_v280GhostActive = false;
    }
    return 0;
}

extern "C" int V280_UI_IsGhostActive(void) {
    return g_v280GhostActive ? 1 : 0;
}

extern "C" int V280_UI_GetGhostText(char* buf, int buf_size) {
    if (buf == nullptr || buf_size <= 0) {
        return 0;
    }
    std::strncpy(buf, g_v280GhostText, static_cast<size_t>(buf_size) - 1U);
    buf[buf_size - 1] = '\0';
    return static_cast<int>(std::strlen(buf));
}

extern "C" void asm_quadbuf_shutdown(void) {}
extern "C" void asm_spengine_shutdown(void) {}
