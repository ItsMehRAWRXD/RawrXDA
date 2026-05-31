#pragma once
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <stdexcept>

#if defined(_WIN32)
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>
#elif defined(__linux__)
#  include <sys/mman.h>
#endif

namespace RawrXD {

// Cross-platform huge-page allocator.
//
// Windows: tries VirtualAlloc with MEM_LARGE_PAGES; falls back to ordinary
//          VirtualAlloc if the privilege is not held.
// Linux:   tries mmap with MAP_HUGETLB; falls back to mmap without it, then
//          issues MADV_HUGEPAGE as a best-effort hint.
// Other:   falls back to aligned_alloc / malloc.
class HugePageAllocator {
public:
    struct Allocation {
        void*  ptr    = nullptr;
        size_t bytes  = 0;
        bool   huge   = false;   // whether huge pages were actually obtained
    };

    static Allocation allocate(size_t bytes) {
        if (bytes == 0) return {};

#if defined(_WIN32)
        return allocateWindows(bytes);
#elif defined(__linux__)
        return allocateLinux(bytes);
#else
        return allocateFallback(bytes);
#endif
    }

    static void deallocate(const Allocation& a) {
        if (!a.ptr) return;
#if defined(_WIN32)
        VirtualFree(a.ptr, 0, MEM_RELEASE);
#elif defined(__linux__)
        munmap(a.ptr, a.bytes);
#else
        std::free(a.ptr);
#endif
    }

private:
#if defined(_WIN32)
    static Allocation allocateWindows(size_t bytes) {
        // Round up to large-page size (usually 2 MiB)
        SIZE_T lp_size = GetLargePageMinimum();
        if (lp_size > 0) {
            size_t aligned = roundUp(bytes, (size_t)lp_size);
            void* ptr = VirtualAlloc(
                nullptr, aligned,
                MEM_RESERVE | MEM_COMMIT | MEM_LARGE_PAGES,
                PAGE_READWRITE
            );
            if (ptr) return {ptr, aligned, true};
        }

        // Fallback: normal commit
        void* ptr = VirtualAlloc(nullptr, bytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
        if (!ptr) throw std::bad_alloc();
        return {ptr, bytes, false};
    }
#endif

#if defined(__linux__)
    static Allocation allocateLinux(size_t bytes) {
        // Try MAP_HUGETLB (requires kernel support and sufficient huge pages)
        constexpr size_t HUGE_PAGE = 2ULL * 1024 * 1024;
        size_t aligned = roundUp(bytes, HUGE_PAGE);

        void* ptr = mmap(nullptr, aligned,
            PROT_READ | PROT_WRITE,
            MAP_PRIVATE | MAP_ANONYMOUS | MAP_HUGETLB, -1, 0);

        if (ptr != MAP_FAILED) {
            return {ptr, aligned, true};
        }

        // Fallback: normal mmap + MADV_HUGEPAGE
        ptr = mmap(nullptr, bytes,
            PROT_READ | PROT_WRITE,
            MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (ptr == MAP_FAILED) throw std::bad_alloc();
#  ifdef MADV_HUGEPAGE
        madvise(ptr, bytes, MADV_HUGEPAGE);
#  endif
        return {ptr, bytes, false};
    }
#endif

    static Allocation allocateFallback(size_t bytes) {
        void* ptr = nullptr;
#if defined(_MSC_VER)
        ptr = _aligned_malloc(bytes, 64);
#else
        ptr = std::aligned_alloc(64, roundUp(bytes, 64));
#endif
        if (!ptr) throw std::bad_alloc();
        return {ptr, bytes, false};
    }

    static size_t roundUp(size_t v, size_t align) {
        return (v + align - 1) & ~(align - 1);
    }
};

} // namespace RawrXD
