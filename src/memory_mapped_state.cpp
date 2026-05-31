/**
 * MemoryMappedState Implementation
 * Enhancement #3: Zero-Copy State Access
 */

#include "memory_mapped_state.h"
#include <unordered_map>
#include <mutex>

#ifdef _WIN32
#include <windows.h>
#include <io.h>
#else
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#endif

namespace MemoryMappedState {

    // ===== MappedView Implementation =====

    MappedView::MappedView() = default;

    MappedView::~MappedView() {
        unmap();
    }

    MappedView::MappedView(MappedView&& other) noexcept
        : m_view(other.m_view)
        , m_size(other.m_size)
        , m_mode(other.m_mode)
#ifdef _WIN32
        , m_fileHandle(other.m_fileHandle)
        , m_mapHandle(other.m_mapHandle)
#endif
    {
        other.m_view = nullptr;
        other.m_size = 0;
#ifdef _WIN32
        other.m_fileHandle = INVALID_HANDLE_VALUE;
        other.m_mapHandle = nullptr;
#endif
    }

    MappedView& MappedView::operator=(MappedView&& other) noexcept {
        if (this != &other) {
            unmap();
            m_view = other.m_view;
            m_size = other.m_size;
            m_mode = other.m_mode;
#ifdef _WIN32
            m_fileHandle = other.m_fileHandle;
            m_mapHandle = other.m_mapHandle;
            other.m_fileHandle = INVALID_HANDLE_VALUE;
            other.m_mapHandle = nullptr;
#endif
            other.m_view = nullptr;
            other.m_size = 0;
        }
        return *this;
    }

    bool MappedView::map(const std::string& filePath, uint32_t mode) {
        return map(filePath, 0, 0, mode);
    }

    bool MappedView::map(const std::string& filePath, size_t offset, size_t size, uint32_t mode) {
        unmap();
        
#ifdef _WIN32
        DWORD access = (mode & MM_WRITE) ? GENERIC_READ | GENERIC_WRITE : GENERIC_READ;
        DWORD share = FILE_SHARE_READ;
        DWORD create = OPEN_EXISTING;
        DWORD flags = FILE_ATTRIBUTE_NORMAL;
        
        if (mode & MM_SEQUENTIAL) flags |= FILE_FLAG_SEQUENTIAL_SCAN;
        if (mode & MM_RANDOM) flags |= FILE_FLAG_RANDOM_ACCESS;
        
        m_fileHandle = CreateFileA(filePath.c_str(), access, share, nullptr, create, flags, nullptr);
        if (m_fileHandle == INVALID_HANDLE_VALUE) {
            return false;
        }
        
        LARGE_INTEGER fileSize;
        if (!GetFileSizeEx(m_fileHandle, &fileSize)) {
            CloseHandle(m_fileHandle);
            m_fileHandle = INVALID_HANDLE_VALUE;
            return false;
        }
        
        m_size = size > 0 ? size : static_cast<size_t>(fileSize.QuadPart);
        
        DWORD protect = (mode & MM_WRITE) ? PAGE_READWRITE : PAGE_READONLY;
        m_mapHandle = CreateFileMapping(m_fileHandle, nullptr, protect, 0, 0, nullptr);
        if (!m_mapHandle) {
            CloseHandle(m_fileHandle);
            m_fileHandle = INVALID_HANDLE_VALUE;
            return false;
        }
        
        DWORD mapAccess = (mode & MM_WRITE) ? FILE_MAP_WRITE : FILE_MAP_READ;
        if (mode & MM_COPY_ON_WRITE) mapAccess = FILE_MAP_COPY;
        
        m_view = MapViewOfFile(m_mapHandle, mapAccess, 
            static_cast<DWORD>(offset >> 32), 
            static_cast<DWORD>(offset & 0xFFFFFFFF),
            m_size);
        
        if (!m_view) {
            CloseHandle(m_mapHandle);
            CloseHandle(m_fileHandle);
            m_mapHandle = nullptr;
            m_fileHandle = INVALID_HANDLE_VALUE;
            return false;
        }
        
        m_mode = mode;
        return true;
#else
        int flags = (mode & MM_WRITE) ? O_RDWR : O_RDONLY;
        m_fd = open(filePath.c_str(), flags);
        if (m_fd < 0) {
            return false;
        }
        
        struct stat st;
        if (fstat(m_fd, &st) < 0) {
            close(m_fd);
            m_fd = -1;
            return false;
        }
        
        m_size = size > 0 ? size : static_cast<size_t>(st.st_size);
        
        int prot = PROT_READ;
        if (mode & MM_WRITE) prot |= PROT_WRITE;
        
        int mmapFlags = (mode & MM_COPY_ON_WRITE) ? MAP_PRIVATE : MAP_SHARED;
        
        m_view = mmap(nullptr, m_size, prot, mmapFlags, m_fd, offset);
        if (m_view == MAP_FAILED) {
            close(m_fd);
            m_fd = -1;
            m_view = nullptr;
            return false;
        }
        
        // Apply access hints
        if (mode & MM_SEQUENTIAL) {
            madvise(m_view, m_size, MADV_SEQUENTIAL);
        } else if (mode & MM_RANDOM) {
            madvise(m_view, m_size, MADV_RANDOM);
        }
        
        m_mode = mode;
        return true;
#endif
    }

    void MappedView::unmap() {
        if (!m_view) return;
        
#ifdef _WIN32
        UnmapViewOfFile(m_view);
        if (m_mapHandle) {
            CloseHandle(m_mapHandle);
            m_mapHandle = nullptr;
        }
        if (m_fileHandle != INVALID_HANDLE_VALUE) {
            CloseHandle(m_fileHandle);
            m_fileHandle = INVALID_HANDLE_VALUE;
        }
#else
        munmap(m_view, m_size);
        if (m_fd >= 0) {
            close(m_fd);
            m_fd = -1;
        }
#endif
        
        m_view = nullptr;
        m_size = 0;
    }

    const void* MappedView::data() const {
        return m_view;
    }

    void* MappedView::mutableData() {
        return (m_mode & MM_WRITE) ? m_view : nullptr;
    }

    size_t MappedView::size() const {
        return m_size;
    }

    bool MappedView::isMapped() const {
        return m_view != nullptr;
    }

    bool MappedView::flush() {
        if (!m_view || !(m_mode & MM_WRITE)) {
            return false;
        }
        
#ifdef _WIN32
        return FlushViewOfFile(m_view, m_size) != 0;
#else
        return msync(m_view, m_size, MS_SYNC) == 0;
#endif
    }

    void MappedView::prefetch(size_t offset, size_t size) {
        if (!m_view) return;
        
#ifdef _WIN32
        // Win32 prefetch virtual memory
        WIN32_MEMORY_RANGE_ENTRY entry;
        entry.VirtualAddress = static_cast<PVOID>(
            static_cast<char*>(m_view) + offset);
        entry.NumberOfBytes = size;
        PrefetchVirtualMemory(GetCurrentProcess(), 1, &entry, 0);
#else
        madvise(static_cast<char*>(m_view) + offset, size, MADV_WILLNEED);
#endif
    }

    void MappedView::adviseSequential() {
#ifndef _WIN32
        if (m_view) {
            madvise(m_view, m_size, MADV_SEQUENTIAL);
        }
#endif
    }

    void MappedView::adviseRandom() {
#ifndef _WIN32
        if (m_view) {
            madvise(m_view, m_size, MADV_RANDOM);
        }
#endif
    }

    // ===== System Capabilities =====

    bool isSupported() {
        return true; // Both Windows and POSIX support mmap
    }

    size_t getPageSize() {
#ifdef _WIN32
        SYSTEM_INFO si;
        GetSystemInfo(&si);
        return si.dwPageSize;
#else
        return sysconf(_SC_PAGESIZE);
#endif
    }

    size_t getMaxMappingSize() {
#ifdef _WIN32
        return static_cast<size_t>(1ULL << 40); // 1TB on Win64
#else
        return static_cast<size_t>(1ULL <> 47); // 128TB on Linux x64
#endif
    }

} // namespace MemoryMappedState
