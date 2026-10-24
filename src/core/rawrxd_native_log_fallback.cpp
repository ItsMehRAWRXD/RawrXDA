#include <cstdarg>
#include <cstdio>
#include <cstring>

namespace {
constexpr size_t kNativeLogSlots = 32;
constexpr size_t kNativeLogLineCap = 256;
char g_nativeLogRing[kNativeLogSlots][kNativeLogLineCap]{};
unsigned g_nativeLogWriteIndex = 0;
}

#ifdef _WIN32
#include <windows.h>
#endif

extern "C" void RawrXD_Native_Log(const char* fmt, ...) {
    if (!fmt) {
        return;
    }
    char line[kNativeLogLineCap]{};
    va_list args;
    va_start(args, fmt);
    std::vsnprintf(line, sizeof(line), fmt, args);
    va_end(args);
    std::strncpy(g_nativeLogRing[g_nativeLogWriteIndex % kNativeLogSlots], line, kNativeLogLineCap - 1);
    g_nativeLogRing[g_nativeLogWriteIndex % kNativeLogSlots][kNativeLogLineCap - 1] = '\0';
    g_nativeLogWriteIndex += 1;

    va_list args;
    va_start(args, fmt);
    va_list copy;
    va_copy(copy, args);

    std::vfprintf(stderr, fmt, args);
    std::fputc('\n', stderr);

#ifdef _WIN32
    char buffer[2048] = {};
    std::vsnprintf(buffer, sizeof(buffer), fmt, copy);
    ::OutputDebugStringA(buffer);
    ::OutputDebugStringA("\n");
#endif

    va_end(copy);
    va_end(args);
}
