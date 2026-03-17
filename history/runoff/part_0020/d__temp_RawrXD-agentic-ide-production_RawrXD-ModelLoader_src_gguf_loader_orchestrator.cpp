#include "gguf_loader_orchestrator.hpp"
#include <algorithm>
#include <iostream>
#include <filesystem>

#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

GGUFLoaderOrchestrator::GGUFLoaderOrchestrator()
    : m_current_loader(LoaderType::ENTERPRISE),
      m_current_agent(AgentType::EXECUTION),
      m_rotation_strategy("performance"),
      m_auto_rotate(true),
      m_agentic_loop_initialized(false),
      m_agentic_loop_active(false),
      m_use_compression(true),
      m_compression_threshold(256 * 1024 * 1024) {  // 256MB
    
    // Initialize random engine
    m_random_engine.seed(std::chrono::steady_clock::now().time_since_epoch().count());
    
    // Initialize MASM agentic loop
    if (InitializeAgenticLoop() != 0) {
        m_agentic_loop_initialized = true;
        std::cout << "[ORCHESTRATOR] MASM Agentic Loop initialized" << std::endl;
    }
}

GGUFLoaderOrchestrator::~GGUFLoaderOrchestrator() {
    // Stop agentic loop
    if (m_agentic_loop_active) {
        StopAgenticLoop();
    }
    if (m_agentic_loop_initialized) {
        CleanupAgenticLoop();
    }
    
    // Unload all loader DLLs
    for (auto& [type, loader] : m_loaders) {
        if (loader.handle) {
#ifdef _WIN32
            FreeLibrary(static_cast<HMODULE>(loader.handle));
#else
            dlclose(loader.handle);
#endif
        }
    }
}

bool GGUFLoaderOrchestrator::initializeLoaders(const std::string& loaders_dir) {
    std::cout << "[ORCHESTRATOR] Initializing GGUF loaders from: " << loaders_dir << std::endl;
    
    // Define all available MASM loaders
    std::vector<std::pair<LoaderType, std::string>> loader_files = {
        {LoaderType::BENCH, "gguf_bench_loader.dll"},
        {LoaderType::CHAIN, "gguf_chain_loader_unified.dll"},
        {LoaderType::CLEAN, "gguf_loader_clean.dll"},
        {LoaderType::COMPLETE, "gguf_loader_complete.dll"},
        {LoaderType::ENHANCED, "gguf_loader_enhanced.dll"},
        {LoaderType::ENTERPRISE, "gguf_loader_enterprise.dll"},
        {LoaderType::FINAL, "gguf_loader_final.dll"},
        {LoaderType::INTEGRATION, "gguf_loader_integration.dll"},
        {LoaderType::MINIMAL, "gguf_loader_minimal.dll"}
    };
    
    // Load each available loader
    for (const auto& [type, filename] : loader_files) {
        std::string fullpath = loaders_dir + "/" + filename;
        
#ifdef _WIN32
        HMODULE handle = LoadLibraryA(fullpath.c_str());
#else
        void* handle = dlopen(fullpath.c_str(), RTLD_LAZY);
#endif
        
        if (handle) {
            LoaderInstance instance;
            instance.type = type;
            instance.name = filename;
            instance.handle = handle;
            instance.is_active = true;
            instance.last_used = std::chrono::steady_clock::now();
            
            m_loaders[type] = instance;
            
            // Load function pointer
#ifdef _WIN32
            auto func = reinterpret_cast<LoadModelFunc>(GetProcAddress(handle, "LoadModel"));
#else
            auto func = reinterpret_cast<LoadModelFunc>(dlsym(handle, "LoadModel"));
#endif
            if (func) {
                m_loader_funcs[type] = func;
            }
            
            std::cout << "[ORCHESTRATOR] Loaded: " << filename << std::endl;
        } else {
            std::cout << "[ORCHESTRATOR] Failed to load: " << filename << std::endl;
        }
    }
    
    std::cout << "[ORCHESTRATOR] Initialized " << m_loaders.size() << " loaders" << std::endl;
    return !m_loaders.empty();
}

bool GGUFLoaderOrchestrator::initializeAgents() {
    std::cout << "[ORCHESTRATOR] Initializing agents..." << std::endl;
    
    // Initialize all agent types
    std::vector<AgentType> agent_types = {
        AgentType::PLANNING,
        AgentType::EXECUTION,
        AgentType::MONITORING,
        AgentType::OPTIMIZATION
    };
    
    for (AgentType type : agent_types) {
        AgentInstance instance;
        instance.type = type;
        instance.is_active = true;
        instance.last_used = std::chrono::steady_clock::now();
        
        switch (type) {
            case AgentType::PLANNING:
                instance.name = "PlanningAgent";
                break;
            case AgentType::EXECUTION:
                instance.name = "ExecutionAgent";
                break;
            case AgentType::MONITORING:
                instance.name = "MonitoringAgent";
                break;
            case AgentType::OPTIMIZATION:
                instance.name = "OptimizationAgent";
                break;
        }
        
        m_agents[type] = instance;
        std::cout << "[ORCHESTRATOR] Initialized: " << instance.name << std::endl;
    }
    
    std::cout << "[ORCHESTRATOR] Initialized " << m_agents.size() << " agents" << std::endl;
    return !m_agents.empty();
}

LoaderType GGUFLoaderOrchestrator::selectBestLoader(const std::string& model_path, size_t model_size_bytes) {
    // Auto-select based on model size and rotation strategy
    if (m_rotation_strategy == "performance") {
        return selectLoaderByPerformance();
    } else if (m_rotation_strategy == "round-robin") {
        return selectLoaderRoundRobin();
    } else if (m_rotation_strategy == "random") {
        return selectLoaderRandom();
    }
    
    // Default: use model size heuristics
    if (model_size_bytes < 1ULL * 1024 * 1024 * 1024) {  // < 1GB
        return LoaderType::MINIMAL;
    } else if (model_size_bytes < 10ULL * 1024 * 1024 * 1024) {  // < 10GB
        return LoaderType::ENHANCED;
    } else if (model_size_bytes < 50ULL * 1024 * 1024 * 1024) {  // < 50GB
        return LoaderType::COMPLETE;
    } else {
        return LoaderType::ENTERPRISE;  // 70B+ models
    }
}

AgentType GGUFLoaderOrchestrator::selectBestAgent(const std::string& task_type) {
    // Auto-select based on task type and rotation strategy
    if (m_rotation_strategy == "performance") {
        return selectAgentByPerformance();
    } else if (m_rotation_strategy == "round-robin") {
        return selectAgentRoundRobin();
    } else if (m_rotation_strategy == "random") {
        return selectAgentRandom();
    }
    
    // Default: use task type heuristics
    if (task_type.find("plan") != std::string::npos) {
        return AgentType::PLANNING;
    } else if (task_type.find("monitor") != std::string::npos || 
               task_type.find("health") != std::string::npos) {
        return AgentType::MONITORING;
    } else if (task_type.find("optimize") != std::string::npos || 
               task_type.find("perf") != std::string::npos) {
        return AgentType::OPTIMIZATION;
    } else {
        return AgentType::EXECUTION;
    }
}

bool GGUFLoaderOrchestrator::loadModel(const std::string& model_path, LoaderType loader_type) {
    auto start = std::chrono::steady_clock::now();
    
    std::cout << "[ORCHESTRATOR] Loading model: " << model_path << " with loader type " 
              << static_cast<int>(loader_type) << std::endl;
    
    // Check if loader exists
    auto it = m_loaders.find(loader_type);
    if (it == m_loaders.end() || !it->second.is_active) {
        std::cerr << "[ORCHESTRATOR] Loader not available, falling back..." << std::endl;
        loader_type = rotateLoader();
        it = m_loaders.find(loader_type);
    }
    
    // Call MASM loader function
    bool success = false;
    auto func_it = m_loader_funcs.find(loader_type);
    if (func_it != m_loader_funcs.end() && func_it->second) {
        success = func_it->second(model_path.c_str());
    }
    
    // Update stats
    auto end = std::chrono::steady_clock::now();
    double elapsed_ms = std::chrono::duration<double, std::milli>(end - start).count();
    updateLoaderStats(loader_type, elapsed_ms);
    
    // Auto-rotate if enabled
    if (m_auto_rotate) {
        m_current_loader = rotateLoader();
    }
    
    std::cout << "[ORCHESTRATOR] Model load " << (success ? "SUCCESS" : "FAILED") 
              << " in " << elapsed_ms << "ms" << std::endl;
    
    return success;
}

bool GGUFLoaderOrchestrator::executeTask(const std::string& task, AgentType agent_type) {
    auto start = std::chrono::steady_clock::now();
    
    std::cout << "[ORCHESTRATOR] Executing task with agent type " 
              << static_cast<int>(agent_type) << std::endl;
    
    // Check if agent exists
    auto it = m_agents.find(agent_type);
    if (it == m_agents.end() || !it->second.is_active) {
        std::cerr << "[ORCHESTRATOR] Agent not available, falling back..." << std::endl;
        agent_type = rotateAgent();
        it = m_agents.find(agent_type);
    }
    
    // Execute task (placeholder - integrate with actual agent system)
    bool success = true;  // TODO: Call actual agent execution
    
    // Update stats
    auto end = std::chrono::steady_clock::now();
    double elapsed_ms = std::chrono::duration<double, std::milli>(end - start).count();
    updateAgentStats(agent_type, elapsed_ms);
    
    // Auto-rotate if enabled
    if (m_auto_rotate) {
        m_current_agent = rotateAgent();
    }
    
    std::cout << "[ORCHESTRATOR] Task execution " << (success ? "SUCCESS" : "FAILED") 
              << " in " << elapsed_ms << "ms" << std::endl;
    
    return success;
}

LoaderType GGUFLoaderOrchestrator::rotateLoader() {
    if (m_loaders.empty()) return LoaderType::ENTERPRISE;
    
    // Find next active loader
    auto it = m_loaders.find(m_current_loader);
    if (it != m_loaders.end()) {
        ++it;
    }
    
    if (it == m_loaders.end() || it == m_loaders.begin()) {
        it = m_loaders.begin();
    }
    
    m_current_loader = it->first;
    std::cout << "[ORCHESTRATOR] Rotated to loader: " << it->second.name << std::endl;
    
    return m_current_loader;
}

AgentType GGUFLoaderOrchestrator::rotateAgent() {
    if (m_agents.empty()) return AgentType::EXECUTION;
    
    // Find next active agent
    auto it = m_agents.find(m_current_agent);
    if (it != m_agents.end()) {
        ++it;
    }
    
    if (it == m_agents.end()) {
        it = m_agents.begin();
    }
    
    m_current_agent = it->first;
    std::cout << "[ORCHESTRATOR] Rotated to agent: " << it->second.name << std::endl;
    
    return m_current_agent;
}

LoaderType GGUFLoaderOrchestrator::selectLoaderByPerformance() {
    // Select loader with best average load time
    LoaderType best = LoaderType::ENTERPRISE;
    double best_time = std::numeric_limits<double>::max();
    
    for (const auto& [type, loader] : m_loaders) {
        if (loader.is_active && loader.load_count > 0) {
            if (loader.avg_load_time_ms < best_time) {
                best_time = loader.avg_load_time_ms;
                best = type;
            }
        }
    }
    
    return best;
}

LoaderType GGUFLoaderOrchestrator::selectLoaderRoundRobin() {
    return rotateLoader();
}

LoaderType GGUFLoaderOrchestrator::selectLoaderRandom() {
    if (m_loaders.empty()) return LoaderType::ENTERPRISE;
    
    std::uniform_int_distribution<size_t> dist(0, m_loaders.size() - 1);
    size_t index = dist(m_random_engine);
    
    auto it = m_loaders.begin();
    std::advance(it, index);
    
    return it->first;
}

AgentType GGUFLoaderOrchestrator::selectAgentByPerformance() {
    // Select agent with best average response time
    AgentType best = AgentType::EXECUTION;
    double best_time = std::numeric_limits<double>::max();
    
    for (const auto& [type, agent] : m_agents) {
        if (agent.is_active && agent.task_count > 0) {
            if (agent.avg_response_time_ms < best_time) {
                best_time = agent.avg_response_time_ms;
                best = type;
            }
        }
    }
    
    return best;
}

AgentType GGUFLoaderOrchestrator::selectAgentRoundRobin() {
    return rotateAgent();
}

AgentType GGUFLoaderOrchestrator::selectAgentRandom() {
    if (m_agents.empty()) return AgentType::EXECUTION;
    
    std::uniform_int_distribution<size_t> dist(0, m_agents.size() - 1);
    size_t index = dist(m_random_engine);
    
    auto it = m_agents.begin();
    std::advance(it, index);
    
    return it->first;
}

void GGUFLoaderOrchestrator::updateLoaderStats(LoaderType type, double load_time_ms) {
    auto it = m_loaders.find(type);
    if (it != m_loaders.end()) {
        auto& loader = it->second;
        
        // Update rolling average
        double total_time = loader.avg_load_time_ms * loader.load_count;
        loader.load_count++;
        loader.avg_load_time_ms = (total_time + load_time_ms) / loader.load_count;
        loader.last_used = std::chrono::steady_clock::now();
    }
}

void GGUFLoaderOrchestrator::updateAgentStats(AgentType type, double response_time_ms) {
    auto it = m_agents.find(type);
    if (it != m_agents.end()) {
        auto& agent = it->second;
        
        // Update rolling average
        double total_time = agent.avg_response_time_ms * agent.task_count;
        agent.task_count++;
        agent.avg_response_time_ms = (total_time + response_time_ms) / agent.task_count;
        agent.last_used = std::chrono::steady_clock::now();
    }
}

bool GGUFLoaderOrchestrator::initializeAgenticLoop() {
    if (m_agentic_loop_initialized) return true;
    
    int result = InitializeAgenticLoop();
    if (result != 0) {
        m_agentic_loop_initialized = true;
        std::cout << "[ORCHESTRATOR] Agentic loop initialized from MASM" << std::endl;
        return true;
    }
    return false;
}

void GGUFLoaderOrchestrator::startAgenticProcessing(const std::string& task) {
    if (!m_agentic_loop_initialized) {
        initializeAgenticLoop();
    }
    
    if (m_agentic_loop_initialized) {
        StartAgenticLoop(task.c_str());
        m_agentic_loop_active = true;
        std::cout << "[ORCHESTRATOR] Agentic processing started for: " << task << std::endl;
    }
}

void GGUFLoaderOrchestrator::stopAgenticProcessing() {
    if (m_agentic_loop_active) {
        StopAgenticLoop();
        m_agentic_loop_active = false;
        std::cout << "[ORCHESTRATOR] Agentic processing stopped" << std::endl;
    }
}

void* GGUFLoaderOrchestrator::compressModelData(const void* data, size_t size, size_t* compressed_size) {
    if (!m_use_compression || !shouldCompress(size)) {
        return nullptr;
    }
    
    std::cout << "[ORCHESTRATOR] Compressing " << size / (1024.0*1024) << "MB with brutal deflate..." << std::endl;
    
    void* compressed = deflate_brutal_masm(data, size, compressed_size);
    
    if (compressed && compressed_size && *compressed_size > 0) {
        float ratio = (float)size / *compressed_size;
        std::cout << "[ORCHESTRATOR] Compressed to " << *compressed_size / (1024.0*1024) 
                  << "MB (" << ratio << "x ratio)" << std::endl;
    }
    
    return compressed;
}

bool GGUFLoaderOrchestrator::shouldCompress(size_t data_size) const {
    return data_size > m_compression_threshold;
}
