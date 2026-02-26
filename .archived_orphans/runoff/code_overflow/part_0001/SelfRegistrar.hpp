/**
 * @file SelfRegistrar.hpp
 * @brief Self-manifesting capability discovery (stub for patchable build).
 */
#pragma once

#include <string>
#include <vector>
#include <map>
#include <filesystem>

namespace RawrXD::Agentic::Manifestor {

struct CapabilityManifest {
    std::string name;
    uint32_t version;
    std::filesystem::path path;
    void* factory;
    std::vector<std::string> dependencies;
    std::map<std::string, std::string> metadata;
    bool enabled = true;
};

class SelfRegistrar {
public:
    SelfRegistrar();
    std::vector<CapabilityManifest> scanBuildDirectory(const std::filesystem::path& buildDir);
    bool generateWiringDiagram(const std::filesystem::path& outputPath);
    bool generateReverseEngineeringPlan(const std::filesystem::path& outputPath);
    const std::vector<CapabilityManifest>& getCapabilities() const { return capabilities_; }
private:
    std::vector<CapabilityManifest> capabilities_;
    CapabilityManifest discoverCapability(const std::filesystem::path& filePath);
    bool isCapabilityFile(const std::filesystem::path& filePath) const;
    std::string extractCapabilityName(const std::string& exportName) const;
    std::vector<std::string> analyzeDependencies(const std::filesystem::path& filePath);
};

} // namespace RawrXD::Agentic::Manifestor
