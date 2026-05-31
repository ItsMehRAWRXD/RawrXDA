// IDE link gate: main_win32 references probe entry points; use RawrXD-TextEngineHarness for benchmarks.

#include "Win32IDE_TextEngineProbe.h"

#include <cstdio>
#include <cstring>
#include <windows.h>

bool HasTextEngineProbeFlag(const char* cmdLine)
{
    auto matches = [](const char* text)
    {
        return text != nullptr && (std::strstr(text, "--text-engine-telemetry") != nullptr ||
                                   std::strstr(text, "--text-engine-invalidation-telemetry") != nullptr ||
                                   std::strstr(text, "--text-engine-probe") != nullptr);
    };
    if (matches(cmdLine))
    {
        return true;
    }
    return matches(GetCommandLineA());
}

int RunTextEngineProbe(const char* /*telemetryPath*/, int /*cycles*/)
{
    fprintf(stderr, "[text-engine] Use RawrXD-TextEngineHarness.exe for layout probes "
                    "(--text-engine-telemetry).\n");
    return 2;
}

int RunTextEngineInvalidationProbe(const char* /*telemetryPath*/, int /*cycles*/)
{
    fprintf(stderr, "[text-engine] Use RawrXD-TextEngineHarness.exe for invalidation probes "
                    "(--text-engine-invalidation-telemetry).\n");
    return 2;
}
