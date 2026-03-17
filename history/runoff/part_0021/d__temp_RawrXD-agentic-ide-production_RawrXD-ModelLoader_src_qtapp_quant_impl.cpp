/**
 * @file quant_impl.cpp
 * @brief Implementation of quantization functions for inference engine
 */

#include <QByteArray>
#include <QString>
#include <vector>
#include <cmath>
#include <utility>
#include <cstring>

/**
 * @brief Apply quantization to tensor data based on type
 * @param data Raw tensor data
 * @param quantType Quantization type (int8, int4, float16, etc.)
 * @return Pair of quantized data and number of elements
 */
std::pair<QByteArray, int> apply_quant_with_type(const QByteArray& data, const QString& quantType)
{
    if (data.isEmpty()) {
        return {QByteArray(), 0};
    }
    
    // Interpret data as float32 array
    const float* floatData = reinterpret_cast<const float*>(data.constData());
    int elementCount = data.size() / sizeof(float);
    
    if (quantType == "int8") {
        // Quantize float32 to int8
        // Find min/max for scaling
        float minVal = floatData[0];
        float maxVal = floatData[0];
        
        for (int i = 1; i < elementCount; ++i) {
            if (floatData[i] < minVal) minVal = floatData[i];
            if (floatData[i] > maxVal) maxVal = floatData[i];
        }
        
        // Avoid division by zero
        if (std::abs(maxVal - minVal) < 1e-6) {
            maxVal = minVal + 1.0f;
        }
        
        // Scale factor for int8 (-128 to 127)
        float scale = 255.0f / (maxVal - minVal);
        
        QByteArray result;
        result.reserve(elementCount);
        
        for (int i = 0; i < elementCount; ++i) {
            int8_t quantized = static_cast<int8_t>(std::round((floatData[i] - minVal) * scale - 128.0f));
            result.append(reinterpret_cast<const char*>(&quantized), sizeof(int8_t));
        }
        
        return {result, elementCount};
    }
    else if (quantType == "int4") {
        // Quantize float32 to int4 (2 values per byte)
        float minVal = floatData[0];
        float maxVal = floatData[0];
        
        for (int i = 1; i < elementCount; ++i) {
            if (floatData[i] < minVal) minVal = floatData[i];
            if (floatData[i] > maxVal) maxVal = floatData[i];
        }
        
        if (std::abs(maxVal - minVal) < 1e-6) {
            maxVal = minVal + 1.0f;
        }
        
        float scale = 15.0f / (maxVal - minVal);  // 4-bit range: 0-15
        
        QByteArray result;
        result.reserve(elementCount / 2 + 1);
        
        for (int i = 0; i < elementCount; i += 2) {
            uint8_t high = static_cast<uint8_t>(std::round((floatData[i] - minVal) * scale));
            uint8_t low = (i + 1 < elementCount) 
                ? static_cast<uint8_t>(std::round((floatData[i + 1] - minVal) * scale))
                : 0;
            
            uint8_t packed = (high << 4) | (low & 0x0F);
            result.append(reinterpret_cast<const char*>(&packed), sizeof(uint8_t));
        }
        
        return {result, elementCount};
    }
    else if (quantType == "float16") {
        // Quantize float32 to float16
        QByteArray result;
        result.reserve(elementCount * 2);
        
        for (int i = 0; i < elementCount; ++i) {
            // Simple float32 to float16 conversion
            float f = floatData[i];
            uint32_t bits = *reinterpret_cast<uint32_t*>(&f);
            
            uint16_t half = 0;
            uint16_t sign = (bits >> 31) & 0x1;
            uint16_t exponent = (bits >> 23) & 0xFF;
            uint16_t mantissa = (bits >> 13) & 0x3FF;
            
            if (exponent == 0xFF) {
                // Infinity or NaN
                half = (sign << 15) | 0x7C00;
            } else if (exponent > 142) {
                // Overflow to infinity
                half = (sign << 15) | 0x7C00;
            } else if (exponent < 113) {
                // Underflow to zero
                half = (sign << 15);
            } else {
                // Normal number
                half = (sign << 15) | ((exponent - 112) << 10) | mantissa;
            }
            
            result.append(reinterpret_cast<const char*>(&half), sizeof(uint16_t));
        }
        
        return {result, elementCount};
    }
    else if (quantType == "none" || quantType == "float32") {
        // No quantization
        return {data, elementCount};
    }
    else {
        // Unknown type, return original
        return {data, elementCount};
    }
}
