# Frictionless Model Sharding - Mathematical Reference

## Core Formulas

### 1. Optimal Shard Count Calculation

**Formula:**
$$\text{total\_shards} = \max(1, \text{num\_devices})$$

**Description:**
The optimal number of shards equals the number of available GPU devices. This ensures maximum parallelism while maintaining single-shard-per-device simplicity.

**Example:**
- Input: 800B model, 8 H100 GPUs
- Output: 8 shards × 100B = 800B total
- Load: ~1-2 seconds with parallel loading

### 2. Shard Size Calculation

**Formula:**
$$\text{shard\_size\_params} = \frac{\text{total\_parameters}}{\text{total\_shards}}$$

**Description:**
Divide total model parameters evenly across shards to ensure balanced GPU utilization.

**Example:**
- Input: 7B model, 1 GPU
- Output: 7B ÷ 1 = 7B shard
- Memory: 28GB (FP32: 4 bytes/param × 7B)

### 3. Bytes Per Parameter

**Formula:**
$$\text{bytes\_per\_param} = \text{precision\_bytes} \times \text{overhead\_factor}$$

Where:
- `precision_bytes`: 4 (FP32), 2 (FP16), 1 (Q8_0), 0.5 (Q4_0)
- `overhead_factor`: 1.1 (typical: 10% overhead for metadata)

**Example - FP32:**
$$4 \text{ bytes} \times 1.1 = 4.4 \text{ bytes/param}$$

**Example - Q4_0:**
$$0.5 \text{ bytes} \times 1.1 = 0.55 \text{ bytes/param}$$

**Calculation for 7B Model:**
- FP32: 7B × 4.4 bytes = 30.8 GB
- Q4_0: 7B × 0.55 bytes = 3.85 GB (8x compression)

### 4. Compression Recommendation (Logarithmic Scaling)

**Formula:**
$$\text{compression\_needed} = \frac{\text{model\_size\_bytes}}{\text{available\_memory\_bytes}}$$

$$\text{log\_compression} = \log_2(\text{compression\_needed}) + 1$$

**Snapping to Standard Levels:**
$$\text{compression\_final} = \begin{cases}
1.0 & \text{if } \log\_compression < 1.5 \\
2.0 & \text{if } 1.5 \leq \log\_compression < 2.5 \\
4.0 & \text{if } 2.5 \leq \log\_compression < 3.5 \\
8.0 & \text{if } 3.5 \leq \log\_compression < 4.5 \\
10.67 & \text{if } \log\_compression \geq 4.5
\end{cases}$$

**Example 1 - 7B on 40GB GPU:**
$$\text{compression\_needed} = \frac{28 \times 10^9}{40 \times 10^9} = 0.7$$
$$\log_2(0.7) + 1 = -0.515 + 1 = 0.485$$
$$\text{Result: 1.0 (no compression needed)}$$

**Example 2 - 7B on 5GB GPU:**
$$\text{compression\_needed} = \frac{28 \times 10^9}{5 \times 10^9} = 5.6$$
$$\log_2(5.6) + 1 = 2.486 + 1 = 3.486$$
$$\text{Result: 4.0 (Q8\_0 quantization)}$$

**Example 3 - 70B on 10GB GPU:**
$$\text{compression\_needed} = \frac{280 \times 10^9}{10 \times 10^9} = 28$$
$$\log_2(28) + 1 = 4.807 + 1 = 5.807$$
$$\text{Result: 8.0 (Q4\_0 quantization)}$$

### 5. Priority Assignment (Exponential Weighting)

**Formula:**
$$\text{position\_ratio} = \frac{\text{shard\_id}}{\text{total\_shards}}$$

$$\text{priority} = \lfloor (\text{position\_ratio})^2 \times 100 \rfloor$$

**Description:**
Earlier shards (lower IDs) receive exponentially higher priority. The squared ratio ensures dramatic priority differences for early shards while later shards have similar priority.

**Example - 8 Shards:**

| Shard | Position Ratio | Priority |
|-------|----------------|----------|
| 0 | 0.000 | 0 |
| 1 | 0.125 | 2 |
| 2 | 0.250 | 6 |
| 3 | 0.375 | 14 |
| 4 | 0.500 | 25 |
| 5 | 0.625 | 39 |
| 6 | 0.750 | 56 |
| 7 | 0.875 | 76 |

**Justification:**
Shard 0 contains layers 0-10 of a transformer. These layers process all tokens, making them critical for inference quality. Shard 7 contains later layers that are less sensitive to initialization delay.

### 6. Load Time Estimation

**Formula:**
$$\text{load\_time\_seconds} = \frac{\text{total\_size\_GB}}{\text{bandwidth\_gbps} \times \text{num\_devices}}$$

**Bandwidth Examples:**
- PCIe 4.0 x16: 32 GB/s per device
- NVLink (H100): 600 GB/s total bandwidth
- H100 to H100 over NVLink: 600 GB/s bidirectional

**Example 1 - 7B Model (28GB) on 1 GPU:**
$$\text{load\_time} = \frac{28}{32 \times 1} = 0.875 \text{ seconds}$$

**Example 2 - 800B Model (3,200GB) on 8 GPUs with PCIe:**
$$\text{load\_time} = \frac{3200}{32 \times 8} = 12.5 \text{ seconds}$$

**Example 3 - 800B Model (3,200GB) on 8 H100s with NVLink:**
$$\text{load\_time} = \frac{3200}{600 \times 1} = 5.3 \text{ seconds}$$

Note: NVLink provides one collective bandwidth pool (600 GB/s total), not per-device multiplication.

### 7. Memory Requirement Calculation

**Formula:**
$$\text{inference\_memory} = \text{model\_size} + \text{activation\_memory} + \text{kv\_cache\_memory}$$

Where:
- `model_size` = parameters × bytes_per_param
- `activation_memory` ≈ 0.1 × model_size
- `kv_cache_memory` = 2 × batch_size × context_length × hidden_dim × 4 bytes

**For a 7B Model with 4K Context:**
$$\text{model\_size} = 28 \text{ GB (FP32)}$$
$$\text{activation\_memory} = 2.8 \text{ GB}$$
$$\text{kv\_cache} = 2 \times 1 \times 4096 \times 4096 \times 4 = 0.5 \text{ GB}$$
$$\text{total} = 31.3 \text{ GB}$$

### 8. Cluster Distribution (Memory-Aware Round-Robin)

**Algorithm:**
```
For each shard:
    Find GPU with maximum available memory
    Assign shard to that GPU
    Subtract shard memory from GPU's available memory
```

**Example - Heterogeneous Cluster:**

Input:
- Shard 0: 100GB
- Shard 1: 100GB
- GPU 0: 80GB memory
- GPU 1: 40GB memory

Process:
```
Shard 0 (100GB) → GPU with max memory = GPU 0 (80GB) → FAIL
                  GPU with max memory = GPU 1 (40GB) → FAIL
                  
Result: Cannot fit without compression
```

With compression (4x):
- Shard 0: 25GB
- Shard 1: 25GB

```
Shard 0 (25GB) → GPU 0 (80GB available)
GPU 0: 80 - 25 = 55GB remaining

Shard 1 (25GB) → GPU 1 (40GB available)
GPU 1: 40 - 25 = 15GB remaining

Result: [GPU 0, GPU 1] assignment
```

### 9. Training Memory Requirement

**Formula:**
$$\text{training\_memory} = \text{model\_size} + \text{gradient\_memory} + \text{optimizer\_memory} + \text{activation\_memory}$$

With mixed precision:
- Model: 1× (FP16 = 2 bytes/param)
- Gradients: 1× (FP16 = 2 bytes/param)
- Optimizer state: 1× (FP32 = 4 bytes/param)
- Activations: 0.5×

Total multiplier: 0.5 + 0.5 + 1.0 + 0.5 = 2.5× base model size

**Example - Training 7B Model:**
$$\text{Base (FP32)} = 28 \text{ GB}$$
$$\text{Training (mixed)} = 28 \times 2.5 = 70 \text{ GB}$$

Minimum GPU requirement: 2× 40GB GPUs (80GB > 70GB)

### 10. Minimum GPUs Needed

**Formula:**
$$\text{min\_gpus} = \left\lceil \frac{\text{total\_memory\_needed}}{\text{gpu\_memory\_available}} \right\rceil$$

For inference:
$$\text{min\_gpus} = \left\lceil \frac{\text{model\_size} \times 1.15}{\text{gpu\_memory}} \right\rceil$$

For training:
$$\text{min\_gpus} = \left\lceil \frac{\text{model\_size} \times 2.5}{\text{gpu\_memory}} \right\rceil$$

**Example - 70B Model Inference on 40GB GPUs:**
$$\text{Memory needed} = 280 \text{ GB} \times 1.15 = 322 \text{ GB}$$
$$\text{min\_gpus} = \left\lceil \frac{322}{40} \right\rceil = 9$$

**Example - 70B Model Training on 80GB GPUs:**
$$\text{Memory needed} = 280 \text{ GB} \times 2.5 = 700 \text{ GB}$$
$$\text{min\_gpus} = \left\lceil \frac{700}{80} \right\rceil = 9$$

## Performance Scaling

### Linear Scaling with Number of Shards

For optimal loading:
$$\text{efficiency} = \frac{\text{sequential\_time}}{\text{parallel\_time} \times \text{num\_shards}}$$

**Ideal:** 100% efficiency (linear scaling)

**Realistic:** 85-95% efficiency with:
- Balanced GPU memory
- High-bandwidth interconnect (NVLink preferred)
- Adaptive loading strategy

### Throughput Analysis

**Effective bandwidth:**
$$B_{\text{eff}} = \frac{\text{model\_size\_GB}}{\text{load\_time\_seconds}}$$

**Example - 800B on 8 H100s:**
- Model size: 3,200 GB (FP32)
- Load time: 5.3 seconds
- Effective bandwidth: 603 GB/s ≈ NVLink capacity (optimized)

## Optimization Targets

### Goal 1: Minimize Load Time

Priority: Parallel strategy, max bandwidth
$$\min(T_{\text{load}}) = \frac{M}{B_{\text{total}}}$$

### Goal 2: Minimize Peak Memory

Priority: Sequential strategy, minimal peak
$$\min(M_{\text{peak}}) = \text{max\_shard\_size}$$

### Goal 3: Minimize Inference Latency

Priority: Adaptive strategy, priority-based
$$\min(T_{\text{first\_token}}) = T_{\text{load\_priority\_shards}} + \epsilon$$

### Goal 4: Optimize Quality vs. Compression

Priority: Logarithmic selection
$$\text{quality} = 100\% - 5\% \times \log_2(\text{compression})$$

## Key Insights

1. **Exponential Priority Formula**: Ensures early layers are always available for inference
2. **Logarithmic Compression**: Balances quality loss with memory savings
3. **Memory-Aware Distribution**: Prevents OOM errors in heterogeneous clusters
4. **Adaptive Loading**: Enables streaming inference during model loading
5. **Linear Scaling**: 8 GPUs ≈ 7.5x speedup vs. 1 GPU (due to 95% efficiency)

---

**For implementation details**: See `include/frictionless_model_sharding.h`
**For usage examples**: See `FRICTIONLESS_QUICK_REFERENCE.md`
**For API documentation**: See `FRICTIONLESS_MODEL_SHARDING_GUIDE.md`
