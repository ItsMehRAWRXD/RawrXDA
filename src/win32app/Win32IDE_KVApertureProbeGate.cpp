// IDE link gate: main_win32 references KV aperture probe entry points.
// Full Vulkan aperture harness lives in dedicated tools; IDE build uses no-op gate.

#include "Win32IDE_KVApertureProbe.h"

#include <cstdio>
#include <cstring>
#include <windows.h>

bool HasKVApertureProbeFlag(const char* cmdLine)
{
    auto matches = [](const char* text)
    {
        return text != nullptr && (std::strstr(text, "--kv-aperture-probe") != nullptr ||
                                   std::strstr(text, "--kv-aperture-telemetry") != nullptr);
    };
    if (matches(cmdLine))
    {
        return true;
    }
    return matches(GetCommandLineA());
}

int RunKVApertureProbe(const char* /*telemetryPath*/, std::uint64_t /*expectedQuietZoneVa*/, int /*cycles*/,
                       std::uint64_t /*bytes*/)
{
    fprintf(stderr, "[kv-aperture] Vulkan KV aperture probe is not linked in Win32IDE; "
                    "build a dedicated harness target for full aperture validation.\n");
    return 2;
}
