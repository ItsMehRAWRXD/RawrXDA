#include "quantum_dynamic_time_manager.hpp"

#include <algorithm>

namespace RawrXD::Agent {

QuantumDynamicTimeManager::QuantumDynamicTimeManager(AdjustmentStrategy strategy)
    : m_strategy(strategy)
    , m_random_generator(m_random_device())
    , m_gaussian_dist(1.0f, 0.15f)
    , m_uniform_dist(0.7f, 1.3f)
    , m_masm_enabled(false)
    , m_masm_context(nullptr)
    , m_quantum_enabled(false)
    , m_quantum_optimization_enabled(false)
    , m_temporal_uncertainty(0.1f)
    , m_optimization_strength(1.0f)
    , m_last_system_update(std::chrono::steady_clock::now())
    , m_cached_system_load(0.0f)
    , m_cached_memory_pressure(0.0f)
    , m_cached_network_quality(1.0f)
    , m_system_start_time(std::chrono::steady_clock::now())
    , m_learning_rate(0.1f)
    , m_adaptation_window_size(128)
    , m_enable_continuous_learning(true) {
    m_running.store(false);
}

QuantumDynamicTimeManager::~QuantumDynamicTimeManager() {
    m_running.store(false);
    if (m_optimization_thread.joinable()) {
        m_optimization_thread.join();
    }
    if (m_monitoring_thread.joinable()) {
        m_monitoring_thread.join();
    }
}

void QuantumDynamicTimeManager::configurePwshLimits(std::chrono::milliseconds min_time,
                                                    std::chrono::milliseconds max_time) {
    if (min_time > max_time) {
        std::swap(min_time, max_time);
    }
    std::lock_guard<std::mutex> lock(m_pwsh_mutex);
    m_pwsh_config.min_timeout = min_time;
    m_pwsh_config.max_timeout = max_time;
    m_pwsh_config.default_timeout = std::clamp(
        m_pwsh_config.default_timeout, m_pwsh_config.min_timeout, m_pwsh_config.max_timeout);
}

void QuantumDynamicTimeManager::setPwshRandomization(bool enable, float min_factor, float max_factor) {
    if (min_factor > max_factor) {
        std::swap(min_factor, max_factor);
    }
    std::lock_guard<std::mutex> lock(m_pwsh_mutex);
    m_pwsh_config.enable_randomization = enable;
    m_pwsh_config.min_random_factor = std::max(0.01f, min_factor);
    m_pwsh_config.max_random_factor = std::max(m_pwsh_config.min_random_factor, max_factor);
}

}  // namespace RawrXD::Agent
