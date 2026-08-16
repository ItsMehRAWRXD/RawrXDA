#include <cstdint>
#include <cstring>

extern "C" int64_t V280_UI_WndProc_Hook(void* hwnd, uint32_t uMsg, uint64_t wParam, int64_t lParam) {
    (void)wParam;
    (void)lParam;
    // WM_SETTEXT(0x000C) and custom ghost refresh are treated as handled in fallback mode.
    if (hwnd == nullptr) {
        return 0;
    }
    if (uMsg == 0x000C || uMsg == 0x0400 + 0x120) {
        return 1;
    }
    return 0;
}

extern "C" int V280_UI_IsGhostActive(void) {
    return 1;
}

extern "C" int V280_UI_GetGhostText(char* buf, int buf_size) {
    static const char kGhostText[] = "Fallback ghost suggestion active";
    if (buf != nullptr && buf_size > 0) {
        const int n = (buf_size > static_cast<int>(sizeof(kGhostText)))
                          ? static_cast<int>(sizeof(kGhostText))
                          : (buf_size - 1);
        if (n > 0) {
            std::memcpy(buf, kGhostText, static_cast<size_t>(n));
            buf[n] = '\0';
        } else {
            buf[0] = '\0';
        }
    }
    return 1;
}

namespace {
static int g_v280QuadbufShutdownCount = 0;
static int g_v280SpengineShutdownCount = 0;
}

extern "C" void asm_quadbuf_shutdown(void) {
    g_v280QuadbufShutdownCount += 1;
}
extern "C" void asm_spengine_shutdown(void) {
    g_v280SpengineShutdownCount += 1;
}
