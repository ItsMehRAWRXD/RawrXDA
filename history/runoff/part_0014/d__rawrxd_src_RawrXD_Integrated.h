// RawrXD Master Integration — All 12 Components Wired
// Include this ONE header in Win32IDE.cpp to get complete IDE
#pragma once

// 1. Core Infrastructure
#include "RawrXD_SignalSlot.h"
#include "EventBus.h"
#include "GlobalContextExpanded.h"
#include "PerformanceMonitor.h"

// 2. Security & Hotpatch
#include "security/SecureHotpatchOrchestrator.h"
#include "HotpatchBridgeUnified.h"

// 3. Tooling & Agentic
#include "UnifiedToolRegistry.h"
#include "CompilerAgentBridge.h"

// 4. UI Components (Qt-free)
#include "ui/FixedDockWidgets.h"
#include "ui/AgenticChatPanel.h"
#include "IDEMainWindow_Migrated.h"

// 5. LSP & Language
#include "LSPCore.h"

namespace RawrXD {
class IntegratedIDE : public IDEMainWindow_Migrated {
    GlobalContextExpanded m_context;
    SecureHotpatchOrchestrator m_securePatcher;
    UnifiedToolRegistry m_tools;
    EventBus m_bus;
    PerformanceMonitor m_perf;
    std::unique_ptr<TodoDock> m_todo;
    std::unique_ptr<ObservabilityDashboard> m_obs;
    std::unique_ptr<AgenticChatPanel> m_chat;
public:
    IntegratedIDE() : m_securePatcher(&m_context.GetHotPatcher(), &m_context.GetRBAC()) {
        m_tools.ConnectToEventBus(&m_bus);
        m_securePatcher.InitializeAuditChain();
    }
    void Initialize() {
        CreateMainWindow();
        CreateDocks();
        WireBeaconSystem();
        m_perf.StartMonitoring([&](float cpu, float mem){ m_obs->AddMetric(L"CPU", cpu); m_obs->AddMetric(L"Memory", mem); });
    }
    void CreateDocks() {
        m_todo = std::make_unique<TodoDock>(this); 
        m_obs = std::make_unique<ObservabilityDashboard>(this);
        m_chat = std::make_unique<AgenticChatPanel>(this);
        AddDock(m_todo.get(), DockPosition::Left);
        AddDock(m_obs.get(), DockPosition::Right);
        AddDock(m_chat.get(), DockPosition::Bottom);
        m_chat->onAgenticCommand.connect([this](const std::wstring& cmd){ 
            m_bus.Publish(EventType::AgenticCommand, cmd); 
        });
    }
    void InjectAgenticBridge(AgentOrchestrator* orch) {
        m_chat->AttachOrchestrator(orch);
        CompilerAgentBridge bridge(&m_context.GetToolchain(), orch);
    }
};
} // namespace RawrXD
