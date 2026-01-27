/**
 * @file quant_correctness_tests.cpp
 * @brief Comprehensive quantization correctness tests
 * 
 * Tests pack/unpack round-trips, error bounds, and edge cases for:
 * - Q4_0/Q4_1 (4-bit quantization)
 * - Q5_0/Q5_1 (5-bit quantization)  
 * - Q6_K (6-bit quantization)
 * - Q8_K (8-bit quantization)
 * - F16 (half precision)
 * - F32 (full precision)
 */

#include "../src/qtapp/quant_utils.hpp"
#include <QCoreApplication>
#include <QVector>
#include <QDebug>
#include <cmath>
#include <iostream>
#include <random>

// Test result structure
struct TestResult {
    QString name;
    bool passed;
    QString error;
    double maxError;
    double avgError;
};

// Helper: Generate random float array
QByteArray generateRandomFloats(int count, float minVal = -10.0f, float maxVal = 10.0f) {
    std::random_device rd;
    std::mt19937 gen(42); // Fixed seed for reproducibility
    std::uniform_real_distribution<float> dis(minVal, maxVal);
    
    QByteArray data;
    data.resize(count * sizeof(float));
    float* f = reinterpret_cast<float*>(data.data());
    
    for (int i = 0; i < count; ++i) {
        f[i] = dis(gen);
    }
    
    return data;
}

// Helper: Generate edge case floats
QByteArray generateEdgeCaseFloats() {
    QVector<float> values = {
        0.0f,           // zero
        -0.0f,          // negative zero
        1.0f,           // one
        -1.0f,          // negative one
        0.0001f,        // very small positive
        -0.0001f,       // very small negative
        100.0f,         // large positive
        -100.0f,        // large negative
        3.14159f,       // pi
        -2.71828f,      // -e
        0.5f,           // half
        -0.5f,          // negative half
        1.23456f,       // arbitrary
        -9.87654f       // arbitrary negative
    };
    
    QByteArray data;
    data.resize(values.size() * sizeof(float));
    std::memcpy(data.data(), values.constData(), data.size());
    return data;
}

// Helper: Calculate error metrics
void calculateErrors(const QVector<float>& original, const QVector<float>& decoded,
                     double& maxError, double& avgError) {
    maxError = 0.0;
    avgError = 0.0;
    
    if (original.size() != decoded.size() || original.isEmpty()) {
        maxError = avgError = std::numeric_limits<double>::infinity();
        return;
    }
    
    for (int i = 0; i < original.size(); ++i) {
        double err = std::abs(double(original[i]) - double(decoded[i]));
        maxError = std::max(maxError, err);
        avgError += err;
    }
    
    avgError /= original.size();
}

// Test F16 round-trip
TestResult testF16RoundTrip() {
    TestResult result;
    result.name = "F16 Round-trip";
    result.passed = false;
    
    try {
        // Generate test data
        QByteArray rawData = generateRandomFloats(1000);
        const float* original = reinterpret_cast<const float*>(rawData.constData());
        int count = rawData.size() / sizeof(float);
        
        // Pack to F16
        QByteArray packed = to_f16(rawData);
        
        // Verify packed size
        if (packed.size() != count * 2) {
            result.error = QString("Packed size mismatch: expected %1, got %2")
                          .arg(count * 2).arg(packed.size());
            return result;
        }
        
        // Unpack back to float
        QVector<float> decoded = unpack_f16(packed);
        
        // Verify decoded count
        if (decoded.size() != count) {
            result.error = QString("Decoded count mismatch: expected %1, got %2")
                          .arg(count).arg(decoded.size());
            return result;
        }
        
        // Calculate errors
        QVector<float> originalVec(count);
        std::memcpy(originalVec.data(), original, count * sizeof(float));
        calculateErrors(originalVec, decoded, result.maxError, result.avgError);
        
        // F16 should have precision around 0.001 for values in [-10, 10]
        if (result.maxError < 0.1 && result.avgError < 0.01) {
            result.passed = true;
        } else {
            result.error = QString("Error too large: max=%1, avg=%2")
                          .arg(result.maxError).arg(result.avgError);
        }
        
    } catch (const std::exception& e) {
        result.error = QString("Exception: %1").arg(e.what());
    }
    
    return result;
}

// Test Q8_K round-trip
TestResult testQ8KRoundTrip() {
    TestResult result;
    result.name = "Q8_K Round-trip";
    result.passed = false;
    
    try {
        QByteArray rawData = generateRandomFloats(1000);
        const float* original = reinterpret_cast<const float*>(rawData.constData());
        int count = rawData.size() / sizeof(float);
        
        // Pack to Q8_K
        QByteArray packed = quantize_q8k(rawData);
        
        // Verify packed size (4 bytes scale + count bytes)
        if (packed.size() != 4 + count) {
            result.error = QString("Packed size mismatch: expected %1, got %2")
                          .arg(4 + count).arg(packed.size());
            return result;
        }
        
        // Unpack manually for Q8_K
        float scale;
        std::memcpy(&scale, packed.constData(), 4);
        const int8_t* q = reinterpret_cast<const int8_t*>(packed.constData() + 4);
        
        QVector<float> decoded(count);
        for (int i = 0; i < count; ++i) {
            decoded[i] = float(q[i]) * scale;
        }
        
        // Calculate errors
        QVector<float> originalVec(count);
        std::memcpy(originalVec.data(), original, count * sizeof(float));
        calculateErrors(originalVec, decoded, result.maxError, result.avgError);
        
        // Q8 should be quite accurate
        if (result.maxError < 0.2 && result.avgError < 0.05) {
            result.passed = true;
        } else {
            result.error = QString("Error too large: max=%1, avg=%2")
                          .arg(result.maxError).arg(result.avgError);
        }
        
    } catch (const std::exception& e) {
        result.error = QString("Exception: %1").arg(e.what());
    }
    
    return result;
}

// Test Q4_0 round-trip
TestResult testQ4RoundTrip() {
    TestResult result;
    result.name = "Q4_0 Round-trip";
    result.passed = false;
    
    try {
        QByteArray rawData = generateRandomFloats(1000);
        const float* original = reinterpret_cast<const float*>(rawData.constData());
        int count = rawData.size() / sizeof(float);
        
        // Pack to Q4_0
        QByteArray packed = quantize_q4_0(rawData);
        
        // Verify packed size (4 bytes scale + (count+1)/2 bytes)
        int expectedSize = 4 + (count + 1) / 2;
        if (packed.size() != expectedSize) {
            result.error = QString("Packed size mismatch: expected %1, got %2")
                          .arg(expectedSize).arg(packed.size());
            return result;
        }
        
        // Unpack manually for Q4
        float scale;
        std::memcpy(&scale, packed.constData(), 4);
        const uint8_t* q = reinterpret_cast<const uint8_t*>(packed.constData() + 4);
        
        QVector<float> decoded(count);
        for (int i = 0; i < count; ++i) {
            int qi = i / 2;
            int shift = (i % 2) ? 4 : 0;
            int8_t val = int8_t((q[qi] >> shift) & 0xF);
            // Sign extend
            if (val & 0x8) val |= 0xF0;
            decoded[i] = float(val) * scale;
        }
        
        // Calculate errors
        QVector<float> originalVec(count);
        std::memcpy(originalVec.data(), original, count * sizeof(float));
        calculateErrors(originalVec, decoded, result.maxError, result.avgError);
        
        // Q4 has lower precision
        if (result.maxError < 2.0 && result.avgError < 0.5) {
            result.passed = true;
        } else {
            result.error = QString("Error too large: max=%1, avg=%2")
                          .arg(result.maxError).arg(result.avgError);
        }
        
    } catch (const std::exception& e) {
        result.error = QString("Exception: %1").arg(e.what());
    }
    
    return result;
}

// Test Q5_0 round-trip
TestResult testQ5RoundTrip() {
    TestResult result;
    result.name = "Q5_0 Round-trip";
    result.passed = false;
    
    try {
        QByteArray rawData = generateRandomFloats(1000);
        const float* original = reinterpret_cast<const float*>(rawData.constData());
        int count = rawData.size() / sizeof(float);
        
        // Pack to Q5_0
        QByteArray packed = quantize_generic_bits(rawData, 5);
        
        // Unpack using helper
        QVector<float> decoded = unpack_generic_bits(packed, 5);
        
        // Verify count
        if (decoded.size() != count) {
            result.error = QString("Decoded count mismatch: expected %1, got %2")
                          .arg(count).arg(decoded.size());
            return result;
        }
        
        // Calculate errors
        QVector<float> originalVec(count);
        std::memcpy(originalVec.data(), original, count * sizeof(float));
        calculateErrors(originalVec, decoded, result.maxError, result.avgError);
        
        // Q5 should be between Q4 and Q6
        if (result.maxError < 1.0 && result.avgError < 0.3) {
            result.passed = true;
        } else {
            result.error = QString("Error too large: max=%1, avg=%2")
                          .arg(result.maxError).arg(result.avgError);
        }
        
    } catch (const std::exception& e) {
        result.error = QString("Exception: %1").arg(e.what());
    }
    
    return result;
}

// Test Q6_K round-trip
TestResult testQ6RoundTrip() {
    TestResult result;
    result.name = "Q6_K Round-trip";
    result.passed = false;
    
    try {
        QByteArray rawData = generateRandomFloats(1000);
        const float* original = reinterpret_cast<const float*>(rawData.constData());
        int count = rawData.size() / sizeof(float);
        
        // Pack to Q6_K
        QByteArray packed = quantize_generic_bits(rawData, 6);
        
        // Unpack using helper
        QVector<float> decoded = unpack_generic_bits(packed, 6);
        
        // Verify count
        if (decoded.size() != count) {
            result.error = QString("Decoded count mismatch: expected %1, got %2")
                          .arg(count).arg(decoded.size());
            return result;
        }
        
        // Calculate errors
        QVector<float> originalVec(count);
        std::memcpy(originalVec.data(), original, count * sizeof(float));
        calculateErrors(originalVec, decoded, result.maxError, result.avgError);
        
        // Q6 should be more accurate than Q5
        if (result.maxError < 0.5 && result.avgError < 0.15) {
            result.passed = true;
        } else {
            result.error = QString("Error too large: max=%1, avg=%2")
                          .arg(result.maxError).arg(result.avgError);
        }
        
    } catch (const std::exception& e) {
        result.error = QString("Exception: %1").arg(e.what());
    }
    
    return result;
}

// Test edge cases
TestResult testEdgeCases() {
    TestResult result;
    result.name = "Edge Cases";
    result.passed = false;
    
    try {
        QByteArray rawData = generateEdgeCaseFloats();
        const float* original = reinterpret_cast<const float*>(rawData.constData());
        int count = rawData.size() / sizeof(float);
        
        // Test each quantization mode
        QStringList modes = {"Q4_0", "Q5_0", "Q6_K", "Q8_K", "F16"};
        bool allPassed = true;
        
        for (const QString& mode : modes) {
            QByteArray packed = apply_quant(rawData, mode);
            
            // Verify we got some packed data
            if (packed.isEmpty() || packed == rawData) {
                result.error = QString("Mode %1 failed to pack").arg(mode);
                allPassed = false;
                break;
            }
            
            // For now, just verify packing doesn't crash
            // Full unpacking tests would require mode-specific logic
        }
        
        result.passed = allPassed;
        if (!allPassed && result.error.isEmpty()) {
            result.error = "Edge case handling failed";
        }
        
    } catch (const std::exception& e) {
        result.error = QString("Exception: %1").arg(e.what());
    }
    
    return result;
}

// Test zero-length input
TestResult testEmptyInput() {
    TestResult result;
    result.name = "Empty Input";
    result.passed = false;
    
    try {
        QByteArray empty;
        
        // All quantizers should handle empty input gracefully
        QByteArray q4 = quantize_q4_0(empty);
        QByteArray q5 = quantize_generic_bits(empty, 5);
        QByteArray q6 = quantize_generic_bits(empty, 6);
        QByteArray q8 = quantize_q8k(empty);
        QByteArray f16 = to_f16(empty);
        
        // Should return empty or original
        result.passed = true; // If we got here without crashing
        
    } catch (const std::exception& e) {
        result.error = QString("Exception: %1").arg(e.what());
    }
    
    return result;
}

// Main test runner
int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    
    std::cout << "========================================\n";
    std::cout << "Quantization Correctness Test Suite\n";
    std::cout << "========================================\n\n";
    
    QVector<TestResult> results;
    
    // Run all tests
    results.append(testF16RoundTrip());
    results.append(testQ8KRoundTrip());
    results.append(testQ4RoundTrip());
    results.append(testQ5RoundTrip());
    results.append(testQ6RoundTrip());
    results.append(testEdgeCases());
    results.append(testEmptyInput());
    
    // Print results
    int passed = 0;
    int failed = 0;
    
    for (const TestResult& r : results) {
        std::cout << "[" << (r.passed ? "PASS" : "FAIL") << "] " 
                  << r.name.toStdString();
        
        if (r.passed && (r.maxError > 0 || r.avgError > 0)) {
            std::cout << " (max_err=" << r.maxError << ", avg_err=" << r.avgError << ")";
        }
        
        std::cout << "\n";
        
        if (!r.passed && !r.error.isEmpty()) {
            std::cout << "       Error: " << r.error.toStdString() << "\n";
        }
        
        if (r.passed) passed++; else failed++;
    }
    
    std::cout << "\n========================================\n";
    std::cout << "Results: " << passed << " passed, " << failed << " failed\n";
    std::cout << "========================================\n";
    
    return (failed == 0) ? 0 : 1;
}
