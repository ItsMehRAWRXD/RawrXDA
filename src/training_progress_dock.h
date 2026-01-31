#pragma once


// Forward declarations


class ModelTrainer;

/**
 * @brief Real-time training progress visualization widget
 * 
 * Displays live metrics during model training including:
 * - Current epoch and batch progress
 * - Loss values (training and validation)
 * - Perplexity metrics
 * - Training logs
 * - Time remaining estimates
 * 
 * Designed to be embedded in a QDockWidget for non-intrusive monitoring.
 */
class TrainingProgressDock : public void
{

public:
    /**
     * @brief Construct a new Training Progress Dock
     * @param trainer Pointer to ModelTrainer instance (non-owning)
     * @param parent Parent widget for Qt ownership
     */
    explicit TrainingProgressDock(ModelTrainer* trainer, void* parent = nullptr);
    ~TrainingProgressDock() override = default;
    
    void initialize();

public:
    // Connected to ModelTrainer signals
    void onTrainingStarted();
    void onEpochStarted(int currentEpoch, int totalEpochs);
    void onBatchProcessed(int currentBatch, int totalBatches, float loss);
    void onEpochCompleted(int epoch, float avgLoss, float perplexity);
    void onTrainingCompleted(const std::string& modelPath, float finalLoss);
    void onTrainingStopped();
    void onTrainingError(const std::string& error);
    void onLogMessage(const std::string& message);
    void onValidationResults(float perplexity, const std::string& details);


    /**
     * @brief User requested to stop training
     */
    void stopRequested();

private:
    void onStopButtonClicked();
    void onClearLogsClicked();

private:
    void setupUI();
    void setupConnections();
    void resetMetrics();
    void updateTimeEstimate();
    std::string formatDuration(qint64 seconds) const;

    // UI Components - Status Section
    QLabel* m_statusLabel;
    QLabel* m_epochLabel;
    QLabel* m_batchLabel;
    QLabel* m_timeElapsedLabel;
    QLabel* m_timeRemainingLabel;

    // UI Components - Progress Bars
    QProgressBar* m_epochProgressBar;
    QProgressBar* m_batchProgressBar;

    // UI Components - Metrics Display
    QLabel* m_currentLossLabel;
    QLabel* m_avgLossLabel;
    QLabel* m_perplexityLabel;
    QLabel* m_throughputLabel;

    // UI Components - Tabs
    QTabWidget* m_tabWidget;
    QTextEdit* m_trainingLogEdit;
    QTextEdit* m_validationLogEdit;

    // UI Components - Controls
    QPushButton* m_stopButton;
    QPushButton* m_clearLogsButton;

    // Training State
    ModelTrainer* m_trainer;
    int m_currentEpoch;
    int m_totalEpochs;
    int m_currentBatch;
    int m_totalBatches;
    float m_currentLoss;
    float m_bestLoss;
    qint64 m_trainingStartTime;
    qint64 m_lastBatchTime;
    int m_totalBatchesProcessed;

    // Metrics History
    std::vector<float> m_lossHistory;
    std::vector<float> m_perplexityHistory;
};

