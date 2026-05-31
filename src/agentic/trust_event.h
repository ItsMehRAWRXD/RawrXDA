#pragma once

#include <chrono>
#include <iostream>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace rawrxd {

enum class TrustClassification {
    Compliant,
    ParrotPrefixEcho,
    DegenerateOutput,
    PromptLeakage,
    StructureInvalid,
    TimeoutExceeded,
    QuantizationArtifact,
    SafetyRefusal,
    DeterministicFallback,
};

inline const char* ToString(TrustClassification value) {
    switch (value) {
        case TrustClassification::Compliant: return "COMPLIANT";
        case TrustClassification::ParrotPrefixEcho: return "PARROT_PREFIX_ECHO";
        case TrustClassification::DegenerateOutput: return "DEGENERATE_OUTPUT";
        case TrustClassification::PromptLeakage: return "PROMPT_LEAKAGE";
        case TrustClassification::StructureInvalid: return "STRUCTURE_INVALID";
        case TrustClassification::TimeoutExceeded: return "TIMEOUT_EXCEEDED";
        case TrustClassification::QuantizationArtifact: return "QUANTIZATION_ARTIFACT";
        case TrustClassification::SafetyRefusal: return "SAFETY_REFUSAL";
        case TrustClassification::DeterministicFallback: return "DETERMINISTIC_FALLBACK";
    }
    return "UNKNOWN";
}

struct TrustEvent {
    TrustClassification classification = TrustClassification::StructureInvalid;
    std::string requestId;
    std::chrono::steady_clock::time_point timestamp = std::chrono::steady_clock::now();
    double inferenceLatencyMs = 0.0;
    int retryLevelAttempted = -1;
    size_t inputTokens = 0;
    size_t outputTokens = 0;
    std::string rawPreview;
    std::string resolutionAction;
};

class TrustTelemetry {
  public:
    void Record(TrustEvent event) {
        events_.push_back(std::move(event));
    }

    std::map<std::string, int> GetClassificationHistogram() const {
        std::map<std::string, int> histogram;
        for (const auto& event : events_) {
            histogram[ToString(event.classification)]++;
        }
        return histogram;
    }

    double GetCompliantRate() const {
        if (events_.empty()) {
            return 0.0;
        }

        size_t compliant = 0;
        for (const auto& event : events_) {
            if (event.classification == TrustClassification::Compliant) {
                ++compliant;
            }
        }
        return static_cast<double>(compliant) / static_cast<double>(events_.size());
    }

    void EmitReport(std::ostream& out = std::cout) const {
        out << "\n=== TRUST TELEMETRY ===\n";
        for (const auto& [classification, count] : GetClassificationHistogram()) {
            out << classification << ": " << count << "\n";
        }
        out << "Compliant rate: " << (GetCompliantRate() * 100.0) << "%\n";
    }

    const std::vector<TrustEvent>& events() const {
        return events_;
    }

  private:
    std::vector<TrustEvent> events_;
};

}  // namespace rawrxd
