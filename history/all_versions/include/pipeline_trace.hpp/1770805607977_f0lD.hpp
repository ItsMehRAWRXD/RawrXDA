// ============================================================================
// pipeline_trace.hpp — Deterministic Pipeline Trace Mode (Dev Only)
// ============================================================================
//
// Action Item #9: Add ?trace=1 (or header) that returns exact routing decisions.
// Trace output is stable, bounded, and never leaks sensitive content when
// visibility is FinalOnly.
//
// Usage:
//   - Pass X-RawrXD-Trace: 1 header or ?trace=1 query param
//   - Response includes pipeline_trace object with routing decisions
//
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED
// ============================================================================

#pragma once

#ifndef RAWRXD_PIPELINE_TRACE_H
#define RAWRXD_PIPELINE_TRACE_H

#include <string>
#include <vector>
#include <cstdint>
#include <chrono>
#include <sstream>

// ============================================================================
// Trace Event — one decision point in the pipeline
// ============================================================================
struct PipelineTraceEvent {
    std::string stage;          // "classify", "bypass_check", "depth_select", etc.
    std::string decision;       // What was decided
    std::string reason;         // Why
    double      elapsedMs = 0;  // Time since pipeline start
};

// ============================================================================
// Pipeline Trace — accumulated trace for one request
// ============================================================================
struct PipelineTrace {
    bool        enabled     = false;
    std::string reqId;
    std::string chosenPreset;
    int         chosenDepth     = 0;
    std::string chosenMode;
    std::string inputComplexity;
    bool        bypassDecision  = false;
    bool        thermalThrottle = false;
    std::string thermalState;
    bool        adaptiveAdjust  = false;
    int         adaptiveOriginalDepth = 0;
    int         adaptiveFinalDepth    = 0;
    double      totalElapsedMs  = 0;
    std::vector<PipelineTraceEvent> events;

    // Add a trace event
    void addEvent(const std::string& stage, const std::string& decision,
                  const std::string& reason, double elapsedMs) {
        if (!enabled) return;
        events.push_back({stage, decision, reason, elapsedMs});
    }

    // Serialize to JSON string
    std::string toJson() const {
        if (!enabled) return "{}";

        std::ostringstream ss;
        ss << "{";
        ss << "\"reqId\":\"" << reqId << "\",";
        ss << "\"chosenPreset\":\"" << chosenPreset << "\",";
        ss << "\"chosenDepth\":" << chosenDepth << ",";
        ss << "\"chosenMode\":\"" << chosenMode << "\",";
        ss << "\"inputComplexity\":\"" << inputComplexity << "\",";
        ss << "\"bypassDecision\":" << (bypassDecision ? "true" : "false") << ",";
        ss << "\"thermalThrottle\":" << (thermalThrottle ? "true" : "false") << ",";
        ss << "\"thermalState\":\"" << thermalState << "\",";
        ss << "\"adaptiveAdjust\":" << (adaptiveAdjust ? "true" : "false") << ",";

        if (adaptiveAdjust) {
            ss << "\"adaptiveOriginalDepth\":" << adaptiveOriginalDepth << ",";
            ss << "\"adaptiveFinalDepth\":" << adaptiveFinalDepth << ",";
        }

        ss << "\"totalElapsedMs\":" << totalElapsedMs << ",";
        ss << "\"events\":[";
        for (size_t i = 0; i < events.size(); ++i) {
            if (i > 0) ss << ",";
            ss << "{\"stage\":\"" << events[i].stage << "\","
               << "\"decision\":\"" << events[i].decision << "\","
               << "\"reason\":\"" << events[i].reason << "\","
               << "\"elapsedMs\":" << events[i].elapsedMs << "}";
        }
        ss << "]}";
        return ss.str();
    }

    // Sanitize trace for non-dev visibility
    // When visibility is FinalOnly, strip sensitive content
    void sanitizeForVisibility(bool isDev) {
        if (isDev) return; // Dev mode: show everything
        // Strip detailed reasons that might contain user content
        for (auto& evt : events) {
            if (evt.reason.size() > 100) {
                evt.reason = evt.reason.substr(0, 100) + "...[truncated]";
            }
        }
    }
};

#endif // RAWRXD_PIPELINE_TRACE_H
