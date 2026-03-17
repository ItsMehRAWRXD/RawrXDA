#pragma once
#include <cstdint>
#include <string>
#include <map>
#include <windows.h>
#include "RawrXD_SafeGGUFStream.hpp"

namespace RawrXD::Tools {

class HardenedGGUFMetadataParser {
public:
    struct MetadataEntry {
        std::string key;
        uint32_t type;
        std::string string_value;
        uint64_t numeric_value = 0;
    };

    static constexpr const char* HIGH_RISK_KEYS[] = {
        "tokenizer.chat_template",
        "tokenizer.ggml.tokens",
        "tokenizer.ggml.merges",
        "tokenizer.ggml.token_type",
        nullptr
    };

    /**
     * ParseMetadataRobust - Load GGUF metadata with safeguards
     * 
     * @param filepath Path to GGUF file
     * @param skip_high_risk If true, automatically skip known problem keys
     * @param max_string_alloc Hard limit for string allocations (default: 50MB)
     * @return Vector of safe metadata entries
     */
    static std::vector<MetadataEntry> ParseMetadataRobust(
        const std::string& filepath,
        bool skip_high_risk = true,
        size_t max_string_alloc = 50 * 1024 * 1024)
    {
        std::vector<MetadataEntry> entries;
        
        // Open file with explicit size
        HANDLE hFile = CreateFileA(
            filepath.c_str(),
            GENERIC_READ,
            FILE_SHARE_READ,
            nullptr,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN,
            nullptr);
        
        if (hFile == INVALID_HANDLE_VALUE) {
            throw std::runtime_error("Failed to open GGUF file: " + filepath);
        }

        // Get file size
        LARGE_INTEGER liSize;
        if (!GetFileSizeEx(hFile, &liSize)) {
            CloseHandle(hFile);
            throw std::runtime_error("Failed to get file size");
        }

        // Validate 64GB hard limit
        if (liSize.QuadPart > 0x1000000000LL) { // 64GB
            CloseHandle(hFile);
            throw std::runtime_error("GGUF file exceeds 64GB hard limit");
        }

        // Memory-map the file
        HANDLE hMapFile = CreateFileMappingA(
            hFile,
            nullptr,
            PAGE_READONLY,
            liSize.HighPart,
            liSize.LowPart,
            nullptr);
        
        if (!hMapFile) {
            CloseHandle(hFile);
            throw std::runtime_error("Failed to create file mapping");
        }

        // Map view
        const uint8_t* pData = static_cast<const uint8_t*>(
            MapViewOfFile(hMapFile, FILE_MAP_READ, 0, 0, 0));
        
        if (!pData) {
            CloseHandle(hMapFile);
            CloseHandle(hFile);
            throw std::runtime_error("Failed to map file view");
        }

        try {
            // Create parser with callbacks
            SafeGGUFParser parser(pData, static_cast<size_t>(liSize.QuadPart));
            
            SafeGGUFParser::ParseCallbacks cb;
            cb.onMetadata = [&](const std::string& key, uint32_t type, uint64_t size) -> bool {
                // Check if key is high-risk
                if (skip_high_risk && IsHighRiskKey(key)) {
                    return false; // Skip loading
                }
                
                // Check allocation limits
                if (type == 8 && size > max_string_alloc) {
                    fprintf(stderr, "[WARN] String '%s' too large (%llu bytes), skipping\n", 
                            key.c_str(), (unsigned long long)size);
                    return false;
                }
                
                MetadataEntry entry;
                entry.key = key;
                entry.type = type;
                entries.push_back(entry);
                return true;
            };
            
            parser.Parse(cb, true); // Skip large strings
        }
        catch (const std::exception& e) {
            UnmapViewOfFile(pData);
            CloseHandle(hMapFile);
            CloseHandle(hFile);
            throw std::runtime_error(std::string("Metadata parse error: ") + e.what());
        }

        // Cleanup
        UnmapViewOfFile(pData);
        CloseHandle(hMapFile);
        CloseHandle(hFile);
        
        return entries;
    }

    static bool IsHighRiskKey(const std::string& key) {
        for (int i = 0; HIGH_RISK_KEYS[i] != nullptr; ++i) {
            if (key == HIGH_RISK_KEYS[i]) return true;
        }
        return false;
    }
};

} // namespace RawrXD::Tools
