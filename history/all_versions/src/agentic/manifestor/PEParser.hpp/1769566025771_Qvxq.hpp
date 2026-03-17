#pragma once

#include <windows.h>
#include <string>
#include <vector>
#include <cstdint>

namespace RawrXD::Agentic::Manifestor {

/// PE export entry
struct PEExport {
    std::string name;
    uint32_t ordinal;
    uintptr_t address;          // RVA
    uintptr__t absoluteAddress; // Resolved address
    bool isForwarder = false;
    std::string forwarderName;
};

/// PE import entry
struct PEImport {
    std::string dllName;
    std::string functionName;
    uint32_t ordinal;
    uintptr_t thunkAddress;
};

/// PE section info
struct PESection {
    std::string name;
    uint32_t virtualAddress;
    uint32_t virtualSize;
    uint32_t rawDataOffset;
    uint32_t rawDataSize;
    uint32_t characteristics;
    
    bool isExecutable() const { return (characteristics & IMAGE_SCN_MEM_EXECUTE) != 0; }
    bool isWritable() const { return (characteristics & IMAGE_SCN_MEM_WRITE) != 0; }
    bool isReadable() const { return (characteristics & IMAGE_SCN_MEM_READ) != 0; }
};

/// Minimal PE parser for capability discovery
class PEParser {
public:
    explicit PEParser(const std::string& filePath);
    ~PEParser();
    
    /// Load PE from file
    bool load();
    
    /// Check if file is valid PE
    bool isValidPE() const { return m_dosHeader != nullptr && m_ntHeaders != nullptr; }
    
    /// Get module base address (if loaded)
    uintptr_t getModuleBase() const { return reinterpret_cast<uintptr_t>(m_moduleBase); }
    
    /// Parse export table
    std::vector<PEExport> getExports() const;
    
    /// Parse import table
    std::vector<PEImport> getImports() const;
    
    /// Get sections
    std::vector<PESection> getSections() const;
    
    /// Find export by name
    const PEExport* findExport(const std::string& name) const;
    
    /// Get timestamp
    uint32_t getTimestamp() const;
    
    /// Get image size
    uint32_t getImageSize() const;
    
    /// RVA to file offset
    uint32_t rvaToFileOffset(uint32_t rva) const;
    
    /// Find capability exports (prefixed with "capability_")
    std::vector<PEExport> findCapabilityExports() const;
    
private:
    std::string m_filePath;
    HANDLE m_fileHandle = INVALID_HANDLE_VALUE;
    HANDLE m_mappingHandle = nullptr;
    void* m_moduleBase = nullptr;
    size_t m_fileSize = 0;
    
    IMAGE_DOS_HEADER* m_dosHeader = nullptr;
    IMAGE_NT_HEADERS* m_ntHeaders = nullptr;
    
    void cleanup();
};

} // namespace RawrXD::Agentic::Manifestor
