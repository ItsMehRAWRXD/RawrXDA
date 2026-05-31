#pragma once

#include "BraidedQuantizer.hpp"
#include <string>
#include <vector>
#include <numeric>

struct BraidedQuantizerFingerprintResult {
    bool passed = false;
    std::string message;
    double mse_level_0 = 0.0; // Mean Squared Error for base level
    double mse_level_1 = 0.0; // MSE for refined level
    double mse_level_2 = 0.0; // MSE for detail level
    size_t original_size_bytes = 0;
    size_t braided_size_bytes = 0;
    double compression_ratio = 0.0;
};

class BraidedQuantizerFingerprint {
public:
    static BraidedQuantizerFingerprintResult run_test();

private:
    static double calculate_mse(const std::vector<float>& original, const std::vector<float>& reconstructed);
};
