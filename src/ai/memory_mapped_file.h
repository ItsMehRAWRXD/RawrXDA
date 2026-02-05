#pragma once
#include <windows.h>
#include <string>
#include <cstdint>
#include <iostream>

/**
 * Enterprise-grade memory-mapped file handler for large GGUF models (3-70GB)
 * Uses Windows memory mapping to access files without loading entirely into RAM
 * Zero-copy access pattern for optimal performance with large model files
 */
class MemoryMappedFile {
private:
    HANDLE fileHandle = INVALID_HANDLE_VALUE;
    HANDLE mappingHandle = nullptr;
    LPVOID mappedData = nullptr;
    size_t fileSize = 0;
    std::string filePath;

public:
    MemoryMappedFile() = default;
    ~MemoryMappedFile() { Close(); }
    
    // Rule of 5 - prevent copying, enable moving
    MemoryMappedFile(const MemoryMappedFile&) = delete;
    MemoryMappedFile& operator=(const MemoryMappedFile&) = delete;
    MemoryMappedFile(MemoryMappedFile&& other) noexcept;
    MemoryMappedFile& operator=(MemoryMappedFile&& other) noexcept;
    
    /**
     * Open and memory-map a file for read-only access
     * @param path Absolute path to file to map
     * @return true if successful, false on error
     */
    bool Open(const std::string& path);
    
    /**
     * Close and unmap the file
     */
    void Close();
    
    /**
     * Get a typed pointer to a structure at given offset
     * @param offset Byte offset into mapped file
     * @return Pointer to structure or nullptr if bounds exceeded
     */
    template<typename T>
    const T* GetStruct(size_t offset) const {
        if (!mappedData || offset + sizeof(T) > fileSize) return nullptr;
        return reinterpret_cast<const T*>(static_cast<char*>(mappedData) + offset);
    }
    
    /**
     * Get a pointer to a memory region at given offset
     * @param offset Byte offset into mapped file
     * @param size Size of region in bytes
     * @return Pointer to region or nullptr if bounds exceeded
     */
    const void* GetRegion(size_t offset, size_t size) const {
        if (!mappedData || offset + size > fileSize) return nullptr;
        return static_cast<char*>(mappedData) + offset;
    }
    
    /**
     * Read a string from mapped memory
     * @param offset Byte offset into mapped file
     * @param length String length in bytes
     * @return String or empty string if bounds exceeded
     */
    std::string GetString(size_t offset, size_t length) const {
        if (!mappedData || offset + length > fileSize) return "";
        return std::string(static_cast<char*>(mappedData) + offset, length);
    }
    
    size_t GetFileSize() const { return fileSize; }
    bool IsOpen() const { return mappedData != nullptr; }
    
    // Memory statistics
    struct MemoryStats {
        size_t totalSize;
        size_t mappedSize;
        bool isMapped;
        std::string filePath;
    };
    
    MemoryStats GetStats() const {
        return {fileSize, fileSize, IsOpen(), filePath};
    }
};
