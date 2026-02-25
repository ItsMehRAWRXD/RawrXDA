/**
 * @file thermal_plugin_loader.hpp
 * @brief Hot-injection plugin loader for Thermal Dashboard (Qt-free)
 *
 * Enables runtime loading/unloading of thermal_dashboard.dll
 * without IDE restart. Uses Win32 LoadLibrary + named pipe IPC.
 *
 * Pure C++20 / Win32 — zero Qt dependency.
 */

#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

// Forward declare the interface
namespace rawrxd::thermal {
    class IThermalDashboardPlugin;
}

// DLL export signature: IThermalDashboardPlugin* CreateThermalPlugin()
using CreateThermalPluginFunc = rawrxd::thermal::IThermalDashboardPlugin* (*)();

namespace rawrxd::core {

/**
 * @brief Plugin loader status
 */
enum class PluginStatus {
    NotLoaded,
    Loading,
    Loaded,
    Error,
    Unloading
};

// ─────────────────────────────────────────────────────────────────────────────
// Callback types (no Qt signals)
// ─────────────────────────────────────────────────────────────────────────────
using PluginStatusCallback   = std::function<void(PluginStatus)>;
using PluginErrorCallback    = std::function<void(const std::string&)>;
using PluginFileChangedCb    = std::function<void(const std::string&)>;

/**
 * @brief Hot-injectable plugin loader for thermal dashboard
 *
 * Features:
 * - Runtime DLL loading via Win32 LoadLibrary/FreeLibrary
 * - Named pipe IPC for external injection commands
 * - File timestamp polling for auto-reload on DLL update
 * - Thread-safe plugin access
 */
class ThermalPluginLoader {
public:
    ThermalPluginLoader();
    ~ThermalPluginLoader();

    // Non-copyable
    ThermalPluginLoader(const ThermalPluginLoader&) = delete;
    ThermalPluginLoader& operator=(const ThermalPluginLoader&) = delete;

    /**
     * @brief Initialize the plugin loader system
     * @param pluginSearchPaths Directories to search for plugins
     * @return true if initialized successfully
     */
    bool initialize(const std::vector<std::string>& pluginSearchPaths = {});

    /** @brief Shutdown and cleanup */
    void shutdown();

    /**
     * @brief Load the thermal dashboard plugin
     * @param pluginPath Optional explicit path to DLL
     * @return true if loaded successfully
     */
    bool loadPlugin(const std::string& pluginPath = {});

    /** @brief Unload the currently loaded plugin */
    bool unloadPlugin();

    /** @brief Reload the plugin (unload then load) */
    bool reloadPlugin();

    /** @brief Check if plugin is currently loaded */
    bool isLoaded() const;

    /** @brief Get current plugin status */
    PluginStatus status() const { return m_status.load(); }

    /** @brief Get the loaded plugin interface (nullptr if not loaded) */
    rawrxd::thermal::IThermalDashboardPlugin* plugin() const;

    /** @brief Get last error message */
    std::string lastError() const;

    /** @brief Enable/disable auto-reload on DLL changes */
    void setAutoReload(bool enabled);

    /** @brief Enable/disable IPC command server */
    void setIpcEnabled(bool enabled);

    // ── Callbacks (replacement for Qt signals) ──────────────────────────────
    void onPluginLoaded(PluginStatusCallback cb)     { m_cbLoaded = std::move(cb); }
    void onPluginUnloaded(PluginStatusCallback cb)   { m_cbUnloaded = std::move(cb); }
    void onPluginError(PluginErrorCallback cb)       { m_cbError = std::move(cb); }
    void onStatusChanged(PluginStatusCallback cb)    { m_cbStatus = std::move(cb); }
    void onFileChanged(PluginFileChangedCb cb)       { m_cbFileChanged = std::move(cb); }

private:
    // ── Internal helpers ────────────────────────────────────────────────────
    void setStatus(PluginStatus s);
    std::string findPluginPath() const;
    void setupIpcServer();
    void ipcThreadFunc();
    void watcherThreadFunc();
    void handleIpcCommand(const std::string& json, HANDLE pipe);

    // ── State ───────────────────────────────────────────────────────────────
    HMODULE                                   m_hModule = nullptr;
    rawrxd::thermal::IThermalDashboardPlugin* m_plugin  = nullptr;

    std::vector<std::string>                  m_searchPaths;
    std::string                               m_currentPluginPath;
    std::string                               m_lastError;
    std::atomic<PluginStatus>                 m_status{PluginStatus::NotLoaded};

    // Auto-reload (file timestamp polling)
    std::atomic<bool>                         m_autoReloadEnabled{true};
    std::atomic<bool>                         m_watcherRunning{false};
    std::thread                               m_watcherThread;
    HANDLE                                    m_watcherStopEvent = nullptr;
    std::chrono::steady_clock::time_point     m_lastReloadTime;

    // IPC (named pipe)
    std::atomic<bool>                         m_ipcEnabled{true};
    std::atomic<bool>                         m_ipcRunning{false};
    std::thread                               m_ipcThread;
    HANDLE                                    m_ipcStopEvent = nullptr;

    mutable std::mutex                        m_mutex;

    // Callbacks
    PluginStatusCallback                      m_cbLoaded;
    PluginStatusCallback                      m_cbUnloaded;
    PluginErrorCallback                       m_cbError;
    PluginStatusCallback                      m_cbStatus;
    PluginFileChangedCb                       m_cbFileChanged;
};

// ═════════════════════════════════════════════════════════════════════════════
// Inline Implementation
// ═════════════════════════════════════════════════════════════════════════════

inline ThermalPluginLoader::ThermalPluginLoader() = default;

inline ThermalPluginLoader::~ThermalPluginLoader()
{
    shutdown();
}

inline bool ThermalPluginLoader::initialize(const std::vector<std::string>& pluginSearchPaths)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    m_searchPaths = pluginSearchPaths;
    if (m_searchPaths.empty()) {
        // Default search paths — use exe directory
        char exePath[MAX_PATH]{};
        if (GetModuleFileNameA(nullptr, exePath, MAX_PATH)) {
            std::string dir(exePath);
            auto pos = dir.find_last_of("\\/");
            if (pos != std::string::npos) dir = dir.substr(0, pos);
            m_searchPaths.push_back(dir);
            m_searchPaths.push_back(dir + "\\plugins");
        }
        m_searchPaths.push_back("D:\\rawrxd\\build\\bin");
        m_searchPaths.push_back("D:\\rawrxd\\build\\src\\thermal\\Release");
    }

    // Start file watcher thread
    if (m_autoReloadEnabled.load()) {
        m_watcherStopEvent = CreateEventA(nullptr, TRUE, FALSE, nullptr);
        if (m_watcherStopEvent) {
            m_watcherRunning = true;
            m_watcherThread = std::thread(&ThermalPluginLoader::watcherThreadFunc, this);
        }
    }

    // Start IPC server thread
    if (m_ipcEnabled.load()) {
        setupIpcServer();
    }

    return true;
}

inline void ThermalPluginLoader::shutdown()
{
    // Stop watcher
    if (m_watcherRunning.load()) {
        m_watcherRunning = false;
        if (m_watcherStopEvent) SetEvent(m_watcherStopEvent);
        if (m_watcherThread.joinable()) m_watcherThread.join();
        if (m_watcherStopEvent) { CloseHandle(m_watcherStopEvent); m_watcherStopEvent = nullptr; }
    }

    // Stop IPC
    if (m_ipcRunning.load()) {
        m_ipcRunning = false;
        if (m_ipcStopEvent) SetEvent(m_ipcStopEvent);
        if (m_ipcThread.joinable()) m_ipcThread.join();
        if (m_ipcStopEvent) { CloseHandle(m_ipcStopEvent); m_ipcStopEvent = nullptr; }
    }

    unloadPlugin();
}

inline bool ThermalPluginLoader::loadPlugin(const std::string& pluginPath)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    // Unload existing module
    if (m_hModule) {
        if (m_plugin) { m_plugin->shutdown(); m_plugin = nullptr; }
        FreeLibrary(m_hModule);
        m_hModule = nullptr;
    }

    setStatus(PluginStatus::Loading);

    // Resolve path
    std::string path = pluginPath.empty() ? findPluginPath() : pluginPath;
    if (path.empty()) {
        m_lastError = "Could not find thermal_dashboard.dll";
        setStatus(PluginStatus::Error);
        if (m_cbError) m_cbError(m_lastError);
        return false;
    }

    // Load DLL
    m_hModule = LoadLibraryA(path.c_str());
    if (!m_hModule) {
        DWORD err = GetLastError();
        m_lastError = "LoadLibrary failed (" + std::to_string(err) + "): " + path;
        setStatus(PluginStatus::Error);
        if (m_cbError) m_cbError(m_lastError);
        return false;
    }

    // Resolve factory function
    auto factory = reinterpret_cast<CreateThermalPluginFunc>(
        GetProcAddress(m_hModule, "CreateThermalPlugin"));
    if (!factory) {
        m_lastError = "DLL does not export CreateThermalPlugin: " + path;
        FreeLibrary(m_hModule);
        m_hModule = nullptr;
        setStatus(PluginStatus::Error);
        if (m_cbError) m_cbError(m_lastError);
        return false;
    }

    m_plugin = factory();
    if (!m_plugin) {
        m_lastError = "CreateThermalPlugin returned nullptr";
        FreeLibrary(m_hModule);
        m_hModule = nullptr;
        setStatus(PluginStatus::Error);
        if (m_cbError) m_cbError(m_lastError);
        return false;
    }

    // Initialize plugin
    if (!m_plugin->initialize()) {
        m_lastError = "Plugin initialization failed";
        m_plugin = nullptr;
        FreeLibrary(m_hModule);
        m_hModule = nullptr;
        setStatus(PluginStatus::Error);
        if (m_cbError) m_cbError(m_lastError);
        return false;
    }

    m_currentPluginPath = path;
    setStatus(PluginStatus::Loaded);
    if (m_cbLoaded) m_cbLoaded(PluginStatus::Loaded);
    return true;
}

inline bool ThermalPluginLoader::unloadPlugin()
{
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!m_hModule) return true;

    setStatus(PluginStatus::Unloading);

    if (m_plugin) {
        m_plugin->shutdown();
        m_plugin = nullptr;
    }

    if (!FreeLibrary(m_hModule)) {
        m_lastError = "FreeLibrary failed (" + std::to_string(GetLastError()) + ")";
        setStatus(PluginStatus::Error);
        return false;
    }
    m_hModule = nullptr;
    m_currentPluginPath.clear();
    setStatus(PluginStatus::NotLoaded);
    if (m_cbUnloaded) m_cbUnloaded(PluginStatus::NotLoaded);
    return true;
}

inline bool ThermalPluginLoader::reloadPlugin()
{
    std::string path;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        path = m_currentPluginPath;
    }
    if (!unloadPlugin()) return false;
    Sleep(100);  // Brief delay to ensure file handle released
    return loadPlugin(path);
}

inline bool ThermalPluginLoader::isLoaded() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_hModule != nullptr && m_plugin != nullptr;
}

inline rawrxd::thermal::IThermalDashboardPlugin* ThermalPluginLoader::plugin() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_plugin;
}

inline std::string ThermalPluginLoader::lastError() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_lastError;
}

inline void ThermalPluginLoader::setAutoReload(bool enabled)
{
    m_autoReloadEnabled = enabled;
}

inline void ThermalPluginLoader::setIpcEnabled(bool enabled)
{
    m_ipcEnabled = enabled;
}

inline void ThermalPluginLoader::setStatus(PluginStatus s)
{
    auto old = m_status.exchange(s);
    if (old != s && m_cbStatus) {
        m_cbStatus(s);
    }
}

inline std::string ThermalPluginLoader::findPluginPath() const
{
    const char* pluginName = "thermal_dashboard.dll";

    for (const auto& dir : m_searchPaths) {
        namespace fs = std::filesystem;
        fs::path candidate = fs::path(dir) / pluginName;
        if (fs::exists(candidate)) {
            return candidate.string();
        }
    }
    return {};
}

inline void ThermalPluginLoader::setupIpcServer()
{
    m_ipcStopEvent = CreateEventA(nullptr, TRUE, FALSE, nullptr);
    if (!m_ipcStopEvent) return;

    m_ipcRunning = true;
    m_ipcThread = std::thread(&ThermalPluginLoader::ipcThreadFunc, this);
}

inline void ThermalPluginLoader::ipcThreadFunc()
{
    const char* pipeName = "\\\\.\\pipe\\RawrXD_PluginLoader";

    while (m_ipcRunning.load()) {
        // Create named pipe instance
        HANDLE hPipe = CreateNamedPipeA(
            pipeName,
            PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED,
            PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
            PIPE_UNLIMITED_INSTANCES,
            4096, 4096, 0, nullptr);

        if (hPipe == INVALID_HANDLE_VALUE) {
            Sleep(1000);
            continue;
        }

        // Wait for connection with stop event
        OVERLAPPED ov{};
        ov.hEvent = CreateEventA(nullptr, TRUE, FALSE, nullptr);
        ConnectNamedPipe(hPipe, &ov);

        HANDLE waitHandles[] = { ov.hEvent, m_ipcStopEvent };
        DWORD waitResult = WaitForMultipleObjects(2, waitHandles, FALSE, INFINITE);
        CloseHandle(ov.hEvent);

        if (waitResult != WAIT_OBJECT_0) {
            // Stop event or error
            DisconnectNamedPipe(hPipe);
            CloseHandle(hPipe);
            break;
        }

        // Read command
        char buf[4096]{};
        DWORD bytesRead = 0;
        if (ReadFile(hPipe, buf, sizeof(buf) - 1, &bytesRead, nullptr) && bytesRead > 0) {
            buf[bytesRead] = '\0';
            handleIpcCommand(std::string(buf, bytesRead), hPipe);
        }

        FlushFileBuffers(hPipe);
        DisconnectNamedPipe(hPipe);
        CloseHandle(hPipe);
    }
}

inline void ThermalPluginLoader::handleIpcCommand(const std::string& json, HANDLE pipe)
{
    // Minimal JSON field extraction for IPC commands
    // Expected: {"action":"LOAD_PLUGIN","path":"..."} or {"action":"STATUS"}
    std::string response;

    auto extractField = [&](const std::string& key) -> std::string {
        std::string search = "\"" + key + "\":\"";
        auto pos = json.find(search);
        if (pos == std::string::npos) return {};
        pos += search.size();
        auto end = json.find('"', pos);
        if (end == std::string::npos) return {};
        return json.substr(pos, end - pos);
    };

    std::string action = extractField("action");

    if (action == "LOAD_PLUGIN") {
        std::string path = extractField("path");
        bool ok = loadPlugin(path);
        response = ok ? R"({"success":true})"
                      : R"({"success":false,"error":")" + m_lastError + "\"}";
    }
    else if (action == "UNLOAD_PLUGIN") {
        bool ok = unloadPlugin();
        response = ok ? R"({"success":true})" : R"({"success":false})";
    }
    else if (action == "RELOAD_PLUGIN") {
        bool ok = reloadPlugin();
        response = ok ? R"({"success":true})"
                      : R"({"success":false,"error":")" + m_lastError + "\"}";
    }
    else if (action == "STATUS") {
        bool loaded = isLoaded();
        response = R"({"success":true,"loaded":)" + std::string(loaded ? "true" : "false") +
                   R"(,"status":)" + std::to_string(static_cast<int>(m_status.load())) + "}";
    }
    else {
        response = R"({"success":false,"error":"Unknown action"})";
    }

    response += "\n";
    DWORD written = 0;
    WriteFile(pipe, response.c_str(), static_cast<DWORD>(response.size()), &written, nullptr);
}

inline void ThermalPluginLoader::watcherThreadFunc()
{
    namespace fs = std::filesystem;

    while (m_watcherRunning.load()) {
        // Interruptible 1-second poll
        if (WaitForSingleObject(m_watcherStopEvent, 1000) == WAIT_OBJECT_0) break;
        if (!m_autoReloadEnabled.load()) continue;

        std::string pluginPath;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            pluginPath = m_currentPluginPath;
        }
        if (pluginPath.empty()) continue;

        // Check if file was modified recently
        try {
            auto ftime = fs::last_write_time(pluginPath);
            // Convert file_time to system_clock for comparison
            auto ftimeSys = std::chrono::file_clock::to_sys(ftime);
            auto now = std::chrono::system_clock::now();
            auto age = now - ftimeSys;

            // Debounce: only reload if file changed within last 3s
            // and we haven't reloaded in the last 2s
            auto steadyNow = std::chrono::steady_clock::now();
            if (age < std::chrono::seconds(3) &&
                steadyNow - m_lastReloadTime > std::chrono::seconds(2)) {
                if (m_cbFileChanged) m_cbFileChanged(pluginPath);
                m_lastReloadTime = steadyNow;
                reloadPlugin();
            }
        } catch (...) {
            // File might be locked during rebuild — ignore
        }
    }
}

} // namespace rawrxd::core
