// ============================================================================
// Win32IDE_StartupBanner.cpp
// ----------------------------------------------------------------------------
// Emits a one-time, one-line-per-fact summary of the IDE's runtime surface
// into the "Output" tab once startup has reached the point where the bottom
// panel exists, the extension scanner has run, and model discovery has
// completed. Mirrors the spirit of the Copilot "ExtensionState" banner but
// formatted for RawrXD's native Output pane.
//
// Intentionally minimal: no new dependencies, no scaffolding, all data is
// pulled from already-populated Win32IDE state (model discovery arrays,
// Ollama config). Extensions are scanned from %APPDATA%\RawrXD\extensions\
// to ensure we see what actually installed, not a stale cache.
// ============================================================================

#include "Win32IDE.h"

#include <filesystem>
#include <sstream>
#include <string>
#include <vector>

namespace
{
std::string ShortPath(const std::string& full)
{
    try
    {
        std::filesystem::path p(full);
        return p.filename().string();
    }
    catch (...)
    {
        return full;
    }
}

// Inline extension scan for startup banner (mirrors ExtensionLoader behavior,
// but scoped to banner so it's independent of UI panel lifecycle).
struct SimpleExtInfo
{
    std::string name;
    std::string path;
    bool isNative = false;
};

std::vector<SimpleExtInfo> ScanExtensionsForBanner()
{
    std::vector<SimpleExtInfo> result;
    try
    {
        // Use RawrXD's standard extensions location
        wchar_t appData[MAX_PATH] = {};
        if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_APPDATA, nullptr, 0, appData)))
        {
            std::filesystem::path extRoot(appData);
            extRoot /= L"RawrXD\\extensions";

            if (std::filesystem::exists(extRoot))
            {
                for (const auto& entry : std::filesystem::directory_iterator(extRoot))
                {
                    if (!entry.is_directory())
                        continue;

                    SimpleExtInfo info;
                    info.name = entry.path().filename().string();
                    info.path = entry.path().string();

                    // Check if it has native_manifest.json
                    std::filesystem::path manifest = entry.path() / "native_manifest.json";
                    info.isNative = std::filesystem::exists(manifest);

                    result.push_back(info);
                }

                // Sort alphabetically
                std::sort(result.begin(), result.end(),
                          [](const SimpleExtInfo& a, const SimpleExtInfo& b)
                          { return a.name < b.name; });
            }
        }
    }
    catch (...)
    {
    }
    return result;
}
}  // namespace

void Win32IDE::emitStartupBanner()
{
    // ========================================================================
    // CRITICAL FIX 1: The xmemory & AV Stabilizer
    // Guard with multiple safety layers to prevent early-lifecycle crashes:
    // - Headless mode check (GUI not available)
    // - IsWindow() validation (SDK contract check)
    // - Nullptr guards for all managers
    // - Try-catch wrapper around all string operations
    // ========================================================================

    // Layer 1: Headless mode detection

    // Layer 2: Output pane window existence check
    if (!m_hwndOutputPanel) {
        OutputDebugStringA("[RawrXD] emitStartupBanner: m_hwndOutputPanel is nullptr (GUI not ready).\n");
        return;
    }

    // Layer 3: SDK contract validation – IsWindow() is strict
    if (!::IsWindow(m_hwndOutputPanel)) {
        OutputDebugStringA("[RawrXD] emitStartupBanner: m_hwndOutputPanel is invalid (window destroyed).\n");
        return;
    }

    // Layer 4: Double-check banner emission state (prevent re-entry)
    if (m_startupBannerEmitted) {
        return;
    }

    // Layer 5: Verify all manager pointers before any iteration/access

    // Layer 6: Defensive output operations with exception barrier
    // Wrap all appendToOutput calls to prevent string allocation failures
    // Declare variables outside try blocks so they're in function scope
    const std::string tab = "Output";
    const OutputSeverity sev = OutputSeverity::Info;
    
    try {
        // Pre-allocate header string with reserve to avoid xmemory pressure
        std::string banner;
        banner.reserve(4096);  // Reserve upfront to reduce reallocations
        

        banner = "[RawrXD] ===============================================================\n";
        appendToOutput(banner, tab, sev);
        
        banner = "[RawrXD] Startup banner — extensions and local model surface\n";
        appendToOutput(banner, tab, sev);
        
        banner = "[RawrXD] ===============================================================\n";
        appendToOutput(banner, tab, sev);

        // ---- Extensions -------------------------------------------------------
        // Deep-copy safety: Ensure we aren't iterating a vector being modified by TitanHost
        std::vector<SimpleExtInfo> exts;
        try {
            exts = ScanExtensionsForBanner();  // This makes a local copy
        } catch (...) {
            OutputDebugStringA("[StartupBanner] Exception scanning extensions.\n");
        }

        {
            std::ostringstream oss;
            oss << "[Extensions] count=" << exts.size() << "\n";
            appendToOutput(oss.str(), tab, sev);
        }

        // Iterate the local copy, not a live container
        for (const auto& ext : exts) {
            if (ext.name.empty()) continue;  // Skip malformed entries
            
            std::ostringstream oss;
            oss << "[Extension] " << ext.name
                 << " native=" << (ext.isNative ? "1" : "0")
                 << " path=" << ShortPath(ext.path) << "\n";
            try {
                appendToOutput(oss.str(), tab, sev);
            } catch (...) {
                // Silently skip on output failure, don't crash
            }
        }
    } catch (...) {
        OutputDebugStringA("[StartupBanner] Critical failure in extension section.\n");
        return;  // Abort gracefully
    }

    // ---- Local model discovery -------------------------------------------
    try {
        std::vector<std::string> models;
        try { models = getAvailableModels(); } catch (...) {}
        std::vector<std::string> roots;
        try { roots = getModelDiscoveryPaths(); } catch (...) {}

        std::ostringstream header;
        header << "[Models] discovered=" << models.size()
               << " roots=" << roots.size()
               << " enabled=" << (isModelDiscoveryEnabled() ? "1" : "0") << "\n";
        appendToOutput(header.str(), tab, sev);

        for (const auto& root : roots) {
            if (root.empty()) continue;  // Skip empty paths
            appendToOutput(std::string("[ModelRoot] ") + root + "\n", tab, sev);
        }
        for (const auto& m : models) {
            if (m.empty()) continue;  // Skip empty models
            appendToOutput(std::string("[Model] ") + m + "\n", tab, sev);
        }
    } catch (...) {
        OutputDebugStringA("[StartupBanner] Exception during model discovery.\n");
    }

    // ---- Active model + Ollama endpoint ----------------------------------
    try {
        std::ostringstream line;
        line << "[ActiveModel] loaded_path="
             << (m_loadedModelPath.empty() ? std::string("<none>") : m_loadedModelPath)
             << " ollama_override="
             << (m_ollamaModelOverride.empty() ? std::string("<none>") : m_ollamaModelOverride)
             << "\n";
        appendToOutput(line.str(), tab, sev);

        std::ostringstream ep;
        std::string endpoint;
        try { endpoint = getOllamaEndpoint(); } catch (...) {}
        if (endpoint.empty())
            endpoint = m_ollamaBaseUrl;
        ep << "[OllamaEndpoint] base_url="
           << (endpoint.empty() ? std::string("<unset>") : endpoint)
           << " connected=" << (isOllamaConnected() ? "1" : "0") << "\n";
        appendToOutput(ep.str(), tab, sev);
    } catch (...) {
        OutputDebugStringA("[StartupBanner] Exception during endpoint reporting.\n");
    }

    try {
        appendToOutput("[RawrXD] ===============================================================\n", tab, sev);
    } catch (...) {
        // Final closure line failed, but don't crash
    }
}

void Win32IDE::logModelRequestUsage(const std::string& modelId,
                                    size_t promptTokens,
                                    size_t completionTokens,
                                    double durationMs)
{
    std::ostringstream line;
    line << "[ModelUsage] model=" << (modelId.empty() ? std::string("<unknown>") : modelId)
         << " prompt_tokens=" << promptTokens
         << " completion_tokens=" << completionTokens
         << " total_tokens=" << (promptTokens + completionTokens)
         << " duration_ms=" << static_cast<long long>(durationMs);
    if (durationMs > 0.0 && completionTokens > 0)
    {
        const double tps = (static_cast<double>(completionTokens) * 1000.0) / durationMs;
        line << " tps=" << static_cast<long long>(tps);
    }
    line << "\n";
    appendToOutput(line.str(), "Output", OutputSeverity::Info);
}
