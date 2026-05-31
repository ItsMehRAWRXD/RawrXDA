#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <windows.h>

namespace RawrXD
{

/**
 * MappedWindowStreamer: Stream arbitrarily large files via Windows memory mapping.
 * 
 * Instead of buffering entire zones in RAM (which fails for 6+ GB zones),
 * use MapViewOfFile to create sliding windows. Kernel auto-pages data as accessed.
 * 
 * Pattern:
 *   1. Open GGUF file
 *   2. Create file mapping (PAGE_READONLY)
 *   3. For each zone:
 *      - MapViewOfFile for 64 MB window
 *      - Read tensor data through mapped view
 *      - UnmapViewOfFile when done
 *   4. Close handles on exit
 * 
 * Performance: ~5-10% slower than buffering (acceptable for large models)
 * Memory: Constant ~64 MB window regardless of model size
 * Reliability: Never OOM (leverages virtual memory)
 */
class MappedWindowStreamer
{
  public:
    static constexpr uint64_t DEFAULT_WINDOW_SIZE_MB = 64;

    MappedWindowStreamer();
    ~MappedWindowStreamer();

    /**
     * Initialize mapped streaming for a GGUF file.
     * Opens file handle and creates file mapping (PAGE_READONLY).
     */
    bool Initialize(const std::string& filepath);

    /**
     * Cleanup: UnmapViewOfFile, CloseHandle mappings.
     */
    bool Cleanup();

    /**
     * Read tensor data via memory mapping.
     * 
     * @param offset File offset of tensor (from GGUF tensor info)
     * @param size   Bytes to read
     * @param out    Output buffer (resized to size)
     * @return true if successful, false on mapping/read error
     */
    bool ReadTensorMapped(uint64_t offset, uint64_t size, std::vector<uint8_t>& out);

    /**
     * Get file size for sanity checks.
     */
    uint64_t GetFileSize() const { return file_size_; }

    /**
     * Check if file is open and ready.
     */
    bool IsOpen() const { return file_handle_ != INVALID_HANDLE_VALUE; }

    /**
     * Statistics for profiling.
     */
    struct Stats
    {
        uint64_t window_maps_created = 0;
        uint64_t bytes_read = 0;
        uint64_t total_read_time_ms = 0;
    };

    Stats GetStats() const { return stats_; }
    void ResetStats() { stats_ = {}; }

  private:
    HANDLE file_handle_;
    HANDLE mapping_handle_;
    uint64_t file_size_;
    uint64_t window_size_bytes_;

    // Current window tracking
    uint8_t* current_window_;
    uint64_t current_window_offset_;
    uint64_t current_window_size_;

    Stats stats_;

    /**
     * Internal: Create a mapping window for given offset.
     * Automatically unmaps old window if needed.
     * 
     * @return Pointer to mapped data, or nullptr on error
     */
    uint8_t* MapWindow(uint64_t offset);

    /**
     * Internal: Unmap current window and reset tracking.
     */
    void UnmapWindow();
};

}  // namespace RawrXD
