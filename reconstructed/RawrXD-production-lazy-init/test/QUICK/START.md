# TEST INFRASTRUCTURE - 5-Minute Quick Start

## 🚀 60 Seconds to First Test Run

### Step 1: Build Tests (30 seconds)
```bash
cd build
cmake --build . --config Release --target comprehensive-tests
```

### Step 2: Run All 58 Tests (30 seconds)
```bash
ctest -L unit --output-on-failure -V
```

**Expected:**
```
100% tests passed, 58 tests, 0 failures
Total Test time (real) = 234.56 sec
```

---

## 📋 Common Commands

### Run Specific Test Suite
```bash
ctest -R TerminalIntegration       # 13 tests (~2 sec)
ctest -R ModelLoading             # 15 tests (~30 sec)
ctest -R StreamingInference       # 15 tests (~15 sec)
ctest -R AgentCoordination        # 15 tests (~10 sec)
```

### Quick Smoke Test (28 tests)
```bash
make smoke-tests
```

### Parallel Execution (faster)
```bash
ctest -L unit -j 4 --output-on-failure
```

---

## 🎯 First-Time Setup

### 1. Configure CMake
```bash
cd D:\RawrXD-production-lazy-init
mkdir build
cd build
cmake -G "Visual Studio 17 2022" -A x64 ..
```

### 2. Build Everything
```bash
cmake --build . --config Release
```

### 3. Verify Tests Exist
```bash
ctest --verbose --no-tests=error
```

### 4. Run Tests
```bash
ctest -L unit --output-on-failure
```

---

## 🔍 Understanding Results

### Success Output
```
TerminalIntegration.ProcessSpawningBasic (125 ms)
[  PASSED  ]

ModelLoading.GGUFMagicValidation (15 ms)
[  PASSED  ]

StreamingInference.BasicTokenStreaming (250 ms)
[  PASSED  ]

AgentCoordination.SingleAgentTaskRouting (5 ms)
[  PASSED  ]

100% tests passed, 58 tests, 0 failures
```

### Failure Output
```
ModelLoading.GGUFVersionParsing (150 ms)
[  FAILED  ] Expected version >=1, got 0

FAILED TESTS (1/58):
  - ModelLoading.GGUFVersionParsing
```

---

## 🐛 Quick Troubleshooting

| Problem | Solution |
|---|---|
| "No tests found" | `cmake -G "Visual Studio 17 2022" -A x64 ..` then rebuild |
| Timeout errors | `ctest --timeout 600 --output-on-failure` |
| Build fails | `cmake --build . --config Release --clean-first` |
| Hanging test | `taskkill /IM test_*.exe /F` then run with `-j 1` |

---

## 📊 Test Suite Overview

| Suite | Tests | Time | Focus |
|---|---|---|---|
| Terminal | 13 | 2s | Process spawning, I/O, errors |
| Model | 15 | 30s | GGUF parsing, memory mgmt |
| Streaming | 15 | 15s | Token streaming, latency |
| Agent | 15 | 10s | Task routing, load balancing |
| **TOTAL** | **58** | **~4 min** | **Complete end-to-end** |

---

## 🚀 Advanced Usage

### Generate Metrics Report
```bash
test_terminal_integration --gtest_output=json:results.json
python generate_dashboard.py --input results.json --output dashboard.html
```

### Performance Profiling
```bash
ctest -L unit --output-on-failure --verbose > perf_report.txt
grep "duration\|Peak Memory" perf_report.txt
```

### CI/CD Testing
```bash
# All workflows ready to use:
.github/workflows/ci-macos.yml          # macOS builds
.github/workflows/nightly-build.yml     # Extended testing
.github/workflows/performance-regression.yml
.github/workflows/deploy-staging.yml
```

---

## 📞 Next Steps

1. ✅ Run first tests: `ctest -L unit`
2. ✅ Check all suites: `ctest -L unit -V`
3. 🔜 Review metrics: `test_*.exe --gtest_output=json:out.json`
4. 🔜 Set up CI/CD: Push to GitHub to trigger workflows
5. 🔜 Configure secrets: Add REGISTRY/SLACK info to GitHub

---

**Status:** ✅ **READY TO USE**

For detailed docs, see: `TEST_INTEGRATION_GUIDE.md`
