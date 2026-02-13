// ============================================================================
// cot_response_schema.hpp — Stable JSON Schema for CoT Responses (Versioned)
// ============================================================================
//
// Action Item #2: Define schema version "1" for CoT response objects.
// Required keys: final, steps[], meta{ latencyMs, route, preset, depth }
//
// Action Item #13: Pipeline exit invariants — final must be non-empty,
// max length enforced, visibility policy enforced.
//
// Design:
//   - No exceptions — PatchResult-style returns
//   - Validates/repairs internal outputs before emitting
//   - Always emits schema-compliant JSON
//
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED
// ============================================================================

#pragma once

#ifndef RAWRXD_COT_RESPONSE_SCHEMA_H
#define RAWRXD_COT_RESPONSE_SCHEMA_H

#include <string>
#include <vector>
#include <cstdint>
#include <algorithm>
#include <sstream>
#include <chrono>

// ============================================================================
// Schema Version
// ============================================================================
static constexpr const char* COT_SCHEMA_VERSION = "1";

// ============================================================================
// Pipeline caps (Action Item #6 — clamp max input length)
// ============================================================================
static constexpr size_t COT_MAX_INPUT_WORKING_LENGTH   = 200000;   // 200KB working window
static constexpr size_t COT_MAX_STEP_CONTENT_LENGTH    = 50000;    // 50KB per step
static constexpr size_t COT_MAX_FINAL_ANSWER_LENGTH    = 100000;   // 100KB final
static constexpr size_t COT_MAX_STEPS                  = 200;      // Max steps in DOM
static constexpr size_t COT_MAX_RESPONSE_TOTAL_LENGTH  = 500000;   // 500KB total JSON

// ============================================================================
// Structures — CoT Schema v1
// ============================================================================

struct CotStepSchema {
    std::string role;
    std::string content;
    std::string model;
    int         latency_ms      = 0;
    float       confidence      = 0.0f;
    bool        skipped         = false;
    std::string type;           // "analysis", "audit", "synthesis", "notice", etc.
};

struct CotMetaSchema {
    int         latencyMs       = 0;
    std::string route;          // "trivial", "full", "bypass", "fallback"
    std::string preset;         // "fast", "normal", "deep", etc.
    int         depth           = 0;
    std::string reqId;          // Action Item #8 — request ID for tracing
    bool        trivial         = false;
    bool        truncated       = false;
    std::string trivialVersion; // Action Item #5 — classifier version ID
};

struct CotResponseSchema {
    std::string                 schemaVersion;
    std::string                 final_answer;   // MUST be non-empty
    std::vector<CotStepSchema>  steps;
    CotMetaSchema               meta;
};

// ============================================================================
// Validation + Repair
// ============================================================================

struct CotSchemaValidation {
    bool        valid       = false;
    int         repairCount = 0;
    std::string errors;     // Semicolon-delimited error list
};

// Validate and repair a CotResponseSchema in-place.
// Returns validation result. Always repairs to a valid state.
inline CotSchemaValidation validateAndRepairCotResponse(
    CotResponseSchema& resp,
    const std::string& originalInput = "")
{
    CotSchemaValidation result;
    result.valid = true;

    // --- Schema version ---
    if (resp.schemaVersion.empty() || resp.schemaVersion != COT_SCHEMA_VERSION) {
        resp.schemaVersion = COT_SCHEMA_VERSION;
        result.repairCount++;
    }

    // --- Action Item #13: final must be non-empty ---
    if (resp.final_answer.empty() || resp.final_answer.find_first_not_of(" \t\n\r") == std::string::npos) {
        result.valid = false;
        result.errors += "final_answer was empty;";
        // Repair: use last step content, or echo input
        bool repaired = false;
        for (int i = static_cast<int>(resp.steps.size()) - 1; i >= 0; --i) {
            if (!resp.steps[i].content.empty() &&
                resp.steps[i].content.find("[Error") != 0) {
                resp.final_answer = resp.steps[i].content;
                repaired = true;
                result.repairCount++;
                break;
            }
        }
        if (!repaired && !originalInput.empty()) {
            resp.final_answer = "I received your message: '" +
                originalInput.substr(0, 200) + "'. How can I help?";
            result.repairCount++;
        } else if (!repaired) {
            resp.final_answer = "I'm here to help. Could you please rephrase your question?";
            result.repairCount++;
        }
    }

    // --- Action Item #13: max length enforced ---
    if (resp.final_answer.size() > COT_MAX_FINAL_ANSWER_LENGTH) {
        resp.final_answer.resize(COT_MAX_FINAL_ANSWER_LENGTH);
        resp.meta.truncated = true;
        result.repairCount++;
    }

    // --- Steps cap ---
    if (resp.steps.size() > COT_MAX_STEPS) {
        resp.steps.resize(COT_MAX_STEPS);
        result.repairCount++;
    }

    // --- Per-step content cap ---
    for (auto& step : resp.steps) {
        if (step.content.size() > COT_MAX_STEP_CONTENT_LENGTH) {
            step.content.resize(COT_MAX_STEP_CONTENT_LENGTH);
            result.repairCount++;
        }
        if (step.role.empty()) {
            step.role = "unknown";
            result.repairCount++;
        }
    }

    // --- Meta defaults ---
    if (resp.meta.route.empty()) {
        resp.meta.route = "unknown";
        result.repairCount++;
    }
    if (resp.meta.preset.empty()) {
        resp.meta.preset = "normal";
        result.repairCount++;
    }

    return result;
}

// ============================================================================
// Serialization — to JSON string (manual, no external dependencies)
// ============================================================================

inline std::string escapeJsonString(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 16);
    for (char c : s) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) {
                    // Skip control characters
                } else {
                    out += c;
                }
                break;
        }
    }
    return out;
}

inline std::string serializeCotResponse(const CotResponseSchema& resp) {
    std::ostringstream ss;
    ss << "{";
    ss << "\"schemaVersion\":\"" << escapeJsonString(resp.schemaVersion) << "\",";
    ss << "\"final\":\"" << escapeJsonString(resp.final_answer) << "\",";

    // Steps array
    ss << "\"steps\":[";
    for (size_t i = 0; i < resp.steps.size(); ++i) {
        if (i > 0) ss << ",";
        const auto& step = resp.steps[i];
        ss << "{";
        ss << "\"role\":\"" << escapeJsonString(step.role) << "\",";
        ss << "\"content\":\"" << escapeJsonString(step.content) << "\",";
        ss << "\"model\":\"" << escapeJsonString(step.model) << "\",";
        ss << "\"latency_ms\":" << step.latency_ms << ",";
        ss << "\"confidence\":" << step.confidence << ",";
        ss << "\"skipped\":" << (step.skipped ? "true" : "false");
        if (!step.type.empty()) {
            ss << ",\"type\":\"" << escapeJsonString(step.type) << "\"";
        }
        ss << "}";
    }
    ss << "],";

    // Meta object
    ss << "\"meta\":{";
    ss << "\"latencyMs\":" << resp.meta.latencyMs << ",";
    ss << "\"route\":\"" << escapeJsonString(resp.meta.route) << "\",";
    ss << "\"preset\":\"" << escapeJsonString(resp.meta.preset) << "\",";
    ss << "\"depth\":" << resp.meta.depth << ",";
    ss << "\"trivial\":" << (resp.meta.trivial ? "true" : "false") << ",";
    ss << "\"truncated\":" << (resp.meta.truncated ? "true" : "false");
    if (!resp.meta.reqId.empty()) {
        ss << ",\"reqId\":\"" << escapeJsonString(resp.meta.reqId) << "\"";
    }
    if (!resp.meta.trivialVersion.empty()) {
        ss << ",\"trivialVersion\":\"" << escapeJsonString(resp.meta.trivialVersion) << "\"";
    }
    ss << "}";

    ss << "}";
    return ss.str();
}

// ============================================================================
// Build a graceful-degradation response (Action Item #4)
// Used when Python backend is unreachable.
// ============================================================================
inline CotResponseSchema buildFallbackResponse(
    const std::string& userInput,
    const std::string& reqId = "",
    int latencyMs = 0)
{
    CotResponseSchema resp;
    resp.schemaVersion = COT_SCHEMA_VERSION;
    resp.final_answer = userInput; // Echo the input

    CotStepSchema notice;
    notice.role = "system";
    notice.type = "notice";
    notice.content = "CoT backend unavailable. Returning input as-is. "
                     "The Python reasoning engine may be offline.";
    notice.latency_ms = 0;
    notice.skipped = true;
    resp.steps.push_back(notice);

    resp.meta.latencyMs = latencyMs;
    resp.meta.route = "fallback";
    resp.meta.preset = "none";
    resp.meta.depth = 0;
    resp.meta.reqId = reqId;
    resp.meta.trivial = false;

    return resp;
}

// ============================================================================
// Build a "cancelled" response (Action Item #3)
// Used when request is cancelled via AbortController propagation.
// ============================================================================
inline CotResponseSchema buildCancelledResponse(
    const std::string& reqId = "",
    int elapsedMs = 0)
{
    CotResponseSchema resp;
    resp.schemaVersion = COT_SCHEMA_VERSION;
    resp.final_answer = "Request was cancelled.";

    CotStepSchema notice;
    notice.role = "system";
    notice.type = "cancelled";
    notice.content = "The reasoning pipeline was cancelled by the user.";
    notice.latency_ms = elapsedMs;
    notice.skipped = true;
    resp.steps.push_back(notice);

    resp.meta.latencyMs = elapsedMs;
    resp.meta.route = "cancelled";
    resp.meta.preset = "none";
    resp.meta.depth = 0;
    resp.meta.reqId = reqId;

    return resp;
}

#endif // RAWRXD_COT_RESPONSE_SCHEMA_H
