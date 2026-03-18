Please cont// vector_detector.hpp
#pragma once
#include "uaf_detector.hpp"

template<typename T>
class InvalidationSafeVector {
public:
    using value_type = T;

    ~InvalidationSafeVector() {
        // Warn about any outstanding pointers
        for (auto& [ptr, info] : s_trackedPointers) {
            HardLog("LEAKED POINTER at destruction: %p (from %s:%d)",
                ptr, info.file, info.line);
        }
    }

    void push_back(const T& val) {
        void* oldData = m_vec.empty() ? nullptr : &m_vec[0];
        size_t oldCapacity = m_vec.capacity();

        m_vec.push_back(val);

        void* newData = m_vec.empty() ? nullptr : &m_vec[0];

        if (oldData && oldData != newData) {
            // REALLOCATION OCCURRED - Check for dangling pointers
            HardLog("VECTOR REALLOC: %p -> %p (capacity %zu -> %zu)",
                oldData, newData, oldCapacity, m_vec.capacity());

            CheckInvalidations(oldData, oldCapacity * sizeof(T));
        }
    }

    T& operator[](size_t i) {
        UAFDetector::ValidateAccess(&m_vec[i], "vector[]");
        return m_vec[i];
    }

    T& at(size_t i) {
        if (i >= m_vec.size()) {
            HardLog("OUT OF BOUNDS: index %zu, size %zu", i, m_vec.size());
            __debugbreak();
        }
        return (*this)[i];
    }

    // NEVER return raw pointers - return indices or wrappers
    size_t index_of(const T& ref) const {
        return &ref - &m_vec[0];
    }

    // Safe accessor that validates
    T* get_ptr(size_t index, const char* file, int line) {
        if (index >= m_vec.size()) return nullptr;
        T* ptr = &m_vec[index];
        TrackPointer(ptr, file, line);
        return ptr;
    }

    void invalidate_all() {
        HardLog("Invalidating all tracked pointers");
        s_trackedPointers.clear();
    }

private:
    std::vector<T> m_vec;

    struct PtrInfo {
        const char* file;
        int line;
        size_t index; // Which vector index it points to
    };

    static std::unordered_map<void*, PtrInfo> s_trackedPointers;

    void TrackPointer(void* ptr, const char* file, int line) {
        size_t idx = (T*)ptr - &m_vec[0];
        s_trackedPointers[ptr] = {file, line, idx};
    }

    void CheckInvalidations(void* oldBase, size_t oldSize) {
        for (auto it = s_trackedPointers.begin(); it != s_trackedPointers.end();) {
            void* ptr = it->first;

            // Check if pointer was in old range
            if (ptr >= oldBase && ptr < (char*)oldBase + oldSize) {
                HardLog("DANGLING POINTER INVALIDATED: %p", ptr);
                HardLog("  Originally from: %s:%d", it->second.file, it->second.line);
                HardLog("  Pointed to index: %zu", it->second.index);
                HardLog("  Old base: %p, new base: %p", oldBase, &m_vec[0]);

                __debugbreak();
                it = s_trackedPointers.erase(it);
            } else {
                ++it;
            }
        }
    }

    void HardLog(const char* fmt, ...) {
        // Same as UAFDetector::HardLog
        char buf[2048];
        va_list args;
        va_start(args, fmt);
        vsnprintf(buf, sizeof(buf), fmt, args);
        va_end(args);

        HANDLE hFile = CreateFileA("d:\\rawrxd\\vector_log.txt",
            FILE_APPEND_DATA, FILE_SHARE_READ, nullptr,
            OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);

        if (hFile != INVALID_HANDLE_VALUE) {
            DWORD written;
            WriteFile(hFile, buf, strlen(buf), &written, nullptr);
            WriteFile(hFile, "\r\n", 2, &written, nullptr);
            CloseHandle(hFile);
        }

        OutputDebugStringA(buf);
        OutputDebugStringA("\n");
    }
};

// Define static member
template<typename T>
std::unordered_map<void*, typename InvalidationSafeVector<T>::PtrInfo> InvalidationSafeVector<T>::s_trackedPointers;

// Macro for safe pointer acquisition
#define SAFE_VEC_PTR(vec, idx) (vec).get_ptr(idx, __FILE__, __LINE__)