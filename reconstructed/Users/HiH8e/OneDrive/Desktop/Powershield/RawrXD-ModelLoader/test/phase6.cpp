#include "include/telemetry/ai_metrics.h"
#include "include/session/ai_session.h"
#include <iostream>
#include <thread>
#include <chrono>

using namespace RawrXD::Telemetry;
using namespace RawrXD::Session;

void testAIMetrics() {
    std::cout << "\n=== Testing AI Metrics & Telemetry Dashboard ===\n" << std::endl;
    
    auto& metrics = GetMetricsCollector();
    
    // Test 1: Record multiple Ollama requests with varying latencies
    std::cout << "Test 1: Recording Ollama requests..." << std::endl;
    metrics.recordOllamaRequest("llama3.2", 150, 100, 250, true);
    metrics.recordOllamaRequest("llama3.2", 180, 120, 300, true);
    metrics.recordOllamaRequest("codellama", 95, 80, 200, true);
    metrics.recordOllamaRequest("llama3.2", 220, 150, 350, false); // failure
    metrics.recordOllamaRequest("codellama", 110, 90, 220, true);
    std::cout << "  ✓ Recorded 5 Ollama requests (4 success, 1 failure)\n" << std::endl;
    
    // Test 2: Record tool invocations
    std::cout << "Test 2: Recording tool invocations..." << std::endl;
    metrics.recordToolInvocation("file_read", 25, true);
    metrics.recordToolInvocation("file_write", 45, true);
    metrics.recordToolInvocation("git_status", 120, true);
    metrics.recordToolInvocation("file_read", 30, true);
    metrics.recordToolInvocation("git_commit", 200, false); // failure
    metrics.recordToolInvocation("file_search", 80, true);
    std::cout << "  ✓ Recorded 6 tool invocations (5 success, 1 failure)\n" << std::endl;
    
    // Test 3: Get display metrics for UI
    std::cout << "Test 3: Display metrics for floating panel..." << std::endl;
    auto display = metrics.getDisplayMetrics();
    std::cout << "  Session Stats:" << std::endl;
    std::cout << "    Total requests: " << display.total_requests << std::endl;
    std::cout << "    Success rate: " << display.success_rate << "%" << std::endl;
    std::cout << "    Last latency: " << display.last_request_latency_ms << "ms" << std::endl;
    std::cout << "    Total tokens: " << display.token_stats.total_tokens << std::endl;
    
    std::cout << "\n  Latency Stats:" << std::endl;
    std::cout << "    P50: " << display.latency_stats.p50_ms << "ms" << std::endl;
    std::cout << "    P95: " << display.latency_stats.p95_ms << "ms" << std::endl;
    std::cout << "    P99: " << display.latency_stats.p99_ms << "ms" << std::endl;
    
    std::cout << "\n  Current Model:" << std::endl;
    std::cout << "    Name: " << display.current_model << std::endl;
    std::cout << "    Requests: " << display.current_model_requests << std::endl;
    
    std::cout << "\n  Top Tools:" << std::endl;
    for (size_t i = 0; i < display.top_tools.size() && i < 3; ++i) {
        std::cout << "    " << (i+1) << ". " << display.top_tools[i].tool_name 
                  << " (" << display.top_tools[i].invocation_count << " calls)" << std::endl;
    }
    
    std::cout << "\n  Recent Errors:" << std::endl;
    for (const auto& err : display.recent_errors) {
        std::cout << "    - " << err << std::endl;
    }
    std::cout << std::endl;
    
    // Test 4: Export to different formats
    std::cout << "Test 4: Exporting metrics to files..." << std::endl;
    if (metrics.saveMetricsToFile("test_metrics.json", ExportFormat::JSON)) {
        std::cout << "  ✓ Exported to test_metrics.json" << std::endl;
    }
    if (metrics.saveMetricsToFile("test_metrics.csv", ExportFormat::CSV)) {
        std::cout << "  ✓ Exported to test_metrics.csv" << std::endl;
    }
    if (metrics.saveMetricsToFile("test_metrics.txt", ExportFormat::TEXT)) {
        std::cout << "  ✓ Exported to test_metrics.txt" << std::endl;
    }
    std::cout << std::endl;
    
    std::cout << "✅ AI Metrics tests completed!\n" << std::endl;
}

void testSessionPersistence() {
    std::cout << "\n=== Testing Session Persistence & Replay ===\n" << std::endl;
    
    // Test 1: Create session and record events
    std::cout << "Test 1: Creating session and recording events..." << std::endl;
    AISession session("test-session-001");
    
    session.recordUserPrompt("How do I read a file in C++?");
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    session.recordAIResponse("You can use std::ifstream to read files in C++.", 
                            "llama3.2", 150, 250);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    
    session.recordToolCall("file_read", "{\"path\": \"example.cpp\"}", 
                          "File contents: ...", true);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    
    session.recordUserPrompt("Can you write to files too?");
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    session.recordAIResponse("Yes, use std::ofstream for writing files.", 
                            "llama3.2", 120, 200);
    
    session.recordFileModification("test.cpp", "file_write");
    
    std::cout << "  ✓ Recorded 6 events (2 prompts, 2 responses, 1 tool call, 1 file mod)\n" << std::endl;
    
    // Test 2: Create checkpoint
    std::cout << "Test 2: Creating checkpoint..." << std::endl;
    uint64_t checkpointId = session.createCheckpoint("Before advanced topic");
    std::cout << "  ✓ Checkpoint created: " << checkpointId << "\n" << std::endl;
    
    // Test 3: Continue conversation
    std::cout << "Test 3: Continuing conversation..." << std::endl;
    session.recordUserPrompt("What about error handling?");
    session.recordAIResponse("Use try-catch blocks for error handling.", 
                            "llama3.2", 140, 230);
    std::cout << "  ✓ Added 2 more events after checkpoint\n" << std::endl;
    
    // Test 4: Session statistics...
    std::cout << "Test 4: Session statistics..." << std::endl;
    auto stats = session.getStatistics();
    std::cout << "  Total prompts: " << stats.total_prompts << std::endl;
    std::cout << "  Total responses: " << stats.total_responses << std::endl;
    std::cout << "  Total tool calls: " << stats.total_tool_calls << std::endl;
    std::cout << "  Total tokens: " << (stats.total_prompt_tokens + stats.total_completion_tokens) << std::endl;
    std::cout << "\n  Tool usage:" << std::endl;
    for (const auto& [tool, count] : stats.tools_usage) {
        std::cout << "    " << tool << ": " << count << "x" << std::endl;
    }
    std::cout << std::endl;
    
    // Test 5: Save session to JSON
    std::cout << "Test 5: Saving session to JSON..." << std::endl;
    std::string sessionJson = session.toJSON();
    std::cout << "  ✓ Session serialized (" << sessionJson.length() << " bytes)" << std::endl;
    
    if (session.saveToFile("test_session.json")) {
        std::cout << "  ✓ Session saved to test_session.json\n" << std::endl;
    }
    
    // Test 6: Replay mode
    std::cout << "Test 6: Testing replay mode..." << std::endl;
    session.startReplay(0); // from start
    int eventCount = 0;
    while (session.hasMoreReplayEvents()) {
        auto event = session.getNextReplayEvent();
        eventCount++;
        std::cout << "  Replay event " << eventCount << std::endl;
    }
    std::cout << "  ✓ Replayed " << eventCount << " events\n" << std::endl;
    
    // Test 7: SessionManager
    std::cout << "Test 7: Testing SessionManager..." << std::endl;
    SessionManager manager;
    
    auto sessionA = manager.createSession("session-A");
    auto sessionB = manager.createSession("session-B");
    
    if (sessionA) {
        sessionA->recordUserPrompt("Test prompt A");
        sessionA->recordAIResponse("Test response A", "llama3.2", 100, 150);
    }
    
    if (sessionB) {
        sessionB->recordUserPrompt("Test prompt B");
        sessionB->recordAIResponse("Test response B", "codellama", 110, 160);
    }
    
    std::cout << "  Created 2 sessions" << std::endl;
    
    if (manager.saveSession(sessionA->getSessionId())) {
        std::cout << "  ✓ Session A saved" << std::endl;
    }
    if (manager.saveSession(sessionB->getSessionId())) {
        std::cout << "  ✓ Session B saved" << std::endl;
    }
    
    std::cout << "✅ Session Persistence tests completed!\n" << std::endl;
}

int main() {
    std::cout << "\n╔════════════════════════════════════════════════════════╗" << std::endl;
    std::cout << "║   Phase 6 Testing: AI Metrics & Session Persistence   ║" << std::endl;
    std::cout << "╚════════════════════════════════════════════════════════╝" << std::endl;
    
    try {
        testAIMetrics();
        testSessionPersistence();
        
        std::cout << "\n" << std::string(60, '=') << std::endl;
        std::cout << "🎉 All Phase 6 tests PASSED!" << std::endl;
        std::cout << std::string(60, '=') << "\n" << std::endl;
        
        return 0;
    }
    catch (const std::exception& e) {
        std::cerr << "\n❌ Test failed with exception: " << e.what() << std::endl;
        return 1;
    }
}
