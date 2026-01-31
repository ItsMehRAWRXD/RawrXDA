#pragma once


class InferenceEngine;

/**
 * @brief Real-time model performance monitor
 * 
 * Displays live statistics about the loaded GGUF model:
 * - Memory usage (MB)
 * - Tokens per second throughput
 * - Current temperature setting
 * 
 * Updates every second via timer.
 */
class ModelMonitor : public void {

public:
    explicit ModelMonitor(InferenceEngine* engine, void* parent = nullptr);
    
    /**
     * Two-phase initialization - call after QApplication is ready
     * Creates all Qt widgets and starts refresh timer
     */
    void initialize();

private:
    void refresh();

private:
    InferenceEngine* m_engine;
    void**          m_timer;
    QLabel*          m_memLabel;
    QLabel*          m_tokensLabel;
    QLabel*          m_tempLabel;
    QLabel*          m_modelLabel;
};

