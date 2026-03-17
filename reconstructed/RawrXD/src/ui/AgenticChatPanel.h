#pragma once
// AgenticChatPanel.h — Chat panel wired to AgentOrchestrator
// Connects ChatPanel UI → agentic loop for tool-calling conversations
// No Qt. No exceptions. C++20 only.

#include "chatpanel.h"
#include "agentic/AgentOrchestrator.h"
#include "RawrXD_SignalSlot.h"
#include <string>
#include <iostream>

namespace RawrXD {

class AgenticChatPanel : public ChatPanel {
    Agent::AgentOrchestrator* m_orch;

    // Signal for user message submission
    Signal<const std::string&> onMessageSubmit;

public:
    AgenticChatPanel(Window* parent, Agent::AgentOrchestrator* orch)
        : ChatPanel(parent), m_orch(orch)
    {
        // Wire user input to orchestrator
        onMessageSubmit.connect([this](const std::string& msg) {
            if (!m_orch) return;

            appendUserMessage(msg);

            // Run agent loop async, streaming steps back to the UI
            m_orch->RunAgentLoopAsync(msg,
                // Step callback — show tool calls and intermediate results
                [this](const Agent::AgentStep& step) {
                    switch (step.type) {
                        case Agent::AgentStep::Type::AssistantMessage:
                            appendAIMessage(step.content);
                            break;
                        case Agent::AgentStep::Type::ToolCall:
                            appendAIMessage("[Tool: " + step.tool_name + "]");
                            break;
                        case Agent::AgentStep::Type::Error:
                            appendAIMessage("[Error] " + step.content);
                            break;
                        default:
                            break;
                    }
                },
                // Completion callback — show final response
                [this](Agent::AgentSession session) {
                    if (!session.final_response.empty()) {
                        appendAIMessage(session.final_response);
                    }
                    std::cout << "[AgenticChat] Session done: "
                              << session.tool_calls_made << " tool calls, "
                              << session.total_elapsed_ms << " ms\n";
                }
            );
        });
    }

    // Called from UI when user hits send
    void SubmitMessage(const std::string& msg) {
        onMessageSubmit.emit(msg);
    }

    auto& OnMessageSubmit() { return onMessageSubmit; }
};

} // namespace RawrXD
