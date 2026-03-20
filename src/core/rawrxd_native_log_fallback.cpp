#include <cstdarg>
#include <cstdio>

#ifdef _WIN32
#include <windows.h>
#endif

extern "C" void RawrXD_Native_Log(const char* fmt, ...) {
    if (!fmt) {
        return;
    }

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
