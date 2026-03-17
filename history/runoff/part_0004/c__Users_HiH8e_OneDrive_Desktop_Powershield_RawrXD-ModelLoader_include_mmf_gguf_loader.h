// ============================================================================
// Memory-Mapped File GGUF Loader - Advanced Streaming Support
// ============================================================================
// 
// This header extends the standard GGUF loader to support memory-mapped files,
// enabling zero-copy access to massive models with minimal memory footprint.
// Works seamlessly with the PowerShell RawrZ-GGUF-MMF.ps1 sharding orchestrator.
//

#pragma once

#include "gguf_loader.h"
#include <string>
#include <vector>
#include <memory>
#include <windows.h>

// ============================================================================
// MEMORY-MAPPED FILE GGUF LOADER
// ============================================================================

class MMFGgufLoader : public GGUFLoader
{
public:
    MMFGgufLoader()
        : m_mmfHandle(nullptr),
          m_mmfViewHandle(nullptr),
          m_mappedBase(nullptr),
          m_mappedSize(0),
          m_isMemoryMapped(false)
    {
    }

    ~MMFGgufLoader()
    {
        UnmapMemoryFile();
    }

    // ========================================================================
    // Open GGUF file (standard disk file)
    // ========================================================================
    bool OpenDiskFile(const std::string& filepath)
    {
        try {
            return GGUFLoader::Open(filepath);
        }
        catch (const std::exception& e) {
            std::cerr << "Failed to open disk GGUF: " << e.what() << std::endl;
            return false;
        }
    }

    // ========================================================================
    // Open GGUF via Memory-Mapped File
    // ========================================================================
    bool OpenMemoryMappedFile(const std::string& mmfName, uint64_t expectedSize)
    {
        if (m_isMemoryMapped) {
            UnmapMemoryFile();
        }

        try {
            // Convert to wide string for Windows API
            std::wstring mmfNameW(mmfName.begin(), mmfName.end());

            // Open existing memory-mapped file
            m_mmfHandle = OpenFileMappingW(
                FILE_MAP_READ,
                FALSE,
                mmfNameW.c_str()
            );

            if (!m_mmfHandle) {
                std::cerr << "Failed to open MMF: " << mmfName << std::endl;
                return false;
            }

            // Map the entire MMF into address space
            m_mmfViewHandle = MapViewOfFile(
                m_mmfHandle,
                FILE_MAP_READ,
                0, 0,  // Full mapping from offset 0
                0      // Full size
            );

            if (!m_mmfViewHandle) {
                CloseHandle(m_mmfHandle);
                m_mmfHandle = nullptr;
                std::cerr << "Failed to map MMF into address space" << std::endl;
                return false;
            }

            m_mappedBase = reinterpret_cast<uint8_t*>(m_mmfViewHandle);
            m_mappedSize = expectedSize;
            m_isMemoryMapped = true;

            // Parse header from MMF
            if (!ParseHeaderFromMemory()) {
                UnmapMemoryFile();
                return false;
            }

            std::cout << "Opened MMF '" << mmfName << "': "
                      << (m_mappedSize / (1024 * 1024)) << " MB mapped" << std::endl;

            return true;
        }
        catch (const std::exception& e) {
            std::cerr << "Exception opening MMF: " << e.what() << std::endl;
            UnmapMemoryFile();
            return false;
        }
    }

    // ========================================================================
    // Load tensor directly from MMF without buffering
    // ========================================================================
    bool LoadTensorDirectFromMMF(const std::string& tensorName, 
                                  std::vector<uint8_t>& data)
    {
        if (!m_isMemoryMapped) {
            throw std::runtime_error("Not operating in MMF mode");
        }

        // Find tensor info
        auto tensors = GetAllTensorInfo();
        auto it = std::find_if(tensors.begin(), tensors.end(),
                              [&tensorName](const TensorInfo& t) {
                                  return t.name == tensorName;
                              });

        if (it == tensors.end()) {
            throw std::runtime_error("Tensor not found: " + tensorName);
        }

        // Create a view into the MMF (zero-copy read)
        uint8_t* tensorPtr = m_mappedBase + it->offset;
        data.resize(it->size_bytes);
        std::memcpy(data.data(), tensorPtr, it->size_bytes);

        return true;
    }

    // ========================================================================
    // Get direct pointer to tensor data in MMF (dangerous but fast)
    // ========================================================================
    const uint8_t* GetTensorPointerFromMMF(const std::string& tensorName)
    {
        if (!m_isMemoryMapped) {
            return nullptr;
        }

        auto tensors = GetAllTensorInfo();
        auto it = std::find_if(tensors.begin(), tensors.end(),
                              [&tensorName](const TensorInfo& t) {
                                  return t.name == tensorName;
                              });

        if (it == tensors.end()) {
            return nullptr;
        }

        return m_mappedBase + it->offset;
    }

    // ========================================================================
    // Query MMF status
    // ========================================================================
    bool IsMemoryMapped() const
    {
        return m_isMemoryMapped;
    }

    uint64_t GetMMFSize() const
    {
        return m_mappedSize;
    }

    const std::string& GetMMFName() const
    {
        return m_mmfName;
    }

    // ========================================================================
    // Stream tensor from MMF with custom buffer size
    // ========================================================================
    bool StreamTensorFromMMF(const std::string& tensorName,
                            uint64_t bufferSize,
                            std::function<void(const uint8_t*, uint64_t)> callback)
    {
        if (!m_isMemoryMapped) {
            throw std::runtime_error("Not operating in MMF mode");
        }

        auto tensors = GetAllTensorInfo();
        auto it = std::find_if(tensors.begin(), tensors.end(),
                              [&tensorName](const TensorInfo& t) {
                                  return t.name == tensorName;
                              });

        if (it == tensors.end()) {
            throw std::runtime_error("Tensor not found: " + tensorName);
        }

        uint8_t* tensorBase = m_mappedBase + it->offset;
        uint64_t remaining = it->size_bytes;
        uint64_t offset = 0;

        while (remaining > 0) {
            uint64_t chunkSize = std::min(bufferSize, remaining);
            callback(tensorBase + offset, chunkSize);
            offset += chunkSize;
            remaining -= chunkSize;
        }

        return true;
    }

    // ========================================================================
    // Get statistics about MMF usage
    // ========================================================================
    struct MMFStats
    {
        uint64_t totalSize;
        uint64_t tensorCount;
        uint64_t largestTensor;
        uint64_t averageTensorSize;
    };

    MMFStats GetMMFStats() const
    {
        auto tensors = GetAllTensorInfo();
        MMFStats stats{};
        stats.totalSize = m_mappedSize;
        stats.tensorCount = tensors.size();

        if (!tensors.empty()) {
            uint64_t maxSize = 0;
            uint64_t totalTensorSize = 0;

            for (const auto& t : tensors) {
                uint64_t size = t.size_bytes;
                maxSize = std::max(maxSize, size);
                totalTensorSize += size;
            }

            stats.largestTensor = maxSize;
            stats.averageTensorSize = totalTensorSize / stats.tensorCount;
        }

        return stats;
    }

    // ========================================================================
    // Print MMF diagnostic information
    // ========================================================================
    void PrintMMFDiagnostics() const
    {
        if (!m_isMemoryMapped) {
            std::cout << "Not in MMF mode" << std::endl;
            return;
        }

        auto stats = GetMMFStats();

        std::cout << "\n========== MMF Diagnostics ==========" << std::endl;
        std::cout << "MMF Name: " << m_mmfName << std::endl;
        std::cout << "Total Size: " << (stats.totalSize / (1024 * 1024)) << " MB" << std::endl;
        std::cout << "Tensor Count: " << stats.tensorCount << std::endl;
        std::cout << "Largest Tensor: " << (stats.largestTensor / (1024 * 1024)) << " MB" << std::endl;
        std::cout << "Average Tensor Size: " << (stats.averageTensorSize / 1024) << " KB" << std::endl;
        std::cout << "====================================\n" << std::endl;
    }

private:
    // ========================================================================
    // Parse GGUF header from MMF memory
    // ========================================================================
    bool ParseHeaderFromMemory()
    {
        if (!m_mappedBase || m_mappedSize < 32) {
            std::cerr << "MMF too small to contain valid GGUF header" << std::endl;
            return false;
        }

        try {
            // Create a memory stream wrapper for parsing
            class MemoryStream {
            public:
                MemoryStream(const uint8_t* data, uint64_t size)
                    : data_(data), size_(size), pos_(0) {}

                template<typename T>
                bool Read(T& value) {
                    if (pos_ + sizeof(T) > size_) return false;
                    std::memcpy(&value, data_ + pos_, sizeof(T));
                    pos_ += sizeof(T);
                    return true;
                }

                bool ReadString(std::string& value) {
                    uint64_t len = 0;
                    if (!Read(len)) return false;
                    if (pos_ + len > size_) return false;
                    value.assign(reinterpret_cast<const char*>(data_ + pos_), len);
                    pos_ += len;
                    return true;
                }

            private:
                const uint8_t* data_;
                uint64_t size_;
                uint64_t pos_;
            };

            MemoryStream stream(m_mappedBase, m_mappedSize);

            // Read and validate GGUF header
            uint32_t magic;
            if (!stream.Read(magic)) return false;
            if (magic != 0x46554747) {  // "GGUF"
                std::cerr << "Invalid GGUF magic in MMF" << std::endl;
                return false;
            }

            uint32_t version;
            if (!stream.Read(version)) return false;
            if (version != 3) {
                std::cerr << "Unsupported GGUF version: " << version << std::endl;
                return false;
            }

            std::cout << "MMF GGUF header validated successfully" << std::endl;
            return true;
        }
        catch (const std::exception& e) {
            std::cerr << "Error parsing MMF header: " << e.what() << std::endl;
            return false;
        }
    }

    // ========================================================================
    // Clean up MMF resources
    // ========================================================================
    void UnmapMemoryFile()
    {
        if (m_mmfViewHandle) {
            UnmapViewOfFile(m_mmfViewHandle);
            m_mmfViewHandle = nullptr;
            m_mappedBase = nullptr;
        }

        if (m_mmfHandle) {
            CloseHandle(m_mmfHandle);
            m_mmfHandle = nullptr;
        }

        m_mappedSize = 0;
        m_isMemoryMapped = false;
    }

    // ========================================================================
    // Member variables
    // ========================================================================
    HANDLE m_mmfHandle;
    HANDLE m_mmfViewHandle;
    uint8_t* m_mappedBase;
    uint64_t m_mappedSize;
    bool m_isMemoryMapped;
    std::string m_mmfName;
};

#endif // MMF_GGUF_LOADER_H
