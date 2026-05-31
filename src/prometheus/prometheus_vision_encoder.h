#pragma once
#include <cstdint>
#include <vector>
#include <string>

namespace Prometheus {

// =============================================================================
// VISION ENCODER — Minimal stub for compilation
// =============================================================================
struct VisionEncoder {
    uint32_t patchSize = 14;
    uint32_t hiddenDim = 1024;
    uint32_t numLayers = 24;

    std::vector<float> encode(const std::string& imagePathOrBase64, const std::string& mimeType = "") {
        (void)imagePathOrBase64;
        (void)mimeType;
        return {};
    }
};

} // namespace Prometheus
