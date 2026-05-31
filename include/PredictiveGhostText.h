#pragma once
#include <string>
#include <vector>
#include <future>
#include <memory>

namespace RawrXD {
namespace IDE {

struct GhostTextSuggestion {
    std::string text;
    float confidence;
    bool isCompleteLine;
    size_t charOffset;
};

class PredictiveGhostText {
public:
    PredictiveGhostText();
    ~PredictiveGhostText();

    // Triggered on every keystroke
    void updateBuffer(const std::string& currentLine, size_t cursorX, size_t cursorY);
    
    // Request a prediction (async)
    std::future<GhostTextSuggestion> requestPrediction();

    // Accept the current suggestion
    void acceptSuggestion();
    
    // Clear prediction state
    void clear();

    bool hasActiveSuggestion() const { return m_activeSuggestion.has_value(); }
    GhostTextSuggestion getActiveSuggestion() const { return m_activeSuggestion.value_or(GhostTextSuggestion{"", 0.0f, false, 0}); }

private:
    std::string m_currentLine;
    size_t m_cursorX = 0;
    size_t m_cursorY = 0;
    std::optional<GhostTextSuggestion> m_activeSuggestion;
    
    // Internal state for debouncing
    uint64_t m_lastUpdateTick = 0;
    
    // Prediction engine state
    bool m_enabled = true;
    float m_confidenceThreshold = 0.7f;
    size_t m_contextWindow = 50;
    
    // Speculative decoder bridge
    // This will eventually call the SpeculativeDecoder implemented earlier
};

} // namespace IDE
} // namespace RawrXD
