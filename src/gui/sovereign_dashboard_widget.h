#pragma once

#include <windows.h>


class SovereignDashboardWidget {public:
    explicit SovereignDashboardWidget(void* parent = nullptr);
    ~SovereignDashboardWidget();

    void attachSharedMemory(const std::string &name);
\nprivate:\n    void updateDashboard();

private:
    // Timer *m_timer;
    HANDLE m_mmfHandle;
    void* m_mmfView;
    
    void *m_lblTokens;
    void *m_lblSkip;
    void *m_barThermal;
};






