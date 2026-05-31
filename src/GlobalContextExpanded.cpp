// GlobalContextExpanded.cpp — Production global context implementation

#include "GlobalContextExpanded.h"
#include <windows.h>
#include <cstdio>

void GlobalContextExpanded::WireAll(RawrXD::Auth::RBACEngine& rbac) {
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
