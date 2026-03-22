// ============================================================================
// model_runtime_gate.cpp
// ============================================================================

#include "model_runtime_gate.h"

#include "../logging/Logger.h"
#include "rawrxd/runtime/GenerationStopwatch.hpp"

#include <bit>
#include <cstdlib>

namespace RawrXD::Runtime {

namespace {

bool envTruthy(const char* v)
{
    if (!v || !*v) {
        return false;
    }
    return v[0] == '1' || v[0] == 'y' || v[0] == 'Y' || v[0] == 't' || v[0] == 'T';
}

} // namespace

ModelRuntimeGate& ModelRuntimeGate::instance()
{
    static ModelRuntimeGate s;
    return s;
}

std::uint32_t ModelRuntimeGate::laneBit(SubsystemLane lane)
{
    const auto idx = static_cast<std::uint32_t>(lane);
    if (lane == SubsystemLane::Extension) {
        return kExtensionBit;
    }
    if (idx > static_cast<std::uint32_t>(SubsystemLane::Memory)) {
        return kExtensionBit;
    }
    return 1u << idx;
}

bool ModelRuntimeGate::strictLaneBudget() const
{
    return envTruthy(std::getenv("RAWRXD_STRICT_RUNTIME_LANES"));
}

bool ModelRuntimeGate::tryAcquireLane(SubsystemLane lane)
{
    const std::uint32_t bit = laneBit(lane);
    if (lane == SubsystemLane::Extension) {
        for (;;) {
            std::uint32_t cur = m_laneMask.load(std::memory_order_acquire);
            std::uint32_t next = cur | bit;
            if (m_laneMask.compare_exchange_weak(cur, next, std::memory_order_acq_rel)) {
                return true;
            }
        }
    }

    for (;;) {
        std::uint32_t cur = m_laneMask.load(std::memory_order_acquire);
        if ((cur & bit) != 0) {
            return false;
        }
        const std::uint32_t core = cur & kCoreLaneMask;
        if (std::popcount(core) >= 4) {
            return false;
        }
        std::uint32_t next = cur | bit;
        if (m_laneMask.compare_exchange_weak(cur, next, std::memory_order_acq_rel)) {
            return true;
        }
    }
}

void ModelRuntimeGate::releaseLane(SubsystemLane lane)
{
    const std::uint32_t bit = laneBit(lane);
    for (;;) {
        std::uint32_t cur = m_laneMask.load(std::memory_order_acquire);
        std::uint32_t next = cur & ~bit;
        if (m_laneMask.compare_exchange_weak(cur, next, std::memory_order_acq_rel)) {
            return;
        }
    }
}

void ModelRuntimeGate::notifyModelResident(std::string_view modelKey, std::uint64_t bytes)
{
    std::lock_guard<std::mutex> lock(m_residentMutex);
    m_residentKey.assign(modelKey.begin(), modelKey.end());
    m_residentBytes = bytes;
    RawrXD::Logging::Logger::instance().info(
        std::string("[ModelRuntimeGate] Resident model: ") + m_residentKey + " (" + std::to_string(bytes) + " bytes)",
        "ModelRuntimeGate");
}

void ModelRuntimeGate::notifyModelUnloaded(std::string_view modelKey)
{
    std::lock_guard<std::mutex> lock(m_residentMutex);
    const std::string key(modelKey.begin(), modelKey.end());
    if (!m_residentKey.empty() && m_residentKey == key) {
        m_residentKey.clear();
        m_residentBytes = 0;
    }
    RawrXD::Logging::Logger::instance().info(
        std::string("[ModelRuntimeGate] Unloaded model: ") + key,
        "ModelRuntimeGate");
}

void ModelRuntimeGate::beginGeneration(std::string_view backend, std::string_view modelKey)
{
    std::lock_guard<std::mutex> lock(m_genMutex);
    m_genStart = std::chrono::steady_clock::now();
    m_generating.store(true, std::memory_order_release);
    m_generationCount.fetch_add(1, std::memory_order_acq_rel);
    GenerationStopwatch::instance().beginGeneration();
    RawrXD::Logging::Logger::instance().info(
        std::string("[ModelRuntimeGate] beginGeneration backend=") + std::string(backend) + " model=" + std::string(modelKey),
        "ModelRuntimeGate");
}

void ModelRuntimeGate::endGeneration()
{
    using clock = std::chrono::steady_clock;
    std::uint64_t ms = 0;
    {
        std::lock_guard<std::mutex> lock(m_genMutex);
        const auto now = clock::now();
        ms = static_cast<std::uint64_t>(
            std::chrono::duration_cast<std::chrono::milliseconds>(now - m_genStart).count());
        m_generating.store(false, std::memory_order_release);
    }
    GenerationStopwatch::instance().endGeneration();
    m_lastGenMs.store(ms, std::memory_order_release);
    RawrXD::Logging::Logger::instance().info(
        std::string("[ModelRuntimeGate] endGeneration duration_ms=") + std::to_string(ms),
        "ModelRuntimeGate");
}

GenerationScope::GenerationScope(std::string_view backend, std::string_view modelKey)
{
    ModelRuntimeGate::instance().beginGeneration(backend, modelKey);
    m_active = true;
}

GenerationScope::~GenerationScope()
{
    if (m_active) {
        ModelRuntimeGate::instance().endGeneration();
    }
}

LaneGuard::LaneGuard(SubsystemLane lane)
    : m_lane(lane)
{
    ModelRuntimeGate& g = ModelRuntimeGate::instance();
    m_ownsLane = g.tryAcquireLane(lane);
    if (m_ownsLane) {
        m_allowed = true;
        return;
    }
    if (g.strictLaneBudget()) {
        m_allowed = false;
        RawrXD::Logging::Logger::instance().warning(
            std::string("[ModelRuntimeGate] Lane busy (strict): ") + std::to_string(static_cast<unsigned>(lane)),
            "ModelRuntimeGate");
        return;
    }
    m_allowed = true;
    RawrXD::Logging::Logger::instance().warning(
        std::string("[ModelRuntimeGate] Lane busy (fail-open): ") + std::to_string(static_cast<unsigned>(lane)),
        "ModelRuntimeGate");
}

LaneGuard::~LaneGuard()
{
    if (m_ownsLane) {
        ModelRuntimeGate::instance().releaseLane(m_lane);
    }
}

} // namespace RawrXD::Runtime
