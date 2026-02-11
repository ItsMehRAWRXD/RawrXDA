#include "Config.h"
#include <fstream>
#include <sstream>

IDEConfig gConfig;
SolutionConfig gSolutionConfig;

static void WriteKey(std::wofstream& out, const std::wstring& key, const std::wstring& val) {
    out << key << L"=" << val << L"\n";
}

static std::wstring ReadVal(const std::wstring& line, const std::wstring& key) {
    if (line.find(key + L"=") == 0) return line.substr(key.size() + 1);
    return L"";
}

// ==== Global IDE config ====
void SaveConfig(const std::wstring& path) {
    std::wofstream out(path);
    if (!out.is_open()) return;
    WriteKey(out,L"autoClearTrash", gConfig.autoClearTrash?L"1":L"0");
    WriteKey(out,L"askRestoreTrash", gConfig.askRestoreTrash?L"1":L"0");
    WriteKey(out,L"autoSaveReports", gConfig.autoSaveReports?L"1":L"0");
    WriteKey(out,L"defaultCompiler", gConfig.defaultCompiler==COMPILER_MSVC?L"MSVC":L"GPP");
    WriteKey(out,L"defaultGraphZoom", std::to_wstring(gConfig.defaultGraphZoom));
    WriteKey(out,L"theme", gConfig.theme==THEME_LIGHT?L"Light":gConfig.theme==THEME_DARK?L"Dark":L"System");
}

void LoadConfig(const std::wstring& path) {
    std::wifstream in(path);
    if (!in.is_open()) return;
    std::wstring line;
    while (std::getline(in,line)) {
        auto v=ReadVal(line,L"autoClearTrash"); if(!v.empty()) gConfig.autoClearTrash=(v==L"1");
        v=ReadVal(line,L"askRestoreTrash"); if(!v.empty()) gConfig.askRestoreTrash=(v==L"1");
        v=ReadVal(line,L"autoSaveReports"); if(!v.empty()) gConfig.autoSaveReports=(v==L"1");
        v=ReadVal(line,L"defaultCompiler"); if(!v.empty()) gConfig.defaultCompiler=(v==L"MSVC")?COMPILER_MSVC:COMPILER_GPP;
        v=ReadVal(line,L"defaultGraphZoom"); if(!v.empty()) gConfig.defaultGraphZoom=std::stof(v);
        v=ReadVal(line,L"theme"); if(!v.empty()) {
            if(v==L"Light") gConfig.theme=THEME_LIGHT;
            else if(v==L"Dark") gConfig.theme=THEME_DARK;
            else gConfig.theme=THEME_SYSTEM;
        }
    }
}

// ==== Solution config ====
void SaveSolutionConfig(const std::wstring& path) {
    std::wofstream out(path);
    if (!out.is_open()) return;
    WriteKey(out,L"overrideCompiler", gSolutionConfig.overrideCompiler?L"1":L"0");
    WriteKey(out,L"compiler", gSolutionConfig.compiler==COMPILER_MSVC?L"MSVC":L"GPP");
    WriteKey(out,L"overrideTheme", gSolutionConfig.overrideTheme?L"1":L"0");
    WriteKey(out,L"theme", gSolutionConfig.theme==THEME_LIGHT?L"Light":gSolutionConfig.theme==THEME_DARK?L"Dark":L"System");
    WriteKey(out,L"overrideGraphZoom", gSolutionConfig.overrideGraphZoom?L"1":L"0");
    WriteKey(out,L"graphZoom", std::to_wstring(gSolutionConfig.graphZoom));
}

void LoadSolutionConfig(const std::wstring& path) {
    std::wifstream in(path);
    if (!in.is_open()) return;
    std::wstring line;
    while (std::getline(in,line)) {
        auto v=ReadVal(line,L"overrideCompiler"); if(!v.empty()) gSolutionConfig.overrideCompiler=(v==L"1");
        v=ReadVal(line,L"compiler"); if(!v.empty()) gSolutionConfig.compiler=(v==L"MSVC")?COMPILER_MSVC:COMPILER_GPP;
        v=ReadVal(line,L"overrideTheme"); if(!v.empty()) gSolutionConfig.overrideTheme=(v==L"1");
        v=ReadVal(line,L"theme"); if(!v.empty()) {
            if(v==L"Light") gSolutionConfig.theme=THEME_LIGHT;
            else if(v==L"Dark") gSolutionConfig.theme=THEME_DARK;
            else gSolutionConfig.theme=THEME_SYSTEM;
        }
        v=ReadVal(line,L"overrideGraphZoom"); if(!v.empty()) gSolutionConfig.overrideGraphZoom=(v==L"1");
        v=ReadVal(line,L"graphZoom"); if(!v.empty()) gSolutionConfig.graphZoom=std::stof(v);
    }
}

// ==== Project config ====
bool SaveProjectConfig(const std::wstring& projectName, const ProjectConfig& cfg) {
    std::wofstream out(projectName+L".projconfig.ini");
    if (!out.is_open()) return false;
    out << L"overrideCompiler=" << (cfg.overrideCompiler?L"1":L"0") << L"\n";
    out << L"compiler=" << (cfg.compiler==COMPILER_MSVC?L"MSVC":L"GPP") << L"\n";
    out << L"overrideTheme=" << (cfg.overrideTheme?L"1":L"0") << L"\n";
    out << L"theme=" << (cfg.theme==THEME_LIGHT?L"Light":cfg.theme==THEME_DARK?L"Dark":L"System") << L"\n";
    out << L"overrideGraphZoom=" << (cfg.overrideGraphZoom?L"1":L"0") << L"\n";
    out << L"graphZoom=" << std::to_wstring(cfg.graphZoom) << L"\n";
    return true;
}

bool LoadProjectConfig(const std::wstring& projectName, ProjectConfig& cfg) {
    std::wifstream in(projectName+L".projconfig.ini");
    if (!in.is_open()) return false;
    std::wstring line;
    while (std::getline(in,line)) {
        if(line.find(L"overrideCompiler=") == 0) cfg.overrideCompiler=(line.substr(17)==L"1");
        if(line.find(L"compiler=") == 0) cfg.compiler=(line.substr(9)==L"MSVC")?COMPILER_MSVC:COMPILER_GPP;
        if(line.find(L"overrideTheme=") == 0) cfg.overrideTheme=(line.substr(14)==L"1");
        if(line.find(L"theme=") == 0) {
            std::wstring v=line.substr(6);
            if(v==L"Light") cfg.theme=THEME_LIGHT;
            else if(v==L"Dark") cfg.theme=THEME_DARK;
            else cfg.theme=THEME_SYSTEM;
        }
        if(line.find(L"overrideGraphZoom=") == 0) cfg.overrideGraphZoom=(line.substr(18)==L"1");
        if(line.find(L"graphZoom=") == 0) cfg.graphZoom=std::stof(line.substr(10));
    }
    return true;
}