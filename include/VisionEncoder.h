#pragma once

#include <vector>
#include <string>
#include <cstdint>

namespace RawrXD {

struct VisionInput {
    std::vector<uint8_t> pixel_data;
    uint32_t width;
    uint32_t height;
    uint32_t channels;
};

struct VisionEmbedding {
    std::vector<float> vector;
};

class VisionEncoder {
public:
    VisionEncoder();
    ~VisionEncoder();

    bool load_model(const std::string& model_path);
    VisionEmbedding encode_image(const VisionInput& input);
    
    // Performance metrics for parity analysis
    float get_last_inference_ms() const { return last_ms; }

private:
    float last_ms = 0.0f;
    void* internal_state = nullptr;
};

} // namespace RawrXD
