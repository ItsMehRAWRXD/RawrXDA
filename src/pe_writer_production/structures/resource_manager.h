// ============================================================================
// Resource Manager - Handles PE Resource Table Creation
// Supports version info, icons, strings, and custom resources
// ============================================================================

#pragma once

#include "../pe_writer.h"
#include <vector>
#include <string>
#include <unordered_map>
#include <functional>

namespace pewriter {

// ============================================================================
// RESOURCE STRUCTURES
// ============================================================================

#pragma pack(push, 1)

struct IMAGE_RESOURCE_DIRECTORY {
    uint32_t Characteristics;
    uint32_t TimeDateStamp;
    uint16_t MajorVersion;
    uint16_t MinorVersion;
    uint16_t NumberOfNamedEntries;
    uint16_t NumberOfIdEntries;
};

struct IMAGE_RESOURCE_DIRECTORY_ENTRY {
    uint32_t Name;
    uint32_t OffsetToData;
};

struct IMAGE_RESOURCE_DATA_ENTRY {
    uint32_t OffsetToData;
    uint32_t Size;
    uint32_t CodePage;
    uint32_t Reserved;
};

#pragma pack(pop)

// ============================================================================
// RESOURCE INFORMATION
// ============================================================================

struct ResourceInfo {
    int type;
    int id;
    std::string name;
    std::vector<uint8_t> data;
    uint32_t codePage;
};

// ============================================================================
// RESOURCE MANAGER CLASS
// ============================================================================

class ResourceManager {
public:
    ResourceManager();
    ~ResourceManager() = default;

    bool addResource(int type, int id, const std::vector<uint8_t>& data);
    bool addVersionInfo(const std::unordered_map<std::string, std::string>& info);
    bool build();

    // Get resource table data
    const std::vector<uint8_t>& getResourceTable() const;
    uint32_t getResourceTableRVA() const;
    uint32_t getResourceTableSize() const;

    // Callbacks
    using ProgressCallback = std::function<void(int, const std::string&)>;
    void setProgressCallback(ProgressCallback callback);

private:
    // Resource building
    bool buildResourceDirectory();
    bool buildResourceData();
    bool buildVersionInfoResource(const std::unordered_map<std::string, std::string>& info);

    // Utility functions
    uint32_t addStringToPool(const std::string& str);
    uint32_t alignUp(uint32_t value, uint32_t alignment) const;

    // Member variables
    std::vector<ResourceInfo> resources_;
    std::vector<uint8_t> resourceTable_;
    std::vector<uint8_t> resourceData_;
    std::vector<std::string> stringPool_;
    uint32_t currentRVA_;
    ProgressCallback progressCallback_;

    // Constants
    static constexpr uint32_t ALIGNMENT = 4;
    static constexpr uint32_t DIRECTORY_SIZE = sizeof(IMAGE_RESOURCE_DIRECTORY);
    static constexpr uint32_t ENTRY_SIZE = sizeof(IMAGE_RESOURCE_DIRECTORY_ENTRY);
    static constexpr uint32_t DATA_ENTRY_SIZE = sizeof(IMAGE_RESOURCE_DATA_ENTRY);

    // Resource types
    static constexpr int RT_VERSION = 16;
    static constexpr int RT_ICON = 3;
    static constexpr int RT_GROUP_ICON = 14;
    static constexpr int RT_STRING = 6;
    static constexpr int RT_MANIFEST = 24;
};

} // namespace pewriter