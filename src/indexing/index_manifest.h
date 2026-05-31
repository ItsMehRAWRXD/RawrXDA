#pragma once
#include <windows.h>
#include <unordered_map>
#include <string>
#include <vector>
#include <cstdint>

namespace RawrXD::Indexing {

struct FileMetadata {
    uint64_t pathHash;
    uint64_t lastModified;
    uint64_t contentHash;
    uint32_t vectorIndex; // Position in the flat binary blob
};

struct IndexHeader {
    uint32_t magic;      // 'RXIX'
    uint32_t version;    // 1
    uint32_t dimensions; // e.g. 768
    uint32_t entryCount; // Number of vectors
    uint32_t crc32;      // CRC32 of vector data
};

class IndexManifest {
public:
    // Singleton access or instance? Let's use instance for now.
    IndexManifest() = default;

    // Compares current disk state against manifest to find "dirty" files
    std::vector<std::string> GetDirtyFiles(const std::string& rootPath);
    
    // Checks if a specific file needs updating based on timestamp
    bool NeedsUpdate(const std::string& path, uint64_t lastModified) const;

    // Updates manifest after successful re-indexing of a file
    void UpdateEntry(const std::string& path, uint64_t contentHash, uint32_t vecIdx);

    // Serialization
    bool Load(const std::string& path);
    bool Save(const std::string& path) const;

    size_t GetEntryCount() const { return m_entries.size(); }

private:
    std::unordered_map<std::string, FileMetadata> m_entries;
    
    static uint64_t ComputePathHash(const std::string& path);
};

} // namespace RawrXD::Indexing
