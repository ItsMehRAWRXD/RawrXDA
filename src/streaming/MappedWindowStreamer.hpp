#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <windows.h>


namespace RawrXD
{
/**
 * @brief Windows file mapping-based streaming for large GGUF files.
 *
 * Uses CreateFileMapping + MapViewOfFile to handle zone streaming
 * without copying entire zones to RAM. Dramatically reduces memory
 * overhead for models with large tensor layers (>2 GiB).
 */
class MappedWindowStreamer
{
  public:
    // Window size: 64 MB default (configurable for tuning)
    static constexpr size_t DEFAULT_WINDOW_SIZE_MB = 64;
    static constexpr size_t DEFAULT_WINDOW_BYTES = 64 * 1024 * 1024;

    explicit MappedWindowStreamer(size_t window_size_mb = DEFAULT_WINDOW_SIZE_MB);
    ~MappedWindowStreamer();

    /**
     * Open a GGUF file for mapped streaming.
     * Creates file mapping object but does NOT map entire file.
     */
    bool Open(const std::string& filepath);

    /**
     * Close mapped file and release all views.
     */
    bool Close();

    /**
     * Map a region of the file into virtual address space.
     * Only one view is active at a time (for efficiency).
     *
     * @param offset File byte offset (aligned to allocation granularity)
     * @param size Number of bytes to map (default: full window)
     * @return Pointer to mapped data, or nullptr on failure
     */
    uint8_t* MapRegion(uint64_t offset, size_t size = 0);

    /**
     * Unmap the current view and prepare for next region.
     * Safe to call even if no view is currently mapped.
     */
    void UnmapRegion();

    /**
     * Get current mapped view pointer and size.
     */
    struct MappedView
    {
        uint8_t* ptr;
        size_t size;
        uint64_t file_offset;
    };
    MappedView GetCurrentView() const;

    /**
     * Get file size.
     */
    uint64_t GetFileSize() const { return file_size_; }

    /**
     * Check if offset + size are within valid range.
     */
    bool IsValidRange(uint64_t offset, size_t size) const;

  private:
    std::string filepath_;
    HANDLE file_handle_;
    HANDLE file_mapping_;
    void* current_view_;
    uint64_t current_view_offset_;
    size_t current_view_size_;
    uint64_t file_size_;
    size_t window_size_bytes_;

    void CloseHandles();
    bool MapViewAtOffset(uint64_t offset, size_t size);
};

}  // namespace RawrXD
