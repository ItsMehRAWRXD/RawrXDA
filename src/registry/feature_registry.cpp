// ============================================================================
// Feature Registry — Central Feature Management
// Manages feature registration, discovery, and lifecycle
// ============================================================================
#pragma once
#include "../core/session_manager.h"
#include <memory>
#include <vector>
#include <string>
#include <map>
#include <mutex>
#include <functional>

namespace RawrXD::Registry {

enum class FeatureState {
    UNREGISTERED,
    REGISTERED,
    INITIALIZING,
    ACTIVE,
    PAUSED,
    ERROR,
    DISABLED
};

enum class FeatureCategory {
    CORE,
    AI,
    SECURITY,
    PERFORMANCE,
    QUALITY,
    INTEGRATION,
    UI,
    CUSTOM
};

struct FeatureInfo {
    std::string id;
    std::string name;
    std::string description;
    std::string version;
    FeatureCategory category;
    FeatureState state;
    std::vector<std::string> dependencies;
    std::vector<std::string> provides;
    std::map<std::string, std::string> configuration;
    std::chrono::system_clock::time_point registeredAt;
    std::chrono::system_clock::time_point activatedAt;
    std::string errorMessage;
};

struct FeatureCapability {
    std::string name;
    std::string description;
    std::function<void(void*)> handler;
    bool isAsync;
};

class FeatureRegistry {
public:
    explicit FeatureRegistry(std::shared_ptr<Core::SessionManager> sessionManager)
        : m_sessionManager(sessionManager) {}

    bool RegisterFeature(const FeatureInfo& info) {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        // Check if already registered
        if (m_features.find(info.id) != m_features.end()) {
            return false;
        }
        
        // Check dependencies
        for (const auto& dep : info.dependencies) {
            if (m_features.find(dep) == m_features.end()) {
                // Dependency not registered
                return false;
            }
        }
        
        FeatureInfo mutableInfo = info;
        mutableInfo.state = FeatureState::REGISTERED;
        mutableInfo.registeredAt = std::chrono::system_clock::now();
        
        m_features[info.id] = mutableInfo;
        
        // Update dependency graph
        for (const auto& dep : info.dependencies) {
            m_dependencyGraph[dep].push_back(info.id);
        }
        
        return true;
    }

    bool ActivateFeature(const std::string& featureId) {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        auto it = m_features.find(featureId);
        if (it == m_features.end()) {
            return false;
        }
        
        // Check dependencies are active
        for (const auto& dep : it->second.dependencies) {
            auto depIt = m_features.find(dep);
            if (depIt == m_features.end() || depIt->second.state != FeatureState::ACTIVE) {
                return false;
            }
        }
        
        it->second.state = FeatureState::ACTIVE;
        it->second.activatedAt = std::chrono::system_clock::now();
        
        // Activate dependent features
        auto depIt = m_dependencyGraph.find(featureId);
        if (depIt != m_dependencyGraph.end()) {
            for (const auto& dependent : depIt->second) {
                TryActivateDependent(dependent);
            }
        }
        
        return true;
    }

    bool DeactivateFeature(const std::string& featureId) {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        auto it = m_features.find(featureId);
        if (it == m_features.end()) {
            return false;
        }
        
        // Check if other features depend on this
        auto depIt = m_dependencyGraph.find(featureId);
        if (depIt != m_dependencyGraph.end()) {
            for (const auto& dependent : depIt->second) {
                auto dependentIt = m_features.find(dependent);
                if (dependentIt != m_features.end() && 
                    dependentIt->second.state == FeatureState::ACTIVE) {
                    // Deactivate dependent features first
                    DeactivateFeature(dependent);
                }
            }
        }
        
        it->second.state = FeatureState::PAUSED;
        return true;
    }

    bool UnregisterFeature(const std::string& featureId) {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        auto it = m_features.find(featureId);
        if (it == m_features.end()) {
            return false;
        }
        
        // Check if other features depend on this
        auto depIt = m_dependencyGraph.find(featureId);
        if (depIt != m_dependencyGraph.end() && !depIt->second.empty()) {
            return false; // Can't unregister if others depend on it
        }
        
        // Remove from dependency graph
        for (const auto& [id, dependents] : m_dependencyGraph) {
            auto& vec = m_dependencyGraph[id];
            vec.erase(std::remove(vec.begin(), vec.end(), featureId), vec.end());
        }
        
        m_features.erase(it);
        return true;
    }

    FeatureInfo GetFeatureInfo(const std::string& featureId) const {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        auto it = m_features.find(featureId);
        if (it != m_features.end()) {
            return it->second;
        }
        return FeatureInfo{};
    }

    std::vector<FeatureInfo> GetFeaturesByCategory(FeatureCategory category) const {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        std::vector<FeatureInfo> result;
        for (const auto& [id, info] : m_features) {
            if (info.category == category) {
                result.push_back(info);
            }
        }
        return result;
    }

    std::vector<FeatureInfo> GetActiveFeatures() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        std::vector<FeatureInfo> result;
        for (const auto& [id, info] : m_features) {
            if (info.state == FeatureState::ACTIVE) {
                result.push_back(info);
            }
        }
        return result;
    }

    std::vector<FeatureInfo> GetFeaturesByState(FeatureState state) const {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        std::vector<FeatureInfo> result;
        for (const auto& [id, info] : m_features) {
            if (info.state == state) {
                result.push_back(info);
            }
        }
        return result;
    }

    bool HasFeature(const std::string& featureId) const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_features.find(featureId) != m_features.end();
    }

    bool IsFeatureActive(const std::string& featureId) const {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        auto it = m_features.find(featureId);
        if (it != m_features.end()) {
            return it->second.state == FeatureState::ACTIVE;
        }
        return false;
    }

    void RegisterCapability(const std::string& featureId, const FeatureCapability& capability) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_capabilities[featureId][capability.name] = capability;
    }

    std::optional<FeatureCapability> GetCapability(const std::string& featureId, 
                                                     const std::string& capabilityName) const {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        auto featIt = m_capabilities.find(featureId);
        if (featIt != m_capabilities.end()) {
            auto capIt = featIt->second.find(capabilityName);
            if (capIt != featIt->second.end()) {
                return capIt->second;
            }
        }
        return std::nullopt;
    }

    std::vector<std::string> GetDependencyChain(const std::string& featureId) const {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        std::vector<std::string> chain;
        std::set<std::string> visited;
        
        BuildDependencyChain(featureId, chain, visited);
        
        return chain;
    }

    std::string GenerateRegistryReport() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        std::ostringstream report;
        report << "# Feature Registry Report\n\n";
        report << "**Total Features:** " << m_features.size() << "\n";
        report << "**Active:** " << GetActiveFeatures().size() << "\n\n";
        
        // Group by category
        std::map<FeatureCategory, std::vector<FeatureInfo>> byCategory;
        for (const auto& [id, info] : m_features) {
            byCategory[info.category].push_back(info);
        }
        
        for (const auto& [category, features] : byCategory) {
            report << "## " << CategoryToString(category) << " (" << features.size() << ")\n\n";
            
            for (const auto& feature : features) {
                report << "### " << feature.name << "\n";
                report << "- **ID:** " << feature.id << "\n";
                report << "- **Version:** " << feature.version << "\n";
                report << "- **State:** " << StateToString(feature.state) << "\n";
                report << "- **Description:** " << feature.description << "\n";
                
                if (!feature.dependencies.empty()) {
                    report << "- **Dependencies:** ";
                    for (const auto& dep : feature.dependencies) {
                        report << dep << " ";
                    }
                    report << "\n";
                }
                
                report << "\n";
            }
        }
        
        return report.str();
    }

private:
    std::shared_ptr<Core::SessionManager> m_sessionManager;
    mutable std::mutex m_mutex;
    std::map<std::string, FeatureInfo> m_features;
    std::map<std::string, std::vector<std::string>> m_dependencyGraph;
    std::map<std::string, std::map<std::string, FeatureCapability>> m_capabilities;

    void TryActivateDependent(const std::string& featureId) {
        auto it = m_features.find(featureId);
        if (it == m_features.end()) return;
        
        if (it->second.state != FeatureState::REGISTERED) return;
        
        // Check if all dependencies are active
        bool canActivate = true;
        for (const auto& dep : it->second.dependencies) {
            auto depIt = m_features.find(dep);
            if (depIt == m_features.end() || depIt->second.state != FeatureState::ACTIVE) {
                canActivate = false;
                break;
            }
        }
        
        if (canActivate) {
            it->second.state = FeatureState::ACTIVE;
            it->second.activatedAt = std::chrono::system_clock::now();
            
            // Recursively activate dependents
            auto depIt = m_dependencyGraph.find(featureId);
            if (depIt != m_dependencyGraph.end()) {
                for (const auto& dependent : depIt->second) {
                    TryActivateDependent(dependent);
                }
            }
        }
    }

    void BuildDependencyChain(const std::string& featureId, 
                             std::vector<std::string>& chain,
                             std::set<std::string>& visited) const {
        if (visited.find(featureId) != visited.end()) return;
        visited.insert(featureId);
        
        auto it = m_features.find(featureId);
        if (it == m_features.end()) return;
        
        for (const auto& dep : it->second.dependencies) {
            BuildDependencyChain(dep, chain, visited);
        }
        
        chain.push_back(featureId);
    }

    std::string CategoryToString(FeatureCategory category) {
        switch (category) {
            case FeatureCategory::CORE: return "Core";
            case FeatureCategory::AI: return "AI";
            case FeatureCategory::SECURITY: return "Security";
            case FeatureCategory::PERFORMANCE: return "Performance";
            case FeatureCategory::QUALITY: return "Quality";
            case FeatureCategory::INTEGRATION: return "Integration";
            case FeatureCategory::UI: return "UI";
            case FeatureCategory::CUSTOM: return "Custom";
            default: return "Unknown";
        }
    }

    std::string StateToString(FeatureState state) {
        switch (state) {
            case FeatureState::UNREGISTERED: return "Unregistered";
            case FeatureState::REGISTERED: return "Registered";
            case FeatureState::INITIALIZING: return "Initializing";
            case FeatureState::ACTIVE: return "Active";
            case FeatureState::PAUSED: return "Paused";
            case FeatureState::ERROR: return "Error";
            case FeatureState::DISABLED: return "Disabled";
            default: return "Unknown";
        }
    }
};

} // namespace RawrXD::Registry
