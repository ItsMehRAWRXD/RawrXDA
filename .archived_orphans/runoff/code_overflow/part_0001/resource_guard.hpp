// utils/resource_guard.hpp
// RAII resource management with telemetry integration
#pragma once
#include <utility>
#include "../telemetry/async_logger.hpp"

namespace RawrXD {

    // Generic RAII guard for any handle/resource with custom release function
    template<typename Handle, typename ReleaseFn>
    class ResourceGuard {
        Handle handle_;
        ReleaseFn release_;
        const char* resource_name_;
        bool released_;

    public:
        ResourceGuard(Handle h, ReleaseFn fn, const char* name)
            : handle_(h), release_(fn), resource_name_(name), released_(false) {
            RAWR_LOG(Telemetry::LogLevel::DEBUG, "Acquired resource '%s' @ %p",
                     name, reinterpret_cast<void*>(static_cast<uintptr_t>(h)));
        }

        ~ResourceGuard() {
            if (!released_ && handle_) {
                release_(handle_);
                RAWR_LOG(Telemetry::LogLevel::DEBUG, "Released resource '%s'", resource_name_);
            }
        }

        // Move-only
        ResourceGuard(const ResourceGuard&) = delete;
        ResourceGuard& operator=(const ResourceGuard&) = delete;

        ResourceGuard(ResourceGuard&& other) noexcept
            : handle_(other.handle_), release_(std::move(other.release_)),
              resource_name_(other.resource_name_), released_(other.released_) {
            other.released_ = true;
        }

        ResourceGuard& operator=(ResourceGuard&& other) noexcept {
            if (this != &other) {
                if (!released_ && handle_) release_(handle_);
                handle_ = other.handle_;
                release_ = std::move(other.release_);
                resource_name_ = other.resource_name_;
                released_ = other.released_;
                other.released_ = true;
            }
            return *this;
        }

        Handle get() const { return handle_; }
        explicit operator bool() const { return !released_ && handle_; }

        Handle release() {
            released_ = true;
            return handle_;
        }

        void reset(Handle newHandle = Handle{}) {
            if (!released_ && handle_) {
                release_(handle_);
                RAWR_LOG(Telemetry::LogLevel::DEBUG, "Released resource '%s' (reset)", resource_name_);
            }
            handle_ = newHandle;
            released_ = (newHandle == Handle{});
        }
    };

    // Convenience: Windows HANDLE guard
    inline auto MakeHandleGuard(HANDLE h, const char* name) {
        return ResourceGuard<HANDLE, decltype(&CloseHandle)>(h, &CloseHandle, name);
    }

    // Convenience: VirtualAlloc guard
    inline auto MakeVirtualAllocGuard(void* ptr, const char* name) {
        auto releaser = [](void* p) {
            VirtualFree(p, 0, MEM_RELEASE);
        };
        return ResourceGuard<void*, decltype(releaser)>(ptr, releaser, name);
    }

} // namespace RawrXD
