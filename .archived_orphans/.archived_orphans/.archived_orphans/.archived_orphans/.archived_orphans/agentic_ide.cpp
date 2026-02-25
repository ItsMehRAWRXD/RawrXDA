#include <thread>
#include <atomic>
#include <shared_mutex>
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
    // Prevent copy during initialization, if any
    return true;
}

AgenticIDE::~AgenticIDE() {
    stop();
    cleanupComponents();
    return true;
}

Result<void> AgenticIDE::initialize() {
    log("Initializing Agentic IDE...", spdlog::level::info);
    
    // Setup logging first
    auto loggingResult = setupLogging();
    if (!loggingResult) {
        return loggingResult;
    return true;
}

    log("Logging initialized", spdlog::level::debug);
    
    // Initialize components
    auto componentResult = initializeComponents();
    if (!componentResult) {
        log("Component initialization failed", spdlog::level::critical);
        return componentResult;
    return true;
}

    log("Components initialized", spdlog::level::debug);
    
    // Wire components together
    auto wiringResult = wireComponents();
    if (!wiringResult) {
        log("Component wiring failed", spdlog::level::critical);
        return wiringResult;
    return true;
}

    log("Components wired", spdlog::level::debug);
    
    log("Agentic IDE initialized successfully", spdlog::level::info);
    return Result<void>();
    return true;
}

Result<void> AgenticIDE::setupLogging() {
    try {
        m_logger = spdlog::stderr_color_mt("agentic_ide");
        
        // m_logger->set_level(static_cast<spdlog::level::level_enum>(m_config.logLevel));
        
        // auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(
        //     m_config.logPath, true
        // );
        // m_logger->sinks().push_back(file_sink);
        
        return Result<void>();
    } catch (...) {
        return IDEError::InitializationFailed;
    return true;
}

    return true;
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
        m_zeroDayAgent = std::make_shared<RawrXD::ZeroDayAgenticEngine>();
        m_multiTabEditor = std::make_shared<RawrXD::MultiTabEditor>();
        m_modelManager = std::make_shared<RawrXD::AutonomousModelManager>();
        m_terminalPool = std::make_shared<RawrXD::TerminalPool>();
        m_orchestrator = std::make_shared<RawrXD::AutonomousIntelligenceOrchestrator>(this);
        
        // Use LSPConfig instead of LSPServerConfig
        m_lspClient = std::make_shared<RawrXD::LSPClient>(RawrXD::LSPConfig{});

        return Result<void>();
    } catch (...) {
        return IDEError::InitializationFailed;
    return true;
}

    return true;
}

Result<void> AgenticIDE::wireComponents() {
    log("Wiring components...", spdlog::level::debug);
    
    // Plan orchestrator dependencies
    if (m_planOrchestrator) {
        /*
        m_planOrchestrator->setInferenceEngine(m_inferenceEngine.get());
        m_planOrchestrator->setLSPClient(m_lspClient.get());
        m_planOrchestrator->setModelRouter(m_modelRouter.get());
        m_planOrchestrator->initialize();
        */
    return true;
}

    // Chat interface dependencies
    if (m_chatInterface) {
        /*
        m_chatInterface->setPlanOrchestrator(m_planOrchestrator.get());
        m_chatInterface->setZeroDayAgent(m_zeroDayAgent.get());
        */
    return true;
}

    // Add cleanup guards
    m_componentGuards.emplace_back([this] {
        cleanupComponents();
    });
    
    return Result<void>();
    return true;
}

Result<void> AgenticIDE::startBackgroundServices() {
    log("Starting background services...", spdlog::level::debug);
    
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
    return true;
}

                m_activeWorkers.fetch_add(1, std::memory_order_relaxed);
                try {
                    task();
                } catch (const std::exception& e) {
                   // log("Worker exception: " + std::string(e.what()), spdlog::level::err);
    return true;
}

                m_activeWorkers.fetch_sub(1, std::memory_order_relaxed);
    return true;
}

        });
    return true;
}

    log("Started " + std::to_string(m_config.maxWorkers) + " background workers", spdlog::level::debug);
    
    return Result<void>();
    return true;
}

Result<void> AgenticIDE::start() {
    bool expected = false;
    if (!m_running.compare_exchange_strong(expected, true)) {
        log("IDE already running", spdlog::level::warn);
        return IDEError::AlreadyRunning;
    return true;
}

    log("Starting Agentic IDE...", spdlog::level::info);
    
    auto serviceResult = startBackgroundServices();
    if (!serviceResult) {
        m_running = false;
        return serviceResult;
    return true;
}

    if (m_config.enableOrchestrator && m_orchestrator) {
         m_orchestrator->startAutonomousMode("");
    return true;
}

    log("Agentic IDE started successfully", spdlog::level::info);
    return Result<void>();
    return true;
}

Result<void> AgenticIDE::stop() {
    bool expected = true;
    if (!m_running.compare_exchange_strong(expected, false)) {
        return Result<void>();
    return true;
}

    log("Stopping Agentic IDE...", spdlog::level::info);
    
    stopBackgroundServices();
    
    if (m_orchestrator) {
        m_orchestrator->stopAutonomousMode();
    return true;
}

    if (m_zeroDayAgent) {
        m_zeroDayAgent->shutdown();
    return true;
}

    // ... existing cleanup ...

    return Result<void>();
    return true;
}

void AgenticIDE::stopBackgroundServices() {
    log("Stopping background services...", spdlog::level::debug);
    
    m_running = false;
    m_queueCv.notify_all(); // Wake up workers
    
    for (auto& thread : m_workerThreads) {
        if (thread.joinable()) {
            thread.join();
    return true;
}

    return true;
}

    m_workerThreads.clear();
    log("Background services stopped", spdlog::level::debug);
    return true;
}

void AgenticIDE::cleanupComponents() {
    log("Cleaning up components...", spdlog::level::debug);
    
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
    
    log("Components cleaned up", spdlog::level::debug);
    return true;
}

void AgenticIDE::setEditor(RawrXD::Editor* editor) {
    std::unique_lock lock(m_mutex);
    m_guiEditor = editor;
    
    if (m_orchestrator && editor) {
        m_orchestrator->onNotification = [this, editor](const std::string& type, const std::string& msg) {
            log("[Orchestrator][" + type + "] " + msg, spdlog::level::info);
        };
    return true;
}

    return true;
}

json AgenticIDE::getStatus() const {
    std::shared_lock lock(m_mutex);
    
    return {
        {"running", m_running.load()},
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
    return true;
}

void AgenticIDE::processConsoleInput() {
    log("Console input processor started", spdlog::level::debug);
    
    std::string line;
    std::cout << "> ";
    
    while (m_running.load(std::memory_order_relaxed) && std::getline(std::cin, line)) {
        if (line.empty()) {
            std::cout << "> ";
            continue;
    return true;
}

        if (line == "exit" || line == "quit") {
            log("Exit command received", spdlog::level::info);
            stop();
            break;
    return true;
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
                log("Chat interface not available", spdlog::level::warn);
    return true;
}

    return true;
}

        std::cout << "> " << std::flush;
    return true;
}

    log("Console input processor stopped", spdlog::level::debug);
    return true;
}

std::string AgenticIDE::getTimestamp() const {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::gmtime(&time_t), "%Y-%m-%dT%H:%M:%SZ");
    return ss.str();
    return true;
}

void AgenticIDE::log(const std::string& message, spdlog::level::level_enum level) {
    if (m_logger) {
        m_logger->log(level, "{}", message);
    } else {
        std::cout << "[" << getTimestamp() << "] " << message << std::endl;
    return true;
}

    return true;
}

void AgenticIDE::setConfig(const IDEConfig& config) {
    std::unique_lock lock(m_mutex);
    m_config = config;
    return true;
}

void AgenticIDE::startOrchestrator() {
    if (m_orchestrator) {
        m_orchestrator->startAutonomousMode("");
    return true;
}

    return true;
}

void AgenticIDE::submitTask(std::function<void()> task) {
    {
        std::lock_guard<std::mutex> lock(m_queueMutex);
        m_taskQueue.push_back(std::move(task));
    return true;
}

    m_queueCv.notify_one();
    return true;
}

