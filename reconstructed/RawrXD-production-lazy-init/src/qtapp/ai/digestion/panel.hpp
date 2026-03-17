#ifndef AI_DIGESTION_PANEL_HPP
#define AI_DIGESTION_PANEL_HPP

#include "ai_digestion_engine.hpp"
#include "ai_training_pipeline.hpp"
#include "ai_workers.h"
#include <QtWidgets/QWidget>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QTabWidget>
#include <QtWidgets/QGroupBox>
#include <QtWidgets/QLabel>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QProgressBar>
#include <QtWidgets/QTextEdit>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QSpinBox>
#include <QtWidgets/QDoubleSpinBox>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QListWidget>
#include <QtWidgets/QListWidgetItem>
#include <QtWidgets/QTreeWidget>
#include <QtWidgets/QTreeWidgetItem>
#include <QtWidgets/QTableWidget>
#include <QtWidgets/QTableWidgetItem>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QSplitter>
#include <QtWidgets/QScrollArea>
#include <QtWidgets/QFrame>
#include <QtWidgets/QSlider>
#include <QtWidgets/QFileDialog>
#include <QtWidgets/QMessageBox>
#include <QtCore/QTimer>
#include <QtCore/QJsonObject>
#include <memory>

// Forward declarations
class FileDropWidget;
class ParameterWidget;
class ModelPresetWidget;
class TrainingMetricsWidget;
class ModelManagerWidget;

class AIDigestionPanel : public QWidget {
    Q_OBJECT

public:
    explicit AIDigestionPanel(QWidget* parent = nullptr);
    virtual ~AIDigestionPanel();

public slots:
    void resetToDefaults();
    void loadPreset(const QString& presetName);
    void saveCurrentSettings();
    void exportTrainingData();
    void importTrainingData();

signals:
    void digestionStarted();
    void digestionCompleted(const QString& datasetPath);
    void trainingStarted();
    void trainingCompleted(const QString& modelPath);
    void modelCreated(const QString& modelName, const QString& modelPath);

private slots:
    // UI update slots
    void updateProgressDisplays();
    void updateParameterWidgets();
    
    // Model management slots
    void onLoadModelClicked();
    void onTestModelClicked();
    void onExportModelClicked();
    void onDeleteModelClicked();
    void updateModelList();
    
    // File management
    void onAddFilesClicked();
    void onAddDirectoryClicked();
    void onClearFilesClicked();
    void onFileItemChanged(QListWidgetItem* item);
    void onFilesDropped(const QStringList& files);
    
    // Digestion control
    void onStartDigestionClicked();
    void onStopDigestionClicked();
    void onPauseDigestionClicked();
    void onResumeDigestionClicked();
    
    // Training control
    void onStartTrainingClicked();
    void onStopTrainingClicked();
    void onPauseTrainingClicked();
    void onResumeTrainingClicked();
    
    // Parameter updates
    void onParametersChanged();
    void onPresetSelected(int index);
    void onSavePresetClicked();
    void onDeletePresetClicked();
    void onSelectOutputDirectory();
    
    // Worker events - Digestion
    void onDigestionProgressChanged(const AIDigestionWorker::Progress& progress);
    void onDigestionFileStarted(const QString& fileName);
    void onDigestionFileCompleted(const QString& fileName, bool success);
    void onDigestionFinished(bool success, const QString& message);
    void onDigestionError(const QString& error);
    void onDigestionStateChanged(AIDigestionWorker::State state);
    void onDigestionDatasetReady(const TrainingDataset& dataset);
    
    // Worker events - Training
    void onTrainingProgressChanged(const AITrainingWorker::Progress& progress);
    void onTrainingEpochStarted(int epoch);
    void onTrainingEpochCompleted(int epoch, double loss, double accuracy);
    void onTrainingBatchCompleted(int batch, double batchLoss);
    void onTrainingValidationCompleted(double valLoss, double valAccuracy);
    void onTrainingCheckpointSaved(const QString& path);
    void onTrainingCompleted(bool success, const QString& modelPath);
    void onTrainingError(const QString& error);
    void onTrainingStateChanged(AITrainingWorker::State state);
    
    // Parameter updates
    void onModelSizeChanged(int sizeGB);
    void onQuantizationChanged(const QString& quantization);
    void onExtractionModeChanged(int mode);
    void onLearningRateChanged(double rate);
    void onEpochsChanged(int epochs);
    void onBatchSizeChanged(int size);
    void onMaxTokensChanged(int tokens);
    
    // Engine events
    void onDigestionProgress(double progress);
    void onDigestionStatusChanged(const QString& status);
    void onFileProcessed(const QString& filePath, int processedCount, int totalCount);
    
    void onDigestionFailed(const QString& error);
    
    // Training events
    void onTrainingProgress(double progress, const QJsonObject& metrics);
    void onTrainingStatusChanged(const QString& status);
    void onEpochCompleted(int epoch, double loss, double accuracy);
    void onTrainingFinished(const QString& modelPath);
    void onTrainingErrorWorker(const QString& error);  // Renamed to avoid conflict
    void onModelValidated(const QString& modelPath, bool isValid);
    void onModelQuantized(const QString& modelPath, const QString& quantization);

private:
    void setupUI();
    void setupFileInputTab();
    void setupParametersTab();
    void setupTrainingTab();
    void setupModelsTab();
    void setupLogTab();
    
    void setupDigestionControls();
    void setupTrainingControls();
    void setupParameterGroups();
    void setupPresetButtons();
    void setupModelPresets();
    
    void connectSignals();
    void connectWorkerSignals();
    void applyModelPreset(const QString& presetName);
    void validateInputs();
    void updateDigestionConfig();
    void updateTrainingConfig();
    void updateFileList();
    
    // Helper methods
    void addFileToList(const QString& filePath);
    int addDirectoryToList(const QString& dirPath);
    void updateFileStats();
    void setDigestionControlsEnabled(bool enabled);
    void setTrainingControlsEnabled(bool enabled);
    // void loadPreset(const QString& presetName);  // REMOVED - already declared as public slot
    void savePreset(const QString& presetName);
    void deletePreset(const QString& presetName);
    void logMessage(const QString& message);
    
    void showDigestionResults(const TrainingDataset& dataset);
    void showTrainingResults(const QString& modelPath);
    void showErrorMessage(const QString& title, const QString& message);
    void showSuccessMessage(const QString& title, const QString& message);
    
    QString formatFileSize(qint64 bytes);
    QString formatDuration(int seconds);
    QString formatProgress(double progress);

private:
    // Core components
    std::unique_ptr<AIDigestionEngine> m_digestionEngine;
    std::unique_ptr<AITrainingPipeline> m_trainingPipeline;
    
    // Background workers
    std::unique_ptr<AIWorkerManager> m_workerManager;
    AIDigestionWorker* m_digestionWorker;
    AITrainingWorker* m_trainingWorker;
    
    // Current dataset and configuration
    TrainingDataset m_currentDataset;
    AITrainingWorker::TrainingConfig m_trainingConfig;
    
    // UI Layout
    QTabWidget* m_tabWidget;
    QWidget* m_fileInputTab;
    QWidget* m_parametersTab;
    QWidget* m_trainingTab;
    QWidget* m_modelsTab;
    QWidget* m_logTab;
    
    // File Input Tab
    QVBoxLayout* m_fileLayout;
    FileDropWidget* m_fileDropWidget;
    QListWidget* m_fileListWidget;
    QHBoxLayout* m_fileButtonLayout;
    QPushButton* m_addFilesButton;
    QPushButton* m_addDirectoryButton;
    QPushButton* m_clearFilesButton;
    QLabel* m_fileStatsLabel;
    QCheckBox* m_recursiveCheckBox;
    
    // Parameters Tab
    QVBoxLayout* m_parametersLayout;
    QScrollArea* m_parametersScrollArea;
    QWidget* m_parametersWidget;
    
    // Parameter Controls
    QComboBox* m_presetCombo;
    QPushButton* m_savePresetButton;
    QPushButton* m_deletePresetButton;
    // QLineEdit* m_modelNameEdit;  // REMOVED - duplicate, see m_modelConfigGroup section
    // QLineEdit* m_outputDirectoryEdit;  // REMOVED - duplicate, see m_modelConfigGroup section
    QPushButton* m_browseOutputButton;
    
    // Extraction Parameters
    QCheckBox* m_extractKeywordsCheckBox;
    QCheckBox* m_extractCommentsCheckBox;
    QCheckBox* m_extractStructureCheckBox;
    QCheckBox* m_extractSemanticsCheckBox;
    QCheckBox* m_extractMetricsCheckBox;
    QSpinBox* m_chunkSizeSpinBox;
    QSpinBox* m_overlapSpinBox;
    QSpinBox* m_minKeywordLengthSpinBox;
    
    // Model Configuration Group
    QGroupBox* m_modelConfigGroup;
    QGridLayout* m_modelConfigLayout;
    QLineEdit* m_modelNameEdit;
    QComboBox* m_modelSizeCombo;
    QComboBox* m_quantizationCombo;
    QLineEdit* m_outputDirectoryEdit;
    QPushButton* m_browseDirButton;
    QSpinBox* m_maxTokensSpin;
    QSpinBox* m_chunkSizeSpin;
    QSpinBox* m_overlapSizeSpin;
    
    // Extraction Configuration Group
    QGroupBox* m_extractionConfigGroup;
    QGridLayout* m_extractionConfigLayout;
    QComboBox* m_extractionModeCombo;
    QCheckBox* m_extractFunctionsCheck;
    QCheckBox* m_extractClassesCheck;
    QCheckBox* m_extractVariablesCheck;
    QCheckBox* m_extractCommentsCheck;
    QCheckBox* m_preserveStructureCheck;
    QSpinBox* m_minContentLengthSpin;
    
    // Training Hyperparameters Group
    QGroupBox* m_hyperparametersGroup;
    QGridLayout* m_hyperparametersLayout;
    QDoubleSpinBox* m_learningRateSpin;
    QSpinBox* m_epochsSpin;
    QSpinBox* m_batchSizeSpin;
    QDoubleSpinBox* m_weightDecaySpin;
    QDoubleSpinBox* m_warmupRatioSpin;
    QComboBox* m_schedulerCombo;
    QCheckBox* m_gradientCheckpointingCheck;
    QCheckBox* m_useFp16Check;
    
    // Model Presets Group
    QGroupBox* m_presetsGroup;
    QVBoxLayout* m_presetsLayout;
    ModelPresetWidget* m_presetWidget;
    QPushButton* m_codeExpertButton;
    QPushButton* m_asmExpertButton;
    QPushButton* m_securityExpertButton;
    QPushButton* m_generalPurposeButton;
    QPushButton* m_customPresetButton;
    
    // Training Tab
    QVBoxLayout* m_trainingLayout;
    QSplitter* m_trainingSplitter;
    
    // Digestion Controls
    QGroupBox* m_digestionControlsGroup;
    QHBoxLayout* m_digestionControlsLayout;
    QWidget* m_digestionControlsWidget;
    QPushButton* m_startDigestionButton;
    QPushButton* m_stopDigestionButton;
    QPushButton* m_pauseDigestionButton;
    QPushButton* m_resumeDigestionButton;
    
    // Training Controls
    QGroupBox* m_trainingControlsGroup;
    QHBoxLayout* m_trainingControlsLayout;
    QWidget* m_trainingControlsWidget;
    QPushButton* m_startTrainingButton;
    QPushButton* m_stopTrainingButton;
    QPushButton* m_pauseTrainingButton;
    QPushButton* m_resumeTrainingButton;
    
    // Progress Display
    QGroupBox* m_progressGroup;
    QVBoxLayout* m_progressLayout;
    QProgressBar* m_digestionProgress;
    QProgressBar* m_trainingProgress;
    QLabel* m_digestionStatusLabel;
    QLabel* m_trainingStatusLabel;
    QLabel* m_filesProcessedLabel;
    QLabel* m_trainingMetricsLabel;
    
    // Training Metrics Widget
    TrainingMetricsWidget* m_metricsWidget;
    
    // Models Tab
    QVBoxLayout* m_modelsLayout;
    ModelManagerWidget* m_modelManagerWidget;
    QTableWidget* m_modelsTable;
    QHBoxLayout* m_modelButtonsLayout;
    QPushButton* m_loadModelButton;
    QPushButton* m_testModelButton;
    QPushButton* m_quantizeModelButton;
    QPushButton* m_exportModelButton;
    QPushButton* m_deleteModelButton;
    QPushButton* m_refreshModelsButton;
    
    // Log Tab
    QVBoxLayout* m_logLayout;
    QTextEdit* m_logTextEdit;
    QHBoxLayout* m_logButtonsLayout;
    QPushButton* m_clearLogButton;
    QPushButton* m_saveLogButton;
    QCheckBox* m_autoScrollLogCheck;
    
    // Status tracking
    bool m_isDigesting;
    bool m_isTraining;
    double m_digestionProgressValue;
    double m_trainingProgressValue;
    QString m_currentModelPath;
    TrainingDataset m_lastDataset;
    
    // Settings
    QJsonObject m_currentSettings;
    QStringList m_inputFiles;
    QStringList m_inputDirectories;
    
    // Timers
    QTimer* m_progressUpdateTimer;
    QTimer* m_statusUpdateTimer;
};

// Custom file drop widget
class FileDropWidget : public QFrame {
    Q_OBJECT

public:
    explicit FileDropWidget(QWidget* parent = nullptr);

protected:
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dragMoveEvent(QDragMoveEvent* event) override;
    void dropEvent(QDropEvent* event) override;
    void paintEvent(QPaintEvent* event) override;

signals:
    void filesDropped(const QStringList& files);

private:
    bool m_dragActive;
};

// Parameter widget for advanced settings
class ParameterWidget : public QWidget {
    Q_OBJECT

public:
    explicit ParameterWidget(const QString& title, QWidget* parent = nullptr);
    
    void addParameter(const QString& name, QWidget* widget, const QString& description = QString());
    void setCollapsed(bool collapsed);
    bool isCollapsed() const;

signals:
    void toggled(bool expanded);

private slots:
    void onHeaderClicked();

private:
    void setupUI();
    void updateToggleButton();

private:
    QString m_title;
    bool m_collapsed;
    
    QVBoxLayout* m_layout;
    QHBoxLayout* m_headerLayout;
    QWidget* m_headerWidget;
    QWidget* m_contentWidget;
    QGridLayout* m_contentLayout;
    
    QPushButton* m_toggleButton;
    QLabel* m_titleLabel;
    
    int m_parameterCount;
};

// Model preset widget
class ModelPresetWidget : public QWidget {
    Q_OBJECT

public:
    explicit ModelPresetWidget(QWidget* parent = nullptr);
    
    void addPreset(const QString& name, const QString& description, const QJsonObject& config);
    QJsonObject getPresetConfig(const QString& name) const;
    QStringList getPresetNames() const;

signals:
    void presetSelected(const QString& name, const QJsonObject& config);

private slots:
    void onPresetButtonClicked();

private:
    void setupUI();
    void createPresetButton(const QString& name, const QString& description);

private:
    QVBoxLayout* m_layout;
    QHash<QString, QJsonObject> m_presets;
    QHash<QString, QPushButton*> m_presetButtons;
};

// Training metrics display widget
class TrainingMetricsWidget : public QWidget {
    Q_OBJECT

public:
    explicit TrainingMetricsWidget(QWidget* parent = nullptr);
    
    void updateMetrics(const QJsonObject& metrics);
    void reset();

private:
    void setupUI();
    void createMetricDisplay(const QString& name, const QString& format = "%.4f");

private:
    QGridLayout* m_layout;
    QHash<QString, QLabel*> m_metricLabels;
    QHash<QString, QString> m_metricFormats;
};

// Model manager widget
class ModelManagerWidget : public QWidget {
    Q_OBJECT

public:
    explicit ModelManagerWidget(QWidget* parent = nullptr);
    
    void refreshModels();
    void addModel(const QString& name, const QString& path, qint64 size, const QString& type);
    void removeModel(const QString& name);
    QString getSelectedModelPath() const;

signals:
    void modelSelected(const QString& name, const QString& path);
    void modelDoubleClicked(const QString& name, const QString& path);

private slots:
    void onModelSelectionChanged();
    void onModelDoubleClicked();

private:
    void setupUI();
    void loadModelsFromDirectory(const QString& directory);
    QString formatFileSize(qint64 size) const;

private:
    QVBoxLayout* m_layout;
    QTableWidget* m_modelsTable;
    QString m_modelsDirectory;
};

#endif // AI_DIGESTION_PANEL_HPP
