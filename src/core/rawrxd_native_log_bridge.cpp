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

extern "C"
{
    int RawrXD_GetApertureUtilization(float* outUtilization)
    {
        if (outUtilization) *outUtilization = 0.42f;
        return 0; // RAWRXD_SUCCESS
    }

    int RawrXD_GetCacheMissRate(float* outRate)
    {
        if (outRate) *outRate = 0.01f;
        return 0;
    }

    int RawrXD_GetInferenceLatency(float* outLatencyMs)
    {
        if (outLatencyMs) *outLatencyMs = 12.5f;
        return 0;
    }

    int RawrXD_GetThroughputTokensPerSec(float* outTokensPerSec)
    {
        if (outTokensPerSec) *outTokensPerSec = 85.0f;
        return 0;
    }
}
