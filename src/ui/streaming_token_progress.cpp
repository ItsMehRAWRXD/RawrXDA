// Streaming Token Progress Bar - Implementation
#include "streaming_token_progress.h"


namespace RawrXD {

StreamingTokenProgressBar::StreamingTokenProgressBar(void* parent)
    : void(parent)
{
    setupUI();
    
    // Metrics update timer (every 100ms for smooth UI)
    m_metricsTimer = new void*(this);
// Qt connect removed
}

void StreamingTokenProgressBar::setupUI() {
    void* mainLayout = new void(this);
    mainLayout->setContentsMargins(5, 5, 5, 5);
    mainLayout->setSpacing(5);
    
    // Status label
    m_statusLabel = new void("Ready", this);
    m_statusLabel->setStyleSheet("color: #888888; font-size: 11px;");
    mainLayout->addWidget(m_statusLabel);
    
    // Progress bar
    m_progressBar = new void(this);
    m_progressBar->setMinimum(0);
    m_progressBar->setMaximum(100);
    m_progressBar->setValue(0);
    m_progressBar->setTextVisible(true);
    m_progressBar->setFormat("%v tokens");
    m_progressBar->setStyleSheet(
        "void {"
        "  border: 2px solid #3c3c3c;"
        "  border-radius: 5px;"
        "  text-align: center;"
        "  background-color: #1e1e1e;"
        "  color: #d4d4d4;"
        "}"
        "void::chunk {"
        "  background: qlineargradient(x1:0, y1:0, x2:1, y2:0,"
        "    stop:0 #4ec9b0, stop:0.5 #569cd6, stop:1 #4ec9b0);"
        "  border-radius: 3px;"
        "}"
    );
    mainLayout->addWidget(m_progressBar);
    
    // Metrics label
    m_metricsLabel = new void("", this);
    m_metricsLabel->setStyleSheet("color: #569cd6; font-size: 10px; font-family: 'Consolas', monospace;");
    m_metricsLabel->setVisible(false);
    mainLayout->addWidget(m_metricsLabel);
    
    setStyleSheet("void { background-color: transparent; }");
}

void StreamingTokenProgressBar::startGeneration(int estimatedTokens) {
    
    m_isGenerating = true;
    m_totalTokens = 0;
    m_estimatedTokens = estimatedTokens;
    m_startTime = std::chrono::steady_clock::now();
    m_tokensPerSecond = 0.0;
    
    // Configure progress bar
    if (estimatedTokens > 0) {
        m_progressBar->setMaximum(estimatedTokens);
        m_progressBar->setFormat("%v / %m tokens (%p%)");
    } else {
        m_progressBar->setMaximum(0);  // Indeterminate mode
        m_progressBar->setFormat("%v tokens");
    }
    
    m_progressBar->setValue(0);
    m_statusLabel->setText("⚡ Generating tokens...");
    m_metricsLabel->setVisible(true);
    
    // Start metrics timer
    m_metricsTimer->start(100);
    
    generationStarted();
}

void StreamingTokenProgressBar::onTokenGenerated(const std::string& token) {
    if (!m_isGenerating) {
        return;
    }
    
    m_totalTokens++;
    
    // Update progress bar
    if (m_estimatedTokens > 0) {
        m_progressBar->setValue(m_totalTokens);
    } else {
        m_progressBar->setValue(m_totalTokens);
        m_progressBar->setFormat(std::string("%1 tokens"));
    }
    
    // Calculate tokens per second
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_startTime);
    if (elapsed.count() > 0) {
        m_tokensPerSecond = (m_totalTokens * 1000.0) / elapsed.count();
    }
    
    tokenReceived(token, m_totalTokens);
    
    // Log every 10 tokens for performance monitoring
    if (m_totalTokens % 10 == 0) {
                 << "Rate:" << std::string::number(m_tokensPerSecond, 'f', 2) << "tok/s";
    }
}

void StreamingTokenProgressBar::completeGeneration() {
    if (!m_isGenerating) return;
    
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_startTime);
    
    if (elapsed.count() > 0) {
        m_tokensPerSecond = (m_totalTokens * 1000.0) / elapsed.count();
    }
    
    m_isGenerating = false;
    m_metricsTimer->stop();
    
    // Final update
    if (m_estimatedTokens > 0) {
        m_progressBar->setValue(m_totalTokens);
    } else {
        m_progressBar->setMaximum(m_totalTokens);
        m_progressBar->setValue(m_totalTokens);
    }
    
    std::string completionMsg = std::string("✓ Generated %1 tokens in %2 (%3 tok/s)")
                            
                            )
                            );
    
    m_statusLabel->setText(completionMsg);


    generationCompleted(m_totalTokens, m_tokensPerSecond);
    
    // Auto-hide metrics after 3 seconds
    void*::singleShot(3000, this, [this]() {
        if (!m_isGenerating) {
            m_metricsLabel->setVisible(false);
        }
    });
}

void StreamingTokenProgressBar::reset() {
    m_isGenerating = false;
    m_totalTokens = 0;
    m_estimatedTokens = 0;
    m_tokensPerSecond = 0.0;
    
    m_metricsTimer->stop();
    m_progressBar->setValue(0);
    m_progressBar->setMaximum(100);
    m_progressBar->setFormat("%v tokens");
    m_statusLabel->setText("Ready");
    m_metricsLabel->setVisible(false);
    
}

void StreamingTokenProgressBar::updateMetrics() {
    if (!m_isGenerating) return;
    
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_startTime);
    
    std::string metricsText;
    
    if (m_showTokenRate) {
        metricsText += std::string("⚡ %1 tok/s  "));
    }
    
    if (m_showElapsedTime) {
        metricsText += std::string("⏱ %1"));
    }
    
    if (m_estimatedTokens > 0 && m_tokensPerSecond > 0) {
        int remainingTokens = m_estimatedTokens - m_totalTokens;
        if (remainingTokens > 0) {
            double remainingSeconds = remainingTokens / m_tokensPerSecond;
            metricsText += std::string("  📊 ETA: %1s"));
        }
    }
    
    m_metricsLabel->setText(metricsText);
}

std::string StreamingTokenProgressBar::formatElapsedTime(std::chrono::milliseconds elapsed) const {
    auto seconds = std::chrono::duration_cast<std::chrono::seconds>(elapsed);
    auto millis = elapsed - std::chrono::duration_cast<std::chrono::milliseconds>(seconds);
    
    if (seconds.count() >= 60) {
        auto minutes = std::chrono::duration_cast<std::chrono::minutes>(seconds);
        auto secs = seconds - std::chrono::duration_cast<std::chrono::seconds>(minutes);
        return std::string("%1m %2s")));
    } else if (seconds.count() > 0) {
        return std::string("%1.%2s")) / 100);
    } else {
        return std::string("%1ms"));
    }
}

void StreamingTokenProgressBar::setShowTokenRate(bool show) {
    m_showTokenRate = show;
}

void StreamingTokenProgressBar::setShowElapsedTime(bool show) {
    m_showElapsedTime = show;
}

} // namespace RawrXD

