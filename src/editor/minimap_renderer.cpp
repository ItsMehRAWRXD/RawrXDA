#include "minimap_renderer.h"

namespace RawrXD::Editor {
    MinimapData MinimapRenderer::render(const std::vector<std::string>& lines, size_t targetHeight) {
        MinimapData data;
        if (lines.empty() || targetHeight == 0) return data;

        data.lineMap.reserve(targetHeight);
        size_t linesPerSample = lines.size() / targetHeight;
        if (linesPerSample == 0) linesPerSample = 1;

        for (size_t i = 0; i < targetHeight; ++i) {
            size_t start = i * linesPerSample;
            size_t end = std::min(start + linesPerSample, lines.size());
            if (start >= lines.size()) break;

            MinimapLine ml;
            for (size_t j = start; j < end; ++j) {
                ml.sourceLine = static_cast<int>(j);
                // Simple density: non-whitespace ratio
                size_t nonWs = 0;
                for (char c : lines[j]) if (!isspace(static_cast<unsigned char>(c))) ++nonWs;
                ml.density = static_cast<float>(nonWs) / std::max<size_t>(1, lines[j].size());
            }
            data.lineMap.push_back(ml);
        }
        return data;
    }
}
