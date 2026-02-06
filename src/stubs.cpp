#include "engine/inference_kernels.h"
#include <iostream>
#include <vector>
#include <string>

// Removed register_rawr_inference (defined in rawr_engine.cpp)
void register_sovereign_engines() {}

namespace Diagnostics {
    void error(const std::string& title, const std::string& message) {
        std::cerr << "[ERROR] " << title << ": " << message << std::endl;
    }
}

// InferenceKernels: real SIMD implementations now in inference_kernels.cpp
// Stubs removed to avoid multiple definition linker errors