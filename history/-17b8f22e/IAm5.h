#pragma once

#include <QDialog>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QCheckBox>
#include <QComboBox>
#include <QPushButton>

class AutonomousFeatureEngine;
class AutonomousModelManager;

/**
 * @brief Settings Dialog for Autonomous Features
 * 
 * Provides UI controls for:
 * - Analysis interval configuration
 * - Confidence threshold adjustment
 * - Feature toggles
 * - Model memory settings
 * - Streaming configuration
 */
class AutonomousSettingsDialog : public QDialog {
    Q_OBJECT

public:
    explicit AutonomousSettingsDialog(QWidget* parent = nullptr);
    ~AutonomousSettingsDialog();
    
    // Load/save from engines
    void loadSettings(AutonomousFeatureEngine* featureEngine, AutonomousModelManager* modelManager);
    void applySettings(AutonomousFeatureEngine* featureEngine, AutonomousModelManager* modelManager);

private slots:
    void onApplyClicked();
    void onOkClicked();
    void onCancelClicked();
    void onRestoreDefaultsClicked();
    void onAnalysisIntervalChanged(int value);
    void onConfidenceThresholdChanged(double value);

private:
    void setupUI();
    void setupConnections();
    
    // Analysis Section
    QSpinBox* m_analysisIntervalSpinBox;
    QDoubleSpinBox* m_confidenceThresholdSpinBox;
    QCheckBox* m_enableRealtimeCheckbox;
    QCheckBox* m_enableAutoSuggestionsCheckbox;
    QSpinBox* m_maxConcurrentAnalysesSpinBox;
    
    // Feature Toggles
    QCheckBox* m_enableTestGenCheckbox;
    QCheckBox* m_enableSecurityCheckbox;
    QCheckBox* m_enableOptimizationCheckbox;
    QCheckBox* m_enableDocumentationCheckbox;
    
    // Memory Settings
    QSpinBox* m_maxMemorySpinBox;
    QSpinBox* m_prefetchSizeSpinBox;
    QCheckBox* m_enableStreamingCheckbox;
    QComboBox* m_prefetchStrategyCombo;
    
    // Model Settings
    QSpinBox* m_maxModelSizeSpinBox;
    QCheckBox* m_autoDownloadCheckbox;
    QCheckBox* m_autoOptimizeCheckbox;
    
    // Buttons
    QPushButton* m_applyButton;
    QPushButton* m_okButton;
    QPushButton* m_cancelButton;
    QPushButton* m_restoreDefaultsButton;
};
