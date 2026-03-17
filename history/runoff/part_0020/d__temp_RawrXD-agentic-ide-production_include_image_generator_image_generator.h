#pragma once

// ======================== STB_IMAGE_WRITE (Header-only PNG/JPG/BMP) ========================
// Place this before including image_generator.h to enable PNG export
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

    auto write_u32 = [&](uint32_t v) { f.write(reinterpret_cast<const char*>(&v), 4); };
    auto write_u16 = [&](uint16_t v) { f.write(reinterpret_cast<const char*>(&v), 2); };

    // BITMAPFILEHEADER
    f.put('B');
    f.put('M');
    write_u32(fileSize);
    write_u16(0);
    write_u16(0);
    write_u32(fileHeaderSize + infoHeaderSize);

    // BITMAPINFOHEADER
    write_u32(infoHeaderSize);
    write_u32(static_cast<uint32_t>(canvas.width));
    write_u32(static_cast<uint32_t>(canvas.height));
    write_u16(1);        // planes
    write_u16(32);       // bpp
    write_u32(3);        // compression BI_BITFIELDS
    write_u32(pixelDataSize);
    write_u32(2835);     // ppm X ~ 72 DPI
    write_u32(2835);     // ppm Y
    write_u32(0);        // colors used
    write_u32(0);        // important colors

    // Color masks for BGRA
    write_u32(0x00FF0000);  // red mask
    write_u32(0x0000FF00);  // green mask
    write_u32(0x000000FF);  // blue mask
    write_u32(0xFF000000);  // alpha mask

    // Write pixels: BMP expects rows bottom-up; convert RGBA->BGRA
    for (int y = canvas.height - 1; y >= 0; --y) {
        for (int x = 0; x < canvas.width; ++x) {
            size_t idx = static_cast<size_t>((y * canvas.width + x) * 4);
            uint8_t R = canvas.data[idx + 0];
            uint8_t G = canvas.data[idx + 1];
            uint8_t B = canvas.data[idx + 2];
            uint8_t A = canvas.data[idx + 3];
            uint8_t bgra[4] = {B, G, R, A};
            f.write(reinterpret_cast<const char*>(bgra), 4);
        }
    }

    return true;
}

// ======================== PNG Export (Using stb_image_write if available) ========================

// Inline to avoid ODR violations when included across multiple translation units
#ifdef STB_IMAGE_WRITE_IMPLEMENTATION
inline bool write_png(const Canvas& canvas, const std::string& path) {
    // Convert RGBA to RGB (drop alpha for PNG compatibility)
    std::vector<uint8_t> rgb_data(canvas.width * canvas.height * 3);
    for (size_t i = 0; i < canvas.data.size(); i += 4) {
        size_t rgb_idx = (i / 4) * 3;
        rgb_data[rgb_idx + 0] = canvas.data[i + 0];      // R
        rgb_data[rgb_idx + 1] = canvas.data[i + 1];      // G
        rgb_data[rgb_idx + 2] = canvas.data[i + 2];      // B
    }
    return stbi_write_png(path.c_str(), canvas.width, canvas.height, 3, rgb_data.data(), 0) != 0;
}
#else
inline bool write_png(const Canvas& canvas, const std::string& path) {
    // Fallback to BMP if stb_image_write not available
    return write_bmp(canvas, path);
}
#endif

// ======================== Layer Management ========================

inline Canvas create_layer(int width, int height, const Color& clear_color = Color::transparent()) {
    Canvas layer(width, height);
    layer.clear(clear_color);
    return layer;
}

inline void composite_layer(Canvas& destination, const Canvas& source, int x = 0, int y = 0) {
    destination.composite(source, x, y);
}

// ======================== Filters (Basic) ========================

inline Canvas apply_brightness(const Canvas& src, float factor) {
    Canvas result(src.width, src.height);
    for (int y = 0; y < src.height; ++y) {
        for (int x = 0; x < src.width; ++x) {
            Color c = src.get(x, y);
            c.r = clamp01(c.r * factor);
            c.g = clamp01(c.g * factor);
            c.b = clamp01(c.b * factor);
            result.set(x, y, c);
        }
    }
    return result;
}

inline Canvas apply_contrast(const Canvas& src, float factor) {
    Canvas result(src.width, src.height);
    for (int y = 0; y < src.height; ++y) {
        for (int x = 0; x < src.width; ++x) {
            Color c = src.get(x, y);
            c.r = clamp01((c.r - 0.5f) * factor + 0.5f);
            c.g = clamp01((c.g - 0.5f) * factor + 0.5f);
            c.b = clamp01((c.b - 0.5f) * factor + 0.5f);
            result.set(x, y, c);
        }
    }
    return result;
}

inline Canvas apply_saturation(const Canvas& src, float factor) {
    Canvas result(src.width, src.height);
    for (int y = 0; y < src.height; ++y) {
        for (int x = 0; x < src.width; ++x) {
            Color c = src.get(x, y);
            float gray = (c.r + c.g + c.b) * 0.333333f;
            c.r = clamp01(lerp(gray, c.r, factor));
            c.g = clamp01(lerp(gray, c.g, factor));
            c.b = clamp01(lerp(gray, c.b, factor));
            result.set(x, y, c);
        }
    }
    return result;
}

// ======================== Gaussian Blur (Simple Box Blur Approximation) ========================

inline Canvas apply_blur(const Canvas& src, int radius) {
    Canvas result(src.width, src.height);
    result.clear(Color::transparent());

    if (radius <= 0) return src;

    for (int y = 0; y < src.height; ++y) {
        for (int x = 0; x < src.width; ++x) {
            float r = 0, g = 0, b = 0, a = 0;
            int count = 0;

            for (int dy = -radius; dy <= radius; ++dy) {
                for (int dx = -radius; dx <= radius; ++dx) {
                    Color sample = src.get(x + dx, y + dy);
                    r += sample.r;
                    g += sample.g;
                    b += sample.b;
                    a += sample.a;
                    count++;
                }
            }

            float inv_count = 1.0f / count;
            result.set(x, y, Color::rgba(r * inv_count, g * inv_count, b * inv_count, a * inv_count));
        }
    }

    return result;
}

} // namespace ig
