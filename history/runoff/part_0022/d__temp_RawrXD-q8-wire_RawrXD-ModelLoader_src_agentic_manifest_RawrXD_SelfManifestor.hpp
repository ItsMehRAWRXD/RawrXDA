// RawrXD_SelfManifestor.hpp
// PE parsing + capability export discovery for autonomous wiring

#pragma once
#include <windows.h>
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <expected>

namespace RawrXD::Agentic::Manifest {

struct CapabilityExport {
    std::string exportName;
    uint64_t exportAddress;
    uint32_t ordinal;
    uint32_t capabilityFlags;
    std::string description;
};

struct ToolDescriptor {
    uint32_t toolId;
    std::string toolName;
    std::string signature;
    std::vector<std::string> inputParameters;
    std::vector<std::string> outputParameters;
    uint32_t priority;
    bool isAsynchronous;
};

enum class ManifestError {
    Success = 0,
    InvalidPEFormat,
    ExportTableNotFound,
    NoCapabilityExports,
    ParseFailure,
    SymbolResolutionFailed
};

class SelfManifestor {
public:
    static std::expected<std::unique_ptr<SelfManifestor>, ManifestError>
        scanCurrentProcess();
    
    static std::expected<std::unique_ptr<SelfManifestor>, ManifestError>
        scanModule(HMODULE hModule);
    
    ~SelfManifestor() = default;
    
    SelfManifestor(const SelfManifestor&) = delete;
    SelfManifestor& operator=(const SelfManifestor&) = delete;
    
    const std::vector<CapabilityExport>& getCapabilities() const;
    const std::vector<ToolDescriptor>& getTools() const;
    
    std::expected<uint64_t, ManifestError>
        resolveCapability(const std::string& name) const;
    
    std::expected<const ToolDescriptor*, ManifestError>
        resolveTool(uint32_t toolId) const;
    
    void printManifest() const;
    
    struct Stats {
        uint32_t totalExports;
        uint32_t capabilityExports;
        uint32_t toolDescriptors;
        uint64_t exportTableSize;
    };
    Stats getStats() const;

private:
    SelfManifestor() = default;
    
    bool initialize(HMODULE hModule);
    bool parseExportTable(HMODULE hModule);
    bool discoverToolDescriptors();
    
    std::vector<CapabilityExport> capabilities_;
    std::vector<ToolDescriptor> tools_;
    std::unordered_map<std::string, uint64_t> capabilityMap_;
    std::unordered_map<uint32_t, ToolDescriptor> toolMap_;
    
    HMODULE currentModule_ = nullptr;
    PIMAGE_NT_HEADERS ntHeaders_ = nullptr;
    PIMAGE_EXPORT_DIRECTORY exportDir_ = nullptr;
};

SelfManifestor& getGlobalManifestor();

} // namespace
