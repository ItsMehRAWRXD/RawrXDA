#pragma once

#include "RuntimeTypes.hpp"

#include <expected>
#include <string>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace RawrXD::Runtime {

/// Dynamic ROCm/HIP user-mode (`amdhip64.dll` / `hip.dll`) — no static link requirement.
class RocmDynamicLoader {
public:
    static RocmDynamicLoader& instance();

    [[nodiscard]] RuntimeResult load();
    void unload();

    [[nodiscard]] bool loaded() const { return m_dll != nullptr; }

    /// Optional entry points — nullptr if missing.
    void* resolve(const char* name) const;

private:
    RocmDynamicLoader() = default;
    ~RocmDynamicLoader() { unload(); }

    RocmDynamicLoader(const RocmDynamicLoader&) = delete;
    RocmDynamicLoader& operator=(const RocmDynamicLoader&) = delete;

    HMODULE m_dll = nullptr;
};

}  // namespace RawrXD::Runtime
