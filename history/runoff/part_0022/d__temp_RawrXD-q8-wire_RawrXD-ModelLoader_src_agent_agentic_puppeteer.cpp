// agentic_puppeteer.cpp - Implementation of response correction
#include "agentic_puppeteer.hpp"
#include <iostream>
#include <sstream>

namespace RawrXD::Agent {

AgenticPuppeteer::AgenticPuppeteer() {
    AddRefusalPattern("i (cannot|can't|will not|won't) (help|do|provide|answer)");
    AddRefusalPattern("(sorry|apologize).*(?!able to)");
    AddHallucinationPattern("(invented|made up|false|hallucination)");
    AddLoopPattern("^(.{1,50})\\1{2,}");
}

AgenticPuppeteer::~AgenticPuppeteer() = default;

CorrectionResult AgenticPuppeteer::CorrectResponse(const std::string& original_response, const std::string& user_prompt) {
    if (!enabled_) return CorrectionResult{true, original_response, CorrectionFailureType::None, ""};
    std::lock_guard<std::mutex> lock(stats_mutex_);
    statistics_.responses_analyzed++;
    CorrectionFailureType type = DetectFailure(original_response);
    if (type == CorrectionFailureType::RefusalResponse) return CorrectRefusal(original_response);
    if (type == CorrectionFailureType::Hallucination) return CorrectHallucination(original_response);
    if (type == CorrectionFailureType::InfiniteLoop) return CorrectInfiniteLoop(original_response);
    if (type == CorrectionFailureType::TokenLimitExceeded) return CorrectTokenLimit(original_response);
    return CorrectionResult{true, original_response, CorrectionFailureType::None, "No failures detected"};
}

CorrectionResult AgenticPuppeteer::CorrectJsonResponse(const nlohmann::json& response, const std::string& context) {
    return CorrectResponse(response.dump(), context);
}

CorrectionFailureType AgenticPuppeteer::DetectFailure(const std::string& response) {
    for (auto& p : refusal_patterns_) if (std::regex_search(response, p)) return CorrectionFailureType::RefusalResponse;
    for (auto& p : hallucination_patterns_) if (std::regex_search(response, p)) return CorrectionFailureType::Hallucination;
    for (auto& p : loop_patterns_) if (std::regex_search(response, p)) return CorrectionFailureType::InfiniteLoop;
    if (response.size() > 100) {
        std::string last = response.substr(response.size() - 50);
        if (last.find('{') != std::string::npos && last.find('}') == std::string::npos) return CorrectionFailureType::TokenLimitExceeded;
    }
    return CorrectionFailureType::None;
}

std::string AgenticPuppeteer::DiagnoseFailure(const std::string& response) {
    auto t = DetectFailure(response);
    if (t == CorrectionFailureType::RefusalResponse) return "Model refused to respond";
    if (t == CorrectionFailureType::Hallucination) return "Detected hallucination";
    if (t == CorrectionFailureType::InfiniteLoop) return "Detected infinite loop";
    if (t == CorrectionFailureType::TokenLimitExceeded) return "Detected token limit";
    return "No failure";
}

void AgenticPuppeteer::AddRefusalPattern(const std::string& pattern) { refusal_patterns_.emplace_back(pattern, std::regex::icase); }
void AgenticPuppeteer::AddHallucinationPattern(const std::string& pattern) { hallucination_patterns_.emplace_back(pattern, std::regex::icase); }
void AgenticPuppeteer::AddLoopPattern(const std::string& pattern) { loop_patterns_.emplace_back(pattern, std::regex::icase); }

std::vector<std::string> AgenticPuppeteer::GetRefusalPatterns() const { return {}; }
std::vector<std::string> AgenticPuppeteer::GetHallucinationPatterns() const { return {}; }
AgenticPuppeteer::Statistics AgenticPuppeteer::GetStatistics() const { std::lock_guard<std::mutex> lock(stats_mutex_); return statistics_; }
void AgenticPuppeteer::ResetStatistics() { std::lock_guard<std::mutex> lock(stats_mutex_); statistics_ = Statistics(); }

CorrectionResult AgenticPuppeteer::CorrectRefusal(const std::string& response) { return CorrectionResult::Ok(response, CorrectionFailureType::RefusalResponse); }
CorrectionResult AgenticPuppeteer::CorrectHallucination(const std::string& response) { return CorrectionResult::Ok(response + "\n[Verification needed]", CorrectionFailureType::Hallucination); }
CorrectionResult AgenticPuppeteer::CorrectFormatViolation(const std::string& response) { return CorrectionResult::Ok(response, CorrectionFailureType::FormatViolation); }
CorrectionResult AgenticPuppeteer::CorrectInfiniteLoop(const std::string& response) { return CorrectionResult::Ok(response, CorrectionFailureType::InfiniteLoop); }
CorrectionResult AgenticPuppeteer::CorrectTokenLimit(const std::string& response) { return CorrectionResult::Ok(response + "}", CorrectionFailureType::TokenLimitExceeded); }

} // namespace RawrXD::Agent
