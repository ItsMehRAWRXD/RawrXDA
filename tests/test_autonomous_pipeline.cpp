/**
 * Unit test for RawrXD Autonomous Agentic Pipeline Coordinator.
 * Build with: include Ship/ in include path, link with RawrXD_AutonomousAgenticPipeline.obj
 * Run: pipeline run with mocks succeeds and stats are updated.
 */
#include "RawrXD_AutonomousAgenticPipeline.h"
#include <cassert>
#include <iostream>
#include <string>

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;
    using namespace RawrXD;

    AutonomousAgenticPipelineCoordinator coord;
    std::string promptBuilt;
    std::string responseRendered;
    int tokenCalls = 0;

    coord.setBuildPrompt([&promptBuilt](const std::string& m) {
        promptBuilt = "<|user|>\n" + m + "\n<|end|>\n<|assistant|>\n";
        return promptBuilt;
    });
    coord.setRouteLLM([&responseRendered](const std::string&) {
        responseRendered = "Mock LLM response.";
        return responseRendered;
    });
    coord.setOnToken([&tokenCalls](const std::string&, bool) { ++tokenCalls; });
    coord.setAppendRenderer([&responseRendered](const std::string& s) { responseRendered = s; });

    auto result = coord.runPipeline("test message");
    assert(result.success && "runPipeline should succeed with mocks");
    assert(result.value == "Mock LLM response." && "response should match mock");
    assert(!promptBuilt.empty() && "prompt should have been built");

    auto stats = coord.getStats();
    assert(stats.pipelineRuns == 1 && "one run");
    assert(stats.pipelineFailures == 0 && "no failures");
    assert(stats.tokensStreamed >= 1 && "tokens streamed");

    std::cout << "test_autonomous_pipeline: PASS (runs=" << stats.pipelineRuns
              << " tokens=" << stats.tokensStreamed << ")\n";
    return 0;
}
