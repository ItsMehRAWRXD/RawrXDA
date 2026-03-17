# GPU DLSS Quick Reference Guide

## File Structure

```
RawrXD-production-lazy-init/
├── src/masm/final-ide/
│   ├── gpu_dlss_abstraction.asm          # Main DLSS upscaling engine
│   ├── gpu_model_loader_optimized.asm    # GPU prefetching & quantization
│   ├── gpu_observability.asm             # Logging, metrics, tracing
│   └── gpu_dlss_tests.asm               # Comprehensive test suite
│
├── config/
│   └── gpu_config.toml                   # Configuration management
│
├── Dockerfile                            # Multi-GPU Docker image
├── GPU_DLSS_IMPLEMENTATION_GUIDE.md      # Detailed guide
└── GPU_DLSS_QUICK_REFERENCE.md          # This file
```

## Quick Start

### 1. Initialize GPU System

```c
// Auto-detect and select best GPU backend
int upscaler_id = dlss_upscaler_init(
    DLSS_QUALITY_BALANCED,    // Quality mode 0-4
    1920, 1080,               // Input resolution
    60                        // Target FPS
);

if (upscaler_id == -1) {
    // GPU initialization failed, fallback to CPU
}
```

### 2. Load Model with GPU Acceleration

```c
// Initialize GPU buffer pool (256MB)
gpu_buffer_pool_init(256 * 1024 * 1024, GPU_BACKEND_AUTO);

// Load model with INT8 quantization
int model_id = gpu_load_model_accelerated(
    model_id,
    file_handle,
    file_size,
    QUANT_INT8  // 75% compression
);
```

### 3. Process Frames

```c
for (frame_idx = 0; frame_idx < total_frames; frame_idx++) {
    // Upscale frame through DLSS
    int success = dlss_upscale_frame(
        upscaler_id,
        input_buffer_ptr,
        output_buffer_ptr,
        motion_vectors_ptr
    );
    
    if (!success) {
        // Fallback to CPU upscaling
    }
}
```

### 4. Monitor Performance

```c
// Get prefetch statistics
size_t total_bytes = gpu_get_prefetch_stats(...);

// Log structured event
obs_log(LOG_LEVEL_INFO, "GPU", "Model loaded successfully", trace_id);

// Export Prometheus metrics
obs_export_prometheus_metrics();
```

## Configuration Examples

### High Quality (RTX 4090+)
```toml
[gpu]
backend = "nvidia_dlss"
dlss_quality_mode = 0  # Ultra: 77% native res
dlss_target_fps = 120

[gpu.quantization]
quantization_level = 1  # FP16: good quality, 50% compression
```

### Balanced (RTX 3080)
```toml
[gpu]
backend = "auto"
dlss_quality_mode = 2  # Balanced: 58% native res
dlss_target_fps = 60

[gpu.quantization]
quantization_level = 2  # INT8: standard, 75% compression
```

### Budget (GTX 1080 Ti)
```toml
[gpu]
backend = "vulkan"
dlss_quality_mode = 3  # Performance: 50% native res
dlss_target_fps = 60

[gpu.quantization]
quantization_level = 3  # INT4: aggressive, 87.5% compression

[gpu.memory]
max_gpu_memory_mb = 4096  # 4GB limit
```

## Quality Modes Cheat Sheet

```
┌────────────────┬──────────┬────────┬─────────┐
│ Mode           │ Scale    │ Native │ Latency │
├────────────────┼──────────┼────────┼─────────┤
│ ULTRA          │ 1.30x    │ 77%    │ <2ms    │
│ HIGH           │ 1.50x    │ 66%    │ <2ms    │
│ BALANCED       │ 1.70x    │ 58%    │ <2.5ms  │
│ PERFORMANCE    │ 2.00x    │ 50%    │ <3ms    │
│ ULTRA_PERF     │ 3.00x    │ 33%    │ <4ms    │
└────────────────┴──────────┴────────┴─────────┘
```

## Quantization Types

```
┌─────────┬──────────┬─────────────┬──────────┐
│ Type    │ Compress │ Quality     │ Use Case │
├─────────┼──────────┼─────────────┼──────────┤
│ FP16    │ 50%      │ Excellent   │ GPU mem  │
│ INT8    │ 75%      │ Very Good   │ Standard │
│ INT4    │ 87.5%    │ Good        │ Budget   │
│ INT2    │ 93.75%   │ Fair        │ Extreme  │
└─────────┴──────────┴─────────────┴──────────┘
```

## Metric Names (Prometheus)

```
gpu_memory_used_bytes              # Current GPU memory
gpu_memory_max_bytes               # Total GPU memory
gpu_utilization_percent            # GPU compute usage
gpu_temperature_celsius            # GPU temperature

dlss_upscale_latency_ms            # Frame upscale time
dlss_quality_mode                  # Current quality setting
dlss_frames_processed_total        # Total frames upscaled

gpu_prefetch_bandwidth_mbps        # Model loading speed
gpu_prefetch_jobs_active           # Active prefetch jobs
gpu_prefetch_jobs_total_counter    # Total prefetch jobs

model_quantization_ratio           # Compression achieved
model_load_time_ms                 # Load duration
model_cache_hit_ratio              # Quantization cache hits
```

## Log Levels

```
DEBUG    - Detailed execution information (disabled in production)
INFO     - Normal operations (model loaded, upscale started)
WARNING  - Potential issues (high memory pressure, low bandwidth)
ERROR    - Failures (GPU allocation failed, upscale error)
CRITICAL - System failures (GPU driver crashed, OOM)
```

## Backend Detection Priority

```
1. NVIDIA CUDA (if RTX/Quadro detected)
   └─> DLSS upscaling available
   └─> TensorRT optimization available

2. AMD HIP (if RDNA detected)
   └─> FSR upscaling available
   └─> RDNA optimization available

3. Intel oneAPI (if ARC GPU detected)
   └─> XeSS upscaling available
   └─> DPC++ compute available

4. Vulkan (universal fallback)
   └─> Generic compute shaders
   └─> Always available

5. CPU Fallback (last resort)
   └─> Bilinear upsampling
```

## Common Patterns

### Pattern 1: Initialize & Upscale Loop
```c
// Setup
dlss_upscaler_init(DLSS_QUALITY_BALANCED, 1920, 1080, 60);

// Processing loop
while (has_frames) {
    render_to_input_buffer();
    dlss_upscale_frame(upscaler, input, output, motion_vecs);
    display_output_buffer();
}

// Cleanup
dlss_upscaler_shutdown(upscaler);
```

### Pattern 2: Load Model with Quantization
```c
// Initialize memory pool
gpu_buffer_pool_init(512 * 1024 * 1024, GPU_BACKEND_AUTO);

// Load and quantize model
gpu_load_model_accelerated(model_id, file, file_size, QUANT_INT8);

// Monitor loading progress
while (job_status == PENDING) {
    gpu_process_prefetch_jobs();
    sleep(10);  // 10ms delay
}

// Model ready for inference
inference_engine.load(model_id);
```

### Pattern 3: Monitoring with Prometheus
```c
// Setup observability
obs_init(NULL, "http://prometheus:9091", NULL);

// During operation
obs_log(LOG_LEVEL_INFO, "GPU", "Starting upscale", trace_id);

// Update metrics
movsd xmm0, latency_ms
obs_update_metric(dlss_latency_idx, xmm0);

// Export periodically
if (frame_count % 100 == 0) {
    obs_export_prometheus_metrics();
}
```

## Performance Tuning

### If Upscaling Latency > 5ms
1. Check `gpu_utilization_percent` metric
2. If >95%, reduce quality mode by 1
3. Profile individual shaders: `profile_kernel_times = true`

### If Prefetch Bandwidth < 1000 MB/s
1. Reduce prefetch chunk size: `prefetch_chunk_size = 2097152`
2. Check PCIe generation (Gen4 vs Gen3)
3. Verify no competing GPU workloads

### If GPU Memory Issues
1. Enable quantization: `quantization_enabled = true`
2. Lower quantization level (INT8 → INT4)
3. Reduce max concurrent models: `max_concurrent_models = 2`

### If Frequent Cache Misses
1. Increase cache size: `quant_cache_max_size = 2048`
2. Analyze access patterns in logs
3. Preload frequently used layers

## Docker Commands

```bash
# Build image
docker build -t rawrxd-gpu:latest .

# Run with all GPU resources
docker run --gpus all rawrxd-gpu:latest

# Run with specific GPU
docker run --gpus '"device=0"' rawrxd-gpu:latest

# Run with memory limits
docker run --gpus all -m 8g --cpus 8 rawrxd-gpu:latest

# Run with volume mounts
docker run --gpus all \
  -v /local/models:/app/models \
  -v /local/logs:/app/logs \
  rawrxd-gpu:latest

# Check logs
docker logs <container-id> | grep "ERROR\|CRITICAL"

# Monitor metrics (if Prometheus exposed)
curl http://localhost:9090/metrics
```

## Troubleshooting Checklist

- [ ] GPU drivers updated to latest version
- [ ] CUDA Toolkit installed (NVIDIA) / HIP SDK (AMD)
- [ ] Sufficient GPU memory (4GB minimum, 6GB recommended)
- [ ] PCIe configuration correct (Gen3/Gen4)
- [ ] No other GPU workloads competing (browsers, miners, etc.)
- [ ] Configuration file exists and readable
- [ ] Logs directory writable for output
- [ ] Network accessible to metrics endpoint
- [ ] DNS resolvable for tracing backend
- [ ] Firewall allows tracing/metrics ports

## Emergency Fallbacks

If GPU upscaling fails:
```c
if (dlss_upscale_frame(...) == 0) {
    // Fallback to CPU bilinear upsampling
    dlss_upscale_cpu(input, output);
}
```

If model loading fails:
```c
if (gpu_load_model_accelerated(...) == 0) {
    // Fallback to CPU inference
    load_model_cpu(model_id, file);
}
```

If memory allocation fails:
```c
if (gpu_buffer_pool_allocate(...) == 0) {
    // Trigger cache eviction
    gpu_evict_inactive_models();
    // Retry allocation
    gpu_buffer_pool_allocate(size, backend);
}
```

## Performance Targets

| Metric | Target | Notes |
|--------|--------|-------|
| DLSS Latency | <2.5ms | Per-frame upscaling time |
| Model Load (7B) | <150ms | With INT8 quantization |
| Prefetch Bandwidth | >500 MB/s | Min; >1000 MB/s ideal |
| GPU Utilization | 80-95% | Sweet spot efficiency |
| Cache Hit Rate | >90% | Quantization cache |
| Memory Pressure | <85% | Before eviction trigger |

## Support & Issues

**NVIDIA Issues**:
- Check CUDA Toolkit version compatibility
- Verify TensorRT library linked
- Try fallback to Vulkan backend

**AMD Issues**:
- Ensure HIP SDK installed
- Check ROCm version compatibility
- Verify RDNA generation GPU

**Intel Issues**:
- Install Intel oneAPI Base Toolkit
- Check DPC++ compiler version
- Verify ARC GPU firmware updated

**General**:
- Enable DEBUG logging: `log_level = "DEBUG"`
- Check system logs: `/app/logs/gpu_operations.log`
- Export metrics for analysis: `obs_export_prometheus_metrics()`

---

**Quick Links**:
- Full Guide: `GPU_DLSS_IMPLEMENTATION_GUIDE.md`
- Config: `config/gpu_config.toml`
- Tests: `src/masm/final-ide/gpu_dlss_tests.asm`
- Docker: `Dockerfile`

**Version**: 1.0.0 | **Updated**: 2025-12-30
