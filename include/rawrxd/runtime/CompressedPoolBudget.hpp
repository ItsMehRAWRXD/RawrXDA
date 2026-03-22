#pragma once

#include <atomic>
#include <cstdint>

namespace RawrXD::Runtime {

/// Pool cap tracks **compressed on-disk / wire** footprint only — not expanded math tensors.
/// `tryReserve` succeeds if used + n <= budget (0 = unlimited).
class CompressedPoolBudget {
public:
    static CompressedPoolBudget& instance();

    void setBudgetBytes(std::uint64_t n);
    [[nodiscard]] std::uint64_t budgetBytes() const { return m_budget.load(std::memory_order_relaxed); }

    [[nodiscard]] bool tryReserve(std::uint64_t n);
    void release(std::uint64_t n);

    [[nodiscard]] std::uint64_t usedBytes() const { return m_used.load(std::memory_order_relaxed); }

private:
    CompressedPoolBudget() = default;

    std::atomic<std::uint64_t> m_budget{0};
    std::atomic<std::uint64_t> m_used{0};
};

}  // namespace RawrXD::Runtime
