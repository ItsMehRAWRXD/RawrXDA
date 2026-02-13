#pragma once
#include <cmath>
#include <array>
#include <vector>
#include <numeric>
#include <random>
#include "colors.h"

namespace ig {

// ======================== 2D Perlin Noise ========================

class Perlin2D {
private:
    std::array<int, 512> perm;

    static float fade(float t) { 
        return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f); 
    }

    static float grad(int h, float x, float y) {
        switch (h & 3) {
            case 0: return x + y;
            case 1: return -x + y;
            case 2: return x - y;
            default: return -x - y;
        }
    }

public:
    explicit Perlin2D(uint32_t seed = 1337) {
        std::vector<int> p(256);
        std::iota(p.begin(), p.end(), 0);
        std::mt19937 rng(seed);
        std::shuffle(p.begin(), p.end(), rng);
        for (int i = 0; i < 512; ++i) {
            perm[i] = p[i & 255];
        }
    }

    float noise(float x, float y) const {
        int X = static_cast<int>(std::floor(x)) & 255;
        int Y = static_cast<int>(std::floor(y)) & 255;
        float xf = x - std::floor(x);
        float yf = y - std::floor(y);

        int aa = perm[X + perm[Y]];
        int ab = perm[X + perm[Y + 1]];
        int ba = perm[X + 1 + perm[Y]];
        int bb = perm[X + 1 + perm[Y + 1]];

        float u = fade(xf);
        float v = fade(yf);

        float x1 = lerp(grad(aa, xf, yf), grad(ba, xf - 1.0f, yf), u);
        float x2 = lerp(grad(ab, xf, yf - 1.0f), grad(bb, xf - 1.0f, yf - 1.0f), u);
        return (lerp(x1, x2, v) * 0.5f) + 0.5f;
    }

    float octave_noise(float x, float y, int octaves, float persistence, float lacunarity) const {
        float result = 0.0f;
        float amplitude = 1.0f;
        float frequency = 1.0f;
        float max_value = 0.0f;

        for (int i = 0; i < octaves; ++i) {
            result += noise(x * frequency, y * frequency) * amplitude;
            max_value += amplitude;
            amplitude *= persistence;
            frequency *= lacunarity;
        }

        return result / max_value;
    }
};

// ======================== Simplex Noise (FastSimplex2D) ========================

class SimplexNoise {
private:
    std::array<int, 512> p;
    static constexpr float F2 = 0.366025403f;
    static constexpr float G2 = 0.211324865f;

    static inline int fast_floor(float x) {
        int xi = static_cast<int>(x);
        return x < xi ? xi - 1 : xi;
    }

    float grad(int hash, float x, float y) const {
        int h = hash & 15;
        float u = h < 8 ? x : y;
        float v = h < 8 ? y : x;
        return ((h & 1) == 0 ? u : -u) + ((h & 2) == 0 ? v : -v);
    }

public:
    explicit SimplexNoise(uint32_t seed = 1337) {
        std::vector<int> perm_base(256);
        std::iota(perm_base.begin(), perm_base.end(), 0);
        std::mt19937 rng(seed);
        std::shuffle(perm_base.begin(), perm_base.end(), rng);
        for (int i = 0; i < 256; ++i) {
            p[i] = perm_base[i];
            p[i + 256] = perm_base[i];
        }
    }

    float noise(float x, float y) const {
        float s = (x + y) * F2;
        int i = fast_floor(x + s);
        int j = fast_floor(y + s);

        float t = (i + j) * G2;
        float X0 = i - t;
        float Y0 = j - t;
        float x0 = x - X0;
        float y0 = y - Y0;

        int i1, j1;
        if (x0 > y0) {
            i1 = 1; j1 = 0;
        } else {
            i1 = 0; j1 = 1;
        }

        float x1 = x0 - i1 + G2;
        float y1 = y0 - j1 + G2;
        float x2 = x0 - 1.0f + 2.0f * G2;
        float y2 = y0 - 1.0f + 2.0f * G2;

        int ii = i & 255;
        int jj = j & 255;

        int gi0 = p[ii + p[jj]] % 16;
        int gi1 = p[ii + i1 + p[jj + j1]] % 16;
        int gi2 = p[ii + 1 + p[jj + 1]] % 16;

        float t0 = 0.5f - x0 * x0 - y0 * y0;
        float n0 = t0 < 0 ? 0 : t0 * t0 * t0 * t0 * grad(gi0, x0, y0);

        float t1 = 0.5f - x1 * x1 - y1 * y1;
        float n1 = t1 < 0 ? 0 : t1 * t1 * t1 * t1 * grad(gi1, x1, y1);

        float t2 = 0.5f - x2 * x2 - y2 * y2;
        float n2 = t2 < 0 ? 0 : t2 * t2 * t2 * t2 * grad(gi2, x2, y2);

        return 70.0f * (n0 + n1 + n2);
    }
};

} // namespace ig
