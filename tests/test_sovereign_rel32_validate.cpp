// Validates REL32 ±int32_t reach helpers (x64 patch layer).

#include <cstdio>
#include <cstdint>

#include "rawrxd/sovereign_rel32_validate.hpp"
#include "rawrxd/sovereign_emit_x64.hpp"

using rawrxd::sovereign::emit::encodeRel32Displacement;
using rawrxd::sovereign::emit::kRel32DisplacementMax;
using rawrxd::sovereign::emit::kRel32DisplacementMin;
using rawrxd::sovereign::emit::rel32DisplacementFits;
using rawrxd::sovereign::emit::rel32ReachableFromRipAfter;
using rawrxd::sovereign::emit::ripRelativeDelta;
using rawrxd::sovereign::emit::tryEmitCallRel32;

namespace
{
bool fail(const char* msg)
{
    std::fprintf(stderr, "[test_sovereign_rel32_validate] FAIL: %s\n", msg);
    return false;
}
}  // namespace

int main()
{
    if (!rel32DisplacementFits(0))
    {
        return fail("zero");
    }
    if (!rel32DisplacementFits(kRel32DisplacementMin) || !rel32DisplacementFits(kRel32DisplacementMax))
    {
        return fail("endpoints");
    }
    if (rel32DisplacementFits(kRel32DisplacementMin - 1) || rel32DisplacementFits(kRel32DisplacementMax + 1))
    {
        return fail("outside range");
    }

    const std::uint64_t rip = 0x140001000ull;
    if (ripRelativeDelta(rip + 5u, rip + 5u + 0x100ull) != 0x100)
    {
        return fail("delta +");
    }
    if (!rel32ReachableFromRipAfter(rip + 5u, rip + 5u + static_cast<std::uint64_t>(kRel32DisplacementMax)))
    {
        return fail("max reach");
    }
    if (rel32ReachableFromRipAfter(rip + 5u, rip + 5u + static_cast<std::uint64_t>(kRel32DisplacementMax) + 1ull))
    {
        return fail("past max");
    }

    if (encodeRel32Displacement(42).value() != 42)
    {
        return fail("encode ok");
    }
    if (encodeRel32Displacement(kRel32DisplacementMax + 1).has_value())
    {
        return fail("encode fail expected");
    }

    std::uint8_t buf[8] = {};
    if (!tryEmitCallRel32(buf, rip + 5u, rip + 0x20u).has_value())
    {
        return fail("tryEmit near");
    }
    if (buf[0] != 0xE8)
    {
        return fail("E8 opcode");
    }

    std::fprintf(stderr, "[test_sovereign_rel32_validate] OK\n");
    return 0;
}
