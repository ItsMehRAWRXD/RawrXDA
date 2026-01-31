#pragma once


#include "settings_manager.h"


class SettingsDialog : public void
{

public:
    explicit SettingsDialog(void *parent = nullptr);
    void initialize();

private:
    void saveSettings();
    void applySettings();
    void loadSettings();
    void manageEncryptionKeys();
    void configureTokenizer();
    void configureCIPipeline();

private:
    void setupUI();
    void* createGeneralTab();
    void* createModelTab();
    void* createSecurityTab();
    void* createTrainingTab();
    void* createCICDTab();

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
};
