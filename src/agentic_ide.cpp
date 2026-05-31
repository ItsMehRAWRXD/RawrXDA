#include <thread>
#include <atomic>
#include <shared_mutex>
#include <filesystem>
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <compare>
#include <string>
#include <regex>
#include <vector>
#include <iostream>
#include <format>
#include "agentic_ide.h"
#include "zero_day_agentic_engine.hpp"
// #include <spdlog/sinks/basic_file_sink.h>
// #include <spdlog/sinks/stdout_color_sinks.h>
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
    try {
        m_inferenceEngine = std::make_shared<RawrXD::CPUInferenceEngine>();
        auto result = m_inferenceEngine->loadModel(m_config.modelsPath);
        if (!result) return IDEError::InitializationFailed;

        m_toolRegistry = std::make_shared<RawrXD::ToolRegistry>();
        m_planOrchestrator = std::make_shared<RawrXD::PlanOrchestrator>();
        m_chatInterface = std::make_shared<RawrXD::ChatInterface>();
        m_modelRouter = std::make_shared<RawrXD::UniversalModelRouter>();
        m_zeroDayAgent = std::make_shared<RawrXD::ZeroDayAgenticEngine>(
            m_modelRouter.get(),
            m_toolRegistry.get(),
            m_planOrchestrator.get(),
            nullptr);
        m_multiTabEditor = std::make_shared<RawrXD::MultiTabEditor>();
        m_modelManager = std::make_shared<RawrXD::AutonomousModelManager>();
        m_terminalPool = std::make_shared<RawrXD::TerminalPool>();
        m_orchestrator = std::make_shared<RawrXD::AutonomousIntelligenceOrchestrator>(this);
        
        // Detect workspace root and initialize LSP
        std::string workspaceRoot = detectWorkspaceRoot();
        RawrXD::LSPConfig lspConfig{};
        if (!workspaceRoot.empty()) {
            lspConfig.workspaceFolder = workspaceRoot;
        }
        
        m_lspClient = std::make_shared<RawrXD::LSPClient>(lspConfig);
        m_workspaceRoot = workspaceRoot;

        return Result<void>();
    } catch (...) {
        return IDEError::InitializationFailed;
    }
}

Result<void> AgenticIDE::wireComponents() {
    // Wire components
    
    // Plan orchestrator dependencies
        m_planOrchestrator->setLSPClient(m_lspClient.get());
        m_planOrchestrator->setModelRouter(m_modelRouter.get());
        m_planOrchestrator->setWorkspaceRoot(m_workspaceRoot);
        m_planOrchestrator->initialize();
    }
    
    // Chat interface dependencies
    if (m_chatInterface) {
        m_chatInterface->setPlanOrchestrator(m_planOrchestrator.get());
    }
    
    // Add cleanup guards
    m_componentGuards.emplace_back([this] {
        cleanupComponents();
    });
    
    return Result<void>();
}

Result<void> AgenticIDE::startBackgroundServices() {
    // Start background services
    
    // Start worker threads for background tasks
    for (size_t i = 0; i < m_config.maxWorkers; ++i) {
        m_workerThreads.emplace_back([this, i] {
            while (true) {
                std::function<void()> task;
                {
                    std::unique_lock<std::mutex> lock(m_queueMutex);
                    m_queueCv.wait(lock, [this] {
                        return !m_running.load(std::memory_order_relaxed) || !m_taskQueue.empty();
                    });

                    if (!m_running.load(std::memory_order_relaxed) && m_taskQueue.empty())
                        break;

                    if (m_taskQueue.empty()) continue; // Spurious wake?

                    task = std::move(m_taskQueue.front());
                    m_taskQueue.pop_front();
                }

                m_activeWorkers.fetch_add(1, std::memory_order_relaxed);
                try {
                    task();
                } catch (const std::exception& e) {
                    fprintf(stderr, "[AgenticIDE] Worker exception: %s\n", e.what());
                }
                m_activeWorkers.fetch_sub(1, std::memory_order_relaxed);
            }
        });
    }
}

Result<void> AgenticIDE::start() {
    bool expected = false;
    if (!m_running.compare_exchange_strong(expected, true)) {
        // IDE already running
        return IDEError::AlreadyRunning;
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
    
    // Stopping Agentic IDE
    
    stopBackgroundServices();
    
    if (m_orchestrator) {
        m_orchestrator->stopAutonomousMode();
    }
    
    }
    
    // ... existing cleanup ...

    return Result<void>();
}

void AgenticIDE::stopBackgroundServices() {
    // Stopping background services
    
    m_running = false;
    m_queueCv.notify_all(); // Wake up workers
    
    for (auto& thread : m_workerThreads) {
    m_running = false;
    m_queueCv.notify_all(); // Wake up workers
    
    for (auto& thread : m_workerThreads) {
        if (thread.joinable()) {
            thread.join();
        }
    }
    
    m_workerThreads.clear(
    m_orchestrator.reset();
    m_chatInterface.reset();
    m_multiTabEditor.reset();
    m_toolRegistry.reset();
    m_modelManager.reset();
    m_terminalPool.reset();
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
    m_planOrchestrator.reset();
}

json AgenticIDE::getStatus() const {
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

void AgenticIDE::log(const std::string& message, spdlog::level::level_enum level) {
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



void AgenticIDE::startOrchestrator() {
    if (m_orchestrator) {
        if (m_workspaceRoot.empty()) {
            m_workspaceRoot = detectWorkspaceRoot();
        }
        m_orchestrator->startAutonomousMode(m_workspaceRoot);
    }
}

void AgenticIDE::submitTask(std::function<void()> task) {
    {
        std::lock_guard<std::mutex> lock(m_queueMutex);
        m_taskQueue.push_back(std::move(task));
    }
    m_queueCv.notify_one();
}
std::string AgenticIDE::detectWorkspaceRoot() {
    // Try to detect workspace root by searching for marker files/directories
    // Starting from current working directory and moving up
    std::filesystem::path currentPath = std::filesystem::current_path();
    
    // Workspace marker detection order
    const std::vector<std::string> workspaceMarkers = {
        ".git",           // Git repository
        ".workspace",     // RawrXD workspace marker
        "CMakeLists.txt", // CMake project
        ".vscode",        // VS Code workspace
        "package.json",   // Node/NPM project
        "Cargo.toml"      // Rust project
    };
    
    // Search up to 10 levels or filesystem root
    int maxLevels = 10;
    int currentLevel = 0;
    
    while (currentLevel < maxLevels && !currentPath.empty()) {
        // Check for each marker
        for (const auto& marker : workspaceMarkers) {
            std::filesystem::path markerPath = currentPath / marker;
            
            if (std::filesystem::exists(markerPath)) {
                return currentPath.string();
            }
        }
        
        // Move up one directory
        std::filesystem::path parentPath = currentPath.parent_path();
        if (parentPath == currentPath) {
            // Reached filesystem root
            break;
        }
        
        currentPath = parentPath;
        currentLevel++;
    }
    
    // If no workspace root found, try to use config modelsPath directory
    if (!m_config.modelsPath.empty()) {
        std::filesystem::path modelsDir(m_config.modelsPath);
        if (std::filesystem::exists(modelsDir)) {
            return m_config.modelsPath;
        }
    }
    
    // Default fallback to current working directory
    std::string cwd = std::filesystem::current_path().string();
    log("No workspace markers found, using current directory: " + cwd, 
        spdlog::level::warn);
    
