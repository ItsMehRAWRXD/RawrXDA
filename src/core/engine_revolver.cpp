#include "engine_revolver.hpp"

namespace RawrXD
{

EngineRevolver& EngineRevolver::instance()
{
    static EngineRevolver s;
    return s;
}

EngineRevolver::EngineRevolver()
{
    m_ids = {"default_a", "default_b", "default_c"};
}

std::expected<void, std::string> EngineRevolver::setRing(std::vector<std::string> ids)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (ids.empty())
        return std::unexpected(std::string("EngineRevolver: ring must be non-empty"));
    m_ids = std::move(ids);
    m_next = 0;
    m_generation = 0;
    return {};
}

std::expected<EngineRevolverStep, std::string> EngineRevolver::advance()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_ids.empty())
        return std::unexpected(std::string("EngineRevolver: empty ring"));

    const std::size_t n = m_ids.size();
    const std::size_t i = m_next % n;
    EngineRevolverStep step;
    step.index = i;
    step.id = m_ids[i];
    step.generation = m_generation;
    m_next = (m_next + 1) % n;
    ++m_generation;
    return step;
}

EngineRevolverStep EngineRevolver::peek() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    EngineRevolverStep step;
    if (m_ids.empty())
        return step;
    const std::size_t n = m_ids.size();
    step.index = m_next % n;
    step.id = m_ids[step.index];
    step.generation = m_generation;
    return step;
}

void EngineRevolver::reset()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_next = 0;
    m_generation = 0;
}

}  // namespace RawrXD
