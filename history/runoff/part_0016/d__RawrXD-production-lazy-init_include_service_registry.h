#pragma once

#include <string>
#include <vector>
#include <memory>
#include <chrono>
#include <mutex>
#include <unordered_map>
#include <thread>
#include <atomic>

// Service information for registration
struct ServiceInfo {
    std::string service_id;
    std::string service_name;
    std::string host;
    uint16_t port;
    std::string protocol; // "http" or "https"
    std::vector<std::string> tags;
    std::unordered_map<std::string, std::string> metadata;
    std::chrono::system_clock::time_point last_heartbeat;
    std::string health_check_endpoint = "/health";
    
    // Service status
    enum class Status {
        HEALTHY,
        UNHEALTHY,
        STARTING,
        STOPPING,
        UNKNOWN
    };
    Status status = Status::STARTING;
    
    // Service capabilities
    struct Capabilities {
        bool supports_streaming = true;
        bool supports_websocket = false;
        bool requires_auth = false;
        std::vector<std::string> supported_models;
        std::vector<std::string> supported_endpoints;
    };
    Capabilities capabilities;
};

// Service Registry for automatic discovery
class ServiceRegistry {
public:
    ServiceRegistry();
    ~ServiceRegistry();
    
    // Register this service instance
    bool RegisterService(const ServiceInfo& info);
    
    // Deregister this service instance
    bool DeregisterService(const std::string& service_id);
    
    // Update service status
    bool UpdateServiceStatus(const std::string& service_id, ServiceInfo::Status status);
    
    // Send heartbeat to registry
    bool SendHeartbeat(const std::string& service_id);
    
    // Discover other service instances
    std::vector<ServiceInfo> DiscoverServices(const std::string& service_name);
    
    // Get specific service info
    bool GetServiceInfo(const std::string& service_id, ServiceInfo& out_info);
    
    // Start automatic heartbeat thread
    void StartHeartbeat(const std::string& service_id, int interval_seconds = 60);
    
    // Stop automatic heartbeat
    void StopHeartbeat();
    
    // Set registry URL (for external registries like Consul, etcd)
    void SetRegistryUrl(const std::string& url);
    
    // Get all registered services (local cache)
    std::vector<ServiceInfo> GetAllServices() const;

private:
    std::string registry_url_;
    std::unordered_map<std::string, ServiceInfo> local_services_;
    mutable std::mutex services_mutex_;
    
    // Heartbeat thread
    std::unique_ptr<std::thread> heartbeat_thread_;
    std::atomic<bool> heartbeat_running_{false};
    std::string heartbeat_service_id_;
    int heartbeat_interval_;
    
    // HTTP client for external registry communication
    bool SendRegistryRequest(const std::string& method, 
                            const std::string& endpoint,
                            const std::string& body,
                            std::string& response);
    
    // Serialize/deserialize service info
    std::string SerializeServiceInfo(const ServiceInfo& info);
    ServiceInfo DeserializeServiceInfo(const std::string& json);
    
    // Heartbeat worker
    void HeartbeatWorker();
    
    // Logging
    void LogRegistryEvent(const std::string& event, const std::string& details);
};

// Global service registry instance
extern std::unique_ptr<ServiceRegistry> g_service_registry;
