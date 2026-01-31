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
     * Two-phase initialization - call after void is ready
     * Creates all Qt widgets and starts refresh timer
     */
    void initialize();

private:
    void refresh();

private:
    InferenceEngine* m_engine;
    void**          m_timer;
    void*          m_memLabel;
    void*          m_tokensLabel;
    void*          m_tempLabel;
    void*          m_modelLabel;
};

