#pragma once

#include "RuntimeTypes.hpp"

#include <array>
#include <cstddef>
#include <mutex>

namespace RawrXD::Runtime {

/// Enforces at most kMaxConcurrentLanes active among Parse/Execute/Ui/Render/Memory.
/// No dynamic plugin loading: lane order is fixed at link time; only activation bitmap changes.
class FourLaneGate {
public:
    static FourLaneGate& instance();

    [[nodiscard]] RuntimeResult acquire(Lane lane, bool asExplicitModule = false);
    void release(Lane lane);

    [[nodiscard]] bool isActive(Lane lane) const;
    [[nodiscard]] int activeCount() const;

    /// Compile-time lane priority (static ordering — not dlopen). Lower index = preferred when contended.
    static constexpr std::array<Lane, kLaneCount> staticPriorityOrder() {
        return {Lane::Memory, Lane::Execute, Lane::Parse, Lane::Render, Lane::Ui};
    }

private:
    FourLaneGate() = default;

    mutable std::mutex m_mutex{};
    std::array<bool, kLaneCount> m_active{};
    int m_count = 0;
};

}  // namespace RawrXD::Runtime
