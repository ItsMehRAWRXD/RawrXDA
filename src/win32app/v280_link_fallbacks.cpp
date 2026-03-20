#include <cstdint>

extern "C" int64_t V280_UI_WndProc_Hook(void* hwnd, uint32_t uMsg, uint64_t wParam, int64_t lParam) {
    (void)hwnd;
    (void)uMsg;
    (void)wParam;
    (void)lParam;
    return 0;
}

extern "C" int V280_UI_IsGhostActive(void) {
    return 0;
}

extern "C" int V280_UI_GetGhostText(char* buf, int buf_size) {
    if (buf != nullptr && buf_size > 0) {
        buf[0] = '\0';
    }
    return 0;
}

namespace {
static int g_v280QuadbufShutdownCount = 0;
}

extern "C" void asm_quadbuf_shutdown(void) {
    g_v280QuadbufShutdownCount += 1;
}
extern "C" void asm_spengine_shutdown(void) {}
