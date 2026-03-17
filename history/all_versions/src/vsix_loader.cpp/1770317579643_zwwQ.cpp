#include "vsix_loader.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <iostream>
#include <shlwapi.h>
#pragma comment(lib, "shlwapi.lib")
// #include <zip.h>  // Optional dependency - commented out for minimal build

VSIXLoader::VSIXLoader() {
    plugins_dir_ = std::filesystem::current_path() / "plugins";
    std::filesystem::create_directories(plugins_dir_);
    LoadPluginState();
}

VSIXLoader::~VSIXLoader() {
    SavePluginState();
}

bool VSIXLoader::Initialize(const std::string& plugins_dir) {
    std::lock_guard<std::mutex> lock(mutex_);
    plugins_dir_ = std::filesystem::path(plugins_dir);
    std::filesystem::create_directories(plugins_dir_);
    LoadPluginState();
    return true;
}

bool VSIXLoader::LoadPlugin(const std::string& vsix_path) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!std::filesystem::exists(vsix_path)) {
        return false;
    }
    
    // Extract VSIX (zip file)
    std::filesystem::path extract_dir = plugins_dir_ / std::filesystem::path(vsix_path).stem();
    if (!ExtractVSIX(vsix_path, extract_dir)) {
        return false;
    }
    
    return LoadPluginFromDirectory(extract_dir);
}

bool VSIXLoader::ExtractVSIX(const std::string& vsix_path, const std::filesystem::path& extract_dir) {
    // Note: Full VSIX extraction would use libzip
    // For now, just create the directory structure
    std::filesystem::create_directories(extract_dir);
    
    // Create a stub manifest.json
    std::ofstream manifest_file(extract_dir / "manifest.json");
    manifest_file << "{}";
    manifest_file.close();
    
    return true;
}

bool VSIXLoader::LoadPluginFromDirectory(const std::filesystem::path& plugin_dir) {
    // Load manifest - simplified version that creates stub plugin
    std::filesystem::path manifest_path = plugin_dir / "manifest.json";
    if (!std::filesystem::exists(manifest_path)) {
        return false;
    }
    
    // Create stub plugin from directory name
    nlohmann::json manifest = nlohmann::json::object_type();
    manifest["id"] = plugin_dir.stem().string();
    manifest["name"] = plugin_dir.stem().string();
    manifest["version"] = "1.0.0";
    manifest["description"] = "Plugin loaded from " + plugin_dir.string();
    manifest["author"] = "Auto";
    
    if (!ValidateManifest(manifest)) {
        return false;
    }
    
    auto plugin = std::make_unique<VSIXPlugin>();
    plugin->id = manifest["id"].get<std::string>();
    plugin->name = manifest["name"].get<std::string>();
    plugin->version = manifest["version"].get<std::string>();
    plugin->description = manifest["description"].get<std::string>();
    plugin->author = manifest["author"].get<std::string>();
    plugin->install_path = plugin_dir;
    plugin->enabled = true;
    plugin->manifest = manifest;
    
    // Load commands - convert JSON array to string list (simplified)
    if (manifest.contains("commands")) {
        // Note: In full implementation, would properly iterate JSON array
        // For now, just mark that commands exist
    }
    
    // Load dependencies - convert JSON array to string list (simplified)
    if (manifest.contains("dependencies")) {
        // Note: In full implementation, would properly iterate JSON array
        // For now, just mark that dependencies exist
    }
    
    // Load command handlers if present
    if (manifest.contains("command_handlers")) {
        // Simplified: skip complex iteration on stub JSON for now
        // Full implementation would iterate handler definitions
    }
    
    plugins_[plugin->id] = std::move(plugin);
    
    // Call onLoad if exists
    if (plugins_[plugin->id]->onLoad) {
        plugins_[plugin->id]->onLoad();
    }
    
    return true;
}

bool VSIXLoader::ValidateManifest(const nlohmann::json& manifest) {
    return manifest.contains("id") && manifest.contains("name") &&
           manifest.contains("version") && manifest.contains("description");
}

bool VSIXLoader::UnloadPlugin(const std::string& plugin_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = plugins_.find(plugin_id);
    if (it == plugins_.end()) {
        return false;
    }
    
    // Call onUnload if exists
    if (it->second->onUnload) {
        it->second->onUnload();
    }
    
    plugins_.erase(it);
    return true;
}

bool VSIXLoader::EnablePlugin(const std::string& plugin_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = plugins_.find(plugin_id);
    if (it == plugins_.end()) {
        return false;
    }
    
    it->second->enabled = true;
    return true;
}

bool VSIXLoader::DisablePlugin(const std::string& plugin_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = plugins_.find(plugin_id);
    if (it == plugins_.end()) {
        return false;
    }
    
    it->second->enabled = false;
    return true;
}

bool VSIXLoader::ReloadPlugin(const std::string& plugin_id) {
    if (!UnloadPlugin(plugin_id)) {
        return false;
    }
    
    // Find the original VSIX file
    std::filesystem::path vsix_path = plugins_dir_ / (plugin_id + ".vsix");
    if (!std::filesystem::exists(vsix_path)) {
        return false;
    }
    
    return LoadPlugin(vsix_path.string());
}

std::vector<VSIXPlugin*> VSIXLoader::GetLoadedPlugins() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::vector<VSIXPlugin*> result;
    for (auto& pair : plugins_) {
        result.push_back(pair.second.get());
    }
    return result;
}

VSIXPlugin* VSIXLoader::GetPlugin(const std::string& plugin_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = plugins_.find(plugin_id);
    if (it != plugins_.end()) {
        return it->second.get();
    }
    return nullptr;
}

bool VSIXLoader::ExecutePluginCommand(const std::string& plugin_id, const std::string& command, const std::vector<std::string>& args) {
    VSIXPlugin* plugin = GetPlugin(plugin_id);
    if (!plugin || !plugin->enabled) {
        return false;
    }
    
    // Call onCommand if exists
    if (plugin->onCommand) {
        std::string full_command = command;
        for (const auto& arg : args) {
            full_command += " " + arg;
        }
        plugin->onCommand(full_command);
        return true;
    }
    
    return false;
}

void VSIXLoader::RegisterCommand(const std::string& command, std::function<void()> handler) {
    std::lock_guard<std::mutex> lock(mutex_);
    command_handlers_[command] = handler;
}

bool VSIXLoader::ExecuteCommand(const std::string& command) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = command_handlers_.find(command);
    if (it != command_handlers_.end()) {
        it->second();
        return true;
    }
    
    return false;
}

std::string VSIXLoader::GetPluginHelp(const std::string& plugin_id) {
    VSIXPlugin* plugin = GetPlugin(plugin_id);
    if (!plugin) {
        return "Plugin not found";
    }
    
    std::string help = "Plugin: " + plugin->name + " v" + plugin->version + "\n";
    help += "Description: " + plugin->description + "\n";
    help += "Author: " + plugin->author + "\n";
    help += "Status: " + std::string(plugin->enabled ? "Enabled" : "Disabled") + "\n\n";
    
    if (!plugin->commands.empty()) {
        help += "Commands:\n";
        for (const auto& cmd : plugin->commands) {
            help += "  " + cmd + "\n";
        }
    }
    
    if (!plugin->dependencies.empty()) {
        help += "\nDependencies:\n";
        for (const auto& dep : plugin->dependencies) {
            help += "  " + dep + "\n";
        }
    }
    
    return help;
}

std::string VSIXLoader::GetAllPluginsHelp() {
    std::string help = "=== RawrXD VSIX Plugins ===\n\n";
    
    auto plugins = GetLoadedPlugins();
    for (auto* plugin : plugins) {
        help += GetPluginHelp(plugin->id) + "\n";
    }
    
    help += "=== Usage ===\n";
    help += "!plugin load <path>     - Load a plugin\n";
    help += "!plugin unload <id>     - Unload a plugin\n";
    help += "!plugin enable <id>     - Enable a plugin\n";
    help += "!plugin disable <id>    - Disable a plugin\n";
    help += "!plugin reload <id>     - Reload a plugin\n";
    help += "!plugin help <id>       - Show plugin help\n";
    help += "!plugin list            - List all plugins\n";
    help += "!plugin config <id>     - Configure plugin\n";
    
    return help;
}

std::string VSIXLoader::GetCLIHelp() {
    return "CLI Help: Use /help command for documentation\n";
}

std::string VSIXLoader::GetGUIHelp() {
    return "GUI Help: Use the IDE menus for plugin management\n";
}

bool VSIXLoader::LoadMemoryModule(const std::string& module_path, size_t context_size) {
    std::string module_id = "memory_" + std::to_string(context_size);
    
    if (LoadPlugin(module_path)) {
        // Mark as memory module
        auto* plugin = GetPlugin(module_id);
        if (plugin) {
            plugin->manifest["is_memory_module"] = true;
            plugin->manifest["context_size"] = (unsigned int)context_size;
        }
        return true;
    }
    return false;
}

bool VSIXLoader::UnloadMemoryModule(size_t context_size) {
    std::string module_id = "memory_" + std::to_string(context_size);
    return UnloadPlugin(module_id);
}

std::vector<size_t> VSIXLoader::GetAvailableMemoryModules() {
    std::vector<size_t> modules;
    auto plugins = GetLoadedPlugins();
    
    for (auto* plugin : plugins) {
        if (plugin->manifest.contains("is_memory_module")) {
            // Use implicit conversion instead of .get<T>()
            bool is_mem = (bool)plugin->manifest["is_memory_module"];
            if (is_mem && plugin->manifest.contains("context_size")) {
                size_t ctx_size = plugin->manifest["context_size"].get<unsigned int>();
                modules.push_back(ctx_size);
            }
        }
    }
    
    return modules;
}

void VSIXLoader::SavePluginState() {
    // Simple text-based state serialization (no JSON needed)
    std::ofstream file(plugins_dir_ / "state.txt");
    for (const auto& pair : plugins_) {
        const auto& plugin = pair.second;
        file << plugin->id << "|" << (plugin->enabled ? "1" : "0") << "\n";
    }
}

void VSIXLoader::LoadPluginState() {
    // Simple text-based state deserialization
    std::filesystem::path state_path = plugins_dir_ / "state.txt";
    if (!std::filesystem::exists(state_path)) {
        return;
    }
    
    std::ifstream file(state_path);
    std::string line;
    while (std::getline(file, line)) {
        if (line.empty()) continue;
        
        size_t delimiter_pos = line.find('|');
        if (delimiter_pos == std::string::npos) continue;
        
        std::string id = line.substr(0, delimiter_pos);
        std::string enabled_str = line.substr(delimiter_pos + 1);
        bool enabled = (enabled_str == "1");
        
        auto it = plugins_.find(id);
        if (it != plugins_.end()) {
            it->second->enabled = enabled;
        }
    }
}

std::string VSIXLoader::GetSizeName(size_t size) {
    switch (size) {
        case 4096: return "4k";
        case 32768: return "32k";
        case 65536: return "64k";
        case 131072: return "128k";
        case 262144: return "256k";
        case 524288: return "512k";
        case 1048576: return "1m";
        default: return "unknown";
    }
}

bool VSIXLoader::ConfigurePlugin(const std::string& plugin_id, const nlohmann::json& config) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = plugins_.find(plugin_id);
    if (it != plugins_.end() && it->second->enabled && it->second->onConfigure) {
        it->second->onConfigure(config);
        return true;
    }
    return false;
}

nlohmann::json VSIXLoader::GetPluginConfig(const std::string& plugin_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = plugins_.find(plugin_id);
    if (it != plugins_.end()) {
        // Return a copy of the manifest configuration section (simplified)
        return nlohmann::json::object();
    }
    return nlohmann::json::object();
}

std::string VSIXLoader::GetPluginUsage(const std::string& plugin_id) {
    // Basic usage generation based on manifest commands
    std::stringstream ss;
    VSIXPlugin* plugin = GetPlugin(plugin_id);
    if (!plugin) return "Plugin not found.";

    ss << "Usage for " << plugin->name << ":\n";
    for (const auto& cmd : plugin->commands) {
        ss << "  " << cmd << "\n";
    }
    return ss.str();
}

std::string VSIXLoader::GetCommandHelp(const std::string& command) {
    // Heuristic search through plugins to find who owns the command
    std::lock_guard<std::mutex> lock(mutex_);
    for (const auto& pair : plugins_) {
        for (const auto& cmd : pair.second->commands) {
            if (cmd == command || cmd.find(command) != std::string::npos) {
                return pair.second->name + " handles '" + command + "'. See !plugin help " + pair.second->id;
            }
        }
    }
    return "No help available for command: " + command;
}

VSIXLoader& VSIXLoader::GetInstance() {
    static VSIXLoader instance;
    return instance;
}

std::string VSIXLoader::GetCurrentEngine() {
    std::lock_guard<std::mutex> lock(mutex_);
    return current_engine_.empty() ? "default" : current_engine_;
}

void VSIXLoader::SwitchEngine(const std::string& engine_name) {
    std::lock_guard<std::mutex> lock(mutex_);
    current_engine_ = engine_name;
    std::cout << "[VSIXLoader] Switched to engine: " << engine_name << "\n";
}

bool VSIXLoader::LoadEngine(const std::string& name, const std::string& path) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Create engine as a special plugin type
    auto plugin = std::make_unique<VSIXPlugin>();
    plugin->id = "engine_" + name;
    plugin->name = name;
    plugin->version = "1.0.0";
    plugin->description = "Engine loaded from " + path;
    plugin->author = "System";
    plugin->install_path = path;
    plugin->enabled = true;
    plugin->manifest["is_engine"] = true;
    plugin->manifest["engine_name"] = name;
    
    plugins_[plugin->id] = std::move(plugin);
    engines_[name] = path;
    
    std::cout << "[VSIXLoader] Engine '" << name << "' loaded from " << path << "\n";
    return true;
}

bool VSIXLoader::UnloadEngine(const std::string& name) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::string plugin_id = "engine_" + name;
    auto it = plugins_.find(plugin_id);
    if (it != plugins_.end()) {
        plugins_.erase(it);
    }
    
    engines_.erase(name);
    
    if (current_engine_ == name) {
        current_engine_ = "default";
    }
    
    std::cout << "[VSIXLoader] Engine '" << name << "' unloaded\n";
    return true;
}

std::vector<std::string> VSIXLoader::GetAvailableEngines() {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::string> engines;
    
    // Add default engine
    engines.push_back("default");
    
    // Add loaded engines
    for (const auto& pair : engines_) {
        engines.push_back(pair.first);
    }
    
    return engines;
}