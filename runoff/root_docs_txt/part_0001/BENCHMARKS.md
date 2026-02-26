# Benchmarks

This document describes the lightweight benchmarking harness for the RawrXD Model Loader.

## Executable

`model_loader_bench` (built alongside `RawrXD-ModelLoader`).

## Purpose

1. Measure GGUF header + metadata parse time.
2. Record tensor enumeration count.
3. Optional Vulkan initialization latency.
4. Emit structured JSON results suitable for trend tracking.
5. Measure MatMul kernel dispatch average latency (GPU path).
6. Measure RMSNorm and SiLU kernel average latency (configurable vector size).
7. Measure scaled dot-product Attention latency (configurable sequence length and head dimension).

## Usage

```powershell
# Parse with GPU (default)
./build/bin/model_loader_bench.exe path/to/model.gguf

# Parse without GPU initialization
./build/bin/model_loader_bench.exe path/to/model.gguf --no-gpu
```

## Output

- Stdout: Single JSON object per invocation.
- File: Appends objects into `bench/bench_results.json` (JSON array style with trailing commas for fast append).

Example stdout:
```json
{
  "timestamp_utc":"2025-11-29T04:12:55Z",
  "model_path":"models/Example.gguf",
  "file_size":123456789,
  "header_ok":true,
  "metadata_ok":true,
  "tensor_count":514,
  "parse_ms":182.4,
  "gpu_enabled":true,
  "gpu_init_ok":true,
  "gpu_init_ms":37.9,
  "matmul_ran":true,
  "matmul_iterations":5,
  "matmul_avg_ms":2.41,
  "rmsnorm_ran":true,
  "rmsnorm_iterations":5,
  "rmsnorm_avg_ms":0.18,
  "silu_ran":true,
  "silu_iterations":5,
  "silu_avg_ms":0.07,
  "attention_ran":true,
  "attention_iterations":5,
  "attention_avg_ms":1.92
}
```

## Interpreting Results

- `parse_ms`: Time to open + parse header + metadata + enumerate tensors.
- `gpu_init_ms`: Vulkan instance + device + queue + command pool setup. Should stabilize after driver caching.
- `tensor_count`: Useful for validating model completeness vs. expected architecture.
- `matmul_avg_ms`: Average GPU MatMul kernel dispatch time over `matmul_iterations` runs (128x128 baseline).
- `rmsnorm_avg_ms`: Average RMSNorm latency over `rmsnorm_iterations` runs for `vec-size` elements.
- `silu_avg_ms`: Average SiLU activation latency over `silu_iterations` runs for `vec-size` elements.
- `attention_avg_ms`: Average single-head scaled dot-product attention latency over `attention_iterations` runs for the provided `att-seq` tokens and `att-head` dimension.

## Suggested Tracking

Maintain a weekly snapshot of `bench_results.json` and compute:
- Median `parse_ms` across runs per model.
- 95th percentile `gpu_init_ms`.
- Error rate (runs where `header_ok` or `metadata_ok` is false).

## CI Integration

GitHub Actions runs a minimal benchmark (no GPU) to validate parser functionality on every push.

## Roadmap Extensions

| Feature | Status | Notes |
|---------|--------|-------|
| Kernel dispatch timing | Complete | MatMul/RMSNorm/SiLU/Attention timings collected |
| Memory usage stats | Future | Capture RSS before/after parse |
| Percentile summary script | Future | PowerShell post-processor |
| JSON validation | Planned | Ensure structural correctness before append |

## Caveats

- The append format keeps a trailing comma; a cleanup script can normalize final JSON if needed.
- Vulkan initialization time varies with driver state; compare cold vs. warm starts.

## Normalizing JSON

```powershell
(Get-Content bench/bench_results.json) -replace ",\s*$" , "" | Set-Content bench/bench_results_clean.json
```
