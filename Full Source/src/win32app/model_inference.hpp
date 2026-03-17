// ============================================================================
// model_inference.hpp — Umbrella header for model inference types & metrics
// ============================================================================
// Provides:
//   - SCOPED_METRIC(name)  — RAII timer that records duration in milliseconds
//   - METRICS              — singleton MetricsCollector with .increment(),
//                            .gauge(), .recordDuration(), .getCounter(),
//                            .getGauge(), .getHistogram(), .exportPrometheus()
//   - CONFIG               — singleton IDEConfig accessor
//   - FEATURE_ENABLED(n)   — feature toggle query
//   - ModelInferenceStatus — lightweight enum for callers that only need
//                            success / failure / not-loaded status
//
// Consumers:
//   Win32IDE.h, Win32IDE.cpp, Win32IDE_Core.cpp, Win32IDE_AgenticBridge.cpp,
//   Win32IDE_AgenticBridge.h, Win32IDE_SubAgent.h, SourceFileRegistry.cpp,
//   MainWindowSimple.h, MainWindowSimple.cpp, FileRegistry_Generated.cpp,
//   CircularBeaconSystem.cpp
// ============================================================================
#pragma once

// Pull in the canonical definitions of MetricsCollector, ScopedTimer,
// IDEConfig, FeatureToggle, and the convenience macros.
#include "../config/IDEConfig.h"

// ============================================================================
// ModelInferenceStatus — thin enum used by bridge / UI callers to represent
// the outcome of an inference request without pulling in the full engine.
// ============================================================================
enum class ModelInferenceStatus {
    Success,          // Generation completed normally
    NotLoaded,        // No model is loaded
    ContextExceeded,  // Prompt exceeded context window
    Timeout,          // Generation timed out
    Cancelled,        // User or system cancelled
    Error             // Unspecified error
};

// ============================================================================
// ModelInferenceResult — lightweight result wrapper returned from high-level
// inference helpers.  Keeps the heavy engine types out of UI headers.
// ============================================================================
struct ModelInferenceResult {
    ModelInferenceStatus status = ModelInferenceStatus::NotLoaded;
    std::string          output;           // Generated text
    double               latencyMs = 0.0;  // Wall-clock time for generation
    int                  tokensGenerated = 0;
    int                  tokensPrompt    = 0;

    bool ok() const { return status == ModelInferenceStatus::Success; }
};
