# AgentHotPatcher Test Suite

## Overview

This directory contains comprehensive tests for the `AgentHotPatcher` class, which provides real-time hallucination detection and correction for AI model outputs.

## Test Files

### 1. Unit Tests (`test_agent_hot_patcher.cpp`)

- **Purpose**: Basic functionality testing using Qt Test framework
- **Coverage**:
  - Initialization and basic operations
  - Hallucination detection (path, logic, reasoning)
  - Correction application
  - Navigation error fixing
  - Behavior patch application
  - Statistics collection
  - Thread safety
  - Performance benchmarks

### 2. Integration Tests (`test_agent_hot_patcher_integration.cpp`)

- **Purpose**: Real-world scenario testing with actual model outputs
- **Coverage**:
  - Real model output interception
  - Path hallucination correction
  - Logic contradiction resolution
  - Navigation error fixing
  - Behavior patch application
  - Signal monitoring and statistics

## Building and Running Tests

### Prerequisites

- C++17 compatible compiler
- Qt 6.x
- CMake 3.16+

### Build Commands

```bash
# Create build directory
mkdir build
cd build

# Configure with CMake
cmake ..

# Build the project
make -j$(nproc)

# Run unit tests
./test_agent_hot_patcher

# Run integration tests
./test_agent_hot_patcher_integration
```

### Windows Build

```cmd
mkdir build
cd build
cmake ..
cmake --build . --config Release

# Run tests
.\test_agent_hot_patcher.exe
.\test_agent_hot_patcher_integration.exe
```

## Test Scenarios

### Hallucination Detection

1. **Path Hallucinations**: Detects fabricated or invalid paths
   - `/mystical/path`, `/phantom/dir`, paths with double slashes

2. **Logic Contradictions**: Detects impossible conditions
   - "always succeeds but always fails"

3. **Incomplete Reasoning**: Detects insufficient explanations
   - Short answers without proper reasoning chains

### Correction Mechanisms

1. **Path Normalization**: Fixes invalid path formats
2. **Logic Resolution**: Replaces contradictory statements
3. **Reasoning Expansion**: Adds missing logical steps
4. **Behavior Patching**: Applies output filters and validators

### Performance Metrics

- **Detection Time**: Should complete in < 100ms for typical content
- **Memory Usage**: Efficient pattern matching
- **Thread Safety**: Concurrent access without crashes

## Expected Output

### Unit Tests

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

### Integration Tests

```
=== AgentHotPatcher Integration Tests ===
[SIGNAL] Hallucination detected: fabricated_path (confidence: 0.9)
[SIGNAL] Hallucination corrected: uuid-1234-5678
Corrected content: ./src/kernels/q8k_kernel.cpp
[SIGNAL] Navigation error fixed: /absolute/path/../../with//double/slashes -> ./relative/path
[SIGNAL] Statistics updated: 3 hallucinations detected
=== Integration Tests Completed Successfully ===
```

## Troubleshooting

### Common Issues

1. **Missing Qt Dependencies**: Ensure Qt6 Core and Test modules are installed
2. **Build Failures**: Check CMake configuration and compiler compatibility
3. **Test Failures**: Verify test data matches expected patterns

### Debug Mode

Enable debug logging by setting environment variable:

```bash
export QT_LOGGING_RULES="agent.hotpatcher.debug=true"
./test_agent_hot_patcher
```

## License

Production Grade - Enterprise Ready