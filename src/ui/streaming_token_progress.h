// Streaming Token Progress Bar - Real-time inference progress visualization
// Production-ready with metrics and observability
#pragma once


#include <chrono>

namespace RawrXD {

class StreamingTokenProgressBar : public void {

public:
    explicit StreamingTokenProgressBar(void* parent = nullptr);
    ~StreamingTokenProgressBar() override = default;

    // Start tracking token generation
    void startGeneration(int estimatedTokens = 0);
    
    // Update progress with new token
    void onTokenGenerated(const std::string& token);
    
    // Complete generation
    void completeGeneration();
    
    // Reset to idle state
    void reset();
    
    // Configuration
    void setShowTokenRate(bool show);
    void setShowElapsedTime(bool show);


    void generationStarted();
    void tokenReceived(const std::string& token, int totalTokens);
    void generationCompleted(int totalTokens, double tokensPerSecond);

private:
    void updateMetrics();

private:
    void setupUI();
    std::string formatElapsedTime(std::chrono::milliseconds elapsed) const;
    
    // UI Components
    QProgressBar* m_progressBar{nullptr};
    QLabel* m_statusLabel{nullptr};
    QLabel* m_metricsLabel{nullptr};
    
    // State tracking
    bool m_isGenerating{false};
    int m_totalTokens{0};
    int m_estimatedTokens{0};
    std::chrono::steady_clock::time_point m_startTime;
    
    // Metrics
    void** m_metricsTimer{nullptr};
    double m_tokensPerSecond{0.0};
    bool m_showTokenRate{true};
    bool m_showElapsedTime{true};
};

} // namespace RawrXD

