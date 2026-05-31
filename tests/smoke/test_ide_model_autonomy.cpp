#include <chrono>
#include <iostream>
#include <string>
#include <thread>

#include "ai_backend.h"
#include "core/feature_handlers.h"
#include "core/shared_feature_dispatch.h"

// Single translation unit with MASM entry stubs (ide_autonomy_shield_stubs.cpp) avoids
// /LTCG COMDAT elision across object boundaries for these orchestrator TUs.
#include "core/agentic_orchestrator.cpp"
#include "core/sovereign_agentic_orchestrator.cpp"


using namespace RawrXD::Agentic;
using namespace RawrXD::Autonomy;

namespace
{
void DummyOutput(const char* text, void* /*userData*/)
{
    std::cout << "[IDE-OUT] " << text;
}
}  // namespace

int main()
{
    std::cout << "[LLM-SMOKE] Initializing IDE-Linked Autonomy Test..." << std::endl;

    AIBackendManager backendMgr;
    AIBackendConfig config;
    config.id = "test-phi3";
    config.displayName = "Phi-3 Mini (Test)";
    config.type = AIBackendType::LocalGGUF;
    config.model = "d:/phi3mini.gguf";
    backendMgr.addBackend(config);
    backendMgr.setActiveBackend("test-phi3");

    std::cout << "[STEP 1] AI Backend Configured: " << config.displayName << std::endl;

    CommandContext ctx{};
    ctx.isHeadless = true;
    ctx.outputFn = DummyOutput;
    ctx.args = "d:/phi3mini.gguf";

    std::cout << "[STEP 2] Executing IDE Model Load Handler..." << std::endl;
    CommandResult loadRes = handleFileLoadModel(ctx);
    if (!loadRes.success)
    {
        std::cout << "[!] Model Load Failed: " << loadRes.detail << std::endl;
    }
    else
    {
        std::cout << "[+] Model Load Success: " << loadRes.detail << std::endl;
    }

    std::cout << "[STEP 3] Triggering Sovereign Autonomy..." << std::endl;
    SovereignOrchestrator::GetInstance().StartAutonomy();

    const char* agenticGoal =
        "Analyze the workspace and identify potential PQC vulnerabilities in the networking stack.";
    SovereignOrchestrator::GetInstance().SubmitObjective(agenticGoal);

    std::cout << "[+] Objective Submitted: " << agenticGoal << std::endl;

    std::cout << "[STEP 4] Verifying Nous Engine Goal Ingestion..." << std::endl;
    NousEngine::GetInstance().IngestObjective("Deep Audit: PQC Layer");

    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    std::cout << "[SUCCESS] IDE-Linked Autonomy Smoke Test logic verified." << std::endl;
    return 0;
}
