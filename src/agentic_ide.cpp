#include "agentic_ide.h"
#include "agentic_engine.h"
#include "zero_day_agentic_engine.hpp" 
#include "universal_model_router.h"
#include "tool_registry.hpp"
#include "cpu_inference_engine.h"
#include "autonomous_model_manager.h"
#include "plan_orchestrator.h"
#include "lsp_client.h"
#include "multi_tab_editor.h"
#include "chat_interface.h"
#include "terminal_pool.h"
#include "RawrXD_Editor.h"
#include "masm/interconnect/RawrXD_Interconnect.h"
#include "autonomous_intelligence_orchestrator.h" // Added include

#include <iostream>
#include <thread>
#include <string>
#include <chrono>
#include <sstream>

using namespace RawrXD;

AgenticIDE::AgenticIDE() 
    : m_guiEditor(nullptr)
{
}

AgenticIDE::~AgenticIDE() = default;

void AgenticIDE::initialize() {
    std::cout << "[AgenticIDE] Initializing Core Systems..." << std::endl;

    // 1. Foundation Layers
    m_modelRouter = std::make_unique<RawrXD::UniversalModelRouter>();
    m_inferenceEngine = std::make_unique<RawrXD::CPUInferenceEngine>();
    m_terminalPool = std::make_unique<TerminalPool>();
    
    // 2. Intelligence Layers
    m_modelManager = std::make_unique<AutonomousModelManager>();
    m_toolRegistry = std::make_unique<RawrXD::ToolRegistry>(nullptr, nullptr); // Logger/Metrics null for now
    RawrXD::registerSystemTools(m_toolRegistry.get());
    
    // 3. Orchestration
    m_planOrchestrator = std::make_unique<RawrXD::PlanOrchestrator>();
    m_lspClient = std::make_unique<RawrXD::LSPClient>(RawrXD::LSPServerConfig{}); 
    
    m_planOrchestrator->setInferenceEngine(m_inferenceEngine.get());
    m_planOrchestrator->setLSPClient(m_lspClient.get());
    m_planOrchestrator->setModelRouter(m_modelRouter.get());
    m_planOrchestrator->initialize();
    
    // 4. Agents
    m_orchestrator = std::make_unique<AutonomousIntelligenceOrchestrator>(this); // Early init for brain
    m_orchestrator->initialize(""); // Auto-detect path
    
    m_agenticEngine = std::make_unique<AgenticEngine>();
    m_agenticEngine->initialize();
    
    // Wire Orchestrator to Engine for auto-fixing
    m_orchestrator->setAgenticEngine(m_agenticEngine.get());
    
    m_zeroDayAgent = std::make_unique<ZeroDayAgenticEngine>(
        m_modelRouter.get(),
        m_toolRegistry.get(),
        m_planOrchestrator.get(),
        nullptr
    );

    // 5. UI/Interaction Logic
    m_multiTabEditor = std::make_unique<MultiTabEditor>();
    m_multiTabEditor->initialize();
    
    m_chatInterface = std::make_unique<ChatInterface>();
    m_chatInterface->initialize();
    
    // Wiring Chat to Agents
    m_chatInterface->setAgenticEngine(m_agenticEngine.get());
    m_chatInterface->setPlanOrchestrator(m_planOrchestrator.get());
    m_chatInterface->setZeroDayAgent(m_zeroDayAgent.get());

    // 6. Hardware Acceleration (MASM)
    if (RawrXD::Interconnect::Initialize()) {
        auto metrics = RawrXD::Interconnect::GetMetrics();
        std::cout << "[AgenticIDE] MASM Interconnect Online. Uptime: " << metrics.uptimeMs << "ms" << std::endl;
    } else {
        std::cerr << "[AgenticIDE] MASM Interconnect Failed. Running in standard C++ mode." << std::endl;
    }

    std::cout << "[AgenticIDE] Systems Ready." << std::endl;
}

void AgenticIDE::run() {
    m_running = true;
    std::cout << "[AgenticIDE] Running. Type 'exit' to quit." << std::endl;
    processConsoleInput();
}

void AgenticIDE::stop() {
    m_running = false;
}

void AgenticIDE::setEditor(RawrXD::Editor* editor) {
    m_guiEditor = editor;
    
    // Wire up notification if orchestrator exists
    if (m_orchestrator) {
        m_orchestrator->onNotification = [this](const std::string& type, const std::string& msg) {
            std::cout << "[Orchestrator][" << type << "] " << msg << std::endl;
            // Real implementation would PostMessage to GUI thread here
            // e.g. PostMessage(m_guiEditor->getHwnd(), WM_APP_LOG, ...);
        };
    }
}

void AgenticIDE::startOrchestrator() {
    if (m_orchestrator) {
        std::cout << "[AgenticIDE] Starting Autonomous Orchestrator..." << std::endl;
        m_orchestrator->startAutonomousMode(""); // Scan current dir
    }
}

void AgenticIDE::processConsoleInput() {
    std::string line;
    std::cout << "> ";
    while (m_running && std::getline(std::cin, line)) {
        if (line.empty()) {
            std::cout << "> ";
            continue;
        }
        if (line == "exit" || line == "quit") {
            stop();
            break;
        } else if (line == "status") {
            std::cout << "-- Agentic IDE Status --\n";
            std::cout << "Editor Tabs: " << m_multiTabEditor->getTabCount() << "\n";
            if (m_orchestrator) {
                std::cout << "Orchestrator Mode: " << m_orchestrator->getCurrentMode() << "\n";
                auto q = m_orchestrator->getQualityReport();
                std::cout << "Quality Score: " << q["score"] << "\n";
                std::cout << "Maintainability: " << q["maintainability"] << "\n";
            }
            std::cout << "------------------------\n";
        }

        if (m_chatInterface) {
            m_chatInterface->sendMessage(line);
        } else {
            std::cout << "Error: Chat Interface not initialized." << std::endl;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        std::cout << "> " << std::flush;
    }
}
