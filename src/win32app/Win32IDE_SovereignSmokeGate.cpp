// IDE link gate: --sovereign-smoke CLI is handled by dedicated harness binaries.

#include "sovereign/sovereign_smoketests.h"

#include <cstdio>

namespace RawrXD::Tests
{

int RunSmoketests(const wchar_t* /*journalPath*/)
{
    fprintf(stderr, "[sovereign-smoke] Full sovereign smoketest suite is not linked into Win32IDE; "
                    "use RawrXD-SovereignSmoke or build with the sovereign harness target.\n");
    return 2;
}

}  // namespace RawrXD::Tests
