/**
 * @file ThermalDashboardWidget.h
 * @brief Qt widget displaying live NVMe thermal data + inference stats
 *
 * Connects to Pocket-Lab Turbo DLL for:
 * - Real-time NVMe temperatures (5 drives)
 * - Auto-detected tier (70B/120B/800B)
 * - TurboSparse skip percentage
 * - PowerInfer GPU/CPU split
 */
#pragma once

#include <QWidget>
#include <QTimer>
#include <QLabel>

class ThermalDashboardWidget : public QWidget {
    Q_OBJECT

public:
    explicit ThermalDashboardWidget(QWidget* parent = nullptr);
    ~ThermalDashboardWidget();

    /// Check if DLL is loaded and functional
    bool isConnected() const { return m_dllLoaded; }

    /// Force immediate refresh
    void refresh();

signals:
    /// Emitted when thermal state changes significantly
    void thermalStateChanged(int tier, int maxTempC);

    /// Emitted when a drive exceeds threshold
    void thermalWarning(int driveIndex, int tempC);

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private slots:
    void onTimerTick();

private:
    void loadDll();
    void updateLayout();
    QString tierName(unsigned int tier) const;
    QColor tempColor(double tempC) const;

    // Timer for periodic refresh
    QTimer* m_timer;

    // DLL state
    bool m_dllLoaded;
    void* m_hDll;

    // Function pointers
    typedef int (__stdcall *PFN_Init)(void);
    typedef void (__stdcall *PFN_GetThermal)(void*);

    PFN_Init m_pfnInit;
    PFN_GetThermal m_pfnGetThermal;

    // Cached thermal data
    double m_temps[5];
    unsigned int m_tier;
    unsigned int m_sparseSkipPct;
    unsigned int m_gpuSplit;

    // UI state
    int m_barWidth;
    int m_barMaxHeight;

    // Thresholds
    static constexpr int TEMP_WARN = 45;
    static constexpr int TEMP_CRIT = 55;
};
