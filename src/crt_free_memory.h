#pragma once
#include <windows.h>
#include <stdio.h> // For sprintf_s

namespace RawrXD {

// ============================================================================
// CRTFreeMemory — Pure Win32 kernel allocations, zero CRT dependencies
// ============================================================================

class CRTFreeMemory {
public:
    static void* Allocate(size_t size) {
        if (size == 0) return nullptr;
        return HeapAlloc(GetProcessHeap(), 0, size);
    }
    
    static void* AllocateAligned(size_t size, size_t alignment) {
        if (size == 0 || alignment == 0) return nullptr;
        size_t allocSize = size + alignment + sizeof(void*);
        void* raw = HeapAlloc(GetProcessHeap(), 0, allocSize);
        if (!raw) return nullptr;
        
        uintptr_t addr = reinterpret_cast<uintptr_t>(raw);
        uintptr_t aligned = (addr + alignment + sizeof(void*)) & ~(alignment - 1);
        void** ptrLoc = reinterpret_cast<void**>(aligned - sizeof(void*));
        *ptrLoc = raw;  // Store original ptr for later free
        
        return reinterpret_cast<void*>(aligned);
    }
    
    static void* AllocateVirtual(size_t size) {
        if (size == 0) return nullptr;
        return VirtualAlloc(nullptr, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    }
    
    static void Deallocate(void* ptr) {
        if (ptr) {
            HeapFree(GetProcessHeap(), 0, ptr);
        }
    }
    
    static void DeallocateAligned(void* ptr) {
        if (ptr) {
            void** ptrLoc = reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(ptr) - sizeof(void*));
            HeapFree(GetProcessHeap(), 0, *ptrLoc);
        }
    }
    
    static void DeallocateVirtual(void* ptr) {
        if (ptr) {
            VirtualFree(ptr, 0, MEM_RELEASE);
        }
    }
    
    static size_t GetAllocationSize(void* ptr) {
        if (!ptr) return 0;
        return HeapSize(GetProcessHeap(), 0, ptr);
    }
};

// ============================================================================
// CRTFreeString — Pure stack-based string, no malloc
// ============================================================================

class CRTFreeString {
public:
    static constexpr size_t BUFFER_SIZE = 512;
    
    CRTFreeString() : m_len(0) {
        m_buffer[0] = '\0';
    }
    
    explicit CRTFreeString(const char* str) : m_len(0) {
        if (str) assign(str);
    }
    
    void assign(const char* str) {
        m_len = 0;
        if (str) {
            while (str[m_len] && m_len < BUFFER_SIZE - 1) {
                m_buffer[m_len] = str[m_len];
                m_len++;
            }
        }
        m_buffer[m_len] = '\0';
    }
    
    void append(const char* str) {
        if (str) {
            while (*str && m_len < BUFFER_SIZE - 1) {
                m_buffer[m_len++] = *str++;
            }
            m_buffer[m_len] = '\0';
        }
    }
    
    void format(const char* fmt, ...) {
        // Minimal format support (just %s, %d, %x for now)
        va_list args;
        va_start(args, fmt);
        
        m_len = 0;
        const char* p = fmt;
        
        while (*p && m_len < BUFFER_SIZE - 1) {
            if (*p == '%' && *(p + 1)) {
                p++;
                if (*p == 's') {
                    const char* str = va_arg(args, const char*);
                    while (*str && m_len < BUFFER_SIZE - 1) {
                        m_buffer[m_len++] = *str++;
                    }
                } else if (*p == 'd') {
                    int val = va_arg(args, int);
                    // Convert int to string (CRT-free)
                    if (val < 0) {
                        if (m_len < BUFFER_SIZE - 1) m_buffer[m_len++] = '-';
                        val = -val;
                    }
                    if (val == 0) {
                        if (m_len < BUFFER_SIZE - 1) m_buffer[m_len++] = '0';
                    } else {
                        char temp[16];
                        int idx = 0;
                        while (val > 0 && idx < 15) {
                            temp[idx++] = '0' + (val % 10);
                            val /= 10;
                        }
                        for (int i = idx - 1; i >= 0 && m_len < BUFFER_SIZE - 1; i--) {
                            m_buffer[m_len++] = temp[i];
                        }
                    }
                } else if (*p == 'u') {
                    unsigned int val = va_arg(args, unsigned int);
                    // Convert unsigned int to string (CRT-free)
                    if (val == 0) {
                        if (m_len < BUFFER_SIZE - 1) m_buffer[m_len++] = '0';
                    } else {
                        char temp[16];
                        int idx = 0;
                        while (val > 0 && idx < 15) {
                            temp[idx++] = '0' + (val % 10);
                            val /= 10;
                        }
                        for (int i = idx - 1; i >= 0 && m_len < BUFFER_SIZE - 1; i--) {
                            m_buffer[m_len++] = temp[i];
                        }
                    }
                } else if (*p == 'c') {
                    char val = va_arg(args, int); // char promoted to int
                    if (m_len < BUFFER_SIZE - 1) m_buffer[m_len++] = val;
                }
                p++;
            } else {
                m_buffer[m_len++] = *p++;
            }
        }
        
        m_buffer[m_len] = '\0';
        va_end(args);
    }
    
    const char* c_str() const { return m_buffer; }
    size_t length() const { return m_len; }
    size_t size() const { return m_len; }
    bool empty() const { return m_len == 0; }
    
    void clear() {
        m_len = 0;
        m_buffer[0] = '\0';
    }
    
private:
    char m_buffer[BUFFER_SIZE];
    size_t m_len;
};

// ============================================================================
// CRTFreeConsole — Pure Win32 console I/O, zero CRT dependencies
// ============================================================================

class CRTFreeConsole {
public:
    static void Write(const char* str) {
        if (!str) return;
        HANDLE hStdout = GetStdHandle(STD_OUTPUT_HANDLE);
        if (hStdout == INVALID_HANDLE_VALUE) return;
        
        // CRT-free strlen
        size_t len = 0;
        while (str[len]) len++;
        
        DWORD written;
        WriteFile(hStdout, str, (DWORD)len, &written, nullptr);
    }
    
    static void WriteError(const char* str) {
        if (!str) return;
        HANDLE hStderr = GetStdHandle(STD_ERROR_HANDLE);
        if (hStderr == INVALID_HANDLE_VALUE) return;
        
        // CRT-free strlen
        size_t len = 0;
        while (str[len]) len++;
        
        DWORD written;
        WriteFile(hStderr, str, (DWORD)len, &written, nullptr);
    }
    
    static void WriteLine(const char* str) {
        Write(str);
        Write("\n");
    }
    
    static void WriteFormat(const char* fmt, ...) {
        CRTFreeString s;
        va_list args;
        va_start(args, fmt);
        s.format(fmt, args);
        va_end(args);
        Write(s.c_str());
    }
    
    static CRTFreeString ReadLine() {
        CRTFreeString result;
        HANDLE hStdin = GetStdHandle(STD_INPUT_HANDLE);
        if (hStdin == INVALID_HANDLE_VALUE) return result;
        
        char buffer[256];
        DWORD read;
        if (ReadFile(hStdin, buffer, sizeof(buffer) - 1, &read, nullptr) && read > 0) {
            buffer[read] = '\0';
            // Remove trailing \r\n
            if (read >= 2 && buffer[read-2] == '\r' && buffer[read-1] == '\n') {
                buffer[read-2] = '\0';
                result.assign(buffer);
            } else if (read >= 1 && buffer[read-1] == '\n') {
                buffer[read-1] = '\0';
                result.assign(buffer);
            } else {
                result.assign(buffer);
            }
        }
        return result;
    }
};

}  // namespace RawrXD
