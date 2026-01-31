#pragma once
#include <windows.h>
#include <string>
#include "Config.h"

// Only declare functions, do not redefine ProjectConfig
bool LoadProjectConfig(const std::wstring& projectName, ProjectConfig& cfg);
bool SaveProjectConfig(const std::wstring& projectName, const ProjectConfig& cfg);
void ShowProjectSettings(HWND parent, const std::wstring& projectName, ProjectConfig& cfg);
