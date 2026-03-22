#include "rawrxd/runtime/CanvasREBatch.hpp"

namespace RawrXD::Runtime {

void CanvasREBatch::clear() {
    m_data.clear();
}

void CanvasREBatch::reserve(std::size_t quads) {
    m_data.reserve(quads * 24);
}

void CanvasREBatch::appendQuad(float x0, float y0, float x1, float y1, float z, float kind) {
    // Two triangles: (x0,y0)(x1,y0)(x0,y1) and (x1,y0)(x1,y1)(x0,y1)
    const float tri[6][4] = {
        {x0, y0, z, kind},
        {x1, y0, z, kind},
        {x0, y1, z, kind},
        {x1, y0, z, kind},
        {x1, y1, z, kind},
        {x0, y1, z, kind},
    };
    for (int i = 0; i < 6; ++i) {
        for (int j = 0; j < 4; ++j) {
            m_data.push_back(tri[i][j]);
        }
    }
}

}  // namespace RawrXD::Runtime
