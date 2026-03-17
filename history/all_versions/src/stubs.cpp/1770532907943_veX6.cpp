#include "engine/inference_kernels.h"
#include <iostream>
#include <vector>
#include <string>

// Removed register_rawr_inference (defined in rawr_engine.cpp)
void register_sovereign_engines() {}

// Diagnostics::error — real implementation now in src/utils/Diagnostics.cpp
// Stub removed to avoid multiple definition linker errors (LNK2005 / ld)

// InferenceKernels: real SIMD implementations now in inference_kernels.cpp
// Stubs removed to avoid multiple definition linker errors