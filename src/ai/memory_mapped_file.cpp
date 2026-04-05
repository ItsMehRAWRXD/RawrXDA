#include "memory_mapped_file.h"
#include <algorithm>
#include <stdexcept>

MemoryMappedFile::MemoryMappedFile(MemoryMappedFile&& other) noexcept 
    : fileHandle(other.fileHandle), mappingHandle(other.mappingHandle), 
            mappedViewBase(other.mappedViewBase), mappedData(other.mappedData), fileSize(other.fileSize), filePath(std::move(other.filePath)),
            mappedOffset(other.mappedOffset), mappedViewSize(other.mappedViewSize),
            allocationGranularity(other.allocationGranularity), preferredWindowSize(other.preferredWindowSize) {
    other.fileHandle = INVALID_HANDLE_VALUE;
    other.mappingHandle = nullptr;
        other.mappedViewBase = nullptr;
    other.mappedData = nullptr;
    other.fileSize = 0;
        other.mappedOffset = 0;
        other.mappedViewSize = 0;
}

MemoryMappedFile& MemoryMappedFile::operator=(MemoryMappedFile&& other) noexcept {
    if (this != &other) {
        Close();
        fileHandle = other.fileHandle;
        mappingHandle = other.mappingHandle;
        mappedViewBase = other.mappedViewBase;
        mappedData = other.mappedData;
        fileSize = other.fileSize;
        filePath = std::move(other.filePath);
        mappedOffset = other.mappedOffset;
        mappedViewSize = other.mappedViewSize;
        allocationGranularity = other.allocationGranularity;
        preferredWindowSize = other.preferredWindowSize;
        
        other.fileHandle = INVALID_HANDLE_VALUE;
        other.mappingHandle = nullptr;
        other.mappedViewBase = nullptr;
        other.mappedData = nullptr;
        other.fileSize = 0;
        other.mappedOffset = 0;
        other.mappedViewSize = 0;
    }
    return *this;
}

bool MemoryMappedFile::EnsureMappedRange(size_t offset, size_t size) const {
    if (!mappingHandle || size == 0) {
        return false;
    }

    const uint64_t requestStart = static_cast<uint64_t>(offset);
    const uint64_t requestEnd = requestStart + static_cast<uint64_t>(size);
    const uint64_t currentStart = mappedOffset;
    const uint64_t currentEnd = currentStart + static_cast<uint64_t>(mappedViewSize);
    if (mappedData && requestStart >= currentStart && requestEnd <= currentEnd) {
        return true;
    }

    const uint64_t granularity = static_cast<uint64_t>(std::max<size_t>(allocationGranularity, 64 * 1024));
    const uint64_t alignedStart = (requestStart / granularity) * granularity;
    uint64_t desiredSize = static_cast<uint64_t>(std::max(preferredWindowSize, size));
    if (desiredSize % granularity != 0) {
        desiredSize += granularity - (desiredSize % granularity);
    }
    const uint64_t maxSize = static_cast<uint64_t>(fileSize) - alignedStart;
    const uint64_t alignedSize = std::min(desiredSize, maxSize);
    if (alignedSize == 0) {
        return false;
    }

    if (mappedViewBase) {
        if (!UnmapViewOfFile(mappedViewBase)) {
            DWORD error = GetLastError();
            std::cerr << "[MemoryMappedFile] Failed to unmap sliding view (Error: " << error << ")" << std::endl;
            return false;
        }
        mappedViewBase = nullptr;
        mappedData = nullptr;
        mappedOffset = 0;
        mappedViewSize = 0;
    }

    void* view = MapViewOfFile(mappingHandle, FILE_MAP_READ,
        static_cast<DWORD>(alignedStart >> 32),
        static_cast<DWORD>(alignedStart & 0xFFFFFFFFULL),
        static_cast<SIZE_T>(alignedSize));
    if (!view) {
        DWORD error = GetLastError();
        std::cerr << "[MemoryMappedFile] Failed to map sliding view for: " << filePath
                  << " (Error: " << error << ")" << std::endl;
        return false;
    }

    mappedViewBase = view;
    mappedData = static_cast<char*>(view) + static_cast<size_t>(requestStart - alignedStart);
    mappedOffset = alignedStart;
    mappedViewSize = static_cast<size_t>(alignedSize);
    return true;
}

bool MemoryMappedFile::Open(const std::string& path) {
    Close();
    filePath = path;
    
    // Open file for reading with FILE_SHARE_READ to allow concurrent access
    fileHandle = CreateFileA(path.c_str(), GENERIC_READ, FILE_SHARE_READ, 
                            nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    
    if (fileHandle == INVALID_HANDLE_VALUE) {
        DWORD error = GetLastError();
        std::cerr << "[MemoryMappedFile] Failed to open file: " << path 
                  << " (Error: " << error << ")" << std::endl;
        return false;
    }
    
    // Get file size using GetFileSizeEx for large file support (>4GB)
    LARGE_INTEGER fileSizeLI;
    if (!GetFileSizeEx(fileHandle, &fileSizeLI)) {
        std::cerr << "[MemoryMappedFile] Failed to get file size for: " << path << std::endl;
        Close();
        return false;
    }
    fileSize = static_cast<size_t>(fileSizeLI.QuadPart);
    
    if (fileSize == 0) {
        std::cerr << "[MemoryMappedFile] File is empty: " << path << std::endl;
        Close();
        return false;
    }

    SYSTEM_INFO sysInfo{};
    GetSystemInfo(&sysInfo);
    allocationGranularity = std::max<size_t>(sysInfo.dwAllocationGranularity, 64 * 1024);
    
    // Create file mapping object with PAGE_READONLY
    mappingHandle = CreateFileMapping(fileHandle, nullptr, PAGE_READONLY, 
                                     0, 0, nullptr);
    
    if (mappingHandle == nullptr) {
        DWORD error = GetLastError();
        std::cerr << "[MemoryMappedFile] Failed to create file mapping for: " << path 
                  << " (Error: " << error << ")" << std::endl;
        Close();
        return false;
    }
    
    std::cout << "[MemoryMappedFile] Opened sliding map: " << path 
              << " (" << (fileSize / 1024 / 1024) << " MB)" << std::endl;
    return true;
}

void MemoryMappedFile::Close() {
    if (mappedViewBase != nullptr) {
        if (!UnmapViewOfFile(mappedViewBase)) {
            DWORD error = GetLastError();
            std::cerr << "[MemoryMappedFile] Failed to unmap view (Error: " << error << ")" << std::endl;
        }
        mappedViewBase = nullptr;
        mappedData = nullptr;
    }
    mappedOffset = 0;
    mappedViewSize = 0;
    
    if (mappingHandle != nullptr) {
        if (!CloseHandle(mappingHandle)) {
            DWORD error = GetLastError();
            std::cerr << "[MemoryMappedFile] Failed to close mapping handle (Error: " << error << ")" << std::endl;
        }
        mappingHandle = nullptr;
    }
    
    if (fileHandle != INVALID_HANDLE_VALUE) {
        if (!CloseHandle(fileHandle)) {
            DWORD error = GetLastError();
            std::cerr << "[MemoryMappedFile] Failed to close file handle (Error: " << error << ")" << std::endl;
        }
        fileHandle = INVALID_HANDLE_VALUE;
    }
    
    fileSize = 0;
    filePath.clear();
}
