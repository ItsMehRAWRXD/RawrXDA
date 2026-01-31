// src/io/io_factory.cpp
#include "backend_interface.hpp"

#ifdef _WIN32
    #include "direct_io_ring_win.hpp"
#endif

// Linux support coming in v1.1.1
// #ifdef __linux__
//     #include "direct_io_ring_linux.hpp"
// #endif

IDirectIOBackend* CreateIOBackend(IOBackendType type) {
#ifdef _WIN32
    if (type == IOBackendType::IORING_WINDOWS) {
        return new DirectIORingWindows();
    }
#endif

    // Fallback or unsupported platform
    return nullptr;
}
