#pragma once
// CompilerAgentBridge.h — Bridges Compiler ↔ Agentic via EventBus
// Uses actual ToolchainBridge API (buildAsync/compileFile) and AgenticEngine
// No Qt. No exceptions. No std::cout. C++20 only.

#include "compiler/toolchain_bridge.hpp"
#include "agentic_engine.h"
#include "EventBus.h"
#include "logger.h"
#include <string>

class CompilerAgentBridge {
    RawrXD::Compiler::ToolchainBridge* m_compiler;
    AgenticEngine* m_agent;

public:
    CompilerAgentBridge(RawrXD::Compiler::ToolchainBridge* compiler, AgenticEngine* agent)
        : m_compiler(compiler), m_agent(agent)
    {
        RawrXD::EventBus::Get().BuildFinished.connect(
            [this](const std::string& target, bool success) {
                if (success && m_agent && m_agent->isModelLoaded()) {
                    Logger::info("CompilerAgent: Build complete, notifying agent: {}", target);
                }
            }
        );
    }

    bool CompileWithAgent(const std::string& file) {
        RawrXD::EventBus::Get().BuildProgress.emit(file, 0.0f);

        // Use the actual compileFile API
        RawrXD::Compiler::BuildTarget target;
        target.sources = {file};
        bool ok = m_compiler ? m_compiler->compileFile(file, target) : false;

        RawrXD::EventBus::Get().BuildProgress.emit(file, ok ? 1.0f : -1.0f);
        RawrXD::EventBus::Get().BuildFinished.emit(file, ok);

        if (!ok && m_agent && m_agent->isModelLoaded()) {
            // Auto-diagnose compilation errors via the agent
            std::string diags;
            for (const auto& d : m_compiler->diagnostics()) {
                diags += d.file + ":" + std::to_string(d.line) + ": " + d.message + "\n";
            }
            std::string diagnosis = m_agent->explainError("Compilation failed: " + file + "\n" + diags);
            RawrXD::EventBus::Get().AgentMessage.emit(diagnosis);
            Logger::error("CompilerAgent: Build failed for {}, agent diagnosis dispatched", file);
        }

        return ok;
    }

    RawrXD::Compiler::ToolchainBridge* GetCompiler() { return m_compiler; }
    AgenticEngine* GetAgent() { return m_agent; }
};
