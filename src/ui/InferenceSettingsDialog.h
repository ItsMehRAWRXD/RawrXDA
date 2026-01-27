#pragma once

#include <QDialog>
#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QCheckBox>
#include <QLineEdit>
#include <QPushButton>
#include <QGroupBox>
#include <QListWidget>
#include <QDialogButtonBox>
#include "../utils/InferenceSettingsManager.h"

class InferenceSettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit InferenceSettingsDialog(QWidget* parent = nullptr);
    ~InferenceSettingsDialog();

private slots:
    void onPresetChanged(int index);
    void onTemperatureChanged(double value);
    void onTopPChanged(double value);
    void onTopKChanged(int value);
    void onMaxTokensChanged(int value);
    void onRepetitionPenaltyChanged(double value);
    void onUseOllamaChanged(int state);
    void onOllamaModelChanged(const QString& text);
    void onRecentModelSelected(QListWidgetItem* item);
    void onClearRecentModels();
    void onApplyClicked();
    void onOkClicked();
    void onCancelClicked();

signals:
    void modelSelected(const QString& modelPath);

private:
    void setupUI();
    void loadSettings();
    void updatePresetIndicator();
    void updateCustomSettings();
    
    InferenceSettingsManager& m_settings;
    
    // UI elements
    QComboBox* m_presetCombo;
    QDoubleSpinBox* m_temperatureSpin;
    QDoubleSpinBox* m_topPSpin;
    QSpinBox* m_topKSpin;
    QSpinBox* m_maxTokensSpin;
    QDoubleSpinBox* m_repetitionPenaltySpin;
    QCheckBox* m_useOllamaCheck;
    QLineEdit* m_ollamaModelEdit;
    QListWidget* m_recentModelsList;
    QPushButton* m_clearRecentButton;
    QLabel* m_presetIndicator;
    QDialogButtonBox* m_buttonBox;
};