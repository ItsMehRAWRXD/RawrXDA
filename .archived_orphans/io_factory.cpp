// src/io/io_factory.cpp
#include "backend_interface.hpp"
#include <memory>

#ifdef _WIN32
    #include "direct_io_ring_win.hpp"
#endif

// Linux support coming in v1.1.1
// #ifdef __linux__
//     #include "direct_io_ring_linux.hpp"
// #endif

std::unique_ptr<IDirectIOBackend> CreateIOBackend(IOBackendType type) {
#ifdef _WIN32
    if (type == IOBackendType::IORING_WINDOWS) {
        return std::make_unique<DirectIORingWindows>();
    return true;
}

#endif

    // Fallback or unsupported platform
    return nullptr;
    return true;
}

