# AgentHotPatcher Test Suite - Quick Start Guide

## Overview

This guide provides step-by-step instructions for building, running, and maintaining the comprehensive test suite for the `AgentHotPatcher` class.

## Prerequisites

- **C++17 compatible compiler** (GCC 7+, Clang 5+, MSVC 2017+)
- **Qt 6.x** (Core and Test modules)
- **CMake 3.16+**
- **RawrXD-ModelLoader project** (already contains AgentHotPatcher implementation)

## Quick Start

### Linux/macOS

```bash
# Make build script executable
chmod +x build_tests.sh

# Run complete test suite
./build_tests.sh
```

### Windows

```cmd
# Run complete test suite
build_tests.bat
```

## Manual Build Steps

### 1. Create Build Directory

```bash
mkdir build_tests
cd build_tests
```

### 2. Configure with CMake

```bash
cmake .. -DCMAKE_BUILD_TYPE=Release
```

### 3. Build Tests

```bash
# Linux/macOS
make -j$(nproc)

# Windows
cmake --build . --config Release
```

### 4. Run Tests

```bash
# Unit Tests
./test_agent_hot_patcher

# Integration Tests
./test_agent_hot_patcher_integration
```

## Test Results Interpretation

### Expected Output - Unit Tests

```
=== AgentHotPatcher Unit Tests ===
PASS   : TestAgentHotPatcher::testInitialization()
PASS   : TestAgentHotPatcher::testHallucinationDetection()
PASS   : TestAgentHotPatcher::testHallucinationCorrection()
PASS   : TestAgentHotPatcher::testNavigationErrorFix()
PASS   : TestAgentHotPatcher::testBehaviorPatches()
PASS   : TestAgentHotPatcher::testStatistics()
PASS   : TestAgentHotPatcher::testThreadSafety()
PASS   : TestAgentHotPatcher::testPerformance()
Totals: 8 passed, 0 failed, 0 skipped
```

### Expected Output - Integration Tests

```
=== AgentHotPatcher Integration Tests ===
[SIGNAL] Hallucination detected: fabricated_path (confidence: 0.9)
[SIGNAL] Hallucination corrected: uuid-1234-5678
Corrected content: ./src/kernels/q8k_kernel.cpp
[SIGNAL] Navigation error fixed: /absolute/path/../../with//double/slashes -> ./relative/path
[SIGNAL] Statistics updated: 3 hallucinations detected
=== Integration Tests Completed Successfully ===
```

## Troubleshooting Common Issues

### 1. Missing Qt Dependencies

**Error**: `Could NOT find Qt6`

**Solution**:
```bash
# Install Qt6 development packages
# Ubuntu/Debian
sudo apt-get install qt6-base-dev qt6-tools-dev

# macOS (Homebrew)
brew install qt6

# Windows (vcpkg)
vcpkg install qt6-base
```

### 2. Build Failures

**Error**: `undefined reference to Qt symbols`

**Solution**: Verify CMake finds Qt correctly:
```bash
# Check Qt installation
qmake6 --version

# Set Qt6_DIR if needed
export Qt6_DIR=/path/to/qt6/lib/cmake/Qt6
```

### 3. Test Failures

**Error**: Tests fail with assertion errors

**Solution**: Enable debug logging:
```bash
# Set debug environment variable
export QT_LOGGING_RULES="agent.hotpatcher.debug=true"
./test_agent_hot_patcher
```

## Maintenance Commands

### Clean Build

```bash
# Remove build directory and rebuild
rm -rf build_tests
mkdir build_tests
cd build_tests
cmake ..
make -j$(nproc)
```

### Run Specific Tests

```bash
# Run only unit tests
./test_agent_hot_patcher

# Run only integration tests  
./test_agent_hot_patcher_integration

# Run with verbose output
./test_agent_hot_patcher -v2
```

### Generate Test Reports

```bash
# Generate XML test reports (for CI/CD)
./test_agent_hot_patcher -xml -o test_report.xml
```

## CI/CD Integration

### GitHub Actions Example

```yaml
name: AgentHotPatcher Tests
on: [push, pull_request]

jobs:
  test:
    runs-on: ubuntu-latest
    steps:
    - uses: actions/checkout@v3
    - name: Install Qt6
      run: sudo apt-get install qt6-base-dev qt6-tools-dev
    - name: Build and Run Tests
      run: |
        chmod +x build_tests.sh
        ./build_tests.sh
    - name: Upload Test Results
      uses: actions/upload-artifact@v3
      with:
        name: test-reports
        path: test_report.xml
```

## Performance Monitoring

### Benchmark Thresholds

- **Detection Time**: Should complete in < 100ms
- **Memory Usage**: Minimal overhead for pattern matching
- **Concurrency**: Thread-safe operations

### Performance Testing

```bash
# Run performance-focused tests
./test_agent_hot_patcher testPerformance

# Monitor memory usage
valgrind --tool=massif ./test_agent_hot_patcher
```

## Maintenance Checklist

### After Code Changes

- [ ] Run complete test suite
- [ ] Verify all tests pass
- [ ] Check performance benchmarks
- [ ] Update documentation if needed

### Regular Maintenance

- [ ] Monthly: Run full test suite
- [ ] Quarterly: Review test coverage
- [ ] Annually: Update for new Qt versions

## Support

For issues or questions:

1. Check this guide first
2. Review test output and logs
3. Enable debug logging: `export QT_LOGGING_RULES="agent.hotpatcher.debug=true"`
4. Contact development team if issues persist

## License

Production Grade - Enterprise Ready