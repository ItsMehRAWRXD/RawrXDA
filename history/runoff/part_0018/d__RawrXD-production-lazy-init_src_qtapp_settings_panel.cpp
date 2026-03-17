/**
 * @file settings_panel.cpp
 * @brief Implementation of settings panel with keychain integration
 */

#include "settings_panel.hpp"
#include <QTabWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QLineEdit>
#include <QComboBox>
#include <QPushButton>
#include <QCheckBox>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QLabel>
#include <QGroupBox>
#include <QSettings>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFile>
#include <QFileInfo>
#include <QFileDialog>
#include <QMessageBox>
#include <QDebug>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QTimer>
#include <QUrl>

#ifdef Q_OS_WIN
#include <windows.h>
#include <wincred.h>
#pragma comment(lib, "Advapi32.lib")
#endif

// ─────────────────────────────────────────────────────────────────────
// KeychainHelper Implementation
// ─────────────────────────────────────────────────────────────────────

bool KeychainHelper::storeCredential(const QString& service, const QString& account,
                                    const QString& password)
{
#ifdef Q_OS_WIN
    return storeCredentialWindows(service, account, password);
#else
    // Fallback to QSettings for non-Windows (ideally use libsecret or keychain)
    QSettings settings(QSettings::SystemScope, "RawrXD", service);
    settings.setValue(account, password);
    return true;
#endif
}

QString KeychainHelper::retrieveCredential(const QString& service, const QString& account)
{
#ifdef Q_OS_WIN
    return retrieveCredentialWindows(service, account);
#else
    QSettings settings(QSettings::SystemScope, "RawrXD", service);
    return settings.value(account, QString()).toString();
#endif
}

bool KeychainHelper::deleteCredential(const QString& service, const QString& account)
{
#ifdef Q_OS_WIN
    return deleteCredentialWindows(service, account);
#else
    QSettings settings(QSettings::SystemScope, "RawrXD", service);
    settings.remove(account);
    return true;
#endif
}

bool KeychainHelper::credentialExists(const QString& service, const QString& account)
{
#ifdef Q_OS_WIN
    return !retrieveCredentialWindows(service, account).isEmpty();
#else
    QSettings settings(QSettings::SystemScope, "RawrXD", service);
    return settings.contains(account);
#endif
}

#ifdef Q_OS_WIN
bool KeychainHelper::storeCredentialWindows(const QString& service, const QString& account,
                                            const QString& password)
{
    CREDENTIAL cred = {};
    cred.Type = CRED_TYPE_GENERIC;
    cred.TargetName = (LPWSTR)service.utf16();
    cred.UserName = (LPWSTR)account.utf16();
    cred.CredentialBlob = (LPBYTE)password.utf16();
    cred.CredentialBlobSize = (password.length() + 1) * sizeof(wchar_t);
    cred.Persist = CRED_PERSIST_LOCAL_MACHINE;

    bool success = CredWriteW(&cred, 0);
    if (!success) {
        qWarning() << "[Keychain] Failed to store credential:" << GetLastError();
    }
    return success;
}

QString KeychainHelper::retrieveCredentialWindows(const QString& service, const QString& account)
{
    PCREDENTIAL pcred = nullptr;
    bool success = CredReadW((LPWSTR)service.utf16(), CRED_TYPE_GENERIC, 0, &pcred);

    if (!success) {
        qWarning() << "[Keychain] Failed to retrieve credential:" << GetLastError();
        return QString();
    }

    if (pcred->UserName && pcred->CredentialBlob) {
        QString password = QString::fromWCharArray((wchar_t*)pcred->CredentialBlob);
        CredFree(pcred);
        return password;
    }

    CredFree(pcred);
    return QString();
}

bool KeychainHelper::deleteCredentialWindows(const QString& service, const QString& account)
{
    bool success = CredDeleteW((LPWSTR)service.utf16(), CRED_TYPE_GENERIC, 0);
    if (!success) {
        qWarning() << "[Keychain] Failed to delete credential:" << GetLastError();
    }
    return success;
}
#endif

// ─────────────────────────────────────────────────────────────────────
// Configuration JSON Serialization
// ─────────────────────────────────────────────────────────────────────

QJsonObject LLMConfig::toJSON() const
{
    QJsonObject obj;
    obj["backend"] = backend;
    obj["endpoint"] = endpoint;
    obj["model"] = model;
    obj["max_tokens"] = maxTokens;
    obj["temperature"] = temperature;
    obj["cache_enabled"] = cacheEnabled;
    // API key NOT serialized (stored in keychain only)
    return obj;
}

LLMConfig LLMConfig::fromJSON(const QJsonObject& obj)
{
    LLMConfig config;
    config.backend = obj.value("backend").toString("ollama");
    config.endpoint = obj.value("endpoint").toString("http://localhost:11434");
    config.model = obj.value("model").toString("mistral");
    config.maxTokens = obj.value("max_tokens").toInt(2048);
    config.temperature = obj.value("temperature").toDouble(0.7);
    config.cacheEnabled = obj.value("cache_enabled").toBool(true);
    return config;
}

QJsonObject GGUFConfig::toJSON() const
{
    QJsonObject obj;
    obj["model_path"] = modelPath;
    obj["quantization_mode"] = quantizationMode;
    obj["context_size"] = contextSize;
    obj["gpu_layers"] = gpuLayers;
    obj["threads"] = threads;
    obj["offload_embeddings"] = offloadEmbeddings;
    obj["use_memory_mapping"] = useMemoryMapping;
    return obj;
}

GGUFConfig GGUFConfig::fromJSON(const QJsonObject& obj)
{
    GGUFConfig config;
    config.modelPath = obj.value("model_path").toString();
    config.quantizationMode = obj.value("quantization_mode").toString("Q4_0");
    config.contextSize = obj.value("context_size").toInt(2048);
    config.gpuLayers = obj.value("gpu_layers").toInt(0);
    config.threads = obj.value("threads").toInt(0);
    config.offloadEmbeddings = obj.value("offload_embeddings").toBool(true);
    config.useMemoryMapping = obj.value("use_memory_mapping").toBool(true);
    return config;
}

QJsonObject BuildConfig::toJSON() const
{
    QJsonObject obj;
    obj["cmake_path"] = cmakePath;
    obj["msbuild_path"] = msbuildPath;
    obj["masm_path"] = masmPath;
    obj["build_threads"] = buildThreads;
    obj["parallel_build"] = parallelBuild;
    obj["incremental"] = incremental;
    return obj;
}

BuildConfig BuildConfig::fromJSON(const QJsonObject& obj)
{
    BuildConfig config;
    config.cmakePath = obj.value("cmake_path").toString();
    config.msbuildPath = obj.value("msbuild_path").toString();
    config.masmPath = obj.value("masm_path").toString();
    config.buildThreads = obj.value("build_threads").toInt(0);
    config.parallelBuild = obj.value("parallel_build").toBool(true);
    config.incremental = obj.value("incremental").toBool(true);
    return config;
}

// ─────────────────────────────────────────────────────────────────────
// SettingsPanel Implementation
// ─────────────────────────────────────────────────────────────────────

SettingsPanel::SettingsPanel(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle("RawrXD Settings");
    setMinimumWidth(700);
    setMinimumHeight(600);

    setupUI();
    loadSettings();

    connect(m_llmBackendCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &SettingsPanel::onLLMBackendChanged);
    connect(m_testLLMButton, &QPushButton::clicked, this, &SettingsPanel::onTestLLMConnection);
    connect(m_testGGUFButton, &QPushButton::clicked, this, &SettingsPanel::onTestGGUFConnection);
    connect(m_applyButton, &QPushButton::clicked, this, &SettingsPanel::onApplySettings);
    connect(m_resetButton, &QPushButton::clicked, this, &SettingsPanel::onResetToDefaults);
    connect(m_exportButton, &QPushButton::clicked, this, &SettingsPanel::onExportSettings);
    connect(m_importButton, &QPushButton::clicked, this, &SettingsPanel::onImportSettings);
    connect(m_okButton, &QPushButton::clicked, this, [this]() {
        onApplySettings();
        accept();
    });
    connect(m_cancelButton, &QPushButton::clicked, this, &QDialog::reject);
}

void SettingsPanel::setupUI()
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);

    m_tabWidget = new QTabWidget(this);
    mainLayout->addWidget(m_tabWidget);

    setupLLMTab();
    setupGGUFTab();
    setupBuildTab();
    setupHotpatchTab();

    // Dialog buttons
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch();

    m_applyButton = new QPushButton("Apply", this);
    m_resetButton = new QPushButton("Reset to Defaults", this);
    m_exportButton = new QPushButton("Export...", this);
    m_importButton = new QPushButton("Import...", this);
    m_okButton = new QPushButton("OK", this);
    m_cancelButton = new QPushButton("Cancel", this);

    buttonLayout->addWidget(m_exportButton);
    buttonLayout->addWidget(m_importButton);
    buttonLayout->addStretch();
    buttonLayout->addWidget(m_resetButton);
    buttonLayout->addWidget(m_applyButton);
    buttonLayout->addWidget(m_okButton);
    buttonLayout->addWidget(m_cancelButton);

    mainLayout->addLayout(buttonLayout);
}

void SettingsPanel::setupLLMTab()
{
    QWidget* llmTab = new QWidget(this);
    QGridLayout* layout = new QGridLayout(llmTab);

    // Backend selection
    layout->addWidget(new QLabel("Backend:"), 0, 0);
    m_llmBackendCombo = new QComboBox(this);
    m_llmBackendCombo->addItems({"Ollama", "Claude", "OpenAI"});
    layout->addWidget(m_llmBackendCombo, 0, 1);

    // Endpoint
    layout->addWidget(new QLabel("Endpoint:"), 1, 0);
    m_llmEndpointEdit = new QLineEdit(this);
    m_llmEndpointEdit->setPlaceholderText("http://localhost:11434");
    layout->addWidget(m_llmEndpointEdit, 1, 1);

    // API Key
    layout->addWidget(new QLabel("API Key:"), 2, 0);
    m_llmApiKeyEdit = new QLineEdit(this);
    m_llmApiKeyEdit->setEchoMode(QLineEdit::Password);
    layout->addWidget(m_llmApiKeyEdit, 2, 1);

    // Model
    layout->addWidget(new QLabel("Model:"), 3, 0);
    m_llmModelEdit = new QLineEdit(this);
    m_llmModelEdit->setPlaceholderText("mistral");
    layout->addWidget(m_llmModelEdit, 3, 1);

    // Max tokens
    layout->addWidget(new QLabel("Max Tokens:"), 4, 0);
    m_llmMaxTokensSpinBox = new QSpinBox(this);
    m_llmMaxTokensSpinBox->setRange(128, 32768);
    m_llmMaxTokensSpinBox->setValue(2048);
    layout->addWidget(m_llmMaxTokensSpinBox, 4, 1);

    // Temperature
    layout->addWidget(new QLabel("Temperature:"), 5, 0);
    m_llmTemperatureSpinBox = new QDoubleSpinBox(this);
    m_llmTemperatureSpinBox->setRange(0.0, 2.0);
    m_llmTemperatureSpinBox->setSingleStep(0.1);
    m_llmTemperatureSpinBox->setValue(0.7);
    layout->addWidget(m_llmTemperatureSpinBox, 5, 1);

    // Cache
    m_llmCacheCheckBox = new QCheckBox("Enable Response Caching", this);
    m_llmCacheCheckBox->setChecked(true);
    layout->addWidget(m_llmCacheCheckBox, 6, 0, 1, 2);

    // Test button
    m_testLLMButton = new QPushButton("Test Connection", this);
    layout->addWidget(m_testLLMButton, 7, 0, 1, 2);

    // Status
    m_llmStatusLabel = new QLabel("Not tested", this);
    layout->addWidget(m_llmStatusLabel, 8, 0, 1, 2);

    layout->addStretch();
    m_tabWidget->addTab(llmTab, "LLM Backend");
}

void SettingsPanel::setupGGUFTab()
{
    QWidget* ggufTab = new QWidget(this);
    QGridLayout* layout = new QGridLayout(ggufTab);

    // Model path
    layout->addWidget(new QLabel("Model Path:"), 0, 0);
    m_ggufModelPathEdit = new QLineEdit(this);
    layout->addWidget(m_ggufModelPathEdit, 0, 1);
    QPushButton* browseGGUF = new QPushButton("Browse...", this);
    connect(browseGGUF, &QPushButton::clicked, this, &SettingsPanel::onBrowseGGUFModel);
    layout->addWidget(browseGGUF, 0, 2);

    // Quantization
    layout->addWidget(new QLabel("Quantization:"), 1, 0);
    m_ggufQuantCombo = new QComboBox(this);
    m_ggufQuantCombo->addItems({"Q4_0", "Q4_1", "Q5_0", "Q5_1", "Q8_0", "F16", "F32"});
    layout->addWidget(m_ggufQuantCombo, 1, 1);

    // Context size
    layout->addWidget(new QLabel("Context Size:"), 2, 0);
    m_ggufContextSpinBox = new QSpinBox(this);
    m_ggufContextSpinBox->setRange(128, 32768);
    m_ggufContextSpinBox->setValue(2048);
    layout->addWidget(m_ggufContextSpinBox, 2, 1);

    // GPU layers
    layout->addWidget(new QLabel("GPU Layers:"), 3, 0);
    m_ggufGpuLayersSpinBox = new QSpinBox(this);
    m_ggufGpuLayersSpinBox->setRange(0, 100);
    layout->addWidget(m_ggufGpuLayersSpinBox, 3, 1);

    // Offload embeddings
    m_ggufOffloadEmbCheckBox = new QCheckBox("Offload Embeddings to GPU", this);
    m_ggufOffloadEmbCheckBox->setChecked(true);
    layout->addWidget(m_ggufOffloadEmbCheckBox, 4, 0, 1, 2);

    // Memory mapping
    m_ggufMemMapCheckBox = new QCheckBox("Use Memory Mapping", this);
    m_ggufMemMapCheckBox->setChecked(true);
    layout->addWidget(m_ggufMemMapCheckBox, 5, 0, 1, 2);

    // Test button
    m_testGGUFButton = new QPushButton("Test GGUF Server", this);
    layout->addWidget(m_testGGUFButton, 6, 0, 1, 2);

    // Status
    m_ggufStatusLabel = new QLabel("Not tested", this);
    layout->addWidget(m_ggufStatusLabel, 7, 0, 1, 2);

    layout->addStretch();
    m_tabWidget->addTab(ggufTab, "GGUF Model");
}

void SettingsPanel::setupBuildTab()
{
    QWidget* buildTab = new QWidget(this);
    QGridLayout* layout = new QGridLayout(buildTab);

    // CMake
    layout->addWidget(new QLabel("CMake Path:"), 0, 0);
    m_cmakePathEdit = new QLineEdit(this);
    layout->addWidget(m_cmakePathEdit, 0, 1);
    QPushButton* browseCMake = new QPushButton("Browse...", this);
    connect(browseCMake, &QPushButton::clicked, this, &SettingsPanel::onBrowseCMakePath);
    layout->addWidget(browseCMake, 0, 2);

    // MSBuild
    layout->addWidget(new QLabel("MSBuild Path:"), 1, 0);
    m_msbuildPathEdit = new QLineEdit(this);
    layout->addWidget(m_msbuildPathEdit, 1, 1);
    QPushButton* browseMSBuild = new QPushButton("Browse...", this);
    connect(browseMSBuild, &QPushButton::clicked, this, &SettingsPanel::onBrowseMSBuildPath);
    layout->addWidget(browseMSBuild, 1, 2);

    // MASM
    layout->addWidget(new QLabel("MASM Path:"), 2, 0);
    m_masmPathEdit = new QLineEdit(this);
    layout->addWidget(m_masmPathEdit, 2, 1);
    QPushButton* browseMASM = new QPushButton("Browse...", this);
    connect(browseMASM, &QPushButton::clicked, this, &SettingsPanel::onBrowseMASMPath);
    layout->addWidget(browseMASM, 2, 2);

    // Build threads
    layout->addWidget(new QLabel("Build Threads:"), 3, 0);
    m_buildThreadsSpinBox = new QSpinBox(this);
    m_buildThreadsSpinBox->setRange(0, 64);
    m_buildThreadsSpinBox->setValue(0);
    m_buildThreadsSpinBox->setToolTip("0 = auto-detect");
    layout->addWidget(m_buildThreadsSpinBox, 3, 1);

    // Parallel build
    m_parallelBuildCheckBox = new QCheckBox("Parallel Build", this);
    m_parallelBuildCheckBox->setChecked(true);
    layout->addWidget(m_parallelBuildCheckBox, 4, 0, 1, 2);

    // Incremental
    m_incrementalCheckBox = new QCheckBox("Incremental Build", this);
    m_incrementalCheckBox->setChecked(true);
    layout->addWidget(m_incrementalCheckBox, 5, 0, 1, 2);

    layout->addStretch();
    m_tabWidget->addTab(buildTab, "Build Tools");
}

void SettingsPanel::setupHotpatchTab()
{
    QWidget* hotpatchTab = new QWidget(this);
    QGridLayout* layout = new QGridLayout(hotpatchTab);

    m_hotpatchEnabledCheckBox = new QCheckBox("Enable Hotpatch Module", this);
    m_hotpatchEnabledCheckBox->setChecked(true);
    layout->addWidget(m_hotpatchEnabledCheckBox, 0, 0, 1, 2);

    layout->addWidget(new QLabel("Timeout (seconds):"), 1, 0);
    m_hotpatchTimeoutSpinBox = new QSpinBox(this);
    m_hotpatchTimeoutSpinBox->setRange(1, 300);
    m_hotpatchTimeoutSpinBox->setValue(30);
    layout->addWidget(m_hotpatchTimeoutSpinBox, 1, 1);

    m_hotpatchVerboseCheckBox = new QCheckBox("Verbose Logging", this);
    layout->addWidget(m_hotpatchVerboseCheckBox, 2, 0, 1, 2);

    layout->addStretch();
    m_tabWidget->addTab(hotpatchTab, "Hotpatch");
}

LLMConfig SettingsPanel::getLLMConfig() const
{
    LLMConfig config;
    config.backend = m_llmBackendCombo->currentText().toLower();
    config.endpoint = m_llmEndpointEdit->text();
    config.apiKey = m_llmApiKeyEdit->text();
    config.model = m_llmModelEdit->text();
    config.maxTokens = m_llmMaxTokensSpinBox->value();
    config.temperature = m_llmTemperatureSpinBox->value();
    config.cacheEnabled = m_llmCacheCheckBox->isChecked();
    return config;
}

void SettingsPanel::setLLMConfig(const LLMConfig& config)
{
    m_llmBackendCombo->setCurrentText(config.backend.at(0).toUpper() + config.backend.mid(1));
    m_llmEndpointEdit->setText(config.endpoint);
    m_llmModelEdit->setText(config.model);
    m_llmMaxTokensSpinBox->setValue(config.maxTokens);
    m_llmTemperatureSpinBox->setValue(config.temperature);
    m_llmCacheCheckBox->setChecked(config.cacheEnabled);
}

GGUFConfig SettingsPanel::getGGUFConfig() const
{
    GGUFConfig config;
    config.modelPath = m_ggufModelPathEdit->text();
    config.quantizationMode = m_ggufQuantCombo->currentText();
    config.contextSize = m_ggufContextSpinBox->value();
    config.gpuLayers = m_ggufGpuLayersSpinBox->value();
    config.offloadEmbeddings = m_ggufOffloadEmbCheckBox->isChecked();
    config.useMemoryMapping = m_ggufMemMapCheckBox->isChecked();
    return config;
}

void SettingsPanel::setGGUFConfig(const GGUFConfig& config)
{
    m_ggufModelPathEdit->setText(config.modelPath);
    m_ggufQuantCombo->setCurrentText(config.quantizationMode);
    m_ggufContextSpinBox->setValue(config.contextSize);
    m_ggufGpuLayersSpinBox->setValue(config.gpuLayers);
    m_ggufOffloadEmbCheckBox->setChecked(config.offloadEmbeddings);
    m_ggufMemMapCheckBox->setChecked(config.useMemoryMapping);
}

BuildConfig SettingsPanel::getBuildConfig() const
{
    BuildConfig config;
    config.cmakePath = m_cmakePathEdit->text();
    config.msbuildPath = m_msbuildPathEdit->text();
    config.masmPath = m_masmPathEdit->text();
    config.buildThreads = m_buildThreadsSpinBox->value();
    config.parallelBuild = m_parallelBuildCheckBox->isChecked();
    config.incremental = m_incrementalCheckBox->isChecked();
    return config;
}

void SettingsPanel::setBuildConfig(const BuildConfig& config)
{
    m_cmakePathEdit->setText(config.cmakePath);
    m_msbuildPathEdit->setText(config.msbuildPath);
    m_masmPathEdit->setText(config.masmPath);
    m_buildThreadsSpinBox->setValue(config.buildThreads);
    m_parallelBuildCheckBox->setChecked(config.parallelBuild);
    m_incrementalCheckBox->setChecked(config.incremental);
}

void SettingsPanel::saveSettings()
{
    QSettings settings("RawrXD", "QtShell");

    LLMConfig llm = getLLMConfig();
    settings.setValue("llm/backend", llm.backend);
    settings.setValue("llm/endpoint", llm.endpoint);
    settings.setValue("llm/model", llm.model);
    settings.setValue("llm/max_tokens", llm.maxTokens);
    settings.setValue("llm/temperature", llm.temperature);
    settings.setValue("llm/cache_enabled", llm.cacheEnabled);

    // Store API key in keychain
    if (!llm.apiKey.isEmpty()) {
        KeychainHelper::storeCredential("RawrXD", "llm_api_key_" + llm.backend, llm.apiKey);
    }

    GGUFConfig gguf = getGGUFConfig();
    settings.setValue("gguf/model_path", gguf.modelPath);
    settings.setValue("gguf/quantization", gguf.quantizationMode);
    settings.setValue("gguf/context_size", gguf.contextSize);
    settings.setValue("gguf/gpu_layers", gguf.gpuLayers);

    BuildConfig build = getBuildConfig();
    settings.setValue("build/cmake_path", build.cmakePath);
    settings.setValue("build/msbuild_path", build.msbuildPath);
    settings.setValue("build/masm_path", build.masmPath);
    settings.setValue("build/threads", build.buildThreads);

    qInfo() << "[SettingsPanel] Settings saved";
}

void SettingsPanel::loadSettings()
{
    QSettings settings("RawrXD", "QtShell");

    LLMConfig llm;
    llm.backend = settings.value("llm/backend", "ollama").toString();
    llm.endpoint = settings.value("llm/endpoint", "http://localhost:11434").toString();
    llm.model = settings.value("llm/model", "mistral").toString();
    llm.maxTokens = settings.value("llm/max_tokens", 2048).toInt();
    llm.temperature = settings.value("llm/temperature", 0.7).toDouble();
    llm.cacheEnabled = settings.value("llm/cache_enabled", true).toBool();

    // Retrieve API key from keychain
    llm.apiKey = KeychainHelper::retrieveCredential("RawrXD", "llm_api_key_" + llm.backend);

    setLLMConfig(llm);

    GGUFConfig gguf;
    gguf.modelPath = settings.value("gguf/model_path", "").toString();
    gguf.quantizationMode = settings.value("gguf/quantization", "Q4_0").toString();
    gguf.contextSize = settings.value("gguf/context_size", 2048).toInt();
    gguf.gpuLayers = settings.value("gguf/gpu_layers", 0).toInt();
    setGGUFConfig(gguf);

    BuildConfig build;
    build.cmakePath = settings.value("build/cmake_path", "cmake.exe").toString();
    build.msbuildPath = settings.value("build/msbuild_path", "msbuild.exe").toString();
    build.masmPath = settings.value("build/masm_path", "ml64.exe").toString();
    build.buildThreads = settings.value("build/threads", 0).toInt();
    setBuildConfig(build);

    qInfo() << "[SettingsPanel] Settings loaded";
}

void SettingsPanel::onLLMBackendChanged(int index)
{
    // Could update endpoint based on backend
}

void SettingsPanel::onTestLLMConnection()
{
    m_llmStatusLabel->setText("Testing...");
    m_llmStatusLabel->setStyleSheet("color: orange;");
    
    // Real LLM connection test implementation
    QString endpoint = m_llmEndpointEdit->text().trimmed();
    if (endpoint.isEmpty()) {
        m_llmStatusLabel->setText("✗ No endpoint configured");
        m_llmStatusLabel->setStyleSheet("color: red;");
        return;
    }
    
    // Create network manager for async request
    QNetworkAccessManager* manager = new QNetworkAccessManager(this);
    
    // Determine endpoint type and adjust URL
    QUrl url(endpoint);
    QString testPath;
    QByteArray testData;
    
    if (endpoint.contains("ollama") || endpoint.contains("11434")) {
        // Ollama-style endpoint
        testPath = url.path().isEmpty() ? "/api/tags" : url.path();
        url.setPath(testPath);
    } else if (endpoint.contains("openai") || endpoint.contains("api.")) {
        // OpenAI-style endpoint - test with models list
        testPath = "/v1/models";
        url.setPath(testPath);
    } else {
        // Generic endpoint - try a simple GET request
        testPath = url.path().isEmpty() ? "/" : url.path();
        url.setPath(testPath);
    }
    
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    
    // Add API key if configured
    QString apiKey = m_llmApiKeyEdit ? m_llmApiKeyEdit->text().trimmed() : QString();
    if (!apiKey.isEmpty()) {
        request.setRawHeader("Authorization", QString("Bearer %1").arg(apiKey).toUtf8());
    }
    
    QNetworkReply* reply = manager->get(request);
    
    // Set timeout
    QTimer* timer = new QTimer(this);
    timer->setSingleShot(true);
    
    connect(timer, &QTimer::timeout, this, [this, reply, manager, timer]() {
        reply->abort();
        m_llmStatusLabel->setText("✗ Connection timeout");
        m_llmStatusLabel->setStyleSheet("color: red;");
        reply->deleteLater();
        manager->deleteLater();
        timer->deleteLater();
    });
    
    connect(reply, &QNetworkReply::finished, this, [this, reply, manager, timer]() {
        timer->stop();
        
        if (reply->error() == QNetworkReply::NoError) {
            int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            if (statusCode >= 200 && statusCode < 300) {
                m_llmStatusLabel->setText(QString("✓ Connected (HTTP %1)").arg(statusCode));
                m_llmStatusLabel->setStyleSheet("color: green;");
            } else {
                m_llmStatusLabel->setText(QString("⚠ HTTP %1").arg(statusCode));
                m_llmStatusLabel->setStyleSheet("color: orange;");
            }
        } else {
            QString error = reply->errorString();
            if (error.length() > 30) error = error.left(27) + "...";
            m_llmStatusLabel->setText(QString("✗ %1").arg(error));
            m_llmStatusLabel->setStyleSheet("color: red;");
        }
        
        reply->deleteLater();
        manager->deleteLater();
        timer->deleteLater();
    });
    
    timer->start(10000); // 10 second timeout
}

void SettingsPanel::onTestGGUFConnection()
{
    m_ggufStatusLabel->setText("Testing...");
    m_ggufStatusLabel->setStyleSheet("color: orange;");
    
    // Real GGUF file validation implementation
    QString modelPath = m_ggufModelPathEdit ? m_ggufModelPathEdit->text().trimmed() : QString();
    
    if (modelPath.isEmpty()) {
        m_ggufStatusLabel->setText("✗ No model path configured");
        m_ggufStatusLabel->setStyleSheet("color: red;");
        return;
    }
    
    QFileInfo fileInfo(modelPath);
    
    if (!fileInfo.exists()) {
        m_ggufStatusLabel->setText("✗ File not found");
        m_ggufStatusLabel->setStyleSheet("color: red;");
        return;
    }
    
    if (!fileInfo.isReadable()) {
        m_ggufStatusLabel->setText("✗ File not readable");
        m_ggufStatusLabel->setStyleSheet("color: red;");
        return;
    }
    
    // Validate GGUF magic bytes
    QFile file(modelPath);
    if (!file.open(QIODevice::ReadOnly)) {
        m_ggufStatusLabel->setText("✗ Cannot open file");
        m_ggufStatusLabel->setStyleSheet("color: red;");
        return;
    }
    
    QByteArray magic = file.read(4);
    file.close();
    
    if (magic != "GGUF") {
        m_ggufStatusLabel->setText("✗ Invalid GGUF format");
        m_ggufStatusLabel->setStyleSheet("color: red;");
        return;
    }
    
    // File is valid - show size info
    double sizeMB = fileInfo.size() / (1024.0 * 1024.0);
    double sizeGB = sizeMB / 1024.0;
    
    QString sizeStr;
    if (sizeGB >= 1.0) {
        sizeStr = QString::number(sizeGB, 'f', 2) + " GB";
    } else {
        sizeStr = QString::number(sizeMB, 'f', 0) + " MB";
    }
    
    m_ggufStatusLabel->setText(QString("✓ Valid GGUF (%1)").arg(sizeStr));
    m_ggufStatusLabel->setStyleSheet("color: green;");
}

void SettingsPanel::onBrowseCMakePath()
{
    QString path = QFileDialog::getOpenFileName(this, "Select CMake", "", "Executable (*.exe);;All Files (*)");
    if (!path.isEmpty()) {
        m_cmakePathEdit->setText(path);
    }
}

void SettingsPanel::onBrowseMSBuildPath()
{
    QString path = QFileDialog::getOpenFileName(this, "Select MSBuild", "", "Executable (*.exe);;All Files (*)");
    if (!path.isEmpty()) {
        m_msbuildPathEdit->setText(path);
    }
}

void SettingsPanel::onBrowseMASMPath()
{
    QString path = QFileDialog::getOpenFileName(this, "Select MASM", "", "Executable (*.exe);;All Files (*)");
    if (!path.isEmpty()) {
        m_masmPathEdit->setText(path);
    }
}

void SettingsPanel::onBrowseGGUFModel()
{
    QString path = QFileDialog::getOpenFileName(this, "Select GGUF Model", "", "GGUF Files (*.gguf);;All Files (*)");
    if (!path.isEmpty()) {
        m_ggufModelPathEdit->setText(path);
    }
}

void SettingsPanel::onApplySettings()
{
    saveSettings();
    emit settingsChanged();
    QMessageBox::information(this, "Settings Applied", "Settings have been saved successfully.");
}

void SettingsPanel::onResetToDefaults()
{
    if (QMessageBox::question(this, "Reset to Defaults",
                             "Are you sure you want to reset all settings to defaults?") == QMessageBox::Yes) {
        m_llmBackendCombo->setCurrentIndex(0);
        m_llmEndpointEdit->setText("http://localhost:11434");
        m_llmModelEdit->setText("mistral");
        m_llmMaxTokensSpinBox->setValue(2048);
        m_llmTemperatureSpinBox->setValue(0.7);
        m_llmCacheCheckBox->setChecked(true);
        // ... reset other fields
    }
}

void SettingsPanel::onExportSettings()
{
    QString filePath = QFileDialog::getSaveFileName(this, "Export Settings", "", "JSON Files (*.json)");
    if (!filePath.isEmpty()) {
        exportSettings(filePath);
    }
}

void SettingsPanel::onImportSettings()
{
    QString filePath = QFileDialog::getOpenFileName(this, "Import Settings", "", "JSON Files (*.json)");
    if (!filePath.isEmpty()) {
        importSettings(filePath);
    }
}

bool SettingsPanel::exportSettings(const QString& filePath)
{
    QJsonObject root;
    root["llm"] = getLLMConfig().toJSON();
    root["gguf"] = getGGUFConfig().toJSON();
    root["build"] = getBuildConfig().toJSON();

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        QMessageBox::critical(this, "Export Failed", "Could not write to file: " + filePath);
        return false;
    }

    file.write(QJsonDocument(root).toJson());
    file.close();

    QMessageBox::information(this, "Export Successful", "Settings exported to: " + filePath);
    return true;
}

bool SettingsPanel::importSettings(const QString& filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        QMessageBox::critical(this, "Import Failed", "Could not read file: " + filePath);
        return false;
    }

    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();

    if (!doc.isObject()) {
        QMessageBox::critical(this, "Import Failed", "Invalid JSON format");
        return false;
    }

    QJsonObject root = doc.object();
    setLLMConfig(LLMConfig::fromJSON(root.value("llm").toObject()));
    setGGUFConfig(GGUFConfig::fromJSON(root.value("gguf").toObject()));
    setBuildConfig(BuildConfig::fromJSON(root.value("build").toObject()));

    QMessageBox::information(this, "Import Successful", "Settings imported from: " + filePath);
    return true;
}
