#pragma once
#include <string>

// Unified enums
enum DefaultCompiler { COMPILER_MSVC, COMPILER_GPP };
enum UITheme { THEME_LIGHT, THEME_DARK, THEME_SYSTEM };

// Unified ProjectConfig
struct ProjectConfig {
    bool overrideCompiler = false;
    DefaultCompiler compiler = COMPILER_MSVC;
    bool overrideTheme = false;
    UITheme theme = THEME_SYSTEM;
    bool overrideGraphZoom = false;
    float graphZoom = 1.0f;
    bool propagateSettings = false;
};

struct IDEConfig {
    bool autoClearTrash = true;
    bool askRestoreTrash = true;
    bool autoSaveReports = true;
    DefaultCompiler defaultCompiler = COMPILER_MSVC;
    float defaultGraphZoom = 1.0f;
    UITheme theme = THEME_SYSTEM;
};

struct SolutionConfig {
    bool overrideCompiler = false;
    DefaultCompiler compiler = COMPILER_MSVC;
    bool overrideTheme = false;
    UITheme theme = THEME_SYSTEM;
    bool overrideGraphZoom = false;
    float graphZoom = 1.0f;
};

extern IDEConfig gConfig;
extern SolutionConfig gSolutionConfig;

// Unified function signatures
bool LoadProjectConfig(const std::wstring& projectName, ProjectConfig& cfg);
bool SaveProjectConfig(const std::wstring& projectName, const ProjectConfig& cfg);
void SaveConfig(const std::wstring& path);
void LoadConfig(const std::wstring& path);
void SaveSolutionConfig(const std::wstring& path);
void LoadSolutionConfig(const std::wstring& path);