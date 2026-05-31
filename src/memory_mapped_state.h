#pragma once
/**
 * MemoryMappedState - Enhancement #3: Zero-Copy State Access
 * 
 * Provides memory-mapped file access for large state files.
 * Enables fast read-only access without loading into heap.
 * 
 * Symbols: MM_READ, MM_WRITE, MM_COPY_ON_WRITE
 */

#include <string>
#include <cstdint>
#include <memory>
#include <optional>
#include <nlohmann/json.hpp>

// Memory mapping modes
#define MM_READ             0x01  // Read-only mapping
#define MM_WRITE            0x02  // Read-write mapping
#define MM_COPY_ON_WRITE    0x04  // Private copy-on-write
#define MM_SEQUENTIAL       0x08  // Sequential access hint
#define MM_RANDOM           0x10  // Random access hint

// Platform-specific handle types
#ifdef _WIN32
#include <windows.h>
typedef HANDLE MM_HANDLE;
#else
typedef int MM_HANDLE;
#endif

namespace MemoryMappedState {

    /**
     * Memory-mapped file view
     */
    class MappedView {
    public:
        MappedView();
        ~MappedView();

        // Disable copy, enable move
        MappedView(const MappedView&) = delete;
        MappedView& operator=(const MappedView&) = delete;
        MappedView(MappedView&& other) noexcept;
        MappedView& operator=(MappedView&& other) noexcept;

        // Map file into memory
        bool map(const std::string& filePath, uint32_t mode = MM_READ);
        bool map(const std::string& filePath, size_t offset, size_t size, uint32_t mode);

        // Unmap and release resources
        void unmap();

        // Access mapped data
        const void* data() const;
        void* mutableData();
        size_t size() const;
        bool isMapped() const;

        // Flush changes to disk (write mode only)
        bool flush();

        // Prefetch data into memory
        void prefetch(size_t offset, size_t size);

        // Advise on access pattern
        void adviseSequential();
        void adviseRandom();

    private:
        void* m_view = nullptr;
        size_t m_size = 0;
        uint32_t m_mode = 0;
#ifdef _WIN32
        HANDLE m_fileHandle = INVALID_HANDLE_VALUE;
        HANDLE m_mapHandle = nullptr;
#else
        int m_fd = -1;
#endif
    };

    /**
     * Memory-mapped state cache
     * Keeps frequently accessed states mapped
     */
    class MappedStateCache {
    public:
        MappedStateCache();
        ~MappedStateCache();

        // Cache configuration
        void setMaxCachedFiles(size_t maxFiles);
        void setMaxCacheSize(size_t maxBytes);

        // Get or create mapped view
        std::shared_ptr<MappedView> getMappedView(
            const std::string& filePath,
            uint32_t mode = MM_READ);

        // Invalidate cached entry
        void invalidate(const std::string& filePath);

        // Clear all cached entries
        void clear();

        // Cache statistics
        struct Stats {
            size_t hits = 0;
            size_t misses = 0;
            size_t evictions = 0;
            size_t currentSize = 0;
        };
        Stats getStats() const;

    private:
        class Impl;
        std::unique_ptr<Impl> m_impl;
    };

    /**
     * Zero-copy JSON parser for mapped memory
     */
    class MappedJsonParser {
    public:
        // Parse JSON directly from mapped memory
        static nlohmann::json parse(const MappedView& view);
        
        // Validate JSON without full parse
        static bool validate(const MappedView& view);
        
        // Extract specific field without full parse
        static std::optional<nlohmann::json> extractField(
            const MappedView& view,
            const std::string& fieldPath);
    };

    /**
     * System capabilities
     */
    bool isSupported();
    size_t getPageSize();
    size_t getMaxMappingSize();

} // namespace MemoryMappedState
