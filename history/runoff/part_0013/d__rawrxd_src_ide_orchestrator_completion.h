#pragma once
#include "ide_orchestrator.h"
#include "security/security_hotpatch_bridge.h"
#include "unified_tool_registry.h"
#include "agentic/AgentOrchestrator.h"

namespace RawrXD {
class IDEOrchestratorV2 : public IDEOrchestrator {
    std::unique_ptr<SecurityHotpatchBridge> m_secBridge;
    std::unique_ptr<AgentOrchestrator> m_agentOrch;
public:
    void InitializeSecurityLayer() {
        m_secBridge = std::make_unique<SecurityHotpatchBridge>(&GlobalContext::Get().GetRBAC(), &GlobalContext::Get().GetHotPatcher());
    }
    void InitializeAgentOrchestrator() {
        m_agentOrch = std::make_unique<AgentOrchestrator>();
        m_agentOrch->SetToolRegistry(&UnifiedToolRegistry::Instance());
    }
    void WireAllSignals() {
        // Connect ToolRegistry to ChatPanel
        UnifiedToolRegistry::Instance().ConnectInvocationSignal([this](const std::wstring& tool, const ToolContext& ctx){
            m_chatPanel->AppendSystemMessage(L"Tool invoked: " + tool + L" by " + ctx.sessionId);
        });
        // Connect HotPatcher to Metrics
        GlobalContext::Get().GetHotPatcher().onPatchApplied.connect([this](const std::wstring& id){
            m_metrics->AddMetric(L"HotPatch", 1.0f);
        });
    }
};
} // namespace RawrXD
