#pragma once
#include <cstdint>

namespace RawrXD {

struct GlyphMetadata {
    float u1, v1, u2, v2; // Texture Coordinates
    float width, height;
};

class GlyphAtlas {
private:
    class GlyphAtlasImpl* m_impl;

public:
    GlyphAtlas();
    ~GlyphAtlas();

    // Use void* to avoid D2D1 header pollution in public API
    void Initialize(void* ctx, void* fontFace);

    // Instance Rendering: Draw 10,000 characters in ONE call
    void DrawCodeBuffer(void* ctx, const uint32_t* tokenBuffer, size_t len);
};

} // namespace RawrXD
