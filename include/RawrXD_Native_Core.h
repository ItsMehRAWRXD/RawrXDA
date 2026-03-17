#ifndef RAWRXD_NATIVE_CORE_H
#define RAWRXD_NATIVE_CORE_H

#include <windows.h>
#include <cstdint>

/**
 * @file RawrXD_Native_Core.h
 * @brief Self-Hosting Bootstrap: Native C++ entry point for the Titan JIT-emitted core.
 * 
 * Provides high-performance, zero-bloat alternatives to PowerShell .NET methods.
 * Architecture: C++20 Bridge -> MASM64/Titan Stage 9 Core.
 */

extern "C" {
    // ASM Functions from RawrXD_Native_Core.asm
    void RawrXD_Native_Log(const char* message);
    bool RawrXD_Native_WriteFile(const char* path, const char* content);
}

namespace rawrxd::native {

/**
 * @brief Zero-Touch Bootstrap Controller
 * Manages the transition from PowerShell loader to Native core.
 */
class NativeBootstrap {
public:
    static NativeBootstrap& instance() {
        static NativeBootstrap s_instance;
        return s_instance;
    }

    // Initialize the native core
    void initialize() {
        RawrXD_Native_Log("Bootstrap Initialized: Titan Stage 9 Core Online.");
    }

    // High-performance log bridge
    void log(const char* msg) {
        RawrXD_Native_Log(msg);
    }

    // Self-hosting metadata: Dump current state to disk
    bool dumpBootstrapState(const char* checkpointPath) {
        char buffer[256];
        wsprintfA(buffer, "Bootstrap Checkpoint: Timestamp=%llu, ThreadID=%u", 
                  GetTickCount64(), GetCurrentThreadId());
        return RawrXD_Native_WriteFile(checkpointPath, buffer);
    }

private:
    NativeBootstrap() = default;
    ~NativeBootstrap() = default;
};

} // namespace rawrxd::native

#endif // RAWRXD_NATIVE_CORE_H
