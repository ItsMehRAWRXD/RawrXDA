#pragma once

/**
 * @file image_generator.h
 * @brief Zero-dependency header-only image generation library for RawrXD IDE
 * 
 * 100% Qt-free -- pure C++20/STL only.
 * Provides: Canvas, Colors (sRGB/linear), Perlin/Simplex noise,
 * anti-aliased primitives (Xiaolin Wu), gradients, BMP export.
 * Optional PNG export via stb_image_write.h.
 */

// Place this before including image_generator.h to enable PNG export:
// #define STB_IMAGE_WRITE_IMPLEMENTATION
// #include "stb_image_write.h"

#include "colors.h"
#include "canvas.h"
#include "gradients.h"
#include "noise.h"
#include "primitives.h"

#include <fstream>
#include <string>
#include <cstdint>
#include <cstring>

namespace ig {

// ======================== Image Export (BMP) ========================

inline bool write_bmp(const Canvas& canvas, const std::string& path) {
    const uint32_t fileHeaderSize = 14;
    const uint32_t infoHeaderSize = 40;
    const uint32_t pixelDataSize = canvas.width * canvas.height * 4;
    const uint32_t fileSize = fileHeaderSize + infoHeaderSize + pixelDataSize;

    std::ofstream f(path, std::ios::binary);
    if (!f) return false;

    // File header
    uint8_t fileHeader[14] = {};
    fileHeader[0] = 'B';
    fileHeader[1] = 'M';
    std::memcpy(&fileHeader[2], &fileSize, 4);
    uint32_t dataOffset = fileHeaderSize + infoHeaderSize;
    std::memcpy(&fileHeader[10], &dataOffset, 4);
    f.write(reinterpret_cast<const char*>(fileHeader), 14);

    // Info header (BITMAPINFOHEADER)
    uint8_t infoHeader[40] = {};
    std::memcpy(&infoHeader[0], &infoHeaderSize, 4);
    int32_t w = canvas.width;
    int32_t h = -canvas.height; // negative = top-down
    std::memcpy(&infoHeader[4], &w, 4);
    std::memcpy(&infoHeader[8], &h, 4);
    uint16_t planes = 1;
    std::memcpy(&infoHeader[12], &planes, 2);
    uint16_t bpp = 32;
    std::memcpy(&infoHeader[14], &bpp, 2);
    f.write(reinterpret_cast<const char*>(infoHeader), 40);

    // Pixel data (BGRA)
    for (int y = 0; y < canvas.height; ++y) {
        for (int x = 0; x < canvas.width; ++x) {
            size_t idx = static_cast<size_t>((y * canvas.width + x) * 4);
            uint8_t pixel[4] = {
                canvas.data[idx + 2], // B
                canvas.data[idx + 1], // G
                canvas.data[idx + 0], // R
                canvas.data[idx + 3]  // A
            };
            f.write(reinterpret_cast<const char*>(pixel), 4);
        }
    }

    return true;
}

} // namespace ig
