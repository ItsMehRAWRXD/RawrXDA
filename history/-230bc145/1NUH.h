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

private:
    void setupUI();
    QWidget* createGeneralTab();
    QWidget* createModelTab();
    QWidget* createSecurityTab();
    QWidget* createTrainingTab();
    QWidget* createAIChatTab();

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
    
    // AI Chat Tab
    QCheckBox *m_enableCloudAI = nullptr;
    QCheckBox *m_enableLocalAI = nullptr;
    QLineEdit *m_cloudEndpoint = nullptr;
    QLineEdit *m_localEndpoint = nullptr;
    QLineEdit *m_apiKey = nullptr;
    QSpinBox *m_requestTimeout = nullptr;
    
    // Model Tab
    QCheckBox *m_enableGPU = nullptr;
    QComboBox *m_gpuBackend = nullptr;
    QSpinBox *m_maxTokens = nullptr;
    QDoubleSpinBox *m_temperature = nullptr;
};