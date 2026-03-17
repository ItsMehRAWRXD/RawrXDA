# Tier Hopping + Activation Compression Integration

## Architecture Overview

```
┌─────────────────────────────────────────────────────────────────┐
│                    TIER HOPPING SYSTEM                          │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  INFERENCE RUNNING (TIER_70B)                                  │
│  ├─ GPU: Compute embeddings + attention                        │
│  ├─ Memory: 42GB model + 5GB KV cache + 3GB activations       │
│  └─ Total: 50GB in-use                                         │
│                                                                 │
│  ↓ [HOTPATCH TRIGGER: Low memory / Low latency needed]         │
│                                                                 │
│  COMPRESSION PHASE (<100ms)                                    │
│  ├─ compressKVCache()        [5GB → 500MB]                    │
│  ├─ compressActivations()    [3GB → 300MB]                    │
│  └─ Memory freed: ~7.2GB                                       │
│                                                                 │
│  TIER TRANSITION PHASE (~50ms)                                 │
│  ├─ Unload TIER_70B weights  [42GB freed]                     │
│  ├─ Load TIER_21B weights    [14GB allocated]                 │
│  └─ Net memory change: -28GB                                   │
│                                                                 │
│  DECOMPRESSION PHASE (<50ms)                                   │
│  ├─ decompressKVCache()      [500MB → 5GB]                    │
│  ├─ decompressActivations()  [300MB → 3GB]                    │
│  └─ Memory consumed: 8GB (matches context)                     │
│                                                                 │
│  INFERENCE RESUMED (TIER_21B)                                  │
│  ├─ GPU: Compute with 21B model                                │
│  ├─ Memory: 14GB model + 5GB KV cache + 3GB activations      │
│  └─ Total: 22GB in-use (61% reduction!)                       │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

## Integration Points in Code

### 1. agentic_copilot_bridge.h Extension

```cpp
// In agentic_copilot_bridge.h (existing class)

#include "activation_compressor.h"

// Add to class definition:
public:
    // Tier hopping with activation compression
    bool hotpatchToModelTierWithCompression(ModelTier target_tier);
    
    // Called before tier transition
    void prepareForTierTransition();
    
    // Called after tier transition
    void resumeInferenceAfterTierTransition();
    
private:
    // Cached compressed data
    inference::KVCacheCompressor::CompressedKVCache m_compressed_kv_cache;
    std::vector<inference::ActivationPruner::SparseActivation> m_compressed_activations;
    inference::CompressionTier m_current_compression_tier;
```

### 2. Implementation in agentic_copilot_bridge.cpp

```cpp
#include "activation_compressor.h"
#include <chrono>

// New method: Intelligent tier hopping with compression
bool AgenticCopilotBridge::hotpatchToModelTierWithCompression(ModelTier target_tier) {
    auto start = std::chrono::high_resolution_clock::now();
    
    qInfo() << "[AgenticBridge] Initiating hotpatch to tier" << (int)target_tier;
    
    // PHASE 1: Compression (if needed)
    std::unique_lock<std::mutex> lock(m_mutex);
    
    if (target_tier < m_current_model_tier) {
        // Downgrading to lighter model (more aggressive compression)
        this->prepareForTierTransition();
    }
    
    // PHASE 2: Model swap
    bool swap_success = false;
    {
        lock.unlock();  // Release lock during heavy I/O
        swap_success = this->loadModelTier(target_tier);
        lock.lock();
    }
    
    if (!swap_success) {
        qCritical() << "[AgenticBridge] Tier swap failed!";
        return false;
    }
    
    // PHASE 3: Decompression
    this->resumeInferenceAfterTierTransition();
    
    m_current_model_tier = target_tier;
    m_current_compression_tier = inference::getCompressionTierForModel(target_tier);
    
    auto elapsed = std::chrono::high_resolution_clock::now() - start;
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();
    
    qInfo() << "[AgenticBridge] Hotpatch complete in" << ms << "ms";
    
    // Emit telemetry
    QJsonObject meta;
    meta["source_tier"] = (int)m_current_model_tier;
    meta["target_tier"] = (int)target_tier;
    meta["transition_ms"] = (int)ms;
    meta["kv_cache_size_mb"] = (int)(m_compressed_kv_cache.key_data.size() / 1024 / 1024);
    GetTelemetry().recordEvent("tier_hotpatch", meta);
    
    return true;
}

void AgenticCopilotBridge::prepareForTierTransition() {
    qInfo() << "[AgenticBridge] Preparing for tier transition...";
    
    auto start = std::chrono::high_resolution_clock::now();
    
    // Get compression config based on target tier
    auto prune_config = inference::getCompressionConfig(m_current_compression_tier);
    
    // Step 1: Compress KV cache (5GB → 500MB)
    if (m_transformer_inference && m_transformer_inference->hasKVCache()) {
        auto [key_data, value_data, seq_len] = m_transformer_inference->getKVCacheData();
        
        m_compressed_kv_cache = inference::KVCacheCompressor::compressForTierHop(
            key_data,
            value_data,
            seq_len,
            m_transformer_inference->getNumHeads(),
            m_transformer_inference->getHeadDim()
        );
        
        size_t saved = inference::KVCacheCompressor::getMemorySaved(m_compressed_kv_cache);
        qInfo() << "[Compression] KV cache compressed, saved" << (saved / 1024 / 1024) << "MB";
    }
    
    // Step 2: Compress activations (3GB → 300MB)
    if (m_transformer_inference && m_transformer_inference->hasActivationBuffer()) {
        auto activations = m_transformer_inference->getActivationBuffer();
        
        auto sparse = inference::ActivationPruner::prune(
            activations.data(),
            activations.size(),
            prune_config
        );
        
        float ratio = inference::ActivationPruner::getCompressionRatio(sparse);
        qInfo() << "[Compression] Activations compressed, ratio:" << (ratio * 100) << "%";
        
        m_compressed_activations.push_back(sparse);
    }
    
    auto elapsed = std::chrono::high_resolution_clock::now() - start;
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();
    qInfo() << "[AgenticBridge] Compression phase complete in" << ms << "ms";
}

void AgenticCopilotBridge::resumeInferenceAfterTierTransition() {
    qInfo() << "[AgenticBridge] Resuming inference...";
    
    auto start = std::chrono::high_resolution_clock::now();
    
    // Step 1: Decompress KV cache
    if (!m_compressed_kv_cache.key_data.empty()) {
        uint32_t seq_len = m_transformer_inference->getSequenceLength();
        
        // Allocate new KV cache for new model tier
        m_transformer_inference->allocateKVCache(seq_len);
        
        auto [key_buffer, value_buffer] = m_transformer_inference->getKVCacheBuffers();
        
        inference::KVCacheCompressor::decompressForTierHop(
            m_compressed_kv_cache,
            key_buffer,
            value_buffer,
            seq_len
        );
        
        qInfo() << "[Decompression] KV cache restored";
        m_compressed_kv_cache.key_data.clear();  // Free memory
    }
    
    // Step 2: Decompress activations
    for (const auto& sparse_activation : m_compressed_activations) {
        auto dense = inference::ActivationPruner::recover(sparse_activation);
        // Feed back to activation buffer for next layer computation
        m_transformer_inference->setActivationBuffer(dense);
    }
    
    m_compressed_activations.clear();  // Free memory
    
    auto elapsed = std::chrono::high_resolution_clock::now() - start;
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();
    qInfo() << "[AgenticBridge] Decompression phase complete in" << ms << "ms";
}

// Helper: Map model tier to compression profile
static inline inference::CompressionTier 
getCompressionTierForModel(ModelTier tier) {
    switch (tier) {
        case ModelTier::TIER_70B:
            return inference::CompressionTier::TIER_AGGRESSIVE;
        case ModelTier::TIER_21B:
            return inference::CompressionTier::TIER_BALANCED;
        case ModelTier::TIER_6B:
            return inference::CompressionTier::TIER_FAST;
        case ModelTier::TIER_2B:
            return inference::CompressionTier::TIER_ULTRA_FAST;
        default:
            return inference::CompressionTier::TIER_BALANCED;
    }
}
```

### 3. Integration with Inference Loop

```cpp
// In inference loop (streaming_inference.cpp)

void StreamingInferenceEngine::generateNextToken() {
    auto start = std::chrono::high_resolution_clock::now();
    
    // Forward pass
    Tensor logits = m_transformer->forward(m_input_ids, m_position);
    
    // Sample next token
    uint32_t next_token = this->sampleToken(logits);
    m_input_ids.push_back(next_token);
    
    // Update KV cache
    this->updateKVCache(m_transformer->getKVCache());
    
    // CHECK: Should we hotpatch to lighter tier?
    auto elapsed = std::chrono::high_resolution_clock::now() - start;
    float tokens_per_sec = 1000.0f / std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();
    
    if (tokens_per_sec < TARGET_THROUGHPUT && getMemoryUsage() > MEMORY_THRESHOLD) {
        // Memory pressure + slow inference = switch to lighter model
        qInfo() << "[Inference] Throughput low (" << tokens_per_sec << " tok/s), switching tier";
        
        m_agentic_bridge->hotpatchToModelTierWithCompression(ModelTier::TIER_21B);
    }
    
    emit tokenGenerated(next_token);
}
```

### 4. Autonomous Tier Selection

```cpp
// In autonomous_model_manager.cpp (from conversation context)

class AutonomousModelManager {
private:
    struct TaskProfile {
        std::string task_type;          // "completion", "refactoring", "analysis"
        float avg_latency_ms;
        float avg_throughput_tokens_per_sec;
        ModelTier preferred_tier;
        int samples = 0;
    };
    
    std::map<std::string, TaskProfile> m_task_profiles;
    
public:
    // Learn which tier is best for each task type
    void recordTaskExecution(const std::string& task_type, 
                            ModelTier tier_used,
                            float latency_ms,
                            float throughput) {
        auto& profile = m_task_profiles[task_type];
        
        // Running average
        float prev_avg = profile.avg_latency_ms * profile.samples;
        profile.samples++;
        profile.avg_latency_ms = (prev_avg + latency_ms) / profile.samples;
        profile.avg_throughput_tokens_per_sec = 
            (profile.avg_throughput_tokens_per_sec * (profile.samples - 1) + throughput) / profile.samples;
        
        // Update preferred tier if this is better
        if (throughput > (TARGET_THROUGHPUT * 0.8) && latency_ms < 100) {
            profile.preferred_tier = tier_used;
        }
    }
    
    // Get best tier for task based on history
    ModelTier getBestTierForTask(const std::string& task_type) {
        auto it = m_task_profiles.find(task_type);
        if (it != m_task_profiles.end()) {
            return it->second.preferred_tier;
        }
        return ModelTier::TIER_70B;  // Default to full model
    }
};
```

### 5. Memory Monitoring Integration

```cpp
// In autonomous_resource_manager.cpp (existing class)

class AutonomousResourceManager {
private:
    static constexpr size_t MEMORY_THRESHOLD_MB = 50 * 1024;  // 50GB
    static constexpr float THROUGHPUT_TARGET = 70.0f;  // 70 tok/s
    
public:
    void checkAndAutoTierDown() {
        size_t current_mem_mb = getProcessMemoryUsageMB();
        float current_throughput = m_metrics_emitter->getThroughputTokensPerSec();
        
        bool should_tier_down = false;
        std::string reason;
        
        // Rule 1: Memory pressure
        if (current_mem_mb > MEMORY_THRESHOLD_MB) {
            should_tier_down = true;
            reason = "Memory pressure: " + std::to_string(current_mem_mb) + " MB";
        }
        
        // Rule 2: Throughput degradation
        if (current_throughput < THROUGHPUT_TARGET * 0.5) {
            should_tier_down = true;
            reason = "Low throughput: " + std::to_string(current_throughput) + " tok/s";
        }
        
        // Rule 3: Thermal throttling detected (requires platform support)
        if (m_thermal_monitor && m_thermal_monitor->isThrottling()) {
            should_tier_down = true;
            reason = "Thermal throttling detected";
        }
        
        if (should_tier_down) {
            qInfo() << "[ResourceManager] Auto tier-down:" << reason.c_str();
            
            ModelTier current = m_agentic_bridge->getCurrentModelTier();
            ModelTier next = getNextLighterTier(current);
            
            if (next != current) {
                m_agentic_bridge->hotpatchToModelTierWithCompression(next);
            }
        }
    }
};
```

## Performance Expectations

### Before Compression
```
TIER_70B inference:
├─ Memory: 50GB (model + KV + activations)
├─ Latency: 500ms/token (2 tok/s)
├─ Hotpatch: ~5000ms (reload entire 42GB model)
└─ Status: OOM risk, slow
```

### After Compression + Tier Hopping
```
TIER_70B → TIER_21B hotpatch cycle:
├─ Compression phase: 30-50ms (compress 5GB KV + 3GB activations)
├─ Model swap: 50-100ms (unload 42GB, load 14GB)
├─ Decompression phase: 20-50ms (restore KV + activations)
├─ Total hotpatch time: ~100-150ms ✅
├─ Memory saved: 28GB (5GB + 3GB + 42GB - 14GB - 5GB - 3GB)
├─ New memory: 22GB (61% reduction!)
└─ Status: Stable, fast tier transitions
```

## Testing Checklist

- [ ] Quantization codec: Test quantize/dequantize roundtrip on random tensors
- [ ] Sparsity detection: Verify compression ratio on activations
- [ ] KV cache compression: Measure memory saved vs inference quality impact
- [ ] Tier transition: Benchmark <100ms hotpatch on real models
- [ ] Integration: End-to-end inference with automatic tier switching
- [ ] Stability: Run 10M+ token inference without crashes
- [ ] Throughput: Measure 70+ tok/s achievement on TIER_21B
- [ ] Memory: Verify 25.6GB total footprint (70B equivalent on 64GB RAM)
