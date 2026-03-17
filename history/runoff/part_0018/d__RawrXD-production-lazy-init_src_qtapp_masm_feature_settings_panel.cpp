#include "masm_feature_settings_panel.hpp"
#include "settings_manager.h"
#include "settings.h"
#include "masm_kernels.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSplitter>
#include <QGroupBox>
#include <QFileDialog>
#include <QMessageBox>
#include <QHeaderView>

MasmFeatureSettingsPanel::MasmFeatureSettingsPanel(QWidget* parent)
    : QWidget(parent)
    , m_manager(MasmFeatureManager::instance())
    , m_selectedItem(nullptr)
    , m_totalFeatures(0)
    , m_enabledFeatures(0)
    , m_totalMemoryKB(0)
    , m_totalCPUPercent(0)
{
    setupUI();
    populateFeatureTree();
    updateMetricsDisplay();
    
    // Connect signals from manager
    connect(m_manager, &MasmFeatureManager::featureEnabledChanged,
            this, &MasmFeatureSettingsPanel::updateMetricsDisplay);
    connect(m_manager, &MasmFeatureManager::presetChanged,
            this, [this](MasmFeatureManager::Preset preset) {
                m_presetCombo->setCurrentIndex(static_cast<int>(preset));
                populateFeatureTree();
                updateMetricsDisplay();
            });
}

MasmFeatureSettingsPanel::~MasmFeatureSettingsPanel() = default;

void MasmFeatureSettingsPanel::setupUI() {
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    
    // ========================================
    // TOP: Preset Selection
    // ========================================
    QGroupBox* presetGroup = new QGroupBox("Preset Configuration", this);
    QHBoxLayout* presetLayout = new QHBoxLayout(presetGroup);
    
    QLabel* presetLabel = new QLabel("Select Preset:", presetGroup);
    m_presetCombo = new QComboBox(presetGroup);
    m_presetCombo->addItem("Minimal (32 features, ~10 MB, ~15% CPU)");
    m_presetCombo->addItem("Standard (68 features, ~25 MB, ~40% CPU) [Default]");
    m_presetCombo->addItem("Performance (45 features, ~18 MB, ~30% CPU)");
    m_presetCombo->addItem("Development (120 features, ~45 MB, ~70% CPU)");
    m_presetCombo->addItem("Maximum (212 features, ~85 MB, ~150% CPU)");
    m_presetCombo->addItem("Custom (User-defined)");
    m_presetCombo->setCurrentIndex(static_cast<int>(m_manager->getCurrentPreset()));
    
    presetLayout->addWidget(presetLabel);
    presetLayout->addWidget(m_presetCombo, 1);
    
    connect(m_presetCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MasmFeatureSettingsPanel::onPresetChanged);
    
    mainLayout->addWidget(presetGroup);
    
    // ========================================
    // MIDDLE: Splitter with Tree and Details
    // ========================================
    QSplitter* splitter = new QSplitter(Qt::Horizontal, this);
    
    // Left: Feature Tree
    m_featureTree = new QTreeWidget(splitter);
    m_featureTree->setHeaderLabels({"Feature", "Status", "Memory", "CPU", "Hot-Reload"});
    m_featureTree->setColumnWidth(0, 300);
    m_featureTree->setColumnWidth(1, 80);
    m_featureTree->setColumnWidth(2, 80);
    m_featureTree->setColumnWidth(3, 60);
    m_featureTree->setColumnWidth(4, 90);
    m_featureTree->setAlternatingRowColors(true);
    m_featureTree->header()->setStretchLastSection(false);
    
    connect(m_featureTree, &QTreeWidget::itemClicked,
            this, &MasmFeatureSettingsPanel::onFeatureToggled);
    connect(m_featureTree, &QTreeWidget::itemClicked,
            this, &MasmFeatureSettingsPanel::updateFeatureDetails);
    
    splitter->addWidget(m_featureTree);
    
    // Right: Details Panel
    QWidget* rightPanel = new QWidget(splitter);
    QVBoxLayout* rightLayout = new QVBoxLayout(rightPanel);
    
    // Metrics Dashboard
    QGroupBox* metricsGroup = new QGroupBox("System Metrics", rightPanel);
    QVBoxLayout* metricsLayout = new QVBoxLayout(metricsGroup);

    // MASM Backend Controls
    QGroupBox* masmGroup = new QGroupBox("MASM CPU Backend", rightPanel);
    QHBoxLayout* masmLayout = new QHBoxLayout(masmGroup);
    m_enableMasmBackend = new QCheckBox("Enable MASM CPU Backend", masmGroup);
    m_masmStatusLabel = new QLabel("Status: Unknown", masmGroup);
    QPushButton* masmRefresh = new QPushButton("Refresh Status", masmGroup);
    masmLayout->addWidget(m_enableMasmBackend);
    masmLayout->addWidget(m_masmStatusLabel, 1);
    masmLayout->addWidget(masmRefresh);
    metricsLayout->addWidget(masmGroup);

    // Initialize MASM checkbox from QSettings
    bool masmEnabled = SettingsManager::instance().getValue("masm/enable", true).toBool();
    m_enableMasmBackend->setChecked(masmEnabled);
    extern AppState g_app_state; g_app_state.enable_masm_cpu_backend = masmEnabled;
    auto updateMASMStatus = [this]() {
        auto info = masm::backend_info();
        QString status = QString("compiled=%1 cpu=%2 os_xsave=%3")
            .arg(info.compiled_with_avx2 ? "1" : "0")
            .arg(info.cpu_avx2 ? "1" : "0")
            .arg(info.os_xsave_enabled ? "1" : "0");
        QString enabled = m_enableMasmBackend->isChecked() ? "(user-enabled)" : "(user-disabled)";
        m_masmStatusLabel->setText("Status: " + status + " " + enabled);
    };
    updateMASMStatus();
    connect(masmRefresh, &QPushButton::clicked, this, updateMASMStatus);
    connect(m_enableMasmBackend, &QCheckBox::toggled, this, [this, updateMASMStatus](bool checked) {
        SettingsManager::instance().setValue("masm/enable", checked);
        extern AppState g_app_state; g_app_state.enable_masm_cpu_backend = checked;
        updateMASMStatus();
    });
    
    m_totalFeaturesLabel = new QLabel("Total Features: 0", metricsGroup);
    m_enabledFeaturesLabel = new QLabel("Enabled: 0", metricsGroup);
    
    QLabel* memLabel = new QLabel("Memory Usage:", metricsGroup);
    m_memoryBar = new QProgressBar(metricsGroup);
    m_memoryBar->setRange(0, 135000); // 135 MB max
    m_totalMemoryLabel = new QLabel("0 MB / 135 MB (0%)", metricsGroup);
    
    QLabel* cpuLabel = new QLabel("CPU Usage:", metricsGroup);
    m_cpuBar = new QProgressBar(metricsGroup);
    m_cpuBar->setRange(0, 500); // 500% max (multi-threaded)
    m_cpuUsageLabel = new QLabel("0% / 500% (0%)", metricsGroup);
    
    metricsLayout->addWidget(m_totalFeaturesLabel);
    metricsLayout->addWidget(m_enabledFeaturesLabel);
    metricsLayout->addWidget(memLabel);
    metricsLayout->addWidget(m_memoryBar);
    metricsLayout->addWidget(m_totalMemoryLabel);
    metricsLayout->addWidget(cpuLabel);
    metricsLayout->addWidget(m_cpuBar);
    metricsLayout->addWidget(m_cpuUsageLabel);
    
    rightLayout->addWidget(metricsGroup);
    
    // Feature Details
    QGroupBox* detailsGroup = new QGroupBox("Feature Details", rightPanel);
    QVBoxLayout* detailsLayout = new QVBoxLayout(detailsGroup);
    
    m_featureDetails = new QTextEdit(detailsGroup);
    m_featureDetails->setReadOnly(true);
    m_featureDetails->setPlaceholderText("Select a feature to view details...");
    
    detailsLayout->addWidget(m_featureDetails);
    
    rightLayout->addWidget(detailsGroup, 1);
    
    splitter->addWidget(rightPanel);
    splitter->setStretchFactor(0, 2);
    splitter->setStretchFactor(1, 1);
    
    mainLayout->addWidget(splitter, 1);
    
    // ========================================
    // BOTTOM: Action Buttons
    // ========================================
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    
    m_applyButton = new QPushButton("Apply Changes", this);
    m_resetButton = new QPushButton("Reset to Defaults", this);
    m_exportButton = new QPushButton("Export Config", this);
    m_importButton = new QPushButton("Import Config", this);
    m_hotReloadButton = new QPushButton("Hot Reload", this);
    m_hotReloadButton->setEnabled(false);
    
    QPushButton* refreshButton = new QPushButton("Refresh Metrics", this);
    
    connect(m_applyButton, &QPushButton::clicked, this, &MasmFeatureSettingsPanel::onApplyChanges);
    connect(m_resetButton, &QPushButton::clicked, this, &MasmFeatureSettingsPanel::onResetDefaults);
    connect(m_exportButton, &QPushButton::clicked, this, &MasmFeatureSettingsPanel::onExportConfig);
    connect(m_importButton, &QPushButton::clicked, this, &MasmFeatureSettingsPanel::onImportConfig);
    connect(m_hotReloadButton, &QPushButton::clicked, this, &MasmFeatureSettingsPanel::onHotReloadFeature);
    connect(refreshButton, &QPushButton::clicked, this, &MasmFeatureSettingsPanel::onRefreshMetrics);
    
    buttonLayout->addWidget(m_applyButton);
    buttonLayout->addWidget(m_resetButton);
    buttonLayout->addWidget(m_exportButton);
    buttonLayout->addWidget(m_importButton);
    buttonLayout->addWidget(m_hotReloadButton);
    buttonLayout->addWidget(refreshButton);
    buttonLayout->addStretch();
    
    mainLayout->addLayout(buttonLayout);
}

void MasmFeatureSettingsPanel::populateFeatureTree() {
    m_featureTree->clear();
    
    // Get all features grouped by category
    QHash<MasmFeatureManager::Category, QList<MasmFeatureManager::FeatureInfo>> categorizedFeatures;
    
    QList<MasmFeatureManager::FeatureInfo> allFeatures = m_manager->getAllFeatures();
    for (const auto& feature : allFeatures) {
        categorizedFeatures[feature.category].append(feature);
    }
    
    // Create category nodes
    QList<MasmFeatureManager::Category> categories = categorizedFeatures.keys();
    
    m_totalFeatures = 0;
    m_enabledFeatures = 0;
    
    for (const auto& category : categories) {
        QString categoryName = m_manager->categoryToString(category);
        QTreeWidgetItem* categoryItem = new QTreeWidgetItem(m_featureTree);
        categoryItem->setText(0, QString("📦 %1 (%2)").arg(categoryName).arg(categorizedFeatures[category].size()));
        categoryItem->setData(0, Qt::UserRole, static_cast<int>(category));
        categoryItem->setData(0, Qt::UserRole + 1, true); // Mark as category
        
        // Count enabled features in this category
        int categoryEnabled = 0;
        for (const auto& feature : categorizedFeatures[category]) {
            if (m_manager->isFeatureEnabled(feature.name)) {
                categoryEnabled++;
            }
        }
        
        // Set category status icon
        if (categoryEnabled == categorizedFeatures[category].size()) {
            categoryItem->setText(1, "✅");
        } else if (categoryEnabled > 0) {
            categoryItem->setText(1, "⚙️");
        } else {
            categoryItem->setText(1, "❌");
        }
        
        categoryItem->setExpanded(true);
        
        // Add features under category
        for (const auto& feature : categorizedFeatures[category]) {
            QTreeWidgetItem* featureItem = new QTreeWidgetItem(categoryItem);
            featureItem->setText(0, feature.displayName);
            featureItem->setData(0, Qt::UserRole, feature.name);
            featureItem->setData(0, Qt::UserRole + 1, false); // Mark as feature
            
            bool enabled = m_manager->isFeatureEnabled(feature.name);
            featureItem->setText(1, enabled ? "✅" : "❌");
            featureItem->setText(2, QString("%1 KB").arg(feature.estimatedMemoryKB));
            featureItem->setText(3, QString("%1%").arg(feature.estimatedCPUPercent));
            featureItem->setText(4, feature.requiresRestart ? "⚠️ Restart" : "✅ Yes");
            
            if (enabled) {
                featureItem->setForeground(0, Qt::darkGreen);
                m_enabledFeatures++;
            } else {
                featureItem->setForeground(0, Qt::gray);
            }
            
            m_totalFeatures++;
        }
    }
}

void MasmFeatureSettingsPanel::updateMetricsDisplay() {
    // Count enabled features and calculate totals
    m_totalFeatures = 0;
    m_enabledFeatures = 0;
    m_totalMemoryKB = 0;
    m_totalCPUPercent = 0;
    
    QList<MasmFeatureManager::FeatureInfo> allFeatures = m_manager->getAllFeatures();
    for (const auto& feature : allFeatures) {
        m_totalFeatures++;
        if (m_manager->isFeatureEnabled(feature.name)) {
            m_enabledFeatures++;
            m_totalMemoryKB += feature.estimatedMemoryKB;
            m_totalCPUPercent += feature.estimatedCPUPercent;
        }
    }
    
    // Update labels
    m_totalFeaturesLabel->setText(QString("📊 Total Features: %1").arg(m_totalFeatures));
    m_enabledFeaturesLabel->setText(QString("✅ Enabled: %1 (%2%)")
        .arg(m_enabledFeatures)
        .arg(m_totalFeatures > 0 ? (m_enabledFeatures * 100 / m_totalFeatures) : 0));
    
    // Update memory bar
    qint64 totalMemoryMB = m_totalMemoryKB / 1024;
    qint64 maxMemoryMB = 135; // 135 MB max
    m_memoryBar->setValue(m_totalMemoryKB);
    m_totalMemoryLabel->setText(QString("💾 %1 MB / %2 MB (%3%)")
        .arg(totalMemoryMB)
        .arg(maxMemoryMB)
        .arg(maxMemoryMB > 0 ? (totalMemoryMB * 100 / maxMemoryMB) : 0));
    
    // Update CPU bar
    int maxCPU = 500; // 500% max
    m_cpuBar->setValue(m_totalCPUPercent);
    m_cpuUsageLabel->setText(QString("⚡ %1% / %2% (%3%)")
        .arg(m_totalCPUPercent)
        .arg(maxCPU)
        .arg(maxCPU > 0 ? (m_totalCPUPercent * 100 / maxCPU) : 0));
}

void MasmFeatureSettingsPanel::updateMemoryUsage() {
    updateMetricsDisplay();
}

void MasmFeatureSettingsPanel::onPresetChanged(int index) {
    MasmFeatureManager::Preset preset = static_cast<MasmFeatureManager::Preset>(index);
    
    // Prevent recursive calls - if preset is already current, skip
    if (preset == m_manager->getCurrentPreset()) {
        return;
    }
    
    QMessageBox::StandardButton reply = QMessageBox::question(
        this,
        "Apply Preset",
        QString("Apply preset configuration? This will change enabled features."),
        QMessageBox::Yes | QMessageBox::No
    );
    
    if (reply == QMessageBox::Yes) {
        m_manager->applyPreset(preset);
        populateFeatureTree();
        updateMetricsDisplay();
    } else {
        // Revert combo box to current preset
        m_presetCombo->setCurrentIndex(static_cast<int>(m_manager->getCurrentPreset()));
    }
}

void MasmFeatureSettingsPanel::onFeatureToggled(QTreeWidgetItem* item, int column) {
    if (!item || column != 1) return;
    
    bool isCategory = item->data(0, Qt::UserRole + 1).toBool();
    
    if (isCategory) {
        // Toggle category
        MasmFeatureManager::Category category = static_cast<MasmFeatureManager::Category>(
            item->data(0, Qt::UserRole).toInt()
        );
        
        bool currentlyEnabled = m_manager->isCategoryEnabled(category);
        m_manager->setCategoryEnabled(category, !currentlyEnabled);
        populateFeatureTree();
        updateMetricsDisplay();
    } else {
        // Toggle feature
        QString featureName = item->data(0, Qt::UserRole).toString();
        bool currentlyEnabled = m_manager->isFeatureEnabled(featureName);
        m_manager->setFeatureEnabled(featureName, !currentlyEnabled);
        
        // Update visual state
        item->setText(1, !currentlyEnabled ? "✅" : "❌");
        item->setForeground(0, !currentlyEnabled ? Qt::darkGreen : Qt::gray);
        
        updateMetricsDisplay();
    }
}

void MasmFeatureSettingsPanel::onCategoryToggled() {
    // Handled in onFeatureToggled
}

void MasmFeatureSettingsPanel::onApplyChanges() {
    m_manager->saveSettings();
    QMessageBox::information(this, "Settings Saved", "Feature configuration saved successfully.");
}

void MasmFeatureSettingsPanel::onResetDefaults() {
    QMessageBox::StandardButton reply = QMessageBox::question(
        this,
        "Reset to Defaults",
        "Reset all features to default configuration?",
        QMessageBox::Yes | QMessageBox::No
    );
    
    if (reply == QMessageBox::Yes) {
        m_manager->resetToDefaults();
        populateFeatureTree();
        updateMetricsDisplay();
    }
}

void MasmFeatureSettingsPanel::onRefreshMetrics() {
    updateMetricsDisplay();
    QMessageBox::information(this, "Metrics Refreshed", "Performance metrics updated.");
}

void MasmFeatureSettingsPanel::onExportConfig() {
    QString filePath = QFileDialog::getSaveFileName(
        this,
        "Export Configuration",
        "masm_features.json",
        "JSON Files (*.json)"
    );
    
    if (!filePath.isEmpty()) {
        if (m_manager->exportConfig(filePath)) {
            QMessageBox::information(this, "Export Successful", 
                QString("Configuration exported to:\n%1").arg(filePath));
        } else {
            QMessageBox::warning(this, "Export Failed", 
                "Failed to export configuration.");
        }
    }
}

void MasmFeatureSettingsPanel::onImportConfig() {
    QString filePath = QFileDialog::getOpenFileName(
        this,
        "Import Configuration",
        "",
        "JSON Files (*.json)"
    );
    
    if (!filePath.isEmpty()) {
        if (m_manager->importConfig(filePath)) {
            populateFeatureTree();
            updateMetricsDisplay();
            QMessageBox::information(this, "Import Successful", 
                "Configuration imported successfully.");
        } else {
            QMessageBox::warning(this, "Import Failed", 
                "Failed to import configuration.");
        }
    }
}

void MasmFeatureSettingsPanel::onHotReloadFeature() {
    if (!m_selectedItem) return;
    
    bool isCategory = m_selectedItem->data(0, Qt::UserRole + 1).toBool();
    if (isCategory) return;
    
    QString featureName = m_selectedItem->data(0, Qt::UserRole).toString();
    
    if (!m_manager->canHotReload(featureName)) {
        QMessageBox::warning(this, "Hot-Reload Not Supported",
            "This feature requires a restart to apply changes.");
        return;
    }
    
    bool success = m_manager->hotReload(featureName);
    if (success) {
        QMessageBox::information(this, "Hot-Reload Successful",
            QString("Feature '%1' reloaded successfully.").arg(featureName));
    } else {
        QMessageBox::warning(this, "Hot-Reload Failed",
            QString("Failed to reload feature '%1'.").arg(featureName));
    }
}

void MasmFeatureSettingsPanel::updateFeatureDetails(QTreeWidgetItem* item, int column) {
    Q_UNUSED(column);
    
    m_selectedItem = item;
    
    if (!item) {
        m_featureDetails->clear();
        m_hotReloadButton->setEnabled(false);
        return;
    }
    
    bool isCategory = item->data(0, Qt::UserRole + 1).toBool();
    
    if (isCategory) {
        // Show category summary
        MasmFeatureManager::Category category = static_cast<MasmFeatureManager::Category>(
            item->data(0, Qt::UserRole).toInt()
        );
        
        QList<MasmFeatureManager::FeatureInfo> features = m_manager->getFeaturesByCategory(category);
        
        int enabledCount = 0;
        qint64 totalMem = 0;
        int totalCPU = 0;
        
        for (const auto& feature : features) {
            if (m_manager->isFeatureEnabled(feature.name)) {
                enabledCount++;
                totalMem += feature.estimatedMemoryKB;
                totalCPU += feature.estimatedCPUPercent;
            }
        }
        
        QString details = QString(
            "<h3>Category: %1</h3>"
            "<p><b>Total Features:</b> %2</p>"
            "<p><b>Enabled:</b> %3 (%4%)</p>"
            "<p><b>Memory:</b> %5 MB</p>"
            "<p><b>CPU:</b> %6%</p>"
        ).arg(m_manager->categoryToString(category))
         .arg(features.size())
         .arg(enabledCount)
         .arg(features.size() > 0 ? (enabledCount * 100 / features.size()) : 0)
         .arg(totalMem / 1024)
         .arg(totalCPU);
        
        m_featureDetails->setHtml(details);
        m_hotReloadButton->setEnabled(false);
    } else {
        // Show feature details
        QString featureName = item->data(0, Qt::UserRole).toString();
        
        QList<MasmFeatureManager::FeatureInfo> allFeatures = m_manager->getAllFeatures();
        MasmFeatureManager::FeatureInfo featureInfo;
        
        for (const auto& feature : allFeatures) {
            if (feature.name == featureName) {
                featureInfo = feature;
                break;
            }
        }
        
        bool enabled = m_manager->isFeatureEnabled(featureName);
        bool canReload = m_manager->canHotReload(featureName);
        
        MasmFeatureManager::PerformanceMetrics metrics = m_manager->getPerformanceMetrics(featureName);
        
        QString details = QString(
            "<h3>%1</h3>"
            "<p><b>Category:</b> %2</p>"
            "<p><b>Status:</b> %3</p>"
            "<p><b>Hot-Reload:</b> %4</p>"
            "<hr>"
            "<p><b>Description:</b><br>%5</p>"
            "<hr>"
            "<p><b>Estimated Memory:</b> %6 KB</p>"
            "<p><b>Estimated CPU:</b> %7%</p>"
            "<hr>"
            "<p><b>Performance Metrics:</b></p>"
            "<ul>"
            "<li>CPU Time: %8 ms</li>"
            "<li>Peak Memory: %9 KB</li>"
            "<li>Call Count: %10</li>"
            "<li>Avg Latency: %11 ms</li>"
            "</ul>"
            "<hr>"
            "<p><b>Dependencies:</b> %12</p>"
            "<p><b>Source File:</b> <code>%13</code></p>"
        ).arg(featureInfo.displayName)
         .arg(m_manager->categoryToString(featureInfo.category))
         .arg(enabled ? "✅ Enabled" : "❌ Disabled")
         .arg(canReload ? "✅ Supported" : "⚠️ Requires Restart")
         .arg(featureInfo.description)
         .arg(featureInfo.estimatedMemoryKB)
         .arg(featureInfo.estimatedCPUPercent)
         .arg(metrics.totalCpuTimeMs)
         .arg(metrics.peakMemoryBytes / 1024)
         .arg(metrics.callCount)
         .arg(metrics.avgLatencyMs, 0, 'f', 2)
         .arg(featureInfo.dependencies.isEmpty() ? "None" : featureInfo.dependencies.join(", "))
         .arg(featureInfo.asmFilePath);
        
        m_featureDetails->setHtml(details);
        m_hotReloadButton->setEnabled(enabled && canReload);
    }
}
