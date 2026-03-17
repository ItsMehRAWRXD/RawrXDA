/*
 * MeyersSingleton Bridge - C++ interface to MASM thread-safe singleton
 * 
 * This header provides extern "C" declarations to call the MASM MeyersSingleton
 * implementation. The MASM version uses Windows InitOnceExecuteOnce for
 * guaranteed thread-safe lazy initialization with minimal overhead.
 *
 * Usage:
 *   #include "meyers_singleton_bridge.hpp"
 *   
 *   // Get singleton (thread-safe, lazy-initialized)
 *   void* instance = MeyersGet();
 *   
 *   // Check if initialized (atomic read)
 *   if (MeyersIsInitialized()) { ... }
 *
 * NOTE: The default MASM implementation allocates 128 bytes on the process heap.
 * For OrchestraManager integration, we use the C++11 Meyers singleton which is
 * already thread-safe. This MASM implementation is provided for scenarios where:
 *   1. Extreme performance is required (avoids C++ runtime overhead)
 *   2. Integration with other MASM modules is needed
 *   3. Custom initialization logic is required
 *
 * Copyright (c) 2025 RawrXD Project
 */

#ifndef MEYERS_SINGLETON_BRIDGE_HPP
#define MEYERS_SINGLETON_BRIDGE_HPP

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Get the singleton instance (thread-safe, lazy-initialized)
 * 
 * On first call, allocates memory using HeapAlloc and initializes via
 * InitOnceExecuteOnce. Subsequent calls return the cached pointer.
 * 
 * @return Pointer to singleton instance (always valid after first call)
 * 
 * Thread-safety: Guaranteed via Windows InitOnceExecuteOnce API
 * Performance: Single branch on fast path after initialization
 */
void* __cdecl MeyersGet(void);

/**
 * @brief Check if singleton is initialized
 * 
 * @return 1 if initialized, 0 if not
 * 
 * Thread-safety: Yes (atomic read)
 */
int __cdecl MeyersIsInitialized(void);

/**
 * @brief Reset singleton (FOR TESTING ONLY)
 * 
 * WARNING: This is NOT thread-safe and should only be used in unit tests.
 * Does NOT free memory - that would require calling the destructor.
 */
void __cdecl MeyersReset(void);

#ifdef __cplusplus
}

// C++ convenience wrapper
namespace rawrxd {
namespace masm {

/**
 * @brief Type-safe wrapper for MASM Meyers singleton
 * 
 * @tparam T The singleton type
 * 
 * Usage:
 *   auto* mgr = MasmSingleton<OrchestraManager>::get();
 */
template<typename T>
class MasmSingleton {
public:
    static T* get() {
        return reinterpret_cast<T*>(MeyersGet());
    }
    
    static bool isInitialized() {
        return MeyersIsInitialized() != 0;
    }
    
    // Deleted to prevent misuse
    MasmSingleton() = delete;
    MasmSingleton(const MasmSingleton&) = delete;
    MasmSingleton& operator=(const MasmSingleton&) = delete;
};

} // namespace masm
} // namespace rawrxd

#endif // __cplusplus

#endif // MEYERS_SINGLETON_BRIDGE_HPP
