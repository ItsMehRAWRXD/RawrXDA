#pragma once

#include <QObject>
#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include <QProgressBar>
#include <QComboBox>
#include <QSpinBox>
#include <QTimer>
#include <QListWidget>
#include <memory>

class AutonomousModelManager;
class StreamingGGUFMemoryManager;

/**
 * @brief Model Loader Widget - Fully functional autonomous model loading UI
 * 
 * Features:
 * - Stream models >2GB without blocking
 * - Real-time progress tracking
 * - System capability analysis
 * - Automatic model recommendation
 * - Memory management
 */
class ModelLoaderWidget : public QWidget {
    Q_OBJECT

public:
    explicit ModelLoaderWidget(QWidget* parent = nullptr);
    ~ModelLoaderWidget();
    
    // Setup with managers
    void setAutonomousModelManager(AutonomousModelManager* manager);
    void setStreamingMemoryManager(StreamingGGUFMemoryManager* memoryManager);
    
    // Manual model loading
    bool loadModel(const QString& modelPath, const QString& modelId);
    bool loadModelAsync(const QString& modelPath, const QString& modelId, bool stream = true);
    
    // Get current state
    bool isLoading() const { return m_isLoading; }
    QString getCurrentModel() const { return m_currentModelId; }
    
signals:
    void modelLoadingStarted(const QString& modelId);
    void modelLoadingProgress(int percentage, qint64 bytesLoaded, qint64 totalBytes, qint64 speedBytesPerSec);
    void modelLoadingCompleted(const QString& modelId, bool success);
    void modelRecommended(const QString& modelId, const QString& reasoning);
    void errorOccurred(const QString& error);
    void systemAnalysisReady();

private slots:
    void onAutoDetectClicked();
    void onLoadSelectedClicked();
    void onStreamingProgressUpdate();
    void onModelRecommended();
    void onDownloadProgress(const QString& modelId, int percentage, qint64 speedBytesPerSec, qint64 etaSeconds);
    void onDownloadCompleted(const QString& modelId, bool success);
    void onSystemAnalysisComplete();
    void onModelInstalled(const QString& modelId);
    void onOptimizationClicked();
    void onCancelClicked();

private:
    void setupUI();
    void updateModelList();
    void updateInstalledModels();
    void analyzeSystemCapabilities();
    void startProgressMonitoring();
    void stopProgressMonitoring();
    QString formatBytes(qint64 bytes) const;
    QString formatSpeed(qint64 bytesPerSec) const;
    QString formatTime(qint64 seconds) const;
    
    // UI Components
    QLabel* m_systemInfoLabel;
    QLabel* m_recommendedModelLabel;
    QComboBox* m_modelSelector;
    QPushButton* m_autoDetectButton;
    QPushButton* m_loadButton;
    QPushButton* m_optimizeButton;
    QPushButton* m_cancelButton;
    
    QProgressBar* m_loadingProgressBar;
    QLabel* m_progressLabel;
    QLabel* m_speedLabel;
    QLabel* m_etaLabel;
    QLabel* m_statusLabel;
    
    QSpinBox* m_maxMemorySpin;
    QSpinBox* m_prefetchSizeSpin;
    
    QListWidget* m_installedModelsList;
    QListWidget* m_suggestionsListWidget;
    
    // Managers
    AutonomousModelManager* m_modelManager;
    StreamingGGUFMemoryManager* m_memoryManager;
    
    // State
    bool m_isLoading;
    QString m_currentModelId;
    qint64 m_totalBytes;
    qint64 m_loadedBytes;
    QTimer* m_progressTimer;
    std::chrono::high_resolution_clock::time_point m_loadStartTime;
};
