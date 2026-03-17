#pragma once
#include <memory>
#include "memory_core.h"
#include "hot_patcher.h"
#include "vsix_loader.h"
#include "agentic_engine.h"
#include "engine/rawr_engine.h"

// Forward declaration
namespace CPUInference {
    class CPUInferenceEngine;
}

struct GlobalContext {
    std::unique_ptr<MemoryCore> memory;
    std::unique_ptr<HotPatcher> patcher;
    std::unique_ptr<AgenticEngine> agent_engine;
    CPUInference::CPUInferenceEngine* inference_engine = nullptr; // Managed externally
    VSIXLoader* vsix_loader; // VSIXLoader is a singleton in original code (GetInstance)

    static GlobalContext& Get();
};
