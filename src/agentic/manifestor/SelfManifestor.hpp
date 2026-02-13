#pragma once

#include <string>
#include <vector>
#include <map>
#include <filesystem>
#include <memory>

namespace RawrXD::Agentic::Manifestor {

// Capability metadata discovered from build artifacts
struct CapabilityManifest {
    std::string name;
    uint32_t version;
    std::filesystem::path path;
    void* factory;
    std::vector<std::string> dependencies;
    std::map<std::string, std::string> metadata;
    bool enabled = true;
};

// PE parser for analyzing executables and DLLs
class PEParser {
public:
    static std::unique_ptr<PEParser> load(const std::filesystem::path& filePath);
    
    struct Export {
        std::string name;
        uint32_t ordinal;
        void* address;
        std::string type;
    };
    
    std::vector<Export> getExports() const;
    std::vector<std::string> getImports() const;
    std::string getArchitecture() const;
    bool isValid() const { return valid_; }
    
private:
    PEParser() = default;
    bool valid_ = false;
    std::vector<uint8_t> data_;
    
    // PE parsing internals
    bool parsePEHeaders();
    void* getExportAddress(const std::string& name) const;
};

// Main self-manifesting engine
class SelfManifestor {
public:
    SelfManifestor();
    
    // Scan build directory for capabilities
    std::vector<CapabilityManifest> scanBuildDirectory(const std::filesystem::path& buildDir);
    
    // Generate wiring diagram JSON
    bool generateWiringDiagram(const std::filesystem::path& outputPath);
    
    // Generate reverse engineering plan
    bool generateReverseEngineeringPlan(const std::filesystem::path& outputPath);
    
    // Get discovered capabilities
    const std::vector<CapabilityManifest>& getCapabilities() const { return capabilities_; }
    
private:
    std::vector<CapabilityManifest> capabilities_;
    
    // Capability discovery methods
    CapabilityManifest discoverCapability(const std::filesystem::path& filePath);
    bool isCapabilityFile(const std::filesystem::path& filePath) const;
    std::string extractCapabilityName(const std::string& exportName) const;
    
    // Dependency analysis
    std::vector<std::string> analyzeDependencies(const std::filesystem::path& filePath);
};

} // namespace RawrXD::Agentic::Manifestor