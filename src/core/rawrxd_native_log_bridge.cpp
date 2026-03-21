#include <cstdarg>
#include <cstdio>
#include <cstring>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

extern "C" void RawrXD_Native_Log(const char* fmt, ...) {
    if (!fmt || fmt[0] == '\0') {
        return;
    }

    char message[2048] = {};
    va_list args;
    va_start(args, fmt);
#if defined(_MSC_VER)
    _vsnprintf_s(message, sizeof(message), _TRUNCATE, fmt, args);
#else
    vsnprintf(message, sizeof(message), fmt, args);
#endif
    va_end(args);

    std::fputs(message, stderr);
    const size_t len = std::strlen(message);
    if (len == 0 || message[len - 1] != '\n') {
        std::fputc('\n', stderr);
    }
    std::fflush(stderr);

#ifdef _WIN32
    OutputDebugStringA(message);
    if (len == 0 || message[len - 1] != '\n') {
        OutputDebugStringA("\n");
    }
#endif
}
