#include "training_progress_dock.h"
#include "model_trainer.h"


TrainingProgressDock::TrainingProgressDock(ModelTrainer* trainer, void* parent)
    : void(parent)
    , m_trainer(trainer)
    , m_currentEpoch(0)
    , m_totalEpochs(0)
    , m_currentBatch(0)
    , m_totalBatches(0)
    , m_currentLoss(0.0f)
    , m_bestLoss(std::numeric_limits<float>::max())
    , m_trainingStartTime(0)
    , m_lastBatchTime(0)
    , m_totalBatchesProcessed(0)
    , m_statusLabel(nullptr)
{
    // Lightweight constructor - defer Qt widget creation
}

void TrainingProgressDock::initialize() {
    if (m_statusLabel) return;  // Already initialized
    
    setupUI();
    setupConnections();
    resetMetrics();
}

void TrainingProgressDock::setupUI()
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(8, 8, 8, 8);
    mainLayout->setSpacing(8);

    // ===== Status Section =====
    QGroupBox* statusGroup = new QGroupBox("Training Status", this);
    QGridLayout* statusLayout = new QGridLayout(statusGroup);

    m_statusLabel = new QLabel("Idle", this);
    m_statusLabel->setStyleSheet("QLabel { font-weight: bold; font-size: 14px; }");
    statusLayout->addWidget(new QLabel("Status:", this), 0, 0);
    statusLayout->addWidget(m_statusLabel, 0, 1, 1, 3);

    m_epochLabel = new QLabel("Epoch: 0 / 0", this);
    statusLayout->addWidget(m_epochLabel, 1, 0, 1, 2);

    m_batchLabel = new QLabel("Batch: 0 / 0", this);
    statusLayout->addWidget(m_batchLabel, 1, 2, 1, 2);

    m_timeElapsedLabel = new QLabel("Elapsed: 00:00:00", this);
    statusLayout->addWidget(m_timeElapsedLabel, 2, 0, 1, 2);

    m_timeRemainingLabel = new QLabel("Remaining: --:--:--", this);
    statusLayout->addWidget(m_timeRemainingLabel, 2, 2, 1, 2);

    mainLayout->addWidget(statusGroup);

    // ===== Progress Bars =====
    QGroupBox* progressGroup = new QGroupBox("Progress", this);
    QGridLayout* progressLayout = new QGridLayout(progressGroup);

    progressLayout->addWidget(new QLabel("Epoch Progress:", this), 0, 0);
    m_epochProgressBar = new QProgressBar(this);
    m_epochProgressBar->setMinimum(0);
    m_epochProgressBar->setMaximum(100);
    m_epochProgressBar->setValue(0);
    m_epochProgressBar->setTextVisible(true);
    progressLayout->addWidget(m_epochProgressBar, 0, 1);

    progressLayout->addWidget(new QLabel("Batch Progress:", this), 1, 0);
    m_batchProgressBar = new QProgressBar(this);
    m_batchProgressBar->setMinimum(0);
    m_batchProgressBar->setMaximum(100);
    m_batchProgressBar->setValue(0);
    m_batchProgressBar->setTextVisible(true);
    progressLayout->addWidget(m_batchProgressBar, 1, 1);

    mainLayout->addWidget(progressGroup);

    // ===== Metrics Section =====
    QGroupBox* metricsGroup = new QGroupBox("Training Metrics", this);
    QGridLayout* metricsLayout = new QGridLayout(metricsGroup);

    metricsLayout->addWidget(new QLabel("Current Loss:", this), 0, 0);
    m_currentLossLabel = new QLabel("--", this);
    m_currentLossLabel->setStyleSheet("QLabel { font-weight: bold; color: #2196F3; }");
    metricsLayout->addWidget(m_currentLossLabel, 0, 1);

    metricsLayout->addWidget(new QLabel("Average Loss:", this), 0, 2);
    m_avgLossLabel = new QLabel("--", this);
    m_avgLossLabel->setStyleSheet("QLabel { font-weight: bold; }");
    metricsLayout->addWidget(m_avgLossLabel, 0, 3);

    metricsLayout->addWidget(new QLabel("Perplexity:", this), 1, 0);
    m_perplexityLabel = new QLabel("--", this);
    m_perplexityLabel->setStyleSheet("QLabel { font-weight: bold; color: #4CAF50; }");
    metricsLayout->addWidget(m_perplexityLabel, 1, 1);

    metricsLayout->addWidget(new QLabel("Throughput:", this), 1, 2);
    m_throughputLabel = new QLabel("-- batches/sec", this);
    metricsLayout->addWidget(m_throughputLabel, 1, 3);

    mainLayout->addWidget(metricsGroup);

    // ===== Logs Section =====
    m_tabWidget = new QTabWidget(this);

    m_trainingLogEdit = new QTextEdit(this);
    m_trainingLogEdit->setReadOnly(true);
    m_trainingLogEdit->setStyleSheet("QTextEdit { font-family: 'Consolas', monospace; font-size: 9pt; }");
    m_tabWidget->addTab(m_trainingLogEdit, "Training Log");

    m_validationLogEdit = new QTextEdit(this);
    m_validationLogEdit->setReadOnly(true);
    m_validationLogEdit->setStyleSheet("QTextEdit { font-family: 'Consolas', monospace; font-size: 9pt; }");
    m_tabWidget->addTab(m_validationLogEdit, "Validation Log");

    mainLayout->addWidget(m_tabWidget, 1); // Stretch factor 1

    // ===== Control Buttons =====
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch();

    m_clearLogsButton = new QPushButton("Clear Logs", this);
    buttonLayout->addWidget(m_clearLogsButton);

    m_stopButton = new QPushButton("Stop Training", this);
    m_stopButton->setEnabled(false);
    m_stopButton->setStyleSheet("QPushButton { background-color: #f44336; color: white; font-weight: bold; }");
    buttonLayout->addWidget(m_stopButton);

    mainLayout->addLayout(buttonLayout);
}

void TrainingProgressDock::setupConnections()
{
    if (m_trainer) {
        // Connect to ModelTrainer signals
// Qt connect removed
// Qt connect removed
// Qt connect removed
// Qt connect removed
// Qt connect removed
// Qt connect removed
// Qt connect removed
// Qt connect removed
// Qt connect removed
    }

    // Connect UI buttons
// Qt connect removed
// Qt connect removed
}

void TrainingProgressDock::resetMetrics()
{
    m_currentEpoch = 0;
    m_totalEpochs = 0;
    m_currentBatch = 0;
    m_totalBatches = 0;
    m_currentLoss = 0.0f;
    m_bestLoss = std::numeric_limits<float>::max();
    m_trainingStartTime = 0;
    m_lastBatchTime = 0;
    m_totalBatchesProcessed = 0;

    m_lossHistory.clear();
    m_perplexityHistory.clear();

    m_statusLabel->setText("Idle");
    m_statusLabel->setStyleSheet("QLabel { font-weight: bold; font-size: 14px; color: gray; }");
    m_epochLabel->setText("Epoch: 0 / 0");
    m_batchLabel->setText("Batch: 0 / 0");
    m_timeElapsedLabel->setText("Elapsed: 00:00:00");
    m_timeRemainingLabel->setText("Remaining: --:--:--");
    m_epochProgressBar->setValue(0);
    m_batchProgressBar->setValue(0);
    m_currentLossLabel->setText("--");
    m_avgLossLabel->setText("--");
    m_perplexityLabel->setText("--");
    m_throughputLabel->setText("-- batches/sec");
    m_stopButton->setEnabled(false);
}

void TrainingProgressDock::onTrainingStarted()
{
    resetMetrics();
    m_trainingStartTime = std::chrono::system_clock::time_point::currentSecsSinceEpoch();
    m_lastBatchTime = m_trainingStartTime;

    m_statusLabel->setText("Training...");
    m_statusLabel->setStyleSheet("QLabel { font-weight: bold; font-size: 14px; color: #4CAF50; }");
    m_stopButton->setEnabled(true);

    std::string timestamp = std::chrono::system_clock::time_point::currentDateTime().toString("hh:mm:ss");
    m_trainingLogEdit->append(std::string("[%1] Training started"));
}

void TrainingProgressDock::onEpochStarted(int currentEpoch, int totalEpochs)
{
    m_currentEpoch = currentEpoch;
    m_totalEpochs = totalEpochs;

    m_epochLabel->setText(std::string("Epoch: %1 / %2"));

    // Update epoch progress bar
    if (totalEpochs > 0) {
        int progress = static_cast<int>((static_cast<float>(currentEpoch - 1) / totalEpochs) * 100);
        m_epochProgressBar->setValue(progress);
    }

    std::string timestamp = std::chrono::system_clock::time_point::currentDateTime().toString("hh:mm:ss");
    m_trainingLogEdit->append(std::string("[%1] Starting epoch %2/%3"));
}

void TrainingProgressDock::onBatchProcessed(int currentBatch, int totalBatches, float loss)
{
    m_currentBatch = currentBatch;
    m_totalBatches = totalBatches;
    m_currentLoss = loss;
    m_totalBatchesProcessed++;

    // Update batch label and progress
    m_batchLabel->setText(std::string("Batch: %1 / %2"));

    if (totalBatches > 0) {
        int progress = static_cast<int>((static_cast<float>(currentBatch) / totalBatches) * 100);
        m_batchProgressBar->setValue(progress);
    }

    // Update current loss
    m_currentLossLabel->setText(std::string::number(loss, 'f', 6));

    // Track best loss
    if (loss < m_bestLoss) {
        m_bestLoss = loss;
    }

    // Add to history
    m_lossHistory.append(loss);

    // Update time estimates
    updateTimeEstimate();

    // Log every 10 batches to avoid spam
    if (currentBatch % 10 == 0 || currentBatch == totalBatches) {
        std::string timestamp = std::chrono::system_clock::time_point::currentDateTime().toString("hh:mm:ss");
        m_trainingLogEdit->append(std::string("[%1] Batch %2/%3 - Loss: %4")


            );
        
        // Auto-scroll to bottom
        m_trainingLogEdit->verticalScrollBar()->setValue(
            m_trainingLogEdit->verticalScrollBar()->maximum());
    }
}

void TrainingProgressDock::onEpochCompleted(int epoch, float avgLoss, float perplexity)
{
    m_avgLossLabel->setText(std::string::number(avgLoss, 'f', 6));
    m_perplexityLabel->setText(std::string::number(perplexity, 'f', 2));

    m_perplexityHistory.append(perplexity);

    // Update epoch progress
    if (m_totalEpochs > 0) {
        int progress = static_cast<int>((static_cast<float>(epoch) / m_totalEpochs) * 100);
        m_epochProgressBar->setValue(progress);
    }

    // Reset batch progress for next epoch
    m_batchProgressBar->setValue(0);

    std::string timestamp = std::chrono::system_clock::time_point::currentDateTime().toString("hh:mm:ss");
    m_trainingLogEdit->append(std::string("[%1] ✓ Epoch %2 completed - Avg Loss: %3, Perplexity: %4")


        );

    // Auto-scroll
    m_trainingLogEdit->verticalScrollBar()->setValue(
        m_trainingLogEdit->verticalScrollBar()->maximum());
}

void TrainingProgressDock::onTrainingCompleted(const std::string& modelPath, float finalLoss)
{
    m_statusLabel->setText("Completed");
    m_statusLabel->setStyleSheet("QLabel { font-weight: bold; font-size: 14px; color: #4CAF50; }");
    m_stopButton->setEnabled(false);

    m_epochProgressBar->setValue(100);
    m_batchProgressBar->setValue(100);

    std::string timestamp = std::chrono::system_clock::time_point::currentDateTime().toString("hh:mm:ss");
    m_trainingLogEdit->append(std::string("[%1] ✓✓✓ Training completed successfully!"));
    m_trainingLogEdit->append(std::string("[%1] Model saved to: %2"));
    m_trainingLogEdit->append(std::string("[%1] Final Loss: %2"));

    // Auto-scroll
    m_trainingLogEdit->verticalScrollBar()->setValue(
        m_trainingLogEdit->verticalScrollBar()->maximum());
}

void TrainingProgressDock::onTrainingStopped()
{
    m_statusLabel->setText("Stopped");
    m_statusLabel->setStyleSheet("QLabel { font-weight: bold; font-size: 14px; color: #FF9800; }");
    m_stopButton->setEnabled(false);

    std::string timestamp = std::chrono::system_clock::time_point::currentDateTime().toString("hh:mm:ss");
    m_trainingLogEdit->append(std::string("[%1] Training stopped by user"));

    // Auto-scroll
    m_trainingLogEdit->verticalScrollBar()->setValue(
        m_trainingLogEdit->verticalScrollBar()->maximum());
}

void TrainingProgressDock::onTrainingError(const std::string& error)
{
    m_statusLabel->setText("Error");
    m_statusLabel->setStyleSheet("QLabel { font-weight: bold; font-size: 14px; color: #f44336; }");
    m_stopButton->setEnabled(false);

    std::string timestamp = std::chrono::system_clock::time_point::currentDateTime().toString("hh:mm:ss");
    m_trainingLogEdit->append(std::string("[%1] ✗ ERROR: %2"));

    // Auto-scroll
    m_trainingLogEdit->verticalScrollBar()->setValue(
        m_trainingLogEdit->verticalScrollBar()->maximum());
}

void TrainingProgressDock::onLogMessage(const std::string& message)
{
    std::string timestamp = std::chrono::system_clock::time_point::currentDateTime().toString("hh:mm:ss");
    m_trainingLogEdit->append(std::string("[%1] %2"));

    // Auto-scroll
    m_trainingLogEdit->verticalScrollBar()->setValue(
        m_trainingLogEdit->verticalScrollBar()->maximum());
}

void TrainingProgressDock::onValidationResults(float perplexity, const std::string& details)
{
    m_perplexityLabel->setText(std::string::number(perplexity, 'f', 2));

    std::string timestamp = std::chrono::system_clock::time_point::currentDateTime().toString("hh:mm:ss");
    m_validationLogEdit->append(std::string("[%1] Validation Perplexity: %2")
        
        );

    if (!details.isEmpty()) {
        m_validationLogEdit->append(std::string("[%1] Details: %2"));
    }

    // Auto-scroll
    m_validationLogEdit->verticalScrollBar()->setValue(
        m_validationLogEdit->verticalScrollBar()->maximum());
}

void TrainingProgressDock::onStopButtonClicked()
{
    stopRequested();
    m_stopButton->setEnabled(false);
    m_stopButton->setText("Stopping...");

    std::string timestamp = std::chrono::system_clock::time_point::currentDateTime().toString("hh:mm:ss");
    m_trainingLogEdit->append(std::string("[%1] Stop requested..."));
}

void TrainingProgressDock::onClearLogsClicked()
{
    m_trainingLogEdit->clear();
    m_validationLogEdit->clear();
}

void TrainingProgressDock::updateTimeEstimate()
{
    qint64 currentTime = std::chrono::system_clock::time_point::currentSecsSinceEpoch();
    qint64 elapsed = currentTime - m_trainingStartTime;

    // Update elapsed time
    m_timeElapsedLabel->setText(std::string("Elapsed: %1")));

    // Calculate throughput
    if (elapsed > 0 && m_totalBatchesProcessed > 0) {
        float batchesPerSec = static_cast<float>(m_totalBatchesProcessed) / elapsed;
        m_throughputLabel->setText(std::string("%1 batches/sec"));

        // Estimate remaining time
        if (m_totalEpochs > 0 && m_totalBatches > 0) {
            int totalBatchesRemaining = (m_totalEpochs - m_currentEpoch) * m_totalBatches 
                                       + (m_totalBatches - m_currentBatch);
            
            if (batchesPerSec > 0) {
                qint64 remainingSeconds = static_cast<qint64>(totalBatchesRemaining / batchesPerSec);
                m_timeRemainingLabel->setText(std::string("Remaining: %1")));
            }
        }
    }
}

std::string TrainingProgressDock::formatDuration(qint64 seconds) const
{
    int hours = seconds / 3600;
    int minutes = (seconds % 3600) / 60;
    int secs = seconds % 60;

    return std::string("%1:%2:%3")
        )
        )
        );
}

