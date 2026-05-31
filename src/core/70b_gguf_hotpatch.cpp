#include "70b_gguf_hotpatch.h"

#include "../logging/Logger.h"

#include <cstdlib>

namespace RawrXD {

namespace {

bool isTruthyValue(const char* value)
{
    if (!value || !value[0]) {
        return false;
    }
    return value[0] == '1' || value[0] == 'y' || value[0] == 'Y' || value[0] == 't' || value[0] == 'T';
}

void ensureEnvDefault(const char* key, const char* defaultValue)
{
    char buf[8] = {};
    const DWORD len = GetEnvironmentVariableA(key, buf, static_cast<DWORD>(sizeof(buf)));
    if (len == 0 || len >= sizeof(buf)) {
        SetEnvironmentVariableA(key, defaultValue);
    }
}

} // namespace

bool GGUFHotpatch::apply70BGgufHotpatch()
{
    HMODULE exe = GetModuleHandleA("RawrXD-Win32IDE.exe");
    if (!exe) {
        RawrXD::Logging::Logger::instance().warning(
            "[GGUFHotpatch] Skipping 70B hotpatch: RawrXD-Win32IDE module not loaded",
            "GGUFHotpatch");
        return false;
    }

    // Keep these defaults opt-out: explicit user values continue to win.
    ensureEnvDefault("RAWRXD_PLACEHOLDER_ALIGN_2MB", "1");

    MEMORYSTATUSEX mem = {};
    mem.dwLength = sizeof(mem);
    if (GlobalMemoryStatusEx(&mem)) {
        const unsigned long long ramBytes = static_cast<unsigned long long>(mem.ullTotalPhys);
        const unsigned long long threshold = 24ull * 1024ull * 1024ull * 1024ull;
        if (ramBytes >= threshold) {
            ensureEnvDefault("RAWRXD_GGUF_USE_UNIFIED_MEMORY", "1");
        }
    }

    const char* forceMapTelemetry = std::getenv("RAWRXD_70B_HOTPATCH_TELEMETRY");
    if (isTruthyValue(forceMapTelemetry)) {
        ensureEnvDefault("RAWRXD_GGUF_MAP_TELEMETRY", "1");
    }

    RawrXD::Logging::Logger::instance().info(
        "[GGUFHotpatch] 70B runtime defaults applied (alignment + optional unified memory)",
        "GGUFHotpatch");
    return true;
}

} // namespace RawrXD
