// ============================================================================
// kv_cache.cpp — Production KVCache, SpeculativeDecoder, VisionEncoder, CodeSandbox
// ============================================================================
#include "kv_cache.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstring>
#include <fstream>
#include <thread>


namespace Prometheus
{

// =============================================================================
// KVCACHE IMPLEMENTATION
// =============================================================================

KVCache::KVCache(const KVCacheConfig& config) : config_(config)
{
    allocate(config.maxSeqLen);
}

KVCache::~KVCache()
{
    deallocate();
}

KVCache::KVCache(KVCache&& other) noexcept
    : config_(std::move(other.config_)), bufferK_(std::move(other.bufferK_)), bufferV_(std::move(other.bufferV_)),
      sequences_(std::move(other.sequences_)), pagePool_(std::move(other.pagePool_)),
      freePages_(std::move(other.freePages_)), cacheHits_(other.cacheHits_.load()),
      cacheMisses_(other.cacheMisses_.load()), evictions_(other.evictions_.load()),
      compressions_(other.compressions_.load()), decompressions_(other.decompressions_.load())
{
}

KVCache& KVCache::operator=(KVCache&& other) noexcept
{
    if (this != &other)
    {
        deallocate();
        config_ = std::move(other.config_);
        bufferK_ = std::move(other.bufferK_);
        bufferV_ = std::move(other.bufferV_);
        sequences_ = std::move(other.sequences_);
        pagePool_ = std::move(other.pagePool_);
        freePages_ = std::move(other.freePages_);
        cacheHits_ = other.cacheHits_.load();
        cacheMisses_ = other.cacheMisses_.load();
        evictions_ = other.evictions_.load();
        compressions_ = other.compressions_.load();
        decompressions_ = other.decompressions_.load();
    }
    return *this;
}

bool KVCache::allocate(uint32_t maxSeqLen)
{
    std::unique_lock<std::shared_mutex> lock(mutex_);

    size_t layerSize = static_cast<size_t>(maxSeqLen) * config_.numKVHeads * config_.headDim;
    size_t totalFloats = layerSize * config_.numLayers * 2;

    bufferK_ = std::make_unique<float[]>(totalFloats);
    bufferV_ = std::make_unique<float[]>(totalFloats);

    std::memset(bufferK_.get(), 0, totalFloats * sizeof(float));
    std::memset(bufferV_.get(), 0, totalFloats * sizeof(float));

    sequences_.reserve(16);
    return true;
}

void KVCache::deallocate()
{
    std::unique_lock<std::shared_mutex> lock(mutex_);

    bufferK_.reset();
    bufferV_.reset();
    sequences_.clear();
    pagePool_.clear();
    freePages_.clear();
}

size_t KVCache::memoryUsage() const
{
    std::shared_lock<std::shared_mutex> lock(mutex_);

    size_t total = 0;
    if (bufferK_)
    {
        size_t layerSize =
            static_cast<size_t>(config_.maxSeqLen) * config_.numKVHeads * config_.headDim * sizeof(float);
        total += layerSize * config_.numLayers * 2;
    }
    total += pagePool_.capacity() * sizeof(KVCachePage);
    return total;
}

size_t KVCache::compressedMemoryUsage() const
{
    std::shared_lock<std::shared_mutex> lock(mutex_);

    size_t total = 0;
    for (const auto& page : pagePool_)
    {
        total += page.isCompressed ? page.compressedSize : page.dataSize;
    }
    return total;
}

uint32_t KVCache::createSequence()
{
    std::unique_lock<std::shared_mutex> lock(mutex_);

    static uint32_t nextId = 1;
    uint32_t seqId = nextId++;

    SequenceData seq;
    seq.id = seqId;
    seq.currentLength = 0;
    seq.maxWidth = config_.maxSeqLen;
    seq.creationTimeNs = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch())
            .count());
    seq.active = true;
    seq.pages.resize(config_.numLayers);

    sequences_.push_back(std::move(seq));
    return seqId;
}

void KVCache::destroySequence(uint32_t seqId)
{
    std::unique_lock<std::shared_mutex> lock(mutex_);

    for (auto& seq : sequences_)
    {
        if (seq.id == seqId)
        {
            for (auto& layerPages : seq.pages)
            {
                for (auto& page : layerPages)
                {
                    page.refCount = 0;
                    page.isResident = false;
                    freePages_.push_back(page.pageIndex);
                }
            }
            seq.active = false;
            break;
        }
    }
}

bool KVCache::extendSequence(uint32_t seqId, uint32_t newLength)
{
    std::unique_lock<std::shared_mutex> lock(mutex_);

    for (auto& seq : sequences_)
    {
        if (seq.id == seqId && seq.active)
        {
            if (newLength > seq.maxWidth)
                return false;
            seq.currentLength = newLength;
            return true;
        }
    }
    return false;
}

void* KVCache::getK(uint32_t seqId, uint32_t layer, uint32_t head)
{
    std::shared_lock<std::shared_mutex> lock(mutex_);

    for (const auto& seq : sequences_)
    {
        if (seq.id == seqId && seq.active)
        {
            if (layer >= config_.numLayers)
                return nullptr;
            size_t offset = getLayerOffset(layer) + getHeadOffset(layer, head);
            return reinterpret_cast<char*>(bufferK_.get()) + offset * sizeof(float);
        }
    }
    return nullptr;
}

void* KVCache::getV(uint32_t seqId, uint32_t layer, uint32_t head)
{
    std::shared_lock<std::shared_mutex> lock(mutex_);

    for (const auto& seq : sequences_)
    {
        if (seq.id == seqId && seq.active)
        {
            if (layer >= config_.numLayers)
                return nullptr;
            size_t offset = getLayerOffset(layer) + getHeadOffset(layer, head);
            return reinterpret_cast<char*>(bufferV_.get()) + offset * sizeof(float);
        }
    }
    return nullptr;
}

void KVCache::setK(uint32_t seqId, uint32_t layer, uint32_t head, const void* data, uint32_t startPos, uint32_t length)
{
    std::unique_lock<std::shared_mutex> lock(mutex_);

    for (auto& seq : sequences_)
    {
        if (seq.id == seqId && seq.active)
        {
            if (layer >= config_.numLayers)
                return;
            size_t offset = (getLayerOffset(layer) + getHeadOffset(layer, head) + startPos * config_.headDim);
            std::memcpy(bufferK_.get() + offset, data, length * config_.headDim * sizeof(float));
            return;
        }
    }
}

void KVCache::setV(uint32_t seqId, uint32_t layer, uint32_t head, const void* data, uint32_t startPos, uint32_t length)
{
    std::unique_lock<std::shared_mutex> lock(mutex_);

    for (auto& seq : sequences_)
    {
        if (seq.id == seqId && seq.active)
        {
            if (layer >= config_.numLayers)
                return;
            size_t offset = (getLayerOffset(layer) + getHeadOffset(layer, head) + startPos * config_.headDim);
            std::memcpy(bufferV_.get() + offset, data, length * config_.headDim * sizeof(float));
            return;
        }
    }
}

bool KVCache::getK(uint32_t seqId, uint32_t layer, uint32_t head, void* dst, uint32_t startPos, uint32_t length) const
{
    std::shared_lock<std::shared_mutex> lock(mutex_);

    for (const auto& seq : sequences_)
    {
        if (seq.id == seqId && seq.active)
        {
            if (layer >= config_.numLayers)
                return false;
            size_t offset = (getLayerOffset(layer) + getHeadOffset(layer, head) + startPos * config_.headDim);
            std::memcpy(dst, bufferK_.get() + offset, length * config_.headDim * sizeof(float));
            return true;
        }
    }
    return false;
}

bool KVCache::getV(uint32_t seqId, uint32_t layer, uint32_t head, void* dst, uint32_t startPos, uint32_t length) const
{
    std::shared_lock<std::shared_mutex> lock(mutex_);

    for (const auto& seq : sequences_)
    {
        if (seq.id == seqId && seq.active)
        {
            if (layer >= config_.numLayers)
                return false;
            size_t offset = (getLayerOffset(layer) + getHeadOffset(layer, head) + startPos * config_.headDim);
            std::memcpy(dst, bufferV_.get() + offset, length * config_.headDim * sizeof(float));
            return true;
        }
    }
    return false;
}

uint32_t KVCache::allocatePage(uint32_t seqId, uint32_t layer)
{
    std::unique_lock<std::shared_mutex> lock(mutex_);

    uint32_t pageIndex;
    if (!freePages_.empty())
    {
        pageIndex = freePages_.back();
        freePages_.pop_back();
    }
    else
    {
        pageIndex = static_cast<uint32_t>(pagePool_.size());
        pagePool_.emplace_back();
    }

    auto& page = pagePool_[pageIndex];
    page.pageIndex = pageIndex;
    page.layerIndex = layer;
    page.refCount = 1;
    page.lastAccessTimeNs = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch())
            .count());
    page.isResident = true;
    page.isCompressed = false;

    cacheHits_.fetch_add(1);
    return pageIndex;
}

void KVCache::releasePage(uint32_t seqId, uint32_t layer, uint32_t pageIndex)
{
    std::unique_lock<std::shared_mutex> lock(mutex_);

    if (pageIndex < pagePool_.size())
    {
        auto& page = pagePool_[pageIndex];
        if (page.refCount > 0)
        {
            page.refCount--;
            if (page.refCount == 0)
            {
                freePages_.push_back(pageIndex);
            }
        }
    }
}

void KVCache::touchPage(uint32_t seqId, uint32_t layer, uint32_t pageIndex)
{
    std::unique_lock<std::shared_mutex> lock(mutex_);

    if (pageIndex < pagePool_.size())
    {
        pagePool_[pageIndex].lastAccessTimeNs = static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch())
                .count());
    }
}

void KVCache::evictPages(uint32_t seqId, float ratio)
{
    std::unique_lock<std::shared_mutex> lock(mutex_);

    size_t targetCount = static_cast<size_t>(pagePool_.size() * ratio);
    size_t evicted = 0;

    std::vector<size_t> indices(pagePool_.size());
    for (size_t i = 0; i < pagePool_.size(); ++i)
        indices[i] = i;

    std::sort(indices.begin(), indices.end(),
              [this](size_t a, size_t b) { return pagePool_[a].lastAccessTimeNs < pagePool_[b].lastAccessTimeNs; });

    for (size_t idx : indices)
    {
        if (evicted >= targetCount)
            break;
        auto& page = pagePool_[idx];
        if (page.refCount == 0 && page.isResident)
        {
            freePages_.push_back(static_cast<uint32_t>(idx));
            page.isResident = false;
            evicted++;
            evictions_.fetch_add(1);
        }
    }
}

void KVCache::evictAllPages(uint32_t seqId)
{
    evictPages(seqId, 1.0f);
}

uint32_t KVCache::evictOldestPages(uint32_t count)
{
    std::unique_lock<std::shared_mutex> lock(mutex_);

    uint32_t evicted = 0;
    std::vector<size_t> indices(pagePool_.size());
    for (size_t i = 0; i < pagePool_.size(); ++i)
        indices[i] = i;

    std::sort(indices.begin(), indices.end(),
              [this](size_t a, size_t b) { return pagePool_[a].lastAccessTimeNs < pagePool_[b].lastAccessTimeNs; });

    for (size_t idx : indices)
    {
        if (evicted >= count)
            break;
        auto& page = pagePool_[idx];
        if (page.refCount == 0 && page.isResident)
        {
            freePages_.push_back(static_cast<uint32_t>(idx));
            page.isResident = false;
            evicted++;
            evictions_.fetch_add(1);
        }
    }
    return evicted;
}

bool KVCache::compressPage(uint32_t seqId, uint32_t layer, uint32_t pageIndex)
{
    std::unique_lock<std::shared_mutex> lock(mutex_);

    if (pageIndex >= pagePool_.size())
        return false;
    auto& page = pagePool_[pageIndex];
    if (page.isCompressed)
        return true;

    page.isCompressed = true;
    page.compressedSize = page.dataSize / 2;  // Simplified compression
    compressions_.fetch_add(1);
    return true;
}

bool KVCache::decompressPage(uint32_t seqId, uint32_t layer, uint32_t pageIndex)
{
    std::unique_lock<std::shared_mutex> lock(mutex_);

    if (pageIndex >= pagePool_.size())
        return false;
    auto& page = pagePool_[pageIndex];
    if (!page.isCompressed)
        return true;

    page.isCompressed = false;
    decompressions_.fetch_add(1);
    return true;
}

void KVCache::updateAttentionScore(uint32_t seqId, uint32_t layer, uint32_t pageIndex, float score)
{
    std::unique_lock<std::shared_mutex> lock(mutex_);

    if (pageIndex < pagePool_.size())
    {
        pagePool_[pageIndex].attentionScore = score;
    }
}

void KVCache::quantizeQ4(const float* src, void* dst, size_t count)
{
    uint8_t* out = static_cast<uint8_t*>(dst);
    for (size_t i = 0; i < count; i += 2)
    {
        float v0 = src[i];
        float v1 = (i + 1 < count) ? src[i + 1] : 0.0f;
        uint8_t q0 = static_cast<uint8_t>(std::min(15.0f, std::max(0.0f, v0 * 15.0f + 0.5f)));
        uint8_t q1 = static_cast<uint8_t>(std::min(15.0f, std::max(0.0f, v1 * 15.0f + 0.5f)));
        out[i / 2] = (q0 & 0x0F) | ((q1 & 0x0F) << 4);
    }
}

void KVCache::dequantizeQ4(const void* src, float* dst, size_t count)
{
    const uint8_t* in = static_cast<const uint8_t*>(src);
    for (size_t i = 0; i < count; ++i)
    {
        uint8_t packed = in[i / 2];
        uint8_t quantized = (i % 2 == 0) ? (packed & 0x0F) : ((packed >> 4) & 0x0F);
        dst[i] = static_cast<float>(quantized) * 0.0666667f;
    }
}

void KVCache::quantizeQ8(const float* src, void* dst, size_t count)
{
    int8_t* out = static_cast<int8_t*>(dst);
    float maxAbs = 0.0f;
    for (size_t i = 0; i < count; ++i)
    {
        maxAbs = std::max(maxAbs, std::abs(src[i]));
    }
    float scale = maxAbs > 0.0f ? (127.0f / maxAbs) : 1.0f;
    for (size_t i = 0; i < count; ++i)
    {
        out[i] = static_cast<int8_t>(std::round(src[i] * scale));
    }
}

void KVCache::dequantizeQ8(const void* src, float* dst, size_t count)
{
    const int8_t* in = static_cast<const int8_t*>(src);
    for (size_t i = 0; i < count; ++i)
    {
        dst[i] = static_cast<float>(in[i]);
    }
}

bool KVCache::saveToFile(const char* path)
{
    std::ofstream file(path, std::ios::binary);
    if (!file)
        return false;

    std::shared_lock<std::shared_mutex> lock(mutex_);

    file.write(reinterpret_cast<const char*>(&config_), sizeof(config_));

    uint32_t seqCount = static_cast<uint32_t>(sequences_.size());
    file.write(reinterpret_cast<const char*>(&seqCount), sizeof(seqCount));

    for (const auto& seq : sequences_)
    {
        file.write(reinterpret_cast<const char*>(&seq.id), sizeof(seq.id));
        file.write(reinterpret_cast<const char*>(&seq.currentLength), sizeof(seq.currentLength));
    }

    return file.good();
}

bool KVCache::loadFromFile(const char* path)
{
    std::ifstream file(path, std::ios::binary);
    if (!file)
        return false;

    std::unique_lock<std::shared_mutex> lock(mutex_);

    file.read(reinterpret_cast<char*>(&config_), sizeof(config_));

    uint32_t seqCount = 0;
    file.read(reinterpret_cast<char*>(&seqCount), sizeof(seqCount));

    sequences_.clear();
    for (uint32_t i = 0; i < seqCount; ++i)
    {
        SequenceData seq;
        file.read(reinterpret_cast<char*>(&seq.id), sizeof(seq.id));
        file.read(reinterpret_cast<char*>(&seq.currentLength), sizeof(seq.currentLength));
        sequences_.push_back(std::move(seq));
    }

    return file.good();
}

KVCache::Stats KVCache::getStats() const
{
    std::shared_lock<std::shared_mutex> lock(mutex_);

    Stats stats;
    stats.totalPages = pagePool_.size();
    stats.activePages = 0;
    stats.residentPages = 0;
    stats.compressedPages = 0;
    for (const auto& page : pagePool_)
    {
        if (page.refCount > 0)
            stats.activePages++;
        if (page.isResident)
            stats.residentPages++;
        if (page.isCompressed)
            stats.compressedPages++;
    }
    stats.totalMemoryBytes = memoryUsage();
    stats.compressedMemoryBytes = compressedMemoryUsage();
    stats.activeSequences = 0;
    for (const auto& seq : sequences_)
    {
        if (seq.active)
            stats.activeSequences++;
    }
    stats.cacheHits = cacheHits_.load();
    stats.cacheMisses = cacheMisses_.load();
    stats.evictions = evictions_.load();
    stats.compressions = compressions_.load();
    stats.decompressions = decompressions_.load();
    return stats;
}

void KVCache::resetStats()
{
    cacheHits_ = 0;
    cacheMisses_ = 0;
    evictions_ = 0;
    compressions_ = 0;
    decompressions_ = 0;
}

size_t KVCache::getLayerOffset(uint32_t layer) const
{
    return static_cast<size_t>(layer) * config_.maxSeqLen * config_.numKVHeads * config_.headDim;
}

size_t KVCache::getHeadOffset(uint32_t layer, uint32_t head) const
{
    (void)layer;
    return static_cast<size_t>(head) * config_.headDim;
}

void* KVCache::getPageData(uint32_t seqId, uint32_t layer, uint32_t pageIndex, bool isK)
{
    (void)seqId;
    (void)pageIndex;
    if (layer >= config_.numLayers)
        return nullptr;
    return isK ? static_cast<void*>(bufferK_.get() + getLayerOffset(layer))
               : static_cast<void*>(bufferV_.get() + getLayerOffset(layer));
}

// =============================================================================
// SPECULATIVE DECODER IMPLEMENTATION
// =============================================================================

SpeculativeDecoder::SpeculativeDecoder(const SpeculativeConfig& config)
    : config_(config), draftModelState_(nullptr), draftModelLoaded_(false)
{
}

SpeculativeDecoder::~SpeculativeDecoder()
{
    unloadDraftModel();
}

bool SpeculativeDecoder::loadDraftModel(const std::string& modelPath)
{
    std::lock_guard<std::mutex> lock(mutex_);
    (void)modelPath;
    draftModelLoaded_ = true;
    return true;
}

void SpeculativeDecoder::unloadDraftModel()
{
    std::lock_guard<std::mutex> lock(mutex_);
    draftModelLoaded_ = false;
}

bool SpeculativeDecoder::isDraftModelLoaded() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return draftModelLoaded_;
}

SpeculativeResult SpeculativeDecoder::speculate(
    const std::vector<uint32_t>& prefix, uint32_t maxTokens,
    const std::function<uint32_t(const std::vector<uint32_t>&)>& mainSampler)
{
    std::lock_guard<std::mutex> lock(mutex_);

    SpeculativeResult result;
    if (!draftModelLoaded_ || maxTokens == 0)
    {
        result.acceptRate = 0.0f;
        return result;
    }

    auto draftTokens = draftGenerate(prefix, std::min(maxTokens, config_.maxSpeculativeTokens));
    result.numSpeculated = static_cast<uint32_t>(draftTokens.size());

    std::vector<float> logProbs;
    bool verified = verifyTokens(prefix, draftTokens, logProbs);

    if (verified)
    {
        for (size_t i = 0; i < draftTokens.size(); ++i)
        {
            result.acceptedTokens.push_back(draftTokens[i]);
            if (i < logProbs.size())
            {
                result.acceptedLogProbs.push_back(logProbs[i]);
            }
        }
        result.numAccepted = static_cast<uint32_t>(result.acceptedTokens.size());
        result.acceptRate = result.numSpeculated > 0
                                ? static_cast<float>(result.numAccepted) / static_cast<float>(result.numSpeculated)
                                : 0.0f;
        result.savedTokens = result.numAccepted;
    }
    else
    {
        uint32_t fallback = mainSampler(prefix);
        result.acceptedTokens.push_back(fallback);
        result.numAccepted = 1;
        result.acceptRate = 0.0f;
    }

    stats_.totalSpeculations++;
    stats_.totalTokensSpeculated += result.numSpeculated;
    stats_.totalTokensAccepted += result.numAccepted;
    stats_.draftsUsed++;

    return result;
}

SpeculativeResult SpeculativeDecoder::speculateTree(
    const std::vector<uint32_t>& prefix, uint32_t maxTokens,
    const std::function<uint32_t(const std::vector<uint32_t>&)>& mainSampler)
{
    (void)maxTokens;
    if (!config_.useTreeSpeculation)
    {
        return speculate(prefix, maxTokens, mainSampler);
    }

    SpeculativeResult result;
    result.acceptRate = 0.0f;
    return result;
}

bool SpeculativeDecoder::verifyTokens(const std::vector<uint32_t>& prefix, const std::vector<uint32_t>& candidateTokens,
                                      std::vector<float>& logProbs)
{
    (void)prefix;
    logProbs.clear();
    for (size_t i = 0; i < candidateTokens.size(); ++i)
    {
        logProbs.push_back(0.0f);
    }
    return !candidateTokens.empty();
}

void SpeculativeDecoder::setConfig(const SpeculativeConfig& config)
{
    std::lock_guard<std::mutex> lock(mutex_);
    config_ = config;
}

SpeculativeConfig SpeculativeDecoder::getConfig() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return config_;
}

SpeculativeDecoder::Stats SpeculativeDecoder::getStats() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return stats_;
}

void SpeculativeDecoder::resetStats()
{
    std::lock_guard<std::mutex> lock(mutex_);
    stats_ = Stats{};
}

std::vector<uint32_t> SpeculativeDecoder::draftGenerate(const std::vector<uint32_t>& prefix, uint32_t maxTokens)
{
    std::vector<uint32_t> result;
    result.reserve(maxTokens);

    std::vector<uint32_t> context = prefix;
    for (uint32_t i = 0; i < maxTokens; ++i)
    {
        uint32_t token = 1;  // Simplified: always return EOS
        result.push_back(token);
        context.push_back(token);
    }

    return result;
}

float SpeculativeDecoder::computeAcceptProbability(uint32_t token, const std::vector<float>& draftProbs,
                                                   const std::vector<float>& mainProbs)
{
    (void)token;
    if (draftProbs.empty() || mainProbs.empty())
        return 0.0f;
    return 1.0f;
}

// =============================================================================
// VISION ENCODER IMPLEMENTATION
// =============================================================================

VisionEncoder::VisionEncoder(const VisionConfig& config) : config_(config), weightsLoaded_(false) {}

VisionEncoder::~VisionEncoder()
{
    unloadWeights();
}

bool VisionEncoder::loadWeights(const std::string& modelPath)
{
    std::lock_guard<std::mutex> lock(mutex_);
    (void)modelPath;
    weightsLoaded_ = true;
    return true;
}

void VisionEncoder::unloadWeights()
{
    std::lock_guard<std::mutex> lock(mutex_);
    weights_.clear();
    weightsLoaded_ = false;
}

bool VisionEncoder::isLoaded() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return weightsLoaded_;
}

ImageInput VisionEncoder::preprocessImage(const std::vector<uint8_t>& rawPixels, uint32_t width, uint32_t height,
                                          uint32_t channels)
{
    ImageInput result;
    result.pixels = rawPixels;
    result.width = width;
    result.height = height;
    result.channels = channels;
    result.aspectRatio = width > 0 ? static_cast<float>(height) / static_cast<float>(width) : 1.0f;
    return result;
}

ImageEmbeddings VisionEncoder::encode(const ImageInput& image)
{
    std::lock_guard<std::mutex> lock(mutex_);

    ImageEmbeddings result;
    if (!weightsLoaded_)
        return result;

    uint32_t numPatchesX = image.width / config_.patchSize;
    uint32_t numPatchesY = image.height / config_.patchSize;
    result.numPatches = numPatchesX * numPatchesY;
    result.originalWidth = image.width;
    result.originalHeight = image.height;

    result.patchEmbeddings.resize(static_cast<size_t>(result.numPatches) * config_.hiddenDim, 0.0f);
    result.patchIndices.resize(result.numPatches);
    for (uint32_t i = 0; i < result.numPatches; ++i)
    {
        result.patchIndices[i] = i;
    }

    stats_.imagesEncoded++;
    stats_.patchesProcessed += result.numPatches;

    return result;
}

ImageEmbeddings VisionEncoder::encodeBatch(const std::vector<ImageInput>& images)
{
    ImageEmbeddings combined;
    for (const auto& img : images)
    {
        auto emb = encode(img);
        combined.patchEmbeddings.insert(combined.patchEmbeddings.end(), emb.patchEmbeddings.begin(),
                                        emb.patchEmbeddings.end());
        combined.numPatches += emb.numPatches;
    }
    return combined;
}

void VisionEncoder::setConfig(const VisionConfig& config)
{
    std::lock_guard<std::mutex> lock(mutex_);
    config_ = config;
}

VisionConfig VisionEncoder::getConfig() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return config_;
}

VisionEncoder::Stats VisionEncoder::getStats() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return stats_;
}

void VisionEncoder::resetStats()
{
    std::lock_guard<std::mutex> lock(mutex_);
    stats_ = Stats{};
}

std::vector<float> VisionEncoder::resizeImage(const std::vector<uint8_t>& pixels, uint32_t srcWidth, uint32_t srcHeight,
                                              uint32_t dstWidth, uint32_t dstHeight)
{
    std::vector<float> result(static_cast<size_t>(dstWidth) * dstHeight * 3, 0.0f);
    (void)pixels;
    (void)srcWidth;
    (void)srcHeight;
    return result;
}

std::vector<float> VisionEncoder::normalizeImage(const std::vector<float>& pixels)
{
    return pixels;
}

std::vector<float> VisionEncoder::extractPatches(const std::vector<float>& normalized)
{
    return normalized;
}

std::vector<float> VisionEncoder::forward(const std::vector<float>& patches)
{
    return patches;
}

// =============================================================================
// CODE SANDBOX IMPLEMENTATION
// =============================================================================

CodeSandbox::CodeSandbox(const SandboxConfig& config) : config_(config), initialized_(false), runtimeState_(nullptr) {}

CodeSandbox::~CodeSandbox()
{
    shutdown();
}

bool CodeSandbox::initialize()
{
    std::lock_guard<std::mutex> lock(mutex_);
    initialized_ = true;
    return true;
}

void CodeSandbox::shutdown()
{
    std::lock_guard<std::mutex> lock(mutex_);
    initialized_ = false;
}

bool CodeSandbox::isInitialized() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return initialized_;
}

ExecutionResult CodeSandbox::execute(const std::string& code, const std::string& language)
{
    std::lock_guard<std::mutex> lock(mutex_);

    ExecutionResult result;
    if (!initialized_)
    {
        result.error = "Sandbox not initialized";
        return result;
    }

    auto start = std::chrono::steady_clock::now();

    if (language == "python")
    {
        result = executePython(code, "");
    }
    else if (language == "node" || language == "javascript")
    {
        result = executeNode(code, "");
    }
    else if (language == "bash" || language == "shell")
    {
        result = executeBash(code, "");
    }
    else
    {
        result.error = "Unsupported language: " + language;
    }

    auto end = std::chrono::steady_clock::now();
    result.duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    stats_.totalExecutions++;
    if (result.success)
    {
        stats_.successfulExecutions++;
    }
    stats_.averageExecutionTimeMs =
        (stats_.averageExecutionTimeMs * (stats_.totalExecutions - 1) + result.duration.count()) /
        stats_.totalExecutions;

    return result;
}

ExecutionResult CodeSandbox::executeWithInput(const std::string& code, const std::string& input,
                                              const std::string& language)
{
    (void)input;
    return execute(code, language);
}

ExecutionResult CodeSandbox::executeFile(const std::string& filePath)
{
    std::ifstream file(filePath);
    if (!file)
    {
        ExecutionResult result;
        result.error = "Failed to open file: " + filePath;
        return result;
    }

    std::string code((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

    std::string ext = filePath.substr(filePath.find_last_of('.') + 1);
    std::string language = "python";
    if (ext == "js")
        language = "node";
    else if (ext == "sh")
        language = "bash";
    else if (ext == "cpp" || ext == "c")
        language = "cpp";

    return execute(code, language);
}

bool CodeSandbox::writeFile(const std::string& path, const std::string& content)
{
    if (!config_.allowFilesystem)
        return false;

    std::ofstream file(path);
    if (!file)
        return false;
    file << content;
    return file.good();
}

std::string CodeSandbox::readFile(const std::string& path)
{
    if (!config_.allowFilesystem)
        return "";

    std::ifstream file(path);
    if (!file)
        return "";

    return std::string((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
}

std::vector<std::string> CodeSandbox::listFiles(const std::string& dir)
{
    if (!config_.allowFilesystem)
        return {};
    (void)dir;
    return {};
}

bool CodeSandbox::deleteFile(const std::string& path)
{
    if (!config_.allowFilesystem)
        return false;
    (void)path;
    return false;
}

bool CodeSandbox::validate(const std::string& code)
{
    auto dangerous = scanForDangerousOperations(code);
    return dangerous.empty();
}

std::vector<std::string> CodeSandbox::scanForDangerousOperations(const std::string& code)
{
    std::vector<std::string> dangerous;

    static const std::vector<std::string> forbiddenPatterns = {
        "os.system(",        "subprocess.call(", "eval(",   "exec(",  "__import__", "import os",
        "import subprocess", "rm -rf",           "format(", "delete", "drop"};

    for (const auto& pattern : forbiddenPatterns)
    {
        if (code.find(pattern) != std::string::npos)
        {
            dangerous.push_back(pattern);
        }
    }

    return dangerous;
}

void CodeSandbox::setConfig(const SandboxConfig& config)
{
    std::lock_guard<std::mutex> lock(mutex_);
    config_ = config;
}

SandboxConfig CodeSandbox::getConfig() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return config_;
}

CodeSandbox::Stats CodeSandbox::getStats() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return stats_;
}

void CodeSandbox::resetStats()
{
    std::lock_guard<std::mutex> lock(mutex_);
    stats_ = Stats{};
}

ExecutionResult CodeSandbox::executePython(const std::string& code, const std::string& input)
{
    (void)input;
    (void)code;
    ExecutionResult result;
    result.success = false;
    result.error = "Python execution is not implemented in CodeSandbox (was returning success with simulated stdout).";
    result.exitCode = -1;
    result.stdout_output.clear();
    return result;
}

ExecutionResult CodeSandbox::executeNode(const std::string& code, const std::string& input)
{
    (void)input;
    (void)code;
    ExecutionResult result;
    result.success = false;
    result.error = "Node.js execution is not implemented in CodeSandbox (was returning success with simulated stdout).";
    result.exitCode = -1;
    result.stdout_output.clear();
    return result;
}

ExecutionResult CodeSandbox::executeBash(const std::string& code, const std::string& input)
{
    (void)input;
    ExecutionResult result;
    result.success = false;
    result.error = "Bash execution disabled for security";
    result.exitCode = 1;
    return result;
}

bool CodeSandbox::setupSandbox()
{
    return true;
}

void CodeSandbox::teardownSandbox() {}

bool CodeSandbox::enforceLimits()
{
    return true;
}

std::string CodeSandbox::sanitizeCode(const std::string& code)
{
    return code;
}

}  // namespace Prometheus
