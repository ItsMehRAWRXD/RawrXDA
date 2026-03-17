// ============================================================================
// vscext_registry.cpp — Core VS Code Extension API registry (Batch #N)
// ============================================================================
// Real implementations for vscext.* (10002, 10003, 10004, 10005, 10006, 10001, 10009).
// SSOT and Win32IDE both call these; no delegateToGui-only behavior.
// ============================================================================

#include "vscext_registry.h"
#include "../modules/vscode_extension_api.h"

#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>

#ifdef _WIN32
#include <direct.h>
#include <windows.h>
#endif

namespace
{

static void (*s_reloadCallback)() = nullptr;

static const char* s_providerNames[] = {"CompletionItem",  "Hover",         "Definition",    "References",
                                        "DocumentSymbol",  "CodeAction",    "CodeLens",      "DocumentFormatting",
                                        "RangeFormatting", "Rename",        "SignatureHelp", "FoldingRange",
                                        "DocumentLink",    "ColorProvider", "InlayHints",    "TypeDefinition",
                                        "Implementation",  "Declaration",   "SemanticTokens"};
static const int s_providerCount = static_cast<int>(ProviderType::Count);

}  // namespace

namespace VscextRegistry
{

void setReloadCallback(void (*fn)())
{
    s_reloadCallback = fn;
}

bool listCommands(std::string& out)
{
    vscode::VSCodeExtensionAPI& api = vscode::VSCodeExtensionAPI::instance();
    if (!api.isInitialized())
    {
        out += "[VscextRegistry] Extension API not initialized. Start IDE with extensions to list commands.\n";
        return false;
    }
    std::vector<std::string> ids;
    api.getCommandIds(ids);
    std::ostringstream ss;
    ss << "[VSCode Extension API] Registered Commands: " << ids.size() << "\n";
    for (const auto& id : ids)
        ss << "  " << id << "\n";
    out += ss.str();
    return !ids.empty();
}

bool listProviders(std::string& out)
{
    vscode::VSCodeExtensionAPI& api = vscode::VSCodeExtensionAPI::instance();
    if (!api.isInitialized())
    {
        out += "[VscextRegistry] Extension API not initialized.\n";
        return false;
    }
    std::ostringstream ss;
    ss << "[VSCode Extension API] Provider Counts:\n";
    for (int i = 0; i < s_providerCount; ++i)
    {
        size_t n = api.getProviderCount(static_cast<ProviderType>(i));
        if (n > 0)
            ss << "  " << s_providerNames[i] << ": " << n << "\n";
    }
    out += ss.str();
    return true;
}

bool getDiagnosticsReport(std::string& out)
{
    vscode::VSCodeExtensionAPI& api = vscode::VSCodeExtensionAPI::instance();
    if (!api.isInitialized())
    {
        out += "[VscextRegistry] Extension API not initialized.\n";
        return false;
    }
    char buf[1024];
    api.getStatusString(buf, sizeof(buf));
    out += "[VSCode Extension API Diagnostics]\n";
    out += "  host state: running\n";
    out += "  ";
    out += buf;
    out += "\n";
    return true;
}

bool listExtensions(std::string& out)
{
    vscode::VSCodeExtensionAPI& api = vscode::VSCodeExtensionAPI::instance();
    if (!api.isInitialized())
    {
        out += "[VscextRegistry] Extension API not initialized.\n";
        return false;
    }
    char buf[8192];
    api.getExtensionsSummary(buf, sizeof(buf));
    out += "[VSCode Extension API] Extensions:\n";
    out += buf;
    return true;
}

bool getStatsJson(std::string& out)
{
    vscode::VSCodeExtensionAPI& api = vscode::VSCodeExtensionAPI::instance();
    if (!api.isInitialized())
    {
        out += "[VscextRegistry] Extension API not initialized.\n";
        return false;
    }
    char buf[2048];
    api.serializeStatsToJson(buf, sizeof(buf));
    out += "[VSCode Extension API Stats (JSON)]\n";
    out += buf;
    out += "\n";
    return true;
}

bool reload()
{
    if (!s_reloadCallback)
    {
        return false;
    }
    s_reloadCallback();
    return true;
}

bool getStatusString(std::string& out)
{
    vscode::VSCodeExtensionAPI& api = vscode::VSCodeExtensionAPI::instance();
    if (!api.isInitialized())
    {
        out += "[VscextRegistry] Extension API not initialized.\n";
        return false;
    }
    char buf[1024];
    api.getStatusString(buf, sizeof(buf));
    out += "[VSCode Extension API Status]\n";
    out += buf;
    out += "\n";
    return true;
}

bool exportConfig(std::string& outPath, std::string& outSummary)
{
    vscode::VSCodeExtensionAPI& api = vscode::VSCodeExtensionAPI::instance();
    outPath.clear();
    outSummary.clear();
    if (!api.isInitialized())
    {
        outSummary = "Extension API not initialized; nothing to export.";
        return false;
    }
    const char* dir = ".rawrxd";
#ifdef _WIN32
    _mkdir(dir);
#endif
    std::string path = std::string(dir) + "/vscext_export.json";
    std::ofstream f(path);
    if (!f)
    {
        outSummary = "Failed to create .rawrxd/vscext_export.json";
        return false;
    }
    char statsBuf[2048];
    api.serializeStatsToJson(statsBuf, sizeof(statsBuf));
    f << "{\n  \"export\": \"vscext\",\n  \"stats\": ";
    f << statsBuf;
    f << ",\n  \"installRoot\": \"extensions\"\n}\n";
    f.close();
    if (!f.good())
    {
        outSummary = "Failed to write export file.";
        return false;
    }
    outPath = path;
    outSummary = "Exported to " + path;
    return true;
}

}  // namespace VscextRegistry
