#pragma once
#include <windows.h>
#include <cstdint>
#include <string>
#include <stdexcept>

class MMapFile {
public:
    struct Mapping {
        HANDLE hFile = INVALID_HANDLE_VALUE;
        HANDLE hMapping = nullptr;
        void* baseAddr = nullptr;
        uint64_t fileSize = 0;
        uint32_t largePageGranularity = 0;
        bool usingLargePages = false;
    };

    static Mapping Open(const std::wstring& path, bool write = false, 
                       bool largePages = false) {
        Mapping m;
        
        DWORD access = write ? (GENERIC_READ | GENERIC_WRITE) : GENERIC_READ;
        DWORD share = FILE_SHARE_READ;
        DWORD create = OPEN_EXISTING;
        DWORD flags = FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN;
        
        if (largePages) {
            flags |= FILE_FLAG_RANDOM_ACCESS; // Hint for large pages
        }
        
        m.hFile = CreateFileW(path.c_str(), access, share, nullptr, create, flags, nullptr);
        if (m.hFile == INVALID_HANDLE_VALUE) {
            throw std::runtime_error("Failed to open file: " + std::to_string(GetLastError()));
        }
        
        // Get file size (supporting >4GB)
        LARGE_INTEGER size;
        if (!GetFileSizeEx(m.hFile, &size)) {
            CloseHandle(m.hFile);
            throw std::runtime_error("Failed to get file size");
        }
        m.fileSize = size.QuadPart;
        
        // Create file mapping
        DWORD protect = write ? PAGE_READWRITE : PAGE_READONLY;
        DWORD mapAccess = write ? FILE_MAP_ALL_ACCESS : FILE_MAP_READ;
        
        // Attempt large pages if requested and available
        if (largePages && size.QuadPart > (1ULL << 30)) { // >1GB
            SIZE_T largePageSize = GetLargePageMinimum();
            if (largePageSize > 0) {
                // Requires SeLockMemoryPrivilege - fallback to normal if fails
                m.hMapping = CreateFileMapping(m.hFile, nullptr, 
                    protect | SEC_LARGE_PAGES, 0, 0, nullptr);
                if (m.hMapping) {
                    m.usingLargePages = true;
                    m.largePageGranularity = static_cast<uint32_t>(largePageSize);
                }
            }
        }
        
        if (!m.hMapping) {
            m.hMapping = CreateFileMapping(m.hFile, nullptr, protect, 
                size.HighPart, size.LowPart, nullptr);
        }
        
        if (!m.hMapping) {
            CloseHandle(m.hFile);
            throw std::runtime_error("CreateFileMapping failed");
        }
        
        // Map view - use LARGE_INTEGER for 64-bit offset
        m.baseAddr = MapViewOfFile(m.hMapping, mapAccess, 0, 0, 0);
        if (!m.baseAddr) {
            CloseHandle(m.hMapping);
            CloseHandle(m.hFile);
            throw std::runtime_error("MapViewOfFile failed");
        }
        
        return m;
    }
    
    static void Close(Mapping& m) {
        if (m.baseAddr) {
            UnmapViewOfFile(m.baseAddr);
            m.baseAddr = nullptr;
        }
        if (m.hMapping) {
            CloseHandle(m.hMapping);
            m.hMapping = nullptr;
        }
        if (m.hFile != INVALID_HANDLE_VALUE) {
            CloseHandle(m.hFile);
            m.hFile = INVALID_HANDLE_VALUE;
        }
    }
    
    // Prefetch pages into RAM (AVX-512 friendly access pattern)
    static void Prefetch(void* addr, size_t len, DWORD flags = 0) {
        // Windows 8+ PrefetchVirtualMemory
        static auto pPrefetch = (decltype(&PrefetchVirtualMemory))GetProcAddress(
            GetModuleHandleW(L"kernel32.dll"), "PrefetchVirtualMemory");
            
        if (pPrefetch) {
            WIN32_MEMORY_RANGE_ENTRY entry;
            entry.VirtualAddress = addr;
            entry.NumberOfBytes = len;
            pPrefetch(GetCurrentProcess(), 1, &entry, flags);
        } else {
            // Fallback: touch pages sequentially
            volatile char* p = static_cast<char*>(addr);
            for (size_t i = 0; i < len; i += 4096) {
                (void)(p[i]);
            }
        }
    }
};
