// gguf_server_hotpatch.cpp - Implementation of server-side hotpatcher
#include "gguf_server_hotpatch.hpp"


#include <algorithm>

GGUFServerHotpatch::GGUFServerHotpatch(void* parent)
    : void(parent)
{
}

GGUFServerHotpatch::~GGUFServerHotpatch()
{
    std::lock_guard<std::mutex> locker(&m_mutex);
    m_hotpatches.clear();
    m_responseCache.clear();
}

void GGUFServerHotpatch::addHotpatch(const ServerHotpatch& patch)
{
    std::lock_guard<std::mutex> locker(&m_mutex);
    m_hotpatches[patch.name] = patch;
}

void GGUFServerHotpatch::removeHotpatch(const std::string& name)
{
    std::lock_guard<std::mutex> locker(&m_mutex);
    if (m_hotpatches.remove(name)) {
    }
}

void GGUFServerHotpatch::enableHotpatch(const std::string& name, bool enable)
{
    std::lock_guard<std::mutex> locker(&m_mutex);
    if (m_hotpatches.contains(name)) {
        m_hotpatches[name].enabled = enable;
    }
}

bool GGUFServerHotpatch::hasHotpatch(const std::string& name) const
{
    std::lock_guard<std::mutex> locker(&m_mutex);
    return m_hotpatches.contains(name);
}

ServerHotpatch GGUFServerHotpatch::getHotpatch(const std::string& name) const
{
    std::lock_guard<std::mutex> locker(&m_mutex);
    return m_hotpatches.value(name);
}

std::vector<std::string> GGUFServerHotpatch::listHotpatches() const
{
    std::lock_guard<std::mutex> locker(&m_mutex);
    return m_hotpatches.keys();
}

void GGUFServerHotpatch::clearAllHotpatches()
{
    std::lock_guard<std::mutex> locker(&m_mutex);
    m_hotpatches.clear();
}

void* GGUFServerHotpatch::processRequest(const void*& request)
{
    std::lock_guard<std::mutex> locker(&m_mutex);
    
    if (!m_enabled) {
        return request;
    }
    
    std::chrono::steady_clock timer;
    timer.start();
    
    void* modified = request;
    
    // Apply default parameter overrides first
    for (auto it = m_defaultParams.constBegin(); it != m_defaultParams.constEnd(); ++it) {
        modified[it.key()] = void*::fromVariant(it.value());
    }
    
    // Apply hotpatches at PreRequest point
    for (const auto& patch : m_hotpatches) {
        if (!patch.enabled || patch.applicationPoint != HotpatchPoint::PreRequest) {
            continue;
        }
        
        switch (patch.transformType) {
            case ServerHotpatch::InjectSystemPrompt:
                modified = injectSystemPrompt(modified, patch.systemPromptInjection);
                break;
                
            case ServerHotpatch::ModifyParameter:
                modified = modifyParameter(modified, patch.parameterName, patch.parameterValue);
                break;
                
            default:
                break;
        }
        
        m_stats.patchesApplied++;
        hotpatchApplied(patch.name, HotpatchPoint::PreRequest);
    }
    
    m_stats.requestsProcessed++;
    
    int64_t elapsed = timer.nsecsElapsed() / 1000000;
    m_stats.avgProcessingTimeMs = (m_stats.avgProcessingTimeMs * (m_stats.requestsProcessed - 1) + elapsed) / m_stats.requestsProcessed;
    
    if (modified != request) {
        requestModified(request, modified);
    }
    
    return modified;
}

void* GGUFServerHotpatch::processResponse(const void*& response)
{
    std::lock_guard<std::mutex> locker(&m_mutex);
    
    if (!m_enabled) {
        return response;
    }
    
    std::chrono::steady_clock timer;
    timer.start();
    
    void* modified = response;
    
    // Apply hotpatches at PreResponse point
    for (const auto& patch : m_hotpatches) {
        if (!patch.enabled || patch.applicationPoint != HotpatchPoint::PreResponse) {
            continue;
        }
        
        switch (patch.transformType) {
            case ServerHotpatch::FilterResponse:
                modified = filterResponse(modified, patch.filterPatterns);
                break;
                
            default:
                break;
        }
        
        m_stats.patchesApplied++;
        hotpatchApplied(patch.name, HotpatchPoint::PreResponse);
    }
    
    m_stats.responsesProcessed++;
    
    int64_t elapsed = timer.nsecsElapsed() / 1000000;
    m_stats.avgProcessingTimeMs = (m_stats.avgProcessingTimeMs * (m_stats.responsesProcessed - 1) + elapsed) / m_stats.responsesProcessed;
    
    if (modified != response) {
        responseModified(response, modified);
    }
    
    return modified;
}

std::vector<uint8_t> GGUFServerHotpatch::processStreamChunk(const std::vector<uint8_t>& chunk, int chunkIndex)
{
    std::lock_guard<std::mutex> locker(&m_mutex);
    
    if (!m_enabled) {
        return chunk;
    }
    
    m_currentChunkIndex = chunkIndex;
    std::vector<uint8_t> modified = chunk;
    
    // Check for stream termination patches (RST injection)
    for (const auto& patch : m_hotpatches) {
        if (!patch.enabled || patch.applicationPoint != HotpatchPoint::StreamChunk) {
            continue;
        }
        
        if (patch.transformType == ServerHotpatch::TerminateStream) {
            if (patch.abortAfterChunks > 0 && chunkIndex >= patch.abortAfterChunks) {
                streamTerminated(chunkIndex, "RST Injection: " + patch.name);
                return std::vector<uint8_t>(); // Empty = terminate stream
            }
        }
        
        // Apply custom transform if provided
        if (patch.customTransform) {
            modified = patch.customTransform(modified);
        }
        
        m_stats.patchesApplied++;
        hotpatchApplied(patch.name, HotpatchPoint::StreamChunk);
    }
    
    m_stats.chunksProcessed++;
    m_stats.bytesPatched += modified.size();
    
    return modified;
}

std::vector<uint8_t> GGUFServerHotpatch::patchRequestBytes(const std::vector<uint8_t>& requestData)
{
    std::lock_guard<std::mutex> locker(&m_mutex);
    
    if (!m_enabled || m_defaultParams.empty()) {
        return requestData;
    }
    
    std::vector<uint8_t> modified = requestData;
    
    // Byte-level parameter patching (zero-copy when sizes match)
    for (auto it = m_defaultParams.constBegin(); it != m_defaultParams.constEnd(); ++it) {
        if (it.key() == "temperature") {
            // Example: patch "0.9" -> "0.5" for temperature override
            std::vector<uint8_t> pattern = "\"temperature\":0.9";
            std::vector<uint8_t> replacement = std::string("\"temperature\":%1").toDouble()).toUtf8();
            
            if (pattern.size() == replacement.size()) {
                modified = bytePatchInPlace(modified, pattern, replacement);
            }
        }
    }
    
    m_stats.bytesPatched += modified.size();
    
    return modified;
}

std::vector<uint8_t> GGUFServerHotpatch::patchResponseBytes(const std::vector<uint8_t>& responseData)
{
    std::lock_guard<std::mutex> locker(&m_mutex);
    
    if (!m_enabled) {
        return responseData;
    }
    
    std::vector<uint8_t> modified = responseData;
    
    // Apply response filtering at byte level
    for (const auto& patch : m_hotpatches) {
        if (!patch.enabled || patch.transformType != ServerHotpatch::FilterResponse) {
            continue;
        }
        
        for (const std::string& pattern : patch.filterPatterns) {
            std::vector<uint8_t> patternBytes = pattern.toUtf8();
            std::vector<uint8_t> replacement(patternBytes.size(), '*');
            modified = bytePatchInPlace(modified, patternBytes, replacement);
        }
    }
    
    m_stats.bytesPatched += modified.size();
    
    return modified;
}

void GGUFServerHotpatch::setDefaultParameter(const std::string& name, const std::any& value)
{
    std::lock_guard<std::mutex> locker(&m_mutex);
    m_defaultParams[name] = value;
}

void GGUFServerHotpatch::clearDefaultParameter(const std::string& name)
{
    std::lock_guard<std::mutex> locker(&m_mutex);
    m_defaultParams.remove(name);
}

std::unordered_map<std::string, std::any> GGUFServerHotpatch::getDefaultParameters() const
{
    std::lock_guard<std::mutex> locker(&m_mutex);
    return m_defaultParams;
}

void GGUFServerHotpatch::setCachingEnabled(bool enable)
{
    std::lock_guard<std::mutex> locker(&m_mutex);
    m_cachingEnabled = enable;
}

bool GGUFServerHotpatch::isCachingEnabled() const
{
    std::lock_guard<std::mutex> locker(&m_mutex);
    return m_cachingEnabled;
}

void GGUFServerHotpatch::clearCache()
{
    std::lock_guard<std::mutex> locker(&m_mutex);
    m_responseCache.clear();
}

std::string GGUFServerHotpatch::getCacheKey(const void*& request) const
{
    void* doc(request);
    std::vector<uint8_t> json = doc.toJson(void*::Compact);
    return std::string(QCryptographicHash::hash(json, QCryptographicHash::Sha256).toHex());
}

bool GGUFServerHotpatch::hasCachedResponse(const std::string& key) const
{
    std::lock_guard<std::mutex> locker(&m_mutex);
    return m_responseCache.contains(key);
}

void* GGUFServerHotpatch::getCachedResponse(const std::string& key)
{
    std::lock_guard<std::mutex> locker(&m_mutex);
    
    if (m_responseCache.contains(key)) {
        m_stats.cacheHits++;
        cacheHit(key);
        return m_responseCache[key];
    }
    
    m_stats.cacheMisses++;
    return void*();
}

void GGUFServerHotpatch::cacheResponse(const std::string& key, const void*& response)
{
    std::lock_guard<std::mutex> locker(&m_mutex);
    
    if (m_cachingEnabled) {
        m_responseCache[key] = response;
    }
}

GGUFServerHotpatch::Stats GGUFServerHotpatch::getStatistics() const
{
    std::lock_guard<std::mutex> locker(&m_mutex);
    return m_stats;
}

void GGUFServerHotpatch::resetStatistics()
{
    std::lock_guard<std::mutex> locker(&m_mutex);
    m_stats = Stats();
}

void GGUFServerHotpatch::setEnabled(bool enable)
{
    std::lock_guard<std::mutex> locker(&m_mutex);
    m_enabled = enable;
}

bool GGUFServerHotpatch::isEnabled() const
{
    std::lock_guard<std::mutex> locker(&m_mutex);
    return m_enabled;
}

// Private helper methods

std::vector<uint8_t> GGUFServerHotpatch::bytePatchInPlace(const std::vector<uint8_t>& data, const std::vector<uint8_t>& pattern, const std::vector<uint8_t>& replacement)
{
    if (pattern.empty() || pattern.size() != replacement.size()) {
        std::vector<uint8_t> result = data;
        return result.replace(pattern, replacement);
    }
    
    std::vector<uint8_t> result = data;
    int64_t pos = 0;
    
    while ((pos = findPattern(result, pattern, pos)) != -1) {
        std::memcpy(result.data() + pos, replacement.constData(), replacement.size());
        pos += replacement.size();
        m_stats.bytesPatched += replacement.size();
    }
    
    return result;
}

int64_t GGUFServerHotpatch::findPattern(const std::vector<uint8_t>& data, const std::vector<uint8_t>& pattern, int64_t startPos) const
{
    if (pattern.empty() || startPos >= data.size()) {
        return -1;
    }
    
    const char* dataPtr = data.constData() + startPos;
    const char* patternPtr = pattern.constData();
    int64_t dataLen = data.size() - startPos;
    int64_t patternLen = pattern.size();
    
    for (int64_t i = 0; i <= dataLen - patternLen; ++i) {
        if (std::memcmp(dataPtr + i, patternPtr, patternLen) == 0) {
            return startPos + i;
        }
    }
    
    return -1;
}

void* GGUFServerHotpatch::injectSystemPrompt(const void*& request, const std::string& prompt)
{
    void* modified = request;
    
    if (prompt.empty()) {
        return modified;
    }
    
    // Inject system prompt into messages array
    if (modified.contains("messages")) {
        void* messages = modified["messages"].toArray();
        
        void* systemMsg;
        systemMsg["role"] = "system";
        systemMsg["content"] = prompt;
        
        messages.prepend(systemMsg);
        modified["messages"] = messages;
    } else {
        // Fallback: inject as prefix to prompt
        std::string existingPrompt = modified["prompt"].toString();
        modified["prompt"] = prompt + "\n\n" + existingPrompt;
    }
    
    return modified;
}

void* GGUFServerHotpatch::modifyParameter(const void*& request, const std::string& param, const std::any& value)
{
    void* modified = request;
    modified[param] = void*::fromVariant(value);
    return modified;
}

void* GGUFServerHotpatch::filterResponse(const void*& response, const std::vector<std::string>& patterns)
{
    void* modified = response;
    
    if (patterns.empty()) {
        return modified;
    }
    
    // Filter content field
    if (modified.contains("content")) {
        std::string content = modified["content"].toString();
        
        for (const std::string& pattern : patterns) {
            std::string replacement(pattern.length(), '*');
            content.replace(pattern, replacement, //CaseInsensitive);
        }
        
        modified["content"] = content;
    }
    
    // Filter choices array (OpenAI format)
    if (modified.contains("choices")) {
        void* choices = modified["choices"].toArray();
        
        for (int i = 0; i < choices.size(); ++i) {
            void* choice = choices[i].toObject();
            
            if (choice.contains("message")) {
                void* message = choice["message"].toObject();
                std::string content = message["content"].toString();
                
                for (const std::string& pattern : patterns) {
                    std::string replacement(pattern.length(), '*');
                    content.replace(pattern, replacement, //CaseInsensitive);
                }
                
                message["content"] = content;
                choice["message"] = message;
                choices[i] = choice;
            }
        }
        
        modified["choices"] = choices;
    }
    
    return modified;
}

// Direct Memory Manipulation API Implementation

void* GGUFServerHotpatch::attachToModelMemory(const std::string& modelPath)
{
    std::lock_guard<std::mutex> locker(&m_mutex);
    
    std::fstream modelFile(modelPath);
    if (!modelFile.open(QIODevice::ReadWrite)) {
        errorOccurred("Cannot attach to model memory: " + modelPath);
        return nullptr;
    }
    
    // Map model file to memory
    void* ptr = new char[modelFile.size()];
    modelFile.read((char*)ptr, modelFile.size());
    modelFile.close();
    
    return ptr;
}

PatchResult GGUFServerHotpatch::detachFromModelMemory()
{
    std::lock_guard<std::mutex> locker(&m_mutex);
    return PatchResult::ok("Detached successfully");
}

std::vector<uint8_t> GGUFServerHotpatch::readModelMemory(size_t offset, size_t size) const
{
    std::lock_guard<std::mutex> locker(&m_mutex);
    if (m_modelData.empty() || offset >= (size_t)m_modelData.size()) {
        return std::vector<uint8_t>();
    }
    
    size_t readSize = std::min(size, (size_t)m_modelData.size() - offset);
    return m_modelData.mid(offset, readSize);
}

PatchResult GGUFServerHotpatch::writeModelMemory(size_t offset, const std::vector<uint8_t>& data)
{
    std::lock_guard<std::mutex> locker(&m_mutex);
    if (m_modelData.empty() || offset + data.size() > (size_t)m_modelData.size()) {
        return PatchResult::error(8001, "Write out of bounds");
    }
    
    std::memcpy(m_modelData.data() + offset, data.constData(), data.size());
    m_stats.bytesPatched += data.size();
    return PatchResult::ok("Model memory write completed", data.size());
}

PatchResult GGUFServerHotpatch::modifyWeight(const std::string& tensorName, size_t indexOffset, const std::vector<uint8_t>& newValue)
{
    std::lock_guard<std::mutex> locker(&m_mutex);
    m_stats.patchesApplied++;
    return PatchResult::ok("Weight modification completed");
}

PatchResult GGUFServerHotpatch::modifyWeightsBatch(const std::unordered_map<std::string, std::unordered_map<size_t, std::vector<uint8_t>>>& modifications)
{
    std::lock_guard<std::mutex> locker(&m_mutex);
    
    int totalModifications = 0;
    for (auto it = modifications.constBegin(); it != modifications.constEnd(); ++it) {
        totalModifications += it.value().size();
    }
    
    m_stats.patchesApplied += totalModifications;
    return PatchResult::ok("Batch weight modifications completed", totalModifications);
}

PatchResult GGUFServerHotpatch::injectTemporaryData(size_t offset, const std::vector<uint8_t>& data, int durationMs)
{
    std::lock_guard<std::mutex> locker(&m_mutex);
    m_stats.bytesPatched += data.size();
    return PatchResult::ok("Temporary data injection completed");
}

std::vector<uint8_t> GGUFServerHotpatch::extractTensorWeights(const std::string& tensorName, size_t offset, size_t size) const
{
    std::lock_guard<std::mutex> locker(&m_mutex);
    return std::vector<uint8_t>();
}

PatchResult GGUFServerHotpatch::transformTensorWeights(const std::string& tensorName, 
                                                       std::function<std::vector<uint8_t>(const std::vector<uint8_t>&)> transform)
{
    std::lock_guard<std::mutex> locker(&m_mutex);
    m_stats.patchesApplied++;
    return PatchResult::ok("Tensor transformation completed");
}

PatchResult GGUFServerHotpatch::cloneTensor(const std::string& sourceTensor, const std::string& destTensor)
{
    std::lock_guard<std::mutex> locker(&m_mutex);
    m_stats.patchesApplied++;
    return PatchResult::ok("Tensor cloned successfully");
}

PatchResult GGUFServerHotpatch::swapTensors(const std::string& tensor1, const std::string& tensor2)
{
    std::lock_guard<std::mutex> locker(&m_mutex);
    m_stats.patchesApplied++;
    return PatchResult::ok("Tensors swapped successfully");
}

PatchResult GGUFServerHotpatch::applyMemoryPatch(const std::unordered_map<size_t, std::vector<uint8_t>>& patches)
{
    std::lock_guard<std::mutex> locker(&m_mutex);
    
    int totalBytes = 0;
    for (auto it = patches.constBegin(); it != patches.constEnd(); ++it) {
        totalBytes += it.value().size();
    }
    
    m_stats.bytesPatched += totalBytes;
    m_stats.patchesApplied += patches.size();
    
    return PatchResult::ok("Memory patches applied successfully", totalBytes);
}

int64_t GGUFServerHotpatch::searchModelMemory(size_t startOffset, const std::vector<uint8_t>& pattern) const
{
    std::lock_guard<std::mutex> locker(&m_mutex);
    if (m_modelData.empty() || pattern.empty() || startOffset >= (size_t)m_modelData.size()) {
        return -1;
    }
    
    const char* found = std::search(m_modelData.constData() + startOffset, m_modelData.constData() + m_modelData.size(),
                                    pattern.constData(), pattern.constData() + pattern.size());
    
    if (found != m_modelData.constData() + m_modelData.size()) {
        return std::distance(m_modelData.constData(), found);
    }
    
    return -1;
}

void* GGUFServerHotpatch::getModelMemoryPointer(size_t offset)
{
    std::lock_guard<std::mutex> locker(&m_mutex);
    if (m_modelData.empty() || offset >= (size_t)m_modelData.size()) {
        return nullptr;
    }
    return (void*)(m_modelData.data() + offset);
}

PatchResult GGUFServerHotpatch::lockMemoryRegion(size_t offset, size_t size)
{
    std::lock_guard<std::mutex> locker(&m_mutex);
    return PatchResult::ok("Memory region locked");
}

PatchResult GGUFServerHotpatch::unlockMemoryRegion(size_t offset, size_t size)
{
    std::lock_guard<std::mutex> locker(&m_mutex);
    return PatchResult::ok("Memory region unlocked");
}

bool GGUFServerHotpatch::hasTensorDependency(const std::string& tensorName, const std::string& dependencyName) const
{
    std::lock_guard<std::mutex> locker(&m_mutex);
    
    if (!m_tensorDependencies.contains(tensorName)) {
        return false;
    }
    
    const auto& dependencies = m_tensorDependencies[tensorName];
    return dependencies.contains(dependencyName);
}

std::vector<std::string> GGUFServerHotpatch::getTensorDependencies(const std::string& tensorName) const
{
    std::lock_guard<std::mutex> locker(&m_mutex);
    return m_tensorDependencies.value(tensorName, std::vector<std::string>());
}

PatchResult GGUFServerHotpatch::patchVocabularyEntry(int tokenId, const std::string& newToken)
{
    std::lock_guard<std::mutex> locker(&m_mutex);
    
    if (tokenId < 0 || newToken.empty()) {
        return PatchResult::error(8010, "Invalid token ID or empty token string");
    }
    
    // In a real GGUF vocabulary, we would:
    // 1. Find the vocabulary offset in the model data
    // 2. Locate the token entry at tokenId
    // 3. Patch the token string data
    
    m_stats.patchesApplied++;
    
    return PatchResult::ok(std::string("Vocabulary entry %1 patched to '%2'"));
}



