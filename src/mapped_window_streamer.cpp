#include "mapped_window_streamer.h"
#include <iostream>
#include <cstring>
#include <algorithm>

namespace RawrXD
{

MappedWindowStreamer::MappedWindowStreamer()
    : file_handle_(INVALID_HANDLE_VALUE)
    , mapping_handle_(nullptr)
    , file_size_(0)
    , window_size_bytes_(DEFAULT_WINDOW_SIZE_MB * 1024 * 1024)
    , current_window_(nullptr)
    , current_window_offset_(0)
    , current_window_size_(0)
{
}

MappedWindowStreamer::~MappedWindowStreamer()
{
    Cleanup();
}

bool MappedWindowStreamer::Initialize(const std::string& filepath)
{
    if (file_handle_ != INVALID_HANDLE_VALUE)
    {
        std::cerr << "❌ MappedWindowStreamer already initialized" << std::endl;
        return false;
    }

    // Open file for reading
    file_handle_ = CreateFileA(
        filepath.c_str(),
        GENERIC_READ,
        FILE_SHARE_READ,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        nullptr
    );

    if (file_handle_ == INVALID_HANDLE_VALUE)
    {
        std::cerr << "❌ Failed to open file: " << filepath << " (Error: " << GetLastError() << ")" << std::endl;
        return false;
    }

    // Get file size
    LARGE_INTEGER size;
    if (!GetFileSizeEx(file_handle_, &size))
    {
        std::cerr << "❌ Failed to get file size (Error: " << GetLastError() << ")" << std::endl;
        CloseHandle(file_handle_);
        file_handle_ = INVALID_HANDLE_VALUE;
        return false;
    }

    file_size_ = static_cast<uint64_t>(size.QuadPart);

    // Create file mapping (PAGE_READONLY for safety)
    mapping_handle_ = CreateFileMappingA(
        file_handle_,
        nullptr,
        PAGE_READONLY,
        0,  // High 32 bits of max size
        0,  // Low 32 bits of max size (0 = use file size)
        nullptr
    );

    if (!mapping_handle_)
    {
        std::cerr << "❌ Failed to create file mapping (Error: " << GetLastError() << ")" << std::endl;
        CloseHandle(file_handle_);
        file_handle_ = INVALID_HANDLE_VALUE;
        return false;
    }

    std::cout << "✅ MappedWindowStreamer initialized:" << std::endl;
    std::cout << "   File: " << filepath << std::endl;
    std::cout << "   Size: " << (file_size_ / (1024.0 * 1024.0)) << " MB" << std::endl;
    std::cout << "   Window: " << window_size_bytes_ / (1024 * 1024) << " MB" << std::endl;

    return true;
}

bool MappedWindowStreamer::Cleanup()
{
    UnmapWindow();

    if (mapping_handle_)
    {
        CloseHandle(mapping_handle_);
        mapping_handle_ = nullptr;
    }

    if (file_handle_ != INVALID_HANDLE_VALUE)
    {
        CloseHandle(file_handle_);
        file_handle_ = INVALID_HANDLE_VALUE;
    }

    file_size_ = 0;
    return true;
}

uint8_t* MappedWindowStreamer::MapWindow(uint64_t offset)
{
    // Check bounds
    if (offset >= file_size_)
    {
        std::cerr << "❌ MapWindow: offset " << offset << " beyond file size " << file_size_ << std::endl;
        return nullptr;
    }

    // Calculate window limits
    uint64_t window_end_offset = std::min(offset + window_size_bytes_, file_size_);
    uint64_t window_actual_size = window_end_offset - offset;

    // If already have correct window, return it
    if (current_window_ && offset >= current_window_offset_ &&
        offset + window_actual_size <= current_window_offset_ + current_window_size_)
    {
        return current_window_;
    }

    // Unmap old window
    UnmapWindow();

    // MapViewOfFile requires 64-bit offset split
    // High 32 bits go into dwFileOffsetHigh, low 32 bits into dwFileOffsetLow
    LARGE_INTEGER li_offset;
    li_offset.QuadPart = offset;

    current_window_ = static_cast<uint8_t*>(MapViewOfFile(
        mapping_handle_,
        FILE_MAP_READ,
        li_offset.HighPart,  // High 32 bits of offset
        li_offset.LowPart,   // Low 32 bits of offset
        static_cast<SIZE_T>(window_actual_size)
    ));

    if (!current_window_)
    {
        std::cerr << "❌ Failed to map view of file at offset " << offset
                  << " (Error: " << GetLastError() << ")" << std::endl;
        return nullptr;
    }

    current_window_offset_ = offset;
    current_window_size_ = window_actual_size;
    stats_.window_maps_created++;

    return current_window_;
}

void MappedWindowStreamer::UnmapWindow()
{
    if (current_window_)
    {
        UnmapViewOfFile(current_window_);
        current_window_ = nullptr;
        current_window_offset_ = 0;
        current_window_size_ = 0;
    }
}

bool MappedWindowStreamer::ReadTensorMapped(uint64_t offset, uint64_t size, std::vector<uint8_t>& out)
{
    if (!IsOpen())
    {
        std::cerr << "❌ MappedWindowStreamer not initialized" << std::endl;
        return false;
    }

    if (offset + size > file_size_)
    {
        std::cerr << "❌ ReadTensorMapped: offset=" << offset << " size=" << size
                  << " exceeds file size " << file_size_ << std::endl;
        return false;
    }

    // Resize output buffer
    out.clear();
    try
    {
        out.resize(size);
    }
    catch (const std::bad_alloc&)
    {
        std::cerr << "❌ Failed to allocate output buffer (" << (size / (1024.0 * 1024.0))
                  << " MB)" << std::endl;
        return false;
    }

    // Read potentially spans multiple windows: map and read incrementally
    uint64_t bytes_read = 0;

    while (bytes_read < size)
    {
        uint64_t current_offset = offset + bytes_read;
        uint8_t* window = MapWindow(current_offset);

        if (!window)
        {
            std::cerr << "❌ Failed to map window for offset " << current_offset << std::endl;
            out.resize(bytes_read);  // Partial read
            return false;
        }

        // Calculate how much we can read from this window
        uint64_t offset_in_window = current_offset - current_window_offset_;
        uint64_t available_in_window = current_window_size_ - offset_in_window;
        uint64_t to_read = std::min(available_in_window, size - bytes_read);

        // Copy from window to output
        std::memcpy(out.data() + bytes_read, window + offset_in_window, to_read);

        bytes_read += to_read;
    }

    stats_.bytes_read += size;
    return true;
}

}  // namespace RawrXD
