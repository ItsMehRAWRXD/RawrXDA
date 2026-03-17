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

AgenticIDE::AgenticIDE() {
}

AgenticIDE::~AgenticIDE() {
    stop();
}

void AgenticIDE::setEditor(RawrXD::Editor* editor) {
    m_guiEditor = editor;
}

#include "masm/interconnect/RawrXD_Interconnect.h" 

#include <iostream>
#include <thread>
#include <chrono>
#include <sstream>

AgenticIDE::AgenticIDE() 
    : m_guiEditor(nullptr)
{
}

AgenticIDE::~AgenticIDE() = default;

void AgenticIDE::initialize() {
    std::cout << "[AgenticIDE] Initializing RawrXD Agentic IDE (Qt-Free Core)..." << std::endl;
    
    // Logic Components Init
    m_multiTabEditor = std::make_unique<MultiTabEditor>();
    m_multiTabEditor->initialize();

    // Engine Init
    m_agenticEngine = std::make_unique<AgenticEngine>();
    m_agenticEngine->initialize();
    
    m_inferenceEngine = std::make_unique<RawrXD::CPUInferenceEngine>();
    m_modelManager = std::make_unique<AutonomousModelManager>();

    m_toolRegistry = std::make_unique<ToolRegistry>(nullptr, nullptr);
    m_modelRouter = std::make_unique<RawrXD::UniversalModelRouter>();
    m_planOrchestrator = std::make_unique<RawrXD::PlanOrchestrator>();
    m_planOrchestrator->setInferenceEngine(m_inferenceEngine.get());
    m_planOrchestrator->initialize();

    m_zeroDayAgent = std::make_unique<ZeroDayAgenticEngine>(
        m_modelRouter.get(),
        m_toolRegistry.get(),
        m_planOrchestrator.get(),
        nullptr
    );

    m_agenticEngine->onResponseReady = [](const std::string& resp) {
        std::cout << "\n[AI]: " << resp << "\n> " << std::flush;
    };
    
    std::cout << "[AgenticIDE] Initialization Complete." << std::endl;
}

void AgenticIDE::run() {
    m_running = true;
    std::cout << "[AgenticIDE] System Running. Type 'exit' to quit." << std::endl;
    std::cout << "> " << std::flush;

    // Start console input thread
    std::thread inputThread(&AgenticIDE::processConsoleInput, this);

    while (m_running) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        // Main loop logic (e.g., event processing)
    }

    if (inputThread.joinable()) {
        inputThread.join();
    }
}

void AgenticIDE::stop() {
    m_running = false;
}

void AgenticIDE::setEditor(RawrXD::Editor* editor) {
    m_guiEditor = editor;
}

void AgenticIDE::processConsoleInput() {
    std::string line;
    while (m_running && std::getline(std::cin, line)) {
        if (line == "exit" || line == "quit") {
            m_running = false;
            break;
        }

        if (m_agenticEngine) {
            std::string response = m_agenticEngine->generateResponse(line);
            std::cout << "> " << response << std::endl;
        } else {
            std::cout << "Error: Agentic Engine not initialized." << std::endl;
        }
        // m_agenticEngine->processQuery(line); // Removed old call
    }
}
