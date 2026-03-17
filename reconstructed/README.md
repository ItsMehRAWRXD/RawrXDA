# AgentHotPatcher

Real-time hallucination detection and correction system for AI model outputs.

## Overview

AgentHotPatcher is a C++ library built with Qt that provides real-time detection and correction of hallucinations in AI model outputs. It can detect various types of hallucinations, including:

- Refusal patterns
- Invalid path references
- Logic contradictions
- Incomplete reasoning

The system also provides navigation error fixes and behavior patching capabilities.

## Features

- Real-time hallucination detection and correction
- Navigation error fixing
- Behavior patching for output modification
- Thread-safe operation
- Comprehensive statistics tracking
- Qt-based signal/slot mechanism for event handling
- Configurable patterns and patches

## Building

### Prerequisites

- C++17 compatible compiler
- Qt 6.x
- CMake 3.16 or higher

### Build Steps

```bash
mkdir build
cd build
cmake ..
make
```

## Usage

```cpp
#include "agent_hot_patcher.hpp"

// Create an instance
AgentHotPatcher patcher;

// Initialize with GGUF loader path
patcher.initialize("/path/to/gguf/loader", 0);

// Intercept and correct model output
QJsonObject context; // Optional context
QJsonObject result = patcher.interceptModelOutput("I cannot perform this action", context);

// Check results
QString correctedOutput = result["correctedOutput"].toString();
```

## Testing

The project includes both a simple test application and Qt-based unit tests:

```bash
# Run the simple test application
./test/TestAgentHotPatcher

# Run Qt-based unit tests
./test_qt/TestAgentHotPatcherQtTest
```

## API

### Main Classes

- `AgentHotPatcher` - Main class for hallucination detection and correction
- `HallucinationDetection` - Structure containing hallucination detection information
- `NavigationFix` - Structure containing navigation error fix information
- `BehaviorPatch` - Structure containing behavior patch information

### Key Methods

- `initialize()` - Initialize the patcher
- `interceptModelOutput()` - Intercept and correct model output
- `detectHallucination()` - Detect hallucinations in content
- `correctHallucination()` - Apply correction to detected hallucination
- `fixNavigationError()` - Fix navigation errors in paths
- `applyBehaviorPatches()` - Apply behavior patches to output
- `getCorrectionStatistics()` - Get correction statistics

## License

Production Grade - Enterprise Ready