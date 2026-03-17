// utils/resource_guard.hpp
#pragma once
#include <windows.h>
#include <memory>

namespace RawrXD {

// RAII wrapper for Windows handles
class HandleGuard {
public:
    HandleGuard(HANDLE handle = INVALID_HANDLE_VALUE) : handle_(handle) {}
    ~HandleGuard() { if (handle_ != INVALID_HANDLE_VALUE) CloseHandle(handle_); }
    
    HandleGuard(const HandleGuard&) = delete;
    HandleGuard& operator=(const HandleGuard&) = delete;
    
    HandleGuard(HandleGuard&& other) noexcept : handle_(other.handle_) {
        other.handle_ = INVALID_HANDLE_VALUE;
    }
    
    HandleGuard& operator=(HandleGuard&& other) noexcept {
        if (this != &other) {
            if (handle_ != INVALID_HANDLE_VALUE) CloseHandle(handle_);
            handle_ = other.handle_;
            other.handle_ = INVALID_HANDLE_VALUE;
        }
        return *this;
    }
    
    HANDLE get() const { return handle_; }
    HANDLE* operator&() { return &handle_; }
    
    explicit operator bool() const { return handle_ != INVALID_HANDLE_VALUE; }
    
private:
    HANDLE handle_;
};

// RAII wrapper for heap memory
class HeapGuard {
public:
    HeapGuard(size_t size) : ptr_(HeapAlloc(GetProcessHeap(), 0, size)) {}
    ~HeapGuard() { if (ptr_) HeapFree(GetProcessHeap(), 0, ptr_); }
    
    HeapGuard(const HeapGuard&) = delete;
    HeapGuard& operator=(const HeapGuard&) = delete;
    
    HeapGuard(HeapGuard&& other) noexcept : ptr_(other.ptr_) {
        other.ptr_ = nullptr;
    }
    
    HeapGuard& operator=(HeapGuard&& other) noexcept {
        if (this != &other) {
            if (ptr_) HeapFree(GetProcessHeap(), 0, ptr_);
            ptr_ = other.ptr_;
            other.ptr_ = nullptr;
        }
        return *this;
    }
    
    void* get() const { return ptr_; }
    explicit operator bool() const { return ptr_ != nullptr; }
    
private:
    void* ptr_;
};

} // namespace RawrXD