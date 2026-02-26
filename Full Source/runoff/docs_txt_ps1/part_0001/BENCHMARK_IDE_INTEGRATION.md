/**
 * \file BENCHMARK_IDE_INTEGRATION.md
 * \brief Integration guide for benchmark menu system in RawrXD IDE
 * \author RawrXD Team
 * \date 2025-12-13
 *
 * This document explains how to integrate the benchmark menu system into
 * the RawrXD-Win32IDE and other Qt-based IDE applications.
 */

# RawrXD Benchmark Menu Integration Guide

## Overview

The benchmark menu system provides:
- **Menu dropdown** under "Tools → Benchmarks" for IDE access
- **Test selector widget** to choose which benchmarks to run
- **Real-time logging output** with color-coded messages
- **Progress tracking** and detailed results
- **Background thread execution** to keep IDE responsive

## Files Added

### Headers
- `include/benchmark_menu_widget.hpp` - Main UI components
- `include/benchmark_runner.hpp` - Background benchmark executor

### Implementation
- `src/benchmark_menu_widget.cpp` - UI implementation with Qt layouts
- `src/benchmark_runner.cpp` - Real benchmark execution logic

## Integration Steps

### Step 1: Add to CMakeLists.txt

The files are already added to the RawrXD-Win32IDE target:

```cmake
add_executable(RawrXD-Win32IDE 
    ...
    src/benchmark_menu_widget.cpp
    src/benchmark_runner.cpp
    ...
)
```

### Step 2: Include in IDE Main Window

In your main IDE window class (e.g., `Win32IDE.h`):

```cpp
#include "benchmark_menu_widget.hpp"

class Win32IDE : public QMainWindow {
    Q_OBJECT
    
private:
    std::unique_ptr<BenchmarkMenu> benchmarkMenu_;
    // ... other members ...
};
```

### Step 3: Initialize in Constructor

In the IDE's main window constructor:

```cpp
Win32IDE::Win32IDE(QWidget* parent)
    : QMainWindow(parent) {
    
    // ... other initialization ...
    
    // Initialize benchmark menu
    benchmarkMenu_ = std::make_unique<BenchmarkMenu>(this);
    
    // ... rest of initialization ...
}
```

## User Interface Components

### BenchmarkSelector Widget

Allows users to select which tests to run:

```
┌──────────────────────────────────────────┐
│ BENCHMARK TESTS                          │
├──────────────────────────────────────────┤
│ ☑ Cold Start Latency                     │
│ ☑ Warm Cache Performance                 │
│ ☑ Rapid-Fire Stress Test                 │
│ ☑ Multi-Language Support                 │
│ ☑ Context-Aware Completions              │
│ ☑ Multi-Line Function Generation         │
│ ☑ GPU Acceleration                       │
│ ☑ Memory Profiling                       │
│                                          │
│ [Select All] [Deselect All]              │
├──────────────────────────────────────────┤
│ CONFIGURATION                            │
├──────────────────────────────────────────┤
│ Model: [Default]                         │
│ ☑ Enable GPU Acceleration (Vulkan)       │
│ ☑ Verbose Output                         │
└──────────────────────────────────────────┘
```

### BenchmarkLogOutput Widget

Real-time colored logging output:

```
[14:23:45] INFO  ═══════════════════════════════════════════════════════
[14:23:45] ✓     RawrXD Benchmark Suite Starting
[14:23:45] INFO  ═══════════════════════════════════════════════════════
[14:23:45] INFO  
[14:23:45] INFO  Model: models/ministral-3b-instruct-v0.3-Q4_K_M.gguf
[14:23:45] INFO  GPU: Enabled
[14:23:45] INFO  Verbose: No
[14:23:45] INFO  
[14:23:45] INFO  Running 8 tests...
[14:23:45] INFO  
[14:23:46] INFO  [1/8] Running: cold_start
[14:23:48] ✓     ✅ PASS | Avg: 125.50ms | P95: 145.20ms | Success: 100%
[14:23:48] INFO  
[14:23:49] INFO  [2/8] Running: warm_cache
[14:23:50] ✓     ✅ PASS | Avg: 18.75ms | P95: 22.10ms | Success: 100%
```

Color codes:
- **Gray** (`#808080`): DEBUG level
- **Blue** (`#569cd6`): INFO level
- **Green** (`#6a9955`): SUCCESS level
- **Yellow** (`#dcdcaa`): WARNING level
- **Red** (`#f48771`): ERROR level

### BenchmarkResultsDisplay Widget

Displays progress and summary:

```
┌──────────────────────────────────────────┐
│ Overall Progress:                        │
│ [========════════────────] 6/8 (75%)     │
├──────────────────────────────────────────┤
│ Results Summary:                         │
│                                          │
│ ✅ Cold Start - Avg: 125.50ms, P95: ... │
│ ✅ Warm Cache - Avg: 18.75ms, P95: ...  │
│ ✅ Rapid Fire - Avg: 45.20ms, P95: ...  │
│ ✅ Multi-Lang - Avg: 52.10ms, P95: ...  │
│ ✅ Context    - Avg: 78.50ms, P95: ...  │
│ ✅ Multi-Line - Avg: 95.30ms, P95: ...  │
│ ⏳ GPU Accel  - Running...               │
│ ⏳ Memory     - Running...               │
└──────────────────────────────────────────┘
```

## Menu Integration

The benchmark menu is automatically added to the IDE's menu bar:

```
File  Edit  View  Tools  Help
                  ├─ Build
                  ├─ Benchmarks  ◄─ New menu
                  │  ├─ Run Benchmarks...
                  │  ├─ ─────────────────
                  │  └─ View Results
                  └─ ...
```

Clicking "Run Benchmarks..." opens the benchmark dialog with:
- Test selection on the left
- Logging output on the right
- Progress tracking at the bottom
- Results summary displaying live

## Real Test Execution

The `BenchmarkRunner` class executes actual benchmarks:

```cpp
class BenchmarkRunner : public QObject {
    Q_OBJECT
public:
    void runBenchmarks(const std::vector<std::string>& selectedTests,
                       const QString& modelPath,
                       bool gpuEnabled,
                       bool verbose);
    
signals:
    void started();
    void testStarted(const QString& testName);
    void progress(int current, int total);
    void testCompleted(const QString& testName, bool passed, double latencyMs);
    void finished(int passed, int total, double executionTimeSec);
    void error(const QString& errorMessage);
    void logMessage(const QString& message, int level);
};
```

## Test Implementations

Each benchmark test is implemented as a separate method:

1. **testColdStart()** - Initial model load + first inference
2. **testWarmCache()** - Completion latency after warmup
3. **testRapidFire()** - 50 sequential requests
4. **testMultiLanguage()** - 5 language variants
5. **testContextAware()** - With full context window
6. **testMultiLine()** - Structural code generation
7. **testGPUAcceleration()** - GPU vs CPU performance
8. **testMemory()** - Model memory footprint

Each returns a `BenchmarkTestResult` with:
- Test name
- Pass/fail status
- Latency metrics (min, avg, max, P95, P99, median)
- Success rate
- Throughput
- Notes/observations

## Signal Flow

### Running Benchmarks

```
User clicks "Run Benchmarks"
    ↓
BenchmarkMenu::runSelectedBenchmarks()
    ↓
Collect selected tests from BenchmarkSelector
    ↓
BenchmarkRunner::runBenchmarks() [in background thread]
    ↓
For each test:
    • emit testStarted()
    • Execute test logic
    • Calculate statistics
    • emit testCompleted()
    • BenchmarkLogOutput::logTestResult()
    • BenchmarkResultsDisplay::addResult()
    ↓
BenchmarkRunner::emit finished()
    ↓
Print summary and enable results export
```

### Logging Flow

```
Test execution logic
    ↓
BenchmarkRunner::log() [thread-safe]
    ↓
emit logMessage(message, level)
    ↓
BenchmarkLogOutput::logMessage()
    ↓
Format with timestamp + color
    ↓
Display in QTextEdit
    ↓
Auto-scroll to bottom
```

## Example: Custom Integration

To manually integrate into an existing IDE:

```cpp
// In your IDE main window class
class MyIDE : public QMainWindow {
    Q_OBJECT
    
public:
    MyIDE(QWidget* parent = nullptr) : QMainWindow(parent) {
        // Create central widget
        auto central = new QWidget(this);
        // ... setup your UI ...
        setCentralWidget(central);
        
        // Initialize benchmark menu
        benchmarkMenu_ = std::make_unique<BenchmarkMenu>(this);
    }
    
private:
    std::unique_ptr<BenchmarkMenu> benchmarkMenu_;
};
```

## Configuration

### Model Selection

Benchmarks support different models via dropdown:

```cpp
modelCombo_->addItem("Ministral 3B", 
    "models/ministral-3b-instruct-v0.3-Q4_K_M.gguf");
modelCombo_->addItem("Mistral 7B", 
    "models/mistral-7b-Q4_K_M.gguf");
modelCombo_->addItem("Custom...", "");
```

Users can type custom model paths.

### GPU Acceleration

```cpp
gpuCheckbox_->setChecked(true);  // Enable by default
// BenchmarkRunner checks this flag and uses GPU if available
```

### Verbose Output

```cpp
verboseCheckbox_->setChecked(false);  // Off by default
// Adds DEBUG level messages when enabled
```

## Output Files

After running benchmarks, generates:

**benchmark_results.json** - Per-test detailed results
**benchmark_suite_results.json** - Overall summary

Format compatible with CI/CD pipelines.

## Error Handling

### Model Not Found

```
[14:23:45] ✗ ERROR: Failed to load model: models/model.gguf
[14:23:45] INFO  You can download models from:
[14:23:45] INFO  • https://huggingface.co/TheBloke/...
```

### Test Timeout

```
[14:24:15] ⚠ WARNING: Test exceeded timeout
[14:24:15] ⚠ WARNING: Consider using a smaller model
```

### Resource Exhaustion

```
[14:24:30] ✗ ERROR: Insufficient memory for model
[14:24:30] INFO  Try: Use Q2/Q3 quantization instead of Q8
```

## Performance Considerations

- **Background execution**: Tests run on separate thread
- **Live logging**: Output updates in real-time via signals
- **Responsive UI**: IDE remains responsive during benchmarks
- **Memory efficient**: Results streamed to display
- **Progress tracking**: Accurate time estimates

## Future Enhancements

Potential improvements:

1. **Comparison Mode** - Compare results across models
2. **Export Options** - CSV, graphs, detailed reports
3. **Scheduling** - Run benchmarks on a schedule
4. **Remote Execution** - Run on remote machines
5. **Performance History** - Track trends over time
6. **Regression Detection** - Alert on performance regressions

## Troubleshooting

### Benchmarks not appearing in menu

Check:
1. `BenchmarkMenu` is initialized in IDE constructor
2. MenuBar exists and has actions
3. Qt MOC is running (Q_OBJECT macro)
4. Rebuild with `cmake --build . --target clean`

### Crashes during execution

Check:
1. Model file path is valid
2. Qt is properly initialized before benchmark menu
3. Memory is sufficient for model
4. Check console output for stack traces

### Slow execution

Optimize:
1. Use smaller quantized models (Q2, Q3, Q4)
2. Reduce number of test iterations
3. Disable verbose output
4. Close other applications

## Support

For issues:
1. Check detailed log output
2. Review test-specific notes
3. Consult BENCHMARK_SUITE.md for interpretation
4. File issues with benchmark_results.json attached

---

**Last Updated**: December 13, 2025  
**Version**: 1.0  
**Status**: Production Ready
