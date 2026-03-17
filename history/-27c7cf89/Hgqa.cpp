#include "latency_status_panel.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QFont>

namespace RawrXD {

LatencyStatusPanel::LatencyStatusPanel(LatencyMonitor* latencyMonitor, QWidget* parent)
    : QWidget(parent)
    , m_latencyMonitor(latencyMonitor)
{
    setupUI();
    
    // Connect to latency monitor signals
    connect(m_latencyMonitor, &LatencyMonitor::pingUpdated, this, &LatencyStatusPanel::onPingUpdated);
    connect(m_latencyMonitor, &LatencyMonitor::statsUpdated, this, &LatencyStatusPanel::onStatsUpdated);
}

void LatencyStatusPanel::setupUI()
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(8, 8, 8, 8);
    mainLayout->setSpacing(6);
    
    // Status header
    QHBoxLayout* statusLayout = new QHBoxLayout();
    QLabel* statusHeaderLabel = new QLabel("Model Status:");
    statusHeaderLabel->setStyleSheet("font-weight: bold;");
    m_statusLabel = new QLabel("idle");
    m_statusLabel->setStyleSheet("color: #888; font-weight: bold;");
    statusLayout->addWidget(statusHeaderLabel);
    statusLayout->addWidget(m_statusLabel);
    statusLayout->addStretch();
    mainLayout->addLayout(statusLayout);
    
    // Latency indicator (bottom of IDE)
    QGroupBox* latencyGroup = new QGroupBox("Latency", this);
    QHBoxLayout* latencyLayout = new QHBoxLayout();
    
    m_pingLabel = new QLabel("Ping: -- ms");
    m_pingLabel->setStyleSheet("font-weight: bold; color: #0f0; background: #1a1a1a; padding: 4px; border-radius: 3px;");
    QFont monospace("Courier", 9);
    m_pingLabel->setFont(monospace);
    
    m_latencyBar = new QProgressBar();
    m_latencyBar->setMaximum(100);
    m_latencyBar->setMaximumHeight(20);
    m_latencyBar->setStyleSheet(
        "QProgressBar { border: 1px solid #3c3c3c; background: #2d2d30; border-radius: 2px; }"
        "QProgressBar::chunk { background: #0f0; }"
    );
    
    latencyLayout->addWidget(m_pingLabel);
    latencyLayout->addWidget(m_latencyBar);
    latencyGroup->setLayout(latencyLayout);
    mainLayout->addWidget(latencyGroup);
    
    // Statistics panel
    QGroupBox* statsGroup = new QGroupBox("Statistics", this);
    QVBoxLayout* statsLayout = new QVBoxLayout();
    
    // Min/Max/Avg row
    QHBoxLayout* statsRow1 = new QHBoxLayout();
    
    m_minPingLabel = new QLabel("Min: -- ms");
    m_minPingLabel->setFont(monospace);
    statsRow1->addWidget(m_minPingLabel);
    
    m_avgPingLabel = new QLabel("Avg: -- ms");
    m_avgPingLabel->setFont(monospace);
    statsRow1->addWidget(m_avgPingLabel);
    
    m_maxPingLabel = new QLabel("Max: -- ms");
    m_maxPingLabel->setFont(monospace);
    statsRow1->addWidget(m_maxPingLabel);
    
    statsRow1->addStretch();
    statsLayout->addLayout(statsRow1);
    
    // Samples row
    m_samplesLabel = new QLabel("Samples: 0");
    m_samplesLabel->setFont(monospace);
    statsLayout->addWidget(m_samplesLabel);
    
    statsGroup->setLayout(statsLayout);
    mainLayout->addWidget(statsGroup);
    
    mainLayout->addStretch();
    
    setStyleSheet(
        "QGroupBox { border: 1px solid #3c3c3c; border-radius: 3px; margin-top: 8px; padding-top: 8px; }"
        "QGroupBox::title { subcontrol-origin: margin; subcontrol-position: top left; left: 10px; padding: 0 3px 0 3px; }"
        "QLabel { color: #ccc; }"
    );
}

void LatencyStatusPanel::onPingUpdated(int latencyMs)
{
    // Flash the ping label when updated
    m_pingLabel->setStyleSheet("font-weight: bold; color: #0f0; background: #1a1a1a; padding: 4px; border-radius: 3px; border: 1px solid #0f0;");
    QTimer::singleShot(200, [this]() {
        m_pingLabel->setStyleSheet("font-weight: bold; color: #0f0; background: #1a1a1a; padding: 4px; border-radius: 3px;");
    });
}

void LatencyStatusPanel::onStatsUpdated(const LatencyStats& stats)
{
    updateDisplay();
}

void LatencyStatusPanel::updateDisplay()
{
    const LatencyStats& stats = m_latencyMonitor->getStats();
    
    // Update status
    m_statusLabel->setText(stats.status);
    if (stats.status == "idle") {
        m_statusLabel->setStyleSheet("color: #888; font-weight: bold;");
    } else if (stats.status == "active") {
        m_statusLabel->setStyleSheet("color: #ff0; font-weight: bold;");
    } else if (stats.status == "loading" || stats.status == "computing") {
        m_statusLabel->setStyleSheet("color: #0ff; font-weight: bold;");
    }
    
    // Update ping
    if (stats.currentPing >= 0) {
        m_pingLabel->setText(QString("Ping: %1 ms").arg(stats.currentPing));
        
        // Color code based on latency
        if (stats.currentPing < 10) {
            m_pingLabel->setStyleSheet("font-weight: bold; color: #0f0; background: #1a1a1a; padding: 4px; border-radius: 3px;");
        } else if (stats.currentPing < 50) {
            m_pingLabel->setStyleSheet("font-weight: bold; color: #ff0; background: #1a1a1a; padding: 4px; border-radius: 3px;");
        } else {
            m_pingLabel->setStyleSheet("font-weight: bold; color: #f00; background: #1a1a1a; padding: 4px; border-radius: 3px;");
        }
        
        // Update latency bar
        int barValue = qMin(100, stats.currentPing);
        m_latencyBar->setValue(barValue);
    }
    
    // Update statistics
    if (stats.minPing >= 0) {
        m_minPingLabel->setText(QString("Min: %1 ms").arg(stats.minPing));
    }
    if (stats.maxPing >= 0) {
        m_maxPingLabel->setText(QString("Max: %1 ms").arg(stats.maxPing));
    }
    if (stats.avgPing >= 0) {
        m_avgPingLabel->setText(QString("Avg: %1 ms").arg(stats.avgPing));
    }
    if (stats.totalSamples > 0) {
        m_samplesLabel->setText(QString("Samples: %1").arg(stats.totalSamples));
    }
}

} // namespace RawrXD
