#pragma once
#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <chrono>
#include <windows.h>

namespace RawrXD::UX {

struct InlineSuggestion {
    std::string text;
    std::string triggerLine;
    size_t cursorPos = 0;
    std::chrono::steady_clock::time_point generatedAt;
    double confidence = 1.0;
    double latencyMs = 0.0;
};

class InlineCompletionEngine {
public:
    using SuggestionCallback = std::function<void(const InlineSuggestion&)>;
    using AcceptanceCallback = std::function<void(const std::string& acceptedText)>;
    
    static InlineCompletionEngine& instance();
    
    bool initialize(HWND hwndEditor);
    void shutdown();
    
    void requestSuggestion(const std::string& currentLine, size_t cursorPos);
    void onModelResponse(const std::string& completion, double latencyMs);
    
    void renderGhostText(HDC hdc, const RECT& clientRect);
    void acceptCurrentSuggestion();
    void dismissSuggestion();
    bool hasActiveSuggestion() const { return !m_current.text.empty(); }
    
    void setEnabled(bool enabled) { m_enabled = enabled; }
    bool isEnabled() const { return m_enabled; }
    void setDelayMs(int ms) { m_delayMs = ms; }
    
    struct Metrics {
        uint64_t totalRequests = 0;
        uint64_t accepted = 0;
        uint64_t dismissed = 0;
        double avgLatencyMs = 0.0;
    };
    Metrics getMetrics() const { return m_metrics; }
    
private:
    InlineCompletionEngine() = default;
    
    HWND m_hwndEditor = nullptr;
    WNDPROC m_originalWndProc = nullptr;
    
    InlineSuggestion m_current;
    bool m_enabled = true;
    int m_delayMs = 150;
    bool m_pendingRequest = false;
    
    Metrics m_metrics;
    
    static LRESULT CALLBACK editorSubclassProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    void updateGhostText();
};

} // namespace RawrXD::UX
