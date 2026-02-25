#!/bin/bash
# benchmark-quick-start.sh
# RawrXD Benchmark Suite - Quick Start Guide
# Windows PowerShell version at the end

cat << 'EOF'
╔════════════════════════════════════════════════════════════════════════════╗
║                  RawrXD BENCHMARK SUITE - QUICK START                     ║
║                    Complete IDE & Standalone Utilities                    ║
╚════════════════════════════════════════════════════════════════════════════╝

┌────────────────────────────────────────────────────────────────────────────┐
│ LAUNCHING FROM IDE (RawrXD-Win32IDE)                                       │
├────────────────────────────────────────────────────────────────────────────┤
│                                                                            │
│  1. Open RawrXD-Win32IDE.exe                                              │
│  2. Click: Tools → Benchmarks → Run Benchmarks...                         │
│  3. Select tests you want to run:                                         │
│     □ Cold Start Latency         (first model load + inference)           │
│     □ Warm Cache Performance     (cached completions <10ms)               │
│     □ Rapid-Fire Stress Test     (100+ burst requests)                    │
│     □ Multi-Language Support     (C++, Python, JS, TS, Rust)              │
│     □ Context-Aware Completions  (full context window)                    │
│     □ Multi-Line Generation      (structural code completion)             │
│     □ GPU Acceleration           (Vulkan compute performance)             │
│     □ Memory Profiling           (model footprint + caching)              │
│                                                                            │
│  4. Configure options:                                                     │
│     Model: [ministral-3b-q4.gguf] (or select custom)                      │
│     ✓ Enable GPU Acceleration (Vulkan)                                    │
│     ○ Verbose Output                                                      │
│                                                                            │
│  5. Click "Run Benchmarks"                                                │
│  6. Monitor real-time output with color-coded logging                     │
│  7. View detailed results as tests complete                               │
│  8. Export results automatically (benchmark_results.json)                 │
│                                                                            │
└────────────────────────────────────────────────────────────────────────────┘

┌────────────────────────────────────────────────────────────────────────────┐
│ STANDALONE EXECUTION (PowerShell)                                          │
├────────────────────────────────────────────────────────────────────────────┤
│                                                                            │
│  # List available benchmarks                                              │
│  .\run_benchmarks.ps1 -ListOnly                                           │
│                                                                            │
│  # Run all benchmarks with default model                                  │
│  .\run_benchmarks.ps1                                                     │
│                                                                            │
│  # Run with custom model                                                  │
│  .\run_benchmarks.ps1 -ModelPath "models/mistral-7b-q4.gguf"              │
│                                                                            │
│  # Verbose output + rebuild                                               │
│  .\run_benchmarks.ps1 -Verbose -Rebuild                                   │
│                                                                            │
│  # Increase timeout for large models                                      │
│  .\run_benchmarks.ps1 -TimeoutMin 15                                      │
│                                                                            │
└────────────────────────────────────────────────────────────────────────────┘

┌────────────────────────────────────────────────────────────────────────────┐
│ STANDALONE EXECUTION (Python)                                              │
├────────────────────────────────────────────────────────────────────────────┤
│                                                                            │
│  # List available benchmarks                                              │
│  python3 run_benchmarks.py --list                                         │
│                                                                            │
│  # Run all benchmarks                                                     │
│  python3 run_benchmarks.py                                                │
│                                                                            │
│  # Run with specific model                                                │
│  python3 run_benchmarks.py -m models/mistral-7b-q4.gguf                   │
│                                                                            │
│  # Custom directory and verbose                                           │
│  python3 run_benchmarks.py -d /path/to/RawrXD-ModelLoader -v              │
│                                                                            │
└────────────────────────────────────────────────────────────────────────────┘

┌────────────────────────────────────────────────────────────────────────────┐
│ COMMAND-LINE BENCHMARK EXECUTABLE                                          │
├────────────────────────────────────────────────────────────────────────────┤
│                                                                            │
│  # Run with default model                                                 │
│  benchmark_completions.exe                                                │
│                                                                            │
│  # Specify model path                                                     │
│  benchmark_completions.exe -m models/mistral-7b-q4.gguf                   │
│                                                                            │
│  # CPU-only (no GPU)                                                      │
│  benchmark_completions.exe --cpu-only                                     │
│                                                                            │
│  # Quiet mode (minimal output)                                            │
│  benchmark_completions.exe -q                                             │
│                                                                            │
│  # Show help                                                              │
│  benchmark_completions.exe -h                                             │
│                                                                            │
└────────────────────────────────────────────────────────────────────────────┘

┌────────────────────────────────────────────────────────────────────────────┐
│ OUTPUT & RESULTS                                                            │
├────────────────────────────────────────────────────────────────────────────┤
│                                                                            │
│  benchmark_results.json                                                    │
│  ├─ timestamp: when tests ran                                             │
│  ├─ model: GGUF file used                                                 │
│  ├─ total_tests: number of tests executed                                │
│  ├─ passed_tests: how many passed                                         │
│  ├─ execution_time_sec: total duration                                    │
│  └─ tests[]                                                               │
│     ├─ name: test name                                                    │
│     ├─ avg_latency_ms: average                                            │
│     ├─ p95_latency_ms: 95th percentile                                    │
│     ├─ p99_latency_ms: 99th percentile                                    │
│     ├─ success_rate: % successful completions                             │
│     └─ passed: true/false                                                 │
│                                                                            │
│  Example: Parse in PowerShell                                             │
│  $results = Get-Content "benchmark_results.json" | ConvertFrom-Json       │
│  $results.tests | Format-Table name, avg_latency_ms, success_rate         │
│                                                                            │
└────────────────────────────────────────────────────────────────────────────┘

┌────────────────────────────────────────────────────────────────────────────┐
│ PERFORMANCE TARGETS                                                         │
├────────────────────────────────────────────────────────────────────────────┤
│                                                                            │
│  TEST                      TARGET              EXCELLENT          FAIL    │
│  ────────────────────────────────────────────────────────────────────────  │
│  Cold Start                < 1000ms            < 500ms              > 2s   │
│  Warm Cache                < 50ms              < 20ms               > 100ms│
│  Rapid Fire (req/sec)      > 2 req/sec         > 10 req/sec        < 1/s  │
│  Multi-Language Success    > 80%               > 95%                < 60%  │
│  Context-Aware Latency     < 500ms             < 250ms              > 1s   │
│  GPU Speedup               2-5x faster         5-10x faster         < 1x   │
│  Memory Peak               < 500MB             < 300MB              >1GB   │
│                                                                            │
└────────────────────────────────────────────────────────────────────────────┘

┌────────────────────────────────────────────────────────────────────────────┐
│ COLOR-CODED LOG LEVELS                                                      │
├────────────────────────────────────────────────────────────────────────────┤
│                                                                            │
│  [14:23:45] ✓    Green (#6a9955)   SUCCESS - Test passed                │
│  [14:23:46] INFO Blue (#569cd6)    INFO - Status message                 │
│  [14:23:47] ⚠    Yellow (#dcdcaa)  WARNING - Minor issue                 │
│  [14:23:48] ✗    Red (#f48771)     ERROR - Test failed                   │
│  [14:23:49] ⚙    Gray (#808080)    DEBUG - Detailed info                 │
│                                                                            │
└────────────────────────────────────────────────────────────────────────────┘

┌────────────────────────────────────────────────────────────────────────────┐
│ TROUBLESHOOTING                                                             │
├────────────────────────────────────────────────────────────────────────────┤
│                                                                            │
│  ✗ "Model not found"                                                     │
│    → Download from https://huggingface.co/TheBloke/...                   │
│    → Place in ./models/ directory                                         │
│                                                                            │
│  ✗ "Timeout exceeded"                                                    │
│    → Use -TimeoutMin 15 (PowerShell) or increase via Python               │
│    → Try a smaller model (Q2, Q3, Q4 quantization)                        │
│                                                                            │
│  ✗ "Insufficient memory"                                                 │
│    → Close other applications                                             │
│    → Use quantized model instead of full precision                        │
│    → Reduce context window size                                           │
│                                                                            │
│  ✗ "GPU not detected"                                                    │
│    → Install Vulkan drivers (nvidia-vulkan or AMD)                        │
│    → Rebuild with ENABLE_VULKAN=ON                                        │
│    → Use --cpu-only flag for CPU fallback                                 │
│                                                                            │
│  ✗ "Benchmark menu not visible"                                          │
│    → Ensure Win32IDE initialized properly                                 │
│    → Check that benchmark_menu_widget.cpp is compiled                     │
│    → Verify benchmark_menu_widget.hpp is included                         │
│    → Rebuild entire project: cmake --build . --target RawrXD-Win32IDE    │
│                                                                            │
└────────────────────────────────────────────────────────────────────────────┘

┌────────────────────────────────────────────────────────────────────────────┐
│ DETAILED DOCUMENTATION                                                      │
├────────────────────────────────────────────────────────────────────────────┤
│                                                                            │
│  BENCHMARK_SUITE.md            - Complete benchmark suite documentation  │
│  BENCHMARK_IDE_INTEGRATION.md  - IDE integration guide with examples     │
│  benchmark_completions_full.cpp - Full benchmark implementation source   │
│                                                                            │
└────────────────────────────────────────────────────────────────────────────┘

EOF

# PowerShell equivalent
cat << 'EOF_PS1' > benchmark-quick-start.ps1
# RawrXD Benchmark Quick Start (PowerShell)

Write-Host ""
Write-Host "╔════════════════════════════════════════════════════════════════════════════╗" -ForegroundColor Cyan
Write-Host "║              RawrXD BENCHMARK SUITE - QUICK START (PowerShell)            ║" -ForegroundColor Cyan
Write-Host "╚════════════════════════════════════════════════════════════════════════════╝" -ForegroundColor Cyan
Write-Host ""

Write-Host "OPTION 1: Run from IDE" -ForegroundColor Green
Write-Host "────────────────────────────────────────────────────────────────────────────"
Write-Host "1. Open RawrXD-Win32IDE.exe"
Write-Host "2. Tools → Benchmarks → Run Benchmarks..."
Write-Host "3. Select tests and click 'Run Benchmarks'"
Write-Host ""

Write-Host "OPTION 2: Run Standalone (PowerShell)" -ForegroundColor Green
Write-Host "────────────────────────────────────────────────────────────────────────────"
Write-Host ".\run_benchmarks.ps1                                    # Run all"
Write-Host ".\run_benchmarks.ps1 -ListOnly                          # List only"
Write-Host ".\run_benchmarks.ps1 -ModelPath model.gguf               # Custom model"
Write-Host ".\run_benchmarks.ps1 -Verbose -Rebuild                  # Full output + rebuild"
Write-Host ""

Write-Host "OPTION 3: Run Standalone Executable" -ForegroundColor Green
Write-Host "────────────────────────────────────────────────────────────────────────────"
Write-Host ".\build\bin\Release\benchmark_completions.exe"
Write-Host ".\build\bin\Release\benchmark_completions.exe -m models/model.gguf"
Write-Host ""

Write-Host "OPTION 4: Run via Python" -ForegroundColor Green
Write-Host "────────────────────────────────────────────────────────────────────────────"
Write-Host "python3 run_benchmarks.py"
Write-Host "python3 run_benchmarks.py -m models/model.gguf"
Write-Host "python3 run_benchmarks.py --list"
Write-Host ""

Write-Host "OUTPUT FILES" -ForegroundColor Cyan
Write-Host "────────────────────────────────────────────────────────────────────────────"
Write-Host "benchmark_results.json          - Detailed per-test results"
Write-Host "benchmark_suite_results.json    - Overall run summary"
Write-Host ""

Write-Host "EXAMPLE: Parse Results" -ForegroundColor Yellow
Write-Host "`$results = Get-Content benchmark_results.json | ConvertFrom-Json"
Write-Host "`$results.tests | Format-Table name, avg_latency_ms, success_rate"
Write-Host ""

Write-Host "For detailed information, see:"
Write-Host "  • BENCHMARK_SUITE.md - Complete guide"
Write-Host "  • BENCHMARK_IDE_INTEGRATION.md - IDE integration details"
Write-Host ""

EOF_PS1
