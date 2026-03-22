#include "rawrxd/runtime/GenerationStopwatch.hpp"

#include "../logging/Logger.h"

#include <string>

namespace RawrXD::Runtime {

GenerationStopwatch& GenerationStopwatch::instance() {
    static GenerationStopwatch s;
    return s;
}

void GenerationStopwatch::beginGeneration() {
    if (m_generating.exchange(true, std::memory_order_acq_rel)) {
        return;
    }
    m_start = std::chrono::steady_clock::now();
    RawrXD::Logging::Logger::instance().debug("[GenStopwatch] generation begin", "RuntimeSurface");
}

void GenerationStopwatch::endGeneration() {
    if (!m_generating.exchange(false, std::memory_order_acq_rel)) {
        return;
    }
    const auto now = std::chrono::steady_clock::now();
    const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_start).count();
    m_totalMs.fetch_add(static_cast<std::uint64_t>(ms < 0 ? 0 : ms), std::memory_order_relaxed);
    RawrXD::Logging::Logger::instance().debug(
        "[GenStopwatch] generation end +" + std::to_string(ms) + "ms (total=" +
            std::to_string(m_totalMs.load(std::memory_order_relaxed)) + "ms)",
        "RuntimeSurface");
}

StopwatchGenerationScope::StopwatchGenerationScope() {
    GenerationStopwatch::instance().beginGeneration();
}

StopwatchGenerationScope::~StopwatchGenerationScope() {
    if (m_armed) {
        GenerationStopwatch::instance().endGeneration();
    }
}

}  // namespace RawrXD::Runtime
