// ============================================================================
// System Health Monitor — Comprehensive Health Monitoring
// Monitors all system components and features
// ============================================================================
#pragma once
#include "../core/session_manager.h"
#include "../performance/realtime_profiler.h"
#include <memory>
#include <vector>
#include <string>
#include <map>
#include <mutex>
#include <chrono>
#include <functional>

namespace RawrXD::Health {

enum class HealthStatus {
    HEALTHY,
    DEGRADED,
    UNHEALTHY,
    UNKNOWN
};

enum class ComponentType {
    FEATURE,
    SERVICE,
    DATABASE,
    CACHE,
    QUEUE,
    EXTERNAL_API
};

struct HealthCheck {
    std::string name;
    ComponentType type;
    std::function<HealthStatus()> check;
    std::chrono::seconds interval;
    bool isCritical;
    std::string lastError;
    std::chrono::system_clock::time_point lastChecked;
    HealthStatus lastStatus;
};

struct ComponentHealth {
    std::string componentId;
    std::string componentName;
    ComponentType type;
    HealthStatus status;
    std::string message;
    double responseTime;
    std::chrono::system_clock::time_point checkedAt;
    std::vector<std::string> dependencies;
};

struct SystemHealth {
    HealthStatus overallStatus;
    std::vector<ComponentHealth> components;
    int healthyCount;
    int degradedCount;
    int unhealthyCount;
    std::chrono::system_clock::time_point checkedAt;
    std::vector<std::string> criticalIssues;
};

class SystemHealthMonitor {
public:
    explicit SystemHealthMonitor(std::shared_ptr<Core::SessionManager> sessionManager)
        : m_sessionManager(sessionManager)
        , m_running(false) {}

    ~SystemHealthMonitor() {
        StopMonitoring();
    }

    void RegisterHealthCheck(const HealthCheck& check) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_healthChecks[check.name] = check;
    }

    void UnregisterHealthCheck(const std::string& name) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_healthChecks.erase(name);
    }

    void StartMonitoring() {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        if (m_running) return;
        
        m_running = true;
        
        // Start monitoring thread
        m_monitorThread = std::thread([this]() {
            while (m_running) {
                RunHealthChecks();
                std::this_thread::sleep_for(std::chrono::seconds(30));
            }
        });
    }

    void StopMonitoring() {
        m_running = false;
        if (m_monitorThread.joinable()) {
            m_monitorThread.join();
        }
    }

    SystemHealth GetSystemHealth() {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        SystemHealth health;
        health.checkedAt = std::chrono::system_clock::now();
        health.healthyCount = 0;
        health.degradedCount = 0;
        health.unhealthyCount = 0;
        
        // Run all health checks
        for (auto& [name, check] : m_healthChecks) {
            auto start = std::chrono::steady_clock::now();
            HealthStatus status = HealthStatus::UNKNOWN;
            
            try {
                status = check.check();
            } catch (const std::exception& e) {
                status = HealthStatus::UNHEALTHY;
                check.lastError = e.what();
            }
            
            auto end = std::chrono::steady_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
            
            check.lastChecked = std::chrono::system_clock::now();
            check.lastStatus = status;
            
            ComponentHealth component;
            component.componentId = name;
            component.componentName = name;
            component.type = check.type;
            component.status = status;
            component.responseTime = duration.count();
            component.checkedAt = check.lastChecked;
            
            switch (status) {
                case HealthStatus::HEALTHY:
                    health.healthyCount++;
                    break;
                case HealthStatus::DEGRADED:
                    health.degradedCount++;
                    if (check.isCritical) {
                        health.criticalIssues.push_back(name + " is degraded");
                    }
                    break;
                case HealthStatus::UNHEALTHY:
                    health.unhealthyCount++;
                    if (check.isCritical) {
                        health.criticalIssues.push_back(name + " is unhealthy");
                    }
                    break;
                default:
                    break;
            }
            
            health.components.push_back(component);
        }
        
        // Determine overall status
        if (health.unhealthyCount > 0) {
            health.overallStatus = HealthStatus::UNHEALTHY;
        } else if (health.degradedCount > 0) {
            health.overallStatus = HealthStatus::DEGRADED;
        } else {
            health.overallStatus = HealthStatus::HEALTHY;
        }
        
        m_lastHealth = health;
        return health;
    }

    ComponentHealth GetComponentHealth(const std::string& componentId) {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        auto it = m_healthChecks.find(componentId);
        if (it != m_healthChecks.end()) {
            ComponentHealth health;
            health.componentId = componentId;
            health.componentName = componentId;
            health.type = it->second.type;
            health.status = it->second.lastStatus;
            health.checkedAt = it->second.lastChecked;
            return health;
        }
        
        return ComponentHealth{};
    }

    std::vector<ComponentHealth> GetUnhealthyComponents() {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        std::vector<ComponentHealth> unhealthy;
        for (const auto& [name, check] : m_healthChecks) {
            if (check.lastStatus == HealthStatus::UNHEALTHY ||
                check.lastStatus == HealthStatus::DEGRADED) {
                ComponentHealth health;
                health.componentId = name;
                health.componentName = name;
                health.type = check.type;
                health.status = check.lastStatus;
                health.checkedAt = check.lastChecked;
                unhealthy.push_back(health);
            }
        }
        return unhealthy;
    }

    void SetAlertThreshold(const std::string& componentId, int threshold) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_alertThresholds[componentId] = threshold;
    }

    std::string GenerateHealthReport() {
        auto health = GetSystemHealth();
        
        std::ostringstream report;
        report << "# System Health Report\n\n";
        report << "**Generated:** " << FormatTime(health.checkedAt) << "\n";
        report << "**Overall Status:** " << StatusToString(health.overallStatus) << "\n\n";
        
        report << "## Summary\n";
        report << "- **Healthy:** " << health.healthyCount << "\n";
        report << "- **Degraded:** " << health.degradedCount << "\n";
        report << "- **Unhealthy:** " << health.unhealthyCount << "\n\n";
        
        if (!health.criticalIssues.empty()) {
            report << "## Critical Issues\n";
            for (const auto& issue : health.criticalIssues) {
                report << "- " << issue << "\n";
            }
            report << "\n";
        }
        
        report << "## Component Status\n";
        report << "| Component | Type | Status | Response Time |\n";
        report << "|-----------|------|--------|---------------|\n";
        
        for (const auto& component : health.components) {
            report << "| " << component.componentName << " | "
                   << TypeToString(component.type) << " | "
                   << StatusToString(component.status) << " | "
                   << component.responseTime << "ms |\n";
        }
        
        return report.str();
    }

private:
    std::shared_ptr<Core::SessionManager> m_sessionManager;
    mutable std::mutex m_mutex;
    std::map<std::string, HealthCheck> m_healthChecks;
    std::map<std::string, int> m_alertThresholds;
    SystemHealth m_lastHealth;
    bool m_running;
    std::thread m_monitorThread;

    void RunHealthChecks() {
        // This is called periodically by the monitoring thread
        // Individual checks are performed in GetSystemHealth
    }

    std::string StatusToString(HealthStatus status) {
        switch (status) {
            case HealthStatus::HEALTHY: return "✅ Healthy";
            case HealthStatus::DEGRADED: return "⚠️ Degraded";
            case HealthStatus::UNHEALTHY: return "❌ Unhealthy";
            case HealthStatus::UNKNOWN: return "❓ Unknown";
            default: return "Unknown";
        }
    }

    std::string TypeToString(ComponentType type) {
        switch (type) {
            case ComponentType::FEATURE: return "Feature";
            case ComponentType::SERVICE: return "Service";
            case ComponentType::DATABASE: return "Database";
            case ComponentType::CACHE: return "Cache";
            case ComponentType::QUEUE: return "Queue";
            case ComponentType::EXTERNAL_API: return "External API";
            default: return "Unknown";
        }
    }

    std::string FormatTime(std::chrono::system_clock::time_point time) {
        auto timeT = std::chrono::system_clock::to_time_t(time);
        std::ostringstream oss;
        oss << std::put_time(std::localtime(&timeT), "%Y-%m-%d %H:%M:%S");
        return oss.str();
    }
};

} // namespace RawrXD::Health
