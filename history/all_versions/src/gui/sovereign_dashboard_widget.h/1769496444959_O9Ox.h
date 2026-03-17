#ifndef SOVEREIGN_DASHBOARD_WIDGET_H
#define SOVEREIGN_DASHBOARD_WIDGET_H

#include <QWidget>
#include <QLabel>
#include <QProgressBar>
#include <QTableWidget>
#include <QTimer>

#include <array>
#include <cstdint>
#include <memory>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#endif

// Shared memory layout mirrored by MASM and Qt
#pragma pack(push, 1)
struct SovereignStatsBlock {
    uint32_t magic;          // "SOV" marker (0x00564F53)
    uint32_t reserved;       // future use / padding
    uint64_t tokensPerSec;   // throughput
    uint32_t skipRate;       // TurboSparse skip percent
    uint32_t gpuSplit;       // GPU workload percent
    uint32_t cpuSplit;       // CPU workload percent
    uint32_t thermalCount;   // number of valid temps
    int32_t temps[16];       // thermal readings (Celsius)
    uint64_t lastUpdateMs;   // GetTickCount64 at producer
};
#pragma pack(pop)

static_assert(sizeof(SovereignStatsBlock) <= 256, "SovereignStatsBlock should stay small");

class SovereignDashboardWidget : public QWidget {
    Q_OBJECT

public:
    explicit SovereignDashboardWidget(QWidget *parent = nullptr);
    ~SovereignDashboardWidget() override;

    // Opens the shared memory view. Returns false on failure.
    bool attachSharedMemory(const QString &mmfName = QStringLiteral("Global\\SOVEREIGN_STATS"));
    void detachSharedMemory();

public slots:
    void refreshTelemetry();

private:
    void buildUi();
    void applyStats(const SovereignStatsBlock &stats);

private:
#ifdef _WIN32
    HANDLE m_mmfHandle = nullptr;
    void *m_view = nullptr;
#endif
    QTimer *m_pollTimer = nullptr;
    QLabel *m_tokenRate = nullptr;
    QLabel *m_skipRate = nullptr;
    QProgressBar *m_gpuSplitBar = nullptr;
    QProgressBar *m_cpuSplitBar = nullptr;
    QLabel *m_lastUpdate = nullptr;
    QTableWidget *m_tempTable = nullptr;
};

#endif // SOVEREIGN_DASHBOARD_WIDGET_H
