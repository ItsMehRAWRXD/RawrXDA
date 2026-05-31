// IDE link gate: token-per-tick harness entry points (Lane H).

#include "Win32IDE_TokenTickProbe.h"

#include <cstdio>
#include <cstring>
#include <windows.h>

bool HasTokenTickFlag(const char* cmdLine)
{
    auto matches = [](const char* text)
    {
        return text != nullptr && (std::strstr(text, "--token-tick-probe") != nullptr ||
                                   std::strstr(text, "--token-tick-telemetry") != nullptr);
    };
    if (matches(cmdLine))
    {
        return true;
    }
    return matches(GetCommandLineA());
}

int RunTokenTickProbe(const char* /*telemetryPath*/, std::uint64_t /*expectedQuietZoneVa*/, int /*cycles*/,
                      std::uint32_t /*tickHz*/, std::uint32_t /*nominalTps*/, std::uint32_t /*draftAcceptRatePermille*/)
{
    fprintf(stderr, "[token-tick] Token tick probe is not linked into Win32IDE; use a dedicated harness build.\n");
    return 2;
}
