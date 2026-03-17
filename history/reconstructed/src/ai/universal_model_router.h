#pragma once
#include <string>
#include <vector>
#include <memory>
#include <expected>
#include <unordered_map>
#include <atomic>
#include <mutex>
#include <thread>
#include <numeric>
#include <format>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace RawrXD {

enum class RouterError {
    Success = 0,
    ModelNotFound,
    RoutingFailed,
    LoadBalancingFailed,
    Timeout,
    NoAvailableModels
};

struct ModelRoute {
    std::string modelId;
    std::string endpoint;
    float latency;
    float throughput;
    float cost;
    std::vector<std::string> capabilities;
    std::atomic<bool> isAvailable{true};
    
    ModelRoute() = default;
    ModelRoute(const ModelRoute& other) {
        modelId = other.modelId;
        endpoint = other.endpoint;
        latency = other.latency;
        throughput = other.throughput;
        cost = other.cost;
        capabilities = other.capabilities;
        isAvailable.store(other.isAvailable.load());
    }
};

struct RoutingDecision {
    std::string selectedModel;
    std::string reasoning;
    float confidence;
    std::vector<std::string> alternatives;
};

class UniversalModelRouter {
public:
    UniversalModelRouter();
    ~UniversalModelRouter();
    
    UniversalModelRouter(const UniversalModelRouter&) = delete;
    UniversalModelRouter& operator=(const UniversalModelRouter&) = delete;
    
    // Real model registration
    std::expected<void, RouterError> registerModel(const ModelRoute& route);
    
    // Real routing with load balancing
    std::expected<RoutingDecision, RouterError> routeRequest(
        const std::string& task,
        const std::unordered_map<std::string, std::string>& requirements
    );
    
    // Real health monitoring
    void startHealthMonitoring();
    void stopHealthMonitoring();
    
private:
    std::unordered_map<std::string, ModelRoute> m_models;
    std::unordered_map<std::string, std::vector<float>> m_latencyHistory;
    std::unordered_map<std::string, float> m_errorRates;
    std::atomic<bool> m_healthMonitoringRunning{false};
    std::thread m_healthMonitorThread;
    mutable std::shared_mutex m_mutex;
    
    float calculateModelScore(
        const ModelRoute& model,
        const std::string& task,
        const std::unordered_map<std::string, std::string>& requirements
    );
    
    void healthMonitorLoop();
    void updateModelStats();
    void evictUnhealthyModels();
};

} // namespace RawrXD
