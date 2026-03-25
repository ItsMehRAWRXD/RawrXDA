#pragma once

<<<<<<< HEAD
#include "Win32UIStubs.hpp"

=======

// Forward declarations


>>>>>>> origin/main
class ModelTrainer;

/**
 * @brief Training configuration dialog for ModelTrainer
 *
 * Win32 + STL only. Dataset/model/output paths, hyperparameters, validation options.
 * File pickers use RawrXD::getOpenFileName / getSaveFileName (win32_file_dialog.h).
 */
<<<<<<< HEAD
class TrainingDialog : public DialogBase
{
  public:
=======
class TrainingDialog : public void
{

public:
    /**
     * @brief Construct a new Training Dialog
     * @param trainer Pointer to ModelTrainer instance (non-owning)
     * @param parent Parent widget for Qt ownership
     */
>>>>>>> origin/main
    explicit TrainingDialog(ModelTrainer* trainer, void* parent = nullptr);
    ~TrainingDialog() override = default;

    void initialize();
<<<<<<< HEAD
    void* getTrainingConfig() const;
    void trainingStartRequested(const void*& config);
    void trainingCancelled();

  private:
=======

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
>>>>>>> origin/main
    void onBrowseDataset();
    void onBrowseModel();
    void onBrowseOutputPath();
    void onStartTraining();
    void onCancel();
    void onDatasetFormatChanged(int index);
    void validateInputs();
    void setupUI();
    void setupConnections();
    void loadDefaultSettings();
    void saveSettings();
    bool validateConfiguration() const;
    std::string detectDatasetFormat(const std::string& path) const;

<<<<<<< HEAD
    LineEdit* m_datasetPathEdit;
    PushButton* m_browseDatasetBtn;
    ComboBox* m_datasetFormatCombo;
    Label* m_datasetInfoLabel;
    LineEdit* m_modelPathEdit;
    PushButton* m_browseModelBtn;
    Label* m_modelInfoLabel;
    LineEdit* m_outputPathEdit;
    PushButton* m_browseOutputBtn;
    SpinBox* m_epochsSpinBox;
    DoubleSpinBox* m_learningRateSpinBox;
    SpinBox* m_batchSizeSpinBox;
    SpinBox* m_sequenceLengthSpinBox;
    DoubleSpinBox* m_gradientClipSpinBox;
    DoubleSpinBox* m_weightDecaySpinBox;
    DoubleSpinBox* m_warmupStepsSpinBox;
    DoubleSpinBox* m_validationSplitSpinBox;
    CheckBox* m_validateEveryEpochCheckBox;
    PushButton* m_startTrainingBtn;
    PushButton* m_cancelBtn;
=======
    // UI Components - Dataset Selection
    void* m_datasetPathEdit;
    void* m_browseDatasetBtn;
    void* m_datasetFormatCombo;
    void* m_datasetInfoLabel;

    // UI Components - Model Selection
    void* m_modelPathEdit;
    void* m_browseModelBtn;
    void* m_modelInfoLabel;

    // UI Components - Output Configuration
    void* m_outputPathEdit;
    void* m_browseOutputBtn;

    // UI Components - Hyperparameters
    void* m_epochsSpinBox;
    QDoubleSpinBox* m_learningRateSpinBox;
    void* m_batchSizeSpinBox;
    void* m_sequenceLengthSpinBox;
    QDoubleSpinBox* m_gradientClipSpinBox;
    QDoubleSpinBox* m_weightDecaySpinBox;
    QDoubleSpinBox* m_warmupStepsSpinBox;

    // UI Components - Validation Options
    QDoubleSpinBox* m_validationSplitSpinBox;
    void* m_validateEveryEpochCheckBox;

    // UI Components - Actions
    void* m_startTrainingBtn;
    void* m_cancelBtn;

    // Reference to ModelTrainer (non-owning)
>>>>>>> origin/main
    ModelTrainer* m_trainer;
};

