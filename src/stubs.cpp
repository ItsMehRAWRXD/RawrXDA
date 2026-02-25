#include "engine/inference_kernels.h"
#include "swarm_orchestrator.h"
#include "agent/agentic_deep_thinking_engine.hpp"
#include "agent/quantum_autonomous_todo_system.hpp"
#include "agent/quantum_multi_model_agent_cycling.hpp"
#include <windows.h>
#include <debugapi.h>
#include <iostream>
#include <vector>
#include <string>
#include <algorithm>
#include <chrono>
#include <sstream>
#include <cmath>
#include <cstring>

#if !defined(RAWRXD_GOLD_BUILD) && !defined(RAWR_QUICKJS_STUB)
#include "headers/inference_engine.h"
#endif

// Removed register_rawr_inference (defined in tool_registry_init.cpp)
// register_sovereign_engines — linker fallback for targets without engine module.
// Real implementation in engine/sovereign_engines.cpp registers Engine800B + SovereignSmall.
// Also defined in tool_registry_init.cpp for registry-aware targets.
// This definition is for minimal/test targets that don't link sovereign_engines or tool_registry_init.
#if !defined(RAWRXD_GOLD_BUILD) && !defined(RAWR_HAS_TOOL_REGISTRY_INIT) && !defined(RAWR_HAS_SOVEREIGN_ENGINES)
void register_sovereign_engines() {
    OutputDebugStringA("[FALLBACK] register_sovereign_engines — engine module not linked");
}
#endif

// Diagnostics::error — real implementation now in src/utils/Diagnostics.cpp
// Stub removed to avoid multiple definition linker errors (LNK2005 / ld)

// InferenceKernels: real SIMD implementations now in inference_kernels.cpp
// Stubs removed to avoid multiple definition linker errors

#if defined(RAWR_QUICKJS_STUB)
// Some Win32/Gold variants don't link the full ultra_fast_inference TU.
// Provide bounded fallbacks only for that profile.
#include "inference/ultra_fast_inference.h"
namespace rawrxd::inference {

TensorPruningScorer::TensorPruningScorer(const PruningConfig& config)
    : config_(config) {}
TensorPruningScorer::~TensorPruningScorer() = default;

StreamingTensorReducer::StreamingTensorReducer(const ReductionConfig& config)
    : config_(config) {
    stats_ = {};
}
StreamingTensorReducer::~StreamingTensorReducer() = default;

ModelHotpatcher::ModelHotpatcher(const HotpatchConfig& config)
    : config_(config), current_tier_(TIER_70B) {}
ModelHotpatcher::~ModelHotpatcher() {
    if (prefetch_thread_.joinable()) {
        prefetch_thread_.join();
    }
}

AutonomousInferenceEngine::AutonomousInferenceEngine(
    const AutonomousInferenceEngine::InferenceConfig& config)
    : config_(config) {
    stats_ = {};
}

AutonomousInferenceEngine::~AutonomousInferenceEngine() {
    running_.store(false);
    if (inference_thread_.joinable()) {
        inference_thread_.join();
    }
}

bool AutonomousInferenceEngine::loadModelAutomatic(const std::string& model_path) {
    config_.model_path = model_path;
    return !model_path.empty();
}

void AutonomousInferenceEngine::infer(const std::vector<int32_t>& /*prompt*/,
                                      std::function<void(const std::string&)> token_callback,
                                      size_t /*max_tokens*/) {
    if (token_callback) token_callback("");
}

} // namespace rawrxd::inference
#endif

namespace RawrXD {

SwarmOrchestrator::SwarmOrchestrator(size_t numWorkers) {
    if (numWorkers == 0) {
        numWorkers = std::max<size_t>(1, std::thread::hardware_concurrency());
    }
    m_queues.reserve(numWorkers);
    for (size_t i = 0; i < numWorkers; ++i) {
        m_queues.push_back(std::make_unique<WorkerQueue>());
    }
    m_running.store(true);
    m_threads.reserve(numWorkers);
    for (size_t i = 0; i < numWorkers; ++i) {
        m_threads.emplace_back(&SwarmOrchestrator::loop, this, static_cast<int>(i));
    }
}

SwarmOrchestrator::~SwarmOrchestrator() {
    shutdown();
}

void SwarmOrchestrator::shutdown() {
    m_running.store(false);
    for (auto& t : m_threads) {
        if (t.joinable()) {
            t.join();
        }
    }
}

std::future<SwarmResult> SwarmOrchestrator::submitTaskAsync(const std::string& taskDesc,
                                                            const std::string& context) {
    auto task = std::make_unique<OrchestratorTask>();
    task->description = taskDesc;
    task->context = context;
    task->priority = 0;
    auto fut = task->promise.get_future();

    static std::atomic<size_t> rr{0};
    if (!m_queues.empty()) {
        const size_t idx = rr.fetch_add(1) % m_queues.size();
        std::lock_guard<std::mutex> lock(m_queues[idx]->mutex);
        m_queues[idx]->tasks.push_back(std::move(task));
    } else {
        SwarmResult res{};
        res.consensus = "No queues available";
        res.confidence = 0.0f;
        res.executionTimeMs = 0.0;
        task->promise.set_value(res);
    }
    return fut;
}

#if defined(__cpp_lib_expected) || (defined(_MSVC_LANG) && _MSVC_LANG >= 202302L)
std::expected<SwarmResult, int> SwarmOrchestrator::executeTask(const std::string& task,
                                                               const std::string& context) {
    auto fut = submitTaskAsync(task, context);
    if (fut.wait_for(std::chrono::seconds(3)) == std::future_status::timeout) {
        return std::unexpected(408);
    }
    return fut.get();
}
#else
RawrXD::Expected<SwarmResult, int> SwarmOrchestrator::executeTask(const std::string& task,
                                                                  const std::string& context) {
    auto fut = submitTaskAsync(task, context);
    if (fut.wait_for(std::chrono::seconds(3)) == std::future_status::timeout) {
        return RawrXD::unexpected<int>(408);
    }
    return fut.get();
}
#endif

void SwarmOrchestrator::loop(int workerId) {
    while (m_running.load()) {
        std::unique_ptr<OrchestratorTask> task;
        if (workerId >= 0 && static_cast<size_t>(workerId) < m_queues.size()) {
            auto& q = *m_queues[static_cast<size_t>(workerId)];
            std::lock_guard<std::mutex> lock(q.mutex);
            if (!q.tasks.empty()) {
                task = std::move(q.tasks.front());
                q.tasks.pop_front();
            }
        }

        if (!task) {
            OrchestratorTask stolen;
            if (stealWork(workerId, stolen)) {
                SwarmResult r{};
                r.consensus = "stolen:" + stolen.description;
                r.confidence = 0.6f;
                r.executionTimeMs = 1.0;
                r.individualThoughts = {stolen.context};
                try { stolen.promise.set_value(r); } catch (...) {}
                m_totalTasksExecuted.fetch_add(1);
                continue;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
            continue;
        }

        const auto t0 = std::chrono::steady_clock::now();
        SwarmResult r{};
        r.consensus = task->description;
        r.individualThoughts = {task->context};
        r.confidence = std::min(1.0f, 0.55f + 0.05f * static_cast<float>(task->priority));
        const auto t1 = std::chrono::steady_clock::now();
        r.executionTimeMs = std::chrono::duration<double, std::milli>(t1 - t0).count();
        try { task->promise.set_value(r); } catch (...) {}
        m_totalTasksExecuted.fetch_add(1);
    }
}

bool SwarmOrchestrator::stealWork(int thiefId, OrchestratorTask& stolenTask) {
    const size_t self = thiefId < 0 ? static_cast<size_t>(-1) : static_cast<size_t>(thiefId);
    for (size_t i = 0; i < m_queues.size(); ++i) {
        if (i == self) {
            continue;
        }
        auto& q = *m_queues[i];
        if (!q.mutex.try_lock()) {
            continue;
        }
        if (!q.tasks.empty()) {
            auto ptr = std::move(q.tasks.back());
            q.tasks.pop_back();
            q.mutex.unlock();
            if (ptr) {
                stolenTask = std::move(*ptr);
                return true;
            }
            return false;
        }
        q.mutex.unlock();
    }
    return false;
}

SwarmResult SwarmOrchestrator::synthesizeConsensus(const std::vector<std::string>& results) {
    SwarmResult r{};
    if (results.empty()) {
        r.consensus = "No results";
        r.confidence = 0.0f;
        return r;
    }
    r.consensus = results.front();
    r.individualThoughts = results;
    r.confidence = std::min(1.0f, 0.55f + 0.05f * static_cast<float>(results.size()));
    return r;
}

nlohmann::json SwarmOrchestrator::getStatus() const {
    return {
        {"running", m_running.load()},
        {"queue_count", m_queues.size()},
        {"total_tasks_executed", m_totalTasksExecuted.load()}
    };
}

} // namespace RawrXD

#if !defined(RAWRXD_GOLD_BUILD)
namespace RawrXD::Agent {

QuantumAutonomousTodoSystem::QuantumAutonomousTodoSystem(const AutonomousConfig& config)
    : m_config(config) {
    m_running.store(false);
}

QuantumAutonomousTodoSystem::~QuantumAutonomousTodoSystem() {
    m_running.store(false);
}

std::vector<QuantumAutonomousTodoSystem::TaskDefinition>
QuantumAutonomousTodoSystem::generateTodos(const std::string& from_request) {
    TaskDefinition task;
    task.id = "qtodo_" + std::to_string(std::hash<std::string>{}(from_request));
    task.title = from_request.empty() ? "autonomous-generated-task" : from_request.substr(0, std::min<size_t>(64, from_request.size()));
    task.description = from_request;
    task.priority_score = from_request.empty() ? 0.4f : 0.8f;
    task.difficulty_score = std::min(1.0f, static_cast<float>(from_request.size()) / 200.0f);
    task.estimated_time_ms = static_cast<int>(60000 + task.difficulty_score * 240000);
    task.requires_multi_agent = task.difficulty_score > 0.7f;
    task.min_agent_count = task.requires_multi_agent ? 2 : 1;
    task.max_agent_count = task.requires_multi_agent ? 4 : 1;
    return {task};
}

QuantumMultiModelAgentCycling::QuantumMultiModelAgentCycling(const CyclingConfig& config)
    : m_config(config) {
    m_running.store(false);
}

QuantumMultiModelAgentCycling::~QuantumMultiModelAgentCycling() {
    m_running.store(false);
}

bool QuantumMultiModelAgentCycling::initializeAgents() {
    m_running.store(true);
    std::lock_guard<std::mutex> lock(m_metrics_mutex);
    m_metrics.active_agents = std::max(1, m_config.default_agents);
    m_metrics.idle_agents = m_metrics.active_agents;
    return true;
}

} // namespace RawrXD::Agent
#endif

#if defined(RAWR_QUICKJS_STUB) && !defined(RAWRXD_GOLD_BUILD)
// Win32/GOLD quickjs-stub profile may not link full deep-thinking engine TU.
AgenticDeepThinkingEngine::~AgenticDeepThinkingEngine() = default;
#endif

extern "C" {

static float rawrxd_half_to_float(uint16_t h) {
    const uint32_t sign = (h & 0x8000u) << 16;
    uint32_t exp = (h & 0x7C00u) >> 10;
    uint32_t frac = (h & 0x03FFu);

    uint32_t out_bits = 0;
    if (exp == 0) {
        if (frac == 0) {
            out_bits = sign;
        } else {
            // Normalize subnormal.
            exp = 1;
            while ((frac & 0x0400u) == 0) {
                frac <<= 1;
                --exp;
            }
            frac &= 0x03FFu;
            out_bits = sign | ((exp + (127 - 15)) << 23) | (frac << 13);
        }
    } else if (exp == 0x1F) {
        out_bits = sign | 0x7F800000u | (frac << 13);
    } else {
        out_bits = sign | ((exp + (127 - 15)) << 23) | (frac << 13);
    }

    float out = 0.0f;
    std::memcpy(&out, &out_bits, sizeof(out));
    return out;
}

int64_t asm_dml_rope_rotate_fp32(float* qk, const float* cosTable,
                                 const float* sinTable, uint32_t halfDim,
                                 uint32_t seqLen) {
    if (!qk || !cosTable || !sinTable || halfDim == 0 || seqLen == 0) {
        return -1;
    }

    // qk layout: [seq][2*halfDim] where first half is x_even, second half is x_odd.
    for (uint32_t s = 0; s < seqLen; ++s) {
        float* row = qk + static_cast<size_t>(s) * static_cast<size_t>(halfDim) * 2;
        const float* c = cosTable + static_cast<size_t>(s) * halfDim;
        const float* sn = sinTable + static_cast<size_t>(s) * halfDim;
        for (uint32_t i = 0; i < halfDim; ++i) {
            const float x0 = row[i];
            const float x1 = row[halfDim + i];
            row[i] = x0 * c[i] - x1 * sn[i];
            row[halfDim + i] = x0 * sn[i] + x1 * c[i];
        }
    }
    return 0;
}

int64_t asm_dml_dequant_q4_0_to_fp32(float* dest, const uint8_t* src,
                                     uint64_t blockCount) {
    if (!dest || !src) {
        return -1;
    }
    // GGML q4_0 block: fp16 scale + 16 bytes packed 4-bit signed values (32 elems).
    for (uint64_t b = 0; b < blockCount; ++b) {
        const uint8_t* blk = src + b * 18;
        const uint16_t d16 = static_cast<uint16_t>(blk[0] | (static_cast<uint16_t>(blk[1]) << 8));
        const float d = rawrxd_half_to_float(d16);
        const uint8_t* qs = blk + 2;
        float* out = dest + b * 32;
        for (int i = 0; i < 16; ++i) {
            const uint8_t q = qs[i];
            const int lo = (q & 0x0F) - 8;
            const int hi = ((q >> 4) & 0x0F) - 8;
            out[i * 2] = static_cast<float>(lo) * d;
            out[i * 2 + 1] = static_cast<float>(hi) * d;
        }
    }
    return 0;
}

int64_t asm_dml_dequant_q8_0_to_fp32(float* dest, const uint8_t* src,
                                     uint64_t blockCount) {
    if (!dest || !src) {
        return -1;
    }
    // GGML q8_0 block: fp16 scale + 32 int8 values.
    for (uint64_t b = 0; b < blockCount; ++b) {
        const uint8_t* blk = src + b * 34;
        const uint16_t d16 = static_cast<uint16_t>(blk[0] | (static_cast<uint16_t>(blk[1]) << 8));
        const float d = rawrxd_half_to_float(d16);
        const int8_t* qs = reinterpret_cast<const int8_t*>(blk + 2);
        float* out = dest + b * 32;
        for (int i = 0; i < 32; ++i) {
            out[i] = static_cast<float>(qs[i]) * d;
        }
    }
    return 0;
}

int64_t asm_dml_prefetch_tensor_block(const void* address, uint64_t byteCount) {
    if (!address || byteCount == 0) {
        return -1;
    }
    // Portable prefetch fallback: stride-touch cache lines.
    const uint8_t* p = reinterpret_cast<const uint8_t*>(address);
    volatile uint8_t sink = 0;
    constexpr uint64_t stride = 64;
    for (uint64_t i = 0; i < byteCount; i += stride) {
        sink ^= p[i];
    }
    (void)sink;
    return 0;
}

}
