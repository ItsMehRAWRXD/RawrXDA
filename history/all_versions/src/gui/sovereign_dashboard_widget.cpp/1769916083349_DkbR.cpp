#include "sovereign_dashboard_widget.h"
#include <string>

struct SovereignStatsBlock {
    unsigned int magic;
    double tokensPerSec;
    int driveTemps[8];
    double skipRate;
};
#define SOVEREIGN_STATS_MAGIC 0xFEEDBEEF

SovereignDashboardWidget::SovereignDashboardWidget() 
    : m_mmfHandle(NULL), m_mmfView(NULL) 
{
}

SovereignDashboardWidget::~SovereignDashboardWidget() {
    if (m_mmfView) UnmapViewOfFile(m_mmfView);
    if (m_mmfHandle) CloseHandle(m_mmfHandle);
}

void SovereignDashboardWidget::updateDashboard() {
    if (!m_mmfView) {
        m_mmfHandle = OpenFileMappingA(FILE_MAP_READ, FALSE, "Global\\SOVEREIGN_STATS");
        if (m_mmfHandle) {
            m_mmfView = MapViewOfFile(m_mmfHandle, FILE_MAP_READ, 0, 0, sizeof(SovereignStatsBlock));
        }
        return;
    }
    
    // In a real GUI, this would update text/bars. 
    // For now we just read the data to ensure logic is valid.
    auto* stats = reinterpret_cast<const SovereignStatsBlock*>(m_mmfView);
    if (stats->magic == SOVEREIGN_STATS_MAGIC) {
        // Valid data available
        // float tps = stats->tokensPerSec;
        // int temp = stats->driveTemps[0];
    }
}
