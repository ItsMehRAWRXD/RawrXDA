#include "agentic_ide.h"
#include "agentic/swarm_orchestrator.h"
#include "agentic/chain_of_thought.h"
#include "ai/universal_model_router.h"
#include "ai/token_generator.h"
#include "plan_orchestrator.h" // Legacy/Shim
#include "lsp_client.h" // Legacy/Shim
#include "RawrXD_Editor.h" 

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/basic_file_sink.h>

using namespace RawrXD;

AgenticIDE::AgenticIDE(const IDEConfig& config) 
    : m_config(config) {
    setupLogging();
}

AgenticIDE::~AgenticIDE() {
    stop();
}

void AgenticIDE::setupLogging() {
    try {
        auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        console_sink->set_level(spdlog::level::trace);
        
        std::vector<spdlog::sink_ptr> sinks {console_sink};
        
        if (m_config.enableFileLogging) {
            auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(m_config.logPath, true);
            file_sink->set_level(spdlog::level::trace);
            sinks.push_back(file_sink);
        }
        
        auto logger = std::make_shared<spdlog::logger>("AgenticIDE", sinks.begin(), sinks.end());
        logger->set_level(spdlog::level::from_str(m_config.logLevel));
        spdlog::set_default_logger(logger);
    } catch (const spdlog::spdlog_ex& ex) {
        std::cerr << "Log init failed: " << ex.what() << std::endl;
    }
}

Result<void> AgenticIDE::initialize() {
    spdlog::info("Initializing AgenticIDE v3.0...");
    
    // 1. Initialize Model Router
    m_modelRouter = std::make_shared<UniversalModelRouter>();
    
    // 2. Initialize Tokenizer
    m_tokenizer = std::make_shared<TokenGenerator>();
    
    // 3. Initialize Swarm
    spdlog::info("Initializing Swarm Orchestrator with {} agents...", m_config.maxWorkers);
    m_swarm = std::make_unique<SwarmOrchestrator>(m_config.maxWorkers);
    
    // 4. Initialize Chain of Thought
    m_chainOfThought = std::make_unique<ChainOfThought>();
    
    // 5. Initialize Legacy Components (if needed for interface compliancy)
    // m_planOrchestrator = std::make_unique<PlanOrchestrator>();
    // m_lspClient = std::make_unique<LSPClient>();
    
    spdlog::info("Initialization complete.");
    return Result<void>::unused();
}

Result<void> AgenticIDE::start() {
    if (m_running) return IDEError::AlreadyRunning;
    
    spdlog::info("Starting AgenticIDE services...");
    m_running = true;
    
    return Result<void>::unused();
}

void AgenticIDE::stop() {
    if (!m_running) return;
    spdlog::info("Stopping AgenticIDE...");
    m_running = false;
    
    if (m_swarm) {
        // m_swarm->shutdown();
    }
}

void AgenticIDE::setEditor(RawrXD::Editor* editor) {
    m_editor = editor;
    spdlog::info("Editor attached to IDE backend.");
}

nlohmann::json AgenticIDE::getStatus() const {
    nlohmann::json status;
    status["running"] = m_running.load();
    status["swarm_agents"] = m_swarm ? m_swarm->getActiveAgentCount() : 0;
    status["cot_active"] = m_chainOfThought ? true : false; 
    return status;
}

Result<void> AgenticIDE::loadModels() {
    return Result<void>::unused();
}
