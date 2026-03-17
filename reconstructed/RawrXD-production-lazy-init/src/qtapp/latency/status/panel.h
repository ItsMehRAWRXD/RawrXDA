#pragma once

#include <QWidget>
#include <QLabel>
#include <QProgressBar>
#include "latency_monitor.h"

namespace RawrXD {

class LatencyStatusPanel : public QWidget
{
    Q_OBJECT
public:
    explicit LatencyStatusPanel(LatencyMonitor* latencyMonitor, QWidget* parent = nullptr);

private slots:
    void onPingUpdated(int latencyMs);
    void onStatsUpdated(const LatencyStats& stats);

private:
    void updateDisplay();
    void setupUI();
    
    LatencyMonitor* m_latencyMonitor;
    
    // Status indicators
    QLabel* m_statusLabel;         // Shows current status (idle/active/loading)
    QLabel* m_pingLabel;           // Shows current ping in ms
    QLabel* m_minPingLabel;        // Shows minimum ping
    QLabel* m_maxPingLabel;        // Shows maximum ping
    QLabel* m_avgPingLabel;        // Shows average ping
    QLabel* m_samplesLabel;        // Shows total samples
    QLabel* m_ramLabel;            // Shows RAM usage
    QLabel* m_cpuLabel;            // Shows CPU usage
    QLabel* m_backendLabel;        // Shows active backend
    QProgressBar* m_latencyBar;    // Visual latency indicator
    QProgressBar* m_cpuBar;        // Visual CPU indicator
};

} // namespace RawrXD
