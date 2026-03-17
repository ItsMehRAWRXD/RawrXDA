/**
 * @file settings_dialog_autosave.cpp
 * @brief Auto-save functionality for SettingsDialog
 *
 * Implements real-time auto-save for all settings changes
 * using a 2-second debounce timer to batch changes.
 */

#include "settings_dialog.h"
#include <QDebug>
#include <QTimer>

/**
 * @brief Connect all widget changes to auto-save handler
 *
 * This method connects every settings widget to the onSettingChanged() slot
 * which triggers the auto-save timer.
 */
void SettingsDialog::connectAutoSaveSignals()
{
    // Connect all editor settings
    if (m_autoSaveCheck) connect(m_autoSaveCheck, &QCheckBox::toggled, this, &SettingsDialog::onSettingChanged);
    if (m_autoSaveInterval) connect(m_autoSaveInterval, QOverload<int>::of(&QSpinBox::valueChanged), this, &SettingsDialog::onSettingChanged);
    if (m_showLineNumbers) connect(m_showLineNumbers, &QCheckBox::toggled, this, &SettingsDialog::onSettingChanged);
    if (m_wordWrap) connect(m_wordWrap, &QCheckBox::toggled, this, &SettingsDialog::onSettingChanged);
    if (m_defaultModelPath) connect(m_defaultModelPath, &QLineEdit::textChanged, this, &SettingsDialog::onSettingChanged);
    
    // Connect security settings
    if (m_encryptApiKeys) connect(m_encryptApiKeys, &QCheckBox::toggled, this, &SettingsDialog::onSettingChanged);
    if (m_enableAuditLog) connect(m_enableAuditLog, &QCheckBox::toggled, this, &SettingsDialog::onSettingChanged);
    if (m_autoLockTimeout) connect(m_autoLockTimeout, QOverload<int>::of(&QSpinBox::valueChanged), this, &SettingsDialog::onSettingChanged);
    
    // Connect training settings
    if (m_autoCheckpoint) connect(m_autoCheckpoint, &QCheckBox::toggled, this, &SettingsDialog::onSettingChanged);
    if (m_checkpointInterval) connect(m_checkpointInterval, QOverload<int>::of(&QSpinBox::valueChanged), this, &SettingsDialog::onSettingChanged);
    if (m_checkpointPath) connect(m_checkpointPath, &QLineEdit::textChanged, this, &SettingsDialog::onSettingChanged);
    if (m_defaultTokenizer) connect(m_defaultTokenizer, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &SettingsDialog::onSettingChanged);
    
    // Connect CI/CD settings
    if (m_enableCICD) connect(m_enableCICD, &QCheckBox::toggled, this, &SettingsDialog::onSettingChanged);
    if (m_autoDeploy) connect(m_autoDeploy, &QCheckBox::toggled, this, &SettingsDialog::onSettingChanged);
    if (m_notificationEmail) connect(m_notificationEmail, &QLineEdit::textChanged, this, &SettingsDialog::onSettingChanged);
    
    // Connect GPU/inference settings
    if (m_enableGPU) connect(m_enableGPU, &QCheckBox::toggled, this, &SettingsDialog::onSettingChanged);
    if (m_gpuBackend) connect(m_gpuBackend, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &SettingsDialog::onSettingChanged);
    if (m_maxTokens) connect(m_maxTokens, QOverload<int>::of(&QSpinBox::valueChanged), this, &SettingsDialog::onSettingChanged);
    if (m_temperature) connect(m_temperature, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &SettingsDialog::onSettingChanged);
    
    // Connect AI Chat settings
    if (m_enableCloudAI) connect(m_enableCloudAI, &QCheckBox::toggled, this, &SettingsDialog::onSettingChanged);
    if (m_enableLocalAI) connect(m_enableLocalAI, &QCheckBox::toggled, this, &SettingsDialog::onSettingChanged);
    if (m_cloudEndpoint) connect(m_cloudEndpoint, &QLineEdit::textChanged, this, &SettingsDialog::onSettingChanged);
    if (m_localEndpoint) connect(m_localEndpoint, &QLineEdit::textChanged, this, &SettingsDialog::onSettingChanged);
    if (m_apiKey) connect(m_apiKey, &QLineEdit::textChanged, this, &SettingsDialog::onSettingChanged);
    if (m_requestTimeout) connect(m_requestTimeout, QOverload<int>::of(&QSpinBox::valueChanged), this, &SettingsDialog::onSettingChanged);
    
    // Connect enterprise settings
    if (m_enterpriseLicenseKey) connect(m_enterpriseLicenseKey, &QLineEdit::textChanged, this, &SettingsDialog::onSettingChanged);
    if (m_enableCovertTelemetry) connect(m_enableCovertTelemetry, &QCheckBox::toggled, this, &SettingsDialog::onSettingChanged);
    if (m_telemetryInterval) connect(m_telemetryInterval, QOverload<int>::of(&QSpinBox::valueChanged), this, &SettingsDialog::onSettingChanged);
    if (m_enableShadowContext) connect(m_enableShadowContext, &QCheckBox::toggled, this, &SettingsDialog::onSettingChanged);
    if (m_shadowContextSize) connect(m_shadowContextSize, QOverload<int>::of(&QSpinBox::valueChanged), this, &SettingsDialog::onSettingChanged);
    if (m_enableLicenseKillSwitch) connect(m_enableLicenseKillSwitch, &QCheckBox::toggled, this, &SettingsDialog::onSettingChanged);
    if (m_enableCovertUpdates) connect(m_enableCovertUpdates, &QCheckBox::toggled, this, &SettingsDialog::onSettingChanged);
    if (m_enableHiddenAdminConsole) connect(m_enableHiddenAdminConsole, &QCheckBox::toggled, this, &SettingsDialog::onSettingChanged);
    if (m_enableCryptoFingerprinting) connect(m_enableCryptoFingerprinting, &QCheckBox::toggled, this, &SettingsDialog::onSettingChanged);
    if (m_enableGpuSidebandLeak) connect(m_enableGpuSidebandLeak, &QCheckBox::toggled, this, &SettingsDialog::onSettingChanged);
    if (m_enableGgufWatermark) connect(m_enableGgufWatermark, &QCheckBox::toggled, this, &SettingsDialog::onSettingChanged);
    if (m_enableEmergencyBrickMode) connect(m_enableEmergencyBrickMode, &QCheckBox::toggled, this, &SettingsDialog::onSettingChanged);
    if (m_enableDnsTunnel) connect(m_enableDnsTunnel, &QCheckBox::toggled, this, &SettingsDialog::onSettingChanged);
    
    qDebug() << "[SettingsDialog] Auto-save signals connected";
}

/**
 * @brief Called when any setting changes
 *
 * Marks changes as pending and starts the debounce timer.
 */
void SettingsDialog::onSettingChanged()
{
    // Mark as having unsaved changes
    m_hasUnsavedChanges = true;
    
    // Start the auto-save timer if not already running
    if (m_autoSaveTimer && !m_autoSaveTimer->isActive()) {
        m_autoSaveTimer->start();
        qDebug() << "[SettingsDialog] Auto-save timer started";
    }
}

/**
 * @brief Timer timeout - auto-save batched changes
 *
 * Applies all pending setting changes and stops the timer.
 */
void SettingsDialog::onAutoSaveTimerTimeout()
{
    // Auto-save if changes exist
    if (m_hasUnsavedChanges) {
        applySettings();
        m_hasUnsavedChanges = false;
        
        // Stop timer until next change
        if (m_autoSaveTimer) {
            m_autoSaveTimer->stop();
        }
        
        qDebug() << "[SettingsDialog] Auto-saved all pending settings";
    }
}
