
#pragma once
#include <windows.h>
#include <string>
#include <vector>
#include "Config.h"

struct PropagationEntry {
    std::wstring projName;
    std::wstring compilerChange;
    std::wstring themeChange;
    std::wstring zoomChange;
    bool selected;
    ProjectConfig* targetCfg;
};

extern std::vector<PropagationEntry> gPropPreview;

void ShowPropagationPreview(HWND parent);
