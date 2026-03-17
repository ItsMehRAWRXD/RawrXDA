// ============================================================================
// vsix_loader_win32.cpp - VSIXLoader implementation for Win32IDE target
// Provides GetInstance() singleton and basic plugin management without libzip.
// The full vsix_loader.cpp (with libzip) is used by other targets.
// ============================================================================

#include "vsix_loader.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <iostream>

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
    // In the Win32IDE build, VSIX loading is done via directory-based plugins
    // (no libzip dependency). Attempt to load from extracted directory.
    std::filesystem::path ppath(vsix_path);
    if (std::filesystem::is_directory(ppath)) {
        return LoadPluginFromDirectory(ppath);
    }
    // For .vsix files, log that extraction is not available in this build
    std::cerr << "[VSIXLoader] Cannot extract .vsix files in this build. "
              << "Extract manually to plugins/ directory." << std::endl;
    return false;
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
    // Placeholder — actual memory module loading is done via NativeMemoryModule
    return true;
}

bool VSIXLoader::UnloadMemoryModule(size_t context_size) {
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
    // No libzip in this build — manual extraction not supported
    std::cerr << "[VSIXLoader] .vsix extraction requires libzip (not linked)." << std::endl;
    return false;
}

bool VSIXLoader::ValidateManifest(const nlohmann::json& manifest) {
    return manifest.contains("id") && manifest.contains("name") && manifest.contains("version");
}

bool VSIXLoader::LoadPluginFromDirectory(const std::filesystem::path& plugin_dir) {
    std::filesystem::path manifest_path = plugin_dir / "manifest.json";
    if (!std::filesystem::exists(manifest_path)) return false;

    std::ifstream manifest_file(manifest_path);
    nlohmann::json manifest;
    try {
        manifest_file >> manifest;
    } catch (...) {
        return false;
    }

    if (!ValidateManifest(manifest)) return false;

    auto plugin = std::make_unique<VSIXPlugin>();
    plugin->id = manifest["id"].get<std::string>();
    plugin->name = manifest["name"].get<std::string>();
    plugin->version = manifest["version"].get<std::string>();
    plugin->description = manifest.value("description", "");
    plugin->author = manifest.value("author", "unknown");
    plugin->install_path = plugin_dir;
    plugin->enabled = true;
    plugin->manifest = manifest;

    if (manifest.contains("commands")) {
        for (const auto& cmd : manifest["commands"]) {
            plugin->commands.push_back(cmd.get<std::string>());
        }
    }

    plugins_[plugin->id] = std::move(plugin);
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
        nlohmann::json state;
        f >> state;
        // State is loaded but plugins need to be re-loaded from directories
    } catch (...) {}
}

std::string VSIXLoader::GetSizeName(size_t size) {
    if (size >= 1048576) return std::to_string(size / 1048576) + "M";
    if (size >= 1024) return std::to_string(size / 1024) + "K";
    return std::to_string(size);
}
