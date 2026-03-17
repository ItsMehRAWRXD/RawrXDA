#include "service_registry.h"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <ctime>
#include <algorithm>

// Global service registry instance
std::unique_ptr<ServiceRegistry> g_service_registry;

ServiceRegistry::ServiceRegistry() : heartbeat_running_(false), heartbeat_interval_(60) {
}

ServiceRegistry::~ServiceRegistry() {
    StopHeartbeat();
}

void ServiceRegistry::LogRegistryEvent(const std::string& event, const std::string& details) {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    std::cout << "[" << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S") 
              << "] [ServiceRegistry] [" << event << "] " << details << std::endl;
}

bool ServiceRegistry::RegisterService(const ServiceInfo& info) {
    std::lock_guard<std::mutex> lock(services_mutex_);
    
    if (info.service_id.empty() || info.service_name.empty()) {
        LogRegistryEvent("ERROR", "Invalid service info: missing ID or name");
        return false;
    }
    
    local_services_[info.service_id] = info;
    LogRegistryEvent("INFO", "Registered service: " + info.service_name + 
                     " (ID: " + info.service_id + ") on port " + std::to_string(info.port));
    
    // Send to external registry if configured
    if (!registry_url_.empty()) {
        std::string body = SerializeServiceInfo(info);
        std::string response;
        if (SendRegistryRequest("POST", "/v1/catalog/register", body, response)) {
            LogRegistryEvent("INFO", "Service registered with external registry");
        } else {
            LogRegistryEvent("WARN", "Failed to register with external registry");
        }
    }
    
    return true;
}

bool ServiceRegistry::DeregisterService(const std::string& service_id) {
    std::lock_guard<std::mutex> lock(services_mutex_);
    
    auto it = local_services_.find(service_id);
    if (it == local_services_.end()) {
        LogRegistryEvent("WARN", "Service not found: " + service_id);
        return false;
    }
    
    local_services_.erase(it);
    LogRegistryEvent("INFO", "Deregistered service: " + service_id);
    
    // Send to external registry if configured
    if (!registry_url_.empty()) {
        std::string response;
        SendRegistryRequest("PUT", "/v1/catalog/deregister/" + service_id, "", response);
    }
    
    return true;
}

bool ServiceRegistry::UpdateServiceStatus(const std::string& service_id, ServiceInfo::Status status) {
    std::lock_guard<std::mutex> lock(services_mutex_);
    
    auto it = local_services_.find(service_id);
    if (it == local_services_.end()) {
        return false;
    }
    
    it->second.status = status;
    
    const char* status_names[] = {"HEALTHY", "UNHEALTHY", "STARTING", "STOPPING", "UNKNOWN"};
    LogRegistryEvent("INFO", "Service " + service_id + " status: " + status_names[static_cast<int>(status)]);
    
    return true;
}

bool ServiceRegistry::SendHeartbeat(const std::string& service_id) {
    std::lock_guard<std::mutex> lock(services_mutex_);
    
    auto it = local_services_.find(service_id);
    if (it == local_services_.end()) {
        return false;
    }
    
    it->second.last_heartbeat = std::chrono::system_clock::now();
    
    // Send to external registry if configured
    if (!registry_url_.empty()) {
        std::string response;
        if (SendRegistryRequest("PUT", "/v1/agent/check/pass/" + service_id, "", response)) {
            return true;
        }
    }
    
    return true;
}

std::vector<ServiceInfo> ServiceRegistry::DiscoverServices(const std::string& service_name) {
    std::vector<ServiceInfo> result;
    
    // Query external registry if configured
    if (!registry_url_.empty()) {
        std::string response;
        if (SendRegistryRequest("GET", "/v1/catalog/service/" + service_name, "", response)) {
            // Parse response and populate result
            // Placeholder - in production, parse JSON response
        }
    }
    
    // Also return local services
    std::lock_guard<std::mutex> lock(services_mutex_);
    for (const auto& pair : local_services_) {
        if (pair.second.service_name == service_name) {
            result.push_back(pair.second);
        }
    }
    
    LogRegistryEvent("INFO", "Discovered " + std::to_string(result.size()) + 
                     " instances of service: " + service_name);
    
    return result;
}

bool ServiceRegistry::GetServiceInfo(const std::string& service_id, ServiceInfo& out_info) {
    std::lock_guard<std::mutex> lock(services_mutex_);
    
    auto it = local_services_.find(service_id);
    if (it == local_services_.end()) {
        return false;
    }
    
    out_info = it->second;
    return true;
}

void ServiceRegistry::StartHeartbeat(const std::string& service_id, int interval_seconds) {
    if (heartbeat_running_.load()) {
        LogRegistryEvent("WARN", "Heartbeat already running");
        return;
    }
    
    heartbeat_service_id_ = service_id;
    heartbeat_interval_ = interval_seconds;
    heartbeat_running_ = true;
    
    heartbeat_thread_ = std::make_unique<std::thread>(&ServiceRegistry::HeartbeatWorker, this);
    
    LogRegistryEvent("INFO", "Started heartbeat for service: " + service_id + 
                     " (interval: " + std::to_string(interval_seconds) + "s)");
}

void ServiceRegistry::StopHeartbeat() {
    if (!heartbeat_running_.load()) {
        return;
    }
    
    heartbeat_running_ = false;
    
    if (heartbeat_thread_ && heartbeat_thread_->joinable()) {
        heartbeat_thread_->join();
    }
    
    LogRegistryEvent("INFO", "Stopped heartbeat");
}

void ServiceRegistry::SetRegistryUrl(const std::string& url) {
    registry_url_ = url;
    LogRegistryEvent("INFO", "Set registry URL: " + url);
}

std::vector<ServiceInfo> ServiceRegistry::GetAllServices() const {
    std::lock_guard<std::mutex> lock(services_mutex_);
    std::vector<ServiceInfo> result;
    result.reserve(local_services_.size());
    
    for (const auto& pair : local_services_) {
        result.push_back(pair.second);
    }
    
    return result;
}

bool ServiceRegistry::SendRegistryRequest(const std::string& method, 
                                         const std::string& endpoint,
                                         const std::string& body,
                                         std::string& response) {
    // Placeholder implementation
    // In production, use HTTP client library (libcurl, etc.)
    return false;
}

std::string ServiceRegistry::SerializeServiceInfo(const ServiceInfo& info) {
    // Placeholder JSON serialization
    std::ostringstream ss;
    ss << "{";
    ss << R"("ID":")" << info.service_id << "\",";
    ss << R"("Name":")" << info.service_name << "\",";
    ss << R"("Address":")" << info.host << "\",";
    ss << R"("Port":)" << info.port << ",";
    ss << R"("Tags":[)";
    for (size_t i = 0; i < info.tags.size(); ++i) {
        if (i > 0) ss << ",";
        ss << "\"" << info.tags[i] << "\"";
    }
    ss << "]}";
    return ss.str();
}

ServiceInfo ServiceRegistry::DeserializeServiceInfo(const std::string& json) {
    // Placeholder JSON deserialization
    // In production, use proper JSON library
    ServiceInfo info;
    return info;
}

void ServiceRegistry::HeartbeatWorker() {
    while (heartbeat_running_.load()) {
        SendHeartbeat(heartbeat_service_id_);
        
        // Sleep for interval
        for (int i = 0; i < heartbeat_interval_ && heartbeat_running_.load(); ++i) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }
}
