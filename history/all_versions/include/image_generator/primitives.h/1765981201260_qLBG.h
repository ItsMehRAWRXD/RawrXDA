#pragma once
#include <vector>
#include <cmath>
#include "canvas.h"
#include "colors.h"

namespace ig {

// ======================== Line Drawing (Xiaolin Wu AA) ========================

static inline float ipart(float x) { return std::floor(x); }
static inline float fpart(float x) { return x - std::floor(x); }
static inline float rfpart(float x) { return 1.0f - fpart(x); }

static inline void plot_aa(Canvas& c, int x, int y, const Color& base, float coverage) {
    Color src = base;
    src.a = clamp01(base.a * coverage);
    c.blend(x, y, src);
}

inline void line_aa(Canvas& c, float x0, float y0, float x1, float y1, const Color& col) {
    bool steep = std::abs(y1 - y0) > std::abs(x1 - x0);
    if (steep) { 
        std::swap(x0, y0); 
        std::swap(x1, y1); 
    }
    if (x0 > x1) { 
        std::swap(x0, x1); 
        std::swap(y0, y1); 
    }

    float dx = x1 - x0;
    float dy = y1 - y0;
    float gradient = dx == 0 ? 1.0f : dy / dx;

    // First endpoint
    float xend = std::round(x0);
    float yend = y0 + gradient * (xend - x0);
    float xgap = rfpart(x0 + 0.5f);
    int xpxl1 = static_cast<int>(xend);
    int ypxl1 = static_cast<int>(ipart(yend));
    if (steep) {
        plot_aa(c, ypxl1, xpxl1, col, rfpart(yend) * xgap);
        plot_aa(c, ypxl1 + 1, xpxl1, col, fpart(yend) * xgap);
    } else {
        plot_aa(c, xpxl1, ypxl1, col, rfpart(yend) * xgap);
        plot_aa(c, xpxl1, ypxl1 + 1, col, fpart(yend) * xgap);
    }
    float intery = yend + gradient;

    // Second endpoint
    xend = std::round(x1);
    yend = y1 + gradient * (xend - x1);
    xgap = fpart(x1 + 0.5f);
    int xpxl2 = static_cast<int>(xend);
    int ypxl2 = static_cast<int>(ipart(yend));

    // Main loop
    if (steep) {
        for (int x = xpxl1 + 1; x < xpxl2; ++x) {
            plot_aa(c, static_cast<int>(ipart(intery)), x, col, rfpart(intery));
            plot_aa(c, static_cast<int>(ipart(intery)) + 1, x, col, fpart(intery));
            intery += gradient;
        }
        plot_aa(c, ypxl2, xpxl2, col, rfpart(yend) * xgap);
        plot_aa(c, ypxl2 + 1, xpxl2, col, fpart(yend) * xgap);
    } else {
        for (int x = xpxl1 + 1; x < xpxl2; ++x) {
            plot_aa(c, x, static_cast<int>(ipart(intery)), col, rfpart(intery));
            plot_aa(c, x, static_cast<int>(ipart(intery)) + 1, col, fpart(intery));
            intery += gradient;
        }
        plot_aa(c, xpxl2, ypxl2, col, rfpart(yend) * xgap);
        plot_aa(c, xpxl2, ypxl2 + 1, col, fpart(yend) * xgap);
    }
}

// ======================== Thick Line Drawing ========================

inline void line_thick(Canvas& c, float x0, float y0, float x1, float y1, float thickness, const Color& col) {
    float dx = x1 - x0;
    float dy = y1 - y0;
    float len = std::sqrt(dx * dx + dy * dy);
    if (len < 0.001f) {
        fill_circle(c, x0, y0, thickness * 0.5f, col);
        return;
    }

    float perpx = -dy / len * (thickness * 0.5f);
    float perpy = dx / len * (thickness * 0.5f);

    std::vector<std::pair<float, float>> rect = {
        {x0 + perpx, y0 + perpy},
        {x1 + perpx, y1 + perpy},
        {x1 - perpx, y1 - perpy},
        {x0 - perpx, y0 - perpy}
    };
    fill_polygon(c, rect, col);
}

// ======================== Rectangle ========================

inline void fill_rect(Canvas& c, int x, int y, int w, int h, const Color& col) {
    int x2 = x + w, y2 = y + h;
    for (int yy = y; yy < y2; ++yy) {
        for (int xx = x; xx < x2; ++xx) {
            c.blend(xx, yy, col);
        }
    }
}

inline void stroke_rect(Canvas& c, int x, int y, int w, int h, float thickness, const Color& col) {
    float t2 = thickness * 0.5f;
    // Top
    fill_rect(c, x - static_cast<int>(t2), y - static_cast<int>(t2), 
              w + 2 * static_cast<int>(t2), static_cast<int>(thickness), col);
    // Bottom
    fill_rect(c, x - static_cast<int>(t2), y + h - static_cast<int>(t2), 
              w + 2 * static_cast<int>(t2), static_cast<int>(thickness), col);
    // Left
    fill_rect(c, x - static_cast<int>(t2), y, 
              static_cast<int>(thickness), h, col);
    // Right
    fill_rect(c, x + w - static_cast<int>(t2), y, 
              static_cast<int>(thickness), h, col);
}

// ======================== Circle ========================

inline void fill_circle(Canvas& c, float cx, float cy, float r, const Color& col) {
    int minx = static_cast<int>(std::floor(cx - r));
    int maxx = static_cast<int>(std::ceil(cx + r));
    int miny = static_cast<int>(std::floor(cy - r));
    int maxy = static_cast<int>(std::ceil(cy + r));
    float r2 = r * r;

    for (int y = miny; y <= maxy; ++y) {
        for (int x = minx; x <= maxx; ++x) {
            float dx = x + 0.5f - cx;
            float dy = y + 0.5f - cy;
            float d2 = dx * dx + dy * dy;
            if (d2 <= r2) c.blend(x, y, col);
        }
    }
}

inline void stroke_circle(Canvas& c, float cx, float cy, float r, float thickness, const Color& col) {
    float r_inner = r - thickness * 0.5f;
    float r_outer = r + thickness * 0.5f;
    int minx = static_cast<int>(std::floor(cx - r_outer));
    int maxx = static_cast<int>(std::ceil(cx + r_outer));
    int miny = static_cast<int>(std::floor(cy - r_outer));
    int maxy = static_cast<int>(std::ceil(cy + r_outer));
    float r_inner2 = r_inner * r_inner;
    float r_outer2 = r_outer * r_outer;

    for (int y = miny; y <= maxy; ++y) {
        for (int x = minx; x <= maxx; ++x) {
            float dx = x + 0.5f - cx;
            float dy = y + 0.5f - cy;
            float d2 = dx * dx + dy * dy;
            if (d2 >= r_inner2 && d2 <= r_outer2) c.blend(x, y, col);
        }
    }
}

// ======================== Polygon Fill (Scanline) ========================

inline void fill_polygon(Canvas& c, const std::vector<std::pair<float, float>>& pts, const Color& col) {
    if (pts.size() < 3) return;

    float ymin = pts[0].second, ymax = pts[0].second;
    for (const auto& p : pts) {
        ymin = std::min(ymin, p.second);
        ymax = std::max(ymax, p.second);
    }

    int miny = static_cast<int>(std::floor(ymin));
    int maxy = static_cast<int>(std::ceil(ymax));

    for (int y = miny; y <= maxy; ++y) {
        std::vector<float> nodes;
        size_t j = pts.size() - 1;

        for (size_t i = 0; i < pts.size(); ++i) {
            float xi = pts[i].first, yi = pts[i].second;
            float xj = pts[j].first, yj = pts[j].second;
            bool cond = ((yi < y && yj >= y) || (yj < y && yi >= y));

            if (cond && std::abs(yj - yi) > 0.001f) {
                float x = xi + (y - yi) * (xj - xi) / (yj - yi);
                nodes.push_back(x);
            }
            j = i;
        }

        std::sort(nodes.begin(), nodes.end());

        for (size_t k = 0; k + 1 < nodes.size(); k += 2) {
            int xStart = static_cast<int>(std::floor(nodes[k]));
            int xEnd = static_cast<int>(std::ceil(nodes[k + 1]));
            for (int x = xStart; x < xEnd; ++x) {
                c.blend(x, y, col);
            }
        }
    }
}

// ======================== Ellipse ========================

inline void fill_ellipse(Canvas& c, float cx, float cy, float rx, float ry, const Color& col) {
    int minx = static_cast<int>(std::floor(cx - rx));
    int maxx = static_cast<int>(std::ceil(cx + rx));
    int miny = static_cast<int>(std::floor(cy - ry));
    int maxy = static_cast<int>(std::ceil(cy + ry));

    float rx2 = rx * rx;
    float ry2 = ry * ry;

    for (int y = miny; y <= maxy; ++y) {
        for (int x = minx; x <= maxx; ++x) {
            float dx = x + 0.5f - cx;
            float dy = y + 0.5f - cy;
            float t = (dx * dx) / rx2 + (dy * dy) / ry2;
            if (t <= 1.0f) c.blend(x, y, col);
        }
    }
}

// ======================== Flood Fill ========================

inline void flood_fill(Canvas& c, int x, int y, const Color& new_color) {
    if (!c.in_bounds(x, y)) return;

    Color original = c.get(x, y);
    if (original.r == new_color.r && original.g == new_color.g && 
        original.b == new_color.b && original.a == new_color.a) {
        return;
    }

    std::vector<std::pair<int, int>> queue;
    queue.push_back({x, y});

    while (!queue.empty()) {
        auto [cx, cy] = queue.back();
        queue.pop_back();

        if (!c.in_bounds(cx, cy)) continue;

        Color current = c.get(cx, cy);
        if (current.r != original.r || current.g != original.g || 
            current.b != original.b || current.a != original.a) {
            continue;
        }

        c.set(cx, cy, new_color);

        queue.push_back({cx + 1, cy});
        queue.push_back({cx - 1, cy});
        queue.push_back({cx, cy + 1});
        queue.push_back({cx, cy - 1});
    }
}

} // namespace ig
