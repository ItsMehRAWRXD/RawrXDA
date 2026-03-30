#include <cstdint>
#include <iostream>
#include <memory>
#include <string>

#include "logger.h"
#include "metrics.h"
#include "real_time_completion_engine.h"
#include "test_completion_support.hpp"

namespace {

bool runFreshSnapshotCase() {
    auto logger = std::make_shared<Logger>();
    auto metrics = std::make_shared<Metrics>();
    TestInferenceEngine engine;
    engine.LoadModel("fake://prompt-test");

    RealTimeCompletionEngine completionEngine(logger, metrics);
    completionEngine.setInferenceEngine(&engine);

    ExecutionStateSnapshot snapshot;
    snapshot.valid = true;
    snapshot.capturedAtUnixMs = testNowUnixMs();
    snapshot.threadId = "worker-7";
    snapshot.stackFrames = {"main.cpp:42", "helper.cpp:12"};
    snapshot.registers = {{"rip", "0x401000"}, {"rax", "0x2A"}};
    snapshot.notes = "debugger_state=stopped";

    completionEngine.setExecutionStateProvider(std::make_shared<StaticExecutionStateProvider>(snapshot));
    completionEngine.getCompletions("int x = ", ";", "cpp", "unit-test-context");

    const std::string& prompt = engine.lastPrompt();
        return testContains(prompt, "[Execution State]") &&
            testContains(prompt, "thread=worker-7") &&
            testContains(prompt, "stack_frames:") &&
            testContains(prompt, "#0 main.cpp:42") &&
            testContains(prompt, "registers:") &&
            testContains(prompt, "rip=0x401000") &&
            testContains(prompt, "notes: debugger_state=stopped") &&
            testContains(prompt, "unit-test-context");
}

bool runStaleSnapshotCase() {
    auto logger = std::make_shared<Logger>();
    auto metrics = std::make_shared<Metrics>();
        TestInferenceEngine engine;
    engine.LoadModel("fake://prompt-test-stale");

    RealTimeCompletionEngine completionEngine(logger, metrics);
    completionEngine.setInferenceEngine(&engine);

    ExecutionStateSnapshot snapshot;
    snapshot.valid = true;
    snapshot.capturedAtUnixMs = testNowUnixMs() - 5000;
    snapshot.threadId = "stale-thread";
    snapshot.stackFrames = {"stale_frame"};
    snapshot.registers = {{"rip", "0xDEAD"}};
    snapshot.notes = "stale";

    completionEngine.setExecutionStateProvider(std::make_shared<StaticExecutionStateProvider>(snapshot));
    completionEngine.getCompletions("int y = ", ";", "cpp", "stale-context");

    const std::string& prompt = engine.lastPrompt();
        return !testContains(prompt, "[Execution State]") &&
            !testContains(prompt, "stale-thread") &&
            testContains(prompt, "stale-context");
}

bool runInvalidSnapshotCase() {
    auto logger = std::make_shared<Logger>();
    auto metrics = std::make_shared<Metrics>();
        TestInferenceEngine engine;
    engine.LoadModel("fake://prompt-test-invalid");

    RealTimeCompletionEngine completionEngine(logger, metrics);
    completionEngine.setInferenceEngine(&engine);

    ExecutionStateSnapshot snapshot;
    snapshot.valid = false;
    snapshot.capturedAtUnixMs = testNowUnixMs();
    snapshot.threadId = "invalid-thread";
    snapshot.notes = "invalid";

    completionEngine.setExecutionStateProvider(std::make_shared<StaticExecutionStateProvider>(snapshot));
    completionEngine.getCompletions("int z = ", ";", "cpp", "invalid-context");

    const std::string& prompt = engine.lastPrompt();
        return !testContains(prompt, "[Execution State]") &&
            !testContains(prompt, "invalid-thread") &&
            testContains(prompt, "invalid-context");
}

bool runTruncationCase() {
    auto logger = std::make_shared<Logger>();
    auto metrics = std::make_shared<Metrics>();
        TestInferenceEngine engine;
    engine.LoadModel("fake://prompt-test-truncation");

    RealTimeCompletionEngine completionEngine(logger, metrics);
    completionEngine.setInferenceEngine(&engine);

    ExecutionStateSnapshot snapshot;
    snapshot.valid = true;
    snapshot.capturedAtUnixMs = testNowUnixMs();
    snapshot.threadId = "truncation-thread";
    snapshot.notes = "truncation-check";

    for (int i = 0; i < 20; ++i) {
        snapshot.stackFrames.push_back("frame_" + std::to_string(i));
    }
    for (int i = 0; i < 20; ++i) {
        snapshot.registers.emplace_back("reg" + std::to_string(i), "0x" + std::to_string(i));
    }

    completionEngine.setExecutionStateProvider(std::make_shared<StaticExecutionStateProvider>(snapshot));
    completionEngine.getCompletions("int w = ", ";", "cpp", "truncation-context");

    const std::string& prompt = engine.lastPrompt();
        return testContains(prompt, "#0 frame_0") &&
            testContains(prompt, "#11 frame_11") &&
            !testContains(prompt, "#12 frame_12") &&
            testContains(prompt, "reg0=0x0") &&
            testContains(prompt, "reg15=0x15") &&
            !testContains(prompt, "reg16=0x16") &&
            testContains(prompt, "truncation-context");
}

}  // namespace

int main() {
    std::cout << "\n=== Testing Execution State Prompt Injection ===\n\n";

    const bool freshOk = runFreshSnapshotCase();
    std::cout << (freshOk ? "PASS" : "FAIL") << ": fresh execution state snapshot is injected\n";

    const bool staleOk = runStaleSnapshotCase();
    std::cout << (staleOk ? "PASS" : "FAIL") << ": stale execution state snapshot is excluded\n";

    const bool invalidOk = runInvalidSnapshotCase();
    std::cout << (invalidOk ? "PASS" : "FAIL") << ": invalid execution state snapshot is excluded\n";

    const bool truncationOk = runTruncationCase();
    std::cout << (truncationOk ? "PASS" : "FAIL") << ": execution state prompt truncates frames and registers at configured limits\n";

    if (!(freshOk && staleOk && invalidOk && truncationOk)) {
        std::cerr << "\nExecution state prompt injection test failed.\n";
        return 1;
    }

    std::cout << "\nAll execution-state prompt injection checks passed.\n";
    return 0;
}