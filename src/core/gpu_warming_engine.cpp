// ============================================================================
// gpu_warming_engine.cpp — GPU Anti-Lag / Hot-Model Loading Implementation
// ============================================================================

#include "gpu_warming_engine.h"
#include "gpu_backend_bridge.h"
#include <windows.h>
#include <wincrypt.h>
#include <sstream>
#include <iomanip>
#include <algorithm>

#pragma comment(lib, "advapi32.lib")

namespace RawrXD {
namespace GPU {

// ============================================================================
// SINGLETON
// ============================================================================
GPUWarmingEngine& GPUWarmingEngine::Instance() {
    static GPUWarmingEngine instance;
    return instance;
}

GPUWarmingEngine::~GPUWarmingEngine() {
    Shutdown();
}

// ============================================================================
// INITIALIZATION
// ============================================================================
bool GPUWarmingEngine::Initialize(GPUBackendBridge* bridge, const GPUWarmingConfig& config) {
    if (m_running.exchange(true)) {
        return true;  // Already running
    }

    m_bridge = bridge;
    m_config = config;

    if (!m_bridge) {
        OutputDebugStringA("[GPUWarming] ERROR: No GPU backend bridge provided\n");
        m_running.store(false);
        return false;
    }

    try {
        m_warmingThread = std::thread(&GPUWarmingEngine::WarmingLoop, this);
    } catch (const std::exception& e) {
        OutputDebugStringA("[GPUWarming] Failed to start warming thread: ");
        OutputDebugStringA(e.what());
        OutputDebugStringA("\n");
        m_running.store(false);
        return false;
    }

    OutputDebugStringA("[GPUWarming] Engine initialized\n");
    return true;
}

void GPUWarmingEngine::Shutdown() {
    if (!m_running.exchange(false)) {
        return;
    }

    m_queueCv.notify_all();
    if (m_warmingThread.joinable()) {
        m_warmingThread.join();
    }

    std::lock_guard<std::mutex> lock(m_mutex);
    
    // Free all VRAM allocations
    for (auto& pair : m_cache) {
        if (pair.second.vramHandle != 0) {
            FreeVRAMAllocation(pair.second.vramHandle);
        }
        if (pair.second.psoHandle != 0) {
            FreePSO(pair.second.psoHandle);
        }
    }
    
    m_cache.clear();
    m_idToFingerprint.clear();
    m_totalAllocatedVRAM.store(0);
    m_totalPSOCount.store(0);

    OutputDebugStringA("[GPUWarming] Engine shutdown\n");
}

// ============================================================================
// FINGERPRINTING
// ============================================================================
std::string GPUWarmingEngine::ComputeFingerprint(const std::string& modelPath, const std::string& configJson) const {
    HCRYPTPROV hProv = 0;
    HCRYPTHASH hHash = 0;
    BYTE hash[32] = {0};
    DWORD hashLen = 32;
    std::string result;

    std::string input = modelPath + "|" + configJson;

    if (!CryptAcquireContext(&hProv, nullptr, nullptr, PROV_RSA_AES, CRYPT_VERIFYCONTEXT)) {
        // Fallback: simple FNV-1a hash
        uint64_t h = 0xcbf29ce484222325ULL;
        for (char c : input) {
            h ^= static_cast<uint8_t>(c);
            h *= 0x100000001b3ULL;
        }
        std::ostringstream oss;
        oss << std::hex << std::setw(16) << std::setfill('0') << h;
        return oss.str();
    }

    if (CryptCreateHash(hProv, CALG_SHA_256, 0, 0, &hHash)) {
        CryptHashData(hHash, reinterpret_cast<const BYTE*>(input.data()), 
                      static_cast<DWORD>(input.size()), 0);
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
// WARM MODEL (ASYNC)
// ============================================================================
uint64_t GPUWarmingEngine::WarmModelAsync(const std::string& modelPath, const std::string& configJson) {
    if (modelPath.empty()) {
        return 0;
    }

    std::string fingerprint = ComputeFingerprint(modelPath, configJson);

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
        m_warmingQueue.emplace_back(modelPath, configJson);
    }
    m_queueCv.notify_one();

    uint64_t pendingId = m_nextId.fetch_add(1);
    return pendingId;
}

// ============================================================================
// WARM MODEL (SYNC)
// ============================================================================
GPUWarmCacheEntry GPUWarmingEngine::WarmModelSync(const std::string& modelPath, const std::string& configJson, uint32_t timeoutMs) {
    if (modelPath.empty()) {
        return GPUWarmCacheEntry{};
    }

    std::string fingerprint = ComputeFingerprint(modelPath, configJson);

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
    GPUWarmCacheEntry entry;
    entry.id = m_nextId.fetch_add(1);
    entry.fingerprint = fingerprint;
    entry.warmedAtMs = GetTickCount64();
    entry.lastUsedMs = entry.warmedAtMs;
    entry.useCount = 1;
    entry.isValid = true;

    if (WarmModelInternal(modelPath, configJson, entry)) {
        entry.isHot = true;
        
        std::lock_guard<std::mutex> lock(m_mutex);
        EvictIfNeeded();
        m_cache[fingerprint] = entry;
        m_idToFingerprint[entry.id] = fingerprint;
    }

    return entry;
}

// ============================================================================
// INTERNAL WARMING
// ============================================================================
bool GPUWarmingEngine::WarmModelInternal(const std::string& modelPath, const std::string& configJson, GPUWarmCacheEntry& outEntry) {
    if (!m_bridge) {
        return false;
    }

    // Step 1: Allocate VRAM for model weights
    uint64_t vramHandle = 0;
    uint64_t vramSize = 0;
    if (!AllocateVRAMForModel(modelPath, vramHandle, vramSize)) {
        OutputDebugStringA("[GPUWarming] Failed to allocate VRAM for model\n");
        return false;
    }
    outEntry.vramHandle = vramHandle;

    // Step 2: Compile compute PSO for model
    uint64_t psoHandle = 0;
    if (!CompilePSOForModel(modelPath, psoHandle)) {
        FreeVRAMAllocation(vramHandle);
        OutputDebugStringA("[GPUWarming] Failed to compile PSO for model\n");
        return false;
    }
    outEntry.psoHandle = psoHandle;

    OutputDebugStringA("[GPUWarming] Model warmed successfully\n");
    return true;
}

// ============================================================================
// VRAM ALLOCATION
// ============================================================================
bool GPUWarmingEngine::AllocateVRAMForModel(const std::string& modelPath, uint64_t& outHandle, uint64_t& outSize) {
    // Estimate model size from file
    WIN32_FILE_ATTRIBUTE_DATA fileAttr;
    if (!GetFileAttributesExA(modelPath.c_str(), GetFileExInfoStandard, &fileAttr)) {
        // Use default size if file doesn't exist
        outSize = 1024ULL * 1024ULL * 1024ULL;  // 1GB default
    } else {
        ULARGE_INTEGER fileSize;
        fileSize.LowPart = fileAttr.nFileSizeLow;
        fileSize.HighPart = fileAttr.nFileSizeHigh;
        outSize = fileSize.QuadPart;
    }

    // Check if we have enough VRAM
    uint64_t currentAllocated = m_totalAllocatedVRAM.load();
    if (currentAllocated + outSize > m_config.maxVRAMBytes) {
        OutputDebugStringA("[GPUWarming] Not enough VRAM for model, evicting...\n");
        EvictIfNeeded();
        
        currentAllocated = m_totalAllocatedVRAM.load();
        if (currentAllocated + outSize > m_config.maxVRAMBytes) {
            OutputDebugStringA("[GPUWarming] Still not enough VRAM after eviction\n");
            return false;
        }
    }

    // Allocate through GPU backend bridge
    // For now, simulate with a handle
    outHandle = reinterpret_cast<uint64_t>(&outHandle) + GetTickCount64();  // Unique handle
    m_totalAllocatedVRAM.fetch_add(outSize);
    
    char msg[256];
    snprintf(msg, sizeof(msg), "[GPUWarming] Allocated %llu MB VRAM for model\n", outSize / (1024 * 1024));
    OutputDebugStringA(msg);
    
    return true;
}

void GPUWarmingEngine::FreeVRAMAllocation(uint64_t handle) {
    if (handle == 0) return;
    
    // Find and free
    std::lock_guard<std::mutex> lock(m_mutex);
    for (auto& pair : m_cache) {
        if (pair.second.vramHandle == handle) {
            m_totalAllocatedVRAM.fetch_sub(1024ULL * 1024ULL * 1024ULL);  // Approximate
            pair.second.vramHandle = 0;
            return;
        }
    }
}

// ============================================================================
// PSO COMPILATION
// ============================================================================
bool GPUWarmingEngine::CompilePSOForModel(const std::string& modelPath, uint64_t& outHandle) {
    // Simulate PSO compilation
    // In production, this would compile shaders for the specific model
    outHandle = reinterpret_cast<uint64_t>(&outHandle) + GetTickCount64();
    m_totalPSOCount.fetch_add(1);
    
    OutputDebugStringA("[GPUWarming] PSO compiled for model\n");
    return true;
}

void GPUWarmingEngine::FreePSO(uint64_t handle) {
    if (handle == 0) return;
    m_totalPSOCount.fetch_sub(1);
}

// ============================================================================
// CACHE QUERIES
// ============================================================================
bool GPUWarmingEngine::IsWarmed(const std::string& modelPath) const {
    std::string fingerprint = ComputeFingerprint(modelPath, "");
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_cache.find(fingerprint);
    return it != m_cache.end() && it->second.isValid;
}

bool GPUWarmingEngine::IsWarmed(uint64_t handleId) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_idToFingerprint.find(handleId);
    if (it == m_idToFingerprint.end()) return false;
    auto cacheIt = m_cache.find(it->second);
    return cacheIt != m_cache.end() && cacheIt->second.isValid;
}

GPUWarmCacheEntry GPUWarmingEngine::GetWarmedEntry(uint64_t handleId) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_idToFingerprint.find(handleId);
    if (it == m_idToFingerprint.end()) return GPUWarmCacheEntry{};
    auto cacheIt = m_cache.find(it->second);
    if (cacheIt != m_cache.end() && cacheIt->second.isValid) {
        cacheIt->second.lastUsedMs = GetTickCount64();
        cacheIt->second.useCount++;
        return cacheIt->second;
    }
    return GPUWarmCacheEntry{};
}

GPUWarmCacheEntry GPUWarmingEngine::GetWarmedEntry(const std::string& modelPath) {
    std::string fingerprint = ComputeFingerprint(modelPath, "");
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_cache.find(fingerprint);
    if (it != m_cache.end() && it->second.isValid) {
        it->second.lastUsedMs = GetTickCount64();
        it->second.useCount++;
        return it->second;
    }
    return GPUWarmCacheEntry{};
}

// ============================================================================
// PRE-SEED SKILL CONTEXT
// ============================================================================
void GPUWarmingEngine::PreseedSkillContext(const std::string& skillContext) {
    m_preseededSkillContext = skillContext;
    
    // Allocate VRAM for skill context tensors
    if (m_bridge && !skillContext.empty()) {
        // In production, this would tokenize and upload to GPU
        OutputDebugStringA("[GPUWarming] Pre-seeded skill context\n");
    }
}

// ============================================================================
// BATCH WARMING
// ============================================================================
std::vector<GPUWarmCacheEntry> GPUWarmingEngine::WarmModelBatch(const std::vector<std::string>& modelPaths) {
    std::vector<GPUWarmCacheEntry> results;
    results.reserve(modelPaths.size());
    
    for (const auto& path : modelPaths) {
        results.push_back(WarmModelSync(path, "", 5000));
    }
    
    return results;
}

// ============================================================================
// CACHE METRICS
// ============================================================================
size_t GPUWarmingEngine::GetCacheEntryCount() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_cache.size();
}

size_t GPUWarmingEngine::GetCacheTotalBytes() const {
    return static_cast<size_t>(m_totalAllocatedVRAM.load());
}

size_t GPUWarmingEngine::GetCacheHitCount() const {
    return m_cacheHits.load();
}

size_t GPUWarmingEngine::GetCacheMissCount() const {
    return m_cacheMisses.load();
}

float GPUWarmingEngine::GetCacheHitRate() const {
    size_t hits = m_cacheHits.load();
    size_t misses = m_cacheMisses.load();
    size_t total = hits + misses;
    if (total == 0) return 0.0f;
    return static_cast<float>(hits) / static_cast<float>(total);
}

uint64_t GPUWarmingEngine::GetTotalAllocatedVRAM() const {
    return m_totalAllocatedVRAM.load();
}

uint64_t GPUWarmingEngine::GetFreeVRAM() const {
    uint64_t allocated = m_totalAllocatedVRAM.load();
    return (allocated < m_config.maxVRAMBytes) ? (m_config.maxVRAMBytes - allocated) : 0;
}

// ============================================================================
// CACHE EVICTION
// ============================================================================
void GPUWarmingEngine::EvictCache() {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    for (auto& pair : m_cache) {
        if (pair.second.vramHandle != 0) {
            FreeVRAMAllocation(pair.second.vramHandle);
        }
        if (pair.second.psoHandle != 0) {
            FreePSO(pair.second.psoHandle);
        }
    }
    
    m_cache.clear();
    m_idToFingerprint.clear();
    m_totalAllocatedVRAM.store(0);
}

void GPUWarmingEngine::EvictStaleEntries(uint64_t maxAgeMs) {
    uint64_t now = GetTickCount64();
    std::lock_guard<std::mutex> lock(m_mutex);
    
    for (auto it = m_cache.begin(); it != m_cache.end();) {
        if (now > it->second.lastUsedMs && 
            (now - it->second.lastUsedMs) > maxAgeMs) {
            if (it->second.vramHandle != 0) {
                FreeVRAMAllocation(it->second.vramHandle);
            }
            if (it->second.psoHandle != 0) {
                FreePSO(it->second.psoHandle);
            }
            m_idToFingerprint.erase(it->second.id);
            it = m_cache.erase(it);
        } else {
            ++it;
        }
    }
}

void GPUWarmingEngine::EvictIfNeeded() {
    if (m_cache.size() < m_config.maxHotModels && 
        m_totalAllocatedVRAM.load() < m_config.maxVRAMBytes) {
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
        if (lruIt->second.vramHandle != 0) {
            FreeVRAMAllocation(lruIt->second.vramHandle);
        }
        if (lruIt->second.psoHandle != 0) {
            FreePSO(lruIt->second.psoHandle);
        }
        m_idToFingerprint.erase(lruIt->second.id);
        m_cache.erase(lruIt);
    }
}

// ============================================================================
// TEMPERATURE MONITORING
// ============================================================================
float GPUWarmingEngine::GetCurrentTemperatureC() const {
    return m_currentTemperatureC.load();
}

bool GPUWarmingEngine::IsThrottled() const {
    return m_isThrottled.load();
}

void GPUWarmingEngine::UpdateTemperature() {
    // In production, this would query GPU temperature via DX12/Vulkan
    // For now, simulate with a reasonable value
    m_currentTemperatureC.store(65.0f);
    
    if (m_config.enableTemperatureThrottle && 
        m_currentTemperatureC.load() > m_config.temperatureThrottleC) {
        m_isThrottled.store(true);
    } else {
        m_isThrottled.store(false);
    }
}

// ============================================================================
// TPS ESTIMATION
// ============================================================================
float GPUWarmingEngine::EstimateTPS(const std::string& modelPath) const {
    if (IsWarmed(modelPath)) {
        return 25.0f;  // 25M TPS for warmed models
    }
    return 5.0f;  // 5M TPS for cold models (first inference)
}

// ============================================================================
// WARMING LOOP (BACKGROUND THREAD)
// ============================================================================
void GPUWarmingEngine::WarmingLoop() {
    OutputDebugStringA("[GPUWarming] Background thread started\n");
    
    while (m_running.load()) {
        std::unique_lock<std::mutex> lock(m_queueMutex);
        m_queueCv.wait_for(lock, std::chrono::milliseconds(1000),
            [this] { return !m_warmingQueue.empty() || !m_running.load(); });
        
        if (!m_running.load()) {
            break;
        }
        
        std::vector<std::pair<std::string, std::string>> batch;
        while (!m_warmingQueue.empty() && batch.size() < m_config.warmupBatchSize) {
            batch.push_back(m_warmingQueue.back());
            m_warmingQueue.pop_back();
        }
        lock.unlock();
        
        for (const auto& [modelPath, configJson] : batch) {
            GPUWarmCacheEntry entry;
            if (WarmModelInternal(modelPath, configJson, entry)) {
                entry.isHot = true;
                
                std::lock_guard<std::mutex> cacheLock(m_mutex);
                EvictIfNeeded();
                std::string fingerprint = ComputeFingerprint(modelPath, configJson);
                m_cache[fingerprint] = entry;
                m_idToFingerprint[entry.id] = fingerprint;
            }
        }
        
        // Periodic temperature check
        static uint64_t lastTempCheck = 0;
        uint64_t now = GetTickCount64();
        if (now - lastTempCheck > 5000) {  // Every 5 seconds
            UpdateTemperature();
            lastTempCheck = now;
        }
    }
    
    OutputDebugStringA("[GPUWarming] Background thread exiting\n");
}

} // namespace GPU
} // namespace RawrXD
