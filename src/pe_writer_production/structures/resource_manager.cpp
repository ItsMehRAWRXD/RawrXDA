// ============================================================================
// Resource Manager Implementation
// Builds complete resource table with directory structure
// ============================================================================

#include "resource_manager.h"
#include <algorithm>
#include <cstring>

namespace pewriter {

// ============================================================================
// ResourceManager Implementation
// ============================================================================

ResourceManager::ResourceManager() : currentRVA_(0) {}

bool ResourceManager::addResource(int type, int id, const std::vector<uint8_t>& data) {
    ResourceInfo resource;
    resource.type = type;
    resource.id = id;
    resource.data = data;
    resource.codePage = 0; // Default

    resources_.push_back(resource);
    return true;
}

bool ResourceManager::addVersionInfo(const std::unordered_map<std::string, std::string>& info) {
    // Build version info resource
    std::vector<uint8_t> versionData;
    if (buildVersionInfoResource(info)) {
        return addResource(RT_VERSION, 1, versionData);
    }
    return false;
}

bool ResourceManager::build() {
    try {
        if (progressCallback_) {
            progressCallback_(0, "Starting resource building");
        }

        // Reset data
        resourceTable_.clear();
        resourceData_.clear();
        currentRVA_ = 0;

        if (progressCallback_) {
            progressCallback_(30, "Building resource directory");
        }
        if (!buildResourceDirectory()) return false;

        if (progressCallback_) {
            progressCallback_(70, "Building resource data");
        }
        if (!buildResourceData()) return false;

        if (progressCallback_) {
            progressCallback_(100, "Resource building completed");
        }

        return true;
    } catch (const std::exception&) {
        return false;
    }
}

const std::vector<uint8_t>& ResourceManager::getResourceTable() const {
    return resourceTable_;
}

uint32_t ResourceManager::getResourceTableRVA() const {
    return currentRVA_;
}

uint32_t ResourceManager::getResourceTableSize() const {
    return static_cast<uint32_t>(resourceTable_.size() + resourceData_.size());
}

void ResourceManager::setProgressCallback(ProgressCallback callback) {
    progressCallback_ = callback;
}

// ============================================================================
// PRIVATE METHODS
// ============================================================================

bool ResourceManager::buildResourceDirectory() {
    if (resources_.empty()) {
        return true; // No resources to build
    }

    // Group resources by type
    std::unordered_map<int, std::vector<ResourceInfo>> typeGroups;
    for (const auto& resource : resources_) {
        typeGroups[resource.type].push_back(resource);
    }

    // Build type directory
    IMAGE_RESOURCE_DIRECTORY typeDir = {};
    typeDir.NumberOfNamedEntries = 0;
    typeDir.NumberOfIdEntries = static_cast<uint16_t>(typeGroups.size());

    const uint8_t* dirBytes = reinterpret_cast<const uint8_t*>(&typeDir);
    resourceTable_.insert(resourceTable_.end(), dirBytes, dirBytes + DIRECTORY_SIZE);

    // Build type entries
    uint32_t offset = DIRECTORY_SIZE + static_cast<uint32_t>(typeGroups.size()) * ENTRY_SIZE;

    for (const auto& pair : typeGroups) {
        IMAGE_RESOURCE_DIRECTORY_ENTRY entry = {};
        entry.Name = pair.first; // Type ID
        entry.OffsetToData = offset | 0x80000000; // Subdirectory

        const uint8_t* entryBytes = reinterpret_cast<const uint8_t*>(&entry);
        resourceTable_.insert(resourceTable_.end(), entryBytes, entryBytes + ENTRY_SIZE);

        // Build name directory for this type
        const auto& typeResources = pair.second;
        IMAGE_RESOURCE_DIRECTORY nameDir = {};
        nameDir.NumberOfNamedEntries = 0;
        nameDir.NumberOfIdEntries = static_cast<uint16_t>(typeResources.size());

        const uint8_t* nameDirBytes = reinterpret_cast<const uint8_t*>(&nameDir);
        resourceTable_.insert(resourceTable_.end(), nameDirBytes, nameDirBytes + DIRECTORY_SIZE);

        // Build name entries
        for (const auto& resource : typeResources) {
            IMAGE_RESOURCE_DIRECTORY_ENTRY nameEntry = {};
            nameEntry.Name = resource.id; // Resource ID
            nameEntry.OffsetToData = offset + DIRECTORY_SIZE +
                                   static_cast<uint32_t>(typeResources.size()) * ENTRY_SIZE;

            const uint8_t* nameEntryBytes = reinterpret_cast<const uint8_t*>(&nameEntry);
            resourceTable_.insert(resourceTable_.end(), nameEntryBytes, nameEntryBytes + ENTRY_SIZE);
        }

        offset += DIRECTORY_SIZE + static_cast<uint32_t>(typeResources.size()) * ENTRY_SIZE;
    }

    return true;
}

bool ResourceManager::buildResourceData() {
    for (const auto& resource : resources_) {
        // Add data entry
        IMAGE_RESOURCE_DATA_ENTRY dataEntry = {};
        dataEntry.OffsetToData = currentRVA_ + static_cast<uint32_t>(resourceTable_.size()) +
                               static_cast<uint32_t>(resourceData_.size());
        dataEntry.Size = static_cast<uint32_t>(resource.data.size());
        dataEntry.CodePage = resource.codePage;
        dataEntry.Reserved = 0;

        const uint8_t* entryBytes = reinterpret_cast<const uint8_t*>(&dataEntry);
        resourceTable_.insert(resourceTable_.end(), entryBytes, entryBytes + DATA_ENTRY_SIZE);

        // Add actual data
        resourceData_.insert(resourceData_.end(), resource.data.begin(), resource.data.end());

        // Align data
        while (resourceData_.size() % ALIGNMENT != 0) {
            resourceData_.push_back(0);
        }
    }

    return true;
}

bool ResourceManager::buildVersionInfoResource(const std::unordered_map<std::string, std::string>& info) {
    // Build a proper VS_VERSIONINFO structure:
    //   VS_VERSIONINFO {
    //     WORD  wLength, wValueLength, wType;
    //     WCHAR szKey[] = L"VS_VERSION_INFO\0";
    //     WORD  Padding1[];
    //     VS_FIXEDFILEINFO Value;
    //     WORD  Padding2[];
    //     WORD  Children[];   // StringFileInfo + VarFileInfo
    //   }

    // Helper: append a wide (UTF-16LE) string including null terminator
    auto appendWideString = [this](const std::string& str) {
        for (char c : str) {
            resourceData_.push_back(static_cast<uint8_t>(c));
            resourceData_.push_back(0);
        }
        // Null terminator (2 bytes)
        resourceData_.push_back(0);
        resourceData_.push_back(0);
    };

    // Helper: pad to 32-bit (DWORD) alignment
    auto alignToDword = [this]() {
        while (resourceData_.size() % 4 != 0) {
            resourceData_.push_back(0);
        }
    };

    // Helper: write a WORD into the buffer
    auto writeWord = [this](uint16_t w) {
        resourceData_.push_back(static_cast<uint8_t>(w & 0xFF));
        resourceData_.push_back(static_cast<uint8_t>((w >> 8) & 0xFF));
    };

    // Helper: write a DWORD into the buffer
    auto writeDword = [this](uint32_t d) {
        for (int i = 0; i < 4; ++i)
            resourceData_.push_back(static_cast<uint8_t>((d >> (i * 8)) & 0xFF));
    };

    // --- VS_VERSIONINFO header ---
    size_t versionInfoStart = resourceData_.size();
    writeWord(0);    // wLength  (placeholder — patched later)
    writeWord(sizeof(uint32_t) * 13); // wValueLength = sizeof(VS_FIXEDFILEINFO) = 52
    writeWord(0);    // wType = binary

    // szKey = L"VS_VERSION_INFO"
    appendWideString("VS_VERSION_INFO");
    alignToDword();

    // --- VS_FIXEDFILEINFO ---
    writeDword(0xFEEF04BD);  // dwSignature
    writeDword(0x00010000);  // dwStrucVersion
    writeDword(0x00020000);  // dwFileVersionMS    (2.0)
    writeDword(0x00000000);  // dwFileVersionLS
    writeDword(0x00020000);  // dwProductVersionMS (2.0)
    writeDword(0x00000000);  // dwProductVersionLS
    writeDword(0x0000003F);  // dwFileFlagsMask
    writeDword(0x00000000);  // dwFileFlags
    writeDword(0x00040004);  // dwFileOS = VOS_NT_WINDOWS32
    writeDword(0x00000001);  // dwFileType = VFT_APP
    writeDword(0x00000000);  // dwFileSubtype
    writeDword(0x00000000);  // dwFileDateMS
    writeDword(0x00000000);  // dwFileDateLS
    alignToDword();

    // --- StringFileInfo ---
    size_t stringFileInfoStart = resourceData_.size();
    writeWord(0);    // wLength (placeholder)
    writeWord(0);    // wValueLength = 0
    writeWord(1);    // wType = text

    appendWideString("StringFileInfo");
    alignToDword();

    // --- StringTable (language 0409, codepage 04B0 = Unicode) ---
    size_t stringTableStart = resourceData_.size();
    writeWord(0);    // wLength (placeholder)
    writeWord(0);    // wValueLength = 0
    writeWord(1);    // wType = text

    appendWideString("040904B0");
    alignToDword();

    // --- Individual String entries ---
    for (const auto& pair : info) {
        alignToDword();
        size_t stringStart = resourceData_.size();

        uint16_t valueLen = static_cast<uint16_t>(pair.second.size() + 1); // in WCHARs, including null
        writeWord(0);         // wLength (placeholder)
        writeWord(valueLen);  // wValueLength (in WCHARs)
        writeWord(1);         // wType = text

        // Key
        appendWideString(pair.first);
        alignToDword();

        // Value
        appendWideString(pair.second);
        alignToDword();

        // Patch wLength for this String entry
        uint16_t stringLen = static_cast<uint16_t>(resourceData_.size() - stringStart);
        resourceData_[stringStart]     = static_cast<uint8_t>(stringLen & 0xFF);
        resourceData_[stringStart + 1] = static_cast<uint8_t>((stringLen >> 8) & 0xFF);
    }

    // Patch StringTable wLength
    uint16_t stringTableLen = static_cast<uint16_t>(resourceData_.size() - stringTableStart);
    resourceData_[stringTableStart]     = static_cast<uint8_t>(stringTableLen & 0xFF);
    resourceData_[stringTableStart + 1] = static_cast<uint8_t>((stringTableLen >> 8) & 0xFF);

    // Patch StringFileInfo wLength
    uint16_t stringFileInfoLen = static_cast<uint16_t>(resourceData_.size() - stringFileInfoStart);
    resourceData_[stringFileInfoStart]     = static_cast<uint8_t>(stringFileInfoLen & 0xFF);
    resourceData_[stringFileInfoStart + 1] = static_cast<uint8_t>((stringFileInfoLen >> 8) & 0xFF);

    // --- VarFileInfo ---
    alignToDword();
    size_t varFileInfoStart = resourceData_.size();
    writeWord(0);    // wLength (placeholder)
    writeWord(0);    // wValueLength = 0
    writeWord(1);    // wType = text

    appendWideString("VarFileInfo");
    alignToDword();

    // Var entry: Translation
    size_t varStart = resourceData_.size();
    writeWord(0);    // wLength (placeholder)
    writeWord(4);    // wValueLength = 4 (one translation pair)
    writeWord(0);    // wType = binary

    appendWideString("Translation");
    alignToDword();

    // Translation value: 0x0409 (US English), 0x04B0 (Unicode)
    writeWord(0x0409);
    writeWord(0x04B0);

    // Patch Var wLength
    uint16_t varLen = static_cast<uint16_t>(resourceData_.size() - varStart);
    resourceData_[varStart]     = static_cast<uint8_t>(varLen & 0xFF);
    resourceData_[varStart + 1] = static_cast<uint8_t>((varLen >> 8) & 0xFF);

    // Patch VarFileInfo wLength
    uint16_t varFileInfoLen = static_cast<uint16_t>(resourceData_.size() - varFileInfoStart);
    resourceData_[varFileInfoStart]     = static_cast<uint8_t>(varFileInfoLen & 0xFF);
    resourceData_[varFileInfoStart + 1] = static_cast<uint8_t>((varFileInfoLen >> 8) & 0xFF);

    // Patch VS_VERSIONINFO wLength
    uint16_t versionInfoLen = static_cast<uint16_t>(resourceData_.size() - versionInfoStart);
    resourceData_[versionInfoStart]     = static_cast<uint8_t>(versionInfoLen & 0xFF);
    resourceData_[versionInfoStart + 1] = static_cast<uint8_t>((versionInfoLen >> 8) & 0xFF);

    return true;
}

uint32_t ResourceManager::addStringToPool(const std::string& str) {
    stringPool_.push_back(str);
    return static_cast<uint32_t>(stringPool_.size() - 1);
}

uint32_t ResourceManager::alignUp(uint32_t value, uint32_t alignment) const {
    return (value + alignment - 1) & ~(alignment - 1);
}

} // namespace pewriter