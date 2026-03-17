#pragma once
#include <windows.h>
#include <cstdint>
#include <string>
#include <vector>
#include <functional>

namespace RawrXD::Tools {

class GGUFCorruptionDetector {
public:
    struct CorruptionReport {
        bool is_valid = true;
        std::vector<std::string> warnings;
        std::vector<std::string> errors;
        
        uint64_t file_size = 0;
        uint32_t tensor_count = 0;
        uint64_t metadata_count = 0;
        
        struct SuspiciousEntry {
            std::string key;
            uint32_t type;
            uint64_t size;
            std::string reason;
        };
        std::vector<SuspiciousEntry> suspicious_entries;
    };

    static CorruptionReport ScanFile(const std::string& filepath) {
        CorruptionReport report;
        
        HANDLE hFile = CreateFileA(
            filepath.c_str(),
            GENERIC_READ,
            FILE_SHARE_READ,
            nullptr,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL,
            nullptr);
        
        if (hFile == INVALID_HANDLE_VALUE) {
            report.is_valid = false;
            report.errors.push_back("Cannot open file");
            return report;
        }

        LARGE_INTEGER liSize;
        if (!GetFileSizeEx(hFile, &liSize)) {
            CloseHandle(hFile);
            report.is_valid = false;
            report.errors.push_back("Cannot get file size");
            return report;
        }

        report.file_size = liSize.QuadPart;

        // Validate minimum size
        if (report.file_size < 28) {
            CloseHandle(hFile);
            report.is_valid = false;
            report.errors.push_back("File too small for GGUF header");
            return report;
        }

        // Validate 64GB hard limit
        if (report.file_size > 0x1000000000LL) {
            CloseHandle(hFile);
            report.is_valid = false;
            report.errors.push_back("File exceeds 64GB hard limit");
            return report;
        }

        // Read header
        uint8_t header[28];
        DWORD read;
        if (!ReadFile(hFile, header, 28, &read, nullptr) || read != 28) {
            CloseHandle(hFile);
            report.is_valid = false;
            report.errors.push_back("Cannot read GGUF header");
            return report;
        }

        // Check magic
        if (memcmp(header, "GGUF", 4) != 0) {
            report.is_valid = false;
            report.errors.push_back("Invalid GGUF magic bytes");
            CloseHandle(hFile);
            return report;
        }

        uint32_t version = *reinterpret_cast<const uint32_t*>(header + 4);
        if (version < 2) {
            report.warnings.push_back("GGUF version < 2 (older format)");
        }

        uint64_t tensor_count = *reinterpret_cast<const uint64_t*>(header + 8);
        uint64_t metadata_count = *reinterpret_cast<const uint64_t*>(header + 16);

        report.tensor_count = static_cast<uint32_t>(tensor_count);
        report.metadata_count = metadata_count;

        // Validate tensor count
        if (tensor_count > 100000) {
            report.warnings.push_back("Suspiciously high tensor count: " + std::to_string(tensor_count));
        }

        // Validate metadata count
        if (metadata_count > 10000) {
            report.warnings.push_back("Suspiciously high metadata count: " + std::to_string(metadata_count));
        }

        // Scan metadata entries (simplified)
        uint64_t pos = 28;
        for (uint64_t i = 0; i < metadata_count && i < 1000; ++i) { // Limit scan to first 1000
            if (pos + 8 > report.file_size) {
                report.errors.push_back("Metadata extends past EOF");
                report.is_valid = false;
                break;
            }

            // Read key length (ULEB128)
            uint8_t key_len_byte;
            DWORD read;
            
            SetFilePointer(hFile, pos, nullptr, FILE_BEGIN);
            if (!ReadFile(hFile, &key_len_byte, 1, &read, nullptr)) {
                break;
            }
            
            uint32_t key_len = key_len_byte;
            if (key_len > 4096) {
                report.warnings.push_back("Suspiciously long metadata key at offset " + 
                                         std::to_string(pos));
            }

            pos += 1 + key_len; // ULEB128 + key
            
            if (pos + 4 > report.file_size) {
                report.errors.push_back("Metadata value type exceeds file bounds");
                break;
            }

            // Read value type
            uint8_t type_bytes[4];
            SetFilePointer(hFile, pos, nullptr, FILE_BEGIN);
            if (!ReadFile(hFile, type_bytes, 4, &read, nullptr)) break;
            
            uint32_t val_type = *reinterpret_cast<const uint32_t*>(type_bytes);
            pos += 4;

            // Validate and scan based on type
            if (val_type == 8) { // String
                if (pos + 8 > report.file_size) break;
                
                uint8_t len_bytes[8];
                SetFilePointer(hFile, pos, nullptr, FILE_BEGIN);
                if (!ReadFile(hFile, len_bytes, 8, &read, nullptr)) break;
                
                uint64_t str_len = *reinterpret_cast<const uint64_t*>(len_bytes);
                pos += 8;

                // Check for corruption: absurdly large string
                if (str_len > 512 * 1024 * 1024) { // 512MB
                    CorruptionReport::SuspiciousEntry entry;
                    entry.key = "?UNKNOWN?";
                    entry.type = val_type;
                    entry.size = str_len;
                    entry.reason = "String declares > 512MB";
                    report.suspicious_entries.push_back(entry);
                    report.is_valid = false;
                }

                pos += str_len;
            } else if (val_type == 9) { // Array
                if (pos + 12 > report.file_size) break;
                
                uint8_t arr_header[12];
                SetFilePointer(hFile, pos, nullptr, FILE_BEGIN);
                if (!ReadFile(hFile, arr_header, 12, &read, nullptr)) break;
                
                uint32_t arr_type = *reinterpret_cast<const uint32_t*>(arr_header);
                uint64_t arr_len = *reinterpret_cast<const uint64_t*>(arr_header + 4);
                pos += 12;

                // Check for corruption: millions of elements
                if (arr_len > 500000000) { // 500M elements
                    report.is_valid = false;
                    report.errors.push_back("Array with > 500M elements (likely corruption)");
                }

                if (arr_type != 8) { // Not string array
                    uint64_t type_size = GetTypeSize(arr_type);
                    uint64_t total_size = arr_len * type_size;
                    pos += total_size;
                } else {
                    // String array - skip carefully
                    for (uint64_t j = 0; j < arr_len && pos < report.file_size; ++j) {
                        if (pos + 8 > report.file_size) break;
                        
                        uint8_t s_len_bytes[8];
                        SetFilePointer(hFile, pos, nullptr, FILE_BEGIN);
                        if (!ReadFile(hFile, s_len_bytes, 8, &read, nullptr)) break;
                        
                        uint64_t s_len = *reinterpret_cast<const uint64_t*>(s_len_bytes);
                        pos += 8 + s_len;
                    }
                }
            } else {
                uint64_t type_size = GetTypeSize(val_type);
                pos += type_size;
            }
        }

        CloseHandle(hFile);
        return report;
    }

    static void PrintReport(const CorruptionReport& report) {
        printf("\n=== GGUF Corruption Scan Report ===\n");
        printf("File Size: %llu bytes\n", (unsigned long long)report.file_size);
        printf("Tensor Count: %u\n", report.tensor_count);
        printf("Metadata Count: %llu\n", (unsigned long long)report.metadata_count);
        printf("Valid: %s\n", report.is_valid ? "YES" : "NO");

        if (!report.errors.empty()) {
            printf("\nERRORS:\n");
            for (const auto& e : report.errors) {
                printf("  - %s\n", e.c_str());
            }
        }

        if (!report.warnings.empty()) {
            printf("\nWARNINGS:\n");
            for (const auto& w : report.warnings) {
                printf("  - %s\n", w.c_str());
            }
        }

        if (!report.suspicious_entries.empty()) {
            printf("\nSUSPICIOUS ENTRIES:\n");
            for (const auto& e : report.suspicious_entries) {
                printf("  - Key: %s | Type: %u | Size: %llu | Reason: %s\n",
                       e.key.c_str(), e.type, (unsigned long long)e.size, e.reason.c_str());
            }
        }
    }

private:
    static uint64_t GetTypeSize(uint32_t type) {
        constexpr uint64_t sizes[] = {1,1,2,2,4,4,4,1,0,0,8,8,8};
        return (type < 13) ? sizes[type] : 0;
    }
};

} // namespace RawrXD::Tools
