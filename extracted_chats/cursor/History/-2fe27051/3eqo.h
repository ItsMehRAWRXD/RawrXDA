#pragma once

#include <QDialog>
#include "settings_manager.h"

class QVBoxLayout;
class QHBoxLayout;
class QTabWidget;
class QGroupBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QCheckBox;
class QSpinBox;
class QDoubleSpinBox;
class QComboBox;
class QTextEdit;
class QListWidget;
class QSlider;

class SettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SettingsDialog(QWidget *parent = nullptr);
    void initialize();

private slots:
    void saveSettings();
    void applySettings();
    void loadSettings();
    void manageEncryptionKeys();
    void configureTokenizer();
    void configureCIPipeline();
    void applyVisualSettings();
    void resetVisualSettings();

private:
    void setupUI();
    QWidget* createGeneralTab();
    QWidget* createVisualTab();
    QWidget* createModelTab();
    QWidget* createSecurityTab();
    QWidget* createTrainingTab();
    QWidget* createCICDTab();
    void refreshMemoryList();
    void deleteSelectedMemory();

    SettingsManager *m_settings = nullptr;
    
    // General Tab
    QCheckBox *m_autoSaveCheck = nullptr;
    QSpinBox *m_autoSaveInterval = nullptr;
    QCheckBox *m_showLineNumbers = nullptr;
    QCheckBox *m_wordWrap = nullptr;
    QLineEdit *m_defaultModelPath = nullptr;
    
    // Security Tab
    QCheckBox *m_encryptApiKeys = nullptr;
    QCheckBox *m_enableAuditLog = nullptr;
    QSpinBox *m_autoLockTimeout = nullptr;
    
    // Training Tab
    QCheckBox *m_autoCheckpoint = nullptr;
    QSpinBox *m_checkpointInterval = nullptr;
    QLineEdit *m_checkpointPath = nullptr;
    QComboBox *m_defaultTokenizer = nullptr;
    
    // CI/CD Tab
    QCheckBox *m_enableCICD = nullptr;
    QCheckBox *m_autoDeploy = nullptr;
    QLineEdit *m_notificationEmail = nullptr;
    
    // Model Tab
    QCheckBox *m_enableGPU = nullptr;
    QComboBox *m_gpuBackend = nullptr;
    QSpinBox *m_maxTokens = nullptr;
    QDoubleSpinBox *m_temperature = nullptr;

    // Visual Tab
    QSlider *m_transparencySlider = nullptr;
    QSlider *m_brightnessSlider = nullptr;
    QSlider *m_contrastSlider = nullptr;
    QSlider *m_hueRotationSlider = nullptr;
    QLabel *m_transparencyValue = nullptr;
    QLabel *m_brightnessValue = nullptr;
    QLabel *m_contrastValue = nullptr;
    QLabel *m_hueRotationValue = nullptr;
    QPushButton *m_applyVisualBtn = nullptr;

    // Memory Tab
    QCheckBox *m_enableMemorySpace = nullptr;
    QDoubleSpinBox *m_memoryLimitValue = nullptr;
    QComboBox *m_memoryLimitUnit = nullptr;
    QLabel *m_memoryUsageLabel = nullptr;
    QListWidget *m_memoryList = nullptr;
    QPushButton *m_deleteMemoryBtn = nullptr;

    // Training Tab
    QCheckBox *m_enableTraining = nullptr;
    QLineEdit *m_trainingDataPath = nullptr;
    QSpinBox *m_epochs = nullptr;
    QDoubleSpinBox *m_learningRate = nullptr;
};
