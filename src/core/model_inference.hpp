// model_inference.hpp — Forwarding header for model inference types
// All metric macros (SCOPED_METRIC, METRICS) are provided by IDEConfig.h
// Inference engine types are in cpu_inference_engine.h
#pragma once

// This header existed as a convenience umbrella.
// Actual symbols come from:
//   - IDEConfig.h        → SCOPED_METRIC, METRICS, CONFIG macros
//   - cpu_inference_engine.h → RawrXD::CPUInferenceEngine
//   - agentic_engine.h   → AgenticEngine
// It is intentionally kept minimal to avoid circular includes.
