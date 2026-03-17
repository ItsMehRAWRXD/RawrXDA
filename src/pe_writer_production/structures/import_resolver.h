// ============================================================================
// Import Resolver - Handles PE Import Table Creation and Resolution
// Supports multiple DLLs and symbols with proper IAT/ILT generation
// ============================================================================

#pragma once

#include "../pe_writer.h"
#include <vector>
#include <string>
#include <unordered_map>
#include <functional>

namespace pewriter {

// ============================================================================
// IMPORT STRUCTURES
// ============================================================================

#pragma pack(push, 1)

struct IMAGE_IMPORT_DESCRIPTOR {
    uint32_t OriginalFirstThunk;  // RVA to ILT
    uint32_t TimeDateStamp;
    uint32_t ForwarderChain;
    uint32_t Name;                // RVA to DLL name
    uint32_t FirstThunk;          // RVA to IAT
};

struct IMAGE_IMPORT_BY_NAME {
    uint16_t Hint;
    char Name[1];  // Variable length
};

#pragma pack(pop)

// ============================================================================
// IMPORT INFORMATION
// ============================================================================

struct ImportInfo {
    std::string dllName;
    std::vector<std::string> symbols;
    std::vector<uint16_t> hints;
    uint32_t iltRVA;
    uint32_t iatRVA;
    uint32_t nameRVA;
};

// ============================================================================
// IMPORT RESOLVER CLASS
// ============================================================================

class ImportResolver {
public:
    ImportResolver();
    ~ImportResolver() = default;

    void setLibraries(const std::vector<std::string>& libraries);
    bool addImport(const std::string& dllName, const std::string& symbol, uint16_t hint = 0);
    bool resolve();

    // Get import table data
    const std::vector<uint8_t>& getImportTable() const;
    uint32_t getImportTableRVA() const;
    uint32_t getImportTableSize() const;

    // Get IAT data
    const std::vector<uint8_t>& getIAT() const;
    uint32_t getIATRVA() const;
    uint32_t getIATSize() const;

    // Callbacks
    using ProgressCallback = std::function<void(int, const std::string&)>;
    void setProgressCallback(ProgressCallback callback);

private:
    // Import table building
    bool buildImportDescriptors();
    bool buildILT();
    bool buildIAT();
    bool buildHintNameTable();

    // Utility functions
    uint32_t addString(const std::string& str);
    uint32_t alignUp(uint32_t value, uint32_t alignment) const;

    // Member variables
    std::vector<std::string> libraries_;
    std::unordered_map<std::string, ImportInfo> imports_;
    std::vector<uint8_t> importTable_;
    std::vector<uint8_t> iat_;
    std::vector<std::string> stringTable_;
    uint32_t currentRVA_;
    uint32_t baseRVA_;
    ProgressCallback progressCallback_;

    // Constants
    static constexpr uint32_t ALIGNMENT = 8;
    static constexpr uint32_t DESCRIPTOR_SIZE = sizeof(IMAGE_IMPORT_DESCRIPTOR);
};

} // namespace pewriter