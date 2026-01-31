#pragma once


// Forward declarations


class ModelTrainer;

/**
 * @brief Training configuration dialog for ModelTrainer
 * 
 * Provides a comprehensive UI for configuring model training parameters including:
 * - Dataset selection and format detection
 * - Model selection and validation
 * - Hyperparameter configuration (epochs, learning rate, batch size, etc.)
 * - Training options (validation split, gradient clipping, etc.)
 * - Output path configuration
 */
class TrainingDialog : public void
{

public:
    /**
     * @brief Construct a new Training Dialog
     * @param trainer Pointer to ModelTrainer instance (non-owning)
     * @param parent Parent widget for Qt ownership
     */
    explicit TrainingDialog(ModelTrainer* trainer, void* parent = nullptr);
    ~TrainingDialog() override = default;
    
    void initialize();

    /**
     * @brief Get the configured training parameters
     * @return void* containing all training configuration
     */
    void* getTrainingConfig() const;


    /**
     * @brief Emitted when user starts training with valid configuration
     * @param config Complete training configuration as JSON
     */
    void trainingStartRequested(const void*& config);

    /**
     * @brief Emitted when dialog is cancelled
     */
    void trainingCancelled();

private:
    void onBrowseDataset();
    void onBrowseModel();
    void onBrowseOutputPath();
    void onStartTraining();
    void onCancel();
    void onDatasetFormatChanged(int index);
    void validateInputs();

private:
    void setupUI();
    void setupConnections();
    void loadDefaultSettings();
    void saveSettings();
    bool validateConfiguration() const;
    std::string detectDatasetFormat(const std::string& path) const;

    // UI Components - Dataset Selection
    QLineEdit* m_datasetPathEdit;
    QPushButton* m_browseDatasetBtn;
    QComboBox* m_datasetFormatCombo;
    QLabel* m_datasetInfoLabel;

    // UI Components - Model Selection
    QLineEdit* m_modelPathEdit;
    QPushButton* m_browseModelBtn;
    QLabel* m_modelInfoLabel;

    // UI Components - Output Configuration
    QLineEdit* m_outputPathEdit;
    QPushButton* m_browseOutputBtn;

    // UI Components - Hyperparameters
    QSpinBox* m_epochsSpinBox;
    QDoubleSpinBox* m_learningRateSpinBox;
    QSpinBox* m_batchSizeSpinBox;
    QSpinBox* m_sequenceLengthSpinBox;
    QDoubleSpinBox* m_gradientClipSpinBox;
    QDoubleSpinBox* m_weightDecaySpinBox;
    QDoubleSpinBox* m_warmupStepsSpinBox;

    // UI Components - Validation Options
    QDoubleSpinBox* m_validationSplitSpinBox;
    QCheckBox* m_validateEveryEpochCheckBox;

    // UI Components - Actions
    QPushButton* m_startTrainingBtn;
    QPushButton* m_cancelBtn;

    // Reference to ModelTrainer (non-owning)
    ModelTrainer* m_trainer;
};

