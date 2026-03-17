// ============================================================================
// Import Resolver Implementation
// Builds complete import table with ILT, IAT, and hint/name tables
// ============================================================================

#include "import_resolver.h"
#include <algorithm>
#include <cstring>

namespace pewriter {

// ============================================================================
// ImportResolver Implementation
// ============================================================================

ImportResolver::ImportResolver() : currentRVA_(0), baseRVA_(0) {}

void ImportResolver::setLibraries(const std::vector<std::string>& libraries) {
    libraries_ = libraries;
}

bool ImportResolver::addImport(const std::string& dllName, const std::string& symbol, uint16_t hint) {
    auto& import = imports_[dllName];
    if (import.dllName.empty()) {
        import.dllName = dllName;
    }

    // Check if symbol already exists
    auto it = std::find(import.symbols.begin(), import.symbols.end(), symbol);
    if (it != import.symbols.end()) {
        return true; // Already added
    }

    import.symbols.push_back(symbol);
    import.hints.push_back(hint);
    return true;
}

bool ImportResolver::resolve() {
    try {
        if (progressCallback_) {
            progressCallback_(0, "Starting import resolution");
        }

        // Reset data
        importTable_.clear();
        iat_.clear();
        stringTable_.clear();
        currentRVA_ = baseRVA_;

        if (progressCallback_) {
            progressCallback_(20, "Building import descriptors");
        }
        if (!buildImportDescriptors()) return false;

        if (progressCallback_) {
            progressCallback_(40, "Building ILT");
        }
        if (!buildILT()) return false;

        if (progressCallback_) {
            progressCallback_(60, "Building IAT");
        }
        if (!buildIAT()) return false;

        if (progressCallback_) {
            progressCallback_(80, "Building hint/name table");
        }
        if (!buildHintNameTable()) return false;

        if (progressCallback_) {
            progressCallback_(100, "Import resolution completed");
        }

        return true;
    } catch (const std::exception&) {
        return false;
    }
}

const std::vector<uint8_t>& ImportResolver::getImportTable() const {
    return importTable_;
}

uint32_t ImportResolver::getImportTableRVA() const {
    return baseRVA_;
}

uint32_t ImportResolver::getImportTableSize() const {
    return static_cast<uint32_t>(importTable_.size());
}

const std::vector<uint8_t>& ImportResolver::getIAT() const {
    return iat_;
}

uint32_t ImportResolver::getIATRVA() const {
    // IAT comes after ILT in the import table
    return baseRVA_ + static_cast<uint32_t>(imports_.size()) * (DESCRIPTOR_SIZE + ALIGNMENT);
}

uint32_t ImportResolver::getIATSize() const {
    return static_cast<uint32_t>(iat_.size());
}

void ImportResolver::setProgressCallback(ProgressCallback callback) {
    progressCallback_ = callback;
}

// ============================================================================
// PRIVATE METHODS
// ============================================================================

bool ImportResolver::buildImportDescriptors() {
    uint32_t currentOffset = 0;

    for (auto& pair : imports_) {
        auto& import = pair.second;

        // Calculate RVAs for this import
        import.iltRVA = currentRVA_ + static_cast<uint32_t>(importTable_.size()) +
                       static_cast<uint32_t>(imports_.size()) * DESCRIPTOR_SIZE + ALIGNMENT;
        import.iatRVA = import.iltRVA + 100; // Reserve space for ILT
        import.nameRVA = addString(import.dllName);

        // Create descriptor
        IMAGE_IMPORT_DESCRIPTOR desc = {};
        desc.OriginalFirstThunk = import.iltRVA;
        desc.TimeDateStamp = 0;
        desc.ForwarderChain = 0;
        desc.Name = import.nameRVA;
        desc.FirstThunk = import.iatRVA;

        // Add to import table
        const uint8_t* descBytes = reinterpret_cast<const uint8_t*>(&desc);
        importTable_.insert(importTable_.end(), descBytes, descBytes + DESCRIPTOR_SIZE);
        currentOffset += DESCRIPTOR_SIZE;
    }

    // Add null terminator descriptor
    IMAGE_IMPORT_DESCRIPTOR nullDesc = {};
    const uint8_t* nullBytes = reinterpret_cast<const uint8_t*>(&nullDesc);
    importTable_.insert(importTable_.end(), nullBytes, nullBytes + DESCRIPTOR_SIZE);

    return true;
}

bool ImportResolver::buildILT() {
    for (const auto& pair : imports_) {
        const auto& import = pair.second;

        // ILT entries are 8 bytes (QWORD) for PE32+
        // Each entry is an RVA to IMAGE_IMPORT_BY_NAME
        uint32_t hintNameRVA = currentRVA_ + static_cast<uint32_t>(importTable_.size()) +
                              static_cast<uint32_t>(iat_.size());

        for (size_t i = 0; i < import.symbols.size(); ++i) {
            // 8-byte ILT entry (PE32+ uses QWORD entries)
            uint64_t entry = static_cast<uint64_t>(hintNameRVA);
            const uint8_t* entryBytes = reinterpret_cast<const uint8_t*>(&entry);
            importTable_.insert(importTable_.end(), entryBytes, entryBytes + sizeof(uint64_t));

            hintNameRVA += sizeof(uint16_t) + static_cast<uint32_t>(import.symbols[i].size()) + 1;
            hintNameRVA = alignUp(hintNameRVA, 2);
        }

        // 8-byte null terminator
        uint64_t nullEntry = 0;
        const uint8_t* nullBytes = reinterpret_cast<const uint8_t*>(&nullEntry);
        importTable_.insert(importTable_.end(), nullBytes, nullBytes + sizeof(uint64_t));
    }

    return true;
}

bool ImportResolver::buildIAT() {
    for (const auto& pair : imports_) {
        const auto& import = pair.second;

        // IAT entries are 8 bytes (QWORD) for PE32+, mirroring ILT
        // The loader overwrites these with actual function addresses
        for (size_t i = 0; i < import.symbols.size(); ++i) {
            uint64_t entry = 0; // Will be filled by PE loader at runtime
            const uint8_t* entryBytes = reinterpret_cast<const uint8_t*>(&entry);
            iat_.insert(iat_.end(), entryBytes, entryBytes + sizeof(uint64_t));
        }

        // 8-byte null terminator
        uint64_t nullEntry = 0;
        const uint8_t* nullBytes = reinterpret_cast<const uint8_t*>(&nullEntry);
        iat_.insert(iat_.end(), nullBytes, nullBytes + sizeof(uint64_t));
    }

    return true;
}

bool ImportResolver::buildHintNameTable() {
    for (const auto& pair : imports_) {
        const auto& import = pair.second;

        for (size_t i = 0; i < import.symbols.size(); ++i) {
            // Hint
            uint16_t hint = import.hints[i];
            const uint8_t* hintBytes = reinterpret_cast<const uint8_t*>(&hint);
            importTable_.insert(importTable_.end(), hintBytes, hintBytes + sizeof(uint16_t));

            // Name
            const std::string& name = import.symbols[i];
            importTable_.insert(importTable_.end(), name.begin(), name.end());
            importTable_.push_back(0); // Null terminator

            // Align to word boundary
            while (importTable_.size() % 2 != 0) {
                importTable_.push_back(0);
            }
        }
    }

    return true;
}

uint32_t ImportResolver::addString(const std::string& str) {
    // Calculate actual position where string data will be placed
    // Strings go into the import table after all descriptors, ILT, and IAT data
    uint32_t strOffset = static_cast<uint32_t>(stringTable_.size());
    for (const auto& s : stringTable_) {
        strOffset += static_cast<uint32_t>(s.size()) + 1;
    }
    uint32_t rva = currentRVA_ + static_cast<uint32_t>(importTable_.size()) +
                  static_cast<uint32_t>(iat_.size()) + strOffset;

    stringTable_.push_back(str);
    return rva;
}

uint32_t ImportResolver::alignUp(uint32_t value, uint32_t alignment) const {
    return (value + alignment - 1) & ~(alignment - 1);
}

} // namespace pewriter