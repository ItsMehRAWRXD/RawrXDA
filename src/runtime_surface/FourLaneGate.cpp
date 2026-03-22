#include "rawrxd/runtime/FourLaneGate.hpp"

#include "../logging/Logger.h"

namespace RawrXD::Runtime {

FourLaneGate& FourLaneGate::instance() {
    static FourLaneGate s;
    return s;
}

RuntimeResult FourLaneGate::acquire(Lane lane, bool asExplicitModule) {
    const std::size_t idx = static_cast<std::size_t>(lane);
    if (idx >= kLaneCount) {
        return std::unexpected(std::string("FourLaneGate: invalid lane"));
    }

    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_active[idx]) {
        return {};  // idempotent
    }

    const int limit = asExplicitModule ? kMaxConcurrentLanes + 1 : kMaxConcurrentLanes;
    if (m_count >= limit) {
        return std::unexpected(std::string("FourLaneGate: at capacity (") + std::to_string(m_count) +
                               ") — release a lane or pass asExplicitModule for fifth slot");
    }

    m_active[idx] = true;
    ++m_count;

    RawrXD::Logging::Logger::instance().info(
        "[FourLaneGate] acquire " + laneName(lane) + " (active=" + std::to_string(m_count) + ")",
        "RuntimeSurface");
    return {};
}

void FourLaneGate::release(Lane lane) {
    const std::size_t idx = static_cast<std::size_t>(lane);
    if (idx >= kLaneCount) {
        return;
    }
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_active[idx]) {
        return;
    }
    m_active[idx] = false;
    --m_count;
    RawrXD::Logging::Logger::instance().info(
        "[FourLaneGate] release " + laneName(lane) + " (active=" + std::to_string(m_count) + ")",
        "RuntimeSurface");
}

bool FourLaneGate::isActive(Lane lane) const {
    const std::size_t idx = static_cast<std::size_t>(lane);
    if (idx >= kLaneCount) {
        return false;
    }
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_active[idx];
}

int FourLaneGate::activeCount() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_count;
}

}  // namespace RawrXD::Runtime
