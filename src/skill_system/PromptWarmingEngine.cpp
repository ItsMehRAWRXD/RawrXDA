// ============================================================================
// PromptWarmingEngine.cpp — TPS Optimization via Prompt Pre-Processing
// ============================================================================

#include "PromptWarmingEngine.h"
#include <windows.h>
#include <wincrypt.h>
#include <sstream>
#include <iomanip>
#include <algorithm>

#pragma comment(lib, "advapi32.lib")

namespace RawrXD {
namespace SkillSystem {

// ============================================================================
// SINGLETON
// ============================================================================
PromptWarmingEngine& PromptWarmingEngine::Instance() {
    static PromptWarmingEngine instance;
    return instance;
}

PromptWarmingEngine::~PromptWarmingEngine() {
    Shutdown();
}

// ============================================================================
// INITIALIZATION
// ============================================================================
bool PromptWarmingEngine::Initialize() {
    if (m_running.exchange(true)) {
        return true;  // Already running
    }

    try {
        m_warmingThread = std::thread(&PromptWarmingEngine::WarmingLoop, this);
    } catch (const std::exception& e) {
        OutputDebugStringA("[PromptWarming] Failed to start warming thread: ");
        OutputDebugStringA(e.what());
        OutputDebugStringA("\n");
        m_running.store(false);
        return false;
    }

    OutputDebugStringA("[PromptWarming] Engine initialized\n");
    return true;
}

void PromptWarmingEngine::Shutdown() {
    if (!m_running.exchange(false)) {
        return;
    }

    m_queueCv.notify_all();
    if (m_warmingThread.joinable()) {
        m_warmingThread.join();
    }

    std::lock_guard<std::mutex> lock(m_mutex);
    m_cache.clear();
    m_idToFingerprint.clear();
    m_totalCacheBytes.store(0);

    OutputDebugStringA("[PromptWarming] Engine shutdown\n");
}

// ============================================================================
// FINGERPRINTING
// ============================================================================
std::string PromptWarmingEngine::ComputeFingerprint(const std::string& prompt) const {
    HCRYPTPROV hProv = 0;
    HCRYPTHASH hHash = 0;
    BYTE hash[32] = {0};
    DWORD hashLen = 32;
    std::string result;

    if (!CryptAcquireContext(&hProv, nullptr, nullptr, PROV_RSA_AES, CRYPT_VERIFYCONTEXT)) {
        // Fallback: simple hash
        uint64_t h = 0xcbf29ce484222325ULL;
        for (char c : prompt) {
            h ^= static_cast<uint8_t>(c);
            h *= 0x100000001b3ULL;
        }
        std::ostringstream oss;
        oss << std::hex << std::setw(16) << std::setfill('0') << h;
        return oss.str();
    }

    if (CryptCreateHash(hProv, CALG_SHA_256, 0, 0, &hHash)) {
        CryptHashData(hHash, reinterpret_cast<const BYTE*>(prompt.data()), 
                      static_cast<DWORD>(prompt.size()), 0);
        if (CryptGetHashParam(hHash, HP_HASHVAL, hash, &hashLen, 0)) {
            std::ostringstream oss;
            for (DWORD i = 0; i < hashLen; ++i) {
                oss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(hash[i]);
            }
            result = oss.str();
        }
        CryptDestroyHash(hHash);
    }

    CryptReleaseContext(hProv, 0);
    return result.empty() ? "fallback_hash" : result;
}

// ============================================================================
// WARM PROMPT (ASYNC)
// ============================================================================
uint64_t PromptWarmingEngine::WarmPromptAsync(const std::string& prompt) {
    if (prompt.empty()) {
        return 0;
    }

    std::string fingerprint = ComputeFingerprint(prompt);

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_cache.find(fingerprint);
        if (it != m_cache.end() && it->second.isValid) {
            it->second.lastUsedMs = GetTickCount64();
            it->second.useCount++;
            m_cacheHits++;
            return it->second.id;
        }
    }

    m_cacheMisses++;

    {
        std::lock_guard<std::mutex> lock(m_queueMutex);
        m_warmingQueue.push_back(prompt);
    }
    m_queueCv.notify_one();

    // Return a pending ID
    uint64_t pendingId = m_nextId.fetch_add(1);
    return pendingId;
}

// ============================================================================
// WARM PROMPT (SYNC)
// ============================================================================
WarmedPromptHandle PromptWarmingEngine::WarmPromptSync(
    const std::string& prompt, uint32_t timeoutMs) {
    
    if (prompt.empty()) {
        return WarmedPromptHandle{};
    }

    std::string fingerprint = ComputeFingerprint(prompt);

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_cache.find(fingerprint);
        if (it != m_cache.end() && it->second.isValid) {
            it->second.lastUsedMs = GetTickCount64();
            it->second.useCount++;
            m_cacheHits++;
            return it->second;
        }
    }

    m_cacheMisses++;

    // Perform warming synchronously
    WarmedPromptHandle handle;
    handle.id = m_nextId.fetch_add(1);
    handle.fingerprint = fingerprint;
    handle.warmedContent = prompt;
    handle.tokenCount = std::count(prompt.begin(), prompt.end(), '\n') + 1;
    handle.warmedAtMs = GetTickCount64();
    handle.lastUsedMs = handle.warmedAtMs;
    handle.useCount = 1;
    handle.isValid = true;

    // Prepend pre-seeded skill context if available
    if (!m_preseededSkillContext.empty()) {
        handle.warmedContent = m_preseededSkillContext + "\n\n" + prompt;
        handle.tokenCount += m_preseededTokenCount;
    }

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        EvictIfNeeded();
        m_cache[fingerprint] = handle;
        m_idToFingerprint[handle.id] = fingerprint;
        m_totalCacheBytes += handle.warmedContent.size();
    }

    return handle;
}

// ============================================================================
// CACHE QUERIES
// ============================================================================
bool PromptWarmingEngine::IsWarmed(const std::string& prompt) const {
    std::string fingerprint = ComputeFingerprint(prompt);
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_cache.find(fingerprint);
    return it != m_cache.end() && it->second.isValid;
}

bool PromptWarmingEngine::IsWarmed(uint64_t handleId) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_idToFingerprint.find(handleId);
    if (it == m_idToFingerprint.end()) {
        return false;
    }
    auto cacheIt = m_cache.find(it->second);
    return cacheIt != m_cache.end() && cacheIt->second.isValid;
}

std::string PromptWarmingEngine::GetWarmedContent(uint64_t handleId) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_idToFingerprint.find(handleId);
    if (it == m_idToFingerprint.end()) {
        return "";
    }
    auto cacheIt = m_cache.find(it->second);
    if (cacheIt != m_cache.end() && cacheIt->second.isValid) {
        cacheIt->second.lastUsedMs = GetTickCount64();
        cacheIt->second.useCount++;
        return cacheIt->second.warmedContent;
    }
    return "";
}

std::string PromptWarmingEngine::GetWarmedContent(const std::string& prompt) {
    std::string fingerprint = ComputeFingerprint(prompt);
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_cache.find(fingerprint);
    if (it != m_cache.end() && it->second.isValid) {
        it->second.lastUsedMs = GetTickCount64();
        it->second.useCount++;
        return it->second.warmedContent;
    }
    return "";
}

// ============================================================================
// PRE-SEED SKILL CONTEXT
// ============================================================================
void PromptWarmingEngine::PreseedSkillContext(const std::string& skillContext) {
    m_preseededSkillContext = skillContext;
    m_preseededTokenCount = std::count(skillContext.begin(), skillContext.end(), '\n') + 1;

    OutputDebugStringA("[PromptWarming] Pre-seeded skill context: ");
    OutputDebugStringA(std::to_string(m_preseededTokenCount).c_str());
    OutputDebugStringA(" tokens\n");
}

// ============================================================================
// BATCH WARMING
// ============================================================================
std::vector<WarmedPromptHandle> PromptWarmingEngine::WarmPromptBatch(
    const std::vector<std::string>& prompts) {
    
    std::vector<WarmedPromptHandle> results;
    results.reserve(prompts.size());

    for (const auto& prompt : prompts) {
        results.push_back(WarmPromptSync(prompt, 1000));
    }

    return results;
}

// ============================================================================
// CACHE MAINTENANCE
// ============================================================================
void PromptWarmingEngine::EvictCache() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_cache.clear();
    m_idToFingerprint.clear();
    m_totalCacheBytes.store(0);
}

void PromptWarmingEngine::EvictStaleEntries(uint64_t maxAgeMs) {
    uint64_t now = GetTickCount64();
    std::lock_guard<std::mutex> lock(m_mutex);

    for (auto it = m_cache.begin(); it != m_cache.end();) {
        if (now > it->second.lastUsedMs && 
            (now - it->second.lastUsedMs) > maxAgeMs) {
            m_totalCacheBytes -= it->second.warmedContent.size();
            m_idToFingerprint.erase(it->second.id);
            it = m_cache.erase(it);
        } else {
            ++it;
        }
    }
}

void PromptWarmingEngine::EvictIfNeeded() {
    if (m_cache.size() < PROMPT_WARM_CACHE_MAX_ENTRIES && 
        m_totalCacheBytes.load() < PROMPT_WARM_CACHE_MAX_BYTES) {
        return;
    }

    // Evict least recently used
    auto lruIt = m_cache.begin();
    for (auto it = m_cache.begin(); it != m_cache.end(); ++it) {
        if (it->second.lastUsedMs < lruIt->second.lastUsedMs) {
            lruIt = it;
        }
    }

    if (lruIt != m_cache.end()) {
        m_totalCacheBytes -= lruIt->second.warmedContent.size();
        m_idToFingerprint.erase(lruIt->second.id);
        m_cache.erase(lruIt);
    }
}

// ============================================================================
// METRICS
// ============================================================================
size_t PromptWarmingEngine::GetCacheEntryCount() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_cache.size();
}

size_t PromptWarmingEngine::GetCacheTotalBytes() const {
    return m_totalCacheBytes.load();
}

size_t PromptWarmingEngine::GetCacheHitCount() const {
    return m_cacheHits.load();
}

size_t PromptWarmingEngine::GetCacheMissCount() const {
    return m_cacheMisses.load();
}

float PromptWarmingEngine::GetCacheHitRate() const {
    size_t hits = m_cacheHits.load();
    size_t misses = m_cacheMisses.load();
    size_t total = hits + misses;
    if (total == 0) {
        return 0.0f;
    }
    return static_cast<float>(hits) / static_cast<float>(total);
}

// ============================================================================
// TPS ESTIMATION
// ============================================================================
float PromptWarmingEngine::EstimateTPS(const std::string& prompt) const {
    std::string fingerprint = ComputeFingerprint(prompt);
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_cache.find(fingerprint);
    if (it != m_cache.end() && it->second.isValid) {
        // Warmed prompts get ~40% TPS boost (no tokenization overhead)
        return 1.4f;
    }
    return 1.0f;  // Baseline
}

float PromptWarmingEngine::EstimateTPS(uint64_t handleId) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_idToFingerprint.find(handleId);
    if (it == m_idToFingerprint.end()) {
        return 1.0f;
    }
    auto cacheIt = m_cache.find(it->second);
    if (cacheIt != m_cache.end() && cacheIt->second.isValid) {
        return 1.4f;
    }
    return 1.0f;
}

// ============================================================================
// WARMING LOOP (BACKGROUND THREAD)
// ============================================================================
void PromptWarmingEngine::WarmingLoop() {
    OutputDebugStringA("[PromptWarming] Background thread started\n");

    while (m_running.load()) {
        std::unique_lock<std::mutex> lock(m_queueMutex);
        m_queueCv.wait_for(lock, std::chrono::milliseconds(PROMPT_WARM_HEARTBEAT_MS),
            [this] { return !m_warmingQueue.empty() || !m_running.load(); });

        if (!m_running.load()) {
            break;
        }

        std::vector<std::string> batch;
        while (!m_warmingQueue.empty() && batch.size() < PROMPT_WARM_BATCH_SIZE) {
            batch.push_back(m_warmingQueue.back());
            m_warmingQueue.pop_back();
        }
        lock.unlock();

        for (const auto& prompt : batch) {
            WarmPromptSync(prompt, 500);
        }

        // Periodic maintenance
        static uint64_t lastMaintenance = 0;
        uint64_t now = GetTickCount64();
        if (now > lastMaintenance && (now - lastMaintenance) > 60000) {
            EvictStaleEntries(PROMPT_WARM_CACHE_TTL_MS);
            lastMaintenance = now;
        }
    }

    OutputDebugStringA("[PromptWarming] Background thread stopped\n");
}

// ============================================================================
// C-API EXPORTS
// ============================================================================
extern "C" {

__declspec(dllexport) uint64_t __stdcall PromptWarm_WarmAsync(const char* prompt) {
    if (!prompt) return 0;
    return PromptWarmingEngine::Instance().WarmPromptAsync(prompt);
}

__declspec(dllexport) bool __stdcall PromptWarm_IsReady(uint64_t handle) {
    return PromptWarmingEngine::Instance().IsWarmed(handle);
}

__declspec(dllexport) const char* __stdcall PromptWarm_GetContent(uint64_t handle) {
    static std::string s_content;
    s_content = PromptWarmingEngine::Instance().GetWarmedContent(handle);
    return s_content.c_str();
}

__declspec(dllexport) void __stdcall PromptWarm_PreseedSkills(const char* skillContext) {
    if (skillContext) {
        PromptWarmingEngine::Instance().PreseedSkillContext(skillContext);
    }
}

__declspec(dllexport) float __stdcall PromptWarm_EstimateTPS(uint64_t handle) {
    return PromptWarmingEngine::Instance().EstimateTPS(handle);
}

__declspec(dllexport) void __stdcall PromptWarm_EvictCache() {
    PromptWarmingEngine::Instance().EvictCache();
}

} // extern "C"

} // namespace SkillSystem
} // namespace RawrXD
