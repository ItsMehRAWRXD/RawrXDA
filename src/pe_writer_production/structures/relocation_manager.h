// ============================================================================
// Relocation Manager - Handles PE Relocation Table Creation and Application
// Supports all standard x64 relocation types
// ============================================================================

#pragma once

#include "../pe_writer.h"
#include <vector>
#include <unordered_map>
#include <functional>

namespace pewriter {

// ============================================================================
// RELOCATION STRUCTURES
// ============================================================================

#pragma pack(push, 1)

struct IMAGE_BASE_RELOCATION {
    uint32_t VirtualAddress;
    uint32_t SizeOfBlock;
};

#pragma pack(pop)

// ============================================================================
// RELOCATION ENTRY
// ============================================================================

struct RelocationBlock {
    uint32_t pageRVA;
    std::vector<uint16_t> entries; // Type and offset
};

// ============================================================================
// RELOCATION MANAGER CLASS
// ============================================================================

class RelocationManager {
public:
    RelocationManager();
    ~RelocationManager() = default;

    bool addRelocation(const RelocationEntry& relocation);
    bool apply();

    // Get relocation table data
    const std::vector<uint8_t>& getRelocationTable() const;
    uint32_t getRelocationTableRVA() const;
    uint32_t getRelocationTableSize() const;

    // Symbol resolution
    void setSymbolAddress(const std::string& symbol, uint64_t address);
    uint64_t getSymbolAddress(const std::string& symbol) const;

    // Callbacks
    using ProgressCallback = std::function<void(int, const std::string&)>;
    void setProgressCallback(ProgressCallback callback);

private:
    // Relocation processing
    bool buildRelocationTable();
    bool applyRelocationsToImage(std::vector<uint8_t>& image);

    // Utility functions
    uint16_t createRelocationEntry(uint32_t offset, uint32_t type);
    uint32_t alignUp(uint32_t value, uint32_t alignment) const;

    // Member variables
    std::vector<RelocationEntry> relocations_;
    std::unordered_map<std::string, uint64_t> symbolTable_;
    std::vector<uint8_t> relocationTable_;
    std::unordered_map<uint32_t, RelocationBlock> blocks_;
    ProgressCallback progressCallback_;

    // Constants
    static constexpr uint32_t PAGE_SIZE = 0x1000;
    static constexpr uint32_t ALIGNMENT = 4;

    // x64 relocation types
    static constexpr uint32_t IMAGE_REL_AMD64_ABSOLUTE = 0x0000;
    static constexpr uint32_t IMAGE_REL_AMD64_ADDR64 = 0x0001;
    static constexpr uint32_t IMAGE_REL_AMD64_ADDR32 = 0x0002;
    static constexpr uint32_t IMAGE_REL_AMD64_ADDR32NB = 0x0003;
    static constexpr uint32_t IMAGE_REL_AMD64_REL32 = 0x0004;
    static constexpr uint32_t IMAGE_REL_AMD64_REL32_1 = 0x0005;
    static constexpr uint32_t IMAGE_REL_AMD64_REL32_2 = 0x0006;
    static constexpr uint32_t IMAGE_REL_AMD64_REL32_3 = 0x0007;
    static constexpr uint32_t IMAGE_REL_AMD64_REL32_4 = 0x0008;
    static constexpr uint32_t IMAGE_REL_AMD64_REL32_5 = 0x0009;
    static constexpr uint32_t IMAGE_REL_AMD64_SECTION = 0x000A;
    static constexpr uint32_t IMAGE_REL_AMD64_SECREL = 0x000B;
    static constexpr uint32_t IMAGE_REL_AMD64_SECREL7 = 0x000C;
    static constexpr uint32_t IMAGE_REL_AMD64_TOKEN = 0x000D;
    static constexpr uint32_t IMAGE_REL_AMD64_SREL32 = 0x000E;
    static constexpr uint32_t IMAGE_REL_AMD64_PAIR = 0x000F;
    static constexpr uint32_t IMAGE_REL_AMD64_SSPAN32 = 0x0010;
};

} // namespace pewriter