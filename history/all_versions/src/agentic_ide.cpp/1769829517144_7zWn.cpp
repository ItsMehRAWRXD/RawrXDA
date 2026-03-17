#include "agentic_ide.h"
#include "agentic_engine.h"
#include "zero_day_agentic_engine.hpp" // Keep inconsistent naming if that's how it is on disk?
#include "universal_model_router.h"
#include "tool_registry.hpp"
#include "plan_orchestrator.h"
#include "lsp_client.h"
#include "multi_tab_editor.h"
// #include "terminal_pool.h" // Included transitively or stubbed

#include <iostream>
#include <thread>
#include <chrono>

// Stub header inclusions for things that might not exist yet to fix compilation
// In a real scenario these files should exist.

AgenticIDE::AgenticIDE() {
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
    
    m_toolRegistry = std::make_unique<ToolRegistry>();
    m_modelRouter = std::make_unique<UniversalModelRouter>();
    // m_zeroDayAgent = std::make_unique<ZeroDayAgenticEngine>(); // Commented out until header is confirmed fixed
    
    m_planOrchestrator = std::make_unique<RawrXD::PlanOrchestrator>();
    
    // Wire up callbacks
    m_agenticEngine->onResponseReady = [](const std::string& resp) {
        std::cout << "\n[Agent] " << resp << std::endl;
        std::cout << "> " << std::flush;
    };

    std::cout << "[AgenticIDE] Initialization Complete.\n";
}

void AgenticIDE::run() {
    std::cout << "[AgenticIDE] Starting Main Loop." << std::endl;
    m_running = true;

    // Start a background thread for the "Agent Logic" if pertinent, 
    // but for now we'll do a simple console REPL on the main thread.
    
    processConsoleInput();
}

void AgenticIDE::stop() {
    m_running = false;
}

void AgenticIDE::processConsoleInput() {
    std::string line;
    std::cout << "> ";
    while (m_running && std::getline(std::cin, line)) {
        if (line == "exit" || line == "quit") {
            stop();
            break;
        } else if (line == "status") {
            std::cout << "Editor Tabs: " << m_multiTabEditor->getTabCount() << "\n";
            std::cout << "Active File: " << m_multiTabEditor->getCurrentFilePath() << "\n";
        } else if (line.find("open ") == 0) {
            std::string path = line.substr(5);
            m_multiTabEditor->openFile(path);
            std::cout << "Opened " << path << "\n";
        } else if (line.find("cat") == 0) {
             std::cout << "--- BUFFER START ---\n";
             std::cout << m_multiTabEditor->getCurrentText();
             std::cout << "\n--- BUFFER END ---\n";
        } else if (line.find("edit ") == 0) {
            // Primitive editing from console
            std::string text = line.substr(5);
            if (auto tab = m_multiTabEditor->getActiveTab()) {
                tab->insertText(text + "\n");
                std::cout << "Inserted text.\n";
            }
        } else if (line.find("save") == 0) {
             m_multiTabEditor->saveCurrentFile();
        } else {
             // Pass to agent
             // m_agenticEngine->processRequest(line); // Assuming this method exists
             std::cout << "[Mock Agent] Processing: " << line << "\n";
        }
        std::cout << "> ";
    }
}

