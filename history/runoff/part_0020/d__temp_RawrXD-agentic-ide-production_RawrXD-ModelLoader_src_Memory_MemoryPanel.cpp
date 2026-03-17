#include "MemoryPanel.hpp"
#include "MemorySettings.hpp"
#include "EnterpriseMemoryCatalog.hpp"
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QGroupBox>
#include <QtWidgets/QSpinBox>

MemoryPanel::MemoryPanel(mem::MemorySettings* settings,
                         mem::EnterpriseMemoryCatalog* catalog,
                         QWidget* parent)
    : QDockWidget("Memory Manager", parent),
      m_settings(settings),
      m_catalog(catalog) {
    createUI();
    connectSignals();
    updateStats();
}

MemoryPanel::~MemoryPanel() = default;

void MemoryPanel::createUI() {
    QWidget* mainWidget = new QWidget(this);
    QVBoxLayout* mainLayout = new QVBoxLayout(mainWidget);

    // Context Token Slider Group
    QGroupBox* contextGroup = new QGroupBox("Context Window", this);
    QVBoxLayout* contextLayout = new QVBoxLayout(contextGroup);

    QHBoxLayout* sliderLayout = new QHBoxLayout();
    sliderLayout->addWidget(new QLabel("4K"));
    m_contextSlider = new QSlider(Qt::Horizontal);
    m_contextSlider->setRange(0, 100);
    m_contextSlider->setValue(m_settings->contextTokensSlider());
    sliderLayout->addWidget(m_contextSlider);
    sliderLayout->addWidget(new QLabel("1M"));
    contextLayout->addLayout(sliderLayout);

    QHBoxLayout* contextInfoLayout = new QHBoxLayout();
    m_contextLabel = new QLabel();
    contextInfoLayout->addWidget(new QLabel("Size:"));
    contextInfoLayout->addWidget(m_contextLabel);
    contextInfoLayout->addStretch();
    contextLayout->addLayout(contextInfoLayout);

    QHBoxLayout* vramLayout = new QHBoxLayout();
    m_vramEstimate = new QLabel();
    vramLayout->addWidget(new QLabel("Est. VRAM:"));
    vramLayout->addWidget(m_vramEstimate);
    vramLayout->addStretch();
    contextLayout->addLayout(vramLayout);

    mainLayout->addWidget(contextGroup);

    // Options Group
    QGroupBox* optionsGroup = new QGroupBox("Options", this);
    QVBoxLayout* optionsLayout = new QVBoxLayout(optionsGroup);

    m_gpuKvCheckbox = new QCheckBox("Use GPU KV Cache");
    m_gpuKvCheckbox->setChecked(m_settings->useGpuKv());
    optionsLayout->addWidget(m_gpuKvCheckbox);

    m_compressChatCheckbox = new QCheckBox("Compress Chat");
    m_compressChatCheckbox->setChecked(m_settings->compressChat());
    optionsLayout->addWidget(m_compressChatCheckbox);

    m_longTermMemoryCheckbox = new QCheckBox("Enable Long-Term Memory");
    m_longTermMemoryCheckbox->setChecked(m_settings->enableLongTermMemory());
    optionsLayout->addWidget(m_longTermMemoryCheckbox);

    mainLayout->addWidget(optionsGroup);

    // Actions Group
    QGroupBox* actionsGroup = new QGroupBox("Actions", this);
    QVBoxLayout* actionsLayout = new QVBoxLayout(actionsGroup);

    m_indexButton = new QPushButton("Index Workspace");
    actionsLayout->addWidget(m_indexButton);

    m_clearButton = new QPushButton("Clear All Memory");
    m_clearButton->setStyleSheet("background-color: #ff6b6b;");
    actionsLayout->addWidget(m_clearButton);

    m_refreshButton = new QPushButton("Refresh Stats");
    actionsLayout->addWidget(m_refreshButton);

    mainLayout->addWidget(actionsGroup);

    // Stats
    m_statsLabel = new QLabel();
    m_statsLabel->setWordWrap(true);
    mainLayout->addWidget(m_statsLabel);

    // Facts Table
    m_factsTable = new QTableWidget();
    m_factsTable->setColumnCount(3);
    m_factsTable->setHorizontalHeaderLabels({"Key", "Value", "Delete"});
    mainLayout->addWidget(m_factsTable);

    mainLayout->addStretch();
    setWidget(mainWidget);
}

void MemoryPanel::connectSignals() {
    if (m_settings) {
        connect(m_contextSlider, &QSlider::valueChanged, this, &MemoryPanel::onContextSliderChanged);
        connect(m_gpuKvCheckbox, &QCheckBox::toggled, this, &MemoryPanel::onGpuKvToggled);
        connect(m_compressChatCheckbox, &QCheckBox::toggled, this, &MemoryPanel::onCompressChatToggled);
        connect(m_longTermMemoryCheckbox, &QCheckBox::toggled, this, &MemoryPanel::onLongTermMemoryToggled);
    }

    if (m_catalog) {
        connect(m_indexButton, &QPushButton::clicked, this, &MemoryPanel::onIndexWorkspaceClicked);
        connect(m_clearButton, &QPushButton::clicked, this, &MemoryPanel::onClearMemoryClicked);
        connect(m_refreshButton, &QPushButton::clicked, this, &MemoryPanel::onRefreshStatsClicked);
    }
}

void MemoryPanel::onContextSliderChanged(int value) {
    if (m_settings) {
        m_settings->setContextTokensFromSlider(value);
        m_contextLabel->setText(m_settings->contextTokensDisplayText());
        m_vramEstimate->setText(QString::number(m_settings->estimatedVramMb()) + " MB");
    }
}

void MemoryPanel::onGpuKvToggled(bool checked) {
    if (m_settings) {
        m_settings->setUseGpuKv(checked);
    }
}

void MemoryPanel::onCompressChatToggled(bool checked) {
    if (m_settings) {
        m_settings->setCompressChat(checked);
    }
}

void MemoryPanel::onLongTermMemoryToggled(bool checked) {
    if (m_settings) {
        m_settings->setEnableLongTermMemory(checked);
    }
}

void MemoryPanel::onIndexWorkspaceClicked() {
    // TODO: Show dialog for workspace path
    if (m_catalog) {
        m_catalog->indexRepository("current-repo", QDir::currentPath());
        updateStats();
    }
}

void MemoryPanel::onClearMemoryClicked() {
    // TODO: Show confirmation dialog
    if (m_catalog) {
        m_catalog->clearAll("current-user");
        updateStats();
        updateFactsList();
    }
}

void MemoryPanel::onRefreshStatsClicked() {
    updateStats();
    updateFactsList();
}

void MemoryPanel::onDeleteFact() {
    // TODO: Implement fact deletion
}

void MemoryPanel::updateStats() {
    if (!m_catalog) return;

    auto stats = m_catalog->getStats();
    QString statsText;
    for (const auto& [key, value] : stats) {
        statsText += QString::fromStdString(key) + ": " + QString::number(value) + "\n";
    }
    m_statsLabel->setText(statsText);
}

void MemoryPanel::updateFactsList() {
    // TODO: Query and display facts from catalog
}
