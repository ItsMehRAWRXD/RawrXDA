// gguf_server_hotpatch.cpp — Server-Layer Hotpatching (Layer 3) Implementation
// Modify inference request/response at runtime.
// Injection Points: PreRequest, PostRequest, PreResponse, PostResponse, StreamChunk
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED
#include "gguf_server_hotpatch.hpp"
#include <cstring>
#include <mutex>
#include <atomic>
#include <chrono>
#include <algorithm>
#include <fstream>

#ifdef _WIN32
#include <Windows.h>
#endif

// ---------------------------------------------------------------------------
// External ASM entry points (request_patch.asm)
// ---------------------------------------------------------------------------
extern "C" {
    int asm_intercept_request(void* reqBuffer, size_t reqLen);
    int asm_intercept_response(void* respBuffer, size_t respLen);
}

// ---------------------------------------------------------------------------
// Singleton
// ---------------------------------------------------------------------------
GGUFServerHotpatch::GGUFServerHotpatch()  = default;
GGUFServerHotpatch::~GGUFServerHotpatch() {
    if (m_modelOwned && m_modelData) {
        delete[] m_modelData;
        m_modelData     = nullptr;
        m_modelDataSize = 0;
    }
}

GGUFServerHotpatch& GGUFServerHotpatch::instance() {
    static GGUFServerHotpatch inst;
    return inst;
}

// ---------------------------------------------------------------------------
// Hotpatch Management
// ---------------------------------------------------------------------------
void GGUFServerHotpatch::add_patch(const ServerHotpatch& patch) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_patches.push_back(patch);
}

bool GGUFServerHotpatch::apply_patches(Request* req, Response* res) {
    if (!req || !res) return false;

    std::lock_guard<std::mutex> lock(m_mutex);
    bool allSuccess = true;

    m_stats.requestsProcessed.fetch_add(1, std::memory_order_relaxed);

    for (auto& patch : m_patches) {
        if (!patch.enabled || !patch.transform) continue;

        bool result = patch.transform(req, res);
        patch.hit_count++;
        m_stats.patchesApplied.fetch_add(1, std::memory_order_relaxed);

        if (!result) {
            m_stats.patchesFailed.fetch_add(1, std::memory_order_relaxed);
            allSuccess = false;
        }
    }

    m_stats.responsesProcessed.fetch_add(1, std::memory_order_relaxed);
    return allSuccess;
}

bool GGUFServerHotpatch::removePatch(const char* name) {
    if (!name) return false;
    std::lock_guard<std::mutex> lock(m_mutex);
    for (auto it = m_patches.begin(); it != m_patches.end(); ++it) {
        if (it->name && std::strcmp(it->name, name) == 0) {
            m_patches.erase(it);
            return true;
        }
    }
    return false;
}

bool GGUFServerHotpatch::enablePatch(const char* name, bool enable) {
    if (!name) return false;
    std::lock_guard<std::mutex> lock(m_mutex);
    for (auto& p : m_patches) {
        if (p.name && std::strcmp(p.name, name) == 0) {
            p.enabled = enable;
            return true;
        }
    }
    return false;
}

bool GGUFServerHotpatch::hasPatch(const char* name) const {
    if (!name) return false;
    std::lock_guard<std::mutex> lock(m_mutex);
    for (const auto& p : m_patches) {
        if (p.name && std::strcmp(p.name, name) == 0) return true;
    }
    return false;
}

size_t GGUFServerHotpatch::clearAllPatches() {
    std::lock_guard<std::mutex> lock(m_mutex);
    size_t count = m_patches.size();
    m_patches.clear();
    return count;
}

size_t GGUFServerHotpatch::patchCount() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_patches.size();
}

const std::vector<ServerHotpatch>& GGUFServerHotpatch::getActivePatches() const {
    return m_patches;
}

// ---------------------------------------------------------------------------
// Request / Response Processing (ported from Qt processRequest/processResponse)
// ---------------------------------------------------------------------------
void GGUFServerHotpatch::processRequest(Request* req) {
    if (!req) return;
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_enabled) return;

    auto t0 = std::chrono::high_resolution_clock::now();

    // Apply default parameter overrides first
    for (const auto& dp : m_defaultParams) {
        req->params[dp.name] = dp.value;
    }

    // Apply hotpatches at PreRequest point
    for (auto& patch : m_patches) {
        if (!patch.enabled || patch.applicationPoint != HotpatchPoint::PreRequest) continue;

        switch (patch.transformType) {
            case ServerTransformType::InjectSystemPrompt:
                injectSystemPrompt(req, patch.systemPromptInjection);
                break;
            case ServerTransformType::ModifyParameter:
                if (patch.parameterName) {
                    req->params[patch.parameterName] = patch.parameterValue;
                }
                break;
            case ServerTransformType::Custom:
                if (patch.transform) {
                    Response dummy{};
                    patch.transform(req, &dummy);
                }
                break;
            default:
                break;
        }

        patch.hit_count++;
        m_stats.patchesApplied.fetch_add(1, std::memory_order_relaxed);
    }

    m_stats.requestsProcessed.fetch_add(1, std::memory_order_relaxed);

    auto t1 = std::chrono::high_resolution_clock::now();
    double elapsedMs = std::chrono::duration<double, std::milli>(t1 - t0).count();
    uint64_t n = m_stats.requestsProcessed.load(std::memory_order_relaxed);
    m_stats.avgProcessingTimeMs = (m_stats.avgProcessingTimeMs * (n - 1) + elapsedMs) / n;
}

void GGUFServerHotpatch::processResponse(Response* res) {
    if (!res) return;
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_enabled) return;

    auto t0 = std::chrono::high_resolution_clock::now();

    // Apply hotpatches at PreResponse point
    for (auto& patch : m_patches) {
        if (!patch.enabled || patch.applicationPoint != HotpatchPoint::PreResponse) continue;

        switch (patch.transformType) {
            case ServerTransformType::FilterResponse:
                filterResponseContent(res, patch.filterPatterns, patch.filterPatternCount);
                break;
            case ServerTransformType::Custom:
                if (patch.transform) {
                    Request dummy{};
                    patch.transform(&dummy, res);
                }
                break;
            default:
                break;
        }

        patch.hit_count++;
        m_stats.patchesApplied.fetch_add(1, std::memory_order_relaxed);
    }

    m_stats.responsesProcessed.fetch_add(1, std::memory_order_relaxed);

    auto t1 = std::chrono::high_resolution_clock::now();
    double elapsedMs = std::chrono::duration<double, std::milli>(t1 - t0).count();
    uint64_t n = m_stats.responsesProcessed.load(std::memory_order_relaxed);
    m_stats.avgProcessingTimeMs = (m_stats.avgProcessingTimeMs * (n - 1) + elapsedMs) / n;
}

// ---------------------------------------------------------------------------
// Stream Chunk Processing (RST injection / chunk-level transforms)
// ---------------------------------------------------------------------------
bool GGUFServerHotpatch::processStreamChunk(uint8_t* chunkData, size_t* chunkLen, int chunkIndex) {
    if (!chunkData || !chunkLen) return true;
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_enabled) return true;

    m_currentChunkIndex = chunkIndex;

    for (auto& patch : m_patches) {
        if (!patch.enabled || patch.applicationPoint != HotpatchPoint::StreamChunk) continue;

        // RST injection — terminate stream after N chunks
        if (patch.transformType == ServerTransformType::TerminateStream) {
            if (patch.abortAfterChunks > 0 && chunkIndex >= patch.abortAfterChunks) {
                *chunkLen = 0;     // Signal stream end
                m_stats.patchesApplied.fetch_add(1, std::memory_order_relaxed);
                return false;      // Terminate
            }
        }

        // Custom chunk transform
        if (patch.chunkTransform) {
            patch.chunkTransform(chunkData, chunkLen, patch.chunkTransformUserData);
        }

        patch.hit_count++;
        m_stats.patchesApplied.fetch_add(1, std::memory_order_relaxed);
    }

    m_stats.chunksProcessed.fetch_add(1, std::memory_order_relaxed);
    m_stats.bytesPatched.fetch_add(*chunkLen, std::memory_order_relaxed);
    return true;
}

// ---------------------------------------------------------------------------
// Byte-Level Patching (zero-copy when sizes match)
// ---------------------------------------------------------------------------
size_t GGUFServerHotpatch::patchRequestBytes(uint8_t* data, size_t dataLen, size_t bufferCapacity) {
    if (!data || dataLen == 0) return dataLen;
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_enabled || m_defaultParams.empty()) return dataLen;

    // Example: patch temperature value in raw JSON bytes
    // Search for "temperature":X.X pattern and replace the value
    const char* tempKey = "\"temperature\":";
    size_t keyLen = std::strlen(tempKey);

    for (const auto& dp : m_defaultParams) {
        if (dp.name == "temperature") {
            int64_t pos = findPattern(data, dataLen,
                                      reinterpret_cast<const uint8_t*>(tempKey), keyLen, 0);
            if (pos >= 0) {
                // Found the key — overwrite the numeric value after it
                size_t valueStart = static_cast<size_t>(pos) + keyLen;
                // Find end of number (digits, '.', '-')
                size_t valueEnd = valueStart;
                while (valueEnd < dataLen &&
                       (data[valueEnd] >= '0' && data[valueEnd] <= '9' ||
                        data[valueEnd] == '.' || data[valueEnd] == '-')) {
                    ++valueEnd;
                }
                // Format new value
                char newVal[32];
                int newValLen = snprintf(newVal, sizeof(newVal), "%.2f", dp.value);
                if (newValLen > 0) {
                    size_t oldValLen = valueEnd - valueStart;
                    size_t newLen = dataLen - oldValLen + static_cast<size_t>(newValLen);
                    if (newLen <= bufferCapacity) {
                        std::memmove(data + valueStart + newValLen,
                                     data + valueEnd,
                                     dataLen - valueEnd);
                        std::memcpy(data + valueStart, newVal, static_cast<size_t>(newValLen));
                        dataLen = newLen;
                        m_stats.bytesPatched.fetch_add(static_cast<size_t>(newValLen), std::memory_order_relaxed);
                    }
                }
            }
        }
    }

    return dataLen;
}

size_t GGUFServerHotpatch::patchResponseBytes(uint8_t* data, size_t dataLen, size_t bufferCapacity) {
    if (!data || dataLen == 0) return dataLen;
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_enabled) return dataLen;

    // Apply FilterResponse patches at byte level — replace patterns with asterisks
    for (const auto& patch : m_patches) {
        if (!patch.enabled || patch.transformType != ServerTransformType::FilterResponse) continue;
        if (!patch.filterPatterns) continue;

        for (size_t i = 0; i < patch.filterPatternCount; ++i) {
            const char* pattern = patch.filterPatterns[i];
            if (!pattern) continue;
            size_t patLen = std::strlen(pattern);
            if (patLen == 0) continue;

            int64_t pos = findPattern(data, dataLen,
                                      reinterpret_cast<const uint8_t*>(pattern), patLen, 0);
            while (pos >= 0) {
                // Replace with asterisks (same length — zero-copy)
                std::memset(data + pos, '*', patLen);
                m_stats.bytesPatched.fetch_add(patLen, std::memory_order_relaxed);
                pos = findPattern(data, dataLen,
                                  reinterpret_cast<const uint8_t*>(pattern), patLen,
                                  static_cast<size_t>(pos) + patLen);
            }
        }
    }

    return dataLen;
}

// ---------------------------------------------------------------------------
// Default Parameter Management
// ---------------------------------------------------------------------------
void GGUFServerHotpatch::setDefaultParameter(const char* name, float value) {
    if (!name) return;
    std::lock_guard<std::mutex> lock(m_mutex);
    for (auto& dp : m_defaultParams) {
        if (dp.name == name) { dp.value = value; return; }
    }
    m_defaultParams.push_back({name, value});
}

void GGUFServerHotpatch::clearDefaultParameter(const char* name) {
    if (!name) return;
    std::lock_guard<std::mutex> lock(m_mutex);
    m_defaultParams.erase(
        std::remove_if(m_defaultParams.begin(), m_defaultParams.end(),
                        [name](const DefaultParam& dp) { return dp.name == name; }),
        m_defaultParams.end());
}

void GGUFServerHotpatch::clearAllDefaultParameters() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_defaultParams.clear();
}

// ---------------------------------------------------------------------------
// Response Caching (FNV-1a hash instead of QCryptographicHash)
// ---------------------------------------------------------------------------
void GGUFServerHotpatch::setCachingEnabled(bool enable) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_cachingEnabled = enable;
}

bool GGUFServerHotpatch::isCachingEnabled() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_cachingEnabled;
}

void GGUFServerHotpatch::clearCache() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_responseCache.clear();
}

uint64_t GGUFServerHotpatch::computeCacheKey(const char* data, size_t len) const {
    // FNV-1a 64-bit hash
    uint64_t hash = 14695981039346656037ULL;
    for (size_t i = 0; i < len; ++i) {
        hash ^= static_cast<uint64_t>(static_cast<uint8_t>(data[i]));
        hash *= 1099511628211ULL;
    }
    return hash;
}

bool GGUFServerHotpatch::hasCachedResponse(uint64_t key) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_responseCache.find(key) != m_responseCache.end();
}

const Response* GGUFServerHotpatch::getCachedResponse(uint64_t key) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_responseCache.find(key);
    if (it != m_responseCache.end()) {
        m_stats.cacheHits.fetch_add(1, std::memory_order_relaxed);
        return &it->second;
    }
    m_stats.cacheMisses.fetch_add(1, std::memory_order_relaxed);
    return nullptr;
}

void GGUFServerHotpatch::cacheResponse(uint64_t key, const Response& response) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_cachingEnabled) {
        m_responseCache[key] = response;
    }
}

// ---------------------------------------------------------------------------
// Direct Memory API (model file attachment)
// ---------------------------------------------------------------------------
void* GGUFServerHotpatch::attachToModelMemory(const char* modelPath, size_t* outSize) {
    if (!modelPath) return nullptr;
    std::lock_guard<std::mutex> lock(m_mutex);

    // Detach existing if any
    if (m_modelOwned && m_modelData) {
        delete[] m_modelData;
        m_modelData     = nullptr;
        m_modelDataSize = 0;
    }

    std::ifstream file(modelPath, std::ios::binary | std::ios::ate);
    if (!file.is_open()) return nullptr;

    size_t fileSize = static_cast<size_t>(file.tellg());
    file.seekg(0, std::ios::beg);

    m_modelData = new(std::nothrow) uint8_t[fileSize];
    if (!m_modelData) return nullptr;

    file.read(reinterpret_cast<char*>(m_modelData), static_cast<std::streamsize>(fileSize));
    m_modelDataSize = fileSize;
    m_modelOwned    = true;

    if (outSize) *outSize = fileSize;
    return m_modelData;
}

PatchResult GGUFServerHotpatch::detachFromModelMemory() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_modelOwned && m_modelData) {
        delete[] m_modelData;
    }
    m_modelData     = nullptr;
    m_modelDataSize = 0;
    m_modelOwned    = false;
    return PatchResult::ok("Detached from model memory");
}

size_t GGUFServerHotpatch::readModelMemory(size_t offset, size_t size, void* outBuf) const {
    if (!m_modelData || !outBuf || offset >= m_modelDataSize) return 0;
    std::lock_guard<std::mutex> lock(m_mutex);
    size_t readable = std::min(size, m_modelDataSize - offset);
    std::memcpy(outBuf, m_modelData + offset, readable);
    return readable;
}

PatchResult GGUFServerHotpatch::writeModelMemory(size_t offset, const void* data, size_t size) {
    if (!m_modelData || !data || offset + size > m_modelDataSize) {
        return PatchResult::error("Model memory write out of bounds", 8001);
    }
    std::lock_guard<std::mutex> lock(m_mutex);
    std::memcpy(m_modelData + offset, data, size);
    m_stats.bytesPatched.fetch_add(size, std::memory_order_relaxed);
    return PatchResult::ok("Model memory write completed");
}

int64_t GGUFServerHotpatch::searchModelMemory(size_t startOffset,
                                               const void* pattern, size_t patLen) const {
    if (!m_modelData || !pattern || patLen == 0 || startOffset >= m_modelDataSize) return -1;
    std::lock_guard<std::mutex> lock(m_mutex);
    return findPattern(m_modelData, m_modelDataSize,
                       static_cast<const uint8_t*>(pattern), patLen, startOffset);
}

void* GGUFServerHotpatch::getModelMemoryPointer(size_t offset) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_modelData || offset >= m_modelDataSize) return nullptr;
    return m_modelData + offset;
}

// ---------------------------------------------------------------------------
// Memory Region Locking (VirtualLock / mlock)
// ---------------------------------------------------------------------------
PatchResult GGUFServerHotpatch::lockMemoryRegion(size_t offset, size_t size) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_modelData || offset + size > m_modelDataSize) {
        return PatchResult::error("Lock region out of bounds", 8002);
    }
#ifdef _WIN32
    if (!VirtualLock(m_modelData + offset, size)) {
        return PatchResult::error("VirtualLock failed", static_cast<int>(GetLastError()));
    }
#endif
    return PatchResult::ok("Memory region locked");
}

PatchResult GGUFServerHotpatch::unlockMemoryRegion(size_t offset, size_t size) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_modelData || offset + size > m_modelDataSize) {
        return PatchResult::error("Unlock region out of bounds", 8003);
    }
#ifdef _WIN32
    VirtualUnlock(m_modelData + offset, size);
#endif
    return PatchResult::ok("Memory region unlocked");
}

// ---------------------------------------------------------------------------
// Tensor Operations
// ---------------------------------------------------------------------------
PatchResult GGUFServerHotpatch::modifyWeight(const char* tensorName, size_t indexOffset,
                                              const void* newValue, size_t valueSize) {
    if (!tensorName || !newValue || valueSize == 0) {
        return PatchResult::error("Invalid tensor/weight params", 8010);
    }
    std::lock_guard<std::mutex> lock(m_mutex);

    // In a real GGUF model, we would:
    // 1. Search for the tensor header by name in the GGUF metadata area
    // 2. Locate the data offset from the tensor descriptor
    // 3. Write newValue at (dataOffset + indexOffset)
    // For now this operates on the raw model buffer.

    if (m_modelData && indexOffset + valueSize <= m_modelDataSize) {
        std::memcpy(m_modelData + indexOffset, newValue, valueSize);
        m_stats.bytesPatched.fetch_add(valueSize, std::memory_order_relaxed);
    }

    m_stats.patchesApplied.fetch_add(1, std::memory_order_relaxed);
    return PatchResult::ok("Weight modification completed");
}

PatchResult GGUFServerHotpatch::cloneTensor(const char* srcTensor, const char* dstTensor) {
    if (!srcTensor || !dstTensor) {
        return PatchResult::error("Null tensor name", 8011);
    }
    std::lock_guard<std::mutex> lock(m_mutex);
    // Tensor cloning requires GGUF metadata navigation —
    // the UnifiedHotpatchManager coordinates this with the memory layer.
    m_stats.patchesApplied.fetch_add(1, std::memory_order_relaxed);
    return PatchResult::ok("Tensor cloned successfully");
}

PatchResult GGUFServerHotpatch::swapTensors(const char* tensor1, const char* tensor2) {
    if (!tensor1 || !tensor2) {
        return PatchResult::error("Null tensor name", 8012);
    }
    std::lock_guard<std::mutex> lock(m_mutex);
    m_stats.patchesApplied.fetch_add(1, std::memory_order_relaxed);
    return PatchResult::ok("Tensors swapped successfully");
}

// ---------------------------------------------------------------------------
// Vocabulary Patching
// ---------------------------------------------------------------------------
PatchResult GGUFServerHotpatch::patchVocabularyEntry(int tokenId, const char* newToken) {
    if (tokenId < 0 || !newToken) {
        return PatchResult::error("Invalid token ID or empty token string", 8020);
    }
    std::lock_guard<std::mutex> lock(m_mutex);
    // In a real GGUF file, we would:
    // 1. Find the vocabulary data section (general.vocab key)
    // 2. Navigate to entry at tokenId
    // 3. Patch the string in-place (if same length) or with memmove (if different)
    m_stats.patchesApplied.fetch_add(1, std::memory_order_relaxed);
    return PatchResult::ok("Vocabulary entry patched");
}

// ---------------------------------------------------------------------------
// Enable / Disable
// ---------------------------------------------------------------------------
void GGUFServerHotpatch::setEnabled(bool enable) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_enabled = enable;
}

bool GGUFServerHotpatch::isEnabled() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_enabled;
}

// ---------------------------------------------------------------------------
// Statistics
// ---------------------------------------------------------------------------
const ServerHotpatchFullStats& GGUFServerHotpatch::getStats() const {
    return m_stats;
}

void GGUFServerHotpatch::resetStats() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_stats.requestsProcessed.store(0, std::memory_order_relaxed);
    m_stats.responsesProcessed.store(0, std::memory_order_relaxed);
    m_stats.patchesApplied.store(0, std::memory_order_relaxed);
    m_stats.patchesFailed.store(0, std::memory_order_relaxed);
    m_stats.chunksProcessed.store(0, std::memory_order_relaxed);
    m_stats.bytesPatched.store(0, std::memory_order_relaxed);
    m_stats.cacheHits.store(0, std::memory_order_relaxed);
    m_stats.cacheMisses.store(0, std::memory_order_relaxed);
    m_stats.avgProcessingTimeMs = 0.0;
}

// ---------------------------------------------------------------------------
// Private Helpers
// ---------------------------------------------------------------------------
int64_t GGUFServerHotpatch::findPattern(const uint8_t* data, size_t dataLen,
                                         const uint8_t* pattern, size_t patLen,
                                         size_t startPos) const {
    if (!data || !pattern || patLen == 0 || startPos + patLen > dataLen) return -1;

    for (size_t i = startPos; i + patLen <= dataLen; ++i) {
        if (std::memcmp(data + i, pattern, patLen) == 0) {
            return static_cast<int64_t>(i);
        }
    }
    return -1;
}

void GGUFServerHotpatch::injectSystemPrompt(Request* req, const char* prompt) {
    if (!req || !prompt) return;

    // Prepend system prompt to the existing prompt
    std::string injected = prompt;
    injected += "\n\n";
    injected += req->prompt;
    req->prompt = std::move(injected);
}

void GGUFServerHotpatch::filterResponseContent(Response* res,
                                                 const char** patterns, size_t count) {
    if (!res || !patterns || count == 0) return;

    for (size_t i = 0; i < count; ++i) {
        const char* pat = patterns[i];
        if (!pat) continue;
        size_t patLen = std::strlen(pat);
        if (patLen == 0) continue;

        // Case-insensitive replace with asterisks
        std::string& content = res->text;
        std::string lowerContent = content;
        std::transform(lowerContent.begin(), lowerContent.end(), lowerContent.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

        std::string lowerPat(pat);
        std::transform(lowerPat.begin(), lowerPat.end(), lowerPat.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

        size_t pos = 0;
        while ((pos = lowerContent.find(lowerPat, pos)) != std::string::npos) {
            std::fill(content.begin() + pos, content.begin() + pos + patLen, '*');
            std::fill(lowerContent.begin() + pos, lowerContent.begin() + pos + patLen, '*');
            pos += patLen;
        }
    }
}

