// ============================================================================
// vsix_loader_win32.cpp - VSIXLoader implementation for Win32IDE target
// Provides GetInstance() singleton, .vsix extraction (PowerShell/tar), and
// package.json (VS Code) manifest support. No libzip dependency.
// ============================================================================

#if defined(_WIN32)
#include <windows.h>
#endif
#include "vsix_loader.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <iostream>
#include <cctype>
#include <cstring>

// Singleton instance
VSIXLoader& VSIXLoader::GetInstance() {
    static VSIXLoader instance;
    return instance;
}

VSIXLoader::VSIXLoader() {
    plugins_dir_ = std::filesystem::current_path() / "plugins";
    std::filesystem::create_directories(plugins_dir_);
}

VSIXLoader::~VSIXLoader() {
    SavePluginState();
}

bool VSIXLoader::Initialize(const std::string& plugins_dir) {
    std::lock_guard<std::mutex> lock(mutex_);
    plugins_dir_ = std::filesystem::path(plugins_dir);
    try {
        std::filesystem::create_directories(plugins_dir_);
    } catch (...) {}
    LoadPluginState();
    return true;
}

bool VSIXLoader::LoadPlugin(const std::string& vsix_path) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::filesystem::path ppath(vsix_path);
    if (std::filesystem::is_directory(ppath)) {
        return LoadPluginFromDirectory(ppath);
    }
    // .vsix file: extract to plugins_dir_ then load
    std::string ext = ppath.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return (char)std::tolower(c); });
    if (ext != ".vsix") {
        std::cerr << "[VSIXLoader] Not a directory or .vsix file: " << vsix_path << std::endl;
        return false;
    }
    std::string stem = ppath.stem().string();
    std::filesystem::path extract_dir = plugins_dir_ / stem;
    try {
        std::filesystem::create_directories(extract_dir);
    } catch (...) {
        std::cerr << "[VSIXLoader] Failed to create extract directory: " << extract_dir << std::endl;
        return false;
    }
    if (!ExtractVSIX(vsix_path, extract_dir)) {
        std::cerr << "[VSIXLoader] Extract failed for: " << vsix_path << std::endl;
        return false;
    }
    // VS Code VSIX layout: often root/extension/package.json
    std::filesystem::path load_root = extract_dir;
    if (std::filesystem::exists(extract_dir / "extension" / "package.json")) {
        load_root = extract_dir / "extension";
    } else if (std::filesystem::exists(extract_dir / "package.json")) {
        load_root = extract_dir;
    }
    return LoadPluginFromDirectory(load_root);
}

bool VSIXLoader::UnloadPlugin(const std::string& plugin_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = plugins_.find(plugin_id);
    if (it != plugins_.end()) {
        if (it->second->onUnload) it->second->onUnload();
        plugins_.erase(it);
        return true;
    }
    return false;
}

bool VSIXLoader::EnablePlugin(const std::string& plugin_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = plugins_.find(plugin_id);
    if (it != plugins_.end()) {
        it->second->enabled = true;
        return true;
    }
    return false;
}

bool VSIXLoader::DisablePlugin(const std::string& plugin_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = plugins_.find(plugin_id);
    if (it != plugins_.end()) {
        it->second->enabled = false;
        return true;
    }
    return false;
}

bool VSIXLoader::ReloadPlugin(const std::string& plugin_id) {
    auto* p = GetPlugin(plugin_id);
    if (!p) return false;
    std::string path = p->install_path.string();
    UnloadPlugin(plugin_id);
    return LoadPlugin(path);
}

bool VSIXLoader::ConfigurePlugin(const std::string& plugin_id, const nlohmann::json& config) {
    auto* p = GetPlugin(plugin_id);
    if (p && p->onConfigure) {
        p->onConfigure(config);
        return true;
    }
    return false;
}

std::vector<VSIXPlugin*> VSIXLoader::GetLoadedPlugins() {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<VSIXPlugin*> result;
    for (auto& [id, plugin] : plugins_) {
        result.push_back(plugin.get());
    }
    return result;
}

VSIXPlugin* VSIXLoader::GetPlugin(const std::string& plugin_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = plugins_.find(plugin_id);
    return (it != plugins_.end()) ? it->second.get() : nullptr;
}

bool VSIXLoader::IsPluginLoaded(const std::string& plugin_id) {
    return GetPlugin(plugin_id) != nullptr;
}

bool VSIXLoader::IsPluginEnabled(const std::string& plugin_id) {
    auto* p = GetPlugin(plugin_id);
    return p && p->enabled;
}

nlohmann::json VSIXLoader::GetPluginConfig(const std::string& plugin_id) {
    auto* p = GetPlugin(plugin_id);
    return p ? p->manifest : nlohmann::json{};
}

std::string VSIXLoader::GetPluginHelp(const std::string& plugin_id) {
    auto* p = GetPlugin(plugin_id);
    if (!p) return "Plugin not found: " + plugin_id;
    return p->name + " v" + p->version + "\n" + p->description;
}

std::string VSIXLoader::GetAllPluginsHelp() {
    std::ostringstream ss;
    ss << "Loaded Plugins:\n";
    for (auto& [id, plugin] : plugins_) {
        ss << "  " << plugin->name << " v" << plugin->version
           << (plugin->enabled ? " [enabled]" : " [disabled]") << "\n";
    }
    return ss.str();
}

std::string VSIXLoader::GetCLIHelp() { return "vsix load <path> | vsix list | vsix unload <id>"; }
std::string VSIXLoader::GetGUIHelp() { return "Use Extensions sidebar to manage plugins"; }

bool VSIXLoader::ExecutePluginCommand(const std::string& plugin_id, const std::string& command,
                                       const std::vector<std::string>& args) {
    auto* p = GetPlugin(plugin_id);
    if (p && p->onCommand) {
        p->onCommand(command);
        return true;
    }
    return false;
}

std::string VSIXLoader::GetPluginUsage(const std::string& plugin_id) {
    auto* p = GetPlugin(plugin_id);
    return p ? ("Usage: " + p->name + " — " + p->description) : "Plugin not found.";
}

bool VSIXLoader::LoadMemoryModule(const std::string& module_path, size_t context_size) {
    std::lock_guard<std::mutex> lock(mutex_);

    // Validate module path
    if (module_path.empty()) {
        std::cerr << "[VSIXLoader] LoadMemoryModule: empty path" << std::endl;
        return false;
    }

    if (!std::filesystem::exists(module_path)) {
        std::cerr << "[VSIXLoader] Module not found: " << module_path << std::endl;
        return false;
    }

    // Load as a native DLL module
    HMODULE hMod = LoadLibraryA(module_path.c_str());
    if (!hMod) {
        DWORD err = GetLastError();
        std::cerr << "[VSIXLoader] LoadLibrary failed for " << module_path
                  << " (error " << err << ")" << std::endl;
        return false;
    }

    // Look for standard entry points
    typedef int (*InitFunc)(size_t contextSize);
    typedef const char* (*NameFunc)();

    auto initFn = (InitFunc)GetProcAddress(hMod, "RawrModule_Init");
    auto nameFn = (NameFunc)GetProcAddress(hMod, "RawrModule_Name");

    std::string moduleName = module_path;
    if (nameFn) {
        const char* name = nameFn();
        if (name) moduleName = name;
    }

    if (initFn) {
        int result = initFn(context_size);
        if (result != 0) {
            std::cerr << "[VSIXLoader] Module init failed (code " << result << "): " << moduleName << std::endl;
            FreeLibrary(hMod);
            return false;
        }
    }

    // Store handle for later unload — use engine map with size as pseudo-key
    std::string key = "mem_module_" + std::to_string(context_size);
    engines_[key] = module_path;

    std::cout << "[VSIXLoader] Loaded memory module: " << moduleName
              << " (context: " << context_size << ")" << std::endl;
    return true;
}

bool VSIXLoader::UnloadMemoryModule(size_t context_size) {
    std::lock_guard<std::mutex> lock(mutex_);

    std::string key = "mem_module_" + std::to_string(context_size);
    auto it = engines_.find(key);
    if (it == engines_.end()) {
        std::cerr << "[VSIXLoader] No memory module loaded for context size " << context_size << std::endl;
        return false;
    }

    // Get the module handle and call shutdown if available
    HMODULE hMod = GetModuleHandleA(it->second.c_str());
    if (hMod) {
        typedef void (*ShutdownFunc)();
        auto shutdownFn = (ShutdownFunc)GetProcAddress(hMod, "RawrModule_Shutdown");
        if (shutdownFn) {
            shutdownFn();
        }
        FreeLibrary(hMod);
    }

    engines_.erase(it);
    std::cout << "[VSIXLoader] Unloaded memory module for context size " << context_size << std::endl;
    return true;
}

std::vector<size_t> VSIXLoader::GetAvailableMemoryModules() {
    return {4096, 32768, 65536, 131072, 262144, 524288, 1048576};
}

void VSIXLoader::RegisterCommand(const std::string& command, std::function<void()> handler) {
    command_handlers_[command] = handler;
}

bool VSIXLoader::ExecuteCommand(const std::string& command) {
    auto it = command_handlers_.find(command);
    if (it != command_handlers_.end()) {
        it->second();
        return true;
    }
    return false;
}

std::string VSIXLoader::GetCommandHelp(const std::string& command) {
    return "Command: " + command;
}

bool VSIXLoader::LoadEngine(const std::string& engine_path, const std::string& engine_id) {
    engines_[engine_id] = engine_path;
    current_engine_id_ = engine_id;
    return true;
}

bool VSIXLoader::UnloadEngine(const std::string& engine_id) {
    engines_.erase(engine_id);
    if (current_engine_id_ == engine_id) current_engine_id_.clear();
    return true;
}

bool VSIXLoader::SwitchEngine(const std::string& engine_id) {
    if (engines_.count(engine_id)) {
        current_engine_id_ = engine_id;
        return true;
    }
    return false;
}

std::vector<std::string> VSIXLoader::GetAvailableEngines() {
    std::vector<std::string> result;
    for (const auto& [id, path] : engines_) result.push_back(id);
    return result;
}

std::string VSIXLoader::GetCurrentEngine() {
    return current_engine_id_;
}

// ============================================================================
// Private helpers
// ============================================================================

bool VSIXLoader::ExtractVSIX(const std::string& vsix_path, const std::filesystem::path& extract_dir) {
#if defined(_WIN32)
    // PowerShell Expand-Archive rejects .vsix extension (only accepts .zip).
    // VSIX is ZIP format — copy to temp .zip then expand.
    std::string dest = extract_dir.string();
    std::string pathArg = vsix_path;
    for (auto& c : pathArg) { if (c == '\\') c = '/'; }
    for (auto& c : dest) { if (c == '\\') c = '/'; }
    std::string tempZip = dest + "/_vsix_temp.zip";

    std::string cmd = "powershell -NoProfile -ExecutionPolicy Bypass -Command \"Copy-Item -Path '" + pathArg + "' -Destination '" + tempZip + "' -Force; Expand-Archive -Path '" + tempZip + "' -DestinationPath '" + dest + "' -Force; Remove-Item '" + tempZip + "' -Force -ErrorAction SilentlyContinue\"";

    STARTUPINFOA si = {};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    PROCESS_INFORMATION pi = {};
    std::vector<char> cmdBuf(cmd.begin(), cmd.end());
    cmdBuf.push_back('\0');

    if (!CreateProcessA(nullptr, cmdBuf.data(), nullptr, nullptr, FALSE,
                        CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) {
        std::cerr << "[VSIXLoader] PowerShell Expand-Archive failed to start." << std::endl;
        return false;
    }
    DWORD waitResult = WaitForSingleObject(pi.hProcess, 60000);
    DWORD exitCode = 1;
    GetExitCodeProcess(pi.hProcess, &exitCode);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);

    if (waitResult != WAIT_OBJECT_0 || exitCode != 0) {
        std::cerr << "[VSIXLoader] Extraction failed (exit " << exitCode << ")." << std::endl;
        return false;
    }
    std::cout << "[VSIXLoader] Extracted to " << extract_dir << std::endl;
    return true;
#else
    (void)vsix_path;
    (void)extract_dir;
    std::cerr << "[VSIXLoader] .vsix extraction not implemented on this platform." << std::endl;
    return false;
#endif
}

bool VSIXLoader::ValidateManifest(const nlohmann::json& manifest) {
    if (!manifest.contains("name") || !manifest.contains("version")) return false;
    return manifest.contains("id") || manifest.contains("publisher");
}

// Extract quoted string value for key from JSON content (handles project's stub nlohmann)
static std::string extractJsonString(const std::string& content, const char* key) {
    std::string pattern = "\"";
    pattern += key;
    pattern += "\"";
    size_t pos = content.find(pattern);
    if (pos == std::string::npos) return "";
    pos = content.find(':', pos);
    if (pos == std::string::npos) return "";
    pos = content.find('"', pos);
    if (pos == std::string::npos) return "";
    size_t start = pos + 1;
    std::string result;
    for (size_t i = start; i < content.size(); ++i) {
        char c = content[i];
        if (c == '\\' && i + 1 < content.size()) {
            char next = content[++i];
            if (next == '"') result += '"';
            else if (next == '\\') result += '\\';
            else result += next;
        } else if (c == '"') break;
        else result += c;
    }
    return result;
}

static bool LoadPluginFromPackageJson(const std::filesystem::path& plugin_dir, nlohmann::json& manifest,
                                      std::function<void(const char*, bool)> writeDbg = nullptr) {
    std::filesystem::path pkg_path = plugin_dir / "package.json";
    std::error_code ec;
    if (!std::filesystem::exists(pkg_path, ec)) return false;
    std::ifstream f(pkg_path);
    if (!f.is_open()) return false;
    std::string content((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    // Strip BOM if present (UTF-8 BOM = EF BB BF)
    if (content.size() >= 3 && (unsigned char)content[0] == 0xEF &&
        (unsigned char)content[1] == 0xBB && (unsigned char)content[2] == 0xBF) {
        content = content.substr(3);
    }
    // Project nlohmann::json::parse is a stub that wraps content as string.
    // Use extractJsonString to get name, version, etc. from package.json.
    std::string name = extractJsonString(content, "name");
    std::string version = extractJsonString(content, "version");
    std::string publisher = extractJsonString(content, "publisher");
    std::string displayName = extractJsonString(content, "displayName");
    std::string desc = extractJsonString(content, "description");
    if (displayName.empty()) displayName = name;
    if (name.empty() || version.empty()) return false;
    // Build manifest object for LoadPluginFromDirectory
    manifest = nlohmann::json::object();
    manifest["name"] = name;
    manifest["version"] = version;
    manifest["displayName"] = displayName;
    manifest["description"] = desc;
    manifest["publisher"] = publisher;
    return true;
}

bool VSIXLoader::LoadPluginFromDirectory(const std::filesystem::path& plugin_dir) {
    auto writeDbg = [this, &plugin_dir](const char* step, bool ok) {
        (void)step; (void)ok; (void)plugin_dir; /* debug: set RAWRXD_VSIX_DEBUG=1 to enable */
    };

    nlohmann::json manifest;
    std::filesystem::path manifest_path = plugin_dir / "manifest.json";
    bool from_package_json = false;

    if (std::filesystem::exists(manifest_path)) {
        std::ifstream manifest_file(manifest_path);
        try {
            std::string content((std::istreambuf_iterator<char>(manifest_file)),
                                std::istreambuf_iterator<char>());
            manifest = nlohmann::json::parse(content);
        } catch (...) {
            return false;
        }
        if (!ValidateManifest(manifest)) return false;
    } else if (LoadPluginFromPackageJson(plugin_dir, manifest, writeDbg)) {
        from_package_json = true;
    } else {
        return false;
    }

    auto plugin = std::make_unique<VSIXPlugin>();
    if (from_package_json) {
        std::string name = manifest.value("name", "");
        std::string publisher = manifest.value("publisher", "");
        if (name.empty()) return false;
        plugin->id = publisher.empty() ? name : (publisher + "." + name);
        plugin->name = manifest.value("displayName", name);
        plugin->version = manifest.value("version", "0.0.0");
        plugin->description = manifest.value("description", "");
        plugin->author = publisher.empty() ? "unknown" : publisher;
        if (manifest.contains("contributes") && manifest["contributes"].contains("commands")) {
            for (const auto& cmd : manifest["contributes"]["commands"]) {
                if (cmd.contains("command"))
                    plugin->commands.push_back(cmd["command"].get<std::string>());
            }
        }
    } else {
        plugin->id = manifest["id"].get<std::string>();
        plugin->name = manifest["name"].get<std::string>();
        plugin->version = manifest["version"].get<std::string>();
        plugin->description = manifest.value("description", "");
        plugin->author = manifest.value("author", "unknown");
        if (manifest.contains("commands")) {
            const auto& cmds = manifest["commands"];
            for (size_t i = 0; i < cmds.size(); ++i) {
                plugin->commands.push_back(cmds[i].get<std::string>());
            }
        }
    }

    plugin->install_path = plugin_dir;
    plugin->enabled = true;
    plugin->manifest = manifest;

    try {
        plugins_[plugin->id] = std::move(plugin);
    } catch (...) {
        return false;
    }
    return true;
}

void VSIXLoader::SavePluginState() {
    try {
        nlohmann::json state;
        for (const auto& [id, p] : plugins_) {
            state[id] = {{"enabled", p->enabled}, {"path", p->install_path.string()}};
        }
        std::ofstream f(plugins_dir_ / "plugin_state.json");
        if (f.is_open()) f << state.dump(2);
    } catch (...) {}
}

void VSIXLoader::LoadPluginState() {
    try {
        std::filesystem::path statePath = plugins_dir_ / "plugin_state.json";
        if (!std::filesystem::exists(statePath)) return;
        std::ifstream f(statePath);
        std::string content((std::istreambuf_iterator<char>(f)),
                            std::istreambuf_iterator<char>());
        nlohmann::json state = nlohmann::json::parse(content);
        // State is loaded but plugins need to be re-loaded from directories
    } catch (...) {}
}

std::string VSIXLoader::GetSizeName(size_t size) {
    if (size >= 1048576) return std::to_string(size / 1048576) + "M";
    if (size >= 1024) return std::to_string(size / 1024) + "K";
    return std::to_string(size);
}
