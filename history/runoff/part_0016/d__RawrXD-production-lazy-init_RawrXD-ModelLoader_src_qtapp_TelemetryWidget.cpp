#include "TelemetryWidget.h"
#include "../llm_adapter/complete_model_loader_system.h"
#include <QComboBox>
#include <QGroupBox>
#include <QDebug>
#include <QSysInfo>

TelemetryWidget::TelemetryWidget(QWidget* parent)
    : QWidget(parent)
    , m_updateTimer(std::make_unique<QTimer>(this))
{
    setupUi();
    initializeTimer();
}

TelemetryWidget::~TelemetryWidget()
{
    if (m_updateTimer) {
        m_updateTimer->stop();
    }
}

void TelemetryWidget::setupUi()
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(5, 5, 5, 5);

    // === System Metrics Group ===
    QGroupBox* systemGroup = new QGroupBox("System Health", this);
    QVBoxLayout* systemLayout = new QVBoxLayout(systemGroup);

    // CPU
    QHBoxLayout* cpuLayout = new QHBoxLayout();
    m_cpuLabel = new QLabel("CPU: 0%", this);
    m_cpuProgress = new QProgressBar(this);
    m_cpuProgress->setRange(0, 100);
    m_cpuProgress->setValue(0);
    m_cpuProgress->setMaximumHeight(20);
    cpuLayout->addWidget(m_cpuLabel, 1);
    cpuLayout->addWidget(m_cpuProgress, 2);
    systemLayout->addLayout(cpuLayout);

    // GPU
    QHBoxLayout* gpuLayout = new QHBoxLayout();
    m_gpuLabel = new QLabel("GPU: 0%", this);
    m_gpuProgress = new QProgressBar(this);
    m_gpuProgress->setRange(0, 100);
    m_gpuProgress->setValue(0);
    m_gpuProgress->setMaximumHeight(20);
    gpuLayout->addWidget(m_gpuLabel, 1);
    gpuLayout->addWidget(m_gpuProgress, 2);
    systemLayout->addLayout(gpuLayout);

    // Memory
    QHBoxLayout* memLayout = new QHBoxLayout();
    m_memoryLabel = new QLabel("Memory: 0/64 GB", this);
    m_memoryProgress = new QProgressBar(this);
    m_memoryProgress->setRange(0, 100);
    m_memoryProgress->setValue(0);
    m_memoryProgress->setMaximumHeight(20);
    memLayout->addWidget(m_memoryLabel, 1);
    memLayout->addWidget(m_memoryProgress, 2);
    systemLayout->addLayout(memLayout);

    // Thermal
    m_thermalLabel = new QLabel("🌡️ Thermal: Normal", this);
    m_thermalLabel->setStyleSheet("color: #4CAF50; font-weight: bold;");
    systemLayout->addWidget(m_thermalLabel);

    mainLayout->addWidget(systemGroup);

    // === Model Info Group ===
    QGroupBox* modelGroup = new QGroupBox("Model Status", this);
    QVBoxLayout* modelLayout = new QVBoxLayout(modelGroup);

    m_modelNameLabel = new QLabel("Model: None loaded", this);
    m_modelSizeLabel = new QLabel("Size: --", this);
    m_compressionLabel = new QLabel("Compression: --", this);

    modelLayout->addWidget(m_modelNameLabel);
    modelLayout->addWidget(m_modelSizeLabel);
    modelLayout->addWidget(m_compressionLabel);

    m_modelLoadProgress = new QProgressBar(this);
    m_modelLoadProgress->setVisible(false);
    m_modelLoadProgress->setMaximumHeight(20);
    modelLayout->addWidget(m_modelLoadProgress);

    mainLayout->addWidget(modelGroup);

    // === Tier Management Group ===
    QGroupBox* tierGroup = new QGroupBox("Tier Management", this);
    QVBoxLayout* tierLayout = new QVBoxLayout(tierGroup);

    m_currentTierLabel = new QLabel("Active Tier: TIER_70B", this);
    m_currentTierLabel->setStyleSheet("font-weight: bold; color: #007acc;");
    tierLayout->addWidget(m_currentTierLabel);

    QLabel* tierSelectorLabel = new QLabel("Switch Tier:", this);
    m_tierSelector = new QComboBox(this);
    m_tierSelector->addItems({"TIER_70B", "TIER_21B", "TIER_6B", "TIER_2B"});
    m_tierSelector->setCurrentIndex(0);

    QHBoxLayout* tierSelectLayout = new QHBoxLayout();
    tierSelectLayout->addWidget(tierSelectorLabel);
    tierSelectLayout->addWidget(m_tierSelector, 1);
    tierLayout->addLayout(tierSelectLayout);

    mainLayout->addWidget(tierGroup);

    // === Control Buttons Group ===
    QGroupBox* controlGroup = new QGroupBox("Controls", this);
    QVBoxLayout* controlLayout = new QVBoxLayout(controlGroup);

    m_loadModelButton = new QPushButton("📥 Load Model", this);
    m_unloadModelButton = new QPushButton("📤 Unload Model", this);
    m_autoTuneButton = new QPushButton("⚙️ Auto-Tune", this);
    m_qualityTestButton = new QPushButton("🧪 Quality Test", this);
    m_benchmarkButton = new QPushButton("📈 Benchmark", this);

    QString buttonStyle = "padding: 6px; font-weight: bold; border-radius: 4px;";
    m_loadModelButton->setStyleSheet(buttonStyle + " background-color: #4CAF50; color: white;");
    m_unloadModelButton->setStyleSheet(buttonStyle + " background-color: #f44336; color: white;");
    m_autoTuneButton->setStyleSheet(buttonStyle + " background-color: #FF9800; color: white;");
    m_qualityTestButton->setStyleSheet(buttonStyle + " background-color: #2196F3; color: white;");
    m_benchmarkButton->setStyleSheet(buttonStyle + " background-color: #9C27B0; color: white;");

    controlLayout->addWidget(m_loadModelButton);
    controlLayout->addWidget(m_unloadModelButton);
    controlLayout->addWidget(m_autoTuneButton);
    controlLayout->addWidget(m_qualityTestButton);
    controlLayout->addWidget(m_benchmarkButton);

    mainLayout->addWidget(controlGroup);
    mainLayout->addStretch();

    // Connections
    connect(m_tierSelector, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int index) {
        onTierChanged(index);
    });
    connect(m_loadModelButton, &QPushButton::clicked, this, &TelemetryWidget::onLoadModelClicked);
    connect(m_unloadModelButton, &QPushButton::clicked, this, &TelemetryWidget::onUnloadModelClicked);
    connect(m_autoTuneButton, &QPushButton::clicked, this, &TelemetryWidget::onAutoTuneClicked);
    connect(m_qualityTestButton, &QPushButton::clicked, this, &TelemetryWidget::onQualityTestClicked);
    connect(m_benchmarkButton, &QPushButton::clicked, this, &TelemetryWidget::onBenchmarkClicked);
}

void TelemetryWidget::initializeTimer()
{
    connect(m_updateTimer.get(), &QTimer::timeout, this, &TelemetryWidget::onUpdateTimer);
    m_updateTimer->start(1000);  // Update every 1 second
}

void TelemetryWidget::onUpdateTimer()
{
    updateMetrics();
}

void TelemetryWidget::updateMetrics()
{
    updateSystemMetrics();
    updateModelMetrics();
}

void TelemetryWidget::updateSystemMetrics()
{
    // These would normally read from /proc or WMI
    // For now, we'll use placeholder values
    // In production, integrate with actual system monitoring

    m_cpuLabel->setText("CPU: 25%");
    m_cpuProgress->setValue(25);

    m_gpuLabel->setText("GPU: 40%");
    m_gpuProgress->setValue(40);

    m_memoryLabel->setText("Memory: 16/64 GB");
    m_memoryProgress->setValue(25);

    m_thermalLabel->setText("🌡️ Thermal: Normal");
    m_thermalLabel->setStyleSheet("color: #4CAF50; font-weight: bold;");
}

void TelemetryWidget::updateModelMetrics()
{
    if (!m_modelLoader) return;

    auto health = m_modelLoader->getSystemHealth();
    m_cpuProgress->setValue(static_cast<int>(health.cpu_usage_percent));
    m_gpuProgress->setValue(static_cast<int>(health.gpu_usage_percent));
    m_memoryProgress->setValue(static_cast<int>(health.memory_used_gb / (health.memory_used_gb + health.memory_available_gb) * 100));

    if (health.thermal_throttling_detected) {
        m_thermalLabel->setText("🌡️ Thermal: ⚠️ THROTTLING");
        m_thermalLabel->setStyleSheet("color: #f44336; font-weight: bold;");
    }
}

void TelemetryWidget::setModelLoader(rawr_xd::CompleteModelLoaderSystem* loader)
{
    m_modelLoader = loader;
}

void TelemetryWidget::updateModelInfo(const QString& modelPath, const QString& compressionStats)
{
    m_modelNameLabel->setText("Model: " + modelPath);
    m_compressionLabel->setText("Compression: " + compressionStats);
}

void TelemetryWidget::setCurrentTier(const QString& tier)
{
    m_currentTierLabel->setText("Active Tier: " + tier);
    int index = m_tierSelector->findText(tier);
    if (index >= 0) {
        m_tierSelector->blockSignals(true);
        m_tierSelector->setCurrentIndex(index);
        m_tierSelector->blockSignals(false);
    }
}

void TelemetryWidget::displaySystemHealth()
{
    if (!m_modelLoader) return;

    auto health = m_modelLoader->getSystemHealth();
    m_cpuLabel->setText(QString("CPU: %1%").arg(static_cast<int>(health.cpu_usage_percent)));
    m_gpuLabel->setText(QString("GPU: %1%").arg(static_cast<int>(health.gpu_usage_percent)));
    m_memoryLabel->setText(QString("Memory: %1/%2 GB")
        .arg(static_cast<int>(health.memory_used_gb))
        .arg(static_cast<int>(health.memory_used_gb + health.memory_available_gb)));
}

void TelemetryWidget::onTierChanged(int index)
{
    QString tier = m_tierSelector->itemText(index);
    emit tierSelectionChanged(tier);
}

void TelemetryWidget::onAutoTuneClicked()
{
    emit autoTuneRequested();
}

void TelemetryWidget::onQualityTestClicked()
{
    emit qualityTestRequested();
}

void TelemetryWidget::onBenchmarkClicked()
{
    emit benchmarkRequested();
}

void TelemetryWidget::onLoadModelClicked()
{
    emit modelLoadRequested();
}

void TelemetryWidget::onUnloadModelClicked()
{
    emit modelUnloadRequested();
}
