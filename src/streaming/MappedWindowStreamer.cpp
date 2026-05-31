#include "MappedWindowStreamer.hpp"
#include "utils/Diagnostics.hpp"

namespace RawrXD
{

MappedWindowStreamer::MappedWindowStreamer(size_t window_size_mb)
    : file_handle_(INVALID_HANDLE_VALUE), file_mapping_(nullptr), current_view_(nullptr), current_view_offset_(0),
      current_view_size_(0), file_size_(0), window_size_bytes_(window_size_mb * 1024 * 1024)
{
}

MappedWindowStreamer::~MappedWindowStreamer()
{
    Close();
}

bool MappedWindowStreamer::Open(const std::string& filepath)
{
    filepath_ = filepath;

    // Open file for reading
    file_handle_ = CreateFileA(filepath.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
                               FILE_ATTRIBUTE_NORMAL, nullptr);

    if (file_handle_ == INVALID_HANDLE_VALUE)
    {
        Diagnostics::error("MappedWindowStreamer failed to open: " + filepath, "MappedWindowStreamer");
        return false;
    }

    // Get file size
    LARGE_INTEGER size;
    if (!GetFileSizeEx(file_handle_, &size))
    {
        Diagnostics::error("MappedWindowStreamer: GetFileSizeEx failed", "MappedWindowStreamer");
        CloseHandles();
        return false;
    }
    file_size_ = static_cast<uint64_t>(size.QuadPart);

    // Create file mapping (read-only, entire file)
    file_mapping_ = CreateFileMappingA(file_handle_, nullptr, PAGE_READONLY, size.HighPart, size.LowPart, nullptr);

    if (!file_mapping_)
    {
        Diagnostics::error("MappedWindowStreamer: CreateFileMapping failed", "MappedWindowStreamer");
        CloseHandles();
        return false;
    }

    {
        const double gib = file_size_ / (1024.0 * 1024.0 * 1024.0);
        const double winMb = window_size_bytes_ / (1024.0 * 1024.0);
        Diagnostics::info("MappedWindowStreamer opened " + filepath + " size_gib=" + std::to_string(gib) +
                              " window_mb=" + std::to_string(winMb),
                          "MappedWindowStreamer");
    }

    return true;
}

bool MappedWindowStreamer::Close()
{
    UnmapRegion();
    CloseHandles();
    filepath_.clear();
    file_size_ = 0;
    return true;
}

uint8_t* MappedWindowStreamer::MapRegion(uint64_t offset, size_t size)
{
    if (file_mapping_ == nullptr)
    {
        Diagnostics::error("MappedWindowStreamer: Not open", "MappedWindowStreamer");
        return nullptr;
    }

    if (size == 0)
    {
        size = window_size_bytes_;
    }

    // Ensure we don't map past EOF
    if (offset + size > file_size_)
    {
        size = static_cast<size_t>(file_size_ - offset);
    }

    if (!MapViewAtOffset(offset, size))
    {
        return nullptr;
    }

    return static_cast<uint8_t*>(current_view_);
}

void MappedWindowStreamer::UnmapRegion()
{
    if (current_view_)
    {
        UnmapViewOfFile(current_view_);
        current_view_ = nullptr;
        current_view_offset_ = 0;
        current_view_size_ = 0;
    }
}

MappedWindowStreamer::MappedView MappedWindowStreamer::GetCurrentView() const
{
    return MappedView{static_cast<uint8_t*>(current_view_), current_view_size_, current_view_offset_};
}

bool MappedWindowStreamer::IsValidRange(uint64_t offset, size_t size) const
{
    if (offset >= file_size_)
        return false;
    if (offset + size > file_size_)
        return false;
    return true;
}

void MappedWindowStreamer::CloseHandles()
{
    UnmapRegion();

    if (file_mapping_)
    {
        CloseHandle(file_mapping_);
        file_mapping_ = nullptr;
    }

    if (file_handle_ != INVALID_HANDLE_VALUE)
    {
        CloseHandle(file_handle_);
        file_handle_ = INVALID_HANDLE_VALUE;
    }
}

bool MappedWindowStreamer::MapViewAtOffset(uint64_t offset, size_t size)
{
    // Unmap previous view
    UnmapRegion();

    // Windows requires offset to be aligned to allocation granularity (typically 64KB)
    // For safety, we align down and adjust the view size
    SYSTEM_INFO system_info;
    GetSystemInfo(&system_info);
    DWORD alloc_granularity = system_info.dwAllocationGranularity;

    uint64_t aligned_offset = (offset / alloc_granularity) * alloc_granularity;
    size_t offset_delta = static_cast<size_t>(offset - aligned_offset);

    // Map the view
    DWORD offset_high = static_cast<DWORD>((aligned_offset >> 32) & 0xFFFFFFFF);
    DWORD offset_low = static_cast<DWORD>(aligned_offset & 0xFFFFFFFF);

    void* view = MapViewOfFile(file_mapping_, FILE_MAP_READ, offset_high, offset_low, size + offset_delta);

    if (!view)
    {
        Diagnostics::error("MapViewOfFile failed at offset " + std::to_string(offset), "MappedWindowStreamer");
        return false;
    }

    current_view_ = static_cast<uint8_t*>(view) + offset_delta;
    current_view_offset_ = offset;
    current_view_size_ = size;

    return true;
}

}  // namespace RawrXD
