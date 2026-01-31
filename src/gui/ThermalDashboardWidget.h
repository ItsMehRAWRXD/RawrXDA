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


class ThermalDashboardWidget : public void {

public:
    explicit ThermalDashboardWidget(void* parent = nullptr);
    ~ThermalDashboardWidget();

    /// Check if DLL is loaded and functional
    bool isConnected() const { return m_dllLoaded; }

    /// Force immediate refresh
    void refresh();


    /// Emitted when thermal state changes significantly
    void thermalStateChanged(int tier, int maxTempC);

    /// Emitted when a drive exceeds threshold
    void thermalWarning(int driveIndex, int tempC);

protected:
    void paintEvent(void*  event) override;
    void resizeEvent(void*  event) override;

private:
    void onTimerTick();

private:
    void loadDll();
    void updateLayout();
    std::string tierName(unsigned int tier) const;
    uint32_t tempColor(double tempC) const;

    // Timer for periodic refresh
    void** m_timer;

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

