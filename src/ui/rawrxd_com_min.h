#pragma once

#include <unknwn.h>
#include <utility>

namespace rawrxd::com {

template <typename T>
class ComPtr {
public:
    ComPtr() noexcept = default;
    ComPtr(std::nullptr_t) noexcept {}

    explicit ComPtr(T* ptr) noexcept : ptr_(ptr) {
        internalAddRef();
    }

    ComPtr(const ComPtr& other) noexcept : ptr_(other.ptr_) {
        internalAddRef();
    }

    ComPtr(ComPtr&& other) noexcept : ptr_(other.ptr_) {
        other.ptr_ = nullptr;
    }

    ~ComPtr() {
        internalRelease();
    }

    ComPtr& operator=(const ComPtr& other) noexcept {
        if (this != &other) {
            assign(other.ptr_);
        }
        return *this;
    }

    ComPtr& operator=(ComPtr&& other) noexcept {
        if (this != &other) {
            internalRelease();
            ptr_ = other.ptr_;
            other.ptr_ = nullptr;
        }
        return *this;
    }

    ComPtr& operator=(T* ptr) noexcept {
        assign(ptr);
        return *this;
    }

    void reset() noexcept {
        internalRelease();
    }

    T* get() const noexcept {
        return ptr_;
    }

    T* Get() const noexcept {
        return ptr_;
    }

    T** put() noexcept {
        internalRelease();
        return &ptr_;
    }

    T** Put() noexcept {
        internalRelease();
        return &ptr_;
    }

    T** release_and_get_address_of() noexcept {
        internalRelease();
        return &ptr_;
    }

    T* detach() noexcept {
        T* out = ptr_;
        ptr_ = nullptr;
        return out;
    }

    void attach(T* ptr) noexcept {
        if (ptr_ != ptr) {
            internalRelease();
            ptr_ = ptr;
        }
    }

    T& operator*() const noexcept {
        return *ptr_;
    }

    T* operator->() const noexcept {
        return ptr_;
    }

    explicit operator bool() const noexcept {
        return ptr_ != nullptr;
    }

private:
    void assign(T* ptr) noexcept {
        if (ptr_ != ptr) {
            internalRelease();
            ptr_ = ptr;
            internalAddRef();
        }
    }

    void internalAddRef() noexcept {
        if (ptr_) {
            ptr_->AddRef();
        }
    }

    void internalRelease() noexcept {
        T* local = ptr_;
        if (local) {
            ptr_ = nullptr;
            local->Release();
        }
    }

    T* ptr_ = nullptr;
};

} // namespace rawrxd::com

