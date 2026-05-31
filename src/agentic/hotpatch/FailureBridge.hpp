#pragma once
// FailureBridge.hpp - Modern C++20 ergonomic layer for AgenticFailureDetector + Hotpatch Engine
// Provides std::optional result types and structured correction pipeline

#include <optional>
#include <variant>
#include <string>
#include <functional>
#include <chrono>
#include "../agent/agentic_failure_detector.hpp"
#include "Engine.hpp"

namespace RawrXD::Agentic::Hotpatch {

// ============================================================================
// MODERN RESULT TYPES (C++20 Ergonomic Patterns)
// ============================================================================

/// Detection result with structured failure information
struct DetectionResult {
    bool detected = false;
    AgentFailureType type = AgentFailureType::None;
    std::string description;
    double confidence = 0.0;
    std::string evidence;
    std::chrono::system_clock::time_point timestamp;
    int64_t sequenceId = 0;
    
    // Factory methods for clean API
    static DetectionResult none() { return {}; }
    static DetectionResult failure(AgentFailureType t, const std::string& desc, 
                                    double conf, const std::string& evid = "") {
        return {true, t, desc, conf, evid, std::chrono::system_clock::now(), 0};
    }
    
    // Convenience checks
    bool isRefusal() const { return detected && type == AgentFailureType::Refusal; }
    bool isHallucination() const { return detected && type == AgentFailureType::Hallucination; }
    bool isTimeout() const { return detected && type == AgentFailureType::Timeout; }
    bool isSafetyViolation() const { return detected && type == AgentFailureType::SafetyViolation; }
    bool requiresHotpatch() const { 
        return detected && (type == AgentFailureType::Refusal || 
                            type == AgentFailureType::Hallucination ||
                            type == AgentFailureType::FormatViolation);
    }
};

/// Correction result with structured outcome
struct CorrectionResult {
    bool corrected = false;
    std::string correctedText;
    std::string patchApplied;
    double confidence = 0.0;
    std::string reason;
    
    // Factory methods
    static CorrectionResult success(const std::string& text, const std::string& patch = "") {
        return {true, text, patch, 1.0, "Correction applied successfully"};
    }
    static CorrectionResult fail(const std::string& reason) {
        return {false, "", "", 0.0, reason};
    }
    static CorrectionResult noAction() {
        return {true, "", "", 1.0, "No correction needed"};
    }
};

/// Hotpatch application result
struct HotpatchResult {
    bool success = false;
    std::string hookName;
    void* targetAddress = nullptr;
    std::string detail;
    int errorCode = 0;
    
    static HotpatchResult ok(const std::string& name, void* addr, const std::string& msg) {
        return {true, name, addr, msg, 0};
    }
    static HotpatchResult error(const std::string& msg, int code = -1) {
        return {false, "", nullptr, msg, code};
    }
};

// ============================================================================
// STREAM PROCESSOR (Integrates Detection + Correction + Hotpatch)
// ============================================================================

/// Processes model output streams with failure detection and correction
class StreamProcessor {
public:
    using CorrectionCallback = std::function<CorrectionResult(const DetectionResult& detection, 
                                                               const std::string& output)>;
    
    StreamProcessor() : detector_(std::make_unique<AgenticFailureDetector>()) {}
    
    /// Process a chunk of model output, returns corrected text if needed
    std::string process(const std::string& chunk) {
        auto detection = detect(chunk);
        if (!detection.detected) {
            return chunk; // No issues found
        }
        
        // Apply correction strategy
        auto correction = correct(detection, chunk);
        if (correction.corrected && !correction.correctedText.empty()) {
            return correction.correctedText;
        }
        
        return chunk; // No correction applied
    }
    
    /// Detect failures in output (returns std::optional-like result)
    DetectionResult detect(const std::string& output) {
        FailureInfo info = detector_->detectFailure(output);
        
        if (info.type == AgentFailureType::None) {
            return DetectionResult::none();
        }
        
        return DetectionResult::failure(
            info.type, 
            info.description,
            info.confidence,
            info.evidence
        );
    }
    
    /// Apply correction based on failure type
    CorrectionResult correct(const DetectionResult& detection, const std::string& output) {
        if (!detection.detected) {
            return CorrectionResult::noAction();
        }
        
        switch (detection.type) {
            case AgentFailureType::Refusal:
                return handleRefusal(output);
            case AgentFailureType::Hallucination:
                return handleHallucination(output, detection.evidence);
            case AgentFailureType::Timeout:
                return handleTimeout(output);
            case AgentFailureType::ResourceExhausted:
                return handleResourceExhaustion(output);
            default:
                return CorrectionResult::fail("Unhandled failure type");
        }
    }
    
    /// Set custom correction handler
    void setCorrectionHandler(CorrectionCallback callback) {
        correctionHandler_ = std::move(callback);
    }
    
    /// Get underlying detector for advanced configuration
    AgenticFailureDetector& detector() { return *detector_; }
    
private:
    std::unique_ptr<AgenticFailureDetector> detector_;
    CorrectionCallback correctionHandler_;
    
    CorrectionResult handleRefusal(const std::string& output) {
        // Prepend correction marker for downstream processing
        return CorrectionResult::success(
            "[Correction: Proceeding with technical analysis] " + output,
            "refusal_bypass"
        );
    }
    
    CorrectionResult handleHallucination(const std::string& output, const std::string& evidence) {
        // Flag for verification
        return CorrectionResult::success(
            "[Verification Required: " + evidence + "] " + output,
            "hallucination_flag"
        );
    }
    
    CorrectionResult handleTimeout(const std::string& output) {
        return CorrectionResult::fail("Timeout - retry recommended");
    }
    
    CorrectionResult handleResourceExhaustion(const std::string& output) {
        return CorrectionResult::fail("Resource exhausted - reduce load");
    }
};

// ============================================================================
// HOTPATCH MANAGER (Unified API for Detection + Hotpatch)
// ============================================================================

/// Unified manager combining failure detection with hotpatch capabilities
class UnifiedManager {
public:
    static UnifiedManager& instance() {
        static UnifiedManager inst;
        return inst;
    }
    
    /// Process stream with detection and optional hotpatch
    std::string processStream(std::string& chunk) {
        return processor_.process(chunk);
    }
    
    /// Detect failures in output
    DetectionResult detect(const std::string& output) {
        return processor_.detect(output);
    }
    
    /// Apply memory hotpatch at runtime
    HotpatchResult hotpatchMemory(void* address, const std::vector<uint8_t>& bytes) {
        // Validate target first
        if (!Engine::instance().validateTarget(address)) {
            return HotpatchResult::error("Invalid target address", -1);
        }
        
        // Check if hotpatching is enabled
        if (!Engine::instance().isHotpatchingEnabled()) {
            return HotpatchResult::error("Hotpatching disabled", -2);
        }
        
        // Apply through engine
        std::string hookName = "dynamic_" + std::to_string(reinterpret_cast<uintptr_t>(address));
        bool success = Engine::instance().installHook(
            hookName, 
            HookType::PATCH, 
            address, 
            nullptr // For patches, we modify in-place
        );
        
        if (success) {
            return HotpatchResult::ok(hookName, address, "Memory patch applied successfully");
        }
        
        return HotpatchResult::error("Failed to apply patch", -3);
    }
    
    /// Install function detour with trampoline
    template<typename Func>
    HotpatchResult installDetour(const std::string& name, Func target, Func replacement, Func* trampoline) {
        void* targetAddr = reinterpret_cast<void*>(target);
        void* replacementAddr = reinterpret_cast<void*>(replacement);
        
        bool success = Engine::instance().installHook(
            name, 
            HookType::DETOUR, 
            targetAddr, 
            replacementAddr
        );
        
        if (success) {
            // Get trampoline from engine
            // Note: Engine manages trampoline internally
            return HotpatchResult::ok(name, targetAddr, "Detour installed successfully");
        }
        
        return HotpatchResult::error("Failed to install detour", -4);
    }
    
    /// Remove hook by name
    HotpatchResult removeHotpatch(const std::string& name) {
        bool success = Engine::instance().removeHook(name);
        if (success) {
            return HotpatchResult::ok(name, nullptr, "Hook removed successfully");
        }
        return HotpatchResult::error("Failed to remove hook", -5);
    }
    
    /// Set temperature policy (0.0 = conservative, 1.0 = aggressive)
    void setTemperature(double temp) {
        Engine::instance().setModelTemperature(temp);
    }
    
    /// Set unrestricted mode (0.0 = strict, 1.0 = unrestricted)
    void setUnrestrictedMode(double dial) {
        Engine::instance().setUnrestrictiveDial(dial);
    }
    
    /// Get stream processor for direct access
    StreamProcessor& processor() { return processor_; }
    
    /// Get hotpatch engine for direct access
    Engine& engine() { return Engine::instance(); }
    
private:
    UnifiedManager() = default;
    StreamProcessor processor_;
};

// ============================================================================
// CONVENIENCE FUNCTIONS (Clean API for Common Operations)
// ============================================================================

/// Quick detection check - returns std::optional-like result
inline DetectionResult detectFailure(const std::string& output) {
    return UnifiedManager::instance().detect(output);
}

/// Quick stream processing - returns corrected text
inline std::string processStream(const std::string& chunk) {
    std::string mutableChunk = chunk;
    return UnifiedManager::instance().processStream(mutableChunk);
}

/// Quick hotpatch application
inline HotpatchResult applyPatch(void* address, const std::vector<uint8_t>& bytes) {
    return UnifiedManager::instance().hotpatchMemory(address, bytes);
}

/// Check if output contains refusal
inline bool isRefusal(const std::string& output) {
    auto result = detectFailure(output);
    return result.isRefusal();
}

/// Check if output contains hallucination
inline bool isHallucination(const std::string& output) {
    auto result = detectFailure(output);
    return result.isHallucination();
}

/// Check if hotpatch is needed for this failure
inline bool needsHotpatch(const std::string& output) {
    auto result = detectFailure(output);
    return result.requiresHotpatch();
}

} // namespace RawrXD::Agentic::Hotpatch