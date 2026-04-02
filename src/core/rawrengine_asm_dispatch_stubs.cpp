// rawrengine_asm_dispatch_stubs.cpp
// RawrEngine headless lane: satisfy MASM dispatch bridge symbols.
// These are intentionally minimal; real dispatch is provided by SSOT/unified dispatch in other targets.

#include <cstdio>

extern "C" void rawrxd_dispatch_cli()
{
    std::fputs("[RawrEngine] rawrxd_dispatch_cli stub\n", stderr);
}

extern "C" void rawrxd_dispatch_command()
{
    std::fputs("[RawrEngine] rawrxd_dispatch_command stub\n", stderr);
}

extern "C" void rawrxd_dispatch_feature()
{
    std::fputs("[RawrEngine] rawrxd_dispatch_feature stub\n", stderr);
}

extern "C" unsigned __int64 rawrxd_get_feature_count()
{
    return 0ULL;
}
