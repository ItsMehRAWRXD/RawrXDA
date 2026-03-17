#include "agentic_ide.h"
#include "agentic_engine.h"
#include "zero_day_agentic_engine.hpp"
#include "universal_model_router.h"
#include "tool_registry.hpp"
#include "plan_orchestrator.h"
#include "lsp_client.h"
#include <iostream>

AgenticIDE::AgenticIDE() {
}

AgenticIDE::~AgenticIDE() = default;

void AgenticIDE::initialize() {
    std::cout << "Initializing RawrXD Agentic IDE (Qt-Free Core)..." << std::endl;
    
    m_agenticEngine = std::make_unique<AgenticEngine>();
    m_agenticEngine->initialize();
    
    m_toolRegistry = std::make_unique<ToolRegistry>();
    m_modelRouter = std::make_unique<UniversalModelRouter>();
    m_zeroDayAgent = std::make_unique<ZeroDayAgenticEngine>();
    
    m_planOrchestrator = std::make_unique<RawrXD::PlanOrchestrator>();
    
    // Wire up callbacks
    m_agenticEngine->onResponseReady = [](const std::string& resp) {
        std::cout << "[Agent] " << resp << std::endl;
    };
}

void AgenticIDE::run() {
    std::cout << "RawrXD Agentic IDE Core is running." << std::endl;
    // Main loop or event wait would go here if headless
}

