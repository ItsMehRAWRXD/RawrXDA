#include "braided_quantizer_fingerprint.hpp"
#include <cmath>
#include <vector>
#include <numeric>
#include <sstream>

double BraidedQuantizerFingerprint::calculate_mse(const std::vector<float>& original, const std::vector<float>& reconstructed) {
    if (original.size() != reconstructed.size()) {
        return -1.0; // Indicate error
    }
    double mse = 0.0;
    for (size_t i = 0; i < original.size(); ++i) {
        mse += std::pow(original[i] - reconstructed[i], 2);
    }
    return mse / original.size();
}

BraidedQuantizerFingerprintResult BraidedQuantizerFingerprint::run_test() {
    BraidedQuantizerFingerprintResult result;
    std::stringstream ss;

    // 1. Create some sample float data
    const uint64_t num_elements = 1024;
    std::vector<float> original_data(num_elements);
    for (uint64_t i = 0; i < num_elements; ++i) {
        original_data[i] = std::sin(static_cast<float>(i) / 10.0f) * 128.0f;
    }
    result.original_size_bytes = original_data.size() * sizeof(float);

    // 2. Quantize the data
    BraidedTensor braided_tensor = BraidedQuantizer::quantize("test_tensor", original_data.data(), num_elements);
    result.braided_size_bytes = braided_tensor.base_stream.size() +
                                braided_tensor.refinement_stream_1.size() +
                                braided_tensor.refinement_stream_2.size();
    result.compression_ratio = static_cast<double>(result.original_size_bytes) / result.braided_size_bytes;

    ss << "Original Size: " << result.original_size_bytes << " bytes\n";
    ss << "Braided Size: " << result.braided_size_bytes << " bytes\n";
    ss << "Compression Ratio: " << result.compression_ratio << ":1\n";

    // 3. Dequantize at each level and check correctness (MSE)
    std::vector<float> recon_l0 = BraidedQuantizer::dequantize(braided_tensor, BraidedQuantizationLevel::LEVEL_0_BASE);
    result.mse_level_0 = calculate_mse(original_data, recon_l0);
    ss << "MSE @ Level 0 (2-bit): " << result.mse_level_0 << "\n";

    std::vector<float> recon_l1 = BraidedQuantizer::dequantize(braided_tensor, BraidedQuantizationLevel::LEVEL_1_REFINED);
    result.mse_level_1 = calculate_mse(original_data, recon_l1);
    ss << "MSE @ Level 1 (4-bit): " << result.mse_level_1 << "\n";

    std::vector<float> recon_l2 = BraidedQuantizer::dequantize(braided_tensor, BraidedQuantizationLevel::LEVEL_2_DETAIL);
    result.mse_level_2 = calculate_mse(original_data, recon_l2);
    ss << "MSE @ Level 2 (8-bit): " << result.mse_level_2 << "\n";

    // 4. Validation
    bool passed = true;
    if (result.mse_level_0 < result.mse_level_1 || result.mse_level_1 < result.mse_level_2) {
        ss << "FAIL: MSE did not decrease with increasing precision.\n";
        passed = false;
    }
    if (result.compression_ratio < 3.0) {
        ss << "FAIL: Compression ratio is lower than expected.\n";
        passed = false;
    }

    result.passed = passed;
    if(passed) {
        ss << "SUCCESS: All fingerprint checks passed.\n";
    }
    result.message = ss.str();

    return result;
}
