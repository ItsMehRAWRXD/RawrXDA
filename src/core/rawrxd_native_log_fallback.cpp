#include <array>
#include <cstdarg>
#include <cstdio>
#include <string>
#include <windows.h>

extern "C" void RawrXD_Native_Log(const char* fmt, ...) {
    if (fmt == nullptr) {
        return;
    }

    std::array<char, 2048> stackBuffer = {};
    va_list args;
    va_start(args, fmt);
    const int written = std::vsnprintf(stackBuffer.data(), stackBuffer.size(), fmt, args);
    va_end(args);
    if (written < 0) {
        return;
    }

    const char* text = stackBuffer.data();
    std::string heapBuffer;
    if (static_cast<size_t>(written) >= stackBuffer.size()) {
        heapBuffer.resize(static_cast<size_t>(written) + 1U, '\0');
        va_start(args, fmt);
        std::vsnprintf(heapBuffer.data(), heapBuffer.size(), fmt, args);
        va_end(args);
        text = heapBuffer.c_str();
    }

    std::fputs(text, stderr);
    std::fputc('\n', stderr);
    OutputDebugStringA(text);
    OutputDebugStringA("\n");
}
