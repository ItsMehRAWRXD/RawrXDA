// ============================================================================
// Streaming Integration - Tool Result Injection
// How to use the new DispatchModelToolCallsStreaming() method in Win32IDE chat
// ============================================================================

#pragma once

#include <functional>
#include <string>

namespace RawrXD::Phase1Examples
{

/// How to integrate streaming tool execution in Win32IDE chat panel
///
/// BEFORE (Current - No Streaming):
/// ```
/// User: "Refactor this file"
///   → Model responds (3s)
///   → Tool 1 read (1s)
///   → Tool 2 replace (1s)
///   → Tool 3 verify (1s)
///   → Chat shows: "Done!" (after 6s total)
/// 
/// USER PERCEPTION: App is frozen for 6 seconds
/// ```
///
/// AFTER (Phase 1 - With Streaming):
/// ```
/// User: "Refactor this file"
///   → Model responds (3s)
///   → Chat: "[Tool Started] read_file" (appears immediately)
///   → Tool 1 read (1s)
///   → Chat: "[Tool Result] read_file: ..." (appears immediately)
///   → Chat: "[Tool Started] replace_in_file"
///   → Tool 2 replace (1s)
///   → Chat: "[Tool Result] replace_in_file: success"
///   → etc...
///   → Chat: "[Stream Complete] Done!"
/// 
/// USER PERCEPTION: App is responsive, showing progress in real-time
/// ```

/// Integration point 1: Win32IDE chat panel handler
/// Call from: Win32IDE.cpp onChatInput() or similar
void ExampleIntegrateStreamingInChat()
{
    /*
    // Pseudocode for Win32IDE.cpp integration:
    
    void Win32IDE::onUserChatMessage(const std::string& userMessage)
    {
        // ... existing code ...
        
        // Send to agent
        auto agentPrompt = BuildAgentPromptFromChat(userMessage);
        
        // Option 1: Use new streaming dispatch (Phase 1)
        std::string response = m_agenticBridge->DispatchModelToolCallsStreaming(
            agentPrompt,
            [this](const RawrXD::Agentic::ToolExecutionEvent& event) {
                // Callback: Tool event emitted in real-time
                
                switch (event.eventType) {
                    case ToolExecutionEvent::Type::TOOL_STARTED:
                        // Emit to chat panel: "[Tool] Starting read_file..."
                        this->appendToChatPanel("⏳ " + event.toolName + " starting...");
                        break;
                        
                    case ToolExecutionEvent::Type::TOOL_RESULT:
                        // Emit to chat panel: "[Tool] read_file result: ..."
                        this->appendToChatPanel("✓ " + event.toolName + " completed (" + 
                                                std::to_string(event.executionTimeMs) + "ms)");
                        // Optionally show partial result
                        if (event.content.size() < 2000) {
                            this->appendToChatPanel("   Result: " + event.content);
                        }
                        break;
                        
                    case ToolExecutionEvent::Type::TOOL_ERROR:
                        // Emit to chat panel: "[Error] tool_name failed: ..."
                        this->appendToChatPanel("❌ " + event.toolName + ": " + event.content);
                        break;
                        
                    case ToolExecutionEvent::Type::TOOL_TIMEOUT:
                        // Emit to chat panel: "[Timeout] tool_name did not complete in time"
                        this->appendToChatPanel("⏱️ " + event.toolName + " timeout after " + 
                                                std::to_string(event.executionTimeMs) + "ms");
                        break;
                        
                    case ToolExecutionEvent::Type::TOOL_PARTIAL:
                        // For long-running tools, show intermediate progress
                        this->appendToChatPanel("   [Partial] " + event.content);
                        break;
                        
                    case ToolExecutionEvent::Type::STREAM_COMPLETE:
                        // Final response ready
                        this->appendToChatPanel("🎉 Response complete");
                        this->setChatPanelContent(event.content);
                        break;
                }
            }
        );
        
        // Option 2: Fallback to traditional dispatch (no streaming)
        // string response = m_agenticBridge->DispatchModelToolCalls(agentPrompt, toolResult);
        
        // Continue with response handling...
    }
    */
}

/// Integration point 2: Terminal/CLI streaming
/// Call from: cli_shell.cpp cmd_agent_loop() or similar
void ExampleIntegrateStreamingInCLI()
{
    /*
    // Pseudocode for cli_shell.cpp integration:
    
    void handleAgentLoopCommand(const std::string& prompt)
    {
        // Use streaming dispatch with console output callbacks
        std::string response = bridge->DispatchModelToolCallsStreaming(
            prompt,
            [](const ToolExecutionEvent& event) {
                // Output to terminal in real-time
                std::cout << "[" << ToolExecutionEvent::EventTypeToString(event.eventType) << "] ";
                
                switch (event.eventType) {
                    case ToolExecutionEvent::Type::TOOL_STARTED:
                        std::cout << "Starting: " << event.toolName << std::endl;
                        break;
                    case ToolExecutionEvent::Type::TOOL_RESULT:
                        std::cout << "Completed: " << event.toolName 
                                  << " (" << event.executionTimeMs << "ms)" << std::endl;
                        break;
                    case ToolExecutionEvent::Type::TOOL_ERROR:
                        std::cout << "ERROR in " << event.toolName << ": " 
                                  << event.content << std::endl;
                        break;
                    case ToolExecutionEvent::Type::STREAM_COMPLETE:
                        std::cout << "Stream complete" << std::endl;
                        break;
                    default:
                        break;
                }
            }
        );
        
        std::cout << "\n=== Response ===\n" << response << std::endl;
    }
    */
}

/// Integration point 3: Testing
/// Call from: Unit tests or smoke tests
void ExampleTestStreaming()
{
    /*
    // Pseudocode for unit test:
    
    TEST_CASE("StreamingResultChannel emits events in order") {
        auto channel = std::make_unique<StreamingResultChannel>();
        
        std::vector<ToolExecutionEvent> events;
        channel->SetToolEventCallback([&](const ToolExecutionEvent& event) {
            events.push_back(event);
        });
        
        // Emit tool events
        channel->EmitToolStarted("read_file", 0, 2);
        channel->EmitToolResult("read_file", "file content", 150, 0, 2);
        channel->EmitToolStarted("write_file", 1, 2);
        channel->EmitToolResult("write_file", "written 100 bytes", 200, 1, 2);
        channel->EmitStreamComplete("Final response");
        
        // Verify events
        REQUIRE(events.size() == 5);
        REQUIRE(events[0].eventType == ToolExecutionEvent::Type::TOOL_STARTED);
        REQUIRE(events[1].eventType == ToolExecutionEvent::Type::TOOL_RESULT);
        REQUIRE(events[4].eventType == ToolExecutionEvent::Type::STREAM_COMPLETE);
    }
    */
}

/// Integration point 4: Metrics/Telemetry
/// How to collect data about tool execution streaming
void ExampleMetricsCollection()
{
    /*
    // In Win32IDE or a telemetry module:
    
    class StreamingMetrics {
    private:
        uint64_t totalToolsExecuted = 0;
        uint64_t totalToolsStreamedSuccessfully = 0;
        uint64_t totalToolTimeMs = 0;
        uint64_t totalResponseCount = 0;
        
    public:
        void onStreamingEvent(const ToolExecutionEvent& event) {
            switch (event.eventType) {
                case ToolExecutionEvent::Type::TOOL_RESULT:
                    totalToolsExecuted++;
                    totalToolTimeMs += event.executionTimeMs;
                    break;
                case ToolExecutionEvent::Type::STREAM_COMPLETE:
                    totalResponseCount++;
                    break;
                default:
                    break;
            }
        }
        
        double getAverageToolTime() const {
            if (totalToolsExecuted == 0) return 0.0;
            return static_cast<double>(totalToolTimeMs) / totalToolsExecuted;
        }
    };
    */
}

}  // namespace RawrXD::Streaming

/// ============================================================================
/// MIGRATION GUIDE: From Sequential to Streaming
/// ============================================================================
///
/// Step 1: Locate the tool dispatch call in your code
///   Current:
///     std::string result;
///     bridge->DispatchModelToolCalls(output, result);
///
/// Step 2: Replace with streaming version
///   New:
///     auto result = bridge->DispatchModelToolCallsStreaming(
///         output,
///         [](const ToolExecutionEvent& event) {
///             // Handle tool events
///         }
///     );
///
/// Step 3: In the callback, emit UI updates
///   - "Tool started" → show loading indicator
///   - "Tool result" → show result snippet
///   - "Stream complete" → show full response
///
/// Step 4: Test with multi-tool requests
///   - UI should update in real-time
///   - Progress should be visible
///   - No "frozen" appearance
///
/// ROLLBACK: If issues occur, use the old method:
///   std::string result;
///   bridge->DispatchModelToolCalls(output, result);
///
/// Benefits of Phase 1:
///   ✓ Real-time UI feedback
///   ✓ Progressive disclosure
///   ✓ Better perceived performance
///   ✓ Foundation for Phase 2 (async dispatch)
///
/// ============================================================================
