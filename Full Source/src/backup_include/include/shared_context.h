#pragma once
#include <memory>
#include "memory_core.h"
#include "hot_patcher.h"
#include "vsix_loader.h"
#include "engine/rawr_engine.h"

struct GlobalContext {
    std::unique_ptr<MemoryCore> memory;
    std::unique_ptr<HotPatcher> patcher;
    VSIXLoader* vsix_loader; // VSIXLoader is a singleton in original code (GetInstance)

    static GlobalContext& Get();
};
