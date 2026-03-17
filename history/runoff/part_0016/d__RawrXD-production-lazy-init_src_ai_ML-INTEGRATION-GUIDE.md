# ML Integration Utilities

Non-intrusive observability and fault tolerance utilities specifically designed for AI/ML components.

## Overview

These utilities provide production-ready scaffolding for ML systems without modifying core ML logic. All features are disabled by default and activated via environment variables.

## Available Modules

### 1. Core ML Integration (`MLIntegration.h`)
**Purpose:** Foundational ML observability and lifecycle management.

**Features:**
- `MLMetrics` – ML-specific metrics (inference, training, feature extraction)
- `ModelLifecycleTracker` – Model registration, health tracking, performance monitoring
- `InferenceWrapper<T>` – RAII wrapper for ML inference with observability
- `TrainingPipelineTracker` – Training progress monitoring
- `FeatureStoreHelper` – Feature extraction and cache tracking

**Usage:**
```cpp
#include "ai/MLIntegration.h"
using namespace RawrXD::Integration::ML;

// Track model lifecycle
ModelLifecycleTracker::instance().registerModel("error-detector", "neural_net");

// Wrap inference with observability
InferenceWrapper<QString, bool> wrapper("error-detector", [](const QString& input) {
    return detectError(input);
});

bool result = wrapper(input);
```

### 2. Advanced ML Integration (`MLAdvancedIntegration.h`)
**Purpose:** Advanced ML-specific health checks, configuration, and error analysis.

**Features:**
- `MLHealthChecker` – Comprehensive ML system health checks
- `MLConfigManager` – ML-specific configuration management
- `MLErrorPatternAnalyzer` – Error pattern detection and analysis
- `MLPerformanceProfiler` – Performance profiling for ML operations

**Usage:**
```cpp
#include "ai/MLAdvancedIntegration.h"
using namespace RawrXD::Integration::ML;

// Check ML system health
auto health = MLHealthChecker::instance().checkMLSystemHealth();

// Get ML configuration
auto config = MLConfigManager::instance().getMLConfig();

// Analyze error patterns
MLErrorPatternAnalyzer::instance().analyzeErrorPattern(
    "error-detector", "inference", error, context);
```

## Environment Variables

| Variable | Purpose | Example |
|----------|---------|---------|
| `RAWRXD_ML_FEATURE_*` | ML feature flags | `RAWRXD_ML_FEATURE_MODEL_LIFECYCLE_TRACKING=on` |
| `RAWRXD_ML_*` | ML configuration | `RAWRXD_ML_INFERENCE_LATENCY_THRESHOLD=500` |
| `RAWRXD_LOGGING_ENABLED` | Enable ML logging | `1` or `true` |
| `RAWRXD_ENABLE_METRICS` | Enable ML metrics | `1` or `true` |

## ML-Specific Metrics

### Inference Metrics
- `ml_inference_success` – Successful inference count
- `ml_inference_failure` – Failed inference count
- `ml_inference_duration_ms` – Inference latency histogram
- `ml_prediction_confidence_bucket` – Confidence distribution

### Training Metrics
- `ml_training_completed` – Training completion count
- `ml_training_duration_ms` – Training duration
- `ml_training_samples` – Training sample count

### Feature Metrics
- `ml_feature_extraction` – Feature extraction count
- `ml_feature_duration_ms` – Feature extraction latency
- `ml_feature_quality` – Feature quality scores
- `ml_feature_cache_hit` – Cache hit/miss rates

## Model Lifecycle Tracking

Track model health and performance:

```cpp
// Register model
ModelLifecycleTracker::instance().registerModel("my-model", "decision_tree", "1.2");

// Record inference
ModelLifecycleTracker::instance().recordInference("my-model", 150, true);

// Mark unhealthy
ModelLifecycleTracker::instance().markModelUnhealthy("my-model", "High latency");

// Get status
auto status = ModelLifecycleTracker::instance().getModelStatus();
```

## Training Pipeline Observability

Monitor training progress:

```cpp
// Start training
TrainingPipelineTracker::instance().startTraining("error-classifier", 10000);

// Update progress
TrainingPipelineTracker::instance().updateProgress("error-classifier", 50, "Training epoch 5");

// Complete training
TrainingPipelineTracker::instance().completeTraining("error-classifier", 0.95, 3600000);
```

## Health Checks

Comprehensive ML system health monitoring:

```cpp
auto health = MLHealthChecker::instance().checkMLSystemHealth();
// Returns: { isHealthy, status, details }
// details includes: model health ratio, active trainings, latency metrics
```

## Error Pattern Analysis

Detect and analyze ML error patterns:

```cpp
MLErrorPatternAnalyzer::instance().analyzeErrorPattern(
    "error-detector", "inference", "Model timeout", context);
// Automatically detects patterns like timeout, memory issues, model corruption
```

## Performance Profiling

Profile ML operations for optimization:

```cpp
MLPerformanceProfiler::instance().startProfiling("feature_extraction");
// ... feature extraction code ...
MLPerformanceProfiler::instance().stopProfiling("feature_extraction");
```

## Configuration Management

ML-specific configuration with environment overrides:

```cpp
bool enabled = MLConfigManager::instance().isFeatureEnabled("MODEL_LIFECYCLE_TRACKING");
int timeout = MLConfigManager::instance().getIntConfig("INFERENCE_LATENCY_THRESHOLD", 1000);
double threshold = MLConfigManager::instance().getDoubleConfig("MODEL_HEALTH_THRESHOLD", 0.8);
```

## Integration Points

### ML Error Detector Integration

Wrap existing ML error detector with observability:

```cpp
// Before (direct ML call)
bool result = mlErrorDetector.detect(input);

// After (with integration)
InferenceWrapper<QString, bool> wrapper("error-detector", 
    [&](const QString& input) { return mlErrorDetector.detect(input); });
bool result = wrapper(input);
// Automatic metrics, logging, error handling
```

### Training Pipeline Integration

Monitor existing training pipelines:

```cpp
// Before (untracked training)
trainModel(data);

// After (tracked training)
TrainingPipelineTracker::instance().startTraining("my-model", data.size());
trainModel(data); // Existing logic unchanged
TrainingPipelineTracker::instance().completeTraining("my-model", accuracy, duration);
```

## Best Practices

1. **Register All Models:** Use `ModelLifecycleTracker` for every ML model
2. **Wrap Inference:** Use `InferenceWrapper` for consistent observability
3. **Track Training:** Monitor training pipelines with `TrainingPipelineTracker`
4. **Health Checks:** Implement regular ML system health checks
5. **Error Analysis:** Analyze ML errors for patterns and trends
6. **Performance Profiling:** Profile expensive ML operations

## Testing

Enable ML integration for testing:

```bash
export RAWRXD_LOGGING_ENABLED=1
export RAWRXD_ENABLE_METRICS=1
export RAWRXD_ML_FEATURE_MODEL_LIFECYCLE_TRACKING=on
```

Verify with structured output:
```bash
./RawrXD-AgenticIDE 2>&1 | jq '.'
```

## Production Readiness

✅ **Observability:** ML-specific metrics, logging, health checks
✅ **Fault Tolerance:** Error handling, retry mechanisms, circuit breakers
✅ **Performance Monitoring:** Latency tracking, profiling, optimization hints
✅ **Configuration:** Environment-based feature flags and thresholds
✅ **Zero Overhead:** Disabled by default, no-op when not configured

These utilities provide production-grade observability for ML systems without modifying core ML algorithms or training logic.
