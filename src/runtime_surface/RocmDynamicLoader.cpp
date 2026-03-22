#include "rawrxd/runtime/RocmDynamicLoader.hpp"

#include "../logging/Logger.h"

#include <string>

namespace RawrXD::Runtime {

RocmDynamicLoader& RocmDynamicLoader::instance() {
    static RocmDynamicLoader s;
    return s;
}

RuntimeResult RocmDynamicLoader::load() {
    if (m_dll != nullptr) {
        return {};
    }
    // ROCm on Windows ships user-mode HIP in various layouts; try common names.
    const wchar_t* candidates[] = {L"amdhip64.dll", L"hip.dll", L"hiprtc.dll"};
    for (const wchar_t* name : candidates) {
        m_dll = LoadLibraryW(name);
        if (m_dll != nullptr) {
            char narrow[128] = {};
            WideCharToMultiByte(CP_UTF8, 0, name, -1, narrow, static_cast<int>(sizeof(narrow) - 1), nullptr,
                                nullptr);
            RawrXD::Logging::Logger::instance().info(std::string("[RocmDynamicLoader] loaded ") + narrow,
                                                     "RuntimeSurface");
            return {};
        }
    }
    return std::unexpected("RocmDynamicLoader: no HIP DLL found (install ROCm runtime or ignore on non-AMD)");
}

void RocmDynamicLoader::unload() {
    if (m_dll != nullptr) {
        FreeLibrary(m_dll);
        m_dll = nullptr;
        RawrXD::Logging::Logger::instance().info("[RocmDynamicLoader] unloaded", "RuntimeSurface");
    }
}

void* RocmDynamicLoader::resolve(const char* name) const {
    if (m_dll == nullptr || name == nullptr) {
        return nullptr;
    }
    return reinterpret_cast<void*>(GetProcAddress(m_dll, name));
}

}  // namespace RawrXD::Runtime
