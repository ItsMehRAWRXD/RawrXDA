#pragma once
#include <string>
#include <vector>
#include "index_manifest.h"

namespace RawrXD::Indexing {

class IndexPersistence {
public:
    // Basic Save/Load
    static bool Save(const std::string& path, uint32_t dimensions, const std::vector<float>& vectors);
    static bool Load(const std::string& path, uint32_t& dimensions, std::vector<float>& vectors);

    // Step 3 & 4: Atomic and Mmap
    static bool SaveAtomic(const std::string& finalPath, const IndexHeader& header, const std::vector<float>& vectors);
    
    // Mmap-based loading for O(1) startup
    static bool MmapLoad(const std::string& path, IndexHeader& header, const float*& outVectors, HANDLE& hFile, HANDLE& hMapping);

    // Change detection logic
    static std::vector<std::string> DetectChanges(const std::string& rootPath, const IndexManifest& currentManifest);
};

} // namespace RawrXD::Indexing
