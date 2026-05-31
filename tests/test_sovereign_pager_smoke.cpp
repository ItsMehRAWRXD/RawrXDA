// Smoke test: Init / RegisterModel / Shutdown (no 64 MiB disk read).

#include <cstdio>
#include <memory>
#include <windows.h>

#include "../src/compression/sovereign_pager.h"

int main()
{
    // Pager embeds multi-megabyte tables; never place on the default thread stack.
    auto pager = std::make_unique<sov::SovereignPager>();
    if (!pager->Init(0, 0, nullptr, 0))
    {
        std::puts("FAIL Init");
        return 1;
    }

    const uint64_t offsets_mb[1] = {0};
    const uint32_t page_counts[1] = {0};
    if (!pager->RegisterModel(1, 1, offsets_mb, page_counts))
    {
        std::puts("FAIL RegisterModel");
        pager->Shutdown();
        return 1;
    }

    pager->Shutdown();
    std::puts("PASS sovereign_pager smoke");
    return 0;
}
