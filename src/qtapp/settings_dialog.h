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
    void *m_autoSaveCheck = nullptr;
    void *m_autoSaveInterval = nullptr;
    void *m_showLineNumbers = nullptr;
    void *m_wordWrap = nullptr;
    void *m_defaultModelPath = nullptr;
    
    // Security Tab
    void *m_encryptApiKeys = nullptr;
    void *m_enableAuditLog = nullptr;
    void *m_autoLockTimeout = nullptr;
    
    // Training Tab
    void *m_autoCheckpoint = nullptr;
    void *m_checkpointInterval = nullptr;
    void *m_checkpointPath = nullptr;
    void *m_defaultTokenizer = nullptr;
    
    // CI/CD Tab
    void *m_enableCICD = nullptr;
    void *m_autoDeploy = nullptr;
    void *m_notificationEmail = nullptr;
    
    // Model Tab
    void *m_enableGPU = nullptr;
    void *m_gpuBackend = nullptr;
    void *m_maxTokens = nullptr;
    QDoubleSpinBox *m_temperature = nullptr;
};
