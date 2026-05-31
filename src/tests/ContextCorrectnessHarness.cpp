// ContextCorrectnessHarness.cpp — Implementation of runtime correctness validation
// This is the test harness that validates IDE coherence under real-world conditions.

#include "ContextCorrectnessHarness.h"
#include <iostream>
#include <sstream>

namespace RawrXD {

// ─────────────────────────────────────────────────────────────────────────────
// Additional Stress Tests
// ─────────────────────────────────────────────────────────────────────────────

CorrectnessResult ContextCorrectnessHarness::TestGhostTextDivergence() {
    CorrectnessResult result;
    result.testName = "GhostTextDivergence";
    
    m_engine.Initialize();
    TestSubscriber ghostSub("GhostText", 10);  // High priority
    TestSubscriber chatSub("ChatPanel", 20);   // Lower priority
    TestSubscriber agentSub("AgentSystem", 30); // Lowest priority
    
    m_engine.Subscribe(&ghostSub);
    m_engine.Subscribe(&chatSub);
    m_engine.Subscribe(&agentSub);
    
    // Simulate rapid editing while AI is generating
    std::string baseText = "function test() {";
    
    // Editor changes
    ContextEvent editEvent(ContextEvent::EDITOR_CHANGED, "editor", &baseText);
    m_engine.EmitEvent(editEvent);
    
    // AI generates ghost text
    AIInteraction ghostInteraction;
    ghostInteraction.prompt = "complete";
    ghostInteraction.response = "\n  return true;\n}";
    ghostInteraction.confidence = 0.85f;
    ContextEvent aiEvent(ContextEvent::AI_RESPONSE, "ai", &ghostInteraction);
    m_engine.EmitEvent(aiEvent);
    
    // User continues typing (should invalidate ghost text)
    std::string updatedText = baseText + "\n  console.log('test');";
    ContextEvent editEvent2(ContextEvent::EDITOR_CHANGED, "editor", &updatedText);
    m_engine.EmitEvent(editEvent2);
    
    // Wait for all subscribers
    ghostSub.WaitForFrames(3);
    chatSub.WaitForFrames(3);
    agentSub.WaitForFrames(3);
    
    // All subscribers should see same sequence of frames
    auto ghostFrames = ghostSub.GetReceivedFrames();
    auto chatFrames = chatSub.GetReceivedFrames();
    auto agentFrames = agentSub.GetReceivedFrames();
    
    if (ghostFrames.size() != chatFrames.size() || chatFrames.size() != agentFrames.size()) {
        result.Fail("Subscribers received different frame counts");
        result.metrics["ghost_frames"] = std::to_string(ghostFrames.size());
        result.metrics["chat_frames"] = std::to_string(chatFrames.size());
        result.metrics["agent_frames"] = std::to_string(agentFrames.size());
        return result;
    }
    
    // Verify frame consistency
    for (size_t i = 0; i < ghostFrames.size(); i++) {
        if (ghostFrames[i].version != chatFrames[i].version || 
            chatFrames[i].version != agentFrames[i].version) {
            result.Fail("Frame version mismatch at index " + std::to_string(i));
            return result;
        }
        
        if (ghostFrames[i].bufferText != chatFrames[i].bufferText ||
            chatFrames[i].bufferText != agentFrames[i].bufferText) {
            result.Fail("Frame content mismatch at index " + std::to_string(i));
            return result;
        }
    }
    
    m_engine.Unsubscribe(&ghostSub);
    m_engine.Unsubscribe(&chatSub);
    m_engine.Unsubscribe(&agentSub);
    result.passed = true;
    return result;
}

CorrectnessResult ContextCorrectnessHarness::TestLSPDesyncUnderFastEdits() {
    CorrectnessResult result;
    result.testName = "LSPDesyncUnderFastEdits";
    
    m_engine.Initialize();
    TestSubscriber subscriber("LSPDesyncTest", 10);
    m_engine.Subscribe(&subscriber);
    
    // Simulate fast edits that might cause LSP desync
    const int FAST_EDIT_COUNT = 500;
    
    for (int i = 0; i < FAST_EDIT_COUNT; i++) {
        // Editor change
        std::string text = "line_" + std::to_string(i) + "\n";
        ContextEvent editEvent(ContextEvent::EDITOR_CHANGED, "editor", &text);
        m_engine.EmitEvent(editEvent);
        
        // LSP update (might arrive out of order in real scenario)
        std::vector<SymbolInfo> symbols;
        symbols.push_back({"symbol_" + std::to_string(i), "function", i % 100, 0, ""});
        ContextEvent lspEvent(ContextEvent::LSP_UPDATED, "lsp", &symbols);
        m_engine.EmitEvent(lspEvent);
        
        // Cursor move
        EditorPosition pos{i % 100, i % 80};
        ContextEvent cursorEvent(ContextEvent::CURSOR_MOVED, "editor", &pos);
        m_engine.EmitEvent(cursorEvent);
    }
    
    // Wait for all frames
    subscriber.WaitForFrames(FAST_EDIT_COUNT * 3, 10000);
    
    auto metrics = subscriber.GetMetrics();
    result.metrics["total_events"] = std::to_string(FAST_EDIT_COUNT * 3);
    result.metrics["frames_received"] = std::to_string(subscriber.GetFrameCount());
    result.metrics["version_skips"] = std::to_string(metrics.versionSkips);
    result.metrics["out_of_order"] = std::to_string(metrics.outOfOrderFrames);
    
    // Check for desync (version should be monotonically increasing)
    const auto& frames = subscriber.GetReceivedFrames();
    uint64_t lastVersion = 0;
    int desyncCount = 0;
    
    for (const auto& frame : frames) {
        if (frame.version <= lastVersion) {
            desyncCount++;
        }
        lastVersion = frame.version;
    }
    
    result.metrics["desync_count"] = std::to_string(desyncCount);
    
    if (desyncCount > 0) {
        result.Fail("LSP desync detected: " + std::to_string(desyncCount) + " frames out of order");
        return result;
    }
    
    m_engine.Unsubscribe(&subscriber);
    result.passed = true;
    return result;
}

CorrectnessResult ContextCorrectnessHarness::TestAIHallucinationFromStaleFrames() {
    CorrectnessResult result;
    result.testName = "AIHallucinationFromStaleFrames";
    
    m_engine.Initialize();
    TestSubscriber subscriber("AIHallucinationTest", 10);
    m_engine.Subscribe(&subscriber);
    
    // Simulate scenario where AI might use stale context
    
    // 1. Initial state
    std::string text = "function oldName() { return 1; }";
    ContextEvent editEvent(ContextEvent::EDITOR_CHANGED, "editor", &text);
    m_engine.EmitEvent(editEvent);
    
    // 2. LSP provides symbols for old state
    std::vector<SymbolInfo> oldSymbols;
    oldSymbols.push_back({"oldName", "function", 1, 0, ""});
    ContextEvent lspEvent(ContextEvent::LSP_UPDATED, "lsp", &oldSymbols);
    m_engine.EmitEvent(lspEvent);
    
    // 3. User renames function (fast edit)
    std::string newText = "function newName() { return 1; }";
    ContextEvent editEvent2(ContextEvent::EDITOR_CHANGED, "editor", &newText);
    m_engine.EmitEvent(editEvent2);
    
    // 4. AI tries to complete based on context
    // It should see the NEW context, not the old one
    ContextFrame currentFrame = m_engine.GetFrameCopy();
    
    if (currentFrame.bufferText != newText) {
        result.Fail("AI would see stale buffer text");
        return result;
    }
    
    // The LSP symbols might still be stale (that's expected)
    // But the AI should know to check frame version/timestamp
    
    // 5. Verify frame has correct version tracking
    if (currentFrame.version < 3) {
        result.Fail("Frame version not incremented correctly");
        return result;
    }
    
    // 6. Verify timestamp is recent
    uint64_t now = GetTickCount64();
    if (currentFrame.timestamp > now || currentFrame.timestamp < now - 10000) {
        result.Fail("Frame timestamp is stale or invalid");
        return result;
    }
    
    m_engine.Unsubscribe(&subscriber);
    result.passed = true;
    return result;
}

CorrectnessResult ContextCorrectnessHarness::TestAgentDecisionConsistency() {
    CorrectnessResult result;
    result.testName = "AgentDecisionConsistency";
    
    m_engine.Initialize();
    TestSubscriber agentSub("AgentSystem", 30);
    m_engine.Subscribe(&agentSub);
    
    // Simulate agent making decisions based on context
    
    // 1. Set up context
    std::string text = "def process_data(input):\n    # TODO: implement\n    pass";
    ContextEvent editEvent(ContextEvent::EDITOR_CHANGED, "editor", &text);
    m_engine.EmitEvent(editEvent);
    
    // 2. LSP provides context
    std::vector<SymbolInfo> symbols;
    symbols.push_back({"process_data", "function", 1, 0, ""});
    ContextEvent lspEvent(ContextEvent::LSP_UPDATED, "lsp", &symbols);
    m_engine.EmitEvent(lspEvent);
    
    // 3. Agent sees context and makes decision
    ContextFrame frame1 = m_engine.GetFrameCopy();
    
    // 4. Meanwhile, user edits
    std::string newText = "def process_data(input):\n    return input * 2";
    ContextEvent editEvent2(ContextEvent::EDITOR_CHANGED, "editor", &newText);
    m_engine.EmitEvent(editEvent2);
    
    // 5. Agent tries to act on old decision
    // It should detect that context has changed
    ContextFrame frame2 = m_engine.GetFrameCopy();
    
    if (frame1.version == frame2.version) {
        result.Fail("Context version did not change after edit");
        return result;
    }
    
    // Agent should check version before acting
    // If version changed, it should re-evaluate decision
    
    // 6. Verify agent can detect stale context
    if (frame1.bufferText == frame2.bufferText) {
        result.Fail("Agent cannot detect context change");
        return result;
    }
    
    // 7. Verify agent sees correct current state
    if (frame2.bufferText.find("return input * 2") == std::string::npos) {
        result.Fail("Agent sees stale context");
        return result;
    }
    
    m_engine.Unsubscribe(&agentSub);
    result.passed = true;
    return result;
}

// ─────────────────────────────────────────────────────────────────────────────
// Extended Test Suite
// ─────────────────────────────────────────────────────────────────────────────

std::vector<CorrectnessResult> ContextCorrectnessHarness::RunExtendedTests() {
    std::vector<CorrectnessResult> results;
    
    // Core tests
    results.push_back(TestFrameImmutability());
    results.push_back(TestCausalOrdering());
    results.push_back(TestSubscriberIsolation());
    results.push_back(TestBackpressureHandling());
    results.push_back(TestRaceSafety());
    results.push_back(TestDeterministicReplay());
    results.push_back(TestConflictResolution());
    results.push_back(TestStressTypingBurst());
    
    // Extended tests
    results.push_back(TestGhostTextDivergence());
    results.push_back(TestLSPDesyncUnderFastEdits());
    results.push_back(TestAIHallucinationFromStaleFrames());
    results.push_back(TestAgentDecisionConsistency());
    
    return results;
}

// ─────────────────────────────────────────────────────────────────────────────
// Main Test Runner
// ─────────────────────────────────────────────────────────────────────────────

int RunContextCorrectnessTests() {
    std::cout << "╔════════════════════════════════════════════════════════════════╗\n";
    std::cout << "║        RAWRXD CONTEXT CORRECTNESS HARNESS                      ║\n";
    std::cout << "║        Runtime Validation for IDE Coherence                     ║\n";
    std::cout << "╚════════════════════════════════════════════════════════════════╝\n\n";
    
    ContextCorrectnessHarness harness;
    
    std::cout << "Running core tests...\n\n";
    auto results = harness.RunAllTests();
    
    std::cout << harness.GenerateReport(results) << "\n";
    
    int passed = 0;
    int failed = 0;
    for (const auto& r : results) {
        if (r.passed) passed++;
        else failed++;
    }
    
    std::cout << "Running extended tests...\n\n";
    auto extendedResults = harness.RunExtendedTests();
    
    std::cout << harness.GenerateReport(extendedResults) << "\n";
    
    for (const auto& r : extendedResults) {
        if (r.passed) passed++;
        else failed++;
    }
    
    std::cout << "╔════════════════════════════════════════════════════════════════╗\n";
    std::cout << "║ FINAL RESULT: " << passed << " PASSED, " << failed << " FAILED";
    std::cout << std::setw(46 - (std::string("FINAL RESULT: ").length() + 
        std::to_string(passed).length() + 
        std::string(" PASSED, ").length() + 
        std::to_string(failed).length() + 
        std::string(" FAILED").length())) << "";
    std::cout << "║\n";
    std::cout << "╚════════════════════════════════════════════════════════════════╝\n";
    
    return failed > 0 ? 1 : 0;
}

} // namespace RawrXD

// ─────────────────────────────────────────────────────────────────────────────
// Entry Point
// ─────────────────────────────────────────────────────────────────────────────

#ifndef RAWRXD_SKIP_MAIN
int main(int argc, char** argv) {
    return RawrXD::RunContextCorrectnessTests();
}
#endif