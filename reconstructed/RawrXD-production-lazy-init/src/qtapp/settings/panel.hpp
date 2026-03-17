/**
 * @file settings_panel.hpp
 * @brief Settings and preferences panel with secure credential management
 *
 * Provides configuration UI for:
 * - LLM backend selection and API key management
 * - GGUF model settings (quantization, context size)
 * - Hotpatch configuration
 * - Build tool settings (CMake, MSBuild paths)
 *
 * Uses OS keychain for secure API key storage (Windows DPAPI, macOS Keychain, Linux Secret Service).
 */

#pragma once

#include <QDialog>
#include <QString>
#include <QMap>
#include <QJsonObject>

class QTabWidget;
class QLineEdit;
class QComboBox;
class QPushButton;
class QCheckBox;
class QSpinBox;
class QDoubleSpinBox;
class QLabel;

/**
 * @brief Secure credential management using OS keychain
 *
 * Stores API keys and sensitive data in platform-specific secure storage:
 * - Windows: DPAPI (Data Protection API)
 * - macOS: Keychain
 * - Linux: Secret Service or pass
 */
class KeychainHelper {
public:
    /**
     * @brief Store credential in secure storage
     * @param service Service name (e.g., "RawrXD")
     * @param account Account name (e.g., "ollama_api_key")
     * @param password Credential value
     * @return true if stored successfully
     */
    static bool storeCredential(const QString& service, const QString& account,
                               const QString& password);

    /**
     * @brief Retrieve credential from secure storage
     * @param service Service name
     * @param account Account name
     * @return Credential value, or empty string if not found
     */
    static QString retrieveCredential(const QString& service, const QString& account);

    /**
     * @brief Delete credential from secure storage
     * @param service Service name
     * @param account Account name
     * @return true if deleted successfully
     */
    static bool deleteCredential(const QString& service, const QString& account);

    /**
     * @brief Check if credential exists
     * @param service Service name
     * @param account Account name
     * @return true if credential exists
     */
    static bool credentialExists(const QString& service, const QString& account);

private:
#ifdef Q_OS_WIN
    static bool storeCredentialWindows(const QString& service, const QString& account,
                                       const QString& password);
    static QString retrieveCredentialWindows(const QString& service, const QString& account);
    static bool deleteCredentialWindows(const QString& service, const QString& account);
#endif
};

/**
 * @brief LLM backend configuration
 */
struct LLMConfig {
    QString backend;        // "ollama", "claude", "openai"
    QString endpoint;       // Service URL
    QString apiKey;         // Stored in keychain
    QString model;          // Model name
    int maxTokens = 2048;
    float temperature = 0.7f;
    bool cacheEnabled = true;

    QJsonObject toJSON() const;
    static LLMConfig fromJSON(const QJsonObject& obj);
};

/**
 * @brief GGUF inference configuration
 */
struct GGUFConfig {
    QString modelPath;
    QString quantizationMode;  // "Q4_0", "Q4_1", "Q5_0", etc.
    int contextSize = 2048;
    int gpuLayers = 0;         // 0 = CPU only
    int threads = 0;           // 0 = auto-detect
    bool offloadEmbeddings = true;
    bool useMemoryMapping = true;

    QJsonObject toJSON() const;
    static GGUFConfig fromJSON(const QJsonObject& obj);
};

/**
 * @brief Build tool configuration
 */
struct BuildConfig {
    QString cmakePath;      // Full path to cmake.exe
    QString msbuildPath;    // Full path to msbuild.exe
    QString masmPath;       // Full path to ml64.exe or ml.exe
    int buildThreads = 0;   // 0 = auto
    bool parallelBuild = true;
    bool incremental = true;

    QJsonObject toJSON() const;
    static BuildConfig fromJSON(const QJsonObject& obj);
};

/**
 * @brief Main settings dialog
 */
class SettingsPanel : public QDialog {
    Q_OBJECT

public:
    /**
     * @brief Constructor
     * @param parent Parent widget
     */
    explicit SettingsPanel(QWidget* parent = nullptr);

    /**
     * @brief Get current LLM configuration
     */
    LLMConfig getLLMConfig() const;

    /**
     * @brief Set LLM configuration
     */
    void setLLMConfig(const LLMConfig& config);

    /**
     * @brief Get current GGUF configuration
     */
    GGUFConfig getGGUFConfig() const;

    /**
     * @brief Set GGUF configuration
     */
    void setGGUFConfig(const GGUFConfig& config);

    /**
     * @brief Get current build configuration
     */
    BuildConfig getBuildConfig() const;

    /**
     * @brief Set build configuration
     */
    void setBuildConfig(const BuildConfig& config);

    /**
     * @brief Save all settings to persistent storage
     */
    void saveSettings();

    /**
     * @brief Load all settings from persistent storage
     */
    void loadSettings();

    /**
     * @brief Export settings to JSON file
     * @param filePath Export file path
     * @return true if export successful
     */
    bool exportSettings(const QString& filePath);

    /**
     * @brief Import settings from JSON file
     * @param filePath Import file path
     * @return true if import successful
     */
    bool importSettings(const QString& filePath);

signals:
    /**
     * @brief Emitted when settings changed and applied
     */
    void settingsChanged();

    /**
     * @brief Emitted when LLM backend changed
     * @param backend Backend name
     * @param endpoint Service endpoint
     */
    void llmBackendChanged(const QString& backend, const QString& endpoint);

private slots:
    void onLLMBackendChanged(int index);
    void onTestLLMConnection();
    void onTestGGUFConnection();
    void onBrowseCMakePath();
    void onBrowseMSBuildPath();
    void onBrowseMASMPath();
    void onBrowseGGUFModel();
    void onApplySettings();
    void onResetToDefaults();
    void onExportSettings();
    void onImportSettings();

private:
    void setupUI();
    void setupLLMTab();
    void setupGGUFTab();
    void setupBuildTab();
    void setupHotpatchTab();
    void updateUIFromConfig();

    // LLM Tab widgets
    QComboBox* m_llmBackendCombo{};
    QLineEdit* m_llmEndpointEdit{};
    QLineEdit* m_llmApiKeyEdit{};
    QLineEdit* m_llmModelEdit{};
    QSpinBox* m_llmMaxTokensSpinBox{};
    QDoubleSpinBox* m_llmTemperatureSpinBox{};
    QCheckBox* m_llmCacheCheckBox{};
    QPushButton* m_testLLMButton{};
    QLabel* m_llmStatusLabel{};

    // GGUF Tab widgets
    QLineEdit* m_ggufModelPathEdit{};
    QComboBox* m_ggufQuantCombo{};
    QSpinBox* m_ggufContextSpinBox{};
    QSpinBox* m_ggufGpuLayersSpinBox{};
    QCheckBox* m_ggufOffloadEmbCheckBox{};
    QCheckBox* m_ggufMemMapCheckBox{};
    QPushButton* m_testGGUFButton{};
    QLabel* m_ggufStatusLabel{};

    // Build Tab widgets
    QLineEdit* m_cmakePathEdit{};
    QLineEdit* m_msbuildPathEdit{};
    QLineEdit* m_masmPathEdit{};
    QSpinBox* m_buildThreadsSpinBox{};
    QCheckBox* m_parallelBuildCheckBox{};
    QCheckBox* m_incrementalCheckBox{};

    // Hotpatch Tab widgets
    QCheckBox* m_hotpatchEnabledCheckBox{};
    QSpinBox* m_hotpatchTimeoutSpinBox{};
    QCheckBox* m_hotpatchVerboseCheckBox{};

    // Dialog buttons
    QPushButton* m_applyButton{};
    QPushButton* m_resetButton{};
    QPushButton* m_exportButton{};
    QPushButton* m_importButton{};
    QPushButton* m_okButton{};
    QPushButton* m_cancelButton{};

    QTabWidget* m_tabWidget{};

    // Current configuration
    LLMConfig m_llmConfig;
    GGUFConfig m_ggufConfig;
    BuildConfig m_buildConfig;
};
