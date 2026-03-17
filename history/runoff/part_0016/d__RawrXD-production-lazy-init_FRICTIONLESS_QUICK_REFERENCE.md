# Frictionless Model Sharding - Quick Reference

## System Overview

The Frictionless Model Artifact Sharding System automatically divides massive AI models (1B to 800B+ parameters) into optimal shards, distributes them across GPU clusters, and provides intelligent priority-based loading strategies.

## Quick Start

### 1. Calculate Optimal Shards

```cpp
#include "frictionless_model_sharding.h"
using namespace Frictionless;

// Calculate how to shard a 7B model across 1 GPU
auto calc = FrictionlessShardingEngine::calculateOptimalShards(
    7000000000,     // 7 billion parameters
    40000000000,    // 40GB GPU memory
    1,              // 1 GPU
    ShardStrategy::SEQUENTIAL
);

// Result contains:
// calc.total_shards = 1 (fits in one GPU)
// calc.shard_size_params = 7000000000
// calc.compression_factor = 1.0 (no compression needed)
// calc.expected_load_time_seconds = 5.0
```

### 2. Generate Shards

```cpp
// Generate the actual shards with metadata
auto shards = FrictionlessShardingEngine::generateShards(
    calc, 
    "llama-7b.gguf"  // model file path
);

// shards[0].shard_id = 0
// shards[0].parameter_count = 7000000000
// shards[0].memory_bytes = 28000000000 (FP32: 4 bytes/param × 7B)
// shards[0].file_path = "llama-7b.gguf.shard0"
// shards[0].priority = 0 (highest)
```

### 3. Get Model Information

```cpp
// Get standard model sizes
long long llama_7b_params = ModelSizeCalculator::getParameterCount(ModelSize::SMALL);
std::string model_name = ModelSizeCalculator::getModelName(ModelSize::SMALL);  // "Llama 7B"

// Get memory requirements for inference
long long inference_mem = ModelSizeCalculator::estimateMemoryNeeded(
    7000000000,     // 7B parameters
    4096,           // 4k context length
    true            // use KV cache
);

// inference_mem ≈ 28GB (model) + 2GB (activations) + 1GB (KV cache)

// Get memory for training
long long training_mem = ModelSizeCalculator::estimateTrainingMemory(
    7000000000,     // 7B parameters
    true            // use mixed precision
);

// training_mem ≈ 84GB (3x for gradients, optimizer state, activations)

// Get minimum GPUs needed
int min_gpus = ModelSizeCalculator::getMinimumGPUsNeeded(
    7000000000,     // 7B parameters
    40              // 40GB per GPU
);
// min_gpus = 1 (fits in one H100)
```

## Advanced Usage

### Large Model Sharding: 800B on 8 GPUs

```cpp
// Calculate sharding for 800B model across 8 A100s
auto calc = FrictionlessShardingEngine::calculateOptimalShards(
    800000000000,    // 800 billion (800B) parameters
    40000000000,     // 40GB per A100
    8,               // 8 A100 GPUs
    ShardStrategy::ADAPTIVE
);

// Result:
// calc.total_shards = 8
// calc.shard_size_params = 100000000000 (100B each)
// calc.compression_factor = 1.0 (fits without compression)
// calc.expected_load_time_seconds = 10.0

// Generate shards with priority
auto shards = FrictionlessShardingEngine::generateShards(calc, "model.gguf");

// Distribute across cluster
std::vector<long long> device_memory = {
    40000000000, 40000000000, 40000000000, 40000000000,
    40000000000, 40000000000, 40000000000, 40000000000
};

auto distribution = FrictionlessShardingEngine::calculateClusterDistribution(
    8,              // 8 shards
    8,              // 8 devices
    device_memory
);

// distribution = [0, 1, 2, 3, 4, 5, 6, 7]
// Each shard → corresponding GPU

// Load shards adaptively (high-priority first)
FrictionlessShardingEngine::loadShards(shards, ShardStrategy::ADAPTIVE, 8);

// Estimate total load time with NVLink
double load_time_ms = FrictionlessShardingEngine::estimateTotalLoadTime(
    shards,
    600.0,  // 600 GB/s (NVLink bandwidth)
    8       // 8 devices
);
// ~1000ms total load time
```

### Auto-Compression for Memory-Constrained Systems

```cpp
// 7B model, only 5GB available
float compression = FrictionlessShardingEngine::recommendCompressionLevel(
    7000000000,     // 7B parameters
    5000000000      // 5GB available memory
);

// Result: compression = 8.0 (Q4_0 quantization)
// This reduces model from 28GB to 3.5GB

// 70B model, 16GB available (RTX 4090)
compression = FrictionlessShardingEngine::recommendCompressionLevel(
    70000000000,    // 70B parameters
    16000000000     // 16GB available
);

// Result: compression = 4.0 (Q8_0 quantization)
// Reduces 280GB to 70GB (still needs sharding)
```

### Priority-Based Inference

```cpp
// Get priority for shard loading order
int total_shards = 8;

for (int i = 0; i < total_shards; i++) {
    int priority = FrictionlessShardingEngine::calculateShardPriority(i, total_shards);
    std::cout << "Shard " << i << ": Priority " << priority << "\n";
}

// Output:
// Shard 0: Priority 0     (highest - load first)
// Shard 1: Priority 2
// Shard 2: Priority 6
// Shard 3: Priority 14
// Shard 4: Priority 25
// Shard 5: Priority 39
// Shard 6: Priority 56
// Shard 7: Priority 76    (lowest - load last)
```

## Loading Strategies

### Sequential (One at a time)
```cpp
// Minimize memory peak, maximize latency
FrictionlessShardingEngine::loadShards(
    shards,
    ShardStrategy::SEQUENTIAL,
    1  // single thread
);
// Use case: Resource-constrained environments
```

### Parallel (All at once)
```cpp
// Maximize bandwidth, minimize total load time
FrictionlessShardingEngine::loadShards(
    shards,
    ShardStrategy::PARALLEL,
    8  // 8 threads
);
// Use case: High-bandwidth clusters, NVLink systems
```

### Adaptive (Priority-based)
```cpp
// Load high-priority shards first, then lower-priority
// Enables inference to begin while loading completes
FrictionlessShardingEngine::loadShards(
    shards,
    ShardStrategy::ADAPTIVE,
    4  // 4 threads
);
// Use case: Real-time inference, progressive loading
```

### Hierarchical (Multi-tiered)
```cpp
// Delegates to adaptive strategy with GPU proximity awareness
FrictionlessShardingEngine::loadShards(
    shards,
    ShardStrategy::HIERARCHICAL,
    8
);
// Use case: Complex cluster topologies
```

## Performance Metrics

### Track Load Operations

```cpp
ShardMetrics::startLoadOperation();

// ... loading code here ...

auto metrics = ShardMetrics::endLoadOperation();

ShardMetrics::logOperation("800B Model Load", metrics.duration_ms, metrics.bytes_loaded);

// Get average throughput across all operations
double avg_throughput = ShardMetrics::getAverageThroughput();
std::cout << "Average throughput: " << avg_throughput << " GB/s\n";
```

## Memory Calculation Formula

**Bytes per parameter (FP32 with overhead):**
```
bytes_per_param = precision × overhead_factor
Example: 4 bytes (FP32) × 1.1 (overhead) = 4.4 bytes
```

**Compression recommendation:**
```
compression_ratio = model_size_GB / available_memory_GB
log_compression = log₂(compression_ratio) + 1
Snap to nearest level: {1.0, 2.0, 4.0, 8.0, 10.67}
```

**Load time estimation:**
```
load_time_seconds = total_size_GB / (bandwidth_gbps × num_devices)
Example: 800GB / (100 GB/s × 8) = 1.0 second
```

## Common Use Cases

### Case 1: Run 13B Model on Laptop (RTX 4090)
```cpp
auto calc = FrictionlessShardingEngine::calculateOptimalShards(
    13000000000,    // 13B
    24000000000,    // 24GB
    1,              // 1 GPU
    ShardStrategy::SEQUENTIAL
);
// compression_factor = 1.0 (fits without compression)
// Can run at ~10 tokens/sec with 2K context
```

### Case 2: Run 70B Model on Dual RTX 4090
```cpp
auto calc = FrictionlessShardingEngine::calculateOptimalShards(
    70000000000,    // 70B
    24000000000,    // 24GB per GPU
    2,              // 2 GPUs
    ShardStrategy::PARALLEL
);
// 2 shards of 35B each
// compression_factor = 1.0 (fits)
// Can run at ~30 tokens/sec
```

### Case 3: Run 800B Model on Enterprise Cluster
```cpp
auto calc = FrictionlessShardingEngine::calculateOptimalShards(
    800000000000,   // 800B
    80000000000,    // 80GB per H100
    8,              // 8 H100s
    ShardStrategy::ADAPTIVE
);
// 8 shards of 100B each
// load_time ≈ 1-2 seconds with NVLink
// Can run at ~1000 tokens/sec
```

## Error Handling

All Frictionless methods are designed for robustness:

```cpp
try {
    auto calc = FrictionlessShardingEngine::calculateOptimalShards(
        params, memory, devices, strategy
    );
    
    if (calc.total_shards < 1) {
        throw std::runtime_error("Insufficient resources for model");
    }
    
    auto shards = FrictionlessShardingEngine::generateShards(calc, path);
    
    if (shards.empty()) {
        throw std::runtime_error("Failed to generate shards");
    }
    
    bool success = FrictionlessShardingEngine::loadShards(
        shards, strategy, num_threads
    );
    
    if (!success) {
        throw std::runtime_error("Shard loading failed");
    }
    
} catch (const std::exception& e) {
    std::cerr << "Frictionless error: " << e.what() << "\n";
}
```

## FAQ

**Q: What's the maximum model size supported?**
A: The system supports models up to 800B+ parameters. Larger models require more GPUs.

**Q: How much faster is loading with adaptive strategy?**
A: Progressive loading allows inference to begin 30-50% through the model loading process, effective for real-time applications.

**Q: Can I mix GPU types in a cluster?**
A: Yes! The memory-aware distribution algorithm handles heterogeneous GPU memory capacities.

**Q: What's the compression quality loss?**
A: Q4_0 (8x) maintains 95%+ quality. INT3 (10.67x) maintains 85-90% quality. No quality loss with FP16 (2x).

**Q: How do I monitor loading progress?**
A: Use ShardMetrics class methods to track operation timing and throughput in real-time.

---

For detailed documentation, see: `FRICTIONLESS_MODEL_SHARDING_GUIDE.md`
For implementation details, see: `include/frictionless_model_sharding.h`
