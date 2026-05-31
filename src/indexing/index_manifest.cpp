#include "index_manifest.h"
#include <filesystem>
#include <fstream>
#include <iostream>

namespace RawrXD::Indexing {

bool IndexManifest::NeedsUpdate(const std::string& path, uint64_t lastModified) const {
    auto it = m_entries.find(path);
    if (it == m_entries.end()) return true;
    return it->second.lastModified != lastModified;
}

void IndexManifest::UpdateEntry(const std::string& path, uint64_t contentHash, uint32_t vecIdx) {
    WIN32_FILE_ATTRIBUTE_DATA fileInfo;
    uint64_t mtime = 0;
    if (GetFileAttributesExA(path.c_str(), GetFileExInfoStandard, &fileInfo)) {
        mtime = (static_cast<uint64_t>(fileInfo.ftLastWriteTime.dwHighDateTime) << 32) | 
                 fileInfo.ftLastWriteTime.dwLowDateTime;
    }

    FileMetadata meta;
    meta.pathHash = ComputePathHash(path);
    meta.lastModified = mtime;
    meta.contentHash = contentHash;
    meta.vectorIndex = vecIdx;

    m_entries[path] = meta;
}

bool IndexManifest::Load(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return false;

    uint32_t count = 0;
    in.read(reinterpret_cast<char*>(&count), sizeof(uint32_t));
    
    m_entries.clear();
    for (uint32_t i = 0; i < count; ++i) {
        uint32_t pathLen;
        in.read(reinterpret_cast<char*>(&pathLen), sizeof(pathLen));
        std::string p(pathLen, '\0');
        in.read(&p[0], pathLen);
        
        FileMetadata meta;
        in.read(reinterpret_cast<char*>(&meta), sizeof(meta));
        m_entries[p] = meta;
    }
    return true;
}

bool IndexManifest::Save(const std::string& path) const {
    std::ofstream out(path, std::ios::binary);
    if (!out) return false;

    uint32_t count = static_cast<uint32_t>(m_entries.size());
    out.write(reinterpret_cast<const char*>(&count), sizeof(uint32_t));

    for (const auto& [p, meta] : m_entries) {
        uint32_t pathLen = static_cast<uint32_t>(p.size());
        out.write(reinterpret_cast<const char*>(&pathLen), sizeof(pathLen));
        out.write(p.data(), pathLen);
        out.write(reinterpret_cast<const char*>(&meta), sizeof(meta));
    }
    return true;
}

uint64_t IndexManifest::ComputePathHash(const std::string& path) {
    // Simple FNV-1a for path hashing
    uint64_t hash = 0xcbf29ce484222325ULL;
    for (char c : path) {
        hash ^= static_cast<uint64_t>(c);
        hash *= 0x100000001b3ULL;
    }
    return hash;
}

std::vector<std::string> IndexManifest::GetDirtyFiles(const std::string& rootPath) {
    std::vector<std::string> dirty;
    try {
        for (const auto& entry : std::filesystem::recursive_directory_iterator(rootPath)) {
            if (!entry.is_regular_file()) continue;
            std::string p = entry.path().string();
            if (p.find(".rawrxd") != std::string::npos) continue;

            WIN32_FILE_ATTRIBUTE_DATA fileInfo;
            if (GetFileAttributesExA(p.c_str(), GetFileExInfoStandard, &fileInfo)) {
                uint64_t mtime = (static_cast<uint64_t>(fileInfo.ftLastWriteTime.dwHighDateTime) << 32) | 
                                 fileInfo.ftLastWriteTime.dwLowDateTime;
                if (NeedsUpdate(p, mtime)) {
                    dirty.push_back(p);
                }
            }
        }
    } catch (const std::exception& e) {
        OutputDebugStringA((std::string("[IndexManifest] ScanForDirtyFiles exception: ") + e.what() + "\n").c_str());
    } catch (...) {
        OutputDebugStringA("[IndexManifest] ScanForDirtyFiles unknown exception\n");
    }
    return dirty;
}

} // namespace RawrXD::Indexing
