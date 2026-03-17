// RawrXD_SelfManifestor.hpp
// PE Scanner + Capability Discovery

#pragma once
#include <windows.h>
#include <string>
#include <vector>
#include <filesystem>
#include <functional>
#include <expected>

namespace RawrXD::Agentic::Manifestor {

struct CapabilityExport {
    std::string name;
    uint32_t version;
    uint64_t rva;
    void* factory;
    std::string sourceModule;
    uint32_t timestamp;
    std::vector<uint8_t> hash;
};

struct BuildArtifact {
    std::filesystem::path path;
    std::string type;
    uint64_t size;
    FILETIME lastWrite;
    bool hasDebugInfo;
    bool hasCapabilityExports;
};

struct CapabilityManifest {
    std::string schemaVersion = "2.0";
    uint64_t generatedAt;
    std::vector<CapabilityExport> capabilities;
    std::vector<BuildArtifact> artifacts;
    std::string buildConfig;
    std::string targetArch;
};

enum class ManifestError {
    PathNotFound,
    PEReadFailure,
    NoCapabilitiesFound,
    VersionMismatch,
    IntegrityFailure
};

class SelfManifestor {
public:
    SelfManifestor();
    ~SelfManifestor();

    std::expected<CapabilityManifest, ManifestError> 
        scanBuildDirectory(const std::filesystem::path& buildRoot);
    
    std::expected<std::vector<CapabilityExport>, ManifestError>
        scanPE(const std::filesystem::path& pePath);
    
    bool hasModuleChanged(const CapabilityExport& cap) const;
    
    bool generateWiringDiagram(const std::filesystem::path& outputPath,
                               const CapabilityManifest& manifest);
    
    bool generateCanonicalPlans(const std::filesystem::path& repoRoot,
                                const CapabilityManifest& manifest);

    using ProgressCallback = std::function<void(const std::string& phase, 
                                                size_t current, 
                                                size_t total)>;
    void setProgressCallback(ProgressCallback cb) { progressCb_ = cb; }

private:
    ProgressCallback progressCb_;
    
    std::expected<PIMAGE_NT_HEADERS64, ManifestError> 
        mapPE(const std::filesystem::path& path, 
              HANDLE* hFile, 
              HANDLE* hMapping,
              void** baseAddr);
    
    void unmapPE(HANDLE hFile, HANDLE hMapping, void* baseAddr);
    
    std::vector<CapabilityExport> parseExportDirectory(
        void* baseAddr, 
        PIMAGE_NT_HEADERS64 ntHeaders,
        const std::filesystem::path& sourcePath);
    
    std::string demangleCapabilityName(const std::string& rawExport);
    uint32_t extractVersionFromResources(void* baseAddr);
    
    std::vector<BuildArtifact> scanArtifacts(
        const std::filesystem::path& buildRoot);
    
    std::string detectBuildConfig(const BuildArtifact& artifact);
    
    std::vector<uint8_t> computeExportHash(void* baseAddr, uint64_t rva, 
                                           size_t length);
};

} // namespace
