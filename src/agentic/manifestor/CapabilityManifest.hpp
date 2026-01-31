#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <functional>
#include <memory>

namespace RawrXD::Agentic::Manifestor {

/// Capability version format: major.minor.patch
struct CapabilityVersion {
    uint32_t major = 1;
    uint32_t minor = 0;
    uint32_t patch = 0;
    
    bool isCompatible(const CapabilityVersion& required) const {
        return major == required.major && minor >= required.minor;
    }
    
    std::string toString() const {
        return std::to_string(major) + "." + std::to_string(minor) + "." + std::to_string(patch);
    }
};

/// Capability metadata discovered from PE exports
struct CapabilityDescriptor {
    std::string name;              // e.g., "vulkan_renderer", "tool_parser"
    CapabilityVersion version;
    std::string sourceModule;      // DLL/EXE path
    uintptr_t factoryAddress;      // Function pointer to create instance
    
    // Dependencies
    std::vector<std::string> requiredCapabilities;
    std::vector<std::string> optionalCapabilities;
    
    // Metadata
    std::string description;
    std::string author;
    uint64_t buildTimestamp;
    
    // Runtime state
    bool isLoaded = false;
    bool isActive = false;
    void* instance = nullptr;
};

/// Wiring graph node
struct WiringNode {
    std::string capabilityName;
    std::vector<std::string> dependsOn;     // Outgoing edges
    std::vector<std::string> requiredBy;    // Incoming edges
    int depth = -1;                         // Topological depth
};

/// Full capability manifest
class CapabilityManifest {
public:
    std::vector<CapabilityDescriptor> capabilities;
    std::vector<WiringNode> wiringGraph;
    
    /// Add capability with dependency resolution
    bool addCapability(const CapabilityDescriptor& cap);
    
    /// Get capability by name
    const CapabilityDescriptor* getCapability(const std::string& name) const;
    CapabilityDescriptor* getCapability(const std::string& name);
    
    /// Build dependency graph and resolve load order
    std::vector<std::string> getLoadOrder();
    
    /// Detect circular dependencies
    bool hasCircularDependencies() const;
    
    /// Export to JSON
    std::string toJson() const;
    
    /// Import from JSON
    static CapabilityManifest fromJson(const std::string& json);
    
    /// Generate GraphViz DOT format for visualization
    std::string toDot() const;
    
private:
    void computeDepth(const std::string& name, int currentDepth);
};

/// Factory function signature for capabilities
using CapabilityFactory = std::function<void*(const char* config)>;

} // namespace RawrXD::Agentic::Manifestor
