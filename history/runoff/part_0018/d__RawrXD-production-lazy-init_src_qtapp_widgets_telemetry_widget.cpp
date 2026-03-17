#include "telemetry_widget.h"
#include "../integration/ProdIntegration.h"
#include "../integration/InitializationTracker.h"
#include "../../telemetry_singleton.h"
#include "../telemetry.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QProgressBar>
#include <QComboBox>
#include <QTableWidget>
#include <QHeaderView>
#include <QAbstractItemView>
#include <QPushButton>
#include <QTimer>
#include <QFileDialog>
#include <QMessageBox>
#include <QDateTime>

#ifdef _WIN32
#include <windows.h>
#include <psapi.h>
#pragma comment(lib, "psapi.lib")
#endif

TelemetryWidget::TelemetryWidget(QWidget* parent) : QWidget(parent)
{
    RawrXD::Integration::ScopedInitTimer init("TelemetryWidget");
    QVBoxLayout* layout = new QVBoxLayout(this);
    
    QLabel* title = new QLabel("Telemetry Dashboard - REAL METRICS", this);
    title->setStyleSheet("font-size: 16px; font-weight: bold; color: #00ff00;");
    layout->addWidget(title);
    
    // Performance metrics from REAL hardware telemetry
    QLabel* perfLabel = new QLabel("Hardware Metrics (Real-time)", this);
    perfLabel->setStyleSheet("font-size: 14px; color: #cccccc;");
    layout->addWidget(perfLabel);
    
    mCpuUsage = new QProgressBar(this);
    mCpuUsage->setRange(0, 100);
    mCpuUsage->setValue(0);
    mCpuUsage->setFormat("CPU Usage: %p%");
    layout->addWidget(mCpuUsage);
    
    mMemoryUsage = new QProgressBar(this);
    mMemoryUsage->setRange(0, 100);
    mMemoryUsage->setValue(0);
    mMemoryUsage->setFormat("Memory Usage: %p%");
    layout->addWidget(mMemoryUsage);
    
    // GPU metrics from real telemetry::Poll()
    mGpuUsage = new QProgressBar(this);
    mGpuUsage->setRange(0, 100);
    mGpuUsage->setValue(0);
    mGpuUsage->setFormat("GPU Usage: %p%");
    layout->addWidget(mGpuUsage);
    
    mCpuTempLabel = new QLabel("CPU Temp: —", this);
    mGpuTempLabel = new QLabel("GPU Temp: —", this);
    layout->addWidget(mCpuTempLabel);
    layout->addWidget(mGpuTempLabel);
    
    // Event statistics from REAL recorded events
    QLabel* eventLabel = new QLabel("Telemetry Events (Recorded)", this);
    eventLabel->setStyleSheet("font-size: 14px; color: #cccccc;");
    layout->addWidget(eventLabel);
    
    mEventCountLabel = new QLabel("Total Events: 0", this);
    layout->addWidget(mEventCountLabel);
    
    mLastEventLabel = new QLabel("Last Event: —", this);
    mLastEventLabel->setWordWrap(true);
    layout->addWidget(mLastEventLabel);
    
    // Event filter
    QHBoxLayout* filterRow = new QHBoxLayout();
    filterRow->addWidget(new QLabel("Filter Events:", this));
    mEventFilterCombo = new QComboBox(this);
    mEventFilterCombo->addItem("All Events");
    mEventFilterCombo->addItem("GGUF Operations");
    mEventFilterCombo->addItem("Blob Conversions");
    mEventFilterCombo->addItem("Ollama Operations");
    mEventFilterCombo->addItem("Compression");
    mEventFilterCombo->addItem("Model Loading");
    mEventFilterCombo->addItem("IDE Events");
    connect(mEventFilterCombo, &QComboBox::currentTextChanged, this, &TelemetryWidget::onFilterChanged);
    filterRow->addWidget(mEventFilterCombo, 1);
    layout->addLayout(filterRow);
    
    // Event history table
    setupEventHistoryTable();
    layout->addWidget(mEventHistoryTable);
    
    // Controls
    QPushButton* refreshBtn = new QPushButton("Refresh Metrics", this);
    connect(refreshBtn, &QPushButton::clicked, this, &TelemetryWidget::refreshMetrics);
    layout->addWidget(refreshBtn);
    
    QPushButton* exportBtn = new QPushButton("Export Telemetry Data", this);
    connect(exportBtn, &QPushButton::clicked, this, &TelemetryWidget::onExportData);
    layout->addWidget(exportBtn);
    
    layout->addStretch();
    
    // Auto-refresh timer for REAL data
    mRefreshTimer = new QTimer(this);
    connect(mRefreshTimer, &QTimer::timeout, this, &TelemetryWidget::refreshMetrics);
    connect(mRefreshTimer, &QTimer::timeout, this, &TelemetryWidget::refreshEventHistory);
    mRefreshTimer->start(2000); // Refresh every 2 seconds with real data
    
    // Initial refresh
    refreshMetrics();
    refreshEventHistory();
}

TelemetryWidget::~TelemetryWidget()
{
    // Cleanup if needed
}

void TelemetryWidget::refreshMetrics()
{
    RawrXD::Integration::ScopedTimer timer("TelemetryWidget", "refreshMetrics", "update");
    
    // Get REAL hardware telemetry from telemetry::Poll()
    TelemetrySnapshot snapshot;
    bool polled = telemetry::Poll(snapshot);
    
    if (polled) {
        // Update CPU metrics from real hardware
        mCpuUsage->setValue(static_cast<int>(snapshot.cpuUsagePercent));
        
        if (snapshot.cpuTempValid) {
            mCpuTempLabel->setText(QString("CPU Temp: %1°C").arg(snapshot.cpuTempC, 0, 'f', 1));
            mCpuTempLabel->setStyleSheet(snapshot.cpuTempC > 80.0 ? "color: red;" : "color: #cccccc;");
        } else {
            mCpuTempLabel->setText("CPU Temp: N/A");
        }
        
        // Update GPU metrics from real hardware
        mGpuUsage->setValue(static_cast<int>(snapshot.gpuUsagePercent));
        
        if (snapshot.gpuTempValid) {
            mGpuTempLabel->setText(QString("GPU Temp: %1°C (%2)").arg(snapshot.gpuTempC, 0, 'f', 1).arg(QString::fromStdString(snapshot.gpuVendor)));
            mGpuTempLabel->setStyleSheet(snapshot.gpuTempC > 85.0 ? "color: red;" : "color: #cccccc;");
        } else {
            mGpuTempLabel->setText("GPU Temp: N/A");
        }
    } else {
        mCpuTempLabel->setText("Telemetry: Hardware polling unavailable");
        mCpuTempLabel->setStyleSheet("color: orange;");
    }
    
    // Get REAL memory usage from OS
#ifdef _WIN32
    MEMORYSTATUSEX memInfo;
    memInfo.dwLength = sizeof(MEMORYSTATUSEX);
    if (GlobalMemoryStatusEx(&memInfo)) {
        int memPercent = static_cast<int>(memInfo.dwMemoryLoad);
        mMemoryUsage->setValue(memPercent);
    }
#endif
    
    // Access REAL telemetry events using new getter methods
    Telemetry& telem = GetTelemetry();
    int eventCount = telem.getEventCount();
    QString lastEvent = telem.getLastEventName();
    
    // Record this refresh as a real event
    GetTelemetry().recordEvent("telemetry_widget_refresh", QJsonObject{
        {"cpu_usage", snapshot.cpuUsagePercent},
        {"cpu_temp", snapshot.cpuTempC},
        {"gpu_usage", snapshot.gpuUsagePercent}
    });
    
    // Update event count with REAL data
    mEventCountLabel->setText(QString("Total Events: %1").arg(eventCount));
    
    if (!lastEvent.isEmpty()) {
        mLastEventLabel->setText(QString("Last Event: %1 at %2").arg(lastEvent).arg(QDateTime::currentDateTime().toString("hh:mm:ss")));
    } else {
        mLastEventLabel->setText("Last Event: \u2014");
    }
}

void TelemetryWidget::onExportData()
{
    QString filepath = QFileDialog::getSaveFileName(this, "Export Telemetry Data", "", "JSON Files (*.json)");
    if (filepath.isEmpty()) return;
    
    // Call REAL telemetry save function
    bool success = GetTelemetry().saveTelemetry(filepath);
    
    if (success) {
        QMessageBox::information(this, "Export Success", QString("Telemetry data exported to: %1").arg(filepath));
        RawrXD::Integration::logInfo("TelemetryWidget", "export_success", filepath);
    } else {
        QMessageBox::warning(this, "Export Failed", "Failed to export telemetry data");
        RawrXD::Integration::logError("TelemetryWidget", "export_failed", filepath);
    }
}

void TelemetryWidget::setupEventHistoryTable()
{
    mEventHistoryTable = new QTableWidget(this);
    mEventHistoryTable->setColumnCount(3);
    mEventHistoryTable->setHorizontalHeaderLabels({"Event Name", "Timestamp", "Metadata"});
    mEventHistoryTable->horizontalHeader()->setStretchLastSection(true);
    mEventHistoryTable->setAlternatingRowColors(true);
    mEventHistoryTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    mEventHistoryTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    mEventHistoryTable->setMaximumHeight(200);
}

void TelemetryWidget::refreshEventHistory()
{
    mEventHistoryTable->setRowCount(0);
    
    QJsonArray events = GetTelemetry().getEvents();
    QString filter = mEventFilterCombo->currentText();
    
    // Apply filter
    for (int i = events.size() - 1; i >= 0 && mEventHistoryTable->rowCount() < 50; --i) {
        QJsonObject event = events[i].toObject();
        QString eventName = event["event"].toString();
        
        // Filter logic
        if (filter != "All Events") {
            if (filter == "GGUF Operations" && !eventName.contains("gguf")) continue;
            if (filter == "Blob Conversions" && !eventName.contains("blob")) continue;
            if (filter == "Ollama Operations" && !eventName.contains("ollama")) continue;
            if (filter == "Compression" && !eventName.contains("compress") && !eventName.contains("decompress")) continue;
            if (filter == "Model Loading" && !eventName.contains("model_load") && !eventName.contains("model_cache")) continue;
            if (filter == "IDE Events" && !eventName.contains("ide")) continue;
        }
        
        int row = mEventHistoryTable->rowCount();
        mEventHistoryTable->insertRow(row);
        
        mEventHistoryTable->setItem(row, 0, new QTableWidgetItem(eventName));
        
        qint64 timestamp = event["timestamp"].toVariant().toLongLong();
        QDateTime dt = QDateTime::fromMSecsSinceEpoch(timestamp);
        mEventHistoryTable->setItem(row, 1, new QTableWidgetItem(dt.toString("yyyy-MM-dd hh:mm:ss")));
        
        QJsonObject metadata = event["metadata"].toObject();
        QString metaStr = QJsonDocument(metadata).toJson(QJsonDocument::Compact);
        mEventHistoryTable->setItem(row, 2, new QTableWidgetItem(metaStr));
    }
}

void TelemetryWidget::onFilterChanged(const QString& filter)
{
    refreshEventHistory();
}

void TelemetryWidget::updateCPUMetric(int value)
{
    if (mCpuUsage) {
        mCpuUsage->setValue(value);
    }
}

void TelemetryWidget::updateMemoryMetric(int value)
{
    if (mMemoryUsage) {
        mMemoryUsage->setValue(value);
    }
}

void TelemetryWidget::updateLatencyMetric(int value)
{
    // Update latency display - could add a dedicated label/progress bar
    Q_UNUSED(value);
}

void TelemetryWidget::updateTokenRate(int value)
{
    // Update token rate display - could add a dedicated label
    Q_UNUSED(value);
}