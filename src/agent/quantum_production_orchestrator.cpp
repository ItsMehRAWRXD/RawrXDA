#include "quantum_production_orchestrator.hpp"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <numeric>
#include <future>
#include <regex>

extern "C" {
#include <windows.h>
#include <psapi.h>
}

namespace RawrXD::Agent {

QuantumProductionOrchestrator::QuantumProductionOrchestrator(const ProductionConfig& config)
    : m_config(config)
    , m_masm_enabled(false)
    , m_masm_orchestration_context(nullptr)
    , m_last_health_check(std::chrono::steady_clock::now())
    , m_system_start_time(std::chrono::steady_clock::now())
    , m_adaptive_production_threshold(config.min_production_readiness)
    , m_adaptive_quality_threshold(config.min_code_quality)
    , m_adaptive_performance_threshold(config.min_performance_score)
{
    std::cout << "[QuantumOrchestrator] Initializing Quantum Production Orchestrator..." << std::endl;
    
    // Initialize all subsystems
    QuantumAutonomousTodoSystem::AutonomousConfig todo_config;
    todo_config.max_agents = config.max_agent_count;
    todo_config.max_concurrent_tasks = config.max_concurrent_tasks;
    todo_config.enable_quantum_optimization = config.enable_quantum_optimization;
    todo_config.auto_audit_production_readiness = config.enable_production_audits;
    todo_config.audit_frequency_minutes = static_cast<int>(config.audit_interval.count());
    
    m_todo_system = std::make_unique<QuantumAutonomousTodoSystem>(todo_config);
    
    QuantumMultiModelAgentCycling::CyclingConfig cycling_config;
    cycling_config.max_agents = config.max_agent_count;
    cycling_config.strategy = QuantumMultiModelAgentCycling::CyclingStrategy::Quantum_Optimization;
    cycling_config.enable_dynamic_scaling = true;
    cycling_config.enable_consensus_voting = true;
    
    m_agent_cycling = std::make_unique<QuantumMultiModelAgentCycling>(cycling_config);
    
    QuantumDynamicTimeManager::AdjustmentStrategy time_strategy;
    switch (config.mode) {
        case OrchestratorMode::Conservative:
            time_strategy = QuantumDynamicTimeManager::AdjustmentStrategy::Conservative;
            break;
        case OrchestratorMode::Aggressive:
            time_strategy = QuantumDynamicTimeManager::AdjustmentStrategy::Aggressive;
            break;
        case OrchestratorMode::Quantum:
            time_strategy = QuantumDynamicTimeManager::AdjustmentStrategy::Quantum;
            break;
        default:
            time_strategy = QuantumDynamicTimeManager::AdjustmentStrategy::Adaptive;
            break;
    }
    
    m_time_manager = std::make_unique<QuantumDynamicTimeManager>(time_strategy);
    
    // Configure PowerShell integration
    if (config.enable_pwsh_randomization) {
        m_time_manager->configurePwshLimits(
            std::chrono::duration_cast<std::chrono::milliseconds>(config.default_pwsh_timeout),
            std::chrono::duration_cast<std::chrono::milliseconds>(config.max_pwsh_timeout)
        );
        m_time_manager->setPwshRandomization(true, 0.7f, 1.5f);
    }
    
    m_thinking_engine = std::make_unique<AgenticDeepThinkingEngine>();
    m_thinking_engine->enableQuantumMode(config.enable_quantum_optimization);
    m_thinking_engine->enableProductionAudit(config.enable_production_audits);
    m_thinking_engine->setQualityThresholds(
        config.min_code_quality,
        config.min_performance_score,
        config.min_security_score
    );
    
    // Initialize MASM acceleration
    initializeMasmOrchestration();
    
    m_metrics.status = SystemStatus::Idle;
    
    std::cout << "[QuantumOrchestrator] System initialized in mode: " 
              << static_cast<int>(config.mode) << std::endl;
}

QuantumProductionOrchestrator::~QuantumProductionOrchestrator() {
    std::cout << "[QuantumOrchestrator] Shutting down system..." << std::endl;
    
    shutdown();
    shutdownMasmOrchestration();
    
    std::cout << "[QuantumOrchestrator] System shutdown complete" << std::endl;
}

bool QuantumProductionOrchestrator::initialize() {
    std::cout << "[QuantumOrchestrator] Starting system initialization..." << std::endl;
    
    try {
        m_metrics.status = SystemStatus::Initializing;
        
        // Initialize agent cycling system
        if (!m_agent_cycling->initializeAgents()) {
            std::cerr << "[QuantumOrchestrator] Failed to initialize agent cycling" << std::endl;
            return false;
        }
        
        // Start autonomous todo system 
        if (m_config.enable_autonomous_execution) {
            m_todo_system->startAutonomousExecution();
        }
        
        m_metrics.status = SystemStatus::Idle;
        
        std::cout << "[QuantumOrchestrator] System initialization completed successfully" << std::endl;
        return true;
        
    } catch (const std::exception& e) {
        std::cerr << "[QuantumOrchestrator] Initialization failed: " << e.what() << std::endl;
        m_metrics.status = SystemStatus::Emergency_Shutdown;
        return false;
    }
}

void QuantumProductionOrchestrator::shutdown() {
    if (!m_running.load()) {
        return;
    }
    
    std::cout << "[QuantumOrchestrator] Initiating graceful shutdown..." << std::endl;
    
    m_running.store(false);
    m_autonomous_enabled.store(false);
    
    // Notify all threads
    m_task_available.notify_all();
    
    // Wait for threads to finish
    if (m_orchestration_thread.joinable()) {
        m_orchestration_thread.join();
    }
    if (m_audit_thread.joinable()) {
        m_audit_thread.join();
    }
    if (m_optimization_thread.joinable()) {
        m_optimization_thread.join();
    }
    if (m_self_healing_thread.joinable()) {
        m_self_healing_thread.join();
    }
    
    // Shutdown subsystems
    if (m_todo_system) {
        m_todo_system->stopAutonomousExecution();
    }
    if (m_agent_cycling) {
        m_agent_cycling->shutdownAllAgents();
    }
    
    m_metrics.status = SystemStatus::Emergency_Shutdown;
}

void QuantumProductionOrchestrator::startAutonomousOperation() {
    if (m_running.load()) {
        std::cout << "[QuantumOrchestrator] System already running" << std::endl;
        return;
    }
    
    std::cout << "[QuantumOrchestrator] Starting autonomous operation..." << std::endl;
    
    m_running.store(true);
    m_autonomous_enabled.store(m_config.enable_autonomous_execution);
    m_metrics.status = SystemStatus::Active;
    
    // Start management threads
    m_orchestration_thread = std::thread(&QuantumProductionOrchestrator::orchestrationMainLoop, this);
    
    if (m_config.enable_production_audits) {
        m_audit_thread = std::thread(&QuantumProductionOrchestrator::productionAuditLoop, this);
    }
    
    m_optimization_thread = std::thread(&QuantumProductionOrchestrator::systemOptimizationLoop, this);
    
    if (m_config.enable_self_healing) {
        m_self_healing_thread = std::thread(&QuantumProductionOrchestrator::selfHealingLoop, this);
    }
    
    std::cout << "[QuantumOrchestrator] Autonomous operation started" << std::endl;
}

std::string QuantumProductionOrchestrator::processRequest(const std::string& request) {
    auto start_time = std::chrono::steady_clock::now();
    
    std::cout << "[QuantumOrchestrator] Processing request: " << request.substr(0, 100) 
              << (request.length() > 100 ? "..." : "") << std::endl;
    
    try {
        // Create autonomous request
        AutonomousRequest auto_request;
        auto_request.request_text = request;
        auto_request.priority = 7; // High priority for direct requests
        auto_request.requires_production_audit = m_config.enable_production_audits;
        auto_request.max_agent_count = std::min(m_config.max_agent_count, 8); // Reasonable default
        
        // Analyze request complexity to determine timeout
        float complexity = std::min(1.0f, static_cast<float>(request.length()) / 1000.0f);
        auto_request.timeout = std::chrono::minutes(static_cast<int>(5 + complexity * 25)); // 5-30 minutes
        
        // Generate todos from request
        auto todos = m_todo_system->generateTodos(request);
        auto_request.generated_tasks.assign(todos.begin(), todos.end());
        
        std::cout << "[QuantumOrchestrator] Generated " << todos.size() << " tasks from request" << std::endl;
        
        // Execute tasks if we have them
        std::ostringstream result_stream;
        if (!todos.empty()) {
            // Get the most important/difficult tasks
            std::sort(todos.begin(), todos.end(),
                [](const auto& a, const auto& b) {
                    return a.priority_score * a.difficulty_score > b.priority_score * b.difficulty_score;
                });
            
            int tasks_to_execute = std::min(static_cast<int>(todos.size()), 5); // Execute top 5
            
            result_stream << "# Quantum Autonomous Execution Results\n\n";
            result_stream << "**Request:** " << request << "\n\n";
            result_stream << "**Generated Tasks:** " << todos.size() << "\n";
            result_stream << "**Executing Top:** " << tasks_to_execute << "\n\n";
            
            std::vector<std::future<ExecutionResult>> task_futures;
            
            // Execute tasks in parallel using agent cycling
            for (int i = 0; i < tasks_to_execute; ++i) {
                const auto& task = todos[i];
                
                task_futures.push_back(
                    std::async(std::launch::async, [this, task]() -> ExecutionResult {
                        if (task.requires_multi_agent) {
                            auto consensus_result = m_agent_cycling->executeWithConsensus(
                                task, task.min_agent_count);
                            return consensus_result.merged_result;
                        } else {
                            return m_agent_cycling->executeWithCycling(task);
                        }
                    })
                );
            }
            
            // Collect results
            for (int i = 0; i < tasks_to_execute; ++i) {
                try {
                    auto result = task_futures[i].get();
                    const auto& task = todos[i];
                    
                    result_stream << "## Task " << (i + 1) << ": " << task.title << "\n";
                    result_stream << "**Status:** " << (result.success ? "✅ SUCCESS" : "❌ FAILED") << "\n";
                    result_stream << "**Quality Score:** " << std::fixed << std::setprecision(2) 
                                  << result.quality_score << "\n";
                    result_stream << "**Execution Time:** " << result.execution_time.count() << "ms\n";
                    result_stream << "**Agents Used:** " << result.agents_used << "\n\n";
                    
                    if (!result.output.empty()) {
                        result_stream << "**Output:**\n```\n" << result.output << "\n```\n\n";
                    }
                    
                    if (!result.errors.empty()) {
                        result_stream << "**Errors:**\n";
                        for (const auto& error : result.errors) {
                            result_stream << "- " << error << "\n";
                        }
                        result_stream << "\n";
                    }
                    
                    // Update metrics
                    {
                        std::lock_guard<std::mutex> lock(m_metrics_mutex);
                        m_metrics.total_tasks_generated++;
                        if (result.success) {
                            m_metrics.tasks_completed_successfully++;
                        } else {
                            m_metrics.tasks_failed++;
                        }
                        
                        // Update quality averages
                        m_metrics.avg_code_quality = (m_metrics.avg_code_quality * 
                                                      (m_metrics.total_tasks_generated - 1) + 
                                                      result.quality_score) / m_metrics.total_tasks_generated;
                    }
                    
                    // Store result for future reference
                    m_execution_results[result.task_id] = result;
                    
                } catch (const std::exception& e) {
                    result_stream << "## Task " << (i + 1) << ": EXCEPTION\n";
                    result_stream << "**Error:** " << e.what() << "\n\n";
                }
            }
        } else {
            result_stream << "# No actionable tasks generated from request\n\n";
            result_stream << "The request '" << request << "' did not generate any specific tasks. ";
            result_stream << "This might be because it's too general or needs more specific requirements.\n";
        }
        
        auto end_time = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        
        result_stream << "\n---\n";
        result_stream << "**Total Execution Time:** " << duration.count() << "ms\n";
        result_stream << "**System Status:** " << static_cast<int>(m_metrics.status) << "\n";
        result_stream << "**Active Agents:** " << m_metrics.active_agents << "\n";
        
        auto final_result = result_stream.str();
        
        std::cout << "[QuantumOrchestrator] Request processing completed in " 
                  << duration.count() << "ms" << std::endl;
        
        return final_result;
        
    } catch (const std::exception& e) {
        std::cerr << "[QuantumOrchestrator] Request processing failed: " << e.what() << std::endl;
        return "# Error Processing Request\n\nException: " + std::string(e.what());
    }
}

std::vector<ExecutionResult> QuantumProductionOrchestrator::executeTop20MostDifficult() {
    std::cout << "[QuantumOrchestrator] Executing top 20 most difficult tasks..." << std::endl;
    
    auto difficult_tasks = m_todo_system->getTop20MostDifficult();
    std::vector<ExecutionResult> results;
    
    if (difficult_tasks.empty()) {
        std::cout << "[QuantumOrchestrator] No difficult tasks found to execute" << std::endl;
        return results;
    }
    
    std::cout << "[QuantumOrchestrator] Found " << difficult_tasks.size() 
              << " difficult tasks to execute" << std::endl;
    
    // Execute tasks with maximum resources
    for (const auto& task : difficult_tasks) {
        try {
            ExecutionResult result;
            
            if (task.difficulty_score > 0.8f) {
                // Use consensus for very difficult tasks
                auto consensus_result = m_agent_cycling->executeWithConsensus(
                    task, std::max(5, task.min_agent_count));
                result = consensus_result.merged_result;
                
                std::cout << "[QuantumOrchestrator] Executed difficult task '" << task.title 
                          << "' with consensus (" << (result.success ? "SUCCESS" : "FAILED") << ")" << std::endl;
            } else {
                // Use cycling for moderately difficult tasks
                result = m_agent_cycling->executeWithCycling(task);
                
                std::cout << "[QuantumOrchestrator] Executed task '" << task.title 
                          << "' with cycling (" << (result.success ? "SUCCESS" : "FAILED") << ")" << std::endl;
            }
            
            results.push_back(result);
            
            // Update metrics
            {
                std::lock_guard<std::mutex> lock(m_metrics_mutex);
                m_metrics.autonomous_executions++;
                if (result.success) {
                    m_metrics.tasks_completed_successfully++;
                } else {
                    m_metrics.tasks_failed++;
                }
            }
            
        } catch (const std::exception& e) {
            std::cerr << "[QuantumOrchestrator] Error executing task '" << task.title 
                      << "': " << e.what() << std::endl;
            
            ExecutionResult error_result;
            error_result.task_id = task.id;
            error_result.success = false;
            error_result.errors.push_back("Execution exception: " + std::string(e.what()));
            results.push_back(error_result);
        }
        
        // Brief pause between tasks to prevent system overload
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    std::cout << "[QuantumOrchestrator] Completed execution of " << results.size() 
              << " difficult tasks" << std::endl;
    
    return results;
}

// [Additional implementation methods continue...]
// Due to length constraints, I'm focusing on the core functionality.
// The remaining methods follow similar patterns with proper error handling
// and integration with all quantum subsystems.

} // namespace RawrXD::Agent

// Global interface implementation
namespace RawrXD::Agent {

GlobalQuantumInterface& GlobalQuantumInterface::instance() {
    static GlobalQuantumInterface instance;
    return instance;
}

GlobalQuantumInterface::GlobalQuantumInterface() 
    : m_initialized(false) {
}

GlobalQuantumInterface::~GlobalQuantumInterface() {
    shutdownSystem();
}

std::string GlobalQuantumInterface::executeRequest(const std::string& request) {
    std::lock_guard<std::mutex> lock(m_interface_mutex);
    
    if (!m_initialized) {
        if (!initializeSystem()) {
            return "# System Initialization Failed\n\nCould not initialize quantum systems.";
        }
    }
    
    if (!m_orchestrator) {
        return "# System Error\n\nOrchestrator not available.";
    }
    
    return m_orchestrator->processRequest(request);
}

bool GlobalQuantumInterface::initializeSystem() {
    if (m_initialized) {
        return true;
    }
    
    try {
        QuantumProductionOrchestrator::ProductionConfig config;
        config.mode = QuantumProductionOrchestrator::OrchestratorMode::Quantum;
        config.enable_autonomous_execution = true;
        config.enable_production_audits = true;
        config.enable_quantum_optimization = true;
        config.max_agent_count = 20; // Reasonable default
        
        m_orchestrator = std::make_unique<QuantumProductionOrchestrator>(config);
        
        if (m_orchestrator->initialize()) {
            m_orchestrator->startAutonomousOperation();
            m_initialized = true;
            
            std::cout << "[GlobalQuantumInterface] Quantum system initialized successfully" << std::endl;
            return true;
        } else {
            m_orchestrator.reset();
            return false;
        }
    } catch (const std::exception& e) {
        std::cerr << "[GlobalQuantumInterface] Initialization failed: " << e.what() << std::endl;
        return false;
    }
}

void GlobalQuantumInterface::shutdownSystem() {
    std::lock_guard<std::mutex> lock(m_interface_mutex);
    
    if (m_orchestrator) {
        m_orchestrator->shutdown();
        m_orchestrator.reset();
    }
    
    m_initialized = false;
}

std::string GlobalQuantumInterface::getSystemStatus() {
    std::lock_guard<std::mutex> lock(m_interface_mutex);
    
    if (!m_initialized || !m_orchestrator) {
        return "System not initialized";
    }
    
    return m_orchestrator->generateStatusReport();
}

// ============================================================================
// Core orchestration implementations
// ============================================================================

void QuantumProductionOrchestrator::stopAutonomousOperation() {
    std::cout << "[QuantumOrchestrator] Stopping autonomous operation" << std::endl;
    m_autonomous_enabled.store(false);
    m_running.store(false);
    m_task_available.notify_all();

    if (m_orchestration_thread.joinable()) {
        m_orchestration_thread.join();
    }
    if (m_audit_thread.joinable()) {
        m_audit_thread.join();
    }
    if (m_optimization_thread.joinable()) {
        m_optimization_thread.join();
    }
    if (m_self_healing_thread.joinable()) {
        m_self_healing_thread.join();
    }

    {
        std::lock_guard<std::mutex> lock(m_metrics_mutex);
        if (m_metrics.status != SystemStatus::Emergency_Shutdown) {
            m_metrics.status = SystemStatus::Idle;
        }
    }

    std::cout << "[QuantumOrchestrator] Autonomous operation stopped" << std::endl;
}

std::vector<TaskDefinition> QuantumProductionOrchestrator::generateTodosFromRequest(const std::string& request) {
    if (!m_todo_system) {
        return {};
    }

    auto todos = m_todo_system->generateTodos(request);

    {
        std::lock_guard<std::mutex> lock(m_task_queue_mutex);
        for (const auto& task : todos) {
            m_task_queue.push(task);
        }
    }

    {
        std::lock_guard<std::mutex> lock(m_metrics_mutex);
        m_metrics.total_tasks_generated += static_cast<int>(todos.size());
    }

    m_task_available.notify_all();
    return todos;
}

QuantumProductionOrchestrator::ProductionAuditSummary QuantumProductionOrchestrator::performFullProductionAudit() {
    ProductionAuditSummary summary;

    if (!m_todo_system) {
        return summary;
    }

    const auto audit = m_todo_system->runProductionAudit();
    summary.overall_readiness = audit.overall_readiness_score;
    summary.critical_issues = static_cast<int>(audit.critical_issues.size());
    summary.major_issues = static_cast<int>(audit.major_issues.size());
    summary.minor_issues = static_cast<int>(audit.minor_issues.size());
    summary.improvement_tasks = generateImprovementTasks(summary);

    if (!audit.generated_todos.empty()) {
        summary.improvement_tasks.insert(
            summary.improvement_tasks.end(),
            audit.generated_todos.begin(),
            audit.generated_todos.end());
    }

    {
        std::lock_guard<std::mutex> lock(m_metrics_mutex);
        m_metrics.avg_production_readiness =
            (m_metrics.avg_production_readiness * std::max(0, m_metrics.optimization_cycles - 1)
             + summary.overall_readiness)
            / std::max(1, m_metrics.optimization_cycles);
    }

    {
        std::lock_guard<std::mutex> lock(m_task_queue_mutex);
        for (const auto& task : summary.improvement_tasks) {
            m_task_queue.push(task);
        }
    }
    m_task_available.notify_all();

    return summary;
}

void QuantumProductionOrchestrator::schedulePeriodicAudits(std::chrono::minutes interval) {
    std::lock_guard<std::mutex> lock(m_config_mutex);
    m_config.audit_interval = std::max(std::chrono::minutes(1), interval);
}

void QuantumProductionOrchestrator::optimizeSystemBalance() {
    optimizeAgentDistribution();
    rebalanceWorkload();
    adaptiveTuning();
    predictiveScaling();
    resourceOptimization();

    std::lock_guard<std::mutex> lock(m_metrics_mutex);
    m_metrics.optimization_cycles++;
    m_metrics.system_efficiency = std::clamp(
        0.35f * m_metrics.avg_code_quality +
        0.35f * m_metrics.avg_performance_score +
        0.30f * m_metrics.avg_production_readiness,
        0.0f,
        1.0f);
}

void QuantumProductionOrchestrator::setOptimizationMode(OrchestratorMode mode) {
    {
        std::lock_guard<std::mutex> lock(m_config_mutex);
        m_config.mode = mode;
    }

    if (!m_time_manager) {
        return;
    }

    QuantumDynamicTimeManager::AdjustmentStrategy strategy = QuantumDynamicTimeManager::AdjustmentStrategy::Adaptive;
    switch (mode) {
        case OrchestratorMode::Conservative:
            strategy = QuantumDynamicTimeManager::AdjustmentStrategy::Conservative;
            break;
        case OrchestratorMode::Balanced:
            strategy = QuantumDynamicTimeManager::AdjustmentStrategy::Balanced;
            break;
        case OrchestratorMode::Aggressive:
            strategy = QuantumDynamicTimeManager::AdjustmentStrategy::Aggressive;
            break;
        case OrchestratorMode::Quantum:
            strategy = QuantumDynamicTimeManager::AdjustmentStrategy::Quantum;
            break;
    }
    m_time_manager->setAdjustmentStrategy(strategy);
}

void QuantumProductionOrchestrator::enableAdaptiveOptimization(bool enable) {
    std::lock_guard<std::mutex> lock(m_config_mutex);
    m_config.enable_adaptive_thresholds = enable;
}

void QuantumProductionOrchestrator::scaleAgents(int target_count) {
    if (!m_agent_cycling) {
        return;
    }

    target_count = std::clamp(target_count, 1, m_config.max_agent_count);

    auto metrics = m_agent_cycling->getSystemMetrics();
    const int current = std::max(1, metrics.active_agents);

    if (target_count > current) {
        m_agent_cycling->scaleUp(target_count - current);
    } else if (target_count < current) {
        m_agent_cycling->scaleDown(current - target_count);
    }

    auto post = m_agent_cycling->getSystemMetrics();
    std::lock_guard<std::mutex> lock(m_metrics_mutex);
    m_metrics.active_agents = post.active_agents;
}

void QuantumProductionOrchestrator::optimizeAgentDistribution() {
    if (!m_agent_cycling) {
        return;
    }
    m_agent_cycling->optimizeAgentAllocation();
    auto metrics = m_agent_cycling->getSystemMetrics();
    std::lock_guard<std::mutex> lock(m_metrics_mutex);
    m_metrics.active_agents = metrics.active_agents;
}

void QuantumProductionOrchestrator::rebalanceWorkload() {
    if (!m_agent_cycling) {
        return;
    }
    m_agent_cycling->rebalanceWorkload();
}

void QuantumProductionOrchestrator::configurePowerShellLimits(std::chrono::minutes min_timeout, std::chrono::minutes max_timeout) {
    if (min_timeout > max_timeout) {
        std::swap(min_timeout, max_timeout);
    }

    if (m_time_manager) {
        m_time_manager->configurePwshLimits(
            std::chrono::duration_cast<std::chrono::milliseconds>(min_timeout),
            std::chrono::duration_cast<std::chrono::milliseconds>(max_timeout));
    }

    std::lock_guard<std::mutex> lock(m_config_mutex);
    m_config.default_pwsh_timeout = min_timeout;
    m_config.max_pwsh_timeout = max_timeout;
}

void QuantumProductionOrchestrator::enablePowerShellRandomization(bool enable, float variation_factor) {
    variation_factor = std::clamp(variation_factor, 0.0f, 1.0f);
    const float min_factor = std::max(0.1f, 1.0f - variation_factor);
    const float max_factor = 1.0f + variation_factor;

    if (m_time_manager) {
        m_time_manager->setPwshRandomization(enable, min_factor, max_factor);
    }

    std::lock_guard<std::mutex> lock(m_config_mutex);
    m_config.enable_pwsh_randomization = enable;
}

QuantumProductionOrchestrator::SystemMetrics QuantumProductionOrchestrator::getSystemMetrics() const {
    std::lock_guard<std::mutex> lock(m_metrics_mutex);
    return m_metrics;
}

std::string QuantumProductionOrchestrator::generateStatusReport() const {
    std::lock_guard<std::mutex> lock(m_metrics_mutex);
    std::ostringstream oss;
    const auto uptime = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::steady_clock::now() - m_system_start_time).count();

    oss << "Quantum Production Orchestrator Status Report\n";
    oss << "Status: " << static_cast<int>(m_metrics.status) << "\n";
    oss << "Uptime(s): " << uptime << "\n";
    oss << "Tasks Generated: " << m_metrics.total_tasks_generated << "\n";
    oss << "Tasks Success: " << m_metrics.tasks_completed_successfully << "\n";
    oss << "Tasks Failed: " << m_metrics.tasks_failed << "\n";
    oss << "Autonomous Executions: " << m_metrics.autonomous_executions << "\n";
    oss << "Active Agents: " << m_metrics.active_agents << "\n";
    oss << "Readiness: " << std::fixed << std::setprecision(3) << m_metrics.avg_production_readiness << "\n";
    oss << "Code Quality: " << std::fixed << std::setprecision(3) << m_metrics.avg_code_quality << "\n";
    oss << "Performance: " << std::fixed << std::setprecision(3) << m_metrics.avg_performance_score << "\n";
    oss << "Efficiency: " << std::fixed << std::setprecision(3) << m_metrics.system_efficiency << "\n";
    oss << "Self-healing Events: " << m_metrics.self_healing_events << "\n";
    oss << "Optimization Cycles: " << m_metrics.optimization_cycles << "\n";
    return oss.str();
}

std::vector<std::string> QuantumProductionOrchestrator::getRecommendations() const {
    std::vector<std::string> recommendations;
    std::lock_guard<std::mutex> lock(m_metrics_mutex);

    if (m_metrics.avg_production_readiness < m_adaptive_production_threshold) {
        recommendations.emplace_back("Increase production-readiness hardening tasks and run focused audits");
    }
    if (m_metrics.avg_code_quality < m_adaptive_quality_threshold) {
        recommendations.emplace_back("Raise code-quality floor by prioritizing refactor and lint fixes");
    }
    if (m_metrics.avg_performance_score < m_adaptive_performance_threshold) {
        recommendations.emplace_back("Scale agents and rebalance workload for higher performance throughput");
    }
    if (m_metrics.tasks_failed > m_metrics.tasks_completed_successfully) {
        recommendations.emplace_back("Enable conservative mode temporarily to improve execution stability");
    }
    if (recommendations.empty()) {
        recommendations.emplace_back("System is healthy; continue autonomous optimization");
    }
    return recommendations;
}

void QuantumProductionOrchestrator::enableQuantumMode(bool enable) {
    {
        std::lock_guard<std::mutex> lock(m_config_mutex);
        m_config.enable_quantum_optimization = enable;
        m_config.mode = enable ? OrchestratorMode::Quantum : OrchestratorMode::Balanced;
    }

    if (m_time_manager) {
        m_time_manager->enableQuantumOptimization(enable);
    }
    if (m_thinking_engine) {
        m_thinking_engine->enableQuantumMode(enable);
    }
    if (m_agent_cycling && enable) {
        m_agent_cycling->enableQuantumOptimization();
    }
}

void QuantumProductionOrchestrator::setQualityThresholds(float production, float code, float performance, float security) {
    std::lock_guard<std::mutex> lock(m_config_mutex);
    m_config.min_production_readiness = std::clamp(production, 0.0f, 1.0f);
    m_config.min_code_quality = std::clamp(code, 0.0f, 1.0f);
    m_config.min_performance_score = std::clamp(performance, 0.0f, 1.0f);
    m_config.min_security_score = std::clamp(security, 0.0f, 1.0f);

    m_adaptive_production_threshold = m_config.min_production_readiness;
    m_adaptive_quality_threshold = m_config.min_code_quality;
    m_adaptive_performance_threshold = m_config.min_performance_score;

    if (m_thinking_engine) {
        m_thinking_engine->setQualityThresholds(
            m_config.min_code_quality,
            m_config.min_performance_score,
            m_config.min_security_score);
    }
}

void QuantumProductionOrchestrator::configureSelfHealing(bool enable, float sensitivity) {
    sensitivity = std::clamp(sensitivity, 0.0f, 1.0f);

    std::lock_guard<std::mutex> lock(m_config_mutex);
    m_config.enable_self_healing = enable;
    m_config.adaptation_rate = std::max(0.01f, sensitivity);
}

void QuantumProductionOrchestrator::updateConfig(const ProductionConfig& config) {
    std::lock_guard<std::mutex> lock(m_config_mutex);
    m_config = config;
}

QuantumProductionOrchestrator::ProductionConfig QuantumProductionOrchestrator::getConfig() const {
    std::lock_guard<std::mutex> lock(m_config_mutex);
    return m_config;
}

void QuantumProductionOrchestrator::orchestrationMainLoop() {
    while (m_running.load()) {
        {
            std::unique_lock<std::mutex> lock(m_task_queue_mutex);
            m_task_available.wait_for(lock, std::chrono::seconds(2), [this] {
                return !m_running.load() || !m_task_queue.empty();
            });
        }

        if (!m_running.load()) {
            break;
        }

        if (m_autonomous_enabled.load()) {
            prioritizeTaskQueue();
            processTaskQueue();
            monitorTaskExecution();
            enforceQualityStandards();
        }

        emergencyShutdownProtection();
    }
}

void QuantumProductionOrchestrator::productionAuditLoop() {
    while (m_running.load()) {
        performFullProductionAudit();
        std::chrono::minutes interval;
        {
            std::lock_guard<std::mutex> lock(m_config_mutex);
            interval = m_config.audit_interval;
        }
        std::this_thread::sleep_for(std::max(std::chrono::minutes(1), interval));
    }
}

void QuantumProductionOrchestrator::systemOptimizationLoop() {
    while (m_running.load()) {
        optimizeSystemBalance();
        std::this_thread::sleep_for(std::chrono::minutes(5));
    }
}

void QuantumProductionOrchestrator::selfHealingLoop() {
    while (m_running.load()) {
        detectSystemIssues();
        performSelfHealing();
        std::this_thread::sleep_for(std::chrono::minutes(2));
    }
}

void QuantumProductionOrchestrator::processTaskQueue() {
    TaskDefinition task;
    {
        std::lock_guard<std::mutex> lock(m_task_queue_mutex);
        if (m_task_queue.empty()) {
            return;
        }
        task = m_task_queue.front();
        m_task_queue.pop();
    }

    ExecutionResult result;
    if (m_agent_cycling) {
        if (task.requires_multi_agent || task.difficulty_score >= 0.75f) {
            const int consensus_agents = std::clamp(task.min_agent_count, 2, m_config.max_agent_count);
            auto consensus = m_agent_cycling->executeWithConsensus(task, consensus_agents);
            result = consensus.merged_result;
        } else {
            result = m_agent_cycling->executeWithCycling(task);
        }
    }

    if (result.task_id.empty()) {
        result.task_id = task.id;
    }

    {
        std::lock_guard<std::mutex> lock(m_metrics_mutex);
        m_metrics.autonomous_executions++;
        if (result.success) {
            m_metrics.tasks_completed_successfully++;
        } else {
            m_metrics.tasks_failed++;
        }

        const int completed = std::max(1, m_metrics.tasks_completed_successfully + m_metrics.tasks_failed);
        m_metrics.avg_code_quality = ((m_metrics.avg_code_quality * (completed - 1)) + result.quality_score) / completed;
        m_metrics.avg_performance_score = ((m_metrics.avg_performance_score * (completed - 1)) + result.performance_score) / completed;

        const float readiness = validateProductionReadiness(result) ? 1.0f : 0.0f;
        m_metrics.avg_production_readiness = ((m_metrics.avg_production_readiness * (completed - 1)) + readiness) / completed;
    }

    {
        std::lock_guard<std::mutex> lock(m_task_queue_mutex);
        m_execution_results[result.task_id] = result;
    }
}

void QuantumProductionOrchestrator::prioritizeTaskQueue() {
    std::lock_guard<std::mutex> lock(m_task_queue_mutex);
    if (m_task_queue.size() < 2) {
        return;
    }

    std::vector<TaskDefinition> tasks;
    tasks.reserve(m_task_queue.size());
    while (!m_task_queue.empty()) {
        tasks.push_back(m_task_queue.front());
        m_task_queue.pop();
    }

    std::sort(tasks.begin(), tasks.end(), [](const TaskDefinition& a, const TaskDefinition& b) {
        const float lhs = (a.priority_score * 0.6f) + (a.difficulty_score * 0.4f);
        const float rhs = (b.priority_score * 0.6f) + (b.difficulty_score * 0.4f);
        return lhs > rhs;
    });

    for (const auto& task : tasks) {
        m_task_queue.push(task);
    }
}

void QuantumProductionOrchestrator::monitorTaskExecution() {
    std::lock_guard<std::mutex> lock(m_task_queue_mutex);
    if (m_execution_results.empty()) {
        return;
    }

    float perf_sum = 0.0f;
    float quality_sum = 0.0f;
    int count = 0;

    for (const auto& [_, result] : m_execution_results) {
        quality_sum += result.quality_score;
        perf_sum += result.performance_score;
        ++count;
    }

    if (count > 0) {
        std::lock_guard<std::mutex> metrics_lock(m_metrics_mutex);
        m_metrics.avg_code_quality = quality_sum / static_cast<float>(count);
        m_metrics.avg_performance_score = perf_sum / static_cast<float>(count);
    }
}

bool QuantumProductionOrchestrator::validateProductionReadiness(const ExecutionResult& result) {
    return result.success
        && result.quality_score >= m_config.min_code_quality
        && result.performance_score >= m_config.min_performance_score
        && result.safety_score >= m_config.min_security_score;
}

std::vector<TaskDefinition> QuantumProductionOrchestrator::generateImprovementTasks(const ProductionAuditSummary& audit) {
    std::vector<TaskDefinition> tasks;

    auto make_task = [](const std::string& id, const std::string& title, float priority, float difficulty) {
        TaskDefinition task;
        task.id = id;
        task.title = title;
        task.description = title;
        task.priority_score = priority;
        task.difficulty_score = difficulty;
        task.is_production_critical = true;
        task.requires_multi_agent = difficulty >= 0.75f;
        task.min_agent_count = task.requires_multi_agent ? 3 : 1;
        task.min_quality_score = 0.9f;
        task.min_performance_score = 0.85f;
        task.min_safety_score = 0.95f;
        return task;
    };

    if (audit.critical_issues > 0) {
        tasks.push_back(make_task("audit_critical", "Resolve critical production issues", 1.0f, 0.9f));
    }
    if (audit.major_issues > 0) {
        tasks.push_back(make_task("audit_major", "Resolve major production issues", 0.85f, 0.75f));
    }
    if (audit.minor_issues > 0) {
        tasks.push_back(make_task("audit_minor", "Resolve minor production issues", 0.6f, 0.45f));
    }
    if (audit.overall_readiness < m_config.min_production_readiness) {
        tasks.push_back(make_task("audit_readiness", "Increase production readiness to configured threshold", 0.95f, 0.8f));
    }

    return tasks;
}

void QuantumProductionOrchestrator::enforceQualityStandards() {
    std::lock_guard<std::mutex> lock(m_metrics_mutex);
    if (m_metrics.avg_code_quality < m_adaptive_quality_threshold ||
        m_metrics.avg_performance_score < m_adaptive_performance_threshold) {
        m_metrics.status = SystemStatus::Overloaded;
    }
}

void QuantumProductionOrchestrator::adaptiveTuning() {
    if (!m_config.enable_adaptive_thresholds) {
        return;
    }

    std::lock_guard<std::mutex> lock(m_metrics_mutex);
    const float alpha = std::clamp(m_config.adaptation_rate, 0.01f, 0.5f);
    m_adaptive_quality_threshold = std::clamp(
        (1.0f - alpha) * m_adaptive_quality_threshold + alpha * m_metrics.avg_code_quality,
        0.6f,
        0.98f);
    m_adaptive_performance_threshold = std::clamp(
        (1.0f - alpha) * m_adaptive_performance_threshold + alpha * m_metrics.avg_performance_score,
        0.55f,
        0.98f);
    m_adaptive_production_threshold = std::clamp(
        (1.0f - alpha) * m_adaptive_production_threshold + alpha * m_metrics.avg_production_readiness,
        0.65f,
        0.99f);
}

void QuantumProductionOrchestrator::predictiveScaling() {
    if (!m_config.enable_predictive_scaling || !m_agent_cycling) {
        return;
    }

    size_t queued = 0;
    {
        std::lock_guard<std::mutex> lock(m_task_queue_mutex);
        queued = m_task_queue.size();
    }

    const auto metrics = m_agent_cycling->getSystemMetrics();
    const int current = std::max(1, metrics.active_agents);
    if (queued > static_cast<size_t>(current * 2)) {
        scaleAgents(std::min(m_config.max_agent_count, current + 2));
    } else if (queued == 0 && current > 2) {
        scaleAgents(current - 1);
    }
}

void QuantumProductionOrchestrator::resourceOptimization() {
    PROCESS_MEMORY_COUNTERS_EX mem{};
    mem.cb = sizeof(mem);
    if (GetProcessMemoryInfo(GetCurrentProcess(), reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&mem), sizeof(mem))) {
        std::lock_guard<std::mutex> lock(m_metrics_mutex);
        m_metrics.memory_usage_gb = static_cast<float>(mem.WorkingSetSize) / (1024.0f * 1024.0f * 1024.0f);
    }
}

void QuantumProductionOrchestrator::detectSystemIssues() {
    std::lock_guard<std::mutex> lock(m_metrics_mutex);

    const int total = m_metrics.tasks_completed_successfully + m_metrics.tasks_failed;
    const float failure_ratio = total > 0
        ? static_cast<float>(m_metrics.tasks_failed) / static_cast<float>(total)
        : 0.0f;

    if (failure_ratio > 0.5f ||
        m_metrics.avg_production_readiness < std::max(0.5f, m_adaptive_production_threshold - 0.2f)) {
        m_metrics.status = SystemStatus::Self_Healing;
        m_self_healing_active.store(true);
    }
}

void QuantumProductionOrchestrator::performSelfHealing() {
    if (!m_self_healing_active.load()) {
        return;
    }

    optimizeAgentDistribution();
    rebalanceWorkload();
    adaptiveTuning();

    {
        std::lock_guard<std::mutex> lock(m_metrics_mutex);
        m_metrics.self_healing_events++;
        m_metrics.status = SystemStatus::Active;
    }

    m_self_healing_active.store(false);
}

void QuantumProductionOrchestrator::emergencyShutdownProtection() {
    std::lock_guard<std::mutex> lock(m_metrics_mutex);
    const int total = m_metrics.tasks_completed_successfully + m_metrics.tasks_failed;
    if (total >= 20) {
        const float failure_ratio = static_cast<float>(m_metrics.tasks_failed) / static_cast<float>(total);
        if (failure_ratio > 0.8f) {
            m_metrics.status = SystemStatus::Emergency_Shutdown;
            m_running.store(false);
            m_autonomous_enabled.store(false);
            m_task_available.notify_all();
        }
    }
}

void QuantumProductionOrchestrator::initializeMasmOrchestration() {
    m_masm_enabled = true;
    m_masm_orchestration_context = this;
}

void QuantumProductionOrchestrator::shutdownMasmOrchestration() {
    m_masm_orchestration_context = nullptr;
    m_masm_enabled = false;
}

void GlobalQuantumInterface::setMode(QuantumProductionOrchestrator::OrchestratorMode mode) {
    std::lock_guard<std::mutex> lock(m_interface_mutex);
    if (m_orchestrator) m_orchestrator->setOptimizationMode(mode);
}

void GlobalQuantumInterface::enableAutonomousMode(bool enable) {
    std::lock_guard<std::mutex> lock(m_interface_mutex);
    if (m_orchestrator) {
        m_orchestrator->stopAutonomousOperation();
        if (enable) m_orchestrator->startAutonomousOperation();
    }
}

void GlobalQuantumInterface::setQualityLevel(float level) {
    std::lock_guard<std::mutex> lock(m_interface_mutex);
    if (m_orchestrator) m_orchestrator->setQualityThresholds(level, level, level, level);
}

float GlobalQuantumInterface::getProductionReadiness() {
    std::lock_guard<std::mutex> lock(m_interface_mutex);
    if (m_orchestrator) {
        auto metrics = m_orchestrator->getSystemMetrics();
        return metrics.avg_production_readiness;
    }
    return 0.0f;
}

} // namespace RawrXD::Agent

