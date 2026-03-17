// ============================================================================
// Relocation Manager Implementation
// Handles relocation table building and application
// ============================================================================

#include "relocation_manager.h"
#include <algorithm>

namespace pewriter {

// ============================================================================
// RelocationManager Implementation
// ============================================================================

RelocationManager::RelocationManager() {}

bool RelocationManager::addRelocation(const RelocationEntry& relocation) {
    relocations_.push_back(relocation);

    // Group by page
    uint32_t page = relocation.offset & ~(PAGE_SIZE - 1);
    blocks_[page].pageRVA = page;

    return true;
}

bool RelocationManager::apply() {
    try {
        if (progressCallback_) {
            progressCallback_(0, "Starting relocation processing");
        }

        if (progressCallback_) {
            progressCallback_(50, "Building relocation table");
        }
        if (!buildRelocationTable()) return false;

        if (progressCallback_) {
            progressCallback_(100, "Relocation processing completed");
        }

        return true;
    } catch (const std::exception&) {
        return false;
    }
}

const std::vector<uint8_t>& RelocationManager::getRelocationTable() const {
    return relocationTable_;
}

uint32_t RelocationManager::getRelocationTableRVA() const {
    return 0; // Will be set by caller
}

uint32_t RelocationManager::getRelocationTableSize() const {
    return static_cast<uint32_t>(relocationTable_.size());
}

void RelocationManager::setSymbolAddress(const std::string& symbol, uint64_t address) {
    symbolTable_[symbol] = address;
}

uint64_t RelocationManager::getSymbolAddress(const std::string& symbol) const {
    auto it = symbolTable_.find(symbol);
    return (it != symbolTable_.end()) ? it->second : 0;
}

void RelocationManager::setProgressCallback(ProgressCallback callback) {
    progressCallback_ = callback;
}

// ============================================================================
// PRIVATE METHODS
// ============================================================================

bool RelocationManager::buildRelocationTable() {
    relocationTable_.clear();

    for (const auto& pair : blocks_) {
        const auto& block = pair.second;

        // Block header
        IMAGE_BASE_RELOCATION header;
        header.VirtualAddress = block.pageRVA;
        header.SizeOfBlock = sizeof(IMAGE_BASE_RELOCATION) +
                           static_cast<uint32_t>(block.entries.size()) * sizeof(uint16_t);

        // Add header
        const uint8_t* headerBytes = reinterpret_cast<const uint8_t*>(&header);
        relocationTable_.insert(relocationTable_.end(), headerBytes,
                              headerBytes + sizeof(IMAGE_BASE_RELOCATION));

        // Add entries
        for (uint16_t entry : block.entries) {
            const uint8_t* entryBytes = reinterpret_cast<const uint8_t*>(&entry);
            relocationTable_.insert(relocationTable_.end(), entryBytes,
                                  entryBytes + sizeof(uint16_t));
        }
    }

    return true;
}

bool RelocationManager::applyRelocationsToImage(std::vector<uint8_t>& image) {
    for (const auto& reloc : relocations_) {
        if (reloc.offset >= image.size()) continue;

        uint64_t symbolAddr = getSymbolAddress(reloc.symbol);
        if (symbolAddr == 0 && !reloc.symbol.empty()) continue;

        uint64_t targetAddr = symbolAddr + reloc.addend;

        switch (reloc.type) {
        case IMAGE_REL_AMD64_ADDR64:
            if (reloc.offset + 8 <= image.size()) {
                *reinterpret_cast<uint64_t*>(&image[reloc.offset]) = targetAddr;
            }
            break;

        case IMAGE_REL_AMD64_ADDR32:
            if (reloc.offset + 4 <= image.size()) {
                *reinterpret_cast<uint32_t*>(&image[reloc.offset]) =
                    static_cast<uint32_t>(targetAddr);
            }
            break;

        case IMAGE_REL_AMD64_REL32:
            if (reloc.offset + 4 <= image.size()) {
                int32_t displacement = static_cast<int32_t>(targetAddr - reloc.offset - 4);
                *reinterpret_cast<uint32_t*>(&image[reloc.offset]) =
                    static_cast<uint32_t>(displacement);
            }
            break;

        case IMAGE_REL_AMD64_ADDR32NB:
            if (reloc.offset + 4 <= image.size()) {
                *reinterpret_cast<uint32_t*>(&image[reloc.offset]) =
                    static_cast<uint32_t>(targetAddr);
            }
            break;

        default:
            // Unsupported relocation type
            break;
        }
    }

    return true;
}

uint16_t RelocationManager::createRelocationEntry(uint32_t offset, uint32_t type) {
    return static_cast<uint16_t>((type << 12) | (offset & 0xFFF));
}

uint32_t RelocationManager::alignUp(uint32_t value, uint32_t alignment) const {
    return (value + alignment - 1) & ~(alignment - 1);
}

} // namespace pewriter