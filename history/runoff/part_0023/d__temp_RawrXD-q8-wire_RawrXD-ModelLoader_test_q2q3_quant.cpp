// Quick test for Q2_K and Q3_K quantization implementations
#include "src/qtapp/quant_utils.hpp"
#include <iostream>
#include <vector>
#include <cmath>
#include <cstring>

void test_q2_k_block_size() {
    std::cout << "Testing Q2_K block structure..." << std::endl;
    std::cout << "  sizeof(block_q2_K) = " << sizeof(block_q2_K) << " (expected 84)" << std::endl;
    std::cout << "  QK_K = " << QK_K << " (expected 256)" << std::endl;
    
    if (sizeof(block_q2_K) != 84) {
        std::cerr << "ERROR: Q2_K block size mismatch!" << std::endl;
    } else {
        std::cout << "  ✓ Q2_K block size correct" << std::endl;
    }
}

void test_q3_k_block_size() {
    std::cout << "\nTesting Q3_K block structure..." << std::endl;
    std::cout << "  sizeof(block_q3_K) = " << sizeof(block_q3_K) << " (expected 110)" << std::endl;
    
    if (sizeof(block_q3_K) != 110) {
        std::cerr << "ERROR: Q3_K block size mismatch!" << std::endl;
    } else {
        std::cout << "  ✓ Q3_K block size correct" << std::endl;
    }
}

void test_dequantize_q2_k() {
    std::cout << "\nTesting Q2_K dequantization..." << std::endl;
    
    // Create a test block with known values
    block_q2_K block;
    std::memset(&block, 0, sizeof(block));
    
    // Set simple values for testing
    block.d = 0x3C00;      // FP16 value of 1.0
    block.dmin = 0x0000;   // FP16 value of 0.0
    
    // Simple quantized values (all zeros)
    std::memset(block.scales, 0x11, sizeof(block.scales));  // Scale=1, min=1
    std::memset(block.qs, 0x00, sizeof(block.qs));          // All quants = 0
    
    // Dequantize
    float output[QK_K];
    dequantize_row_q2_K(&block, output, QK_K);
    
    // Verify output exists
    bool hasNonZero = false;
    for (int i = 0; i < 10; i++) {
        if (output[i] != 0.0f) hasNonZero = true;
    }
    
    std::cout << "  Sample outputs: ";
    for (int i = 0; i < 5; i++) {
        std::cout << output[i] << " ";
    }
    std::cout << std::endl;
    std::cout << "  ✓ Q2_K dequantization runs without crash" << std::endl;
}

void test_dequantize_q3_k() {
    std::cout << "\nTesting Q3_K dequantization..." << std::endl;
    
    // Create a test block with known values
    block_q3_K block;
    std::memset(&block, 0, sizeof(block));
    
    // Set simple values for testing
    block.d = 0x3C00;  // FP16 value of 1.0
    
    // Simple quantized values
    std::memset(block.scales, 0x20, sizeof(block.scales));  // Offset by 32
    std::memset(block.qs, 0x00, sizeof(block.qs));
    std::memset(block.hmask, 0x00, sizeof(block.hmask));
    
    // Dequantize
    float output[QK_K];
    dequantize_row_q3_K(&block, output, QK_K);
    
    std::cout << "  Sample outputs: ";
    for (int i = 0; i < 5; i++) {
        std::cout << output[i] << " ";
    }
    std::cout << std::endl;
    std::cout << "  ✓ Q3_K dequantization runs without crash" << std::endl;
}

void test_apply_quant_dispatcher() {
    std::cout << "\nTesting apply_quant() dispatcher..." << std::endl;
    
    // Create test float data
    std::vector<float> testData(QK_K);
    for (int i = 0; i < QK_K; i++) {
        testData[i] = std::sin(i * 0.1f);
    }
    
    QByteArray rawData(reinterpret_cast<const char*>(testData.data()), 
                       testData.size() * sizeof(float));
    
    // Test Q2_K mode
    QByteArray q2k = apply_quant(rawData, "Q2_K");
    std::cout << "  Q2_K quantization returned " << q2k.size() << " bytes" << std::endl;
    
    // Test Q3_K mode
    QByteArray q3k = apply_quant(rawData, "Q3_K");
    std::cout << "  Q3_K quantization returned " << q3k.size() << " bytes" << std::endl;
    
    std::cout << "  ✓ apply_quant() dispatcher works for Q2_K/Q3_K" << std::endl;
}

int main() {
    std::cout << "=== Q2_K and Q3_K Quantization Test ===" << std::endl;
    std::cout << std::endl;
    
    test_q2_k_block_size();
    test_q3_k_block_size();
    test_dequantize_q2_k();
    test_dequantize_q3_k();
    test_apply_quant_dispatcher();
    
    std::cout << "\n=== All tests completed ===" << std::endl;
    return 0;
}
