#pragma once
#include <windows.h>
#include <string>

class SovereignDashboardWidget {
public:
    SovereignDashboardWidget();
    ~SovereignDashboardWidget();

    void updateDashboard();

private:
    HANDLE m_mmfHandle;
    void* m_mmfView;
};

