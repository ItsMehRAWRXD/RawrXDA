#pragma once
#include <vector>
#include <algorithm>
#include "colors.h"

namespace ig {

// ======================== Gradient Stops ========================

struct GradientStop {
    float t;
    Color c;
};

// ======================== Linear Gradient ========================

class LinearGradient {
public:
    std::vector<GradientStop> stops;
    float x0, y0, x1, y1;

    LinearGradient(float x0, float y0, float x1, float y1) 
        : x0(x0), y0(y0), x1(x1), y1(y1) {}

    void add_stop(float t, const Color& c) {
        stops.push_back({clamp01(t), c});
        std::sort(stops.begin(), stops.end(), 
            [](const auto& a, const auto& b) { return a.t < b.t; });
    }

    Color sample(float x, float y) const {
        float dx = x1 - x0, dy = y1 - y0;
        float len2 = dx * dx + dy * dy;
        float t = len2 > 0 ? ((x - x0) * dx + (y - y0) * dy) / len2 : 0.0f;
        t = clamp01(t);

        if (stops.empty()) return Color::transparent();
        if (t <= stops.front().t) return stops.front().c;
        if (t >= stops.back().t) return stops.back().c;

        for (size_t i = 0; i + 1 < stops.size(); ++i) {
            if (t >= stops[i].t && t <= stops[i + 1].t) {
                float u = (t - stops[i].t) / (stops[i + 1].t - stops[i].t);
                return Color::rgba(
                    lerp(stops[i].c.r, stops[i + 1].c.r, u),
                    lerp(stops[i].c.g, stops[i + 1].c.g, u),
                    lerp(stops[i].c.b, stops[i + 1].c.b, u),
                    lerp(stops[i].c.a, stops[i + 1].c.a, u)
                );
            }
        }
        return stops.back().c;
    }
};

// ======================== Radial Gradient ========================

class RadialGradient {
public:
    std::vector<GradientStop> stops;
    float cx, cy, r;

    RadialGradient(float cx, float cy, float r) 
        : cx(cx), cy(cy), r(std::max(1.0f, r)) {}

    void add_stop(float t, const Color& c) {
        stops.push_back({clamp01(t), c});
        std::sort(stops.begin(), stops.end(), 
            [](const auto& a, const auto& b) { return a.t < b.t; });
    }

    Color sample(float x, float y) const {
        float d = std::sqrt((x - cx) * (x - cx) + (y - cy) * (y - cy)) / r;
        float t = clamp01(d);

        if (stops.empty()) return Color::transparent();
        if (t <= stops.front().t) return stops.front().c;
        if (t >= stops.back().t) return stops.back().c;

        for (size_t i = 0; i + 1 < stops.size(); ++i) {
            if (t >= stops[i].t && t <= stops[i + 1].t) {
                float u = (t - stops[i].t) / (stops[i + 1].t - stops[i].t);
                return Color::rgba(
                    lerp(stops[i].c.r, stops[i + 1].c.r, u),
                    lerp(stops[i].c.g, stops[i + 1].c.g, u),
                    lerp(stops[i].c.b, stops[i + 1].c.b, u),
                    lerp(stops[i].c.a, stops[i + 1].c.a, u)
                );
            }
        }
        return stops.back().c;
    }
};

// ======================== Conic Gradient ========================

class ConicGradient {
public:
    std::vector<GradientStop> stops;
    float cx, cy, angle;

    ConicGradient(float cx, float cy, float angle = 0.0f) 
        : cx(cx), cy(cy), angle(angle) {}

    void add_stop(float t, const Color& c) {
        stops.push_back({clamp01(t), c});
        std::sort(stops.begin(), stops.end(), 
            [](const auto& a, const auto& b) { return a.t < b.t; });
    }

    Color sample(float x, float y) const {
        float dx = x - cx;
        float dy = y - cy;
        float theta = std::atan2(dy, dx) - angle;
        // Normalize to [0, 1]
        float t = (theta + 3.14159265f) / (2.0f * 3.14159265f);
        t = clamp01(t);

        if (stops.empty()) return Color::transparent();
        if (t <= stops.front().t) return stops.front().c;
        if (t >= stops.back().t) return stops.back().c;

        for (size_t i = 0; i + 1 < stops.size(); ++i) {
            if (t >= stops[i].t && t <= stops[i + 1].t) {
                float u = (t - stops[i].t) / (stops[i + 1].t - stops[i].t);
                return Color::rgba(
                    lerp(stops[i].c.r, stops[i + 1].c.r, u),
                    lerp(stops[i].c.g, stops[i + 1].c.g, u),
                    lerp(stops[i].c.b, stops[i + 1].c.b, u),
                    lerp(stops[i].c.a, stops[i + 1].c.a, u)
                );
            }
        }
        return stops.back().c;
    }
};

} // namespace ig
