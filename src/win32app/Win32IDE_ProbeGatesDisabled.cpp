// Compiled when RAWRXD_ENABLE_IDE_PROBE_GATES=OFF — satisfies main_win32 probe symbols without harness TUs.

#include "Win32IDE_KVApertureProbe.h"
#include "Win32IDE_TextEngineProbe.h"
#include "Win32IDE_TokenTickProbe.h"
#include "sovereign/sovereign_smoketests.h"

bool HasKVApertureProbeFlag(const char*)
{
    return false;
}

int RunKVApertureProbe(const char*, std::uint64_t, int, std::uint64_t)
{
    return 2;
}

bool HasTextEngineProbeFlag(const char*)
{
    return false;
}

int RunTextEngineProbe(const char*, int)
{
    return 2;
}

bool HasTokenTickFlag(const char*)
{
    return false;
}

int RunTokenTickProbe(const char*, std::uint64_t, int, std::uint32_t, std::uint32_t, std::uint32_t)
{
    return 2;
}

namespace RawrXD::Tests
{

int RunSmoketests(const wchar_t*)
{
    return 2;
}

}  // namespace RawrXD::Tests
