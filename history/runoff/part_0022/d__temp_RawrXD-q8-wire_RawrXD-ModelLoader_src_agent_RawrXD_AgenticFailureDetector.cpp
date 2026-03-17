/**
 * @file RawrXD_AgenticFailureDetector.cpp
 * @brief Pure C++20 failure detection implementation
 */

#include "RawrXD_AgenticFailureDetector.hpp"
#include <iostream>
#include <algorithm>
#include <cctype>

namespace RawrXD::Agent {

AgenticFailureDetector::AgenticFailureDetector() {
    // Default patterns
    AddRefusalPattern("i (cannot|can't|will not|won't) (help|do|provide|answer)");
    AddRefusalPattern("(sorry|apologize|regret).*(?!able to)");
    AddRefusalPattern("(not able|unable|cannot) to (help|assist|comply)");

    AddHallucinationPattern("^(according to|based on|from what).*\\$\\d+\\.\\d+");
    AddHallucinationPattern("(invented|made up|fictional|imaginary)");

    AddLoopPattern("(repeat|same|again)\\s*(and again|repeatedly)");
    AddLoopPattern("^(.{1,50})\\1{2,}");  // Repeating substring 3+ times

    AddSafetyPattern("(cannot assist|violates policy|against our values)");
    AddSafetyPattern("(sexual|violence|illegal|hate speech)");
}

AgenticFailureDetector::~AgenticFailureDetector() = default;

FailureInfo AgenticFailureDetector::DetectFailure(const std::string& output, const std::string& context) {
    if (!enabled_) {
        return {FailureType::None, "", 0.0, "", std::chrono::system_clock::now(), ++sequence_counter_};
    }

    std::lock_guard<std::mutex> lock(stats_mutex_);
    statistics_.total_outputs_analyzed++;

    // Check each failure type
    FailureInfo best_failure;
    best_failure.detected_at = std::chrono::system_clock::now();
    best_failure.sequence_number = ++sequence_counter_;

    if (IsRefusal(output)) {
        best_failure.type = FailureType::Refusal;
        best_failure.description = "Model refused to respond";
        best_failure.confidence = CalculateConfidence(FailureType::Refusal, output);
        best_failure.evidence = output.substr(0, 200);
        return best_failure;
    }

    if (IsHallucination(output)) {
        best_failure.type = FailureType::Hallucination;
        best_failure.description = "Detected hallucination/false information";
        best_failure.confidence = CalculateConfidence(FailureType::Hallucination, output);
        best_failure.evidence = output.substr(0, 200);
        return best_failure;
    }

    if (IsInfiniteLoop(output)) {
        best_failure.type = FailureType::InfiniteLoop;
        best_failure.description = "Output contains repetitive content";
        best_failure.confidence = CalculateConfidence(FailureType::InfiniteLoop, output);
        best_failure.evidence = output.substr(0, 200);
        return best_failure;
    }

    if (IsTokenLimitExceeded(output)) {
        best_failure.type = FailureType::TokenLimitExceeded;
        best_failure.description = "Response appears truncated";
        best_failure.confidence = CalculateConfidence(FailureType::TokenLimitExceeded, output);
        best_failure.evidence = output.substr(output.size() > 100 ? output.size() - 100 : 0);
        return best_failure;
    }

    if (IsSafetyViolation(output)) {
        best_failure.type = FailureType::SafetyViolation;
        best_failure.description = "Safety filter triggered";
        best_failure.confidence = CalculateConfidence(FailureType::SafetyViolation, output);
        best_failure.evidence = output.substr(0, 200);
        return best_failure;
    }

    return {FailureType::None, "", 0.0, "", best_failure.detected_at, best_failure.sequence_number};
}

std::vector<FailureInfo> AgenticFailureDetector::DetectMultipleFailures(const std::string& output) {
    std::vector<FailureInfo> failures;

    auto refusal = DetectFailure(output);
    if (refusal.type != FailureType::None) {
        failures.push_back(refusal);
    }

    return failures;
}

bool AgenticFailureDetector::IsRefusal(const std::string& output) const {
    for (const auto& pattern : refusal_patterns_) {
        if (std::regex_search(output, pattern)) {
            return true;
        }
    }
    return false;
}

bool AgenticFailureDetector::IsHallucination(const std::string& output) const {
    for (const auto& pattern : hallucination_patterns_) {
        if (std::regex_search(output, pattern)) {
            return true;
        }
    }
    return false;
}

bool AgenticFailureDetector::IsFormatViolation(const std::string& output) const {
    // Check if JSON-like output is malformed
    int brace_count = 0;
    for (char c : output) {
        if (c == '{') brace_count++;
        if (c == '}') brace_count--;
        if (brace_count < 0) return true;  // Unmatched brace
    }
    return brace_count != 0;  // Unmatched braces at end
}

bool AgenticFailureDetector::IsInfiniteLoop(const std::string& output) const {
    for (const auto& pattern : loop_patterns_) {
        if (std::regex_search(output, pattern)) {
            return true;
        }
    }

    // Check for highly repetitive content (>80% of output is repeating)
    if (output.size() > 500) {
        std::string first_100 = output.substr(0, 100);
        int count = 0;
        for (size_t i = 0; i + 100 <= output.size(); i += 100) {
            if (output.substr(i, 100) == first_100) count++;
        }
        if (count > 3) return true;  // Same 100-char block appears 4+ times
    }

    return false;
}

bool AgenticFailureDetector::IsTokenLimitExceeded(const std::string& output) const {
    // Check if output ends abruptly (no proper closing)
    if (output.size() > 10) {
        std::string last_10 = output.substr(output.size() - 10);
        
        // Check for incomplete JSON/code structures
        if (output.find('{') != std::string::npos && 
            output.find('}') == std::string::npos) {
            return true;
        }
        if (output.find('[') != std::string::npos && 
            output.find(']') == std::string::npos) {
            return true;
        }
    }

    return false;
}

bool AgenticFailureDetector::IsResourceExhausted(const std::string& output) const {
    return output.find("out of memory") != std::string::npos ||
           output.find("resource exhausted") != std::string::npos ||
           output.find("OOM") != std::string::npos;
}

bool AgenticFailureDetector::IsTimeout(const std::string& output) const {
    return output.find("timeout") != std::string::npos ||
           output.find("timed out") != std::string::npos;
}

bool AgenticFailureDetector::IsSafetyViolation(const std::string& output) const {
    for (const auto& pattern : safety_patterns_) {
        if (std::regex_search(output, pattern)) {
            return true;
        }
    }
    return false;
}

double AgenticFailureDetector::CalculateConfidence(FailureType type, const std::string& output) {
    // Simple confidence scoring based on evidence strength
    switch (type) {
        case FailureType::Refusal:
            return 0.9;  // High confidence if patterns match
        case FailureType::Hallucination:
            return 0.7;  // Medium-high
        case FailureType::InfiniteLoop:
            return 0.85; // High
        case FailureType::TokenLimitExceeded:
            return 0.6;  // Medium
        case FailureType::SafetyViolation:
            return 0.95; // Very high
        default:
            return 0.0;
    }
}

void AgenticFailureDetector::AddRefusalPattern(const std::string& pattern) {
    try {
        refusal_patterns_.emplace_back(pattern, std::regex::icase);
    } catch (const std::regex_error& e) {
        std::cerr << "[AgenticFailureDetector] Invalid refusal pattern: " << e.what() << "\n";
    }
}

void AgenticFailureDetector::AddHallucinationPattern(const std::string& pattern) {
    try {
        hallucination_patterns_.emplace_back(pattern, std::regex::icase);
    } catch (const std::regex_error& e) {
        std::cerr << "[AgenticFailureDetector] Invalid hallucination pattern: " << e.what() << "\n";
    }
}

void AgenticFailureDetector::AddLoopPattern(const std::string& pattern) {
    try {
        loop_patterns_.emplace_back(pattern, std::regex::icase);
    } catch (const std::regex_error& e) {
        std::cerr << "[AgenticFailureDetector] Invalid loop pattern: " << e.what() << "\n";
    }
}

void AgenticFailureDetector::AddSafetyPattern(const std::string& pattern) {
    try {
        safety_patterns_.emplace_back(pattern, std::regex::icase);
    } catch (const std::regex_error& e) {
        std::cerr << "[AgenticFailureDetector] Invalid safety pattern: " << e.what() << "\n";
    }
}

AgenticFailureDetector::Statistics AgenticFailureDetector::GetStatistics() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    return statistics_;
}

void AgenticFailureDetector::ResetStatistics() {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    statistics_ = Statistics();
}

} // namespace RawrXD::Agent
