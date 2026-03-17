#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace ig {

// ======================== Color Space & Utils ========================

static inline float clamp01(float x) { 
    return std::max(0.0f, std::min(1.0f, x)); 
}

static inline float lerp(float a, float b, float t) { 
    return a + (b - a) * t; 
}

static inline float srgb_to_linear(float c) {
    if (c <= 0.04045f) return c / 12.92f;
    return std::pow((c + 0.055f) / 1.055f, 2.4f);
}

static inline float linear_to_srgb(float c) {
    if (c <= 0.0031308f) return 12.92f * c;
    return 1.055f * std::pow(c, 1.0f / 2.4f) - 0.055f;
}

// ======================== Color Structure ========================

struct Color {
    // Stored as non-premultiplied sRGB floats in [0,1]
    float r, g, b, a;

    static Color rgba(float r, float g, float b, float a = 1.0f) {
        return {clamp01(r), clamp01(g), clamp01(b), clamp01(a)};
    }

    static Color rgb(float r, float g, float b) { 
        return rgba(r, g, b, 1.0f); 
    }

    static Color from_hex(uint32_t hex, float a = 1.0f) {
        float r = ((hex >> 16) & 0xFF) / 255.0f;
        float g = ((hex >> 8) & 0xFF) / 255.0f;
        float b = (hex & 0xFF) / 255.0f;
        return rgba(r, g, b, a);
    }

    uint32_t to_hex() const {
        uint32_t r = static_cast<uint32_t>(clamp01(this->r) * 255.0f);
        uint32_t g = static_cast<uint32_t>(clamp01(this->g) * 255.0f);
        uint32_t b = static_cast<uint32_t>(clamp01(this->b) * 255.0f);
        return (r << 16) | (g << 8) | b;
    }

    static Color white() { return rgb(1.0f, 1.0f, 1.0f); }
    static Color black() { return rgb(0.0f, 0.0f, 0.0f); }
    static Color red() { return rgb(1.0f, 0.0f, 0.0f); }
    static Color green() { return rgb(0.0f, 1.0f, 0.0f); }
    static Color blue() { return rgb(0.0f, 0.0f, 1.0f); }
    static Color transparent() { return rgba(0.0f, 0.0f, 0.0f, 0.0f); }
};

// ======================== Blending Modes ========================

static inline Color blend_src_over(const Color& src, const Color& dst) {
    float srcR = srgb_to_linear(src.r), srcG = srgb_to_linear(src.g), srcB = srgb_to_linear(src.b);
    float dstR = srgb_to_linear(dst.r), dstG = srgb_to_linear(dst.g), dstB = srgb_to_linear(dst.b);

    float outA = src.a + dst.a * (1.0f - src.a);
    float alpha = outA > 0 ? outA : 1.0f;
    float outR = (srcR * src.a + dstR * dst.a * (1.0f - src.a)) / alpha;
    float outG = (srcG * src.a + dstG * dst.a * (1.0f - src.a)) / alpha;
    float outB = (srcB * src.a + dstB * dst.a * (1.0f - src.a)) / alpha;

    return Color::rgba(linear_to_srgb(outR), linear_to_srgb(outG), linear_to_srgb(outB), outA);
}

static inline Color blend_multiply(const Color& src, const Color& dst) {
    return Color::rgba(
        src.r * dst.r,
        src.g * dst.g,
        src.b * dst.b,
        src.a * dst.a
    );
}

static inline Color blend_screen(const Color& src, const Color& dst) {
    return Color::rgba(
        1.0f - (1.0f - src.r) * (1.0f - dst.r),
        1.0f - (1.0f - src.g) * (1.0f - dst.g),
        1.0f - (1.0f - src.b) * (1.0f - dst.b),
        src.a + dst.a * (1.0f - src.a)
    );
}

} // namespace ig
