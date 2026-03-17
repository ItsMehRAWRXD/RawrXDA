#pragma once

#include <string>
#include <vector>
#include <map>
#include <functional>
#include <memory>
#include <unordered_map>
#include <cstdint>

namespace RawrXD::Agentic::Wiring {

// Capability interface
class ICapability {
public:
    virtual ~ICapability() = default;
    virtual const std::string& getName() const = 0;
    virtual uint32_t getVersion() const = 0;
    virtual bool initialize() = 0;
    virtual void shutdown() = 0;
    virtual void* getInterface(const std::string& interfaceName) = 0;
};

// Capability factory function
using CapabilityFactory = std::function<std::unique_ptr<ICapability>()>;

// Capability registration info
struct CapabilityRegistration {
    std::string name;
    uint32_t version;
    CapabilityFactory factory;
    std::vector<std::string> dependencies;
    bool enabled = true;
    std::unique_ptr<ICapability> instance;
};

// Dependency graph for capability resolution
class DependencyGraph {
public:
    void addCapability(const std::string& name, const std::vector<std::string>& deps);
    std::vector<std::string> resolveOrder() const;
    bool hasCycles() const;
    
private:
    std::unordered_map<std::string, std::vector<std::string>> graph_;
    
    bool detectCycle(const std::string& node, std::unordered_map<std::string, bool>& visited, 
                     std::unordered_map<std::string, bool>& recStack) const;
};

// Feature flags for runtime toggles
class FeatureFlags {
public:
    static FeatureFlags& instance();
    
    void set(const std::string& feature, bool enabled);
    bool get(const std::string& feature, bool defaultValue = false) const;
    void loadFromFile(const std::string& configPath);
    void saveToFile(const std::string& configPath) const;
    
private:
    std::unordered_map<std::string, bool> flags_;
};

// Main capability router
class CapabilityRouter {
public:
    static CapabilityRouter& instance();
    
    // Capability registration
    bool registerCapability(const std::string& name, uint32_t version, 
                           CapabilityFactory factory, const std::vector<std::string>& deps = {});
    
    // Capability access
    ICapability* getCapability(const std::string& name);
    template<typename T>
    T* getCapabilityAs(const std::string& name) {
        auto cap = getCapability(name);
        return cap ? static_cast<T*>(cap->getInterface(typeid(T).name())) : nullptr;
    }
    
    // Lifecycle management
    bool initializeAll();
    void shutdownAll();
    
    // Feature flag integration
    void setFeatureFlag(const std::string& feature, bool enabled);
    bool isFeatureEnabled(const std::string& feature) const;
    
    // Dependency resolution
    std::vector<std::string> getCapabilityOrder() const;
    
    // Statistics
    size_t getRegisteredCount() const { return registrations_.size(); }
    size_t getInitializedCount() const { return initializedCount_; }
    
private:
    CapabilityRouter() = default;
    
    std::unordered_map<std::string, CapabilityRegistration> registrations_;
    DependencyGraph dependencyGraph_;
    size_t initializedCount_ = 0;
    
    bool initializeCapability(const std::string& name);
    void shutdownCapability(const std::string& name);
};

} // namespace RawrXD::Agentic::Wiring