#pragma once
#include <windows.h>
#include <windows.h>
class Tier5Cosmetics {
private:
    int m_lineEndingMode = 0;
    HWND m_hStatusBar = nullptr;
    int m_debugWatchFormat = 0;
    bool m_symbolsInitialized = false;
public:
    void ApplyCosmetics();
};

