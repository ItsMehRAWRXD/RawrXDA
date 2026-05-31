#pragma once
#include <memory>
#include <string>

// Forward declare — avoids pulling Vulkan stub conflicts into the smoke harness TU.
namespace RawrXD { class CPUInferenceEngine; }
namespace AgenticEngineNS { class AgenticEngine; }
class AgenticEngine;

namespace SmokeTestBridge
{
    /// Returns (or creates) the process-wide shared CPUInferenceEngine.
    std::shared_ptr<RawrXD::CPUInferenceEngine> GetInferenceEngine();

    /// Calls engine->LoadModel(modelPath). Returns true on success.
    bool LoadModel(std::shared_ptr<RawrXD::CPUInferenceEngine>& engine,
                   const std::string& modelPath,
                   std::string& outError);
}
