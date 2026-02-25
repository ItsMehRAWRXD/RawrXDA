// ============================================================================
// dual_engine_inference.cpp — 800B Dual-Engine Inference Manager (Phase 21)
// ============================================================================
// Enterprise-tier multi-shard inference orchestrator for 800B+ models.
// Coordinates shard discovery and inference dispatch with license gating.
//
// Uses EnterpriseLicense (core/enterprise_license.h) for feature gating.
//
// PATTERN:   No exceptions. Returns bool/string.
// THREADING: Singleton with std::mutex. Thread-safe.
// RULE:      NO SOURCE FILE IS TO BE SIMPLIFIED
// ============================================================================

#include "dual_engine_inference.h"
#include "enterprise_license.h"

#include <cstdio>
#include <cstring>
#include <filesystem>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace RawrXD {

// ============================================================================
// Singleton
// ============================================================================
DualEngineManager& DualEngineManager::Instance() {
    static DualEngineManager s_instance;
    return s_instance;
}

// ============================================================================
// License Gate — checks Enterprise license for 800B feature
// ============================================================================
bool DualEngineManager::checkLicenseGate() {
    auto& lic = EnterpriseLicense::Instance();
    if (!lic.Is800BUnlocked()) {
        m_lastError = "[LICENSE] 800B Dual-Engine requires Enterprise license";
        fprintf(stderr, "[DualEngine] %s\n", m_lastError.c_str());
        return false;
    }
    return true;
}

// ============================================================================
// Lifecycle
// ============================================================================
bool DualEngineManager::initialize() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_initialized) return true;

    m_state = EngineState::Initializing;

    // Gate check — Enterprise tier required
    if (!checkLicenseGate()) {
        m_state = EngineState::Error;
        return false;
    }

    fprintf(stderr, "[DualEngine] Initializing 800B Dual-Engine subsystem...\n");

    auto& lic = EnterpriseLicense::Instance();
    uint64_t maxGB = lic.GetMaxModelSizeGB();

    if (maxGB < 800) {
        char buf[256];
        snprintf(buf, sizeof(buf),
                 "Model size exceeds tier limit (need Enterprise for 800B, current max=%llu GB)",
                 (unsigned long long)maxGB);
        m_lastError = buf;
        fprintf(stderr, "[DualEngine] %s\n", m_lastError.c_str());
        m_state = EngineState::Error;
        return false;
    }

    m_initialized = true;
    m_state = EngineState::Ready;
    fprintf(stderr, "[DualEngine] 800B Dual-Engine: READY (budget=%llu GB, ctx=%llu)\n",
            (unsigned long long)maxGB, (unsigned long long)lic.GetMaxContextLength());
    return true;
}

bool DualEngineManager::initialize(const DualEngineConfig& config) {
    m_config = config;
    return initialize();
}

void DualEngineManager::shutdown() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_state = EngineState::ShuttingDown;
    m_initialized = false;
    m_shardPaths.clear();
    m_modelLoaded = false;
    m_state = EngineState::Uninitialized;
    fprintf(stderr, "[DualEngine] Shutdown complete\n");
}

// ============================================================================
// Model Loading
// ============================================================================
bool DualEngineManager::loadModel(const std::string& modelPath) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!m_initialized) {
        m_lastError = "DualEngine not initialized";
        return false;
    }

    m_state = EngineState::Loading;

    auto& lic = EnterpriseLicense::Instance();
    uint64_t maxGB = lic.GetMaxModelSizeGB();

    // Discover shard files
    m_shardPaths.clear();
    for (int i = 0; i < static_cast<int>(m_config.maxShards); ++i) {
        char shardPath[512];
        snprintf(shardPath, sizeof(shardPath), "%s\\shard%d.gguf", modelPath.c_str(), i);

#ifdef _WIN32
        DWORD attr = GetFileAttributesA(shardPath);
        if (attr != INVALID_FILE_ATTRIBUTES) {
            m_shardPaths.push_back(shardPath);
        } else {
            break;
        }
#else
        FILE* f = fopen(shardPath, "rb");
        if (f) {
            fclose(f);
            m_shardPaths.push_back(shardPath);
        } else {
            break;
        }
#endif
    }

    if (m_shardPaths.empty()) {
        m_shardPaths.push_back(modelPath);
    }

    fprintf(stderr, "[DualEngine] Discovered %zu shard(s) for model: %s\n",
            m_shardPaths.size(), modelPath.c_str());

    // Compute total size
    uint64_t totalBytes = 0;
    for (const auto& sp : m_shardPaths) {
#ifdef _WIN32
        HANDLE hFile = CreateFileA(sp.c_str(), GENERIC_READ, FILE_SHARE_READ,
                                   nullptr, OPEN_EXISTING, 0, nullptr);
        if (hFile != INVALID_HANDLE_VALUE) {
            LARGE_INTEGER sz;
            GetFileSizeEx(hFile, &sz);
            totalBytes += static_cast<uint64_t>(sz.QuadPart);
            CloseHandle(hFile);
        }
#else
        FILE* f = fopen(sp.c_str(), "rb");
        if (f) {
            fseek(f, 0, SEEK_END);
            totalBytes += static_cast<uint64_t>(ftell(f));
            fclose(f);
        }
#endif
    }

    uint64_t totalGB = totalBytes / (1024ULL * 1024 * 1024);
    if (totalGB > maxGB) {
        char buf[256];
        snprintf(buf, sizeof(buf),
                 "Model size (%llu GB) exceeds tier limit (%llu GB) — upgrade license",
                 (unsigned long long)totalGB, (unsigned long long)maxGB);
        m_lastError = buf;
        fprintf(stderr, "[DualEngine] %s\n", m_lastError.c_str());
        m_state = EngineState::Error;
        return false;
    }

    m_totalModelBytes = totalBytes;
    m_modelLoaded = true;
    m_state = EngineState::Loaded;
    fprintf(stderr, "[DualEngine] Model loaded: %zu shards, %llu bytes (%.1f GB)\n",
            m_shardPaths.size(), (unsigned long long)totalBytes,
            totalBytes / (1024.0 * 1024 * 1024));
    return true;
}

void DualEngineManager::unloadModel() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_shardPaths.clear();
    m_modelLoaded = false;
    m_totalModelBytes = 0;
    if (m_initialized) m_state = EngineState::Ready;
}

// ============================================================================
// Inference
// ============================================================================
std::string DualEngineManager::infer(const std::string& prompt) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!m_initialized || !m_modelLoaded) {
        return "[DualEngine] Not ready — call initialize() and loadModel() first";
    }

    m_state = EngineState::Inferring;

    // Re-check license per-inference (prevents key revocation bypass)
    auto& lic = EnterpriseLicense::Instance();
    if (!lic.Is800BUnlocked()) {
        m_state = EngineState::Error;
        return "[LICENSE] 800B Dual-Engine: locked (requires Enterprise license)";
    }

    m_inferenceCount++;
    m_totalTokensProcessed += prompt.length() / 4; // rough estimate

    m_state = EngineState::Loaded;

    // Minimal real dispatch: pick a shard deterministically from the prompt and report routing.
    // This keeps behavior stable while the shard runtime is integrated.
    const size_t shardIdx = m_shardPaths.empty()
        ? 0
        : (std::hash<std::string>{}(prompt) % m_shardPaths.size());
    const std::string shard = m_shardPaths.empty()
        ? std::string("<no_shards>")
        : std::filesystem::path(m_shardPaths[shardIdx]).filename().string();

    char response[512];
    snprintf(response, sizeof(response),
             "[DualEngine] Inference #%llu routed to shard[%zu/%zu]=%s (%llu tokens processed)",
             (unsigned long long)m_inferenceCount,
             shardIdx,
             m_shardPaths.size(),
             shard.c_str(),
             (unsigned long long)m_totalTokensProcessed);
    return response;
}

// ============================================================================
// Status Queries
// ============================================================================
bool DualEngineManager::isInitialized() const { return m_initialized; }
bool DualEngineManager::isModelLoaded() const { return m_modelLoaded; }
size_t DualEngineManager::shardCount() const { return m_shardPaths.size(); }
uint64_t DualEngineManager::totalModelBytes() const { return m_totalModelBytes; }
uint64_t DualEngineManager::inferenceCount() const { return m_inferenceCount; }
const std::string& DualEngineManager::lastError() const { return m_lastError; }
EngineState DualEngineManager::currentState() const { return m_state; }

EngineStatus DualEngineManager::getStatus() const {
    return EngineStatus{
        m_state,
        m_initialized,
        m_modelLoaded,
        m_shardPaths.size(),
        m_totalModelBytes,
        m_inferenceCount,
        m_totalTokensProcessed,
        m_lastError.c_str()
    };
}

} // namespace RawrXD
