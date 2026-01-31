#pragma once


#include "../utils/InferenceSettingsManager.h"

class InferenceSettingsDialog : public void
{

public:
    explicit InferenceSettingsDialog(void* parent = nullptr);
    ~InferenceSettingsDialog();

private:
    void onPresetChanged(int index);
    void onTemperatureChanged(double value);
    void onTopPChanged(double value);
    void onTopKChanged(int value);
    void onMaxTokensChanged(int value);
    void onRepetitionPenaltyChanged(double value);
    void onUseOllamaChanged(int state);
    void onOllamaModelChanged(const std::string& text);
    void onRecentModelSelected(QListWidgetItem* item);
    void onClearRecentModels();
    void onApplyClicked();
    void onOkClicked();
    void onCancelClicked();


    void modelSelected(const std::string& modelPath);

private:
    void setupUI();
    void loadSettings();
    void updatePresetIndicator();
    void updateCustomSettings();
    
    InferenceSettingsManager& m_settings;
    
    // UI elements
    void* m_presetCombo;
    QDoubleSpinBox* m_temperatureSpin;
    QDoubleSpinBox* m_topPSpin;
    void* m_topKSpin;
    void* m_maxTokensSpin;
    QDoubleSpinBox* m_repetitionPenaltySpin;
    void* m_useOllamaCheck;
    void* m_ollamaModelEdit;
    QListWidget* m_recentModelsList;
    void* m_clearRecentButton;
    void* m_presetIndicator;
    QDialogButtonBox* m_buttonBox;
};

