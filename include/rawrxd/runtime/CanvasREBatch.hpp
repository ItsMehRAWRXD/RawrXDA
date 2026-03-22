#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace RawrXD::Runtime {

/// Vectorized batch for reverse-engineering overlay draws (quads = 6 verts × float4 each).
struct CanvasREBatch {
    void clear();
    void reserve(std::size_t quads);

    /// Append axis-aligned quad in normalized device-ish space (x0,y0)-(x1,y1), z/w packed as (z, kind).
    void appendQuad(float x0, float y0, float x1, float y1, float z, float kind);

    [[nodiscard]] const float* floatData() const { return m_data.data(); }
    [[nodiscard]] std::size_t floatCount() const { return m_data.size(); }
    [[nodiscard]] std::size_t quadCount() const { return m_data.size() / 24; }

private:
    std::vector<float> m_data{};
};

}  // namespace RawrXD::Runtime
