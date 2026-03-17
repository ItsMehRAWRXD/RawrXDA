#pragma once
/*  drawing.h - Drawing Operations
    Line drawing, polygon fill, circle fill, rectangle fill
    Includes Xiaolin Wu anti-aliasing for lines
*/

#include "primitives.h"
#include <cmath>

namespace ig {

// ======================== Anti-aliased line drawing (Xiaolin Wu) ========================
static inline float ipart(float x){ return std::floor(x); }
static inline float fpart(float x){ return x - std::floor(x); }
static inline float rfpart(float x){ return 1.0f - fpart(x); }

static inline void plot_aa(Canvas& c, int x, int y, const Color& base, float coverage) {
    Color src = base;
    src.a = clamp01(base.a * coverage);
    c.blend(x, y, src);
}

void line_aa(Canvas& c, float x0, float y0, float x1, float y1, const Color& col) {
    bool steep = std::abs(y1 - y0) > std::abs(x1 - x0);
    if (steep) { std::swap(x0, y0); std::swap(x1, y1); }
    if (x0 > x1) { std::swap(x0, x1); std::swap(y0, y1); }

    float dx = x1 - x0;
    float dy = y1 - y0;
    float gradient = dx == 0 ? 1 : dy / dx;

    // first endpoint
    float xend = std::round(x0);
    float yend = y0 + gradient * (xend - x0);
    float xgap = rfpart(x0 + 0.5f);
    int xpxl1 = static_cast<int>(xend);
    int ypxl1 = static_cast<int>(ipart(yend));
    if (steep) {
        plot_aa(c, ypxl1,   xpxl1, col, rfpart(yend) * xgap);
        plot_aa(c, ypxl1+1, xpxl1, col, fpart(yend) * xgap);
    } else {
        plot_aa(c, xpxl1, ypxl1,   col, rfpart(yend) * xgap);
        plot_aa(c, xpxl1, ypxl1+1, col, fpart(yend) * xgap);
    }
    float intery = yend + gradient;

    // second endpoint
    xend = std::round(x1);
    yend = y1 + gradient * (xend - x1);
    xgap = fpart(x1 + 0.5f);
    int xpxl2 = static_cast<int>(xend);
    int ypxl2 = static_cast<int>(ipart(yend));
    
    // main loop
    if (steep) {
        for (int x = xpxl1 + 1; x < xpxl2; ++x) {
            plot_aa(c, static_cast<int>(ipart(intery)), x, col, rfpart(intery));
            plot_aa(c, static_cast<int>(ipart(intery))+1, x, col, fpart(intery));
            intery += gradient;
        }
        plot_aa(c, ypxl2,   xpxl2, col, rfpart(yend) * xgap);
        plot_aa(c, ypxl2+1, xpxl2, col, fpart(yend) * xgap);
    } else {
        for (int x = xpxl1 + 1; x < xpxl2; ++x) {
            plot_aa(c, x, static_cast<int>(ipart(intery)),   col, rfpart(intery));
            plot_aa(c, x, static_cast<int>(ipart(intery))+1, col, fpart(intery));
            intery += gradient;
        }
        plot_aa(c, xpxl2, ypxl2,   col, rfpart(yend) * xgap);
        plot_aa(c, xpxl2, ypxl2+1, col, fpart(yend) * xgap);
    }
}

// ======================== Simple shapes ========================
void fill_rect(Canvas& c, int x, int y, int w, int h, const Color& col) {
    int x2 = x + w, y2 = y + h;
    for (int yy = y; yy < y2; ++yy) {
        for (int xx = x; xx < x2; ++xx) {
            c.blend(xx, yy, col);
        }
    }
}

void fill_circle(Canvas& c, float cx, float cy, float r, const Color& col) {
    int minx = static_cast<int>(std::floor(cx - r));
    int maxx = static_cast<int>(std::ceil (cx + r));
    int miny = static_cast<int>(std::floor(cy - r));
    int maxy = static_cast<int>(std::ceil (cy + r));
    float r2 = r * r;
    for (int y = miny; y <= maxy; ++y) {
        for (int x = minx; x <= maxx; ++x) {
            float dx = x + 0.5f - cx;
            float dy = y + 0.5f - cy;
            float d2 = dx*dx + dy*dy;
            if (d2 <= r2) c.blend(x, y, col);
        }
    }
}

// ======================== Polygon fill (scanline) ========================
void fill_polygon(Canvas& c, const std::vector<std::pair<float,float>>& pts, const Color& col) {
    if (pts.size() < 3) return;
    float ymin = pts[0].second, ymax = pts[0].second;
    for (auto& p : pts) { ymin = std::min(ymin, p.second); ymax = std::max(ymax, p.second); }
    int miny = static_cast<int>(std::floor(ymin));
    int maxy = static_cast<int>(std::ceil(ymax));
    for (int y = miny; y <= maxy; ++y) {
        std::vector<float> nodes;
        size_t j = pts.size() - 1;
        for (size_t i = 0; i < pts.size(); ++i) {
            float xi = pts[i].first, yi = pts[i].second;
            float xj = pts[j].first, yj = pts[j].second;
            bool cond = ((yi < y && yj >= y) || (yj < y && yi >= y));
            if (cond && (yj - yi) != 0) {
                float x = xi + (y - yi) * (xj - xi) / (yj - yi);
                nodes.push_back(x);
            }
            j = i;
        }
        std::sort(nodes.begin(), nodes.end());
        for (size_t k = 0; k + 1 < nodes.size(); k += 2) {
            int xStart = static_cast<int>(std::floor(nodes[k]));
            int xEnd   = static_cast<int>(std::ceil (nodes[k+1]));
            for (int x = xStart; x < xEnd; ++x) c.blend(x, y, col);
        }
    }
}

} // namespace ig
