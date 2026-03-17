#pragma once

#include <QtWidgets/QDockWidget>
#include <QtWidgets/QSlider>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QLabel>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QTableWidget>
#include <memory>

namespace mem {
    class MemorySettings;
    class EnterpriseMemoryCatalog;
}

/**
 * @class MemoryPanel
 * @brief User-facing memory configuration panel (dock widget)
 * 
 * Features:
 * - Context token slider (4K → 1M)
 * - GPU KV cache toggle
 * - Chat compression toggle
 * - Long-term memory control
 * - Index workspace button
 * - Memory facts browser with delete
 * - Stats display (usage, facts count, etc.)
 */
class MemoryPanel : public QDockWidget {
    Q_OBJECT

public:
    explicit MemoryPanel(mem::MemorySettings* settings,
                        mem::EnterpriseMemoryCatalog* catalog,
                        QWidget* parent = nullptr);
    ~MemoryPanel();

private slots:
    void onContextSliderChanged(int value);
    void onGpuKvToggled(bool checked);
    void onCompressChatToggled(bool checked);
    void onLongTermMemoryToggled(bool checked);
    void onIndexWorkspaceClicked();
    void onClearMemoryClicked();
    void onRefreshStatsClicked();
    void onDeleteFact();

private:
    void createUI();
    void updateStats();
    void updateFactsList();
    void connectSignals();

    mem::MemorySettings* m_settings = nullptr;
    mem::EnterpriseMemoryCatalog* m_catalog = nullptr;

    // UI Elements
    QSlider* m_contextSlider = nullptr;
    QLabel* m_contextLabel = nullptr;
    QLabel* m_vramEstimate = nullptr;
    QCheckBox* m_gpuKvCheckbox = nullptr;
    QCheckBox* m_compressChatCheckbox = nullptr;
    QCheckBox* m_longTermMemoryCheckbox = nullptr;
    QPushButton* m_indexButton = nullptr;
    QPushButton* m_clearButton = nullptr;
    QPushButton* m_refreshButton = nullptr;
    QTableWidget* m_factsTable = nullptr;
    QLabel* m_statsLabel = nullptr;
};
