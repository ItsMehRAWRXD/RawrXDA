#pragma once

#include "Win32UIStubs.hpp"

class ModelTrainer;

/**
 * @brief Training configuration dialog for ModelTrainer
 *
 * Win32 + STL only. Dataset/model/output paths, hyperparameters, validation options.
 * File pickers use RawrXD::getOpenFileName / getSaveFileName (win32_file_dialog.h).
 */
class TrainingDialog : public DialogBase
{
  public:
    explicit TrainingDialog(ModelTrainer* trainer, void* parent = nullptr);
    ~TrainingDialog() override = default;

    void initialize();
    void* getTrainingConfig() const;
    void trainingStartRequested(const void*& config);
    void trainingCancelled();

  private:
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
    ModelTrainer* m_trainer;
};

