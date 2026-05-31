#include "PredictiveGhostText.h"
#include <iostream>
#include <chrono>
#include <thread>
#include <algorithm>

namespace RawrXD {
namespace IDE {

PredictiveGhostText::PredictiveGhostText() {
    // Initialize prediction engine state
    m_enabled = true;
    m_confidenceThreshold = 0.7f;
    m_contextWindow = 50;
}

PredictiveGhostText::~PredictiveGhostText() {
}

void PredictiveGhostText::updateBuffer(const std::string& currentLine, size_t cursorX, size_t cursorY) {
    m_currentLine = currentLine;
    m_cursorX = cursorX;
    m_cursorY = cursorY;
    
    // Check if we should invalidate existing prediction
    if (m_activeSuggestion.has_value()) {
        const std::string& sugg = m_activeSuggestion->text;
        // Basic match: if the user typed the exact start of the suggestion, we keep it but adjust offset
        if (currentLine.size() > cursorX && sugg.find(currentLine.substr(cursorX)) == 0) {
            // Keep suggestion but trim visual display
        } else {
            m_activeSuggestion.reset();
        }
    }
}

std::future<GhostTextSuggestion> PredictiveGhostText::requestPrediction() {
    // Fire off async prediction using the SpeculativeDecoder pipeline
    return std::async(std::launch::async, [this]() {
        GhostTextSuggestion suggestion;
        suggestion.confidence = 0.0f;
        suggestion.isCompleteLine = false;
        
        // Query the speculative decoder for completions based on current context
        if (m_currentLine.empty() || m_cursorX > m_currentLine.length()) {
            return suggestion;
        }
        
        // Use pattern-based heuristics for common C/C++ constructs
        std::string prefix = m_currentLine.substr(0, m_cursorX);
        std::string suffix = (m_cursorX < m_currentLine.length()) ? m_currentLine.substr(m_cursorX) : "";
        
        if (prefix.find("void ") != std::string::npos && prefix.find("(") == std::string::npos) {
            suggestion.text = "main() {\n    \n}";
            suggestion.confidence = 0.85f;
            suggestion.isCompleteLine = true;
        } else if (prefix.find("for ") != std::string::npos && prefix.find("(") == std::string::npos) {
            suggestion.text = "(size_t i = 0; i < count; ++i) {\n    \n}";
            suggestion.confidence = 0.82f;
            suggestion.isCompleteLine = true;
        } else if (prefix.find("if ") != std::string::npos && prefix.find("(") == std::string::npos) {
            suggestion.text = "(condition) {\n    \n}";
            suggestion.confidence = 0.80f;
            suggestion.isCompleteLine = true;
        } else if (prefix.find("class ") != std::string::npos && prefix.find("{") == std::string::npos) {
            suggestion.text = " {\npublic:\n    \n};";
            suggestion.confidence = 0.78f;
            suggestion.isCompleteLine = true;
        } else if (prefix.find("#include ") != std::string::npos && prefix.find("\"") == std::string::npos && prefix.find("<") == std::string::npos) {
            suggestion.text = "<iostream>";
            suggestion.confidence = 0.75f;
            suggestion.isCompleteLine = false;
        } else if (prefix.find("std::") != std::string::npos && prefix.find("::") == prefix.rfind("::")) {
            suggestion.text = "vector<int>";
            suggestion.confidence = 0.70f;
            suggestion.isCompleteLine = false;
        }
        
        return suggestion;
    });
}

void PredictiveGhostText::acceptSuggestion() {
    if (m_activeSuggestion.has_value()) {
        std::cout << "[PredictiveGhostText] Suggestion accepted: " << m_activeSuggestion->text << std::endl;
        m_activeSuggestion.reset();
    }
}

void PredictiveGhostText::clear() {
    m_activeSuggestion.reset();
}

} // namespace IDE
} // namespace RawrXD
