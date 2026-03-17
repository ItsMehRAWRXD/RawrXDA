#pragma once

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <QGroupBox>
#include <QTimer>
#include <memory>

namespace rawr_xd {
    class CompleteModelLoaderSystem;
}

/**
 * @class TelemetryWidget
 * @brief Production-ready system health and model status monitoring
 * 
 * Features:
 * - Real-time CPU, GPU, Memory monitoring
 * - Thermal throttling detection
 * - Model compression stats
 * - Tier selection and hotpatching
 * - Auto-tuning controls
 */
class TelemetryWidget : public QWidget {
    Q_OBJECT

public:
    explicit TelemetryWidget(QWidget* parent = nullptr);
    ~TelemetryWidget() override;

    void setModelLoader(rawr_xd::CompleteModelLoaderSystem* loader);
    void updateMetrics();
    void updateModelInfo(const QString& modelPath, const QString& compressionStats);
    void setCurrentTier(const QString& tier);
    void displaySystemHealth();

signals:
    void tierSelectionChanged(const QString& newTier);
    void autoTuneRequested();
    void qualityTestRequested();
    void benchmarkRequested();
    void modelLoadRequested();
    void modelUnloadRequested();

private slots:
    void onUpdateTimer();
    void onTierChanged(int index);
    void onAutoTuneClicked();
    void onQualityTestClicked();
    void onBenchmarkClicked();
    void onLoadModelClicked();
    void onUnloadModelClicked();

private:
    void setupUi();
    void initializeTimer();
    void updateSystemMetrics();
    void updateModelMetrics();

    // System Metrics Labels
    QLabel* m_cpuLabel = nullptr;
    QLabel* m_gpuLabel = nullptr;
    QLabel* m_memoryLabel = nullptr;
    QLabel* m_thermalLabel = nullptr;
    QProgressBar* m_cpuProgress = nullptr;
    QProgressBar* m_gpuProgress = nullptr;
    QProgressBar* m_memoryProgress = nullptr;

    // Model Info Labels
    QLabel* m_modelNameLabel = nullptr;
    QLabel* m_modelSizeLabel = nullptr;
    QLabel* m_compressionLabel = nullptr;
    QProgressBar* m_modelLoadProgress = nullptr;

    // Tier Management
    QLabel* m_currentTierLabel = nullptr;
    QComboBox* m_tierSelector = nullptr;

    // Control Buttons
    QPushButton* m_loadModelButton = nullptr;
    QPushButton* m_unloadModelButton = nullptr;
    QPushButton* m_autoTuneButton = nullptr;
    QPushButton* m_qualityTestButton = nullptr;
    QPushButton* m_benchmarkButton = nullptr;

    // Model Loader Reference
    rawr_xd::CompleteModelLoaderSystem* m_modelLoader = nullptr;

    // Timer for periodic updates
    std::unique_ptr<QTimer> m_updateTimer;
};
