// RawrXD_SelfManifestor.cpp
// PE Scanner and Capability Discovery Implementation

#include "RawrXD_SelfManifestor.hpp"
#include <algorithm>
#include <fstream>
#include <sstream>
#include <cctype>
#include <cstring>

namespace RawrXD::Agentic::Manifestor {

using namespace std::filesystem;

SelfManifestor::SelfManifestor() : progressCb_(nullptr) {}

SelfManifestor::~SelfManifestor() = default;

std::expected<CapabilityManifest, ManifestError>
SelfManifestor::scanBuildDirectory(const path& buildRoot) {
    if (!exists(buildRoot) || !is_directory(buildRoot)) {
        return std::unexpected(ManifestError::PathNotFound);
    }
    
    CapabilityManifest manifest;
    manifest.generatedAt = std::time(nullptr);
    manifest.buildConfig = detectBuildConfig(BuildArtifact{});
    manifest.targetArch = "x64";
    
    try {
        // Scan for artifacts
        auto artifacts = scanArtifacts(buildRoot);
        manifest.artifacts = artifacts;
        
        if (progressCb_) {
            progressCb_("Scanning PE files", 0, artifacts.size());
        }
        
        // Scan each PE file for capabilities
        size_t scanned = 0;
        for (const auto& artifact : artifacts) {
            if (artifact.type == "dll" || artifact.type == "exe") {
                auto result = scanPE(artifact.path);
                if (result) {
                    for (auto& cap : result.value()) {
                        cap.sourceModule = artifact.path.filename().string();
                        manifest.capabilities.push_back(std::move(cap));
                    }
                }
            }
            
            if (progressCb_) {
                progressCb_("Scanning PE files", ++scanned, artifacts.size());
            }
        }
        
        if (manifest.capabilities.empty()) {
            return std::unexpected(ManifestError::NoCapabilitiesFound);
        }
        
        return manifest;
        
    } catch (const std::exception& e) {
        return std::unexpected(ManifestError::PathNotFound);
    }
}

std::expected<std::vector<CapabilityExport>, ManifestError>
SelfManifestor::scanPE(const path& pePath) {
    std::vector<CapabilityExport> capabilities;
    
    HANDLE hFile = INVALID_HANDLE_VALUE;
    HANDLE hMapping = INVALID_HANDLE_VALUE;
    void* baseAddr = nullptr;
    
    try {
        // Map PE file
        auto nt_result = mapPE(pePath, &hFile, &hMapping, &baseAddr);
        if (!nt_result) {
            return std::unexpected(nt_result.error());
        }
        
        PIMAGE_NT_HEADERS64 ntHeaders = nt_result.value();
        
        // Parse export directory
        capabilities = parseExportDirectory(baseAddr, ntHeaders, pePath);
        
        // Extract version info
        for (auto& cap : capabilities) {
            cap.version = extractVersionFromResources(baseAddr);
            cap.timestamp = ntHeaders->FileHeader.TimeDateStamp;
            cap.hash = computeExportHash(baseAddr, cap.rva, 
                                        64);  // Standard export size
        }
        
        unmapPE(hFile, hMapping, baseAddr);
        return capabilities;
        
    } catch (...) {
        if (baseAddr) unmapPE(hFile, hMapping, baseAddr);
        return std::unexpected(ManifestError::PEReadFailure);
    }
}

std::expected<PIMAGE_NT_HEADERS64, ManifestError>
SelfManifestor::mapPE(const path& path, HANDLE* hFile, 
                     HANDLE* hMapping, void** baseAddr) {
    *hFile = CreateFileW(
        path.c_str(),
        GENERIC_READ,
        FILE_SHARE_READ,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        nullptr
    );
    
    if (*hFile == INVALID_HANDLE_VALUE) {
        return std::unexpected(ManifestError::PathNotFound);
    }
    
    *hMapping = CreateFileMappingW(
        *hFile,
        nullptr,
        PAGE_READONLY,
        0, 0,
        nullptr
    );
    
    if (!*hMapping) {
        CloseHandle(*hFile);
        return std::unexpected(ManifestError::PEReadFailure);
    }
    
    *baseAddr = MapViewOfFile(*hMapping, FILE_MAP_READ, 0, 0, 0);
    if (!*baseAddr) {
        CloseHandle(*hMapping);
        CloseHandle(*hFile);
        return std::unexpected(ManifestError::PEReadFailure);
    }
    
    // Validate PE signature
    PIMAGE_DOS_HEADER dosHeader = static_cast<PIMAGE_DOS_HEADER>(*baseAddr);
    if (dosHeader->e_magic != IMAGE_DOS_SIGNATURE) {
        UnmapViewOfFile(*baseAddr);
        CloseHandle(*hMapping);
        CloseHandle(*hFile);
        return std::unexpected(ManifestError::PEReadFailure);
    }
    
    PIMAGE_NT_HEADERS64 ntHeaders = 
        reinterpret_cast<PIMAGE_NT_HEADERS64>(
            reinterpret_cast<uint8_t*>(*baseAddr) + dosHeader->e_lfanew
        );
    
    if (ntHeaders->Signature != IMAGE_NT_SIGNATURE) {
        UnmapViewOfFile(*baseAddr);
        CloseHandle(*hMapping);
        CloseHandle(*hFile);
        return std::unexpected(ManifestError::PEReadFailure);
    }
    
    return ntHeaders;
}

void SelfManifestor::unmapPE(HANDLE hFile, HANDLE hMapping, void* baseAddr) {
    if (baseAddr) UnmapViewOfFile(baseAddr);
    if (hMapping) CloseHandle(hMapping);
    if (hFile != INVALID_HANDLE_VALUE) CloseHandle(hFile);
}

std::vector<CapabilityExport> SelfManifestor::parseExportDirectory(
    void* baseAddr, PIMAGE_NT_HEADERS64 ntHeaders, const path& sourcePath) {
    
    std::vector<CapabilityExport> exports;
    
    // Locate export directory
    DWORD expDir = ntHeaders->OptionalHeader
        .DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress;
    
    if (!expDir) return exports;  // No exports
    
    PIMAGE_EXPORT_DIRECTORY exportDir =
        reinterpret_cast<PIMAGE_EXPORT_DIRECTORY>(
            reinterpret_cast<uint8_t*>(baseAddr) + expDir
        );
    
    DWORD* nameRvas = reinterpret_cast<DWORD*>(
        reinterpret_cast<uint8_t*>(baseAddr) + exportDir->AddressOfNames
    );
    
    DWORD* funcRvas = reinterpret_cast<DWORD*>(
        reinterpret_cast<uint8_t*>(baseAddr) + exportDir->AddressOfFunctions
    );
    
    WORD* ordinals = reinterpret_cast<WORD*>(
        reinterpret_cast<uint8_t*>(baseAddr) + 
        exportDir->AddressOfNameOrdinals
    );
    
    // Iterate through exports
    for (DWORD i = 0; i < exportDir->NumberOfNames; i++) {
        const char* exportName = reinterpret_cast<const char*>(
            reinterpret_cast<uint8_t*>(baseAddr) + nameRvas[i]
        );
        
        // Check for capability_ prefix
        if (strncmp(exportName, "capability_", 11) == 0) {
            WORD ordinal = ordinals[i];
            DWORD funcRva = funcRvas[ordinal];
            
            CapabilityExport cap;
            cap.name = demangleCapabilityName(exportName);
            cap.rva = funcRva;
            cap.factory = reinterpret_cast<uint8_t*>(baseAddr) + funcRva;
            cap.sourceModule = sourcePath.filename().string();
            
            exports.push_back(cap);
        }
    }
    
    return exports;
}

std::string SelfManifestor::demangleCapabilityName(const std::string& rawExport) {
    // capability_ClassName_MethodName -> ClassName::MethodName
    std::string demangled = rawExport.substr(11);  // Skip "capability_"
    
    size_t pos = demangled.find('_');
    if (pos != std::string::npos) {
        demangled[pos] = ':';
        if (pos + 1 < demangled.length()) {
            demangled.insert(pos + 1, ":");
        }
    }
    
    return demangled;
}

uint32_t SelfManifestor::extractVersionFromResources(void* baseAddr) {
    // Simplified: return hardcoded version for now
    // Full implementation would parse resource section
    return 0x01000000;  // v1.0
}

std::vector<BuildArtifact> SelfManifestor::scanArtifacts(const path& buildRoot) {
    std::vector<BuildArtifact> artifacts;
    
    for (const auto& entry : recursive_directory_iterator(buildRoot)) {
        if (!is_regular_file(entry)) continue;
        
        std::string ext = entry.path().extension().string();
        std::string type;
        
        if (ext == ".dll") type = "dll";
        else if (ext == ".exe") type = "exe";
        else if (ext == ".lib") type = "lib";
        else if (ext == ".obj") type = "obj";
        else if (ext == ".asm") type = "asm";
        else continue;
        
        BuildArtifact artifact;
        artifact.path = entry.path();
        artifact.type = type;
        artifact.size = file_size(entry);
        artifact.lastWrite = to_time_t(last_write_time(entry));
        artifact.hasDebugInfo = (ext == ".pdb");
        artifact.hasCapabilityExports = (type == "dll" || type == "exe");
        
        artifacts.push_back(artifact);
    }
    
    return artifacts;
}

std::string SelfManifestor::detectBuildConfig(const BuildArtifact& artifact) {
    return "Release";  // Simplified
}

std::vector<uint8_t> SelfManifestor::computeExportHash(
    void* baseAddr, uint64_t rva, size_t length) {
    
    std::vector<uint8_t> hash(32);  // SHA256 size
    
    // Simplified: just fill with deterministic pattern
    uint8_t* data = reinterpret_cast<uint8_t*>(baseAddr) + rva;
    for (size_t i = 0; i < 32 && i < length; i++) {
        hash[i] = data[i];
    }
    
    return hash;
}

bool SelfManifestor::hasModuleChanged(const CapabilityExport& cap) const {
    // Check file modification time vs stored timestamp
    // Simplified implementation
    return false;
}

bool SelfManifestor::generateWiringDiagram(const path& outputPath,
                                          const CapabilityManifest& manifest) {
    std::ofstream out(outputPath);
    if (!out) return false;
    
    out << "# RawrXD Capability Wiring Diagram\n\n";
    out << "## Manifest Information\n";
    out << "- Schema Version: " << manifest.schemaVersion << "\n";
    out << "- Generated At: " << manifest.generatedAt << "\n";
    out << "- Build Config: " << manifest.buildConfig << "\n";
    out << "- Target Architecture: " << manifest.targetArch << "\n\n";
    
    out << "## Available Capabilities (" << manifest.capabilities.size() << ")\n\n";
    
    for (const auto& cap : manifest.capabilities) {
        out << "### " << cap.name << "\n";
        out << "- Module: " << cap.sourceModule << "\n";
        out << "- Version: 0x" << std::hex << cap.version << std::dec << "\n";
        out << "- RVA: 0x" << std::hex << cap.rva << std::dec << "\n";
        out << "\n";
    }
    
    return true;
}

bool SelfManifestor::generateCanonicalPlans(const path& repoRoot,
                                           const CapabilityManifest& manifest) {
    // Generate canonical execution plans based on discovered capabilities
    // This would create optimized dispatch tables
    // Simplified implementation
    return true;
}

}  // namespace RawrXD::Agentic::Manifestor
