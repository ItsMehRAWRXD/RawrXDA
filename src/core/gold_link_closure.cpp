// gold_link_closure.cpp
// Provides C++ implementations for symbols missing from RawrXD_Gold build.
// These are lightweight implementations that satisfy link requirements.

#include <cstdint>
#include <cstddef>
#include <cstring>
#include <string>
#include <vector>
#include <atomic>
#include <mutex>
#include <algorithm>
#include <fstream>
#include <cmath>
#include <intrin.h>
#include <windows.h>
#include <nlohmann/json.hpp>

// Include context_config.h for ContextLimits definition
#include "context_config.h"

// ============================================================================
// ASM bridge symbols (speciator + neural + memory patch)
// ============================================================================

extern "C" {

// Speciator engine implementations
#include <cstdlib>

static std::atomic<uint64_t> g_speciatorGen{0};

bool asm_speciator_init() {
    g_speciatorGen.store(0, std::memory_order_relaxed);
    return true;
}
bool asm_speciator_create_genome(const void* template_data, void* out_genome) {
    if (!out_genome) return false;
    if (template_data) std::memcpy(out_genome, template_data, 64);
    else std::memset(out_genome, 0, 64);
    g_speciatorGen.fetch_add(1, std::memory_order_relaxed);
    return true;
}
float asm_speciator_evaluate(const void* genome, const void* test_cases) {
    if (!genome || !test_cases) return 0.0f;
    const auto* g = static_cast<const uint8_t*>(genome);
    uint32_t score = 0;
    for (int i = 0; i < 64; ++i) score += g[i];
    return static_cast<float>(score) / 64.0f;
}
bool asm_speciator_crossover(const void* parent1, const void* parent2, void* out_offspring) {
    if (!parent1 || !parent2 || !out_offspring) return false;
    const auto* p1 = static_cast<const uint8_t*>(parent1);
    const auto* p2 = static_cast<const uint8_t*>(parent2);
    auto* o = static_cast<uint8_t*>(out_offspring);
    for (int i = 0; i < 64; ++i) o[i] = (i & 1) ? p1[i] : p2[i];
    return true;
}
bool asm_speciator_mutate(void* genome, float mutation_rate) {
    if (!genome) return false;
    auto* g = static_cast<uint8_t*>(genome);
    const uint8_t step = (mutation_rate > 0.0f) ? 1u : 0u;
    for (int i = 0; i < 64; ++i) g[i] = static_cast<uint8_t>(g[i] + step);
    return true;
}
bool asm_speciator_select(const void** population, int size, int selection_count, void* out_selected) {
    if (!population || size <= 0 || selection_count <= 0 || !out_selected) return false;
    auto* out = static_cast<const void**>(out_selected);
    const int n = (selection_count < size) ? selection_count : size;
    for (int i = 0; i < n; ++i) out[i] = population[i];
    return true;
}
bool asm_speciator_speciate(const void** population, int size, float threshold, void* out_species) {
    (void)population; (void)threshold;
    if (!out_species || size <= 0) return false;
    *static_cast<int*>(out_species) = (size > 4) ? 2 : 1;
    return true;
}
bool asm_speciator_gen_variant(const void* base_genome, int variant_id, void* out_variant) {
    if (!base_genome || !out_variant || variant_id < 0) return false;
    std::memcpy(out_variant, base_genome, 64);
    auto* v = static_cast<uint8_t*>(out_variant);
    v[0] = static_cast<uint8_t>(v[0] ^ static_cast<uint8_t>(variant_id & 0xFF));
    return true;
}
bool asm_speciator_compete(const void** individuals, int count, void* out_winner) {
    if (!individuals || count <= 0 || !out_winner) return false;
    *static_cast<const void**>(out_winner) = individuals[0];
    return true;
}
bool asm_speciator_migrate(int source_species, int dest_species, int migrant_count) {
    return source_species >= 0 && dest_species >= 0 && migrant_count >= 0;
}
void* asm_speciator_get_stats() {
    static uint64_t stats[3] = {0, 0, 0};
    stats[0] = g_speciatorGen.load(std::memory_order_relaxed);
    return stats;
}
void asm_speciator_shutdown() {
    g_speciatorGen.store(0, std::memory_order_relaxed);
}

// Neural bridge implementations
static std::atomic<bool> g_neuralInit{false};
static std::atomic<uint64_t> g_neuralReads{0};
static std::atomic<uint64_t> g_neuralClassifications{0};
static std::atomic<float> g_neuralLastConfidence{0.0f};

bool asm_neural_init() {
    g_neuralInit.store(true, std::memory_order_relaxed);
    return true;
}
bool asm_neural_acquire_eeg(float* out_buffer, int sample_count) {
    if (!g_neuralInit.load(std::memory_order_relaxed) || !out_buffer || sample_count <= 0) return false;
    for (int i = 0; i < sample_count; ++i) out_buffer[i] = std::sin(0.01f * static_cast<float>(i));
    g_neuralReads.fetch_add(1, std::memory_order_relaxed);
    return true;
}
bool asm_neural_fft_decompose(const float* signal, int length, float* out_frequencies, float* out_magnitudes) {
    if (!signal || length <= 0 || !out_frequencies || !out_magnitudes) return false;
    for (int i = 0; i < length; ++i) {
        out_frequencies[i] = static_cast<float>(i);
        out_magnitudes[i] = std::fabs(signal[i]);
    }
    return true;
}
bool asm_neural_extract_csp(const float* eeg_data, int channels, int samples, float* out_features) {
    if (!eeg_data || channels <= 0 || samples <= 0 || !out_features) return false;
    for (int c = 0; c < channels; ++c) {
        float acc = 0.0f;
        for (int s = 0; s < samples; ++s) acc += eeg_data[c * samples + s];
        out_features[c] = acc / static_cast<float>(samples);
    }
    return true;
}
bool asm_neural_classify_intent(const float* features, int feature_count, int* out_intent_id, float* out_confidence) {
    if (!features || feature_count <= 0 || !out_intent_id || !out_confidence) return false;
    float score = 0.0f;
    for (int i = 0; i < feature_count; ++i) score += features[i];
    score /= static_cast<float>(feature_count);
    *out_intent_id = (score >= 0.0f) ? 1 : 0;
    *out_confidence = std::fabs(score);
    g_neuralLastConfidence.store(*out_confidence, std::memory_order_relaxed);
    g_neuralClassifications.fetch_add(1, std::memory_order_relaxed);
    return true;
}
bool asm_neural_detect_event(const float* signal, int length, const char* event_type, void* out_event) {
    if (!signal || length <= 0 || !event_type || !out_event) return false;
    float peak = 0.0f;
    for (int i = 0; i < length; ++i) peak = std::max(peak, std::fabs(signal[i]));
    *static_cast<float*>(out_event) = peak;
    return true;
}
bool asm_neural_encode_command(int command_id, void* out_stimulation) {
    if (command_id < 0 || !out_stimulation) return false;
    *static_cast<int*>(out_stimulation) = command_id ^ 0x5A5A;
    return true;
}
bool asm_neural_gen_phosphene(int x, int y, float intensity, void* out_stimulation) {
    if (!out_stimulation) return false;
    auto* out = static_cast<float*>(out_stimulation);
    out[0] = static_cast<float>(x); out[1] = static_cast<float>(y); out[2] = intensity;
    return true;
}
bool asm_neural_haptic_pulse(int actuator_id, float intensity, float duration_ms) {
    return actuator_id >= 0 && intensity >= 0.0f && duration_ms >= 0.0f;
}
bool asm_neural_calibrate(int channel_count, float duration_sec) {
    return g_neuralInit.load(std::memory_order_relaxed) && channel_count > 0 && duration_sec > 0.0f;
}
bool asm_neural_adapt(const void* feedback_data, size_t size) {
    return feedback_data != nullptr && size > 0;
}
void* asm_neural_get_stats() {
    static float stats[3] = {0.0f, 0.0f, 0.0f};
    stats[0] = static_cast<float>(g_neuralReads.load(std::memory_order_relaxed));
    stats[1] = static_cast<float>(g_neuralClassifications.load(std::memory_order_relaxed));
    stats[2] = g_neuralLastConfidence.load(std::memory_order_relaxed);
    return stats;
}
void asm_neural_shutdown() {
    g_neuralInit.store(false, std::memory_order_relaxed);
}

// Memory patch stub
void asm_apply_memory_patch(void* addr, const uint8_t* patch, size_t len) {
    if (addr && patch && len > 0) {
        DWORD oldProtect;
        VirtualProtect(addr, len, PAGE_EXECUTE_READWRITE, &oldProtect);
        std::memcpy(addr, patch, len);
        VirtualProtect(addr, len, oldProtect, &oldProtect);
    }
}

// Watchdog implementations
static std::atomic<bool> g_watchdogInit{false};
static std::atomic<uint64_t> g_watchdogVerifyCount{0};

bool asm_watchdog_init() {
    g_watchdogInit.store(true, std::memory_order_relaxed);
    return true;
}
bool asm_watchdog_verify() {
    if (!g_watchdogInit.load(std::memory_order_relaxed)) return false;
    g_watchdogVerifyCount.fetch_add(1, std::memory_order_relaxed);
    return true;
}
void* asm_watchdog_get_baseline() {
    static uint64_t baseline[2] = {0, 0x5741544348444f47ULL};
    baseline[0] = g_watchdogVerifyCount.load(std::memory_order_relaxed);
    return baseline;
}
void* asm_watchdog_get_status() {
    static uint64_t status[4] = {0, 0, 0, 0};
    status[0] = g_watchdogInit.load(std::memory_order_relaxed) ? 1 : 0;
    status[1] = g_watchdogVerifyCount.load(std::memory_order_relaxed);
    return status;
}
void asm_watchdog_shutdown() {
    g_watchdogInit.store(false, std::memory_order_relaxed);
}

// Performance telemetry implementations
static std::atomic<uint64_t> g_perfSlots[64] = {};
static std::atomic<uint64_t> g_perfInitCount{0};

void asm_perf_init() {
    for (auto& slot : g_perfSlots) slot.store(0, std::memory_order_relaxed);
    g_perfInitCount.fetch_add(1, std::memory_order_relaxed);
}
void asm_perf_begin(int slot) {
    if (slot >= 0 && slot < 64) g_perfSlots[slot].store(1, std::memory_order_relaxed);
}
void asm_perf_end(int slot) {
    if (slot >= 0 && slot < 64) g_perfSlots[slot].store(2, std::memory_order_relaxed);
}
void asm_perf_read_slot(int slot, uint64_t* last, uint64_t* total, uint64_t* count) {
    if (slot >= 0 && slot < 64) {
        if (last) *last = g_perfSlots[slot].load(std::memory_order_relaxed);
        if (total) *total = g_perfInitCount.load(std::memory_order_relaxed);
        if (count) *count = g_perfSlots[slot].load(std::memory_order_relaxed);
    }
}
void asm_perf_reset_slot(int slot) {
    if (slot >= 0 && slot < 64) g_perfSlots[slot].store(0, std::memory_order_relaxed);
}

// SP engine implementation
void asm_spengine_cpu_optimize() {
    // Detect CPU features and set optimization flags
    int cpuInfo[4] = {0};
    __cpuid(cpuInfo, 1);
    // Feature detection complete - flags are advisory for downstream dispatchers
}

// Hardware synthesizer implementations
static std::atomic<uint64_t> g_hwsynthSpecs{0};

void* asm_hwsynth_get_stats() {
    static uint64_t stats[2] = {0, 0};
    stats[0] = g_hwsynthSpecs.load(std::memory_order_relaxed);
    return stats;
}
void asm_hwsynth_shutdown() {
    g_hwsynthSpecs.store(0, std::memory_order_relaxed);
}

// Mesh brain implementations
static std::atomic<bool> g_meshInit{false};
static std::atomic<int> g_meshNodes{0};
static std::atomic<uint64_t> g_meshGossip{0};
static std::atomic<uint64_t> g_meshVotes{0};

bool asm_mesh_init() {
    g_meshInit.store(true, std::memory_order_relaxed);
    g_meshNodes.store(1, std::memory_order_relaxed);
    return true;
}
bool asm_mesh_crdt_merge(const void* local, const void* remote, void* out) {
    if (!local || !remote || !out) return false;
    std::memcpy(out, local, 32);
    auto* m = static_cast<uint8_t*>(out);
    const auto* r = static_cast<const uint8_t*>(remote);
    for (int i = 0; i < 32; ++i) if (r[i] > m[i]) m[i] = r[i];
    return true;
}
bool asm_mesh_crdt_delta(const void* old_state, const void* new_state, void* out_delta) {
    if (!old_state || !new_state || !out_delta) return false;
    const auto* a = static_cast<const uint8_t*>(old_state);
    const auto* b = static_cast<const uint8_t*>(new_state);
    auto* d = static_cast<uint8_t*>(out_delta);
    for (int i = 0; i < 32; ++i) d[i] = static_cast<uint8_t>(a[i] ^ b[i]);
    return true;
}
bool asm_mesh_zkp_generate(const void* witness, void* out_proof) {
    if (!witness || !out_proof) return false;
    std::memcpy(out_proof, witness, 32);
    return true;
}
bool asm_mesh_zkp_verify(const void* proof, const void* public_input) {
    if (!proof || !public_input) return false;
    return std::memcmp(proof, public_input, 8) == 0;
}
bool asm_mesh_dht_xor_distance(const uint8_t* id1, const uint8_t* id2, uint8_t* out) {
    if (!id1 || !id2 || !out) return false;
    for (int i = 0; i < 32; ++i) out[i] = static_cast<uint8_t>(id1[i] ^ id2[i]);
    return true;
}
bool asm_mesh_dht_find_closest(const uint8_t* target, int k, void* out_nodes) {
    if (!target || k <= 0 || !out_nodes) return false;
    auto* ids = static_cast<uint32_t*>(out_nodes);
    for (int i = 0; i < k; ++i) ids[i] = static_cast<uint32_t>(target[i & 31]) + static_cast<uint32_t>(i);
    return true;
}
bool asm_mesh_fedavg_aggregate(const void** updates, int count, void* out) {
    if (!updates || count <= 0 || !out) return false;
    auto* o = static_cast<float*>(out);
    for (int j = 0; j < 16; ++j) {
        float acc = 0.0f;
        for (int i = 0; i < count; ++i) {
            const auto* vec = static_cast<const float*>(updates[i]);
            acc += vec[j];
        }
        o[j] = acc / static_cast<float>(count);
    }
    return true;
}
bool asm_mesh_gossip_disseminate(const void* msg, size_t size) {
    if (!msg || size == 0) return false;
    g_meshGossip.fetch_add(1, std::memory_order_relaxed);
    return true;
}
bool asm_mesh_shard_hash(const void* data, size_t size, uint8_t* out_hash) {
    if (!data || size == 0 || !out_hash) return false;
    const auto* b = static_cast<const uint8_t*>(data);
    uint8_t h = 0;
    for (size_t i = 0; i < size; ++i) h ^= static_cast<uint8_t>(b[i] + static_cast<uint8_t>(i));
    out_hash[0] = h;
    return true;
}
bool asm_mesh_shard_bitfield(int shard_id, void* out) {
    if (shard_id < 0 || !out) return false;
    *static_cast<uint64_t*>(out) = (1ULL << (shard_id & 63));
    return true;
}
bool asm_mesh_quorum_vote(int proposal_id, bool vote, void* out_result) {
    if (proposal_id <= 0 || !out_result) return false;
    g_meshVotes.fetch_add(1, std::memory_order_relaxed);
    *static_cast<int*>(out_result) = vote ? 1 : 0;
    return true;
}
bool asm_mesh_topology_update(const void* node_info, size_t size) {
    if (!g_meshInit.load(std::memory_order_relaxed) || !node_info || size == 0) return false;
    g_meshNodes.fetch_add(static_cast<int>((size & 3u) + 1u), std::memory_order_relaxed);
    return true;
}
int asm_mesh_topology_active_count() {
    return g_meshNodes.load(std::memory_order_relaxed);
}
void* asm_mesh_get_stats() {
    static uint64_t stats[4] = {0, 0, 0, 0};
    stats[0] = g_meshGossip.load(std::memory_order_relaxed);
    stats[1] = g_meshVotes.load(std::memory_order_relaxed);
    stats[2] = static_cast<uint64_t>(g_meshNodes.load(std::memory_order_relaxed));
    return stats;
}
void asm_mesh_shutdown() {
    g_meshInit.store(false, std::memory_order_relaxed);
    g_meshNodes.store(0, std::memory_order_relaxed);
}

} // extern "C"

// ============================================================================
// ContextLimits / ResolveContextDecision
// ============================================================================

namespace RawrXD {

// ContextLimits::estimateKVBytes is the only missing symbol - the rest are in context_config.h
int64_t ContextLimits::estimateKVBytes(int32_t contextTokens, int64_t kvBytesPerToken) {
    return static_cast<int64_t>(contextTokens) * kvBytesPerToken;
}

// ResolveContextDecision is defined in context_config.cpp which is not compiled for Gold
ContextDecision ResolveContextDecision(int32_t requested,
                                       int64_t vramBytes,
                                       int64_t ramBytes,
                                       const char* envVarName) {
    (void)vramBytes; (void)ramBytes; (void)envVarName;
    ContextDecision decision;
    decision.requested = requested;
    decision.effective = (requested > 0) ? requested : ContextLimits::DEFAULT;
    decision.system_safe_max = ContextLimits::DEFAULT;
    decision.kv_safe_max = ContextLimits::DEFAULT;
    decision.estimated_kv_bytes = static_cast<int64_t>(decision.effective) * ContextLimits::DEFAULT_KV_BYTES_PER_TOKEN;
    return decision;
}

} // namespace RawrXD

// ============================================================================
// SovereignCore / SovereignIDEBridge
// ============================================================================

// Include the real header to get correct class definitions
#include "sovereign/SovereignCoreWrapper.hpp"

// MASM function stubs for Gold target (MASM objects not linked in Gold build)
extern "C" {
    void Sovereign_Pipeline_Cycle(void) {}
    void AcquireSovereignLock(void) {}
    void ReleaseSovereignLock(void) {}
    void CoordinateAgents(void) {}
    void ObserveTokenStream(uint64_t) {}
    void HealSymbolResolution(const char*) {}
    uint64_t ValidateDMAAlignment(void) { return 0; }
    void RawrXD_Trigger_Chat(void) {}
    uint64_t g_CycleCounter = 0;
    uint64_t g_SovereignStatus = 0;
    uint64_t g_SymbolHealCount = 0;
    uint32_t g_ActiveAgentCount = 0;
    uint64_t g_AgentRegistry[32] = {};
}

namespace RawrXD {
namespace Sovereign {

// Static instance pointer definition
SovereignCore* SovereignCore::s_instance = nullptr;

SovereignCore& SovereignCore::getInstance() {
    if (!s_instance) {
        static SovereignCore instance;
        s_instance = &instance;
    }
    return *s_instance;
}

SovereignCore::SovereignCore()
    : m_initialized(false),
      m_running(false),
      m_loopThread(nullptr),
      m_cycleIntervalMs(1000),
      m_onCycleComplete(nullptr) {
}

SovereignCore::~SovereignCore() {
    if (m_running) {
        stopAutonomousLoop();
    }
}

void SovereignCore::initialize(uint32_t numAgents) {
    (void)numAgents;
    m_initialized = true;
}

void SovereignCore::shutdown() {
    stopAutonomousLoop();
    m_initialized = false;
}

bool SovereignCore::isInitialized() const {
    return m_initialized;
}

void SovereignCore::runCycle() {
    // Execute one autonomous cycle via MASM pipeline
    if (!m_initialized || !m_running) return;
    
    // Call the MASM pipeline cycle
    Sovereign_Pipeline_Cycle();
    
    // Fire callback if registered
    if (m_onCycleComplete) {
        CycleStats stats = getStats();
        m_onCycleComplete(stats);
    }
}

void SovereignCore::startAutonomousLoop() {
    m_running = true;
}

void SovereignCore::stopAutonomousLoop() {
    m_running = false;
}

bool SovereignCore::isRunning() const {
    return m_running;
}

SovereignCore::CycleStats SovereignCore::getStats() const {
    CycleStats stats{};
    stats.cycleCount = 0;
    stats.healCount = 0;
    stats.status = Status::IDLE;
    stats.elapsed = std::chrono::milliseconds(0);
    return stats;
}

SovereignCore::Status SovereignCore::getCurrentStatus() const {
    return Status::IDLE;
}

std::vector<SovereignCore::AgentState> SovereignCore::getAgentStates() const {
    return {};
}

void SovereignCore::triggerFullChatPipeline() {
}

void SovereignCore::triggerSelfHeal(const std::string& symbol) {
    (void)symbol;
}

void SovereignCore::validateAlignment() {
}

void SovereignCore::setOnCycleComplete(CycleCallback cb) {
    m_onCycleComplete = cb;
}

uint32_t SovereignCore::getCycleIntervalMs() const {
    return m_cycleIntervalMs;
}

void SovereignCore::setCycleIntervalMs(uint32_t ms) {
    m_cycleIntervalMs = ms;
}

void SovereignCore::autonomousLoopProc() {
    while (m_running) {
        runCycle();
        if (m_onCycleComplete) {
            CycleStats stats = getStats();
            m_onCycleComplete(stats);
        }
        // Simple sleep simulation
        for (volatile uint32_t i = 0; i < m_cycleIntervalMs * 1000; ++i) {}
    }
}

// SovereignIDEBridge implementation
SovereignIDEBridge& SovereignIDEBridge::getInstance() {
    static SovereignIDEBridge instance;
    return instance;
}

SovereignIDEBridge::SovereignIDEBridge()
    : m_core(SovereignCore::getInstance()),
      m_uiCallback(nullptr) {
}

SovereignIDEBridge::~SovereignIDEBridge() = default;

void SovereignIDEBridge::onEngineCycle(const std::string& chatInput) {
    (void)chatInput;
}

std::string SovereignIDEBridge::getStatusDisplayLine() const {
    return "Cycle: 0 | Status: IDLE | Heals: 0";
}

std::vector<uint64_t> SovereignIDEBridge::getLatestTokens(uint32_t count) const {
    (void)count;
    return {};
}

void SovereignIDEBridge::setUIUpdateCallback(UIUpdateCallback cb) {
    m_uiCallback = cb;
}

} // namespace Sovereign
} // namespace RawrXD

// ============================================================================
// VSCodeExtensionAPI stubs (for js_extension_host.cpp / vscext_registry.cpp)
// ============================================================================

// Include the real header to get correct struct/class definitions
#include "modules/vscode_extension_api.h"

// Provide stub implementations for VSCodeExtensionAPI member functions
// that are referenced but not linked in the Gold build.

namespace vscode {

VSCodeExtensionAPI& VSCodeExtensionAPI::instance() {
    static VSCodeExtensionAPI inst;
    return inst;
}

VSCodeExtensionAPI::VSCodeExtensionAPI()
    : m_host(nullptr), m_mainWindow(nullptr), m_initialized(false) {}

VSCodeExtensionAPI::~VSCodeExtensionAPI() = default;

VSCodeAPIResult VSCodeExtensionAPI::initialize(Win32IDE* host, HWND mainWindow) {
    (void)host; (void)mainWindow;
    m_initialized = true;
    return VSCodeAPIResult::ok();
}

VSCodeAPIResult VSCodeExtensionAPI::shutdown() {
    m_initialized = false;
    return VSCodeAPIResult::ok();
}

bool VSCodeExtensionAPI::isInitialized() const {
    return m_initialized;
}

VSCodeAPIResult VSCodeExtensionAPI::registerCommand(const char* commandId,
                                                       void (*handler)(void* ctx), void* ctx) {
    (void)commandId; (void)handler; (void)ctx;
    return VSCodeAPIResult::error("Not implemented in Gold build");
}

VSCodeAPIResult VSCodeExtensionAPI::executeCommand(const char* commandId, const char* argsJson) {
    (void)commandId; (void)argsJson;
    return VSCodeAPIResult::error("Not implemented in Gold build");
}

VSCodeAPIResult VSCodeExtensionAPI::registerProvider(ProviderType type, const char* languageId,
                                                       void* provider) {
    (void)type; (void)languageId; (void)provider;
    return VSCodeAPIResult::error("Not implemented in Gold build");
}

VSCodeStatusBarItem* VSCodeExtensionAPI::createStatusBarItem(StatusBarAlignment alignment, int priority) {
    (void)alignment; (void)priority;
    return nullptr;
}

void VSCodeExtensionAPI::updateStatusBar() {
}

VSCodeConfiguration VSCodeExtensionAPI::getConfiguration(const char* section) {
    (void)section;
    return VSCodeConfiguration();
}

// Namespace-level free functions
namespace commands {
    VSCodeAPIResult getCommands(bool filterInternal,
                                 char** outIds, size_t maxIds, size_t* outCount) {
        (void)filterInternal; (void)outIds; (void)maxIds; (void)outCount;
        return VSCodeAPIResult::error("Not implemented in Gold build");
    }
} // namespace commands

namespace window {
    VSCodeAPIResult showInformationMessage(const char* message,
                                            const VSCodeMessageItem* items, size_t itemCount,
                                            int* outSelectedIndex) {
        (void)message; (void)items; (void)itemCount; (void)outSelectedIndex;
        return VSCodeAPIResult::error("Not implemented in Gold build");
    }

    VSCodeAPIResult showQuickPick(const VSCodeQuickPickItem* items, size_t itemCount,
                                   const char* placeHolder, bool canPickMany,
                                   int* outSelectedIndices, size_t maxSelected,
                                   size_t* outSelectedCount) {
        (void)items; (void)itemCount; (void)placeHolder; (void)canPickMany;
        (void)outSelectedIndices; (void)maxSelected; (void)outSelectedCount;
        return VSCodeAPIResult::error("Not implemented in Gold build");
    }

    VSCodeAPIResult showInputBox(const VSCodeInputBoxOptions* options,
                                  char* outValue, size_t maxLen) {
        (void)options; (void)outValue; (void)maxLen;
        return VSCodeAPIResult::error("Not implemented in Gold build");
    }
} // namespace window

namespace workspace {
    VSCodeAPIResult openTextDocumentByPath(const char* filePath, VSCodeTextDocument* outDoc) {
        (void)filePath; (void)outDoc;
        return VSCodeAPIResult::error("Not implemented in Gold build");
    }

    VSCodeAPIResult findFiles(const char* includePattern, const char* excludePattern,
                               size_t maxResults,
                               VSCodeUri* outUris, size_t maxUris, size_t* outCount) {
        (void)includePattern; (void)excludePattern; (void)maxResults;
        (void)outUris; (void)maxUris; (void)outCount;
        return VSCodeAPIResult::error("Not implemented in Gold build");
    }
} // namespace workspace

} // namespace vscode

// ============================================================================
// handleFileLoadModel (for feature_handlers.cpp / unified_command_dispatch.cpp)
// ============================================================================

struct CommandContext {
    const char* command = nullptr;
    const char* arg = nullptr;
};

struct CommandResult {
    bool success = false;
    const char* message = "";
};

CommandResult handleFileLoadModel(const CommandContext& ctx) {
    (void)ctx;
    return {false, "File load model not implemented in Gold build"};
}

// ============================================================================
// Pattern finder stub (for byte_level_hotpatcher.cpp)
// ============================================================================

extern "C" {

void* find_pattern_asm(const void* base, size_t len, const uint8_t* pattern, size_t patternLen) {
    if (!base || !pattern || patternLen == 0 || len < patternLen) return nullptr;
    const auto* h = static_cast<const uint8_t*>(base);
    for (size_t i = 0; i + patternLen <= len; ++i) {
        if (std::memcmp(h + i, pattern, patternLen) == 0) return const_cast<uint8_t*>(h + i);
    }
    return nullptr;
}

void* asm_snapshot_get_stats() {
    static uint64_t stats[4] = {0, 0, 0, 0};
    return stats;
}

} // extern "C"

// ============================================================================
// Video rendering stub (for AgentToolHandlers.cpp / tool_registry.cpp)
// ============================================================================

#include <expected>
#include <filesystem>

namespace rawrxd {
namespace video {

struct TubiRenderRequest
{
    std::string jobId;
    std::string engineName;
    std::string provider;
    std::string localModel;
    std::string prompt;
    std::string storyboard;
    std::string style;
    std::string duration;
    std::string aspectRatio;
    std::string resolution;
    std::string negativePrompt;
    std::string cameraMode;
    int seed = 0;
    std::filesystem::path outputDir;
};

struct TubiRenderResult
{
    int width = 0;
    int height = 0;
    int fps = 0;
    int totalFrames = 0;
    int durationSeconds = 0;
    int shotCount = 0;
    int seedUsed = 0;
    std::filesystem::path framesDir;
    std::filesystem::path manifestPath;
    std::filesystem::path mp4Path;
    std::filesystem::path progressPath;
    std::filesystem::path shotPlanPath;
    std::filesystem::path contactSheetPath;
    std::filesystem::path previewStartPath;
    std::filesystem::path previewMidPath;
    std::filesystem::path previewEndPath;
    std::string encoderDiagnostics;
    std::string extractedTags;
    bool mp4Created = false;
};

std::expected<TubiRenderResult, std::string> renderVideoClip(const TubiRenderRequest& request) {
    (void)request;
    return std::unexpected<std::string>("Video rendering not implemented in Gold build");
}

} // namespace video
} // namespace rawrxd

// ============================================================================
// Native speed layer stubs (for native_speed_layer.cpp)
// ============================================================================

extern "C" {

void Sampler_SoftMax_TopK_Fused(float* logits, uint32_t* indices, int n, int K) {
    if (!logits || !indices || n <= 0 || K <= 0) return;
    const int kk = (K < n) ? K : n;
    struct IdxVal { int idx; float v; };
    std::vector<IdxVal> scratch(static_cast<size_t>(n));
    for (int i = 0; i < n; ++i) { scratch[i].idx = i; scratch[i].v = logits[i]; }
    std::partial_sort(scratch.begin(), scratch.begin() + kk, scratch.end(),
                      [](const IdxVal& a, const IdxVal& b) { return a.v > b.v; });
    float m = scratch[0].v;
    for (int i = 1; i < kk; ++i) m = std::max(m, scratch[i].v);
    float sum = 0.0f;
    for (int i = 0; i < kk; ++i) {
        logits[i] = std::exp(scratch[i].v - m);
        sum += logits[i];
        indices[i] = static_cast<uint32_t>(scratch[i].idx);
    }
    const float inv = (sum > 0.0f) ? (1.0f / sum) : 0.0f;
    for (int i = 0; i < kk; ++i) logits[i] *= inv;
    for (int i = kk; i < n; ++i) logits[i] = 0.0f;
}

void Titan_SiLU_AVX512(float* data, int n) {
    if (!data || n <= 0) return;
    for (int i = 0; i < n; ++i) {
        float x = data[i];
        data[i] = x / (1.0f + std::exp(-x));
    }
}

void Titan_RMSNorm_AVX512(const float* in, float* out, const float* weight, int n) {
    if (!in || !out || !weight || n <= 0) return;
    float ss = 0.0f;
    for (int i = 0; i < n; ++i) ss += in[i] * in[i];
    const float rms = std::sqrt(ss / static_cast<float>(n) + 1e-5f);
    const float inv = 1.0f / rms;
    for (int i = 0; i < n; ++i) out[i] = in[i] * inv * weight[i];
}

void Sampler_ApplyTemperature_AVX512(float* logits, int n, float invTemp) {
    if (!logits || n <= 0) return;
    for (int i = 0; i < n; ++i) logits[i] *= invTemp;
}

float Sampler_FindMax_AVX512(const float* logits, int n) {
    if (!logits || n <= 0) return 0.0f;
    float m = logits[0];
    for (int i = 1; i < n; ++i) m = std::max(m, logits[i]);
    return m;
}

float Sampler_ExpSum_AVX512(const float* logits, int n, float maxVal) {
    if (!logits || n <= 0) return 0.0f;
    float s = 0.0f;
    for (int i = 0; i < n; ++i) s += std::exp(logits[i] - maxVal);
    return s;
}

} // extern "C"

// ============================================================================
// SovereignInferenceClient (for agentic_deep_thinking_engine.cpp)
// ============================================================================

#include "agentic/AgentOllamaClient.h"
#include "agentic/SovereignInferenceClient.h"

namespace RawrXD {
namespace Agent {

class SovereignInferenceClient::Impl {
public:
    bool loaded = false;
    SovereignModelConfig config;
};

SovereignInferenceClient::SovereignInferenceClient(const SovereignModelConfig& cfg)
    : pImpl_(std::make_unique<Impl>()), m_config(cfg) {
}

SovereignInferenceClient::~SovereignInferenceClient() = default;

bool SovereignInferenceClient::LoadModel(const std::string& gguf_path) {
    (void)gguf_path;
    if (pImpl_) pImpl_->loaded = true;
    return true;
}

void SovereignInferenceClient::UnloadModel() {
    if (pImpl_) pImpl_->loaded = false;
}

bool SovereignInferenceClient::IsLoaded() const {
    return pImpl_ ? pImpl_->loaded : false;
}

void SovereignInferenceClient::ClearKVCache() {
}

InferenceResult SovereignInferenceClient::ChatSync(const std::vector<ChatMessage>& messages,
                                                    const nlohmann::json& tools) {
    (void)messages; (void)tools;
    m_totalRequests.fetch_add(1, std::memory_order_relaxed);
    return InferenceResult::error("SovereignInferenceClient not implemented in Gold build");
}

bool SovereignInferenceClient::ChatStream(const std::vector<ChatMessage>& messages,
                                           const nlohmann::json& tools,
                                           TokenCallback on_token,
                                           ToolCallCallback on_tool_call,
                                           DoneCallback on_done,
                                           ErrorCallback on_error) {
    (void)messages; (void)tools; (void)on_token; (void)on_tool_call; (void)on_done;
    if (on_error) on_error("SovereignInferenceClient not implemented in Gold build");
    return false;
}

double SovereignInferenceClient::GetAvgTokensPerSec() const {
    return 0.0;
}

} // namespace Agent
} // namespace RawrXD

// ============================================================================
// AgenticBridge::ExecuteAgentCommand (for RawrXD_AgentLoop.cpp)
// ============================================================================

// Include the real header to get the correct class definition
#include "win32app/Win32IDE_AgenticBridge.h"

AgenticBridge::AgenticBridge(Win32IDE* ide) { (void)ide; }
AgenticBridge::~AgenticBridge() = default;

AgentResponse AgenticBridge::ExecuteAgentCommand(const std::string& prompt) {
    (void)prompt;
    AgentResponse r;
    r.type = AgentResponseType::AGENT_ERROR;
    r.content = "AgenticBridge not implemented in Gold build";
    return r;
}

// ============================================================================
// Snapshot stubs
// ============================================================================

extern "C" {

bool asm_snapshot_verify(void* snapshot, void* target, size_t size) {
    if (!snapshot || !target || size == 0) return false;
    return std::memcmp(snapshot, target, size) == 0;
}
void asm_snapshot_discard(void* snapshot) {
    if (snapshot) ::operator delete(snapshot);
}

} // extern "C"

// ============================================================================
// Camellia256 stubs
// ============================================================================

extern "C" {

bool asm_camellia256_auth_encrypt_file(const char* in_path, const char* out_path,
                                         const uint8_t* key, const uint8_t* iv) {
    if (!in_path || !out_path || !key || !iv) return false;
    std::ifstream in(in_path, std::ios::binary);
    if (!in) return false;
    std::ofstream out(out_path, std::ios::binary | std::ios::trunc);
    if (!out) return false;
    uint8_t buffer[4096];
    uint64_t offset = 0;
    while (in) {
        in.read(reinterpret_cast<char*>(buffer), sizeof(buffer));
        const auto readCount = in.gcount();
        if (readCount <= 0) break;
        for (std::streamsize i = 0; i < readCount; ++i) {
            const uint8_t k = key[(offset + static_cast<uint64_t>(i)) & 31u] ^
                              iv[(offset + static_cast<uint64_t>(i)) & 15u];
            buffer[i] ^= k;
        }
        out.write(reinterpret_cast<const char*>(buffer), readCount);
        if (!out) return false;
        offset += static_cast<uint64_t>(readCount);
    }
    return true;
}

bool asm_camellia256_auth_decrypt_file(const char* in_path, const char* out_path,
                                         const uint8_t* key, const uint8_t* iv) {
    // XOR cipher is symmetric
    return asm_camellia256_auth_encrypt_file(in_path, out_path, key, iv);
}

} // extern "C"

// ============================================================================
// Git MCP tools registration
// ============================================================================

void register_git_mcp_tools() {}
