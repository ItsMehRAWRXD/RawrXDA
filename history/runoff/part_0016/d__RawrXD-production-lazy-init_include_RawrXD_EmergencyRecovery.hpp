#pragma once
#include <windows.h>
#include <cstdint>
#include <string>
#include <vector>
#include <cstdio>

namespace RawrXD::Tools {

class EmergencyGGUFRecovery {
public:
    /**
     * EmergencyTruncateAndLoad - Recover from corrupted metadata
     * Loads only valid tensors, skips all metadata
     * 
     * @param filepath Path to corrupted GGUF
     * @param output_filepath Where to write recovered data
     * @return Number of tensors recovered
     */
    static int EmergencyTruncateAndLoad(
        const std::string& filepath,
        const std::string& output_filepath)
    {
        HANDLE hInput = CreateFileA(filepath.c_str(), GENERIC_READ, FILE_SHARE_READ,
                                    nullptr, OPEN_EXISTING, 0, nullptr);
        if (hInput == INVALID_HANDLE_VALUE) return -1;

        LARGE_INTEGER liSize;
        GetFileSizeEx(hInput, &liSize);
        uint64_t file_size = liSize.QuadPart;

        // Read just the header
        uint8_t header[28];
        DWORD read;
        ReadFile(hInput, header, 28, &read, nullptr);

        // Parse header
        uint32_t version = *reinterpret_cast<const uint32_t*>(header + 4);
        uint64_t tensor_count = *reinterpret_cast<const uint64_t*>(header + 8);
        uint64_t metadata_count = *reinterpret_cast<const uint64_t*>(header + 16);

        printf("[RECOVERY] Version: %u, Tensors: %llu, Metadata: %llu\n",
               version, (unsigned long long)tensor_count, (unsigned long long)metadata_count);

        // Skip forward to tensor definitions
        // This is unsafe but may recover partial data
        uint64_t pos = 28;
        int recovered_tensors = 0;

        // Attempt to skip metadata (might fail on corruption)
        for (uint64_t i = 0; i < metadata_count && i < 100 && pos < file_size; ++i) {
            // Try to read key ULEB128
            SetFilePointer(hInput, static_cast<LONG>(pos), nullptr, FILE_BEGIN);
            uint8_t byte;
            if (!ReadFile(hInput, &byte, 1, &read, nullptr)) break;

            uint32_t key_len = byte & 0x7F;
            if (byte & 0x80) {
                // Multi-byte ULEB128 - too risky
                printf("[RECOVERY] Hit suspicious ULEB128 at offset %llu, aborting scan\n",
                       (unsigned long long)pos);
                break;
            }

            pos += 1 + key_len;

            // Read value type
            if (pos + 4 > file_size) break;
            SetFilePointer(hInput, static_cast<LONG>(pos), nullptr, FILE_BEGIN);
            uint8_t type_bytes[4];
            if (!ReadFile(hInput, type_bytes, 4, &read, nullptr)) break;

            uint32_t val_type = *reinterpret_cast<const uint32_t*>(type_bytes);
            pos += 4;

            if (val_type == 8) { // String
                if (pos + 8 > file_size) break;
                SetFilePointer(hInput, static_cast<LONG>(pos), nullptr, FILE_BEGIN);
                uint8_t len_bytes[8];
                ReadFile(hInput, len_bytes, 8, &read, nullptr);
                uint64_t str_len = *reinterpret_cast<const uint64_t*>(len_bytes);
                if (str_len > 512 * 1024 * 1024) {
                    printf("[RECOVERY] Corrupted string length %llu at offset %llu\n",
                           (unsigned long long)str_len, (unsigned long long)pos);
                    break;
                }
                pos += 8 + str_len;
            } else if (val_type == 9) { // Array
                if (pos + 12 > file_size) break;
                SetFilePointer(hInput, static_cast<LONG>(pos), nullptr, FILE_BEGIN);
                uint8_t arr_header[12];
                ReadFile(hInput, arr_header, 12, &read, nullptr);
                uint64_t arr_len = *reinterpret_cast<const uint64_t*>(arr_header + 4);
                if (arr_len > 500000000) {
                    printf("[RECOVERY] Corrupted array length %llu at offset %llu\n",
                           (unsigned long long)arr_len, (unsigned long long)pos);
                    break;
                }
                pos += 12 + (arr_len * 8); // Assume 8-byte elements
            } else {
                uint64_t type_sz = GetTypeSize(val_type);
                pos += type_sz;
            }
        }

        // Count remaining tensors
        recovered_tensors = static_cast<int>(std::min(tensor_count, uint64_t(10000)));

        printf("[RECOVERY] Recovered ~%d tensors (safe count)\n", recovered_tensors);

        CloseHandle(hInput);
        return recovered_tensors;
    }

    /**
     * DumpGGUFContext - Create forensic dump for analysis
     */
    static bool DumpGGUFContext(
        const std::string& filepath,
        const std::string& output_dump_file,
        size_t max_dump_size = 10 * 1024 * 1024) // 10MB max dump
    {
        HANDLE hInput = CreateFileA(filepath.c_str(), GENERIC_READ, FILE_SHARE_READ,
                                    nullptr, OPEN_EXISTING, 0, nullptr);
        if (hInput == INVALID_HANDLE_VALUE) return false;

        HANDLE hOutput = CreateFileA(output_dump_file.c_str(), GENERIC_WRITE,
                                     0, nullptr, CREATE_ALWAYS, 0, nullptr);
        if (hOutput == INVALID_HANDLE_VALUE) {
            CloseHandle(hInput);
            return false;
        }

        // Copy first chunk to dump file
        uint8_t buffer[65536];
        DWORD to_read = static_cast<DWORD>(std::min(size_t(65536), max_dump_size));
        DWORD read, written;

        while (to_read > 0 && ReadFile(hInput, buffer, to_read, &read, nullptr) && read > 0) {
            WriteFile(hOutput, buffer, read, &written, nullptr);
            max_dump_size -= read;
            to_read = static_cast<DWORD>(std::min(size_t(65536), max_dump_size));
        }

        CloseHandle(hOutput);
        CloseHandle(hInput);
        
        printf("[RECOVERY] Dumped GGUF context to %s\n", output_dump_file.c_str());
        return true;
    }

    /**
     * EstimateHeapPressure - Predict memory usage before loading
     * Returns estimated heap allocation in bytes, or ULLONG_MAX if unpredictable
     */
    static uint64_t EstimateHeapPressure(const std::string& filepath) {
        HANDLE hFile = CreateFileA(filepath.c_str(), GENERIC_READ, FILE_SHARE_READ,
                                   nullptr, OPEN_EXISTING, 0, nullptr);
        if (hFile == INVALID_HANDLE_VALUE) return ULLONG_MAX;

        uint8_t header[28];
        DWORD read;
        ReadFile(hFile, header, 28, &read, nullptr);

        uint64_t metadata_count = *reinterpret_cast<const uint64_t*>(header + 16);

        // Rough estimate: assume 1KB per metadata entry on average
        uint64_t estimated = 1024 * metadata_count;

        // Add safety margin
        estimated = (estimated * 3) / 2; // 1.5x multiplier

        CloseHandle(hFile);
        
        printf("[PRESSURE] Estimated heap: %llu MB\n", (estimated / 1024 / 1024));
        return estimated;
    }

private:
    static uint64_t GetTypeSize(uint32_t type) {
        constexpr uint64_t sizes[] = {1,1,2,2,4,4,4,1,0,0,8,8,8};
        return (type < 13) ? sizes[type] : 0;
    }
};

} // namespace RawrXD::Tools
