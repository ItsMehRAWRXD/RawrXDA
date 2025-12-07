#pragma once
#include <QWidget>
#include <QLabel>
#include <QTimer>

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
class ModelMonitor : public QWidget {
    Q_OBJECT
public:
    explicit ModelMonitor(InferenceEngine* engine, QWidget* parent = nullptr);

private slots:
    void refresh();

private:
    InferenceEngine* m_engine;
    QTimer*          m_timer;
    QLabel*          m_memLabel;
    QLabel*          m_tokensLabel;
    QLabel*          m_tempLabel;
    QLabel*          m_modelLabel;
};
