#include "memory_mapped_file.h"
#include <stdexcept>

MemoryMappedFile::MemoryMappedFile(MemoryMappedFile&& other) noexcept 
    : fileHandle(other.fileHandle), mappingHandle(other.mappingHandle), 
      mappedData(other.mappedData), fileSize(other.fileSize), filePath(std::move(other.filePath)) {
    other.fileHandle = INVALID_HANDLE_VALUE;
    other.mappingHandle = nullptr;
    other.mappedData = nullptr;
    other.fileSize = 0;
}

MemoryMappedFile& MemoryMappedFile::operator=(MemoryMappedFile&& other) noexcept {
    if (this != &other) {
        Close();
        fileHandle = other.fileHandle;
        mappingHandle = other.mappingHandle;
        mappedData = other.mappedData;
        fileSize = other.fileSize;
        filePath = std::move(other.filePath);
        
        other.fileHandle = INVALID_HANDLE_VALUE;
        other.mappingHandle = nullptr;
        other.mappedData = nullptr;
        other.fileSize = 0;
    }
    return *this;
}

bool MemoryMappedFile::Open(const std::string& path) {
    filePath = path;
    
    // Open file for reading with FILE_SHARE_READ to allow concurrent access
    fileHandle = CreateFileA(path.c_str(), GENERIC_READ, FILE_SHARE_READ, 
                            nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    
    if (fileHandle == INVALID_HANDLE_VALUE) {
        DWORD error = GetLastError();
        
        return false;
    }
    
    // Get file size using GetFileSizeEx for large file support (>4GB)
    LARGE_INTEGER fileSizeLI;
    if (!GetFileSizeEx(fileHandle, &fileSizeLI)) {
        
        Close();
        return false;
    }
    fileSize = static_cast<size_t>(fileSizeLI.QuadPart);
    
    if (fileSize == 0) {
        
        Close();
        return false;
    }
    
    // Create file mapping object with PAGE_READONLY
    mappingHandle = CreateFileMapping(fileHandle, nullptr, PAGE_READONLY, 
                                     0, 0, nullptr);
    
    if (mappingHandle == nullptr) {
        DWORD error = GetLastError();
        
        Close();
        return false;
    }
    
    // Map the entire file into memory with FILE_MAP_READ
    // Windows will handle paging - only active pages will be in RAM
    mappedData = MapViewOfFile(mappingHandle, FILE_MAP_READ, 0, 0, 0);
    
    if (mappedData == nullptr) {
        DWORD error = GetLastError();
        
        Close();
        return false;
    }


    return true;
}

void MemoryMappedFile::Close() {
    if (mappedData != nullptr) {
        if (!UnmapViewOfFile(mappedData)) {
            DWORD error = GetLastError();
            
        }
        mappedData = nullptr;
    }
    
    if (mappingHandle != nullptr) {
        if (!CloseHandle(mappingHandle)) {
            DWORD error = GetLastError();
            
        }
        mappingHandle = nullptr;
    }
    
    if (fileHandle != INVALID_HANDLE_VALUE) {
        if (!CloseHandle(fileHandle)) {
            DWORD error = GetLastError();
            
        }
        fileHandle = INVALID_HANDLE_VALUE;
    }
    
    fileSize = 0;
    filePath.clear();
}
