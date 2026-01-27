/**
 * @file ThermalDashboardWidget.cpp
 * @brief Qt widget for live NVMe thermal visualization
 *
 * Displays:
 * - 5 temperature bars (blue=cool, orange=warm, red=hot)
 * - Current tier (70B/120B/800B)
 * - TurboSparse skip percentage
 * - PowerInfer GPU/CPU split
 *
 * Refreshes every 1 second from pocket_lab_turbo.dll
 */
#include "ThermalDashboardWidget.h"
#include <QPainter>
#include <QVBoxLayout>
#include <QDebug>
#include <cmath>

#ifdef _WIN32
#include <windows.h>
#else
// Stub for non-Windows
#define LoadLibraryA(x) nullptr
#define GetProcAddress(h, n) nullptr
#define FreeLibrary(h)
typedef void* HMODULE;
#endif

// ThermalSnapshot struct (must match DLL)
#pragma pack(push, 1)
struct ThermalSnapshot {
    double t0, t1, t2, t3, t4;
    unsigned int tier;
    unsigned int sparseSkipPct;
    unsigned int gpuSplit;
};
#pragma pack(pop)

// Colors
static const QColor COLOR_COOL(0, 180, 255);      // Blue - < 40°C
static const QColor COLOR_WARM(255, 180, 0);      // Orange - 40-55°C
static const QColor COLOR_HOT(255, 60, 60);       // Red - > 55°C
static const QColor COLOR_BG(30, 30, 35);         // Dark background
static const QColor COLOR_TEXT(220, 220, 220);    // Light text
static const QColor COLOR_ACCENT(100, 200, 255);  // Accent blue

ThermalDashboardWidget::ThermalDashboardWidget(QWidget* parent)
    : QWidget(parent)
    , m_timer(new QTimer(this))
    , m_dllLoaded(false)
    , m_hDll(nullptr)
    , m_pfnInit(nullptr)
    , m_pfnGetThermal(nullptr)
    , m_tier(0)
    , m_sparseSkipPct(0)
    , m_gpuSplit(100)
    , m_barWidth(50)
    , m_barMaxHeight(100)
{
    // Initialize temps to defaults
    for (int i = 0; i < 5; ++i) {
        m_temps[i] = 35.0;
    }

    setMinimumSize(320, 200);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setAutoFillBackground(true);

    // Dark palette
    QPalette pal = palette();
    pal.setColor(QPalette::Window, COLOR_BG);
    setPalette(pal);

    // Load DLL
    loadDll();

    // Timer for refresh
    connect(m_timer, &QTimer::timeout, this, &ThermalDashboardWidget::onTimerTick);
    m_timer->start(1000);  // 1 second refresh

    // Initial refresh
    refresh();
}

ThermalDashboardWidget::~ThermalDashboardWidget()
{
    if (m_hDll) {
#ifdef _WIN32
        FreeLibrary(static_cast<HMODULE>(m_hDll));
#endif
    }
}

void ThermalDashboardWidget::loadDll()
{
#ifdef _WIN32
    // Try multiple paths
    const char* paths[] = {
        "pocket_lab_turbo.dll",
        "bin/pocket_lab_turbo.dll",
        "../pocket_lab_turbo.dll",
        "D:/rawrxd/build/pocket_lab_turbo.dll"
    };

    for (const char* path : paths) {
        m_hDll = LoadLibraryA(path);
        if (m_hDll) {
            qDebug() << "[ThermalDashboard] Loaded DLL from:" << path;
            break;
        }
    }

    if (!m_hDll) {
        qWarning() << "[ThermalDashboard] Failed to load pocket_lab_turbo.dll";
        return;
    }

    // Get function pointers
    m_pfnInit = reinterpret_cast<PFN_Init>(
        GetProcAddress(static_cast<HMODULE>(m_hDll), "PocketLabInit"));
    m_pfnGetThermal = reinterpret_cast<PFN_GetThermal>(
        GetProcAddress(static_cast<HMODULE>(m_hDll), "PocketLabGetThermal"));

    if (!m_pfnInit || !m_pfnGetThermal) {
        qWarning() << "[ThermalDashboard] Failed to get DLL exports";
        FreeLibrary(static_cast<HMODULE>(m_hDll));
        m_hDll = nullptr;
        return;
    }

    // Initialize
    int result = m_pfnInit();
    if (result == 0) {
        m_dllLoaded = true;
        qDebug() << "[ThermalDashboard] Pocket-Lab initialized successfully";
    } else {
        qWarning() << "[ThermalDashboard] Pocket-Lab init failed:" << result;
    }
#else
    qWarning() << "[ThermalDashboard] DLL loading only supported on Windows";
#endif
}

void ThermalDashboardWidget::refresh()
{
    if (!m_dllLoaded || !m_pfnGetThermal) {
        // Use MMF fallback - read directly
        // (This allows widget to work even without DLL)
#ifdef _WIN32
        HANDLE hMMF = OpenFileMappingA(FILE_MAP_READ, FALSE, "Global\\SOVEREIGN_NVME_TEMPS");
        if (hMMF) {
            void* pView = MapViewOfFile(hMMF, FILE_MAP_READ, 0, 0, 160);
            if (pView) {
                unsigned int* data = static_cast<unsigned int*>(pView);
                if (data[0] == 0x534F5645) {  // "SOVE" signature
                    int* temps = reinterpret_cast<int*>(data + 4);  // offset 0x10
                    for (int i = 0; i < 5; ++i) {
                        m_temps[i] = static_cast<double>(temps[i]);
                    }
                }
                UnmapViewOfFile(pView);
            }
            CloseHandle(hMMF);
        }
#endif
        update();
        return;
    }

    // Get thermal snapshot from DLL
    ThermalSnapshot snap{};
    m_pfnGetThermal(&snap);

    m_temps[0] = snap.t0;
    m_temps[1] = snap.t1;
    m_temps[2] = snap.t2;
    m_temps[3] = snap.t3;
    m_temps[4] = snap.t4;
    m_tier = snap.tier;
    m_sparseSkipPct = snap.sparseSkipPct;
    m_gpuSplit = snap.gpuSplit;

    // Check for warnings
    int maxTemp = 0;
    for (int i = 0; i < 5; ++i) {
        int t = static_cast<int>(m_temps[i]);
        if (t > maxTemp) maxTemp = t;
        if (t >= TEMP_CRIT) {
            emit thermalWarning(i, t);
        }
    }

    emit thermalStateChanged(m_tier, maxTemp);

    update();  // Trigger repaint
}

void ThermalDashboardWidget::onTimerTick()
{
    refresh();
}

void ThermalDashboardWidget::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    const int w = width();
    const int h = height();
    const int margin = 10;
    const int headerHeight = 60;
    const int barArea = h - headerHeight - margin * 2;

    // Background
    p.fillRect(rect(), COLOR_BG);

    // Header section
    QFont headerFont = font();
    headerFont.setPointSize(10);
    headerFont.setBold(true);
    p.setFont(headerFont);
    p.setPen(COLOR_TEXT);

    // Tier label
    QString tierStr = QString("Tier: %1 (%2)").arg(m_tier).arg(tierName(m_tier));
    p.drawText(margin, margin + 15, tierStr);

    // TurboSparse
    QString sparseStr = QString("TurboSparse Skip: %1%").arg(m_sparseSkipPct);
    p.drawText(margin, margin + 32, sparseStr);

    // PowerInfer
    QString gpuStr = QString("PowerInfer: GPU %1% / CPU %2%")
                     .arg(m_gpuSplit).arg(100 - m_gpuSplit);
    p.drawText(margin, margin + 49, gpuStr);

    // Connection status indicator
    QColor statusColor = m_dllLoaded ? QColor(0, 200, 100) : QColor(200, 100, 0);
    p.setBrush(statusColor);
    p.setPen(Qt::NoPen);
    p.drawEllipse(w - margin - 12, margin + 5, 10, 10);

    // Temperature bars
    const int barCount = 5;
    const int barSpacing = 8;
    const int totalBarWidth = (w - margin * 2 - barSpacing * (barCount - 1)) / barCount;
    const int barY = headerHeight + margin;

    QFont barFont = font();
    barFont.setPointSize(9);
    p.setFont(barFont);

    for (int i = 0; i < barCount; ++i) {
        double temp = m_temps[i];
        int x = margin + i * (totalBarWidth + barSpacing);

        // Calculate bar height (30°C = 0, 70°C = full)
        double normalizedTemp = (temp - 30.0) / 40.0;
        normalizedTemp = qBound(0.0, normalizedTemp, 1.0);
        int barHeight = static_cast<int>(normalizedTemp * barArea * 0.7);

        // Bar background
        QRect barBg(x, barY, totalBarWidth, barArea - 20);
        p.fillRect(barBg, QColor(50, 50, 55));

        // Temperature bar
        QColor barColor = tempColor(temp);
        QRect barRect(x + 2, barY + barArea - 20 - barHeight - 2,
                      totalBarWidth - 4, barHeight);
        p.fillRect(barRect, barColor);

        // Gradient overlay for depth
        QLinearGradient grad(barRect.topLeft(), barRect.topRight());
        grad.setColorAt(0, QColor(255, 255, 255, 40));
        grad.setColorAt(0.5, QColor(255, 255, 255, 0));
        grad.setColorAt(1, QColor(0, 0, 0, 40));
        p.fillRect(barRect, grad);

        // Temperature label
        p.setPen(COLOR_TEXT);
        QString tempStr = QString("%1°").arg(temp, 0, 'f', 1);
        QRect labelRect(x, barY + barArea - 18, totalBarWidth, 16);
        p.drawText(labelRect, Qt::AlignCenter, tempStr);

        // Drive label
        p.setPen(COLOR_ACCENT);
        QString driveStr = QString("D%1").arg(i);
        QRect driveRect(x, barY - 2, totalBarWidth, 14);
        p.drawText(driveRect, Qt::AlignCenter, driveStr);
    }
}

void ThermalDashboardWidget::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    updateLayout();
}

void ThermalDashboardWidget::updateLayout()
{
    m_barWidth = (width() - 60) / 5;
    m_barMaxHeight = height() - 100;
}

QString ThermalDashboardWidget::tierName(unsigned int tier) const
{
    switch (tier) {
        case 0: return "70B Mobile";
        case 1: return "120B Workstation";
        case 2: return "800B Enterprise";
        default: return "Unknown";
    }
}

QColor ThermalDashboardWidget::tempColor(double tempC) const
{
    if (tempC < 40.0) return COLOR_COOL;
    if (tempC < 55.0) return COLOR_WARM;
    return COLOR_HOT;
}
