#include "AppSettingsDialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QFileDialog>
#include <QMessageBox>
#include <QApplication>

AppSettingsDialog::AppSettingsDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle("Settings - RawrXD Agentic IDE");
    setMinimumSize(700, 500);
    setupUI();
    loadSettings();
}

AppSettingsDialog::~AppSettingsDialog() {}

void AppSettingsDialog::setupUI()
{
    QVBoxLayout* main = new QVBoxLayout(this);
    m_tabs = new QTabWidget(this);

    // General tab
    QWidget* general = new QWidget(this);
    QVBoxLayout* genLayout = new QVBoxLayout(general);

    QGroupBox* endpoints = new QGroupBox("Endpoints", general);
    QVBoxLayout* epLayout = new QVBoxLayout(endpoints);
    {
        QHBoxLayout* row1 = new QHBoxLayout();
        row1->addWidget(new QLabel("LLM Endpoint:"));
        m_llmEndpointEdit = new QLineEdit();
        m_llmEndpointEdit->setPlaceholderText("http://localhost:11434");
        row1->addWidget(m_llmEndpointEdit);
        epLayout->addLayout(row1);

        QHBoxLayout* row2 = new QHBoxLayout();
        row2->addWidget(new QLabel("GGUF Server Port:"));
        m_ggufPortSpin = new QSpinBox();
        m_ggufPortSpin->setRange(1, 65535);
        m_ggufPortSpin->setValue(11434);
        row2->addWidget(m_ggufPortSpin);
        epLayout->addLayout(row2);
    }
    genLayout->addWidget(endpoints);

    QGroupBox* paths = new QGroupBox("Paths", general);
    QVBoxLayout* pathLayout = new QVBoxLayout(paths);
    {
        QHBoxLayout* row1 = new QHBoxLayout();
        row1->addWidget(new QLabel("Project Root:"));
        m_projectRootEdit = new QLineEdit();
        m_browseRootBtn = new QPushButton("Browse...");
        connect(m_browseRootBtn, &QPushButton::clicked, this, &AppSettingsDialog::onBrowseProjectRoot);
        row1->addWidget(m_projectRootEdit);
        row1->addWidget(m_browseRootBtn);
        pathLayout->addLayout(row1);

        QHBoxLayout* row2 = new QHBoxLayout();
        row2->addWidget(new QLabel("Model Cache Dir:"));
        m_modelCacheEdit = new QLineEdit();
        m_browseCacheBtn = new QPushButton("Browse...");
        connect(m_browseCacheBtn, &QPushButton::clicked, this, &AppSettingsDialog::onBrowseModelCache);
        row2->addWidget(m_modelCacheEdit);
        row2->addWidget(m_browseCacheBtn);
        pathLayout->addLayout(row2);
    }
    genLayout->addWidget(paths);

    QGroupBox* logging = new QGroupBox("Logging", general);
    QHBoxLayout* logLayout = new QHBoxLayout(logging);
    logLayout->addWidget(new QLabel("Level:"));
    m_logLevelCombo = new QComboBox();
    m_logLevelCombo->addItems({"DEBUG","INFO","WARN","ERROR"});
    m_logLevelCombo->setCurrentText("INFO");
    logLayout->addWidget(m_logLevelCombo);
    genLayout->addWidget(logging);

    m_loadEnvBtn = new QPushButton("Load From Environment");
    connect(m_loadEnvBtn, &QPushButton::clicked, this, &AppSettingsDialog::onLoadFromEnv);
    genLayout->addWidget(m_loadEnvBtn);

    m_tabs->addTab(general, "General");

    // Agent toggles tab
    QWidget* agent = new QWidget(this);
    QVBoxLayout* agentLayout = new QVBoxLayout(agent);
    m_enableAutonomousCheck = new QCheckBox("Enable Autonomous Features");
    m_enableStreamingCheck = new QCheckBox("Enable Streaming");
    m_enableMetricsCheck = new QCheckBox("Enable Metrics");
    m_enableAutonomousCheck->setChecked(true);
    m_enableStreamingCheck->setChecked(true);
    m_enableMetricsCheck->setChecked(true);
    agentLayout->addWidget(m_enableAutonomousCheck);
    agentLayout->addWidget(m_enableStreamingCheck);
    agentLayout->addWidget(m_enableMetricsCheck);
    m_tabs->addTab(agent, "Agent");

    main->addWidget(m_tabs);

    QHBoxLayout* buttons = new QHBoxLayout();
    buttons->addStretch();
    m_saveBtn = new QPushButton("Save");
    m_cancelBtn = new QPushButton("Cancel");
    connect(m_saveBtn, &QPushButton::clicked, this, &AppSettingsDialog::onSave);
    connect(m_cancelBtn, &QPushButton::clicked, this, &AppSettingsDialog::onCancel);
    buttons->addWidget(m_saveBtn);
    buttons->addWidget(m_cancelBtn);
    main->addLayout(buttons);
}

void AppSettingsDialog::loadSettings()
{
    QSettings s("RawrXD", "AgenticIDE");
    m_llmEndpointEdit->setText(s.value("AppSettings/llm.endpoint", "http://localhost:11434").toString());
    m_ggufPortSpin->setValue(s.value("AppSettings/gguf.server_port", 11434).toInt());
    m_projectRootEdit->setText(s.value("AppSettings/project.default_root", "E:/").toString());
    m_modelCacheEdit->setText(s.value("AppSettings/model.cache_dir", QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + "/RawrXD Agentic IDE/cache/models").toString());
    m_logLevelCombo->setCurrentText(s.value("AppSettings/logging.level", "INFO").toString());
    m_enableAutonomousCheck->setChecked(s.value("AgentToggles/enable_autonomous", true).toBool());
    m_enableStreamingCheck->setChecked(s.value("AgentToggles/enable_streaming", true).toBool());
    m_enableMetricsCheck->setChecked(s.value("AgentToggles/enable_metrics", true).toBool());
}

void AppSettingsDialog::applySettings()
{
    QSettings s("RawrXD", "AgenticIDE");
    s.setValue("AppSettings/llm.endpoint", m_llmEndpointEdit->text());
    s.setValue("AppSettings/gguf.server_port", m_ggufPortSpin->value());
    s.setValue("AppSettings/project.default_root", m_projectRootEdit->text());
    s.setValue("AppSettings/model.cache_dir", m_modelCacheEdit->text());
    s.setValue("AppSettings/logging.level", m_logLevelCombo->currentText());
    s.setValue("AgentToggles/enable_autonomous", m_enableAutonomousCheck->isChecked());
    s.setValue("AgentToggles/enable_streaming", m_enableStreamingCheck->isChecked());
    s.setValue("AgentToggles/enable_metrics", m_enableMetricsCheck->isChecked());
    s.sync();
}

void AppSettingsDialog::onBrowseProjectRoot()
{
    const QString dir = QFileDialog::getExistingDirectory(this, "Select Project Root", m_projectRootEdit->text());
    if (!dir.isEmpty()) m_projectRootEdit->setText(dir);
}

void AppSettingsDialog::onBrowseModelCache()
{
    const QString dir = QFileDialog::getExistingDirectory(this, "Select Model Cache Dir", m_modelCacheEdit->text());
    if (!dir.isEmpty()) m_modelCacheEdit->setText(dir);
}

void AppSettingsDialog::onLoadFromEnv()
{
    const QString proj = qEnvironmentVariable("RAWRXD_PROJECT_ROOT");
    const QString cache = qEnvironmentVariable("RAWRXD_MODEL_CACHE");
    if (!proj.isEmpty()) m_projectRootEdit->setText(proj);
    if (!cache.isEmpty()) m_modelCacheEdit->setText(cache);
    QMessageBox::information(this, "Loaded", "Environment variables loaded into settings form.");
}

void AppSettingsDialog::onSave()
{
    applySettings();
    emit settingsSaved();
    QMessageBox::information(this, "Saved", "Settings have been saved.");
    accept();
}

void AppSettingsDialog::onCancel()
{
    reject();
}
