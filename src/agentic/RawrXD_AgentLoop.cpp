#include "RawrXD_AgentLoop.h"
#include "../IDELogger.h"

#include <chrono>

using RawrXD::Agentic::AgentLoop;
using RawrXD::Agentic::PerceptionEvent;

AgentLoop::AgentLoop()
    : m_running(false)
    , m_bridge(nullptr)
    , m_toolRegistry(&RawrXD::Agent::AgentToolRegistry::Instance()) {

}

AgentLoop::~AgentLoop() {
    Stop();

}

void AgentLoop::SetBridge(AgenticBridge* bridge) {
    m_bridge = bridge;
}

void AgentLoop::Start() {
    if (m_running.exchange(true)) return;
    m_thread = std::thread([this] { Loop(); });

}

void AgentLoop::Stop() {
    if (!m_running.exchange(false)) return;
    if (m_thread.joinable()) {
        m_thread.join();
    }

}

void AgentLoop::TriggerPerception(PerceptionType type, const std::string& data) {
    std::lock_guard<std::mutex> lock(m_queueMutex);
    m_queue.push({type, data, GetTickCount64()});
}

void AgentLoop::Loop() {
    while (m_running.load()) {
        PerceptionEvent evt{};
        bool hasEvent = false;
        {
            std::lock_guard<std::mutex> lock(m_queueMutex);
            if (!m_queue.empty()) {
                evt = m_queue.front();
                m_queue.pop();
                hasEvent = true;
            }
        }
        if (hasEvent) {
            ProcessEvent(evt);
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
    }
}

void AgentLoop::ProcessEvent(const PerceptionEvent& evt) {
    std::string summary = "Perception received: " + evt.data;

    if (!m_bridge || !m_bridge->IsInitialized()) {
        LOG_WARNING("AgentLoop bridge not initialized, skipping execution");
        return;
    }

    auto response = m_bridge->ExecuteAgentCommand(evt.data);
    if (response.type == AgentResponseType::TOOL_CALL) {
        std::string output;
        std::string argsJson = response.toolArgs.empty() ? "{}" : response.toolArgs;
        nlohmann::json args;
        try {
            args = nlohmann::json::parse(argsJson);
        } catch (...) {
            args = nlohmann::json::object();
        }
        auto result = m_toolRegistry->Dispatch(response.toolName, args);
        output = result.output;

        if (!output.empty()) {

        }
    }
}
