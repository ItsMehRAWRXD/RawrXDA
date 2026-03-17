#pragma once
// CompilerAgentBridge.h — Bridges Compiler ↔ Agentic via EventBus
// Integrates ToolchainBridge compile results into AgenticEngine notifications
// No Qt. No exceptions. C++20 only.

#include "compiler/toolchain_bridge.hpp"
#include "agentic_engine.h"
#include "EventBus.h"
#include <string>
#include <iostream>

class CompilerAgentBridge {
    ToolchainBridge* m_compiler;
    AgenticEngine* m_agent;

public:
    CompilerAgentBridge(ToolchainBridge* compiler, AgenticEngine* agent)
        : m_compiler(compiler), m_agent(agent)
    {
        RawrXD::EventBus::Get().BuildFinished.connect(
            [this](const std::string& target, bool success) {
                if (success && m_agent && m_agent->isModelLoaded()) {
                    std::cout << "[CompilerAgent] Build complete, notifying agent: " << target << "\n";
                    // Agent can react to build results for auto-fix workflows
                }
            }
        );
    }

    bool CompileWithAgent(const std::string& file) {
        RawrXD::EventBus::Get().BuildProgress.emit(file, 0.0f);

        bool ok = m_compiler ? m_compiler->Compile(file) : false;

        RawrXD::EventBus::Get().BuildProgress.emit(file, ok ? 1.0f : -1.0f);
        RawrXD::EventBus::Get().BuildFinished.emit(file, ok);

        if (!ok && m_agent && m_agent->isModelLoaded()) {
            // Auto-diagnose compilation errors via the agent
            std::string diagnosis = m_agent->explainError("Compilation failed: " + file);
            RawrXD::EventBus::Get().AgentMessage.emit(diagnosis);
        }

        return ok;
    }

    ToolchainBridge* GetCompiler() { return m_compiler; }
    AgenticEngine* GetAgent() { return m_agent; }
};
