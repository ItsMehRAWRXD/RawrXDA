#pragma once
/*  primitives.h - Image Generator Primitives
    Provides basic drawing primitives: colors, canvas, shapes, gradients, and noise
    No external dependencies - C++17 only
*/

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>
#include <cstring>

namespace ig {

// ======================== Math & Utils ========================
static inline float clamp01(float x) { return std::max(0.0f, std::min(1.0f, x)); }
static inline float lerp(float a, float b, float t) { return a + (b - a) * t; }

static inline float srgb_to_linear(float c) {
    if (c <= 0.04045f) return c / 12.92f;
    return std::pow((c + 0.055f) / 1.055f, 2.4f);
}

static inline float linear_to_srgb(float c) {
    if (c <= 0.0031308f) return 12.92f * c;
    return 1.055f * std::pow(c, 1.0f / 2.4f) - 0.055f;
}

struct RNG {
    std::mt19937 rng;
    explicit RNG(uint32_t seed = 0xC0FFEEu) : rng(seed) {}
    float uniform() { return std::uniform_real_distribution<float>(0.0f, 1.0f)(rng); }
};

// ======================== Color ========================
struct Color {
    // Stored as non-premultiplied sRGB floats in [0,1]
    float r, g, b, a;
    
    static Color rgba(float r, float g, float b, float a = 1.0f) {
        return {clamp01(r), clamp01(g), clamp01(b), clamp01(a)};
    }
    
    static Color rgb(float r, float g, float b) { 
        return rgba(r, g, b, 1.0f); 
    }
};

static inline Color blend_src_over(const Color& src, const Color& dst) {
    // Convert to linear space for blending then back to sRGB
    float srcR = srgb_to_linear(src.r), srcG = srgb_to_linear(src.g), srcB = srgb_to_linear(src.b);
    float dstR = srgb_to_linear(dst.r), dstG = srgb_to_linear(dst.g), dstB = srgb_to_linear(dst.b);

    float outA = src.a + dst.a * (1.0f - src.a);
    float outR = (srcR * src.a + dstR * dst.a * (1.0f - src.a)) / (outA > 0 ? outA : 1.0f);
    float outG = (srcG * src.a + dstG * dst.a * (1.0f - src.a)) / (outA > 0 ? outA : 1.0f);
    float outB = (srcB * src.a + dstB * dst.a * (1.0f - src.a)) / (outA > 0 ? outA : 1.0f);

    return Color::rgba(linear_to_srgb(outR), linear_to_srgb(outG), linear_to_srgb(outB), outA);
}

// ======================== Canvas ========================
class Canvas {
public:
    const int width;
    const int height;
    std::vector<uint8_t> data; // RGBA 8-bit

    Canvas(int w, int h) : width(w), height(h), data(static_cast<size_t>(w*h*4), 0) {}

    void clear(const Color& c) {
        uint8_t R = static_cast<uint8_t>(clamp01(c.r)*255.0f);
        uint8_t G = static_cast<uint8_t>(clamp01(c.g)*255.0f);
        uint8_t B = static_cast<uint8_t>(clamp01(c.b)*255.0f);
        uint8_t A = static_cast<uint8_t>(clamp01(c.a)*255.0f);
        for (size_t i = 0; i < data.size(); i += 4) {
            data[i+0] = R; data[i+1] = G; data[i+2] = B; data[i+3] = A;
        }
    }

    bool in_bounds(int x, int y) const {
        return x >= 0 && y >= 0 && x < width && y < height;
    }

    Color get(int x, int y) const {
        if (!in_bounds(x,y)) return Color::rgba(0,0,0,0);
        size_t idx = static_cast<size_t>((y*width + x)*4);
        return Color::rgba(data[idx]/255.0f, data[idx+1]/255.0f, data[idx+2]/255.0f, data[idx+3]/255.0f);
    }

    void set(int x, int y, const Color& c) {
        if (!in_bounds(x,y)) return;
        size_t idx = static_cast<size_t>((y*width + x)*4);
        data[idx+0] = static_cast<uint8_t>(clamp01(c.r)*255.0f);
        data[idx+1] = static_cast<uint8_t>(clamp01(c.g)*255.0f);
        data[idx+2] = static_cast<uint8_t>(clamp01(c.b)*255.0f);
        data[idx+3] = static_cast<uint8_t>(clamp01(c.a)*255.0f);
    }

    void blend(int x, int y, const Color& src) {
        if (!in_bounds(x,y)) return;
        Color dst = get(x,y);
        set(x,y, blend_src_over(src, dst));
    }
};

// ======================== Gradients ========================
struct GradientStop { float t; Color c; };

class LinearGradient {
public:
    std::vector<GradientStop> stops;
    float x0, y0, x1, y1;

    LinearGradient(float x0, float y0, float x1, float y1) 
        : x0(x0), y0(y0), x1(x1), y1(y1) {}

    void add_stop(float t, const Color& c) {
        stops.push_back({clamp01(t), c});
        std::sort(stops.begin(), stops.end(), [](auto&a, auto&b){ return a.t < b.t; });
    }

    Color sample(float x, float y) const {
        float dx = x1 - x0, dy = y1 - y0;
        float len2 = dx*dx + dy*dy;
        float t = len2 > 0 ? ((x - x0)*dx + (y - y0)*dy) / len2 : 0.0f;
        t = clamp01(t);
        if (stops.empty()) return Color::rgba(0,0,0,0);
        if (t <= stops.front().t) return stops.front().c;
        if (t >= stops.back().t) return stops.back().c;
        for (size_t i = 0; i + 1 < stops.size(); ++i) {
            if (t >= stops[i].t && t <= stops[i+1].t) {
                float u = (t - stops[i].t) / (stops[i+1].t - stops[i].t);
                return Color::rgba(
                    lerp(stops[i].c.r, stops[i+1].c.r, u),
                    lerp(stops[i].c.g, stops[i+1].c.g, u),
                    lerp(stops[i].c.b, stops[i+1].c.b, u),
                    lerp(stops[i].c.a, stops[i+1].c.a, u)
                );
            }
        }
        return stops.back().c;
    }
};

class RadialGradient {
public:
    std::vector<GradientStop> stops;
    float cx, cy, r;
    
    RadialGradient(float cx, float cy, float r) 
        : cx(cx), cy(cy), r(std::max(1.0f, r)) {}
    
    void add_stop(float t, const Color& c) {
        stops.push_back({clamp01(t), c});
        std::sort(stops.begin(), stops.end(), [](auto&a, auto&b){ return a.t < b.t; });
    }
    
    Color sample(float x, float y) const {
        float d = std::sqrt((x-cx)*(x-cx) + (y-cy)*(y-cy)) / r;
        float t = clamp01(d);
        if (stops.empty()) return Color::rgba(0,0,0,0);
        if (t <= stops.front().t) return stops.front().c;
        if (t >= stops.back().t) return stops.back().c;
        for (size_t i = 0; i + 1 < stops.size(); ++i) {
            if (t >= stops[i].t && t <= stops[i+1].t) {
                float u = (t - stops[i].t) / (stops[i+1].t - stops[i].t);
                return Color::rgba(
                    lerp(stops[i].c.r, stops[i+1].c.r, u),
                    lerp(stops[i].c.g, stops[i+1].c.g, u),
                    lerp(stops[i].c.b, stops[i+1].c.b, u),
                    lerp(stops[i].c.a, stops[i+1].c.a, u)
                );
            }
        }
        return stops.back().c;
    }
};

// ======================== Noise ========================
class Perlin2D {
    std::array<int, 512> perm;
    
    static float fade(float t){ return t*t*t*(t*(t*6 - 15) + 10); }
    
    static float grad(int h, float x, float y){
        switch (h & 3) {
            case 0: return  x + y;
            case 1: return -x + y;
            case 2: return  x - y;
            default: return -x - y;
        }
    }
    
public:
    explicit Perlin2D(uint32_t seed = 1337) {
        std::vector<int> p(256);
        std::iota(p.begin(), p.end(), 0);
        std::mt19937 rng(seed);
        std::shuffle(p.begin(), p.end(), rng);
        for (int i = 0; i < 512; ++i) perm[i] = p[i & 255];
    }

    float noise(float x, float y) const {
        int X = static_cast<int>(std::floor(x)) & 255;
        int Y = static_cast<int>(std::floor(y)) & 255;
        float xf = x - std::floor(x);
        float yf = y - std::floor(y);
        int aa = perm[X     + perm[Y    ]];
        int ab = perm[X     + perm[Y + 1]];
        int ba = perm[X + 1 + perm[Y    ]];
        int bb = perm[X + 1 + perm[Y + 1]];
        float u = fade(xf);
        float v = fade(yf);
        float x1 = lerp(grad(aa, xf, yf), grad(ba, xf - 1, yf), u);
        float x2 = lerp(grad(ab, xf, yf - 1), grad(bb, xf - 1, yf - 1), u);
        return (lerp(x1, x2, v) * 0.5f) + 0.5f; // [0,1]
    }
};

} // namespace ig
