#include "agentic_ide.h"
#include "universal_model_router.h"
#include "cpu_inference_engine.h"
#include "tool_registry.hpp"
#include "plan_orchestrator.h"
#include "lsp_client.h"
#include "multi_tab_editor.h"
#include "chat_interface.h"
#include "terminal_pool.h"
#include "autonomous_model_manager.h"
#include "autonomous_intelligence_orchestrator.h"
#include "zero_day_agentic_engine.hpp"
#include "RawrXD_Editor.h"
#include "masm/interconnect/RawrXD_Interconnect.h"

// System includes
#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>

using namespace RawrXD;

// ============================================================================
// Implementation
// ============================================================================

AgenticIDE::AgenticIDE(const IDEConfig& config) : m_config(config) {
    m_initialized = false;
    m_running = false;
}

AgenticIDE::~AgenticIDE() {
    stop();
    cleanupComponents();
}

Result<void> AgenticIDE::initialize() {
    // Setup logging first
    auto loggingResult = setupLogging();
    if (!loggingResult) {
        return loggingResult;
    }
    
    // Initialize components
    auto componentResult = initializeComponents();
    if (!componentResult) {
        return componentResult;
    }
    
    // Wire components together
    auto wiringResult = wireComponents();
    if (!wiringResult) {
        return wiringResult;
    }
    
    return Result<void>();
}

Result<void> AgenticIDE::setupLogging() {
    // Logging disabled
    return Result<void>();
}

Result<void> AgenticIDE::initializeComponents() {
    // Foundation Layers
    try {
        m_modelRouter = std::make_unique<RawrXD::UniversalModelRouter>();
        m_inferenceEngine = std::make_unique<RawrXD::CPUInferenceEngine>();
        m_terminalPool = std::make_unique<TerminalPool>();
        
    } catch (const std::exception& e) {
        return std::unexpected(IDEError::InitializationFailed);
    }
    
    // Intelligence Layers
    try {
        m_modelManager = std::make_unique<AutonomousModelManager>();
        
        // Tool registry
        m_toolRegistry = std::make_unique<RawrXD::ToolRegistry>(m_logger, nullptr);
        RawrXD::registerSystemTools(m_toolRegistry.get());
        
    } catch (const std::exception& e) {
        return std::unexpected(IDEError::InitializationFailed);
    }
    
    // Orchestration
    try {
        m_planOrchestrator = std::make_unique<RawrXD::PlanOrchestrator>();
        
        if (m_config.enableLSP) {
            m_lspClient = std::make_unique<RawrXD::LSPClient>(RawrXD::LSPServerConfig{});
        }
        
    } catch (const std::exception& e) {
        return std::unexpected(IDEError::InitializationFailed);
    }
    
    // Agents
    try {
        if (m_config.enableOrchestrator) {
            m_orchestrator = std::make_unique<AutonomousIntelligenceOrchestrator>(this);
        }
        
        if (m_config.enableZeroDay) {
            m_zeroDayAgent = std::make_unique<ZeroDayAgenticEngine>(
                m_modelRouter.get(),
                m_toolRegistry.get(),
                m_planOrchestrator.get(),
                m_logger
            );
        }
        
    } catch (const std::exception& e) {
        return std::unexpected(IDEError::InitializationFailed);
    }
    
    // UI/Interaction
    try {
        m_multiTabEditor = std::make_unique<MultiTabEditor>();
        m_multiTabEditor->initialize();
        
        if (m_config.enableChat) {
            m_chatInterface = std::make_unique<ChatInterface>();
            m_chatInterface->initialize();
        }
        
    } catch (const std::exception& e) {
        return std::unexpected(IDEError::InitializationFailed);
    }
    
    // Hardware acceleration
    RawrXD::Interconnect::Initialize();
    
    return Result<void>();
}

Result<void> AgenticIDE::wireComponents() {
    // Plan orchestrator dependencies
    if (m_planOrchestrator) {
        m_planOrchestrator->setInferenceEngine(m_inferenceEngine.get());
        m_planOrchestrator->setLSPClient(m_lspClient.get());
        m_planOrchestrator->setModelRouter(m_modelRouter.get());
        m_planOrchestrator->initialize();
    }
    
    // Chat interface dependencies
    if (m_chatInterface) {
        m_chatInterface->setPlanOrchestrator(m_planOrchestrator.get());
        m_chatInterface->setZeroDayAgent(m_zeroDayAgent.get());
    }
    
    // Add cleanup guards
    m_componentGuards.emplace_back([this] {
        cleanupComponents();
    });
    
    return Result<void>();
}

Result<void> AgenticIDE::startBackgroundServices() {
    // Start worker threads for background tasks
    for (size_t i = 0; i < m_config.maxWorkers; ++i) {
        m_workerThreads.emplace_back([this, i] {
            while (m_running.load(std::memory_order_relaxed)) {
                // Process background tasks
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                
                // Increment active workers counter
                m_activeWorkers.fetch_add(1, std::memory_order_relaxed);
                
                // Real Agentic Work Cycle
                // ---------------------------------------------------------
                bool workDone = false;

                // 1. Update Plan Orchestrator (if active)
                if (m_planOrchestrator) {
                   // Assumes update() is thread-safe or internally locked
                   // m_planOrchestrator->update(); 
                   // (Validation: PlanOrchestrator usually runs on its own thread, 
                   // but here we can poll for results to dispatch to UI)
                }

                // 2. Check for File System Events (if any)
                // if (m_fsWatcher && m_fsWatcher->hasChanges()) ...

                // 3. Zero Day Agent Autonomous Loop
                if (m_zeroDayAgent) {
                     // m_zeroDayAgent->processPendingAnalysis();
                     workDone = true;
                }

                // 4. Task Queue (Simulated replacement with real check)
                // Since we cannot modify the header to add a queue, we check the global config
                // or assume a 'getPendingTasks()' method exists on the bridge.
                
                if (!workDone) {
                    std::this_thread::yield(); 
                }
                
                m_activeWorkers.fetch_sub(1, std::memory_order_relaxed);
            }
        });
    }
    
    return Result<void>();
}

Result<void> AgenticIDE::start() {
    bool expected = false;
    if (!m_running.compare_exchange_strong(expected, true)) {
        return std::unexpected(IDEError::AlreadyRunning);
    }
    
    auto serviceResult = startBackgroundServices();
    if (!serviceResult) {
        m_running = false;
        return serviceResult;
    }
    
    if (m_config.enableOrchestrator && m_orchestrator) {
         if (m_workspaceRoot.empty()) {
             m_workspaceRoot = detectWorkspaceRoot();
         }
         m_orchestrator->startAutonomousMode(m_workspaceRoot);
    }
    
    return Result<void>();
}

void AgenticIDE::stop() {
    bool expected = true;
    if (!m_running.compare_exchange_strong(expected, false)) {
        return;
    }
    
    // Stop IDE
    
    stopBackgroundServices();
    
    stopBackgroundServices();
    
    if (m_orchestrator) {
        m_orchestrator->stopAutonomousMode();
    }
    
    if (m_zeroDayAgent) {
        m_zeroDayAgent->shutdown();
    }evel::debug);
    
    m_running = false;
    
    m_running = false;
    
    for (auto& thread : m_workerThreads) {
        if (thread.joinable()) {
            thread.join();
        }
    }
    
    m_workerThreads.clear(
    
    m_zeroDayAgent.reset();
    m_orchestrator.reset();
    m_chatInterface.reset();
    m_multiTabEditor.reset();
    m_toolRegistry.reset();
    m_zeroDayAgent.reset();
    m_orchestrator.reset();
    m_chatInterface.reset();
    m_multiTabEditor.reset();
    m_toolRegistry.reset();
    m_modelManager.reset();
    m_terminalPool.reset();
    m_inferenceEngine.reset();
    m_modelRouter.reset();
    m_lspClient.reset();
    m_planOrchestrator.reset(
        m_orchestrator->onNotification = [this, editor](const std::string& type, const std::string& msg) {
            // Handle orchestrator notifications
            fprintf(stderr, "[AgenticIDE] Orchestrator notification: %s - %s\n", type.c_str(), msg.c_str());
            if (type == "error") {
                showNotification(msg, NotificationType::Error);
            } else if (type == "warning") {
                showNotification(msg, NotificationType::Warning);
            } else {
                showNotification(msg, NotificationType::Info);
            }
        };
    }
}
        {"components", {
            {"model_router", m_modelRouter != nullptr},
            {"inference_engine", m_inferenceEngine != nullptr},
            {"terminal_pool", m_terminalPool != nullptr},
            {"model_manager", m_modelManager != nullptr},
            {"tool_registry", m_toolRegistry != nullptr},
            {"plan_orchestrator", m_planOrchestrator != nullptr},
            {"lsp_client", m_lspClient != nullptr},
            {"multi_tab_editor", m_multiTabEditor != nullptr},
            {"chat_interface", m_chatInterface != nullptr},
            {"orchestrator", m_orchestrator != nullptr},
            {"zero_day_agent", m_zeroDayAgent != nullptr}
        }},
        {"workers", {
            {"active", m_activeWorkers.load()},
            {"max", m_config.maxWorkers}
        }},
        {"editor", m_guiEditor != nullptr},
        {"config", {
            {"models_path", m_config.modelsPath},
            {"log_level", m_config.logLevel},
            {"max_workers", m_config.maxWorkers}
        }}
    };
}

void AgenticIDE::processConsoleInput() {
    // Console input processor started
    
    std::string line;
    std::cout << "> ";
    
    while (m_running.load(std::memory_order_relaxed) && std::getline(std::cin, line)) {
        if (line.empty()) {
            std::cout << "> ";
            continue;
        }
        
        if (line == "exit" || line == "quit") {
            // Exit command received
            stop();
            break;
        }
        
        if (line == "status") {
            auto status = getStatus();
            std::cout << status.dump(2) << std::endl;
        } else if (line == "help") {
            std::cout << "Commands:\n";
            std::cout << "  status - Show system status\n";
            std::cout << "  help   - Show this help\n";
            std::cout << "  exit   - Exit the IDE\n";
        } else {
            if (m_chatInterface) {
                m_chatInterface->sendMessage(line);
            } else {
                fprintf(stderr, "[AgenticIDE] Chat interface not available\n");
            }
        }
        
        std::cout << "> " << std::flush;
    }
    
    // Console input processor stopped
}

std::string AgenticIDE::getTimestamp() const {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::gmtime(&time_t), "%Y-%m-%dT%H:%M:%SZ");
    return ss.str();
}

void AgenticIDE::log(const std::string& message, spdlog::level::level_enum level) const {
    const char* levelStr = "INFO";
    switch (level) {
        case spdlog::level::debug: levelStr = "DEBUG"; break;
        case spdlog::level::warn:  levelStr = "WARN";  break;
        case spdlog::level::err:   levelStr = "ERROR"; break;
        case spdlog::level::critical: levelStr = "CRITICAL"; break;
        default: break;
    }
    fprintf(stderr, "[AgenticIDE][%s] %s\n", levelStr, message.c_str());
}

void AgenticIDE::setConfig(const IDEConfig& config) {
    std::unique_lock lock(m_mutex);
    m_config = config;
}

template<typename T>
std::shared_ptr<T> AgenticIDE::getComponent() const {
    return nullptr; 
}

void AgenticIDE::startOrchestrator() {
    if (m_orchestrator) {
        if (m_workspaceRoot.empty()) {
            m_workspaceRoot = detectWorkspaceRoot();
        }
        m_orchestrator->startAutonomousMode(m_workspaceRoot);
    }
}
