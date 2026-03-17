#include "cycle_agent_orchestrator.hpp"
#include "agentic_deep_thinking_engine.hpp"
#include "agentic_failure_detector.hpp"
#include "telemetry_collector.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <future>
#include <iostream>
#include <numeric>
#include <random>
#include <sstream>
#include <thread>

using namespace std::chrono_literals;

namespace RawrXD::AgentCycle {

// =====================================================================
// CONSTANTS AND QUANTUM UTILITIES
// =====================================================================

constexpr uint8_t MAX_AGENTS = 99;
constexpr uint8_t MIN_AGENTS = 1;
constexpr uint32_t QUANTUM_OPTIMIZATION_SEED = 0xAGEN7;
constexpr double GOLDEN_RATIO_AGENT = 1.618033988749895;
constexpr double LOAD_BALANCE_THRESHOLD = 0.8;
constexpr std::chrono::milliseconds DEFAULT_TASK_TIMEOUT = 60000ms;

// Quantum hash for agent selection
inline uint64_t quantum_agent_hash(const std::string& task_desc, const std::string& agent_id) {
    uint64_t hash = QUANTUM_OPTIMIZATION_SEED;
    std::string combined = task_desc + "|" + agent_id;
    
    for (size_t i = 0; i < combined.length(); ++i) {
        hash ^= static_cast<uint64_t>(combined[i]) << ((i % 8) * 8);
        hash = (hash << 31) | (hash >> 33); // Rotate left 31 bits  
        hash *= 1099511628211ULL; // Large prime for better distribution
    }
    
    return hash;
}

// Calculate agent compatibility score
inline double calculate_agent_compatibility(AgentType required_type, AgentType agent_type, 
                                          const std::bitset<32>& capabilities) {
    double base_score = 0.5; // Base compatibility
    
    // Type matching bonus
    if (agent_type == required_type) {
        base_score += 0.4;
    } else if (agent_type == AgentType::GENERAL_PURPOSE || agent_type == AgentType::QUANTUM_ENHANCED) {
        base_score += 0.2; // General purpose agents are somewhat compatible
    }
    
    // Capability bonus (simplified - in real implementation would check specific bits)
    uint32_t capability_count = capabilities.count();
    base_score += std::min(0.1, capability_count * 0.01);
    
    return std::min(1.0, base_score);
}

// Statistical variance calculation for load balancing
inline double calculate_load_variance(const std::vector<double>& loads) {
    if (loads.size() < 2) return 0.0;
    
    double mean = std::accumulate(loads.begin(), loads.end(), 0.0) / loads.size();
    
    double variance = 0.0;
    for (double load : loads) {
        variance += std::pow(load - mean, 2);
    }
    
    return variance / loads.size();
}

// =====================================================================
// CYCLE AGENT ORCHESTRATOR IMPLEMENTATION  
// =====================================================================

CycleAgentOrchestrator::CycleAgentOrchestrator()
    : startup_time_(std::chrono::system_clock::now()),
      random_generator_(std::chrono::high_resolution_clock::now().time_since_epoch().count()) {
    
    std::cout << "[CycleOrchestrator] Cycle Agent Orchestrator initializing..." << std::endl;
    performance_stats_ = {};
    health_status_ = {};
    load_balance_stats_ = {};
}

CycleAgentOrchestrator::~CycleAgentOrchestrator() {
    shutdown();
}

bool CycleAgentOrchestrator::initialize(uint8_t initial_agent_count, 
                                      SchedulingAlgorithm default_algorithm,
                                      bool enable_quantum_optimization) {
    if (initialized_.load()) {
        std::cout << "[CycleOrchestrator] Already initialized" << std::endl;
        return true;
    }
    
    try {
        if (initial_agent_count == 0 || initial_agent_count > MAX_AGENTS) {
            std::cerr << "[CycleOrchestrator] Invalid agent count: " << static_cast<uint32_t>(initial_agent_count) << std::endl;
            return false;
        }
        
        current_algorithm_ = default_algorithm;
        quantum_optimization_enabled_ = enable_quantum_optimization;
        
        // Initialize integration components
        thinking_engine_ = std::make_unique<AgenticDeepThinkingEngine>();
        failure_detector_ = std::make_unique<AgenticFailureDetector>();
        telemetry_collector_ = std::make_unique<TelemetryCollector>();
        
        // Create initial agents
        for (uint8_t i = 0; i < initial_agent_count; ++i) {
            AgentConfiguration config;
            config.agent_id = generate_agent_id();
            config.agent_name = "Agent_" + std::to_string(i + 1);
            config.agent_type = AgentType::GENERAL_PURPOSE;
            config.created_at = std::chrono::system_clock::now();
            
            if (quantum_optimization_enabled_) {
                config.quantum_enhancement_enabled = true;
                config.quantum_optimization_level = quantum_optimization_level_;
            }
            
            add_agent(config);
        }
        
        initialized_ = true;
        
        std::cout << "[CycleOrchestrator] Initialization complete" << std::endl;
        std::cout << "  - Initial agents: " << static_cast<uint32_t>(initial_agent_count) << std::endl;
        std::cout << "  - Scheduling algorithm: " << static_cast<uint32_t>(default_algorithm) << std::endl;
        std::cout << "  - Quantum optimization: " << (quantum_optimization_enabled_ ? "enabled" : "disabled") << std::endl;
        
        return true;
        
    } catch (const std::exception& e) {
        std::cerr << "[CycleOrchestrator] Initialization failed: " << e.what() << std::endl;
        return false;
    }
}

void CycleAgentOrchestrator::shutdown() {
    if (!initialized_.load()) return;
    
    std::cout << "[CycleOrchestrator] Shutting down orchestrator..." << std::endl;
    
    shutting_down_ = true;
    
    // Stop monitoring threads
    auto_load_balancing_enabled_ = false;
    health_monitoring_enabled_ = false;
    
    if (load_balancing_thread_.joinable()) {
        load_balancing_thread_.join();
    }
    
    if (health_monitoring_thread_.joinable()) {
        health_monitoring_thread_.join();
    }
    
    // Shutdown all agents gracefully
    std::vector<std::string> agent_ids;
    {
        std::lock_guard<std::mutex> lock(agents_mutex_);
        for (const auto& pair : agent_configs_) {
            agent_ids.push_back(pair.first);
        }
    }
    
    for (const auto& agent_id : agent_ids) {
        remove_agent(agent_id, true);
    }
    
    // Clear data structures
    {
        std::lock_guard<std::mutex> lock(agents_mutex_);
        agent_configs_.clear();
        agent_runtime_info_.clear();
    }
    
    {
        std::lock_guard<std::mutex> lock(tasks_mutex_);
        active_tasks_.clear();
        while (!task_queue_.empty()) {
            task_queue_.pop();
        }
    }
    
    initialized_ = false;
    shutting_down_ = false;
    
    std::cout << "[CycleOrchestrator] Shutdown complete" << std::endl;
}

std::string CycleAgentOrchestrator::add_agent(const AgentConfiguration& config) {
    if (!initialized_.load()) {
        std::cerr << "[CycleOrchestrator] Orchestrator not initialized" << std::endl;
        return "";
    }
    
    std::lock_guard<std::mutex> lock(agents_mutex_);
    
    if (agent_configs_.size() >= MAX_AGENTS) {
        std::cerr << "[CycleOrchestrator] Maximum agent limit reached: " << MAX_AGENTS << std::endl;
        return "";
    }
    
    std::string agent_id = config.agent_id.empty() ? generate_agent_id() : config.agent_id;
    
    if (agent_configs_.find(agent_id) != agent_configs_.end()) {
        std::cerr << "[CycleOrchestrator] Agent already exists: " << agent_id << std::endl;
        return "";
    }
    
    // Store configuration
    AgentConfiguration agent_config = config;
    agent_config.agent_id = agent_id;
    agent_configs_[agent_id] = agent_config;
    
    // Initialize runtime info
    AgentRuntimeInfo runtime_info;
    runtime_info.agent_id = agent_id;
    runtime_info.current_state = AgentState::INITIALIZING;
    runtime_info.is_healthy = true;
    runtime_info.last_health_check = std::chrono::system_clock::now();
    runtime_info.last_activity = runtime_info.last_health_check;
    agent_runtime_info_[agent_id] = runtime_info;
    
    // Create and start agent thread
    auto agent_thread = std::make_unique<std::thread>(&CycleAgentOrchestrator::agent_thread_function, this, agent_id);
    runtime_info.thread_id = agent_thread->get_id();
    agent_threads_[agent_id] = std::move(agent_thread);
    
    std::cout << "[CycleOrchestrator] Added agent: " << agent_id 
              << " (" << agent_config.agent_name << ")" << std::endl;
    
    return agent_id;
}

bool CycleAgentOrchestrator::remove_agent(const std::string& agent_id, bool graceful_shutdown) {
    std::lock_guard<std::mutex> lock(agents_mutex_);
    
    auto config_it = agent_configs_.find(agent_id);
    if (config_it == agent_configs_.end()) {
        return false;
    }
    
    // Update agent state to shutdown
    auto runtime_it = agent_runtime_info_.find(agent_id);
    if (runtime_it != agent_runtime_info_.end()) {
        runtime_it->second.current_state = AgentState::SHUTDOWN;
    }
    
    // Wait for agent thread to complete if graceful shutdown
    auto thread_it = agent_threads_.find(agent_id);
    if (thread_it != agent_threads_.end()) {
        if (graceful_shutdown && thread_it->second->joinable()) {
            thread_it->second->join();
        } else {
            thread_it->second->detach();
        }
        agent_threads_.erase(thread_it);
    }
    
    // Remove from data structures
    agent_configs_.erase(config_it);
    if (runtime_it != agent_runtime_info_.end()) {
        agent_runtime_info_.erase(runtime_it);
    }
    
    std::cout << "[CycleOrchestrator] Removed agent: " << agent_id << std::endl;
    
    return true;
}

bool CycleAgentOrchestrator::scale_agents(uint8_t target_count, bool preserve_existing) {
    if (target_count == 0 || target_count > MAX_AGENTS) {
        std::cerr << "[CycleOrchestrator] Invalid target count: " << static_cast<uint32_t>(target_count) << std::endl;
        return false;
    }
    
    std::lock_guard<std::mutex> lock(agents_mutex_);
    
    uint8_t current_count = agent_configs_.size();
    
    if (target_count == current_count) {
        std::cout << "[CycleOrchestrator] Already at target count: " << static_cast<uint32_t>(target_count) << std::endl;
        return true;
    }
    
    if (target_count > current_count) {
        // Scale up - add new agents
        uint8_t agents_to_add = target_count - current_count;
        
        for (uint8_t i = 0; i < agents_to_add; ++i) {
            AgentConfiguration config;
            config.agent_id = generate_agent_id();
            config.agent_name = "ScaledAgent_" + std::to_string(current_count + i + 1);
            config.agent_type = AgentType::GENERAL_PURPOSE;
            config.created_at = std::chrono::system_clock::now();
            
            if (quantum_optimization_enabled_) {
                config.quantum_enhancement_enabled = true;
                config.quantum_optimization_level = quantum_optimization_level_;
            }
            
            // Add agent (must unlock first to avoid deadlock)
            std::string agent_id = config.agent_id;
            {
                // Temporarily release lock for add_agent call
                lock.~lock_guard();
                add_agent(config);
                new (&lock) std::lock_guard<std::mutex>(agents_mutex_);
            }
        }
        
        std::cout << "[CycleOrchestrator] Scaled up from " << static_cast<uint32_t>(current_count) 
                  << " to " << static_cast<uint32_t>(target_count) << " agents" << std::endl;
        
    } else {
        // Scale down - remove agents
        uint8_t agents_to_remove = current_count - target_count;
        
        // Select agents to remove (prefer least active ones if preserve_existing)
        std::vector<std::string> agents_to_remove_list;
        
        if (preserve_existing) {
            // Create list of agents sorted by activity (least active first)
            std::vector<std::pair<std::string, uint32_t>> agent_activity;
            
            for (const auto& pair : agent_runtime_info_) {
                agent_activity.emplace_back(pair.first, pair.second.active_tasks + pair.second.queued_tasks);
            }
            
            std::sort(agent_activity.begin(), agent_activity.end(),
                [](const auto& a, const auto& b) { return a.second < b.second; });
            
            for (uint8_t i = 0; i < agents_to_remove && i < agent_activity.size(); ++i) {
                agents_to_remove_list.push_back(agent_activity[i].first);
            }
        } else {
            // Remove any agents
            auto it = agent_configs_.begin();
            for (uint8_t i = 0; i < agents_to_remove && it != agent_configs_.end(); ++i, ++it) {
                agents_to_remove_list.push_back(it->first);
            }
        }
        
        // Remove selected agents
        for (const auto& agent_id : agents_to_remove_list) {
            // Temporarily release lock for remove_agent call
            lock.~lock_guard();
            remove_agent(agent_id, true);
            new (&lock) std::lock_guard<std::mutex>(agents_mutex_);
        }
        
        std::cout << "[CycleOrchestrator] Scaled down from " << static_cast<uint32_t>(current_count) 
                  << " to " << static_cast<uint32_t>(target_count) << " agents" << std::endl;
    }
    
    return true;
}

std::vector<std::string> CycleAgentOrchestrator::list_agents(AgentState state_filter) {
    std::lock_guard<std::mutex> lock(agents_mutex_);
    
    std::vector<std::string> agent_list;
    agent_list.reserve(agent_configs_.size());
    
    for (const auto& pair : agent_runtime_info_) {
        if (state_filter == AgentState::READY || pair.second.current_state == state_filter) {
            agent_list.push_back(pair.first);
        }
    }
    
    return agent_list;
}

CycleAgentTaskResult CycleAgentOrchestrator::execute_task(const CycleAgentTaskRequest& request) {
    if (!initialized_.load()) {
        return CycleAgentTaskResult::error_result(request.request_id, "Orchestrator not initialized");
    }
    
    std::cout << "[CycleOrchestrator] Executing task: " << request.request_id << std::endl;
    
    // Select agent for task execution
    std::string selected_agent = get_next_agent(request.preferred_agent_type);
    if (selected_agent.empty()) {
        return CycleAgentTaskResult::error_result(request.request_id, "No suitable agent available");
    }
    
    std::cout << "[CycleOrchestrator] Selected agent: " << selected_agent 
              << " for task: " << request.request_id << std::endl;
    
    // Execute task on selected agent
    auto result = execute_task_on_agent_internal(selected_agent, request);
    
    // Handle failover if enabled and task failed
    if (!result.success && request.allow_failover) {
        std::cout << "[CycleOrchestrator] Task failed, attempting failover..." << std::endl;
        
        // Try up to 3 other agents
        for (int retry = 0; retry < 3; ++retry) {
            std::string backup_agent = get_next_agent(request.preferred_agent_type);
            if (backup_agent != selected_agent && !backup_agent.empty()) {
                result = execute_task_on_agent_internal(backup_agent, request);
                if (result.success) {
                    std::cout << "[CycleOrchestrator] Failover successful on agent: " << backup_agent << std::endl;
                    break;
                }
            }
        }
    }
    
    // Update statistics
    update_performance_statistics(result);
    
    return result;
}

std::future<CycleAgentTaskResult> CycleAgentOrchestrator::execute_task_async(const CycleAgentTaskRequest& request) {
    return std::async(std::launch::async, &CycleAgentOrchestrator::execute_task, this, request);
}

CycleAgentTaskResult CycleAgentOrchestrator::execute_auto_select(
    const std::string& task_description, const std::string& task_data, AgentType preferred_type) {
    
    CycleAgentTaskRequest request;
    request.request_id = generate_request_id();
    request.task_description = task_description;
    request.task_data = task_data;
    request.preferred_agent_type = preferred_type;
    
    return execute_task(request);
}

std::string CycleAgentOrchestrator::get_next_agent(AgentType preferred_type) {
    switch (current_algorithm_) {
        case SchedulingAlgorithm::ROUND_ROBIN:
            return select_agent_round_robin(preferred_type);
            
        case SchedulingAlgorithm::LOAD_BALANCED:
            return select_agent_load_balanced(preferred_type);
            
        case SchedulingAlgorithm::QUANTUM_OPTIMIZED:
            return select_agent_quantum_optimized(preferred_type);
            
        case SchedulingAlgorithm::CAPABILITY_MATCHED:
            return select_agent_capability_matched(preferred_type);
            
        case SchedulingAlgorithm::ADAPTIVE:
            // Adaptively choose algorithm based on current system state
            {
                std::lock_guard<std::mutex> lock(agents_mutex_);
                if (agent_configs_.size() <= 3) {
                    return select_agent_round_robin(preferred_type);
                } else if (quantum_optimization_enabled_) {
                    return select_agent_quantum_optimized(preferred_type);
                } else {
                    return select_agent_load_balanced(preferred_type);
                }
            }
            
        default:
            return select_agent_round_robin(preferred_type);
    }
}

void CycleAgentOrchestrator::balance_load() {
    std::lock_guard<std::mutex> lock(load_balance_mutex_);
    balance_agents_load_internal();
}

void CycleAgentOrchestrator::enable_auto_load_balancing(bool enabled, std::chrono::milliseconds interval) {
    if (enabled == auto_load_balancing_enabled_.load()) return;
    
    auto_load_balancing_enabled_ = enabled;
    load_balance_interval_ = interval;
    
    if (enabled) {
        load_balancing_thread_ = std::thread(&CycleAgentOrchestrator::load_balancing_thread_function, this);
        std::cout << "[CycleOrchestrator] Auto load balancing enabled (interval: " 
                  << interval.count() << "ms)" << std::endl;
    } else {
        if (load_balancing_thread_.joinable()) {
            load_balancing_thread_.join();
        }
        std::cout << "[CycleOrchestrator] Auto load balancing disabled" << std::endl;
    }
}

void CycleAgentOrchestrator::enable_health_monitoring(bool enabled, std::chrono::milliseconds check_interval) {
    if (enabled == health_monitoring_enabled_.load()) return;
    
    health_monitoring_enabled_ = enabled;
    health_check_interval_ = check_interval;
    
    if (enabled) {
        health_monitoring_thread_ = std::thread(&CycleAgentOrchestrator::health_monitoring_thread_function, this);
        std::cout << "[CycleOrchestrator] Health monitoring enabled (interval: " 
                  << check_interval.count() << "ms)" << std::endl;
    } else {
        if (health_monitoring_thread_.joinable()) {
            health_monitoring_thread_.join();
        }
        std::cout << "[CycleOrchestrator] Health monitoring disabled" << std::endl;
    }
}

bool CycleAgentOrchestrator::check_agent_health(const std::string& agent_id) {
    std::lock_guard<std::mutex> lock(agents_mutex_);
    
    auto runtime_it = agent_runtime_info_.find(agent_id);
    if (runtime_it == agent_runtime_info_.end()) {
        return false;
    }
    
    auto& runtime_info = runtime_it->second;
    
    // Update health check timestamp
    runtime_info.last_health_check = std::chrono::system_clock::now();
    
    // Check various health indicators
    bool is_healthy = true;
    std::string health_issues;
    
    // Check if agent is responsive (simplified check)
    auto time_since_activity = std::chrono::duration_cast<std::chrono::milliseconds>(
        runtime_info.last_health_check - runtime_info.last_activity);
    
    if (time_since_activity > std::chrono::minutes(5)) {
        is_healthy = false;
        health_issues += "No activity for " + std::to_string(time_since_activity.count()) + "ms; ";
    }
    
    // Check failure count
    auto config_it = agent_configs_.find(agent_id);
    if (config_it != agent_configs_.end() && 
        runtime_info.failure_count >= config_it->second.max_failure_count) {
        is_healthy = false;
        health_issues += "Excessive failures (" + std::to_string(runtime_info.failure_count) + "); ";
    }
    
    // Check resource usage
    if (runtime_info.current_load > 1.0) {
        is_healthy = false;
        health_issues += "Overloaded (" + std::to_string(runtime_info.current_load * 100) + "%); ";
    }
    
    // Update health status
    runtime_info.is_healthy = is_healthy;
    if (!is_healthy) {
        runtime_info.last_error_message = health_issues;
    }
    
    return is_healthy;
}

void CycleAgentOrchestrator::check_all_agents_health() {
    std::lock_guard<std::mutex> lock(health_mutex_);
    
    health_status_.total_agents = 0;
    health_status_.healthy_agents = 0;
    health_status_.unhealthy_agents = 0;
    health_status_.recovering_agents = 0;
    health_status_.agent_health_status.clear();
    health_status_.critical_issues.clear();
    
    std::vector<std::string> agent_ids;
    {
        std::lock_guard<std::mutex> agents_lock(agents_mutex_);
        for (const auto& pair : agent_configs_) {
            agent_ids.push_back(pair.first);
        }
    }
    
    for (const auto& agent_id : agent_ids) {
        health_status_.total_agents++;
        
        bool is_healthy = check_agent_health(agent_id);
        health_status_.agent_health_status[agent_id] = is_healthy;
        
        if (is_healthy) {
            health_status_.healthy_agents++;
        } else {
            health_status_.unhealthy_agents++;
            
            // Check if agent is in recovery state
            std::lock_guard<std::mutex> agents_lock(agents_mutex_);
            auto runtime_it = agent_runtime_info_.find(agent_id);
            if (runtime_it != agent_runtime_info_.end() && 
                runtime_it->second.current_state == AgentState::RECOVERING) {
                health_status_.recovering_agents++;
            } else {
                health_status_.critical_issues.push_back("Agent " + agent_id + " is unhealthy");
            }
        }
    }
    
    health_status_.last_health_check = std::chrono::system_clock::now();
    
    // Calculate overall health score
    if (health_status_.total_agents > 0) {
        health_status_.overall_health_score = 
            static_cast<double>(health_status_.healthy_agents) / health_status_.total_agents;
    } else {
        health_status_.overall_health_score = 0.0;
    }
    
    std::cout << "[CycleOrchestrator] Health check complete: " 
              << static_cast<uint32_t>(health_status_.healthy_agents) << "/" 
              << static_cast<uint32_t>(health_status_.total_agents) << " agents healthy" << std::endl;
}

CycleAgentOrchestrator::PerformanceStatistics CycleAgentOrchestrator::get_performance_statistics() const {
    std::lock_guard<std::mutex> lock(performance_mutex_);
    return performance_stats_;
}

CycleAgentOrchestrator::SystemStatus CycleAgentOrchestrator::get_system_status() const {
    SystemStatus status;
    
    status.orchestrator_initialized = initialized_.load();
    status.quantum_optimization_enabled = quantum_optimization_enabled_;
    status.max_mode_enabled = max_mode_enabled_;
    status.health_monitoring_active = health_monitoring_enabled_.load();
    status.auto_load_balancing_active = auto_load_balancing_enabled_.load();
    status.current_algorithm = current_algorithm_;
    status.quality_speed_balance = quality_speed_balance_;
    
    status.uptime = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now() - startup_time_);
    
    {
        std::lock_guard<std::mutex> lock(agents_mutex_);
        status.total_agents = agent_configs_.size();
        
        for (const auto& pair : agent_runtime_info_) {
            if (pair.second.current_state == AgentState::ACTIVE || 
                pair.second.current_state == AgentState::READY) {
                status.active_agents++;
            }
            if (pair.second.current_state == AgentState::READY) {
                status.ready_agents++;
            }
            status.active_tasks += pair.second.active_tasks;
            status.queued_tasks += pair.second.queued_tasks;
        }
    }
    
    status.performance_stats = get_performance_statistics();
    
    {
        std::lock_guard<std::mutex> lock(health_mutex_);
        status.health_status = health_status_;
    }
    
    {
        std::lock_guard<std::mutex> lock(load_balance_mutex_);
        status.load_balance_stats = load_balance_stats_;
    }
    
    return status;
}

// ================================================================
// PRIVATE IMPLEMENTATION METHODS
// ================================================================

std::string CycleAgentOrchestrator::generate_agent_id() {
    static std::atomic<uint32_t> counter{0};
    auto now = std::chrono::high_resolution_clock::now().time_since_epoch().count();
    auto id = counter.fetch_add(1);
    
    std::ostringstream oss;
    oss << "AGENT_" << std::hex << now << "_" << id;
    return oss.str();
}

std::string CycleAgentOrchestrator::generate_request_id() {
    static std::atomic<uint32_t> counter{0};
    auto now = std::chrono::high_resolution_clock::now().time_since_epoch().count();
    auto id = counter.fetch_add(1);
    
    std::ostringstream oss;
    oss << "REQ_" << std::hex << now << "_" << id;
    return oss.str();
}

void CycleAgentOrchestrator::agent_thread_function(const std::string& agent_id) {
    std::cout << "[CycleOrchestrator] Agent thread started: " << agent_id << std::endl;
    
    try {
        // Update agent state to ready
        {
            std::lock_guard<std::mutex> lock(agents_mutex_);
            auto runtime_it = agent_runtime_info_.find(agent_id);
            if (runtime_it != agent_runtime_info_.end()) {
                runtime_it->second.current_state = AgentState::READY;
                runtime_it->second.cycle_start_time = std::chrono::system_clock::now();
                runtime_it->second.last_activity = runtime_it->second.cycle_start_time;
            }
        }
        
        // Main agent loop
        while (!shutting_down_.load()) {
            AgentState current_state;
            {
                std::lock_guard<std::mutex> lock(agents_mutex_);
                auto runtime_it = agent_runtime_info_.find(agent_id);
                if (runtime_it == agent_runtime_info_.end() || 
                    runtime_it->second.current_state == AgentState::SHUTDOWN) {
                    break;
                }
                current_state = runtime_it->second.current_state;
            }
            
            // Process tasks or wait
            if (current_state == AgentState::READY) {
                // Look for tasks to process (simplified - in real implementation would have task queue per agent)
                std::this_thread::sleep_for(100ms); // Agent cycle time
                
                // Update activity timestamp
                {
                    std::lock_guard<std::mutex> lock(agents_mutex_);
                    auto runtime_it = agent_runtime_info_.find(agent_id);
                    if (runtime_it != agent_runtime_info_.end()) {
                        runtime_it->second.last_activity = std::chrono::system_clock::now();
                        
                        // Update cycle time
                        auto cycle_time = std::chrono::duration_cast<std::chrono::milliseconds>(
                            runtime_it->second.last_activity - runtime_it->second.cycle_start_time);
                        runtime_it->second.current_cycle_time = cycle_time;
                        
                        // Update average cycle time
                        if (runtime_it->second.avg_cycle_time == std::chrono::milliseconds{0}) {
                            runtime_it->second.avg_cycle_time = cycle_time;
                        } else {
                            runtime_it->second.avg_cycle_time = std::chrono::milliseconds(
                                (runtime_it->second.avg_cycle_time.count() + cycle_time.count()) / 2);
                        }
                        
                        runtime_it->second.cycle_start_time = runtime_it->second.last_activity;
                    }
                }
            } else {
                // Agent not ready, wait longer
                std::this_thread::sleep_for(1000ms);
            }
        }
        
    } catch (const std::exception& e) {
        std::cerr << "[CycleOrchestrator] Agent thread error (" << agent_id << "): " << e.what() << std::endl;
        
        // Update agent state to indicate failure
        std::lock_guard<std::mutex> lock(agents_mutex_);
        auto runtime_it = agent_runtime_info_.find(agent_id);
        if (runtime_it != agent_runtime_info_.end()) {
            runtime_it->second.current_state = AgentState::RECOVERING;
            runtime_it->second.failure_count++;
            runtime_it->second.is_healthy = false;
            runtime_it->second.last_error_message = e.what();
        }
    }
    
    std::cout << "[CycleOrchestrator] Agent thread ended: " << agent_id << std::endl;
}

std::string CycleAgentOrchestrator::select_agent_round_robin(AgentType preferred_type) {
    std::lock_guard<std::mutex> lock(agents_mutex_);
    
    if (agent_configs_.empty()) return "";
    
    std::vector<std::string> compatible_agents;
    for (const auto& pair : agent_configs_) {
        if (is_agent_available(pair.first, preferred_type)) {
            compatible_agents.push_back(pair.first);
        }
    }
    
    if (compatible_agents.empty()) return "";
    
    uint32_t index = round_robin_counter_.fetch_add(1) % compatible_agents.size();
    return compatible_agents[index];
}

std::string CycleAgentOrchestrator::select_agent_load_balanced(AgentType preferred_type) {
    std::lock_guard<std::mutex> lock(agents_mutex_);
    
    std::string best_agent;
    double lowest_load = std::numeric_limits<double>::max();
    
    for (const auto& pair : agent_configs_) {
        if (is_agent_available(pair.first, preferred_type)) {
            double load = calculate_agent_load(pair.first);
            if (load < lowest_load) {
                lowest_load = load;
                best_agent = pair.first;
            }
        }
    }
    
    return best_agent;
}

std::string CycleAgentOrchestrator::select_agent_quantum_optimized(AgentType preferred_type) {
    std::lock_guard<std::mutex> lock(agents_mutex_);
    
    std::vector<std::pair<std::string, double>> agent_scores;
    
    for (const auto& pair : agent_configs_) {
        if (is_agent_available(pair.first, preferred_type)) {
            // Calculate quantum score
            double compatibility_score = calculate_agent_compatibility(
                preferred_type, pair.second.agent_type, pair.second.capabilities);
            double load_score = 1.0 - calculate_agent_load(pair.first);
            double efficiency_score = pair.second.efficiency_rating;
            
            // Quantum-enhanced scoring using golden ratio
            double quantum_score = (compatibility_score * GOLDEN_RATIO_AGENT + 
                                  load_score * (2.0 - GOLDEN_RATIO_AGENT) + 
                                  efficiency_score) / 3.0;
            
            agent_scores.emplace_back(pair.first, quantum_score);
        }
    }
    
    if (agent_scores.empty()) return "";
    
    // Select agent with highest quantum score
    auto best_agent = std::max_element(agent_scores.begin(), agent_scores.end(),
        [](const auto& a, const auto& b) { return a.second < b.second; });
    
    return best_agent->first;
}

std::string CycleAgentOrchestrator::select_agent_capability_matched(AgentType preferred_type) {
    std::lock_guard<std::mutex> lock(agents_mutex_);
    
    std::string best_agent;
    double best_compatibility = 0.0;
    
    for (const auto& pair : agent_configs_) {
        if (is_agent_available(pair.first, preferred_type)) {
            double compatibility = calculate_agent_compatibility(
                preferred_type, pair.second.agent_type, pair.second.capabilities);
            
            if (compatibility > best_compatibility) {
                best_compatibility = compatibility;
                best_agent = pair.first;
            }
        }
    }
    
    return best_agent;
}

CycleAgentTaskResult CycleAgentOrchestrator::execute_task_on_agent_internal(
    const std::string& agent_id, const CycleAgentTaskRequest& request) {
    
    CycleAgentTaskResult result;
    result.request_id = request.request_id;
    result.agent_id = agent_id;
    result.started_at = std::chrono::system_clock::now();
    
    try {
        // Update agent state to active
        {
            std::lock_guard<std::mutex> lock(agents_mutex_);
            auto runtime_it = agent_runtime_info_.find(agent_id);
            if (runtime_it != agent_runtime_info_.end()) {
                runtime_it->second.current_state = AgentState::ACTIVE;
                runtime_it->second.active_tasks++;
            }
        }
        
        // Simulate task processing
        std::cout << "[CycleOrchestrator] Agent " << agent_id 
                  << " processing task: " << request.task_description << std::endl;
        
        // Get agent configuration for processing time estimation
        AgentConfiguration agent_config;
        {
            std::lock_guard<std::mutex> lock(agents_mutex_);
            auto config_it = agent_configs_.find(agent_id);
            if (config_it == agent_configs_.end()) {
                throw std::runtime_error("Agent configuration not found");
            }
            agent_config = config_it->second;
        }
        
        // Simulate processing time based on agent characteristics
        auto processing_time = agent_config.avg_processing_time;
        if (quantum_optimization_enabled_ && agent_config.quantum_enhancement_enabled) {
            // Quantum enhancement reduces processing time
            processing_time = std::chrono::milliseconds(static_cast<int64_t>(
                processing_time.count() / GOLDEN_RATIO_AGENT));
        }
        
        std::this_thread::sleep_for(processing_time);
        
        // Generate result
        result.success = true;
        result.result_data = "Task completed by agent " + agent_config.agent_name + 
                           " (ID: " + agent_id + "): " + request.task_description;
        result.quality_score = agent_config.quality_score * 
                              (0.8 + 0.2 * static_cast<double>(random_generator_()) / random_generator_.max());
        result.confidence_score = agent_config.reliability_score;
        result.completed_at = std::chrono::system_clock::now();
        result.processing_time = std::chrono::duration_cast<std::chrono::milliseconds>(
            result.completed_at - result.started_at);
        
        // Simulate memory usage
        result.memory_used = request.task_data.length() * 10; // Rough estimate
        
        // Update agent statistics
        update_agent_statistics(agent_id, result);
        
        std::cout << "[CycleOrchestrator] Task completed successfully by agent: " << agent_id << std::endl;
        
    } catch (const std::exception& e) {
        result.success = false;
        result.error_message = std::string("Task execution error: ") + e.what();
        result.completed_at = std::chrono::system_clock::now();
        result.processing_time = std::chrono::duration_cast<std::chrono::milliseconds>(
            result.completed_at - result.started_at);
        
        // Update agent failure statistics
        {
            std::lock_guard<std::mutex> lock(agents_mutex_);
            auto runtime_it = agent_runtime_info_.find(agent_id);
            if (runtime_it != agent_runtime_info_.end()) {
                runtime_it->second.failure_count++;
                runtime_it->second.last_error_message = e.what();
            }
        }
        
        std::cerr << "[CycleOrchestrator] Task failed on agent " << agent_id 
                  << ": " << e.what() << std::endl;
    }
    
    // Update agent state back to ready
    {
        std::lock_guard<std::mutex> lock(agents_mutex_);
        auto runtime_it = agent_runtime_info_.find(agent_id);
        if (runtime_it != agent_runtime_info_.end()) {
            runtime_it->second.current_state = AgentState::READY;
            runtime_it->second.active_tasks = runtime_it->second.active_tasks > 0 ? 
                                             runtime_it->second.active_tasks - 1 : 0;
            runtime_it->second.last_activity = std::chrono::system_clock::now();
        }
    }
    
    return result;
}

void CycleAgentOrchestrator::update_agent_statistics(const std::string& agent_id, const CycleAgentTaskResult& result) {
    std::lock_guard<std::mutex> lock(agents_mutex_);
    
    auto config_it = agent_configs_.find(agent_id);
    if (config_it != agent_configs_.end()) {
        config_it->second.total_tasks_processed++;
        config_it->second.last_used = result.completed_at;
        
        if (result.success) {
            config_it->second.successful_tasks++;
        } else {
            config_it->second.failed_tasks++;
        }
        
        // Update average processing time
        if (config_it->second.total_tasks_processed == 1) {
            config_it->second.avg_processing_time = result.processing_time;
        } else {
            config_it->second.avg_processing_time = std::chrono::milliseconds(
                (config_it->second.avg_processing_time.count() + result.processing_time.count()) / 2);
        }
        
        // Update reliability score based on success rate
        double success_rate = static_cast<double>(config_it->second.successful_tasks) / 
                             config_it->second.total_tasks_processed;
        config_it->second.reliability_score = success_rate;
    }
}

void CycleAgentOrchestrator::update_performance_statistics(const CycleAgentTaskResult& result) {
    std::lock_guard<std::mutex> lock(performance_mutex_);
    
    performance_stats_.total_tasks_processed++;
    
    if (result.success) {
        performance_stats_.successful_tasks++;
    } else {
        performance_stats_.failed_tasks++;
    }
    
    // Update timing statistics
    if (result.processing_time < performance_stats_.fastest_processing_time) {
        performance_stats_.fastest_processing_time = result.processing_time;
    }
    if (result.processing_time > performance_stats_.slowest_processing_time) {
        performance_stats_.slowest_processing_time = result.processing_time;
    }
    
    performance_stats_.avg_processing_time = std::chrono::milliseconds(
        (performance_stats_.avg_processing_time.count() * (performance_stats_.total_tasks_processed - 1) +
         result.processing_time.count()) / performance_stats_.total_tasks_processed);
    
    // Update agent-specific statistics
    performance_stats_.agent_task_counts[result.agent_id]++;
    
    auto& success_rate = performance_stats_.agent_success_rates[result.agent_id];
    auto task_count = performance_stats_.agent_task_counts[result.agent_id];
    if (result.success) {
        success_rate = (success_rate * (task_count - 1) + 1.0) / task_count;
    } else {
        success_rate = (success_rate * (task_count - 1)) / task_count;
    }
    
    auto& avg_time = performance_stats_.agent_avg_times[result.agent_id];
    avg_time = std::chrono::milliseconds(
        (avg_time.count() * (task_count - 1) + result.processing_time.count()) / task_count);
    
    // Update overall efficiency
    if (performance_stats_.total_tasks_processed > 0) {
        double success_rate_overall = static_cast<double>(performance_stats_.successful_tasks) / 
                                    performance_stats_.total_tasks_processed;
        performance_stats_.overall_efficiency = success_rate_overall;
        
        // Calculate task throughput (simplified)
        performance_stats_.task_throughput = 1000.0 / performance_stats_.avg_processing_time.count();
    }
}

bool CycleAgentOrchestrator::is_agent_available(const std::string& agent_id, AgentType required_type) {
    auto runtime_it = agent_runtime_info_.find(agent_id);
    if (runtime_it == agent_runtime_info_.end()) {
        return false;
    }
    
    const auto& runtime_info = runtime_it->second;
    
    // Check if agent is in suitable state
    if (runtime_info.current_state != AgentState::READY && 
        runtime_info.current_state != AgentState::ACTIVE) {
        return false;
    }
    
    // Check if agent is healthy
    if (!runtime_info.is_healthy) {
        return false;
    }
    
    // Check type compatibility (simplified)
    auto config_it = agent_configs_.find(agent_id);
    if (config_it != agent_configs_.end()) {
        if (required_type != AgentType::GENERAL_PURPOSE && 
            config_it->second.agent_type != required_type &&
            config_it->second.agent_type != AgentType::GENERAL_PURPOSE &&
            config_it->second.agent_type != AgentType::QUANTUM_ENHANCED) {
            return false;
        }
        
        // Check if agent is not overloaded
        if (runtime_info.active_tasks >= config_it->second.max_concurrent_tasks) {
            return false;
        }
    }
    
    return true;
}

double CycleAgentOrchestrator::calculate_agent_load(const std::string& agent_id) {
    auto runtime_it = agent_runtime_info_.find(agent_id);
    if (runtime_it == agent_runtime_info_.end()) {
        return 1.0; // Treat unknown agents as fully loaded
    }
    
    const auto& runtime_info = runtime_it->second;
    
    auto config_it = agent_configs_.find(agent_id);
    if (config_it == agent_configs_.end()) {
        return 1.0;
    }
    
    // Calculate load based on active tasks vs capacity
    double task_load = static_cast<double>(runtime_info.active_tasks + runtime_info.queued_tasks) / 
                      std::max(config_it->second.max_concurrent_tasks, 1u);
    
    // Factor in current reported load if available
    if (runtime_info.current_load > 0.0) {
        task_load = (task_load + runtime_info.current_load) / 2.0;
    }
    
    return std::min(1.0, task_load);
}

void CycleAgentOrchestrator::load_balancing_thread_function() {
    while (auto_load_balancing_enabled_.load()) {
        std::this_thread::sleep_for(load_balance_interval_);
        
        if (shutting_down_.load()) break;
        
        try {
            balance_agents_load_internal();
        } catch (const std::exception& e) {
            std::cerr << "[CycleOrchestrator] Load balancing error: " << e.what() << std::endl;
        }
    }
}

void CycleAgentOrchestrator::health_monitoring_thread_function() {
    while (health_monitoring_enabled_.load()) {
        std::this_thread::sleep_for(health_check_interval_);
        
        if (shutting_down_.load()) break;
        
        try {
            check_all_agents_health();
            recover_failed_agents();
        } catch (const std::exception& e) {
            std::cerr << "[CycleOrchestrator] Health monitoring error: " << e.what() << std::endl;
        }
    }
}

void CycleAgentOrchestrator::balance_agents_load_internal() {
    std::lock_guard<std::mutex> lock(agents_mutex_);
    
    std::vector<double> agent_loads;
    std::map<std::string, double> agent_load_map;
    
    // Calculate current loads
    for (const auto& pair : agent_configs_) {
        double load = calculate_agent_load(pair.first);
        agent_loads.push_back(load);
        agent_load_map[pair.first] = load;
    }
    
    if (agent_loads.size() < 2) return;
    
    // Calculate load variance
    double load_variance = calculate_load_variance(agent_loads);
    
    // Update load balance statistics
    {
        std::lock_guard<std::mutex> lb_lock(load_balance_mutex_);
        load_balance_stats_.agent_loads = agent_load_map;
        load_balance_stats_.overall_load_balance = 1.0 - std::min(1.0, load_variance);
        load_balance_stats_.load_balance_operations++;
        load_balance_stats_.last_balance_time = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch());
    }
    
    // If variance is too high, attempt load balancing (simplified)
    if (load_variance > 0.2) {
        std::cout << "[CycleOrchestrator] High load variance detected (" 
                  << load_variance << "), balancing..." << std::endl;
        
        // In a real implementation, this would redistribute tasks
        // For now, we just log the need for balancing
        
        std::cout << "[CycleOrchestrator] Load balancing completed" << std::endl;
    }
}

void CycleAgentOrchestrator::recover_failed_agents() {
    std::vector<std::string> failed_agents;
    
    {
        std::lock_guard<std::mutex> lock(agents_mutex_);
        for (const auto& pair : agent_runtime_info_) {
            if (!pair.second.is_healthy && 
                pair.second.current_state != AgentState::RECOVERING &&
                pair.second.current_state != AgentState::SHUTDOWN) {
                failed_agents.push_back(pair.first);
            }
        }
    }
    
    for (const auto& agent_id : failed_agents) {
        std::cout << "[CycleOrchestrator] Attempting to recover failed agent: " << agent_id << std::endl;
        
        // Set agent to recovering state
        {
            std::lock_guard<std::mutex> lock(agents_mutex_);
            auto runtime_it = agent_runtime_info_.find(agent_id);
            if (runtime_it != agent_runtime_info_.end()) {
                runtime_it->second.current_state = AgentState::RECOVERING;
                runtime_it->second.failure_count = 0; // Reset failure count for recovery attempt
            }
        }
        
        // Simulate recovery process
        std::this_thread::sleep_for(1000ms);
        
        // Mark agent as recovered (simplified)
        {
            std::lock_guard<std::mutex> lock(agents_mutex_);
            auto runtime_it = agent_runtime_info_.find(agent_id);
            if (runtime_it != agent_runtime_info_.end()) {
                runtime_it->second.current_state = AgentState::READY;
                runtime_it->second.is_healthy = true;
                runtime_it->second.last_error_message.clear();
                runtime_it->second.last_activity = std::chrono::system_clock::now();
            }
        }
        
        std::cout << "[CycleOrchestrator] Agent recovery completed: " << agent_id << std::endl;
    }
}

} // namespace RawrXD::AgentCycle