// FailureBridge.cpp - Implementation of ergonomic C++20 bridge layer
// Integrates AgenticFailureDetector with Hotpatch Engine

#include "FailureBridge.hpp"
#include <algorithm>

namespace RawrXD::Agentic::Hotpatch {

// ============================================================================
// STREAM PROCESSOR IMPLEMENTATION
// ============================================================================

// Note: Most of StreamProcessor is header-only for performance
// This file contains only complex implementations that benefit from separation

// ============================================================================
// UNIFIED MANAGER IMPLEMENTATION  
// ============================================================================

// Note: UnifiedManager is header-only singleton
// All core logic is inline for zero-overhead abstraction

// ============================================================================
// UTILITY FUNCTIONS
// ============================================================================

/// Convert legacy FailureInfo to modern DetectionResult
DetectionResult toDetectionResult(const FailureInfo& info) {
    if (info.type == AgentFailureType::None) {
        return DetectionResult::none();
    }
    
    DetectionResult result;
    result.detected = true;
    result.type = info.type;
    result.description = info.description;
    result.confidence = info.confidence;
    result.evidence = info.evidence;
    result.timestamp = info.detectedAt;
    result.sequenceId = info.sequenceNumber;
    
    return result;
}

/// Convert modern DetectionResult to legacy FailureInfo (for backward compatibility)
FailureInfo toFailureInfo(const DetectionResult& result) {
    FailureInfo info;
    info.type = result.type;
    info.description = result.description;
    info.confidence = result.confidence;
    info.evidence = result.evidence;
    info.detectedAt = result.timestamp;
    info.sequenceNumber = result.sequenceId;
    
    return info;
}

/// Get human-readable name for failure type
const char* failureTypeName(AgentFailureType type) {
    switch (type) {
        case AgentFailureType::None: return "None";
        case AgentFailureType::Refusal: return "Refusal";
        case AgentFailureType::Hallucination: return "Hallucination";
        case AgentFailureType::FormatViolation: return "FormatViolation";
        case AgentFailureType::InfiniteLoop: return "InfiniteLoop";
        case AgentFailureType::TokenLimitExceeded: return "TokenLimitExceeded";
        case AgentFailureType::ResourceExhausted: return "ResourceExhausted";
        case AgentFailureType::Timeout: return "Timeout";
        case AgentFailureType::SafetyViolation: return "SafetyViolation";
        case AgentFailureType::QualityDegradation: return "QualityDegradation";
        case AgentFailureType::EmptyResponse: return "EmptyResponse";
        case AgentFailureType::ToolError: return "ToolError";
        case AgentFailureType::InvalidOutput: return "InvalidOutput";
        case AgentFailureType::LowConfidence: return "LowConfidence";
        case AgentFailureType::UserAbort: return "UserAbort";
        default: return "Unknown";
    }
}

/// Get severity level for failure type (0=info, 1=warning, 2=error, 3=critical)
int failureSeverity(AgentFailureType type) {
    switch (type) {
        case AgentFailureType::None: return 0;
        case AgentFailureType::Refusal: return 1;        // Warning - can be corrected
        case AgentFailureType::Hallucination: return 2;  // Error - needs verification
        case AgentFailureType::FormatViolation: return 1;
        case AgentFailureType::InfiniteLoop: return 3;   // Critical - must stop
        case AgentFailureType::TokenLimitExceeded: return 2;
        case AgentFailureType::ResourceExhausted: return 2;
        case AgentFailureType::Timeout: return 1;
        case AgentFailureType::SafetyViolation: return 3; // Critical - security issue
        case AgentFailureType::QualityDegradation: return 1;
        case AgentFailureType::EmptyResponse: return 1;
        case AgentFailureType::ToolError: return 2;
        case AgentFailureType::InvalidOutput: return 2;
        case AgentFailureType::LowConfidence: return 1;
        case AgentFailureType::UserAbort: return 2;
        default: return 1;
    }
}

/// Check if failure type is recoverable without user intervention
bool isRecoverable(AgentFailureType type) {
    switch (type) {
        case AgentFailureType::Refusal:
        case AgentFailureType::FormatViolation:
        case AgentFailureType::Timeout:
        case AgentFailureType::LowConfidence:
            return true;
        default:
            return false;
    }
}

/// Check if failure type requires immediate hotpatch
bool requiresImmediateHotpatch(AgentFailureType type) {
    switch (type) {
        case AgentFailureType::SafetyViolation:
        case AgentFailureType::InfiniteLoop:
        case AgentFailureType::ResourceExhausted:
            return true;
        default:
            return false;
    }
}

} // namespace RawrXD::Agentic::Hotpatch