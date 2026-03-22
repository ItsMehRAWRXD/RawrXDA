#include "rawrxd/runtime/CompressedPoolBudget.hpp"

#include "../logging/Logger.h"

#include <string>

namespace RawrXD::Runtime
{

CompressedPoolBudget& CompressedPoolBudget::instance()
{
    static CompressedPoolBudget s;
    return s;
}

void CompressedPoolBudget::setBudgetBytes(std::uint64_t n)
{
    m_budget.store(n, std::memory_order_relaxed);
    RawrXD::Logging::Logger::instance().info("[CompressedPoolBudget] budget_bytes=" + std::to_string(n),
                                             "RuntimeSurface");
}

bool CompressedPoolBudget::tryReserve(std::uint64_t n)
{
    const std::uint64_t cap = m_budget.load(std::memory_order_relaxed);
    if (cap == 0)
    {
        m_used.fetch_add(n, std::memory_order_relaxed);
        return true;
    }
    for (;;)
    {
        std::uint64_t u = m_used.load(std::memory_order_relaxed);
        if (u + n > cap)
        {
            return false;
        }
        if (m_used.compare_exchange_weak(u, u + n, std::memory_order_acq_rel))
        {
            return true;
        }
    }
}

void CompressedPoolBudget::release(std::uint64_t n)
{
    std::uint64_t prev = m_used.fetch_sub(n, std::memory_order_acq_rel);
    (void)prev;
}

}  // namespace RawrXD::Runtime
