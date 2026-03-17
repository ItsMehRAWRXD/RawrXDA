#include "quantum_dynamic_time_manager.hpp"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <numeric>
#include <random>
#include <sstream>

extern "C" {
#include <windows.h>
}

namespace RawrXD::Agent {

namespace {
constexpr float SYSTEM_LOAD_IMPACT = 0.30f;
constexpr float MEMORY_PRESSURE_IMPACT = 0.20f;
constexpr float NETWORK_IMPACT = 0.15f;
constexpr float QUANTUM_UNCERTAINTY_BASE = 0.05f;
constexpr float DEFAULT_LEARNING_RATE = 0.10f;
constexpr int MIN_HISTORY_SAMPLES = 32;
constexpr char PROFILE_DB_FILE[] = "quantum_time_profiles.dat";
constexpr double PI_D = 3.14159265358979323846;

std::atomic<uint64_t> g_session_counter{0};
std::mutex g_checkpoint_mutex;
std::vector<std::string> g_checkpoint_names = {"Analysis", "Planning", "Implementation", "Validation"};

using TimeProfile = QuantumDynamicTimeManager::TimeProfile;
using ExecutionContext = QuantumDynamicTimeManager::ExecutionContext;
using TimeAllocation = QuantumDynamicTimeManager::TimeAllocation;

std::chrono::milliseconds scaleDuration(std::chrono::milliseconds value, float factor) {
    const auto scaled = static_cast<double>(value.count()) * static_cast<double>(factor);
    const auto non_negative = std::max(0.0, scaled);
    return std::chrono::milliseconds(static_cast<long long>(std::llround(non_negative)));
}

std::chrono::milliseconds clampDuration(std::chrono::milliseconds value,
                                        std::chrono::milliseconds min_time,
                                        std::chrono::milliseconds max_time) {
    if (min_time > max_time) {
        std::swap(min_time, max_time);
    }
    return std::max(min_time, std::min(value, max_time));
}

std::vector<std::string> splitLine(const std::string& line, char delimiter) {
    std::vector<std::string> parts;
    std::stringstream ss(line);
    std::string token;
    while (std::getline(ss, token, delimiter)) {
        parts.push_back(token);
    }
    return parts;
}

std::string sanitizeField(const std::string& value) {
    std::string out = value;
    std::replace(out.begin(), out.end(), '|', '_');
    std::replace(out.begin(), out.end(), '\n', ' ');
    std::replace(out.begin(), out.end(), '\r', ' ');
    return out;
}

std::string escapePowershellCommand(const std::string& command) {
    std::string escaped;
    escaped.reserve(command.size() + 8);
    for (char ch : command) {
        if (ch == '"') {
            escaped += "\\\"";
        } else {
            escaped += ch;
        }
    }
    return escaped;
}

uint64_t fileTimeToUInt64(const FILETIME& ft) {
    ULARGE_INTEGER value;
    value.LowPart = ft.dwLowDateTime;
    value.HighPart = ft.dwHighDateTime;
    return value.QuadPart;
}

}  // namespace

void QuantumDynamicTimeManager::configurePwshLimits(std::chrono::milliseconds min_time,
                                                    std::chrono::milliseconds max_time) {
    if (min_time > max_time) {
        std::swap(min_time, max_time);
    }
    std::lock_guard<std::mutex> lock(m_pwsh_mutex);
    m_pwsh_config.min_timeout = min_time;
    m_pwsh_config.max_timeout = max_time;
    m_pwsh_config.default_timeout = clampDuration(
        m_pwsh_config.default_timeout,
        m_pwsh_config.min_timeout,
        m_pwsh_config.max_timeout);
}

void QuantumDynamicTimeManager::setPwshRandomization(bool enable, float min_factor, float max_factor) {
    if (min_factor > max_factor) {
        std::swap(min_factor, max_factor);
    }
    std::lock_guard<std::mutex> lock(m_pwsh_mutex);
    m_pwsh_config.enable_randomization = enable;
    m_pwsh_config.min_random_factor = std::max(0.05f, min_factor);
    m_pwsh_config.max_random_factor = std::max(m_pwsh_config.min_random_factor, max_factor);
}

void QuantumDynamicTimeManager::addTimeProfile(const std::string& task_type, const TimeProfile& profile) {
    if (task_type.empty()) {
        return;
    }
    std::lock_guard<std::mutex> lock(m_profiles_mutex);
    TimeProfile copy = profile;
    copy.task_type = task_type;
    m_time_profiles[task_type] = copy;
}

void QuantumDynamicTimeManager::updateTimeProfile(const std::string& task_type, const TimeProfile& profile) {
    addTimeProfile(task_type, profile);
}

TimeProfile* QuantumDynamicTimeManager::getTimeProfile(const std::string& task_type) {
    std::lock_guard<std::mutex> lock(m_profiles_mutex);
    auto it = m_time_profiles.find(task_type);
    if (it == m_time_profiles.end()) {
        return nullptr;
    }
    return &it->second;
}

std::vector<TimeProfile> QuantumDynamicTimeManager::getAllProfiles() const {
    std::lock_guard<std::mutex> lock(m_profiles_mutex);
    std::vector<TimeProfile> profiles;
    profiles.reserve(m_time_profiles.size());
    for (const auto& [_, profile] : m_time_profiles) {
        profiles.push_back(profile);
    }
    return profiles;
}

void QuantumDynamicTimeManager::optimizeGlobalSettings() {
    std::lock_guard<std::mutex> lock(m_metrics_mutex);

    if (m_metrics.total_tasks < 10) {
        return;
    }

    const float success_rate = static_cast<float>(m_metrics.successful_completions) /
        static_cast<float>(std::max(1, m_metrics.total_tasks));

    if (success_rate < 0.70f) {
        m_strategy = AdjustmentStrategy::Conservative;
        m_learning_rate = std::min(0.5f, m_learning_rate * 1.10f);
    } else if (success_rate > 0.90f && m_quantum_enabled) {
        m_strategy = AdjustmentStrategy::Quantum;
        m_learning_rate = std::max(0.01f, m_learning_rate * 0.95f);
    } else {
        m_strategy = AdjustmentStrategy::Adaptive;
    }

    m_metrics.system_efficiency = std::clamp(success_rate, 0.0f, 1.0f);
    m_metrics.adaptation_effectiveness = std::clamp(
        (m_metrics.prediction_accuracy * 0.7f) + (m_metrics.system_efficiency * 0.3f),
        0.0f,
        1.0f);
    m_metrics.adaptation_cycles++;
}

void QuantumDynamicTimeManager::resetLearning() {
    {
        std::lock_guard<std::mutex> profile_lock(m_profiles_mutex);
        m_execution_history.clear();
        m_success_history.clear();

        for (auto& [_, profile] : m_time_profiles) {
            profile.success_rate = 0.8f;
            profile.avg_completion_ratio = 0.7f;
            profile.adaptation_samples = 0;
            profile.last_update = std::chrono::steady_clock::now();
        }
    }

    {
        std::lock_guard<std::mutex> metrics_lock(m_metrics_mutex);
        m_metrics = AdaptationMetrics{};
    }

    m_learning_rate = DEFAULT_LEARNING_RATE;
}

void QuantumDynamicTimeManager::adjustForSystemLoad(float load_factor) {
    m_cached_system_load = std::clamp(load_factor, 0.0f, 1.0f);
}

void QuantumDynamicTimeManager::adjustForMemoryPressure(float memory_factor) {
    m_cached_memory_pressure = std::clamp(memory_factor, 0.0f, 1.0f);
}

void QuantumDynamicTimeManager::adjustForNetworkConditions(float network_factor) {
    m_cached_network_quality = std::clamp(network_factor, 0.1f, 2.0f);
}

TimeAllocation QuantumDynamicTimeManager::extendTimeAllocation(const TimeAllocation& original,
                                                               const std::string& reason) {
    TimeAllocation extended = original;
    float extension_factor = 1.10f;

    if (reason.find("network") != std::string::npos) {
        extension_factor = 1.25f;
    } else if (reason.find("compile") != std::string::npos || reason.find("build") != std::string::npos) {
        extension_factor = 1.35f;
    } else if (reason.find("production") != std::string::npos) {
        extension_factor = 1.40f;
    }

    extended.execution_time = scaleDuration(extended.execution_time, extension_factor);
    extended.buffer_time = scaleDuration(extended.buffer_time, std::max(1.1f, extension_factor - 0.1f));
    extended.total_time = extended.thinking_time + extended.execution_time + extended.buffer_time;
    return extended;
}

void QuantumDynamicTimeManager::enableQuantumOptimization(bool enable) {
    m_quantum_enabled = enable;
    if (!enable && m_strategy == AdjustmentStrategy::Quantum) {
        m_strategy = AdjustmentStrategy::Adaptive;
    }
}

void QuantumDynamicTimeManager::setQuantumParameters(float temporal_uncertainty, float optimization_strength) {
    m_temporal_uncertainty = std::clamp(temporal_uncertainty, 0.0f, 1.0f);
    m_optimization_strength = std::clamp(optimization_strength, 0.1f, 3.0f);
}

float QuantumDynamicTimeManager::calculateQuantumTimeBonus(const ExecutionContext& context) {
    float bonus = 0.0f;
    bonus += context.complexity_score * 0.35f;
    bonus += std::min(0.25f, static_cast<float>(std::max(0, context.agent_count - 1)) * 0.03f);
    bonus += (1.0f - context.recent_success_rate) * 0.2f;
    bonus += (1.0f - m_cached_network_quality) * 0.1f;
    if (context.is_production_critical) {
        bonus += 0.15f;
    }
    return std::clamp(bonus, 0.0f, 1.0f);
}

QuantumDynamicTimeManager::AdaptationMetrics QuantumDynamicTimeManager::getMetrics() const {
    std::lock_guard<std::mutex> lock(m_metrics_mutex);
    return m_metrics;
}

void QuantumDynamicTimeManager::generateTimeReport(std::ostream& output) const {
    const auto metrics = getMetrics();
    output << "Quantum Dynamic Time Manager Report\n";
    output << "Strategy: " << static_cast<int>(m_strategy) << "\n";
    output << "Quantum Enabled: " << (m_quantum_enabled ? "true" : "false") << "\n";
    output << "Total Tasks: " << metrics.total_tasks << "\n";
    output << "Success: " << metrics.successful_completions << "\n";
    output << "Timeouts: " << metrics.timeout_failures << "\n";
    output << "Prediction Accuracy: " << std::fixed << std::setprecision(3) << metrics.prediction_accuracy << "\n";
    output << "System Efficiency: " << std::fixed << std::setprecision(3) << metrics.system_efficiency << "\n";
}

std::vector<std::string> QuantumDynamicTimeManager::getPerformanceRecommendations() const {
    std::vector<std::string> recommendations;
    const auto metrics = getMetrics();

    if (metrics.prediction_accuracy < 0.60f) {
        recommendations.emplace_back("Raise adaptation sample size and tune profile multipliers.");
    }
    if (metrics.timeout_failures > (metrics.successful_completions / 2)) {
        recommendations.emplace_back("Increase timeout bounds for high complexity tasks.");
    }
    if (m_cached_system_load > 0.85f) {
        recommendations.emplace_back("High CPU load detected; scale back concurrent agents.");
    }
    if (m_cached_memory_pressure > 0.80f) {
        recommendations.emplace_back("High memory pressure detected; use conservative scheduling.");
    }
    if (recommendations.empty()) {
        recommendations.emplace_back("System performance is stable.");
    }

    return recommendations;
}

void QuantumDynamicTimeManager::setAdjustmentStrategy(AdjustmentStrategy strategy) {
    m_strategy = strategy;
}

void QuantumDynamicTimeManager::updatePowerShellConfig(const PowerShellConfig& config) {
    std::lock_guard<std::mutex> lock(m_pwsh_mutex);
    m_pwsh_config = config;
    if (m_pwsh_config.min_timeout > m_pwsh_config.max_timeout) {
        std::swap(m_pwsh_config.min_timeout, m_pwsh_config.max_timeout);
    }
    m_pwsh_config.default_timeout = clampDuration(
        m_pwsh_config.default_timeout,
        m_pwsh_config.min_timeout,
        m_pwsh_config.max_timeout);
}

QuantumDynamicTimeManager::PowerShellConfig QuantumDynamicTimeManager::getPowerShellConfig() const {
    std::lock_guard<std::mutex> lock(m_pwsh_mutex);
    return m_pwsh_config;
}

void QuantumDynamicTimeManager::enableMasmAcceleration(bool enable) {
    if (enable == m_masm_enabled) {
        return;
    }
    if (enable) {
        m_masm_enabled = true;
        initializeMasmAcceleration();
    } else {
        shutdownMasmAcceleration();
        m_masm_enabled = false;
    }
}

void QuantumDynamicTimeManager::setLearningRate(float rate) {
    m_learning_rate = std::clamp(rate, 0.001f, 1.0f);
}

void QuantumDynamicTimeManager::configureCheckpoints(const std::vector<std::string>& checkpoint_names) {
    if (checkpoint_names.empty()) {
        return;
    }
    std::lock_guard<std::mutex> lock(g_checkpoint_mutex);
    g_checkpoint_names.clear();
    for (const auto& name : checkpoint_names) {
        if (!name.empty()) {
            g_checkpoint_names.push_back(name);
        }
    }
    if (g_checkpoint_names.empty()) {
        g_checkpoint_names = {"Analysis", "Planning", "Implementation", "Validation"};
    }
}

void QuantumDynamicTimeManager::adaptiveOptimizationLoop() {
    while (m_running.load()) {
        std::this_thread::sleep_for(std::chrono::seconds(5));
        if (!m_running.load()) {
            break;
        }
        adaptProfilesBasedOnHistory();
        optimizeGlobalSettings();
    }
}

void QuantumDynamicTimeManager::performanceMonitoringLoop() {
    while (m_running.load()) {
        updateSystemMetrics();
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}

std::chrono::milliseconds QuantumDynamicTimeManager::calculateBasicTime(const ExecutionContext& context) {
    auto profile = getTimeProfile(context.task_type);
    std::chrono::milliseconds base = std::chrono::minutes(5);
    if (profile != nullptr) {
        base = profile->base_time;
    }
    return applySystemFactors(applyComplexityMultiplier(base, context.complexity_score), context);
}

std::chrono::milliseconds QuantumDynamicTimeManager::applyComplexityMultiplier(std::chrono::milliseconds base_time,
                                                                               float complexity) {
    float multiplier = 1.0f;
    if (complexity <= 0.20f) {
        multiplier = 0.5f;
    } else if (complexity <= 0.50f) {
        multiplier = 1.0f;
    } else if (complexity <= 0.80f) {
        multiplier = 2.0f;
    } else if (complexity <= 0.95f) {
        multiplier = 4.0f;
    } else {
        multiplier = 7.5f;
    }
    return scaleDuration(base_time, multiplier);
}

std::chrono::milliseconds QuantumDynamicTimeManager::applySystemFactors(std::chrono::milliseconds base_time,
                                                                         const ExecutionContext& context) {
    float factor = 1.0f;
    factor += context.current_system_load * SYSTEM_LOAD_IMPACT;
    factor += (1.0f - (context.available_memory_gb / 32.0f)) * MEMORY_PRESSURE_IMPACT;
    factor += (2.0f - context.network_quality) * NETWORK_IMPACT;
    factor += m_cached_system_load * 0.10f;
    factor += m_cached_memory_pressure * 0.10f;
    return scaleDuration(base_time, std::max(0.2f, factor));
}

void QuantumDynamicTimeManager::updateProfileWithFeedback(TimeProfile& profile,
                                                          std::chrono::milliseconds actual_time,
                                                          std::chrono::milliseconds predicted_time,
                                                          bool success) {
    const long long predicted_ms = std::max<long long>(1, predicted_time.count());
    const float ratio = static_cast<float>(actual_time.count()) / static_cast<float>(predicted_ms);
    const float alpha = std::clamp(m_learning_rate, 0.01f, 0.75f);

    profile.avg_completion_ratio = (1.0f - alpha) * profile.avg_completion_ratio + alpha * ratio;
    profile.success_rate = (1.0f - alpha) * profile.success_rate + alpha * (success ? 1.0f : 0.0f);
    profile.adaptation_samples++;
    profile.last_update = std::chrono::steady_clock::now();

    const float base_adjust = 1.0f + ((ratio - 1.0f) * alpha * 0.5f);
    profile.base_time = scaleDuration(profile.base_time, std::clamp(base_adjust, 0.80f, 1.25f));

    std::vector<float> perf_data = {ratio, success ? 1.0f : 0.0f};
    adaptMultipliers(profile, perf_data);
}

float QuantumDynamicTimeManager::calculatePredictionError(
    const std::vector<std::chrono::milliseconds>& predicted,
    const std::vector<std::chrono::milliseconds>& actual) {
    if (predicted.empty() || actual.empty()) {
        return 0.0f;
    }
    const size_t count = std::min(predicted.size(), actual.size());
    float total_error = 0.0f;
    for (size_t i = 0; i < count; ++i) {
        const float p = static_cast<float>(std::max<long long>(1, predicted[i].count()));
        const float a = static_cast<float>(actual[i].count());
        total_error += std::abs(a - p) / p;
    }
    return total_error / static_cast<float>(count);
}

void QuantumDynamicTimeManager::adaptMultipliers(TimeProfile& profile, const std::vector<float>& performance_data) {
    if (performance_data.empty()) {
        return;
    }
    const float avg = std::accumulate(performance_data.begin(), performance_data.end(), 0.0f) /
        static_cast<float>(performance_data.size());

    if (avg > 1.2f) {
        profile.simple_multiplier *= 1.02f;
        profile.moderate_multiplier *= 1.03f;
        profile.complex_multiplier *= 1.04f;
    } else if (avg < 0.8f) {
        profile.simple_multiplier *= 0.99f;
        profile.moderate_multiplier *= 0.98f;
        profile.complex_multiplier *= 0.97f;
    }

    profile.simple_multiplier = std::clamp(profile.simple_multiplier, 0.1f, 2.0f);
    profile.moderate_multiplier = std::clamp(profile.moderate_multiplier, 0.5f, 3.0f);
    profile.complex_multiplier = std::clamp(profile.complex_multiplier, 1.0f, 5.0f);
}

std::chrono::milliseconds QuantumDynamicTimeManager::generateRandomTimeout(std::chrono::milliseconds base_timeout) {
    float random_factor = 1.0f;
    if (m_pwsh_config.use_gaussian_distribution) {
        random_factor = m_gaussian_dist(m_random_generator);
    } else {
        const float range = m_pwsh_config.max_random_factor - m_pwsh_config.min_random_factor;
        random_factor = m_pwsh_config.min_random_factor + (m_uniform_dist(m_random_generator) * range);
    }
    random_factor = std::clamp(
        random_factor,
        m_pwsh_config.min_random_factor,
        m_pwsh_config.max_random_factor);
    return scaleDuration(base_timeout, random_factor);
}

std::chrono::milliseconds QuantumDynamicTimeManager::getCommandSpecificTimeout(const std::string& command) {
    std::lock_guard<std::mutex> lock(m_pwsh_mutex);
    for (const auto& [pattern, timeout] : m_pwsh_config.command_timeouts) {
        if (!pattern.empty() && command.find(pattern) != std::string::npos) {
            return timeout;
        }
    }
    return m_pwsh_config.default_timeout;
}

void QuantumDynamicTimeManager::updatePwshStatistics(const std::string& command,
                                                     std::chrono::milliseconds duration,
                                                     bool success) {
    {
        std::lock_guard<std::mutex> lock(m_metrics_mutex);
        m_metrics.pwsh_executions++;
        if (!success) {
            m_metrics.pwsh_timeouts++;
        }
        const float duration_ms = static_cast<float>(duration.count());
        const int count = std::max(1, m_metrics.pwsh_executions);
        m_metrics.avg_pwsh_duration_ms =
            ((m_metrics.avg_pwsh_duration_ms * static_cast<float>(count - 1)) + duration_ms) /
            static_cast<float>(count);
    }

    std::lock_guard<std::mutex> lock(m_pwsh_mutex);
    m_pwsh_history[command] = duration;
    m_recent_pwsh_results.emplace(command, success);
    while (m_recent_pwsh_results.size() > 256) {
        m_recent_pwsh_results.pop();
    }
}

void QuantumDynamicTimeManager::initializeMasmAcceleration() {
    m_masm_context = nullptr;
}

void QuantumDynamicTimeManager::shutdownMasmAcceleration() {
    m_masm_context = nullptr;
}

std::chrono::milliseconds QuantumDynamicTimeManager::getMasmPrediction(const ExecutionContext& context) {
    auto predicted = calculateBasicTime(context);
    if (m_masm_enabled) {
        predicted = scaleDuration(predicted, 0.90f);
    }
    return predicted;
}

float QuantumDynamicTimeManager::getCurrentSystemLoad() {
    FILETIME idle_time{};
    FILETIME kernel_time{};
    FILETIME user_time{};
    if (!GetSystemTimes(&idle_time, &kernel_time, &user_time)) {
        return m_cached_system_load;
    }

    static std::mutex sample_mutex;
    static uint64_t last_idle = 0;
    static uint64_t last_kernel = 0;
    static uint64_t last_user = 0;
    static bool initialized = false;

    std::lock_guard<std::mutex> lock(sample_mutex);
    const uint64_t idle = fileTimeToUInt64(idle_time);
    const uint64_t kernel = fileTimeToUInt64(kernel_time);
    const uint64_t user = fileTimeToUInt64(user_time);

    if (!initialized) {
        last_idle = idle;
        last_kernel = kernel;
        last_user = user;
        initialized = true;
        return m_cached_system_load;
    }

    const uint64_t idle_delta = idle - last_idle;
    const uint64_t kernel_delta = kernel - last_kernel;
    const uint64_t user_delta = user - last_user;

    last_idle = idle;
    last_kernel = kernel;
    last_user = user;

    const uint64_t total = kernel_delta + user_delta;
    if (total == 0) {
        return m_cached_system_load;
    }

    const float busy = 1.0f - (static_cast<float>(idle_delta) / static_cast<float>(total));
    return std::clamp(busy, 0.0f, 1.0f);
}

float QuantumDynamicTimeManager::getMemoryPressure() {
    MEMORYSTATUSEX memory_status{};
    memory_status.dwLength = sizeof(memory_status);
    if (!GlobalMemoryStatusEx(&memory_status)) {
        return m_cached_memory_pressure;
    }
    return std::clamp(static_cast<float>(memory_status.dwMemoryLoad) / 100.0f, 0.0f, 1.0f);
}

float QuantumDynamicTimeManager::getNetworkQuality() {
    const float contention = (m_cached_system_load + m_cached_memory_pressure) * 0.5f;
    const float quality = 1.2f - contention;
    return std::clamp(quality, 0.1f, 1.0f);
}

void QuantumDynamicTimeManager::updateSystemMetrics() {
    const auto now = std::chrono::steady_clock::now();
    if (now - m_last_system_update < std::chrono::milliseconds(500)) {
        return;
    }
    m_cached_system_load = getCurrentSystemLoad();
    m_cached_memory_pressure = getMemoryPressure();
    m_cached_network_quality = getNetworkQuality();
    m_last_system_update = now;
}

float QuantumDynamicTimeManager::calculateQuantumUncertainty(const ExecutionContext& context) {
    float uncertainty = QUANTUM_UNCERTAINTY_BASE;
    uncertainty += context.complexity_score * 0.20f;
    uncertainty += (1.0f - context.recent_success_rate) * 0.30f;
    uncertainty += m_cached_system_load * 0.10f;
    uncertainty += m_cached_memory_pressure * 0.10f;
    uncertainty += m_temporal_uncertainty * 0.20f;
    return std::clamp(uncertainty, 0.0f, 1.0f);
}

std::chrono::milliseconds QuantumDynamicTimeManager::applyQuantumOptimization(
    std::chrono::milliseconds base_time,
    const ExecutionContext& context) {
    const float uncertainty = calculateQuantumUncertainty(context);
    const float jitter = (m_uniform_dist(m_random_generator) - 0.5f) * uncertainty * 0.4f;
    float factor = 1.0f - (0.15f * m_optimization_strength) + jitter;
    factor = std::clamp(factor, 0.55f, 1.60f);
    return clampDuration(scaleDuration(base_time, factor), std::chrono::seconds(1), std::chrono::hours(24));
}

void QuantumDynamicTimeManager::saveProfilesToDisk() {
    std::map<std::string, TimeProfile> snapshot;
    {
        std::lock_guard<std::mutex> lock(m_profiles_mutex);
        snapshot = m_time_profiles;
    }

    std::ofstream out(PROFILE_DB_FILE, std::ios::trunc);
    if (!out.is_open()) {
        return;
    }

    out << "RAWRXD_QTM|1\n";
    for (const auto& [task, profile] : snapshot) {
        out << sanitizeField(task) << "|"
            << static_cast<int>(profile.category) << "|"
            << profile.base_time.count() << "|"
            << profile.min_time.count() << "|"
            << profile.max_time.count() << "|"
            << profile.simple_multiplier << "|"
            << profile.moderate_multiplier << "|"
            << profile.complex_multiplier << "|"
            << profile.expert_multiplier << "|"
            << profile.quantum_multiplier << "|"
            << profile.success_rate << "|"
            << profile.avg_completion_ratio << "|"
            << profile.adaptation_samples << "\n";
    }
}

void QuantumDynamicTimeManager::loadProfilesFromDisk() {
    std::ifstream in(PROFILE_DB_FILE);
    if (!in.is_open()) {
        return;
    }

    std::string header;
    if (!std::getline(in, header)) {
        return;
    }
    const auto header_parts = splitLine(header, '|');
    if (header_parts.size() != 2 || header_parts[0] != "RAWRXD_QTM") {
        return;
    }

    std::map<std::string, TimeProfile> loaded;
    std::string line;
    while (std::getline(in, line)) {
        const auto parts = splitLine(line, '|');
        if (parts.size() < 13) {
            continue;
        }
        try {
            TimeProfile profile;
            profile.task_type = parts[0];
            profile.category = static_cast<TimeCategory>(std::stoi(parts[1]));
            profile.base_time = std::chrono::milliseconds(std::stoll(parts[2]));
            profile.min_time = std::chrono::milliseconds(std::stoll(parts[3]));
            profile.max_time = std::chrono::milliseconds(std::stoll(parts[4]));
            profile.simple_multiplier = std::stof(parts[5]);
            profile.moderate_multiplier = std::stof(parts[6]);
            profile.complex_multiplier = std::stof(parts[7]);
            profile.expert_multiplier = std::stof(parts[8]);
            profile.quantum_multiplier = std::stof(parts[9]);
            profile.success_rate = std::stof(parts[10]);
            profile.avg_completion_ratio = std::stof(parts[11]);
            profile.adaptation_samples = std::stoi(parts[12]);
            profile.last_update = std::chrono::steady_clock::now();
            loaded[profile.task_type] = profile;
        } catch (...) {
            continue;
        }
    }

    if (!loaded.empty()) {
        std::lock_guard<std::mutex> lock(m_profiles_mutex);
        for (const auto& [task, profile] : loaded) {
            m_time_profiles[task] = profile;
        }
    }
}

void QuantumDynamicTimeManager::cleanupOldData() {
    const size_t max_history = static_cast<size_t>(std::max(MIN_HISTORY_SAMPLES, m_adaptation_window_size));
    while (m_execution_history.size() > max_history) {
        m_execution_history.erase(m_execution_history.begin());
    }
    while (m_success_history.size() > max_history) {
        m_success_history.erase(m_success_history.begin());
    }
}

PowerShellTimeoutManager::PowerShellTimeoutManager(QuantumDynamicTimeManager* time_manager)
    : m_time_manager(time_manager)
    , m_randomization_enabled(true)
    , m_min_random_factor(0.9f)
    , m_max_random_factor(1.2f) {}

PowerShellTimeoutManager::~PowerShellTimeoutManager() {
    std::lock_guard<std::mutex> lock(m_sessions_mutex);
    m_sessions.clear();
}

std::string PowerShellTimeoutManager::createSession(std::chrono::milliseconds timeout) {
    timeout = std::max(timeout, std::chrono::milliseconds(1000));

    std::lock_guard<std::mutex> lock(m_sessions_mutex);
    if (m_sessions.size() >= 20) {
        m_sessions.erase(m_sessions.begin());
    }

    const std::string id = "pwsh_session_" + std::to_string(g_session_counter.fetch_add(1) + 1);
    TerminalSession session(id);
    session.allocated_time = timeout;
    session.remaining_time = timeout;
    m_sessions.emplace(id, session);
    return id;
}

void PowerShellTimeoutManager::destroySession(const std::string& session_id) {
    std::lock_guard<std::mutex> lock(m_sessions_mutex);
    m_sessions.erase(session_id);
}

bool PowerShellTimeoutManager::extendSession(const std::string& session_id,
                                             std::chrono::milliseconds additional_time) {
    if (additional_time <= std::chrono::milliseconds::zero()) {
        return false;
    }

    std::lock_guard<std::mutex> lock(m_sessions_mutex);
    auto it = m_sessions.find(session_id);
    if (it == m_sessions.end()) {
        return false;
    }

    it->second.allocated_time += additional_time;
    it->second.remaining_time += additional_time;
    it->second.extension_count++;
    return true;
}

std::string PowerShellTimeoutManager::executeWithTimeout(const std::string& command,
                                                         std::chrono::milliseconds timeout) {
    if (command.empty()) {
        return "[PowerShellTimeoutManager] Empty command.";
    }

    if (timeout <= std::chrono::milliseconds::zero()) {
        timeout = m_time_manager != nullptr
            ? m_time_manager->getRandomPwshTimeout("powershell")
            : std::chrono::seconds(30);
    }

    if (m_randomization_enabled) {
        std::random_device rd;
        std::mt19937 rng(rd());
        std::uniform_real_distribution<float> dist(m_min_random_factor, m_max_random_factor);
        timeout = scaleDuration(timeout, dist(rng));
    }

    const std::string session_id = createSession(timeout);

    SECURITY_ATTRIBUTES sa{};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;

    HANDLE read_pipe = nullptr;
    HANDLE write_pipe = nullptr;
    if (!CreatePipe(&read_pipe, &write_pipe, &sa, 0)) {
        destroySession(session_id);
        return "[PowerShellTimeoutManager] Failed to create pipe.";
    }

    SetHandleInformation(read_pipe, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOA si{};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdOutput = write_pipe;
    si.hStdError = write_pipe;
    si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);

    PROCESS_INFORMATION pi{};
    std::string cmdline = "powershell.exe -NoProfile -ExecutionPolicy Bypass -Command \"" +
        escapePowershellCommand(command) + "\"";
    std::vector<char> mutable_cmdline(cmdline.begin(), cmdline.end());
    mutable_cmdline.push_back('\0');

    const BOOL created = CreateProcessA(
        nullptr,
        mutable_cmdline.data(),
        nullptr,
        nullptr,
        TRUE,
        CREATE_NO_WINDOW,
        nullptr,
        nullptr,
        &si,
        &pi);

    CloseHandle(write_pipe);

    if (!created) {
        CloseHandle(read_pipe);
        destroySession(session_id);
        return "[PowerShellTimeoutManager] Failed to launch powershell.exe.";
    }

    const DWORD wait_result = WaitForSingleObject(pi.hProcess, static_cast<DWORD>(timeout.count()));
    if (wait_result == WAIT_TIMEOUT) {
        TerminateProcess(pi.hProcess, 124);
    }

    std::string output;
    char buffer[4096];
    DWORD bytes_read = 0;
    while (ReadFile(read_pipe, buffer, sizeof(buffer), &bytes_read, nullptr) && bytes_read > 0) {
        output.append(buffer, static_cast<size_t>(bytes_read));
    }

    DWORD exit_code = 0;
    GetExitCodeProcess(pi.hProcess, &exit_code);

    CloseHandle(read_pipe);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    destroySession(session_id);

    if (wait_result == WAIT_TIMEOUT) {
        return "[PowerShellTimeoutManager] Timeout after " + std::to_string(timeout.count()) + "ms.\n" + output;
    }

    if (output.empty()) {
        output = "[PowerShellTimeoutManager] Command completed with exit code " + std::to_string(exit_code) + ".";
    }
    return output;
}

bool PowerShellTimeoutManager::isSessionActive(const std::string& session_id) const {
    std::lock_guard<std::mutex> lock(m_sessions_mutex);
    const auto it = m_sessions.find(session_id);
    if (it == m_sessions.end()) {
        return false;
    }
    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - it->second.start_time);
    return elapsed < it->second.allocated_time;
}

std::chrono::milliseconds PowerShellTimeoutManager::getRemainingTime(const std::string& session_id) const {
    std::lock_guard<std::mutex> lock(m_sessions_mutex);
    const auto it = m_sessions.find(session_id);
    if (it == m_sessions.end()) {
        return std::chrono::milliseconds::zero();
    }
    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - it->second.start_time);
    if (elapsed >= it->second.allocated_time) {
        return std::chrono::milliseconds::zero();
    }
    return it->second.allocated_time - elapsed;
}

void PowerShellTimeoutManager::setRandomizationRange(float min_factor, float max_factor) {
    if (min_factor > max_factor) {
        std::swap(min_factor, max_factor);
    }
    m_min_random_factor = std::max(0.1f, min_factor);
    m_max_random_factor = std::max(m_min_random_factor, max_factor);
}

}  // namespace RawrXD::Agent
