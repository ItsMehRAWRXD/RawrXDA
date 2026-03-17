#pragma once

#include "masm_feature_manager.hpp"
#include <QWidget>
#include <QTreeWidget>
#include <QLabel>
#include <QPushButton>
#include <QComboBox>
#include <QCheckBox>
#include <QProgressBar>
#include <QTextEdit>

/**
 * @brief MASM Feature Settings Panel
 * 
 * Interactive UI for managing 212+ MASM features.
 * Displays all features in a tree view organized by category.
 * Shows real-time performance metrics and memory usage.
 * Supports presets and hot-reload where available.
 */
class MasmFeatureSettingsPanel : public QWidget {
    Q_OBJECT

public:
    explicit MasmFeatureSettingsPanel(QWidget* parent = nullptr);
    ~MasmFeatureSettingsPanel() override;

private slots:
    void onPresetChanged(int index);
    void onFeatureToggled(QTreeWidgetItem* item, int column);
    void onCategoryToggled();
    void onApplyChanges();
    void onResetDefaults();
    void onRefreshMetrics();
    void onExportConfig();
    void onImportConfig();
    void onHotReloadFeature();
    void updateFeatureDetails(QTreeWidgetItem* item, int column);

private:
    void setupUI();
    void populateFeatureTree();
    void updateMetricsDisplay();
    void updateMemoryUsage();
    
    // Widgets
    QTreeWidget* m_featureTree;
    QComboBox* m_presetCombo;
    QLabel* m_totalFeaturesLabel;
    QLabel* m_enabledFeaturesLabel;
    QLabel* m_totalMemoryLabel;
    QLabel* m_cpuUsageLabel;
    QProgressBar* m_memoryBar;
    QProgressBar* m_cpuBar;
    QTextEdit* m_featureDetails;
    QPushButton* m_applyButton;
    QPushButton* m_resetButton;
    QPushButton* m_exportButton;
    QPushButton* m_importButton;
    QPushButton* m_hotReloadButton;
    
    // State
    MasmFeatureManager* m_manager;
    QTreeWidgetItem* m_selectedItem;
    int m_totalFeatures;
    int m_enabledFeatures;
    qint64 m_totalMemoryKB;
    int m_totalCPUPercent;
};
