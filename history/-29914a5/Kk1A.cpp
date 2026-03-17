// RawrXD_SelfManifestor.cpp
// Implementation of PE scanning and capability discovery

#include "RawrXD_SelfManifestor.hpp"
#include <spdlog/spdlog.h>
#include <algorithm>
#include <iostream>

namespace RawrXD::Agentic::Manifest {

std::expected<std::unique_ptr<SelfManifestor>, ManifestError>
SelfManifestor::scanCurrentProcess() {
    HMODULE hModule = GetModuleHandleW(nullptr);
    if (!hModule) {
        return std::unexpected(ManifestError::ParseFailure);
    }
    return scanModule(hModule);
}

std::expected<std::unique_ptr<SelfManifestor>, ManifestError>
SelfManifestor::scanModule(HMODULE hModule) {
    auto manifest = std::make_unique<SelfManifestor>();
    if (!manifest->initialize(hModule)) {
        return std::unexpected(ManifestError::ParseFailure);
    }
    return manifest;
}

bool SelfManifestor::initialize(HMODULE hModule) {
    currentModule_ = hModule;
    
    PIMAGE_DOS_HEADER dosHeader = (PIMAGE_DOS_HEADER)hModule;
    if (dosHeader->e_magic != IMAGE_DOS_SIGNATURE) {
        spdlog::error("Invalid DOS signature");
        return false;
    }
    
    ntHeaders_ = (PIMAGE_NT_HEADERS)((uint64_t)hModule + dosHeader->e_lfanew);
    if (ntHeaders_->Signature != IMAGE_NT_SIGNATURE) {
        spdlog::error("Invalid NT signature");
        return false;
    }
    
    const IMAGE_DATA_DIRECTORY& exportDataDir = 
        ntHeaders_->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
    
    if (exportDataDir.Size == 0) {
        spdlog::warn("No export table found");
        return false;
    }
    
    exportDir_ = (PIMAGE_EXPORT_DIRECTORY)((uint64_t)hModule + exportDataDir.VirtualAddress);
    
    if (!parseExportTable(hModule)) {
        return false;
    }
    
    discoverToolDescriptors();
    
    spdlog::info("Manifest scan complete: {} capabilities, {} tools",
                 capabilities_.size(), tools_.size());
    return true;
}

bool SelfManifestor::parseExportTable(HMODULE hModule) {
    if (!exportDir_) return false;
    
    uint32_t* nameTable = (uint32_t*)((uint64_t)hModule + exportDir_->AddressOfNames);
    uint16_t* ordinalTable = (uint16_t*)((uint64_t)hModule + exportDir_->AddressOfNameOrdinals);
    uint32_t* addressTable = (uint32_t*)((uint64_t)hModule + exportDir_->AddressOfFunctions);
    
    for (uint32_t i = 0; i < exportDir_->NumberOfNames; i++) {
        const char* exportName = (const char*)((uint64_t)hModule + nameTable[i]);
        uint16_t ordinal = ordinalTable[i];
        uint64_t exportAddress = (uint64_t)hModule + addressTable[ordinal];
        
        std::string nameStr(exportName);
        
        if (nameStr.find("capability_") != std::string::npos) {
            CapabilityExport cap{
                .exportName = nameStr,
                .exportAddress = exportAddress,
                .ordinal = ordinal,
                .capabilityFlags = 0,
                .description = "Exported capability"
            };
            
            size_t pos = nameStr.find("capability_");
            size_t nameStart = pos + 11;
            size_t nameEnd = nameStr.find('_', nameStart);
            if (nameEnd == std::string::npos) nameEnd = nameStr.length();
            
            std::string capName = nameStr.substr(nameStart, nameEnd - nameStart);
            std::transform(capName.begin(), capName.end(), capName.begin(), ::toupper);
            
            if (capName == "FILE_OPS") cap.capabilityFlags |= 0x0001;
            else if (capName == "EDIT") cap.capabilityFlags |= 0x0002;
            else if (capName == "VIEW") cap.capabilityFlags |= 0x0004;
            else if (capName == "NAVIGATION") cap.capabilityFlags |= 0x0008;
            else if (capName == "EXEC") cap.capabilityFlags |= 0x0010;
            
            capabilities_.push_back(cap);
            capabilityMap_[nameStr] = exportAddress;
            
            spdlog::debug("Found capability: {} at 0x{:X}", nameStr, exportAddress);
        }
    }
    
    return !capabilities_.empty();
}

bool SelfManifestor::discoverToolDescriptors() {
    for (size_t i = 0; i < capabilities_.size(); i++) {
        uint32_t toolId = 5000 + i;
        ToolDescriptor tool{
            .toolId = toolId,
            .toolName = capabilities_[i].exportName,
            .signature = "",
            .priority = 50,
            .isAsynchronous = false
        };
        
        tool.inputParameters.push_back("context");
        tool.inputParameters.push_back("parameters");
        tool.outputParameters.push_back("result");
        tool.outputParameters.push_back("status");
        
        tools_.push_back(tool);
        toolMap_[toolId] = tool;
    }
    
    return true;
}

const std::vector<CapabilityExport>& SelfManifestor::getCapabilities() const {
    return capabilities_;
}

const std::vector<ToolDescriptor>& SelfManifestor::getTools() const {
    return tools_;
}

std::expected<uint64_t, ManifestError>
SelfManifestor::resolveCapability(const std::string& name) const {
    auto it = capabilityMap_.find(name);
    if (it == capabilityMap_.end()) {
        return std::unexpected(ManifestError::SymbolResolutionFailed);
    }
    return it->second;
}

std::expected<const ToolDescriptor*, ManifestError>
SelfManifestor::resolveTool(uint32_t toolId) const {
    auto it = toolMap_.find(toolId);
    if (it == toolMap_.end()) {
        return std::unexpected(ManifestError::SymbolResolutionFailed);
    }
    return &it->second;
}

void SelfManifestor::printManifest() const {
    std::cout << "\n=== RawrXD Capability Manifest ===\n";
    std::cout << "Capabilities Found: " << capabilities_.size() << "\n";
    
    for (const auto& cap : capabilities_) {
        std::cout << "  - " << cap.exportName << " @ 0x" << std::hex << cap.exportAddress << "\n";
    }
    
    std::cout << "\nTools Discovered: " << tools_.size() << "\n";
    for (const auto& tool : tools_) {
        std::cout << "  - [" << std::dec << tool.toolId << "] " << tool.toolName << "\n";
    }
}

SelfManifestor::Stats SelfManifestor::getStats() const {
    return {
        (uint32_t)(exportDir_ ? exportDir_->NumberOfNames : 0),
        (uint32_t)capabilities_.size(),
        (uint32_t)tools_.size(),
        (uint64_t)(exportDir_ ? exportDir_->AddressOfNames : 0)
    };
}

static SelfManifestor* g_manifestor = nullptr;

SelfManifestor& getGlobalManifestor() {
    if (!g_manifestor) {
        auto result = SelfManifestor::scanCurrentProcess();
        if (result) {
            g_manifestor = result.value().release();
        }
    }
    return *g_manifestor;
}

}
