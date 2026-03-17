#pragma once
#include <cstdint>
#include <vector>
#include "colors.h"

namespace ig {

// ======================== Canvas Class ========================

class Canvas {
public:
    const int width;
    const int height;
    std::vector<uint8_t> data; // RGBA 8-bit per channel

    Canvas(int w, int h) 
        : width(w), height(h), data(static_cast<size_t>(w * h * 4), 0) {}

    Canvas(const Canvas& other) 
        : width(other.width), height(other.height), data(other.data) {}

    Canvas(Canvas&& other) noexcept 
        : width(other.width), height(other.height), data(std::move(other.data)) {}

    Canvas& operator=(const Canvas& other) {
        if (this != &other) {
            const_cast<int&>(width) = other.width;
            const_cast<int&>(height) = other.height;
            data = other.data;
        }
        return *this;
    }

    void clear(const Color& c) {
        uint8_t R = static_cast<uint8_t>(clamp01(c.r) * 255.0f);
        uint8_t G = static_cast<uint8_t>(clamp01(c.g) * 255.0f);
        uint8_t B = static_cast<uint8_t>(clamp01(c.b) * 255.0f);
        uint8_t A = static_cast<uint8_t>(clamp01(c.a) * 255.0f);
        for (size_t i = 0; i < data.size(); i += 4) {
            data[i + 0] = R; data[i + 1] = G; data[i + 2] = B; data[i + 3] = A;
        }
    }

    bool in_bounds(int x, int y) const {
        return x >= 0 && y >= 0 && x < width && y < height;
    }

    Color get(int x, int y) const {
        if (!in_bounds(x, y)) return Color::transparent();
        size_t idx = static_cast<size_t>((y * width + x) * 4);
        return Color::rgba(
            data[idx] / 255.0f,
            data[idx + 1] / 255.0f,
            data[idx + 2] / 255.0f,
            data[idx + 3] / 255.0f
        );
    }

    void set(int x, int y, const Color& c) {
        if (!in_bounds(x, y)) return;
        size_t idx = static_cast<size_t>((y * width + x) * 4);
        data[idx + 0] = static_cast<uint8_t>(clamp01(c.r) * 255.0f);
        data[idx + 1] = static_cast<uint8_t>(clamp01(c.g) * 255.0f);
        data[idx + 2] = static_cast<uint8_t>(clamp01(c.b) * 255.0f);
        data[idx + 3] = static_cast<uint8_t>(clamp01(c.a) * 255.0f);
    }

    void blend(int x, int y, const Color& src) {
        if (!in_bounds(x, y)) return;
        Color dst = get(x, y);
        set(x, y, blend_src_over(src, dst));
    }

    void blend_mode(int x, int y, const Color& src, int mode) {
        if (!in_bounds(x, y)) return;
        Color dst = get(x, y);
        Color result;
        switch (mode) {
            case 0: result = blend_src_over(src, dst); break;      // SRC_OVER
            case 1: result = blend_multiply(src, dst); break;      // MULTIPLY
            case 2: result = blend_screen(src, dst); break;        // SCREEN
            default: result = blend_src_over(src, dst);
        }
        set(x, y, result);
    }

    void fill_rect_area(int x1, int y1, int x2, int y2, const Color& c) {
        for (int y = y1; y < y2; ++y) {
            for (int x = x1; x < x2; ++x) {
                blend(x, y, c);
            }
        }
    }

    Canvas get_region(int x, int y, int w, int h) const {
        Canvas sub(w, h);
        for (int yy = 0; yy < h; ++yy) {
            for (int xx = 0; xx < w; ++xx) {
                sub.set(xx, yy, get(x + xx, y + yy));
            }
        }
        return sub;
    }

    void composite(const Canvas& src, int dx = 0, int dy = 0) {
        for (int y = 0; y < src.height; ++y) {
            for (int x = 0; x < src.width; ++x) {
                blend(dx + x, dy + y, src.get(x, y));
            }
        }
    }

    void fill(const Color& c) {
        fill_rect_area(0, 0, width, height, c);
    }

    size_t get_data_size() const { return data.size(); }
};

} // namespace ig
