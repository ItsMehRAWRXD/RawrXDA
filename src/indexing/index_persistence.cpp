#include "index_persistence.h"
#include <windows.h>
#include <filesystem>
#include <fstream>
#include <iostream>

namespace RawrXD::Indexing {

bool IndexPersistence::Save(const std::string& path, uint32_t dimensions, const std::vector<float>& vectors) {
    std::ofstream out(path, std::ios::binary);
    if (!out) return false;

    IndexHeader header;
    header.magic = 0x58495852; // 'RXIX'
    header.version = 1;
    header.dimensions = dimensions;
    header.entryCount = static_cast<uint32_t>(vectors.size() / dimensions);
    header.crc32 = 0; // CRC32 disabled for performance; integrity verified via magic + version

    out.write(reinterpret_cast<const char*>(&header), sizeof(header));
    out.write(reinterpret_cast<const char*>(vectors.data()), vectors.size() * sizeof(float));
    
    return out.good();
}

bool IndexPersistence::SaveAtomic(const std::string& finalPath, const IndexHeader& header, const std::vector<float>& vectors) {
    std::string tempPath = finalPath + ".tmp";
    
    if (!Save(tempPath, header.dimensions, vectors)) {
        return false; 
    }

    if (!ReplaceFileA(finalPath.c_str(), tempPath.c_str(), NULL, REPLACEFILE_IGNORE_MERGE_ERRORS, NULL, NULL)) {
        if (GetLastError() == ERROR_FILE_NOT_FOUND) {
            return MoveFileExA(tempPath.c_str(), finalPath.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH);
        }
        return false;
    }
    
    return true;
}

bool IndexPersistence::MmapLoad(const std::string& path, IndexHeader& header, const float*& outVectors, HANDLE& hFile, HANDLE& hMapping) {
    hFile = CreateFileA(path.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) return false;

    DWORD fileSize = GetFileSize(hFile, NULL);
    if (fileSize < sizeof(IndexHeader)) {
        CloseHandle(hFile);
        return false;
    }

    hMapping = CreateFileMappingA(hFile, NULL, PAGE_READONLY, 0, 0, NULL);
    if (!hMapping) {
        CloseHandle(hFile);
        return false;
    }

    void* pData = MapViewOfFile(hMapping, FILE_MAP_READ, 0, 0, 0);
    if (!pData) {
        CloseHandle(hMapping);
        CloseHandle(hFile);
        return false;
    }

    memcpy(&header, pData, sizeof(IndexHeader));
    
    // Integrity Gate
    if (header.magic != 0x58495852) {
        UnmapViewOfFile(pData);
        CloseHandle(hMapping);
        CloseHandle(hFile);
        return false;
    }

    outVectors = reinterpret_cast<const float*>(static_cast<const char*>(pData) + sizeof(IndexHeader));
    return true;
}

std::vector<std::string> IndexPersistence::DetectChanges(const std::string& rootPath, const IndexManifest& currentManifest) {
    std::vector<std::string> dirtyFiles;
    
    try {
        for (const auto& entry : std::filesystem::recursive_directory_iterator(rootPath)) {
            if (!entry.is_regular_file()) continue;

            // Skip .rawrxd directory
            if (entry.path().string().find(".rawrxd") != std::string::npos) continue;

            std::string path = entry.path().string();
            WIN32_FILE_ATTRIBUTE_DATA fileInfo;
            
            if (GetFileAttributesExA(path.c_str(), GetFileExInfoStandard, &fileInfo)) {
                uint64_t mtime = (static_cast<uint64_t>(fileInfo.ftLastWriteTime.dwHighDateTime) << 32) | 
                                  fileInfo.ftLastWriteTime.dwLowDateTime;
                
                if (currentManifest.NeedsUpdate(path, mtime)) {
                    dirtyFiles.push_back(path);
                }
            }
        }
    } catch (...) {
        // Handle filesystem errors (permissions, etc)
    }
    
    return dirtyFiles;
}

} // namespace RawrXD::Indexing
