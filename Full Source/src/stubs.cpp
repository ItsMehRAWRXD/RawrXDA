<<<<<<< HEAD:Full Source/src/stubs.cpp
#include "engine/inference_kernels.h"
#include <windows.h>
#include <debugapi.h>
#include <vector>
#include <string>

// Removed register_rawr_inference (defined in tool_registry_init.cpp)
// register_sovereign_engines — linker fallback for targets without engine module.
// Real implementation in engine/sovereign_engines.cpp registers Engine800B + SovereignSmall.
// Also defined in tool_registry_init.cpp for registry-aware targets.
// This definition is for minimal/test targets that don't link sovereign_engines or tool_registry_init.
#if !defined(RAWR_HAS_TOOL_REGISTRY_INIT) && !defined(RAWR_HAS_SOVEREIGN_ENGINES)
void register_sovereign_engines() {
    OutputDebugStringA("[FALLBACK] register_sovereign_engines — engine module not linked");
}
#endif

// Diagnostics::error — real implementation now in src/utils/Diagnostics.cpp
// Stub removed to avoid multiple definition linker errors (LNK2005 / ld)

// InferenceKernels: real SIMD implementations now in inference_kernels.cpp
=======
#include "engine/inference_kernels.h"
#include <windows.h>
#include <debugapi.h>
#include <iostream>
#include <vector>
#include <string>

// Removed register_rawr_inference (defined in tool_registry_init.cpp)
// register_sovereign_engines — linker fallback for targets without engine module.
// Real implementation in engine/sovereign_engines.cpp registers Engine800B + SovereignSmall.
// Also defined in tool_registry_init.cpp for registry-aware targets.
// This definition is for minimal/test targets that don't link sovereign_engines or tool_registry_init.
#if !defined(RAWR_HAS_TOOL_REGISTRY_INIT) && !defined(RAWR_HAS_SOVEREIGN_ENGINES)
void register_sovereign_engines() {
    OutputDebugStringA("[FALLBACK] register_sovereign_engines — engine module not linked");
}
#endif

// Diagnostics::error — real implementation now in src/utils/Diagnostics.cpp
// Stub removed to avoid multiple definition linker errors (LNK2005 / ld)

// InferenceKernels: real SIMD implementations now in inference_kernels.cpp
>>>>>>> origin/main:src/stubs.cpp
// Stubs removed to avoid multiple definition linker errors