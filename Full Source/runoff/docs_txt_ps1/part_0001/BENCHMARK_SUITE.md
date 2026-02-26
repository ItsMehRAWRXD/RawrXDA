# RawrXD Benchmark Suite - Complete Utility Guide

## Overview

The RawrXD Benchmark Suite is a **production-ready** set of comprehensive performance testing utilities for the AI completion engine. It measures real-world performance across multiple dimensions:

- **Latency**: Cold start, warm cache, and cached performance
- **Throughput**: Requests per second under load
- **Multi-Language Support**: C++, Python, JavaScript, TypeScript, Rust
- **Context Awareness**: Performance with full context windows
- **GPU Acceleration**: Vulkan compute shader performance vs CPU
- **Memory Profiling**: Model loading and caching overhead

## Quick Start

### Windows (PowerShell)

```powershell
# List available benchmarks
.\run_benchmarks.ps1 -ListOnly

# Run all benchmarks with default model
.\run_benchmarks.ps1

# Run with custom model
.\run_benchmarks.ps1 -ModelPath "path/to/model.gguf"

# Rebuild and run
.\run_benchmarks.ps1 -Rebuild -ModelPath "path/to/model.gguf"

# Verbose output
.\run_benchmarks.ps1 -Verbose
```

### Linux/macOS (Python)

```bash
# List available benchmarks
python3 run_benchmarks.py --list

# Run all benchmarks
python3 run_benchmarks.py

# Run with custom model
python3 run_benchmarks.py -m path/to/model.gguf

# Verbose output
python3 run_benchmarks.py -v
```

## Benchmark Executable

### Main: `benchmark_completions.exe` (Production Version)

The primary benchmark executable with 8 comprehensive tests:

```bash
benchmark_completions.exe [options]

Options:
  -m <path>        Path to GGUF model file
  -q, --quiet      Suppress verbose output  
  --cpu-only       Disable GPU acceleration
  -h, --help       Show help message
```

#### Test 1: Cold Start Latency
- **What**: First model load + initial inference requests
- **Why**: Measures startup performance
- **Target**: < 1000ms per request
- **Metrics**: Min, avg, max, P95, P99 latency

#### Test 2: Warm Cache Performance
- **What**: Completion requests after model is warmed up
- **Why**: Measures typical runtime performance
- **Target**: < 50ms average latency
- **Metrics**: Cache hit rate, throughput

#### Test 3: Rapid-Fire Stress Test
- **What**: 100 sequential completion requests
- **Why**: Tests burst capacity and consistency
- **Target**: > 2 requests/second
- **Metrics**: Throughput, success rate, latency distribution

#### Test 4: Multi-Language Support
- **What**: Completions for C++, Python, JS, TS, Rust
- **Why**: Validates language-agnostic quality
- **Target**: > 80% success rate
- **Metrics**: Per-language latency and accuracy

#### Test 5: Context-Aware Completions
- **What**: Completions with full function context
- **Why**: Tests context window utilization
- **Target**: < 500ms, accurate suggestions
- **Metrics**: Latency with context, suggestion quality

#### Test 6: Multi-Line Function Generation
- **What**: Generate multiple lines of code
- **Why**: Tests structural code completion
- **Target**: Complete function structures
- **Metrics**: Generation size, correctness

#### Test 7: GPU Acceleration
- **What**: Vulkan compute shader performance
- **Why**: Measures GPU speedup vs CPU
- **Target**: 2-5x speedup on modern GPUs
- **Metrics**: GPU vs CPU latency comparison

#### Test 8: Memory Profiling
- **What**: Model memory footprint and caching overhead
- **Why**: Evaluates resource efficiency
- **Target**: < 500MB peak memory (Q4 quantization)
- **Metrics**: Peak memory, cache overhead

## Output Files

### `benchmark_results.json`
Per-test results in JSON format for CI/CD integration:

```json
{
  "timestamp": "2025-12-13T14:30:45",
  "model": "path/to/model.gguf",
  "total_tests": 8,
  "passed_tests": 8,
  "execution_time_sec": 45.23,
  "tests": [
    {
      "name": "Cold Start",
      "avg_latency_ms": 125.5,
      "p95_latency_ms": 145.2,
      "p99_latency_ms": 156.8,
      "success_rate": 100.0,
      "passed": true
    },
    ...
  ]
}
```

### `benchmark_suite_results.json`
Summary of all benchmark runs (from runner script):

```json
{
  "timestamp": "2025-12-13T14:30:45",
  "total_time_sec": 156.4,
  "benchmark_count": 3,
  "passed": 3,
  "results": {
    "benchmark_completions.exe": {
      "Success": true,
      "ExitCode": 0
    },
    ...
  }
}
```

## Building from Source

### Prerequisites
- CMake 3.20+
- Visual Studio 2022 (Windows)
- Qt 6.7.3+
- GGML + Vulkan support
- Python 3.7+ (for runner scripts)

### Build Steps

```bash
# Configure (Windows)
cmake -G "Visual Studio 17 2022" -A x64 -DCMAKE_BUILD_TYPE=Release -B build

# Build
cmake --build build --config Release

# Build only benchmarks
cmake --build build --config Release --target benchmark_completions
```

### Troubleshooting Build Issues

**Issue**: Qt not found
```bash
# Set Qt path
cmake -DQt6_DIR="C:/Qt/6.7.3/msvc2022_64/lib/cmake/Qt6" -B build
```

**Issue**: GGML compilation errors
```bash
# Ensure Vulkan headers are available
# Update 3rdparty/ggml/include/ggml-vulkan.h
```

**Issue**: Linker errors
```bash
# Check all dependencies are built
cmake --build build --config Release --target ALL_BUILD
```

## Performance Analysis Guide

### Interpreting Results

**Latency Metrics**
- **Avg**: Most representative of typical performance
- **P95/P99**: Tail latencies (outliers)
- **Min/Max**: Best/worst case scenarios

**Targets** (Cursor-killer performance)
- Cold Start: < 1000ms (initial load)
- Warm Cache: < 50ms (typical use)
- Rapid Fire: > 2 req/sec (burst capacity)
- Multi-Language: > 80% success
- GPU: 2-5x faster than CPU
- Memory: < 500MB (Q4 quantization)

### Performance Comparison

Run with different models:

```bash
# Small model (faster, less accurate)
.\run_benchmarks.ps1 -ModelPath models/phi-2-q4.gguf

# Medium model (balanced)
.\run_benchmarks.ps1 -ModelPath models/mistral-7b-q4.gguf

# Large model (slower, more accurate)
.\run_benchmarks.ps1 -ModelPath models/mixtral-8x7b-q4.gguf
```

Compare `benchmark_results.json` outputs:

```powershell
# Quick comparison
$results1 = Get-Content "benchmark_results.json" | ConvertFrom-Json
$results1.tests | Select-Object name, avg_latency_ms, success_rate | Format-Table
```

### GPU vs CPU Profiling

Test GPU acceleration specifically:

```bash
# CPU-only (baseline)
benchmark_completions.exe -m model.gguf --cpu-only

# GPU-enabled (with Vulkan)
benchmark_completions.exe -m model.gguf
```

Compare latencies to measure speedup factor.

## Continuous Integration

### GitHub Actions Example

```yaml
name: AI Benchmark Suite
on: [push, pull_request]

jobs:
  benchmark:
    runs-on: windows-latest
    steps:
      - uses: actions/checkout@v3
      
      - name: Build benchmarks
        run: |
          cmake -B build -G "Visual Studio 17 2022" -A x64
          cmake --build build --config Release --target benchmark_completions
      
      - name: Run benchmarks
        run: |
          .\run_benchmarks.ps1 -ModelPath models/test-model.gguf
      
      - name: Upload results
        uses: actions/upload-artifact@v3
        with:
          name: benchmark-results
          path: |
            benchmark_results.json
            benchmark_suite_results.json
```

## Advanced Usage

### Custom Test Configurations

Modify `benchmark_completions_full.cpp` to add custom tests:

```cpp
void BenchmarkCustom() {
    LogTest("9/9", "Custom Performance Test");
    
    // Your test code here
    std::vector<double> latencies;
    
    // ... measurement code ...
    
    BenchmarkResult result = CalculateStats("Custom", latencies, success, total);
    results_.push_back(result);
}
```

### Profiling with Instruments

**Windows** (Visual Studio Profiler):
```bash
# Build with debug info
cmake -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build

# Profile
VsPerfCmd.exe -start:sample -output:profile.vsp benchmark_completions.exe
```

**Linux** (perf):
```bash
# Profile
perf record -g ./benchmark_completions models/model.gguf
perf report
```

### Memory Profiling

**Windows** (Valgrind):
```bash
# With memory leak detection
drmemory.exe -app benchmark_completions.exe models/model.gguf
```

## Troubleshooting

### "Model not found" Error

```
✗ Model not found at: models/ministral-3b-instruct-v0.3-Q4_K_M.gguf
```

**Solution**: Download a model from HuggingFace:
```bash
# Using huggingface-cli
huggingface-cli download TheBloke/Mistral-7B-Instruct-v0.1-GGUF \
  mistral-7b-instruct-v0.1.Q4_K_M.gguf --local-dir models
```

### Timeout Errors

```
❌ Timeout: benchmark_completions exceeded 5 minutes
```

**Solution**: Increase timeout or use a smaller model:
```powershell
.\run_benchmarks.ps1 -TimeoutMin 10
```

### Memory Exhaustion

```
✗ ERROR: Failed to load model - insufficient memory
```

**Solution**: 
- Use a quantized model (Q2, Q3, Q4 instead of Q5, Q6, Q8)
- Reduce context window
- Close other applications

### GPU Not Detected

```
ℹ GPU not available - using CPU inference
```

**Check**:
- Vulkan drivers installed (`vulkaninfo`)
- GGML built with Vulkan support
- GPU device available (`nvidia-smi` or equivalent)

## Performance Benchmarking Tips

1. **Warm up first**: Run a dummy completion before measuring
2. **Consistent environment**: Close background applications
3. **Multiple runs**: Average results over 3+ runs
4. **Same hardware**: Use identical systems for fair comparison
5. **Model cache**: Pre-warm model before stress tests
6. **Monitor**: Watch CPU/GPU/Memory during tests

## Results Interpretation

### Excellent Performance (✅ All PASS)
- Cold Start: < 500ms
- Warm Cache: < 20ms  
- Rapid Fire: > 10 req/sec
- All languages: > 95% success
- Memory: < 300MB

### Good Performance (✅ PASS)
- Cold Start: 500-1000ms
- Warm Cache: 20-50ms
- Rapid Fire: > 2 req/sec
- Languages: > 80% success
- Memory: 300-500MB

### Acceptable Performance (⚠ WARNING)
- Cold Start: 1000-2000ms
- Warm Cache: 50-100ms
- Rapid Fire: > 1 req/sec
- Languages: > 60% success
- Memory: 500-1000MB

### Poor Performance (❌ FAIL)
- Cold Start: > 2000ms
- Warm Cache: > 100ms
- Rapid Fire: < 1 req/sec
- Languages: < 60% success
- Memory: > 1000MB

## Support & Contribution

For issues or improvements:
1. Check existing benchmark outputs
2. Review AI Toolkit Production Readiness instructions
3. Submit detailed performance data
4. Include system specs and model information

---

**Last Updated**: December 13, 2025  
**Version**: 2.0 (Production Ready)  
**Maintainer**: RawrXD Team
