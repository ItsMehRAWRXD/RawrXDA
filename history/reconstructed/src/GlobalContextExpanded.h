#pragma once
// GlobalContextExpanded.h — Expanded singleton holding all 7 subsystems with SignalSlot
// Extends shared_context.h GlobalContext with new unified components
// No Qt. No exceptions. C++20 only.

#include "shared_context.h"
#include "UnifiedToolRegistry.h"
#include "security/SecureHotpatchOrchestrator.h"
#include "agentic/AgentOrchestrator.h"
#include "PerformanceMonitor.h"
#include "RawrXD_SignalSlot.h"
#include <memory>
#include <iostream>

struct GlobalContextExpanded : public GlobalContext {
    // Expanded subsystems (lazy-init)
    UnifiedToolRegistry* tools = nullptr;
    SecureHotpatchOrchestrator* securePatcher = nullptr;
    RawrXD::Agent::AgentOrchestrator* agent = nullptr;
    std::unique_ptr<PerformanceMonitor> perf;

    RawrXD::Signal<> SystemsInitialized;

    static GlobalContextExpanded& Get() {
        static GlobalContextExpanded instance;
        return instance;
    }

    void WireAll(RawrXD::Auth::RBACEngine& rbac) {
        tools = &UnifiedToolRegistry::Instance();

        if (!agent) {
            agent = new RawrXD::Agent::AgentOrchestrator();
        }

        if (patcher && agent) {
            securePatcher = new SecureHotpatchOrchestrator(rbac, *patcher, *agent);
        }

        perf = std::make_unique<PerformanceMonitor>();

        SystemsInitialized.connect([this]() {
            if (perf) perf->StartTracking();
            std::cout << "[GlobalContext] All subsystems wired.\n";
        });

        SystemsInitialized.emit();
    }

    ~GlobalContextExpanded() {
        delete securePatcher;
        delete agent;
    }

private:
    GlobalContextExpanded() = default;
};

#define gCtx GlobalContextExpanded::Get()
