/**
 * @file RawrXD_AgenticPuppeteer.cpp
 * @brief Pure C++20 Response correction implementation
 */

#include "RawrXD_AgenticPuppeteer.hpp"
#include <iostream>
#include <sstream>

namespace RawrXD::Agent {

AgenticPuppeteer::AgenticPuppeteer() {
    // Initialize default patterns
    AddRefusalPattern("i (cannot|can't|will not|won't) (help|do|provide|answer)");
    AddRefusalPattern("(sorry|apologize).*(?!able to)");

    AddHallucinationPattern("(invented|made up|false|hallucination)");

    AddLoopPattern("^(.{1,50})\\1{2,}");
}

AgenticPuppeteer::~AgenticPuppeteer() = default;

CorrectionResult AgenticPuppeteer::CorrectResponse(const std::string& original_response, const std::string& user_prompt) {
    if (!enabled_) {
        return CorrectionResult{true, original_response, CorrectionFailureType::None, ""};
    }

    std::lock_guard<std::mutex> lock(stats_mutex_);
    statistics_.responses_analyzed++;

    // Detect failure type
    CorrectionFailureType failure_type = DetectFailure(original_response);

    if (failure_type == CorrectionFailureType::RefusalResponse) {
        statistics_.failures_detected++;
        return CorrectRefusal(original_response);
    }

    if (failure_type == CorrectionFailureType::Hallucination) {
        statistics_.failures_detected++;
        return CorrectHallucination(original_response);
    }

    if (failure_type == CorrectionFailureType::InfiniteLoop) {
        statistics_.failures_detected++;
        return CorrectInfiniteLoop(original_response);
    }

    if (failure_type == CorrectionFailureType::TokenLimitExceeded) {
        statistics_.failures_detected++;
        return CorrectTokenLimit(original_response);
    }

    // No correction needed
    return CorrectionResult{true, original_response, CorrectionFailureType::None, "No failures detected"};
}

CorrectionResult AgenticPuppeteer::CorrectJsonResponse(const nlohmann::json& response, const std::string& context) {
    std::string json_str = response.dump();
    return CorrectResponse(json_str, context);
}

CorrectionFailureType AgenticPuppeteer::DetectFailure(const std::string& response) {
    // Check refusal patterns
    for (const auto& pattern : refusal_patterns_) {
        if (std::regex_search(response, pattern)) {
            return CorrectionFailureType::RefusalResponse;
        }
    }

    // Check hallucination patterns
    for (const auto& pattern : hallucination_patterns_) {
        if (std::regex_search(response, pattern)) {
            return CorrectionFailureType::Hallucination;
        }
    }

    // Check loop patterns
    for (const auto& pattern : loop_patterns_) {
        if (std::regex_search(response, pattern)) {
            return CorrectionFailureType::InfiniteLoop;
        }
    }

    // Check for truncation (token limit)
    if (response.size() > 100) {
        std::string last_50 = response.substr(response.size() - 50);
        if (last_50.find('{') != std::string::npos && last_50.find('}') == std::string::npos) {
            return CorrectionFailureType::TokenLimitExceeded;
        }
        if (last_50.find('[') != std::string::npos && last_50.find(']') == std::string::npos) {
            return CorrectionFailureType::TokenLimitExceeded;
        }
    }

    return CorrectionFailureType::None;
}

std::string AgenticPuppeteer::DiagnoseFailure(const std::string& response) {
    switch (DetectFailure(response)) {
        case CorrectionFailureType::RefusalResponse:
            return "Model refused to respond - likely safety trigger";
        case CorrectionFailureType::Hallucination:
            return "Detected false or made-up information in response";
        case CorrectionFailureType::InfiniteLoop:
            return "Response contains repetitive/looping content";
        case CorrectionFailureType::TokenLimitExceeded:
            return "Response appears truncated - hit token limit";
        case CorrectionFailureType::None:
        default:
            return "No failure detected";
    }
}

CorrectionResult AgenticPuppeteer::CorrectRefusal(const std::string& response) {
    // Try to extract any useful content before the refusal
    size_t refusal_pos = response.find("cannot");
    if (refusal_pos == std::string::npos) {
        refusal_pos = response.find("sorry");
    }

    std::string corrected = response;
    if (refusal_pos != std::string::npos && refusal_pos > 0) {
        corrected = response.substr(0, refusal_pos);
    }

    statistics_.successful_corrections++;
    return CorrectionResult::Ok(corrected, CorrectionFailureType::RefusalResponse);
}

CorrectionResult AgenticPuppeteer::CorrectHallucination(const std::string& response) {
    // Mark hallucinated sections as uncertain
    std::string corrected = response;
    
    // Add disclaimer
    if (!corrected.empty()) {
        corrected += "\n[Note: This response may contain inaccurate information and should be verified]";
    }

    statistics_.successful_corrections++;
    return CorrectionResult::Ok(corrected, CorrectionFailureType::Hallucination);
}

CorrectionResult AgenticPuppeteer::CorrectFormatViolation(const std::string& response) {
    // Attempt to fix common format issues
    std::string corrected = response;

    // Count unmatched braces
    int brace_balance = 0;
    for (char c : response) {
        if (c == '{') brace_balance++;
        if (c == '}') brace_balance--;
    }

    // Add missing closing braces
    while (brace_balance > 0) {
        corrected += "}";
        brace_balance--;
    }

    statistics_.successful_corrections++;
    return CorrectionResult::Ok(corrected, CorrectionFailureType::FormatViolation);
}

CorrectionResult AgenticPuppeteer::CorrectInfiniteLoop(const std::string& response) {
    // Remove repetitive sections
    std::string corrected;
    std::string last_line;

    std::istringstream iss(response);
    std::string line;
    int repeat_count = 0;

    while (std::getline(iss, line)) {
        if (line == last_line) {
            repeat_count++;
            if (repeat_count > 2) {  // Skip if repeating more than 2x
                continue;
            }
        } else {
            repeat_count = 0;
        }

        corrected += line + "\n";
        last_line = line;
    }

    statistics_.successful_corrections++;
    return CorrectionResult::Ok(corrected, CorrectionFailureType::InfiniteLoop);
}

CorrectionResult AgenticPuppeteer::CorrectTokenLimit(const std::string& response) {
    // Gracefully truncate or indicate incompleteness
    std::string corrected = response;

    if (!corrected.empty()) {
        // Check for and fix unclosed structures
        if (corrected.find('{') != std::string::npos && 
            corrected.find('}') == std::string::npos) {
            corrected += "}";
        }
        if (corrected.find('[') != std::string::npos && 
            corrected.find(']') == std::string::npos) {
            corrected += "]";
        }
    }

    statistics_.successful_corrections++;
    return CorrectionResult::Ok(corrected, CorrectionFailureType::TokenLimitExceeded);
}

void AgenticPuppeteer::AddRefusalPattern(const std::string& pattern) {
    try {
        refusal_patterns_.emplace_back(pattern, std::regex::icase);
    } catch (const std::regex_error& e) {
        std::cerr << "[AgenticPuppeteer] Invalid refusal pattern: " << e.what() << "\n";
    }
}

void AgenticPuppeteer::AddHallucinationPattern(const std::string& pattern) {
    try {
        hallucination_patterns_.emplace_back(pattern, std::regex::icase);
    } catch (const std::regex_error& e) {
        std::cerr << "[AgenticPuppeteer] Invalid hallucination pattern: " << e.what() << "\n";
    }
}

void AgenticPuppeteer::AddLoopPattern(const std::string& pattern) {
    try {
        loop_patterns_.emplace_back(pattern, std::regex::icase);
    } catch (const std::regex_error& e) {
        std::cerr << "[AgenticPuppeteer] Invalid loop pattern: " << e.what() << "\n";
    }
}

std::vector<std::string> AgenticPuppeteer::GetRefusalPatterns() const {
    std::vector<std::string> result;
    // In a real implementation, we'd store the original pattern strings
    return result;
}

std::vector<std::string> AgenticPuppeteer::GetHallucinationPatterns() const {
    std::vector<std::string> result;
    // In a real implementation, we'd store the original pattern strings
    return result;
}

AgenticPuppeteer::Statistics AgenticPuppeteer::GetStatistics() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    return statistics_;
}

void AgenticPuppeteer::ResetStatistics() {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    statistics_ = Statistics();
}

} // namespace RawrXD::Agent
