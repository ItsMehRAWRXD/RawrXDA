# Hotpatch Playback System

Media-style controls for smoke testing and analyzing model hotpatch configurations.

## Overview

The Hotpatch Playback System provides a media-player style interface for testing, recording, and analyzing different model hotpatch configurations. This enables "lockpicking" - suspending the model and analyzing quality/speed/memory gains/losses from different hotpatch operations to find optimal configurations for fitting larger models on consumer hardware.

## Features

### Media Controls
- **Play/Pause/Stop** - Control playback of hotpatch sessions
- **Fast Forward (2x/4x/8x)** - Accelerate through configurations
- **Rewind** - Step back through previous states
- **Eject** - Export session data and reports

### Navigation
- **Seek to Frame/State/Time** - Jump to specific points in session
- **Step Forward/Backward** - Navigate one state at a time
- **Jump to Best/Worst** - Jump to optimal or worst configurations

### Recording
- **Session Recording** - Record all hotpatch operations
- **Frame Capture** - Capture quality/speed/memory metrics
- **Operation Tracking** - Track which hotpatches were applied

### Smoke Testing
- **Quick Mode** - Fast validation (5-10 iterations)
- **Standard Mode** - Normal testing (20-50 iterations)
- **Thorough Mode** - Comprehensive testing (100+ iterations)
- **Exhaustive Mode** - Full exploration (500+ iterations)
- **Stress Mode** - Stress testing with aggressive operations
- **Comparison Mode** - Compare multiple configurations

### Analytics
- **Timeline Analysis** - Track metrics over time
- **Variance Calculation** - Measure stability
- **Peak Gains** - Identify best improvements
- **State Comparison** - Compare configurations
- **Report Generation** - Markdown and JSON exports

### Model Map
- **Configuration Pinning** - Save optimal configurations
- **Validation** - Verify configurations work
- **Export/Import** - Share configurations

## Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│                    PlaybackController                            │
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────────────────┐  │
│  │   Session   │  │  Analytics │  │    SmokeTestConfig      │  │
│  │  Recording  │  │   Engine    │  │    & Results            │  │
│  └─────────────┘  └─────────────┘  └─────────────────────────┘  │
└─────────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────────┐
│                OmnidirectionalHotpatch                           │
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────────────────┐  │
│  │ StateGraph  │  │ Navigation  │  │    HotpatchOps          │  │
│  │             │  │   Engine    │  │                         │  │
│  └─────────────┘  └─────────────┘  └─────────────────────────┘  │
└─────────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────────┐
│                    LiveHotpatch                                  │
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────────────────┐  │
│  │ Checkpoint  │  │   Weight    │  │    KV Cache             │  │
│  │  Manager    │  │   Pruning   │  │    Compression          │  │
│  └─────────────┘  └─────────────┘  └─────────────────────────┘  │
└─────────────────────────────────────────────────────────────────┘
```

## Usage

### Basic Playback

```c
#include "hotpatch_playback.h"

// Create controller
PlaybackController* ctrl = playback_create(omni);

// Start recording
playback_record_start(ctrl, "My Session");

// Apply hotpatches
apply_omni_hotpatch(omni, HOTPATCH_PRUNE_WEIGHTS | HOTPATCH_QUANTIZE, NULL, 0);
playback_record_frame(ctrl, HOTPATCH_PRUNE_WEIGHTS | HOTPATCH_QUANTIZE);

// Stop recording
playback_record_stop(ctrl);

// Generate report
playback_generate_report(ctrl, "session_report.md");
```

### Smoke Testing

```c
#include "hotpatch_playback.h"

// Configure smoke test
SmokeTestConfig config = {
    .mode = SMOKE_STANDARD,
    .max_iterations = 50,
    .max_time_seconds = 300,
    .test_operations = HOTPATCH_ALL,
    .exploration_rate = 0.3f,
    .record_session = true,
    .generate_report = true
};

// Run test
SmokeTestResult result = smoke_test_run(ctrl, &config);

// Check results
if (result.passed) {
    printf("Quality: %.4f, Speed: %.4f, Memory: %lu MB\n",
           result.final_quality, result.final_speed,
           result.final_memory / (1024 * 1024));
}
```

### Lockpicking

```c
#include "hotpatch_playback_integration.h"

// Create integration
PlaybackIntegration* integration = playback_integration_create(hw, model);

// Start lockpicking session
LockpickSession* session = lockpick_start(integration, "Lockpick Session");

// Try configurations
LockpickResult result = lockpick_try_config(session, 
    HOTPATCH_PRUNE_WEIGHTS | HOTPATCH_QUANTIZE, 0.3f, 8);

if (result.success && result.quality_gain > 0) {
    // Pin good configuration
    lockpick_pin_config(session, "Optimized-Config");
}

// End session and get discovered configurations
ModelMap* configs = lockpick_end(session, true, "configs.map");
```

### Model Map

```c
#include "hotpatch_playback_integration.h"

// Create model map
ModelMap* map = model_map_create("MyModelMap");

// Add configuration
ModelMapEntry entry = {
    .state_id = 42,
    .quality_score = 0.85f,
    .speed_score = 0.72f,
    .memory_usage = 12ULL * 1024 * 1024 * 1024, // 12GB
    .is_validated = true
};
strncpy(entry.model_name, "Codestral-22B", sizeof(entry.model_name) - 1);
strncpy(entry.config_name, "Optimized-16GB", sizeof(entry.config_name) - 1);

model_map_add_entry(map, &entry);

// Find best configuration for constraints
ModelMapEntry* best = model_map_find_best(map, 
    0.8f,   // min_quality
    0.6f,   // min_speed  
    14ULL * 1024 * 1024 * 1024  // max_memory (14GB)
);

// Save for later
model_map_save(map, "model_map.bin");
model_map_export_json(map, "model_map.json");
```

## Hotpatch Operations

| Operation | Description | Quality Impact | Speed Impact | Memory Impact |
|-----------|-------------|----------------|--------------|---------------|
| `HOTPATCH_PRUNE_WEIGHTS` | Remove low-importance weights | -2% to -5% | +5% to +15% | -20% to -40% |
| `HOTPATCH_QUANTIZE` | Reduce precision (FP16/INT8) | -1% to -3% | +10% to +20% | -25% to -50% |
| `HOTPATCH_COMPRESS_KV` | Compress KV cache | -0.5% to -1% | +2% to +5% | -10% to -15% |
| `HOTPATCH_PRUNE_HEADS` | Remove attention heads | -2% to -4% | +8% to +12% | -10% to -15% |
| `HOTPATCH_FUSE_LAYERS` | Merge consecutive layers | -1% to -2% | +5% to +10% | -5% to -10% |
| `HOTPATCH_OPTIMIZE_ATTENTION` | Flash attention optimization | 0% | +15% to +25% | 0% |
| `HOTPATCH_MERGE_EMBEDDINGS` | Combine embedding layers | -0.5% | +3% to +5% | -2% to -5% |
| `HOTPATCH_PRUNE_EXPERTS` | Remove MoE experts | -3% to -8% | +10% to +20% | -30% to -50% |

## Smoke Test Modes

### Quick Mode
- **Iterations**: 5-10
- **Time Limit**: 30 seconds
- **Use Case**: Fast validation during development

### Standard Mode
- **Iterations**: 20-50
- **Time Limit**: 5 minutes
- **Use Case**: Regular testing before commits

### Thorough Mode
- **Iterations**: 100-200
- **Time Limit**: 30 minutes
- **Use Case**: Pre-release validation

### Exhaustive Mode
- **Iterations**: 500+
- **Time Limit**: 2 hours
- **Use Case**: Final optimization pass

### Stress Mode
- **Iterations**: Unlimited
- **Time Limit**: Until failure or timeout
- **Use Case**: Stability testing

### Comparison Mode
- **Iterations**: N/A
- **Time Limit**: N/A
- **Use Case**: Compare specific configurations

## Reports

### Markdown Report

```markdown
# Playback Session Report

## Session Information
- Session ID: 1234567890
- Name: Optimization Session
- Frames: 150
- Duration: 45.23 seconds
- Hotpatches: 47
- Failures: 2

## Best States
| Metric | State | Value |
|--------|-------|-------|
| Quality | #42 | 0.8523 |
| Speed | #38 | 0.7891 |
| Memory | #45 | 11.2 GB |

## Cumulative Gains
- Quality: +0.1234
- Speed: +0.2345
- Memory: -2.1 GB
```

### JSON Export

```json
{
  "session_id": 1234567890,
  "session_name": "Optimization Session",
  "frame_count": 150,
  "frames": [
    {
      "frame": 0,
      "timestamp": 1234567890000000000,
      "state_id": 1,
      "quality": 0.7234,
      "speed": 0.5678,
      "memory_used": 14000000000,
      "operation": 3
    }
  ],
  "summary": {
    "total_quality_gain": 0.1234,
    "total_speed_gain": 0.2345,
    "total_memory_saved": 2100000000
  }
}
```

## Building

### CMake

```bash
mkdir build && cd build
cmake .. -DHOTPATCH_PLAYBACK_BUILD_TESTS=ON -DHOTPATCH_PLAYBACK_BUILD_DEMO=ON
cmake --build . --config Release
ctest -C Release
```

### MSVC

```bash
cmake -G "Visual Studio 17 2022" -A x64 ..
cmake --build . --config Release
```

### GCC/Clang

```bash
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j$(nproc)
```

## Integration with RawrXD

The Hotpatch Playback System integrates with:

1. **LiveHotpatch** - Checkpoint/rollback, weight pruning
2. **OmnidirectionalHotpatch** - State graph navigation
3. **ProgressiveEngine** - Layer loading, memory tiers
4. **AutoConfigurator** - Hardware detection, auto-configuration

## Performance

- **Memory Overhead**: ~1MB per 1000 recorded frames
- **CPU Overhead**: <1% during playback
- **Storage**: ~500 bytes per frame in JSON format
- **Latency**: <1ms for seek operations

## Thread Safety

The playback controller is **not thread-safe**. Use external synchronization if accessing from multiple threads.

## Error Handling

All functions return error codes or boolean success indicators:

```c
bool result = playback_play(ctrl);
if (!result) {
    printf("Error: %s\n", playback_get_error(ctrl));
}
```

## License

Part of RawrXD IDE - MIT License

## Contributing

1. Fork the repository
2. Create a feature branch
3. Add tests for new functionality
4. Submit a pull request

## Changelog

### v1.0.0
- Initial release
- Media controls (play/pause/stop/FF/RW/eject)
- Smoke testing with 6 modes
- Model map for configuration pinning
- Lockpicking API for configuration discovery
- Analytics and reporting