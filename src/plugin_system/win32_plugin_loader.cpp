// =============================================================================
// Win32PluginLoader Implementation — Native Win32 Plugin System
//
// Phase 43: Pure Win32 LoadLibrary/GetProcAddress plugin loading.
// Thread-safe, exception-free, structured logging.
// =============================================================================

#include "../../include/plugin_system/win32_plugin_loader.h"
#include <algorithm>
#include <chrono>
#include <sstream>

namespace RawrXD {

// Static instance for C callback routing
Win32PluginLoader* Win32PluginLoader::s_instance = nullptr;

// =============================================================================
// SEH-safe wrapper — MSVC can't mix __try with C++ objects that need unwinding.
// Isolate the SEH into a plain-C helper that calls a void(void) function ptr.
// =============================================================================
static bool safeCallPluginFn_void(void (*fn)()) {
    __try {
        fn();
        return true;
    } __except(EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

static bool safeCallPluginFn_str(void (*fn)(const char*), const char* arg) {
    __try {
        fn(arg);
        return true;
    } __except(EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

// =============================================================================
// Constructor / Destructor
// =============================================================================
Win32PluginLoader::Win32PluginLoader() {
    s_instance = this;

    // Initialize the PluginContext with our static C callback
    m_context.requestFileOperation = &Win32PluginLoader::pluginFileOperationCallback;
}

Win32PluginLoader::~Win32PluginLoader() {
    unloadAll();
    s_instance = nullptr;
}

// =============================================================================
// loadPlugin — Load a DLL, resolve C ABI symbols, call plugin_init
// =============================================================================
bool Win32PluginLoader::loadPlugin(const std::string& pluginPath) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (pluginPath.empty()) {
        log("loadPlugin: empty path", 2);
        return false;
    }

    std::string name = extractPluginName(pluginPath);

    // Check for duplicate
    if (m_plugins.find(name) != m_plugins.end()) {
        log("loadPlugin: plugin already loaded: " + name, 1);
        return false;
    }

    auto startTime = std::chrono::steady_clock::now();

    // Load the DLL
    HMODULE hModule = LoadLibraryA(pluginPath.c_str());
    if (!hModule) {
        DWORD err = GetLastError();
        log("loadPlugin: LoadLibrary failed for '" + pluginPath +
            "' (error " + std::to_string(err) + ")", 2);
        return false;
    }

    // Build PluginInstance
    PluginInstance inst;
    inst.hModule = hModule;
    inst.name = name;
    inst.path = pluginPath;
    inst.state = PluginState::Loaded;

    // Resolve required symbol: plugin_init
    inst.fn_init = reinterpret_cast<PluginInfo*(*)(PluginContext*)>(
        GetProcAddress(hModule, "plugin_init"));

    if (!inst.fn_init) {
        log("loadPlugin: missing 'plugin_init' export in '" + pluginPath + "'", 2);
        FreeLibrary(hModule);
        return false;
    }

    // Resolve optional hook symbols
    inst.fn_onFileSave = reinterpret_cast<void(*)(const char*)>(
        GetProcAddress(hModule, "plugin_onFileSave"));
    inst.fn_onChatMessage = reinterpret_cast<void(*)(const char*)>(
        GetProcAddress(hModule, "plugin_onChatMessage"));
    inst.fn_onCommand = reinterpret_cast<void(*)(const char*)>(
        GetProcAddress(hModule, "plugin_onCommand"));
    inst.fn_onModelLoad = reinterpret_cast<void(*)(const char*)>(
        GetProcAddress(hModule, "plugin_onModelLoad"));
    inst.fn_cleanup = reinterpret_cast<void(*)()>(
        GetProcAddress(hModule, "plugin_cleanup"));

    // Call plugin_init with our context
    inst.info = inst.fn_init(&m_context);
    if (!inst.info) {
        log("loadPlugin: plugin_init returned null for '" + name + "'", 1);
        // Non-fatal — plugin may still function with hooks
    }

    auto endTime = std::chrono::steady_clock::now();
    inst.loadTimeMs = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count());

    inst.state = PluginState::Active;
    m_plugins[name] = inst;

    std::string loadMsg = "Plugin loaded: " + name;
    if (inst.info) {
        loadMsg += " v" + std::string(inst.info->version ? inst.info->version : "?");
        if (inst.info->description) {
            loadMsg += " — " + std::string(inst.info->description);
        }
    }
    loadMsg += " (" + std::to_string(inst.loadTimeMs) + "ms)";
    log(loadMsg, 0);

    return true;
}

// =============================================================================
// unloadPlugin — Call cleanup, FreeLibrary, remove from map
// =============================================================================
bool Win32PluginLoader::unloadPlugin(const std::string& pluginName) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_plugins.find(pluginName);
    if (it == m_plugins.end()) {
        log("unloadPlugin: not found: " + pluginName, 1);
        return false;
    }

    PluginInstance& inst = it->second;
    inst.state = PluginState::Unloading;

    // Call cleanup if available
    if (inst.fn_cleanup) {
        if (!safeCallPluginFn_void(inst.fn_cleanup)) {
            log("unloadPlugin: exception in plugin_cleanup for '" + pluginName + "'", 2);
        }
    }

    // Unload the DLL
    if (inst.hModule) {
        FreeLibrary(inst.hModule);
        inst.hModule = nullptr;
    }

    log("Plugin unloaded: " + pluginName, 0);
    m_plugins.erase(it);
    return true;
}

// =============================================================================
// unloadAll
// =============================================================================
void Win32PluginLoader::unloadAll() {
    std::lock_guard<std::mutex> lock(m_mutex);

    // Collect names first (can't iterate + erase simultaneously)
    std::vector<std::string> names;
    names.reserve(m_plugins.size());
    for (const auto& kv : m_plugins) {
        names.push_back(kv.first);
    }

    // Unlock, then unload individually (unloadPlugin re-locks)
    m_mutex.unlock();
    for (const auto& name : names) {
        unloadPlugin(name);
    }
    m_mutex.lock();
}

// =============================================================================
// Hook dispatch methods
// =============================================================================
void Win32PluginLoader::dispatchFileSave(const std::string& filePath) {
    std::lock_guard<std::mutex> lock(m_mutex);
    for (auto& kv : m_plugins) {
        auto& inst = kv.second;
        if (inst.state == PluginState::Active && inst.fn_onFileSave) {
            if (safeCallPluginFn_str(inst.fn_onFileSave, filePath.c_str())) {
                inst.hookCalls++;
            } else {
                log("dispatchFileSave: exception in plugin '" + inst.name + "'", 2);
                inst.state = PluginState::Error;
            }
        }
    }
}

void Win32PluginLoader::dispatchChatMessage(const std::string& message) {
    std::lock_guard<std::mutex> lock(m_mutex);
    for (auto& kv : m_plugins) {
        auto& inst = kv.second;
        if (inst.state == PluginState::Active && inst.fn_onChatMessage) {
            if (safeCallPluginFn_str(inst.fn_onChatMessage, message.c_str())) {
                inst.hookCalls++;
            } else {
                log("dispatchChatMessage: exception in plugin '" + inst.name + "'", 2);
                inst.state = PluginState::Error;
            }
        }
    }
}

void Win32PluginLoader::dispatchCommand(const std::string& command) {
    std::lock_guard<std::mutex> lock(m_mutex);
    for (auto& kv : m_plugins) {
        auto& inst = kv.second;
        if (inst.state == PluginState::Active && inst.fn_onCommand) {
            if (safeCallPluginFn_str(inst.fn_onCommand, command.c_str())) {
                inst.hookCalls++;
            } else {
                log("dispatchCommand: exception in plugin '" + inst.name + "'", 2);
                inst.state = PluginState::Error;
            }
        }
    }
}

void Win32PluginLoader::dispatchModelLoad(const std::string& modelPath) {
    std::lock_guard<std::mutex> lock(m_mutex);
    for (auto& kv : m_plugins) {
        auto& inst = kv.second;
        if (inst.state == PluginState::Active && inst.fn_onModelLoad) {
            if (safeCallPluginFn_str(inst.fn_onModelLoad, modelPath.c_str())) {
                inst.hookCalls++;
            } else {
                log("dispatchModelLoad: exception in plugin '" + inst.name + "'", 2);
                inst.state = PluginState::Error;
            }
        }
    }
}

// =============================================================================
// Query methods
// =============================================================================
size_t Win32PluginLoader::pluginCount() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_plugins.size();
}

std::vector<std::string> Win32PluginLoader::pluginNames() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<std::string> names;
    names.reserve(m_plugins.size());
    for (const auto& kv : m_plugins) {
        names.push_back(kv.first);
    }
    return names;
}

const PluginInstance* Win32PluginLoader::getPlugin(const std::string& name) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_plugins.find(name);
    return (it != m_plugins.end()) ? &it->second : nullptr;
}

bool Win32PluginLoader::isLoaded(const std::string& name) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_plugins.find(name) != m_plugins.end();
}

// =============================================================================
// scanDirectory — Find all .dll files in a directory
// =============================================================================
std::vector<std::string> Win32PluginLoader::scanDirectory(const std::string& dirPath) const {
    std::vector<std::string> results;

    std::string searchPath = dirPath;
    if (!searchPath.empty() && searchPath.back() != '\\' && searchPath.back() != '/') {
        searchPath += "\\";
    }
    searchPath += "*.dll";

    WIN32_FIND_DATAA findData;
    HANDLE hFind = FindFirstFileA(searchPath.c_str(), &findData);
    if (hFind == INVALID_HANDLE_VALUE) {
        return results;
    }

    do {
        if (!(findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
            std::string fullPath = dirPath;
            if (!fullPath.empty() && fullPath.back() != '\\' && fullPath.back() != '/') {
                fullPath += "\\";
            }
            fullPath += findData.cFileName;
            results.push_back(fullPath);
        }
    } while (FindNextFileA(hFind, &findData));

    FindClose(hFind);
    return results;
}

// =============================================================================
// setLogCallback
// =============================================================================
void Win32PluginLoader::setLogCallback(PluginLogCallback cb) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_logCallback = std::move(cb);
}

// =============================================================================
// Internal helpers
// =============================================================================
void Win32PluginLoader::log(const std::string& msg, int severity) {
    if (m_logCallback) {
        m_logCallback(msg, severity);
    }
    // Always emit to debug output
    OutputDebugStringA(("[PluginLoader] " + msg + "\n").c_str());
}

std::string Win32PluginLoader::extractPluginName(const std::string& path) const {
    // Extract filename without extension
    size_t lastSlash = path.find_last_of("\\/");
    std::string filename = (lastSlash != std::string::npos) ? path.substr(lastSlash + 1) : path;
    size_t dot = filename.rfind('.');
    return (dot != std::string::npos) ? filename.substr(0, dot) : filename;
}

void Win32PluginLoader::pluginFileOperationCallback(const char* operation,
                                                     const char* filePath,
                                                     const char* content) {
    if (!s_instance) return;

    std::string msg = "Plugin file operation: " + std::string(operation ? operation : "null") +
                      " on " + std::string(filePath ? filePath : "null");
    s_instance->log(msg, 0);

    // Route through the IDE's agentic file operations system in the future
    // For now, log-only — actual file ops require user consent dialog
}

} // namespace RawrXD
