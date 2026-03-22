// GGUF tensor autopsy / diff — linked when RawrXD-ModelAnalysisLib is wired (RAWR_HAS_MODEL_ANATOMY).
#include "Win32IDE.h"

#if defined(RAWR_HAS_MODEL_ANATOMY)

#include "../core/model_anatomy.hpp"
#include "../core/neurological_diff.hpp"

std::string Win32IDE::getModelAnatomyJson(bool pretty) const
{
    if (m_loadedModelPath.empty()) {
        return {};
    }
    RawrXD::ModelAnatomy an;
    if (!RawrXD::BuildAnatomyFromGgufPath(m_loadedModelPath, an, nullptr, nullptr)) {
        return {};
    }
    return RawrXD::ExportAnatomyToJson(an, pretty);
}

std::string Win32IDE::getModelDiffJson(const std::string& pathA, const std::string& pathB, bool pretty) const
{
    RawrXD::ModelAnatomy a;
    RawrXD::ModelAnatomy b;
    if (!RawrXD::BuildAnatomyFromGgufPath(pathA, a, nullptr, nullptr)) {
        return {};
    }
    if (!RawrXD::BuildAnatomyFromGgufPath(pathB, b, nullptr, nullptr)) {
        return {};
    }
    const std::vector<RawrXD::DiffEntry> diff = RawrXD::DiffAnatomies(a, b, nullptr);
    return RawrXD::ExportDiffToJson(diff, a.modelName, b.modelName, pretty);
}

#endif  // RAWR_HAS_MODEL_ANATOMY
