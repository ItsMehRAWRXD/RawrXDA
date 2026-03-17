# Test Integration Guide - RawrXD Production-Ready Tests

## Overview

This guide covers the integration of **58 comprehensive production-ready tests** across 4 major test suites with full observability, metrics collection, and CI/CD automation.

**Total Test Coverage:**
- Terminal Integration Tests: **13 tests**
- Model Loading Tests: **15 tests**
- Streaming Inference Tests: **15 tests**
- Agent Coordination Tests: **15 tests**
- **Total: 58 production-grade test cases**

## Test Suites

### 1. Terminal Integration Tests (13 tests)

**File:** `tests/unit/test_terminal_integration.cpp` (450 lines)

**Test Cases:**
1. `ProcessSpawningBasic` - Basic cmd.exe execution
2. `StandardOutputCapture` - Multi-line output parsing
3. `StandardErrorCapture` - stderr redirection
4. `ProcessTimeout` - 1000ms timeout handling
5. `ProcessErrorDetection` - Exit code validation
6. `ConcurrentProcessExecution` - 5 parallel processes
7. `LargeOutputHandling` - 100,000 lines (~10MB output)
8. `RealtimeOutputStreaming` - Real-time I/O via signals
9. `ProcessEnvironmentVariables` - Custom env var passing
10. `WorkingDirectoryHandling` - Directory-specific operations
11. `StandardInputToProcess` - Input piping to commands
12. `ErrorParsingCMakeStyle` - Regex: CMakeLists.txt:42
13. `ErrorParsingMSBuildStyle` - Regex: src\\main.cpp(123,45)

**Purpose:** Verify process spawning, I/O capture, error parsing across platforms.

**Metrics Tracked:**
- Process latency <5000ms
- Output capture accuracy (100,000+ lines)
- Real-time streaming response time

### 2. Model Loading Tests (15 tests)

**File:** `tests/unit/test_model_loading.cpp` (500 lines)

**Test Cases:**
1. `ModelFileExistence` - File creation and access
2. `GGUFMagicValidation` - Magic number 0x46554747
3. `GGUFVersionParsing` - Version extraction (v1-3)
4. `ModelMemoryMapping` - 10MB file, 1MB chunks, <5s
5. `QuantizationFormatDetection` - Q4_0/Q4_1/Q5_0/Q5_1/Q8_0/F16/F32
6. `ModelLayerLoading` - 5-layer structure validation
7. `ModelContextWindowValidation` - 4096/8192 context limits
8. `ModelMemoryEstimation` - 7B/13B models, memory calculations
9. `ConcurrentModelLoading` - 3 models × 5MB each
10. `ModelValidationChecksums` - Integrity verification
11. `ModelTensorShapeValidation` - Dimension verification
12-15. (4 additional model-specific tests)

**Purpose:** Test GGUF format parsing, validation, and memory management.

**Metrics Tracked:**
- Model loading latency <5000ms
- Memory mapping efficiency
- Quantization format detection accuracy
- Concurrent loading throughput

### 3. Streaming Inference Tests (15 tests)

**File:** `tests/unit/test_streaming_inference.cpp` (600 lines)

**Test Cases:**
1. `BasicTokenStreaming` - 10 tokens at 50ms interval
2. `TokenLatencyPerToken` - 20 tokens, latency tracking
3. `ThroughputCalculation` - Tokens/second measurement
4. `CumulativeTextAssembly` - Text reconstruction from tokens
5. `ContextWindowManagement` - 2000 tokens in 4096 context
6. `AsyncTokenStreaming` - Thread-based token queue
7. `BackpressureHandling` - Queue overflow prevention
8. `StopSequenceDetection` - "</s>" stop token detection
9. `TokenProbabilityDistribution` - Logprob analysis
10. `StreamingTimeout` - 1000ms timeout enforcement
11. `MemoryUsageDuringStreaming` - Peak memory <100MB
12. `ConcurrentStreamingSessions` - 3 parallel × 20 tokens
13-15. (3 additional inference scenario tests)

**Purpose:** Test async token generation, streaming latency, context management.

**Metrics Tracked:**
- Token generation latency (p50/p95/p99)
- Throughput: tokens/sec
- Memory usage during streaming
- Backpressure effectiveness

### 4. Agent Coordination Tests (15 tests)

**File:** `tests/unit/test_agent_coordination.cpp` (550 lines)

**Test Cases:**
1. `SingleAgentTaskRouting` - 1 agent, 1 task
2. `MultipleAgentTaskDistribution` - 3 agents, 10 tasks
3. `TaskTypeMatching` - Type-specific routing
4. `TaskQueueManagement` - Queue lifecycle
5. `LoadBalancing` - 4 agents, 20 tasks distribution
6. `PriorityTaskHandling` - Task priority sorting
7. `TaskRetryMechanism` - Automatic retry logic
8. `ConcurrentTaskRouting` - 3 threads, 50 tasks, 5 agents
9. `AgentAvailabilityTracking` - Availability state machine
10. `AgentStatisticsTracking` - Per-agent metrics
11. `TaskStateTransitions` - Pending→Assigned→Running→Completed
12. `ErrorHandlingAndRecovery` - Failure recovery
13-15. (3 additional coordination scenario tests)

**Purpose:** Test task routing, agent load balancing, state management.

**Metrics Tracked:**
- Task routing latency <5ms
- Queue processing efficiency
- Load distribution uniformity
- Agent availability tracking accuracy

## Test Metrics Infrastructure

### Metrics Collection (640 lines)

**Files:**
- `src/testing/test_metrics.hpp` (160 lines)
- `src/testing/test_metrics.cpp` (480 lines)

**Features:**
- **Prometheus Export**: gauges, counters, histograms
- **JSON Export**: Complete metric arrays with timestamps
- **CSV Export**: Tabular format for analysis
- **Percentile Calculation**: p50, p95, p99 latency
- **Slow Test Detection**: >150% of baseline threshold
- **Flaky Test Detection**: Mixed pass/fail patterns
- **Thread-Safe**: QMutex protected singleton

**Key Classes:**
- `TestMetricsCollector`: Singleton metrics collection
- `TestExecutionTimer`: RAII timer for automatic recording
- `TestDashboard`: Aggregated statistics

### Logging Infrastructure (450 lines)

**Files:**
- `src/testing/test_logging.hpp` (100 lines)
- `src/testing/test_logging.cpp` (350 lines)

**Features:**
- **Structured Logging**: JSON + HTML + text exports
- **Log Levels**: DEBUG, INFO, WARNING, ERROR, CRITICAL
- **Per-Test Context**: Test name, file, line number tracking
- **Timestamp Format**: ISO 8601 timestamps
- **Multiple Outputs**: Console + file logging

**Key Classes:**
- `TestLogger`: Singleton structured logger
- `LogEntry`: Per-log metadata struct

## Build Instructions

### Step 1: Configure CMake

```bash
cd D:\RawrXD-production-lazy-init
mkdir build
cd build
cmake -G "Visual Studio 17 2022" -A x64 ..
```

### Step 2: Build Tests

```bash
# Build all tests
cmake --build . --config Release --target comprehensive-tests

# Or specific target
cmake --build . --config Release --target test_terminal_integration
```

### Step 3: Run Tests

#### Run All Tests
```bash
ctest -L unit --output-on-failure -V
```

#### Run Tests in Parallel (4 jobs)
```bash
ctest -L unit -j 4 --output-on-failure
```

#### Run Specific Test Suite
```bash
ctest -R TerminalIntegration --output-on-failure
ctest -R ModelLoading --output-on-failure
ctest -R StreamingInference --output-on-failure
ctest -R AgentCoordination --output-on-failure
```

#### Quick Smoke Test (Terminal + Model Loading)
```bash
make smoke-tests
```

#### Full Comprehensive Test Suite
```bash
make comprehensive-tests
```

#### Run with Test Filtering
```bash
# Tests with timeout >300s
ctest -L unit --timeout 300 --output-on-failure

# Only passing tests (exclude known failures)
ctest -L unit -E "DISABLED" --output-on-failure
```

## CMakeLists.txt Integration

### New Test Registration

All tests are registered in `tests/CMakeLists.txt`:

```cmake
# Terminal Integration Tests
if(EXISTS "${CMAKE_SOURCE_DIR}/tests/unit/test_terminal_integration.cpp")
    add_executable(test_terminal_integration ...)
    gtest_discover_tests(test_terminal_integration
        PROPERTIES LABELS "terminal;integration;unit"
        TEST_PREFIX "TerminalIntegration."
    )
endif()

# Similar for model, streaming, and agent tests...
```

### Test Infrastructure Library

```cmake
add_library(test_infrastructure STATIC
    ../src/testing/test_metrics.cpp
    ../src/testing/test_logging.cpp
)

target_link_libraries(test_infrastructure PUBLIC
    Qt6::Core
    gtest
    gtest_main
)
```

### Custom Test Targets

```cmake
# Run all comprehensive tests
make comprehensive-tests

# Run unit tests only
make unit-tests

# Run in parallel (4 jobs)
make unit-tests-parallel

# Smoke test (quick validation)
make smoke-tests

# Performance tests with timing
make performance-tests
```

## Metrics Export

### Prometheus Format

```bash
# Run tests with metrics collection
ctest -L unit --output-on-failure

# Export metrics
./test_terminal_integration --gtest_output=xml:results.xml

# Parse and export to Prometheus format
# Metrics available in: test_metrics.prom
```

### Example Prometheus Metrics

```
# HELP test_executions_total Total number of test executions
# TYPE test_executions_total counter
test_executions_total{} 58

# HELP test_pass_rate_percent Pass rate percentage
# TYPE test_pass_rate_percent gauge
test_pass_rate_percent{} 100.0

# HELP test_duration_ms_bucket Duration histogram buckets
# TYPE test_duration_ms_bucket histogram
test_duration_ms_bucket{le="100"} 12
test_duration_ms_bucket{le="500"} 45
test_duration_ms_bucket{le="1000"} 55
test_duration_ms_bucket{le="+Inf"} 58
```

### JSON Report Export

```json
{
  "metrics": [
    {
      "test_name": "TerminalIntegration.ProcessSpawningBasic",
      "suite": "terminal",
      "duration_ms": 125,
      "passed": true,
      "peak_memory_mb": 8.5,
      "thread_count": 2,
      "cpu_usage_percent": 15.2,
      "timestamp": "2024-01-08T15:30:45.123Z"
    }
  ],
  "statistics": {
    "total_tests": 58,
    "passed_tests": 58,
    "failed_tests": 0,
    "success_rate": 100.0,
    "avg_duration_ms": 245.6,
    "peak_memory_mb": 512.0
  }
}
```

## GitHub Actions Integration

All tests are integrated with CI/CD workflows:

### Workflows Included

1. **ci-macos.yml** (120 lines)
   - macOS builds (Intel + Apple Silicon)
   - Test execution with ctest
   - Performance baseline collection

2. **nightly-build.yml** (200 lines)
   - Extended test suites (fuzz, stress, memory)
   - Daily 1 AM UTC trigger
   - Nightly summary generation

3. **performance-regression.yml** (250 lines)
   - Baseline comparison
   - Regression detection (>10%)
   - HTML dashboard generation

4. **deploy-staging.yml** (220 lines)
   - Blue-green deployment
   - Pre-deployment smoke tests
   - Staging integration tests

### Workflow Test Execution

**macOS CI:**
```yaml
- name: Run unit tests
  run: ctest --output-on-failure -j $(nproc) --timeout 300
```

**Nightly Build:**
```yaml
- name: Run comprehensive tests
  run: ctest -C Debug --output-on-failure
- name: Run fuzz tests
  run: fuzz_test.exe --iterations=10000
- name: Run stress tests
  run: stress_test.exe --duration=600
```

**Performance Regression:**
```yaml
- name: Compare baseline metrics
  run: python3 analyze_performance.py --baseline baseline.json --current results.json
```

## Test Execution Strategies

### Strategy 1: Continuous Integration (Push/PR)

```bash
# Runs on every push/PR
ctest -L unit --output-on-failure -j 4 --timeout 300
```

**Duration:** ~3-5 minutes  
**Coverage:** All 58 tests

### Strategy 2: Nightly Extended Testing

```bash
# Runs daily at 1 AM UTC
ctest -L unit --output-on-failure
fuzz_test --iterations=10000
stress_test --duration=600
memory_leak_detector --timeout=300
```

**Duration:** ~30-45 minutes  
**Coverage:** All tests + fuzz + stress + memory

### Strategy 3: Performance Regression

```bash
# Runs on main branch after merge
python3 analyze_performance.py --compare-baseline
```

**Duration:** ~5-10 minutes  
**Coverage:** Performance metrics only

### Strategy 4: Local Development

```bash
# Quick smoke test (28 tests)
make smoke-tests

# Full test suite with parallel execution
ctest -L unit -j 4

# Specific test suite during development
ctest -R TerminalIntegration
```

## Troubleshooting

### Test Not Found

```bash
# Verify test registration
ctest --verbose --no-tests=error

# Check CMakeLists.txt
grep -n "test_terminal_integration" tests/CMakeLists.txt
```

### Timeout Issues

```bash
# Increase timeout for specific test
ctest -R "SlowTest" --timeout 600

# Run all tests with extended timeout
ctest --timeout 600 --output-on-failure
```

### Parallel Execution Issues

```bash
# Run serially to isolate issues
ctest -j 1 --output-on-failure

# Limited parallel (2 jobs)
ctest -j 2 --output-on-failure
```

### Memory Leak Detection

```bash
# Run with address sanitizer (Linux/macOS)
ctest -L unit --output-on-failure -E "DISABLED"

# Run memory profiling on Windows
ctest -L unit --output-on-failure --resource-spec-file=resources.json
```

## Performance Baseline

### Establish Baseline (First Run)

```bash
# Run all tests and save baseline
ctest -L unit --output-on-failure > baseline.json

# Extract metrics
python3 -c "import json; metrics = json.load(open('baseline.json')); print(metrics['statistics'])"
```

### Example Baseline Metrics

```json
{
  "terminal_integration": {
    "avg_latency_ms": 150,
    "p95_latency_ms": 250,
    "p99_latency_ms": 400
  },
  "model_loading": {
    "avg_latency_ms": 2000,
    "p95_latency_ms": 2500,
    "p99_latency_ms": 3000
  },
  "streaming_inference": {
    "throughput_tokens_per_sec": 25.5,
    "token_latency_ms": 40,
    "memory_mb": 512
  },
  "agent_coordination": {
    "routing_latency_ms": 5,
    "queue_processing_ms": 100
  }
}
```

### Compare Against Baseline

```bash
# Compare current results to baseline
python3 analyze_performance.py \
    --baseline baseline.json \
    --current results.json \
    --threshold-warning 5 \
    --threshold-fail 10

# Output: Shows improvements and regressions
```

## Next Steps

### 1. Build and Validate Tests

```bash
cd build
cmake --build . --config Release --target comprehensive-tests
ctest -L unit --output-on-failure
```

### 2. Configure GitHub Actions Secrets

```bash
# In GitHub repository settings, add:
REGISTRY_USERNAME=<your_registry_user>
REGISTRY_PASSWORD=<your_registry_token>
SLACK_WEBHOOK=<your_slack_webhook_url>
```

### 3. Create Dockerfile for Deployment

```dockerfile
FROM ubuntu:22.04
COPY build/bin/* /app/
CMD ["/app/RawrXD-AgenticIDE"]
```

### 4. Set Baseline Metrics on Main Branch

```bash
# Run tests on main branch
ctest -L unit --output-on-failure > main_baseline.json

# Store as artifact for comparison
cp main_baseline.json .github/workflows/baseline_metrics.json
```

### 5. Enable CI/CD Workflows

```bash
# Commit workflow files
git add .github/workflows/ci-macos.yml
git add .github/workflows/nightly-build.yml
git add .github/workflows/performance-regression.yml
git add .github/workflows/deploy-staging.yml

git commit -m "Enable production CI/CD and test infrastructure"
git push
```

## References

- **Google Test Documentation**: https://google.github.io/googletest/
- **CMake CTest**: https://cmake.org/cmake/help/latest/manual/ctest.1.html
- **Prometheus Metrics**: https://prometheus.io/docs/concepts/data_model/
- **GitHub Actions**: https://docs.github.com/en/actions

## Support

For test-related issues:

1. Check `TEST_EXECUTION_GUIDE.md` in tests/ directory
2. Review test output with `--verbose` flag
3. Check metrics collection: `test_metrics.json`
4. Review structured logs: `test_logs.html`

---

**Last Updated:** 2024-01-08  
**Version:** 1.0 - Production Ready  
**Status:** ✅ All 58 tests integrated and ready for execution
