#include "agentic_ide.h"
#include "agentic_engine.h"
#include "zero_day_agentic_engine.hpp" // Keep inconsistent naming if that's how it is on disk?
#include "universal_model_router.h"
#include "tool_registry.hpp"
#include "plan_orchestrator.h"
#include "lsp_client.h"
#include "multi_tab_editor.h"
#include "terminal_pool.h"
#include "chat_interface.h"
#include "cpu_inference_engine.h"
#include "RawrXD_Editor.h"

#include <iostream>
#include <thread>
#include <chrono>

// Stub header inclusions for things that might not exist yet to fix compilation
// In a real scenario these files should exist.

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

    m_toolRegistry = std::make_unique<ToolRegistry>(nullptr, nullptr);
    m_modelRouter = std::make_unique<UniversalModelRouter>();
    m_planOrchestrator = std::make_unique<RawrXD::PlanOrchestrator>();
    m_planOrchestrator->setInferenceEngine(m_inferenceEngine.get());
    m_planOrchestrator->initialize();

    m_zeroDayAgent = std::make_unique<ZeroDayAgenticEngine>(
        m_modelRouter.get(),
        m_toolRegistry.get(),
        m_planOrchestrator.get(),
        nullptr
    );

    // Initial check of health
    // m_modelRouter->getAvailableModels(); 
    
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
             // Pass to agent with context!
             std::string context = "";
             if (m_multiTabEditor) {
                 context = m_multiTabEditor->getCurrentText();
             }
             
             std::cout << "[AgenticIDE] Sending to engine with " << context.length() << " chars context...\n";
             m_agenticEngine->processMessage(line, context);
        }
        std::cout << "> ";
    }
}

void AgenticIDE::setEditor(RawrXD::Editor* editor) {
    m_guiEditor = editor;
    // Wire up agent engine to see this editor context if needed
}

