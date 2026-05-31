// smoke_engine_bridge.cpp
// Isolates cpu_inference_engine.h (which pulls rawrxd_inference.h Vk stubs)
// into a single TU where vulkan_compute.h is included first, preventing
// the VkPhysicalDeviceProperties struct redefinition conflict.
//
// Rule: this file must NOT include gguf_loader.h or streaming_gguf_loader.h
// before cpu_inference_engine.h.

// vulkan_compute.h must come first — it defines the authoritative Vk stubs.
#include "../../include/vulkan_compute.h"
// Now rawrxd_inference.h's stubs will see duplicate typedefs but we suppress them:
#define RAWRXD_VK_STUBS_INCLUDED 1
#include "../cpu_inference_engine.h"

#include "smoke_engine_bridge.h"

namespace SmokeTestBridge
{

std::shared_ptr<RawrXD::CPUInferenceEngine> GetInferenceEngine()
{
    return RawrXD::CPUInferenceEngine::GetSharedInstance();
}

bool LoadModel(std::shared_ptr<RawrXD::CPUInferenceEngine>& engine,
               const std::string& modelPath,
               std::string& outError)
{
    if (!engine)
        engine = RawrXD::CPUInferenceEngine::GetSharedInstance();
    if (!engine->LoadModel(modelPath))
    {
        outError = engine->GetLastLoadErrorMessage();
        return false;
    }
    return true;
}

} // namespace SmokeTestBridge
