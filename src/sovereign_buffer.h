#pragma once
#include <windows.h>
#include <cstring>

namespace RawrXD {

// ============================================================================
// SovereignBuffer — Pure Win32 VirtualAlloc/HeapAlloc, no STL
// Replaces std::vector with direct kernel memory management
// ============================================================================

template<typename T>
class SovereignBuffer {
public:
    SovereignBuffer() : m_data(nullptr), m_capacity(0), m_size(0) {}
    
    ~SovereignBuffer() {
        deallocate();
    }
    
    // Allocate via VirtualAlloc (page-aligned, most efficient for large buffers)
    bool allocate(size_t count) {
        if (count == 0) return false;
        if (m_data) deallocate();
        
        size_t bytes = count * sizeof(T);
        m_data = static_cast<T*>(VirtualAlloc(nullptr, bytes, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
        
        if (!m_data) {
            return false;  // Out of memory
        }
        
        m_capacity = count;
        m_size = 0;
        return true;
    }
    
    // Deallocate
    void deallocate() {
        if (m_data) {
            VirtualFree(m_data, 0, MEM_RELEASE);
            m_data = nullptr;
            m_capacity = 0;
            m_size = 0;
        }
    }
    
    // Push element
    bool push_back(const T& val) {
        if (m_size >= m_capacity) {
            // Grow by doubling (or allocate if empty)
            size_t newCap = (m_capacity == 0) ? 16 : m_capacity * 2;
            if (!grow(newCap)) {
                return false;
            }
        }
        m_data[m_size++] = val;
        return true;
    }
    
    // Access
    T* data() { return m_data; }
    const T* data() const { return m_data; }
    
    T& operator[](size_t idx) { return m_data[idx]; }
    const T& operator[](size_t idx) const { return m_data[idx]; }
    
    T& back() { return m_data[m_size - 1]; }
    const T& back() const { return m_data[m_size - 1]; }
    
    // Queries
    size_t size() const { return m_size; }
    size_t capacity() const { return m_capacity; }
    bool empty() const { return m_size == 0; }
    
    // Clear (keeps allocation)
    void clear() { m_size = 0; }
    
    // Resize to exact size (zero-fills new elements if growing)
    bool resize(size_t newSize) {
        if (newSize > m_capacity) {
            if (!grow(newSize)) return false;
        }
        if (newSize > m_size) {
            // Zero-fill new elements
            ZeroMemory(m_data + m_size, (newSize - m_size) * sizeof(T));
        }
        m_size = newSize;
        return true;
    }
    
    // Iterator support
    T* begin() { return m_data; }
    T* end() { return m_data + m_size; }
    const T* begin() const { return m_data; }
    const T* end() const { return m_data + m_size; }
    
private:
    // Grow to new capacity
    bool grow(size_t newCap) {
        if (newCap <= m_capacity) return true;
        
        T* newData = static_cast<T*>(VirtualAlloc(nullptr, newCap * sizeof(T), MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
        if (!newData) {
            return false;
        }
        
        // Copy existing data
        if (m_data && m_size > 0) {
            CopyMemory(newData, m_data, m_size * sizeof(T));
            VirtualFree(m_data, 0, MEM_RELEASE);
        }
        
        m_data = newData;
        m_capacity = newCap;
        return true;
    }
    
    T* m_data;
    size_t m_capacity;
    size_t m_size;
};

// ============================================================================
// SovereignString — Fixed-buffer stack string, no heap allocation
// Replaces std::string for small strings (up to 256 chars)
// ============================================================================

class SovereignString {
public:
    static constexpr size_t MAX_LEN = 256;
    
    SovereignString() : m_len(0) {
        m_data[0] = '\0';
    }
    
    SovereignString(const char* str) : m_len(0) {
        if (str) {
            assign(str);
        }
    }
    
    void assign(const char* str) {
        if (!str) {
            m_len = 0;
            m_data[0] = '\0';
            return;
        }
        size_t len = 0;
        while (str[len] && len < MAX_LEN - 1) {
            m_data[len] = str[len];
            len++;
        }
        m_len = len;
        m_data[m_len] = '\0';
    }
    
    void append(const char* str) {
        if (!str) return;
        while (*str && m_len < MAX_LEN - 1) {
            m_data[m_len++] = *str++;
        }
        m_data[m_len] = '\0';
    }
    
    void append(char ch) {
        if (m_len < MAX_LEN - 1) {
            m_data[m_len++] = ch;
            m_data[m_len] = '\0';
        }
    }
    
    const char* c_str() const { return m_data; }
    char* data() { return m_data; }
    const char* data() const { return m_data; }
    
    size_t length() const { return m_len; }
    size_t size() const { return m_len; }
    bool empty() const { return m_len == 0; }
    
    void clear() {
        m_len = 0;
        m_data[0] = '\0';
    }
    
    char& operator[](size_t idx) { return m_data[idx]; }
    const char& operator[](size_t idx) const { return m_data[idx]; }
    
private:
    char m_data[MAX_LEN];
    size_t m_len;
};

}  // namespace RawrXD
