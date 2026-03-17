#include "vsix_loader.h"
#include <fstream>
#include <sstream>
#include <algorithm>
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
    int err = 0;
    zip* z = zip_open(vsix_path.c_str(), 0, &err);
    if (!z) return false;
    
    std::filesystem::create_directories(extract_dir);
    
    zip_int64_t num_entries = zip_get_num_entries(z, 0);
    for (zip_int64_t i = 0; i < num_entries; i++) {
        const char* name = zip_get_name(z, i, 0);
        if (!name) continue;
        
        std::filesystem::path entry_path = extract_dir / name;
        std::filesystem::create_directories(entry_path.parent_path());
        
        zip_file* f = zip_fopen_index(z, i, 0);
        if (!f) continue;
        
        std::ofstream out(entry_path, std::ios::binary);
        char buffer[8192];
        zip_int64_t bytes_read;
        
        while ((bytes_read = zip_fread(f, buffer, sizeof(buffer))) > 0) {
            out.write(buffer, bytes_read);
        }
        
        zip_fclose(f);
        out.close();
    }
    
    zip_close(z);
    return true;
}

bool VSIXLoader::LoadPluginFromDirectory(const std::filesystem::path& plugin_dir) {
    // Load manifest
    std::filesystem::path manifest_path = plugin_dir / "manifest.json";
    if (!std::filesystem::exists(manifest_path)) {
        return false;
    }
    
    std::ifstream manifest_file(manifest_path);
    nlohmann::json manifest;
    try {
        manifest_file >> manifest;
    } catch (...) {
        return false;
    }
    
    if (!ValidateManifest(manifest)) {
        return false;
    }
    
    auto plugin = std::make_unique<VSIXPlugin>();
    plugin->id = manifest["id"];
    plugin->name = manifest["name"];
    plugin->version = manifest["version"];
    plugin->description = manifest["description"];
    plugin->author = manifest["author"];
    plugin->install_path = plugin_dir;
    plugin->enabled = true;
    plugin->manifest = manifest;
    
    // Load commands
    if (manifest.contains("commands")) {
        for (const auto& cmd : manifest["commands"]) {
            plugin->commands.push_back(cmd);
        }
    }
    
    // Load dependencies
    if (manifest.contains("dependencies")) {
        for (const auto& dep : manifest["dependencies"]) {
            plugin->dependencies.push_back(dep);
        }
    }
    
    // Load command handlers if present
    if (manifest.contains("command_handlers")) {
        for (const auto& [cmd, handler] : manifest["command_handlers"].items()) {
            RegisterCommand(cmd, [handler]() {
                // Execute handler (simplified)
                std::cout << "Executing: " << handler << std::endl;
            });
        }
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
    return R"(
=== RawrXD CLI Plugin Commands ===

Plugin Management:
  !plugin load <path>     - Load VSIX plugin from file
  !plugin unload <id>     - Unload plugin by ID
  !plugin enable <id>     - Enable disabled plugin
  !plugin disable <id>    - Disable plugin
  !plugin reload <id>     - Reload plugin (useful after updates)
  !plugin help <id>       - Show detailed plugin help
  !plugin list            - List all loaded plugins
  !plugin config <id> <key> <value> - Configure plugin settings

Memory Module Management:
  !memory load <size>     - Load memory module (4k/32k/64k/128k/256k/512k/1m)
  !memory unload <size>   - Unload memory module
  !memory list            - List available memory modules
  !memory current         - Show current context size

Agentic Commands:
  /plan <task>            - Create execution plan
  /react-server <name>    - Generate React project
  /bugreport <file|dir>   - Analyze for bugs
  /suggest <code|file>    - Get code suggestions
  /hotpatch <file> <old> <new> - Apply code hotpatch
  /analyze <file>         - Deep code analysis
  /optimize <file>        - Performance optimization
  /security <file>        - Security vulnerability scan

Mode Toggles:
  /maxmode <on|off>       - Toggle max mode (32K+ context)
  /deepthinking <on|off>  - Toggle deep thinking (chain-of-thought)
  /deepresearch <on|off>  - Toggle deep research (workspace scan)
  /norefusal <on|off>     - Toggle no refusal (technical answers)
  /autocorrect <on|off>   - Toggle auto-correction of hallucinations

Context & Memory:
  /context <size>         - Set context size (4k/32k/64k/128k/256k/512k/1m)
  /context+               - Increase context size
  /context-               - Decrease context size
  /memory-status          - Show memory usage

Shell & Interaction:
  /shell                  - Start interactive AI shell
  /shell-exit             - Exit shell mode
  /clear                  - Clear conversation history
  /save <filename>        - Save conversation to file
  /load <filename>        - Load conversation from file
  /status                 - Show system status
  /help                   - Show this help
  /exit                   - Exit RawrXD

Examples:
  !plugin load "C:\plugins\react-advanced.vsix"
  !plugin enable react-advanced
  !plugin help react-advanced
  !memory load 128k
  /plan create a full-stack app with auth and database
  /react-server my-app --auth --database=postgresql
  /bugreport src/
  /suggest src/main.cpp
  /hotpatch src/main.cpp "void main()" "int main()"
  /deepthinking on
  /context 64k
  /shell

For more help: https://github.com/ItsMehRAWRXD/RawrXD/wiki
)";
}

std::string VSIXLoader::GetGUIHelp() {
    return R"(
=== RawrXD GUI Plugin Controls ===

Plugin Menu:
  File → Plugins → Load Plugin...     - Browse and load VSIX file
  File → Plugins → Manage Plugins...  - Enable/disable plugins
  Tools → Plugin Console...           - Execute plugin commands

Memory Controls:
  View → Memory → Context Size → [4K|32K|64K|128K|256K|512K|1M]
  View → Memory → Advanced Modes → [Max Mode|Deep Thinking|Deep Research|No Refusal]

Plugin Toolbar:
  [Load] [Enable] [Disable] [Reload] [Help]

Right-click on plugin → Configure → Set options

Keyboard Shortcuts:
  Ctrl+Shift+P - Open plugin command palette
  Ctrl+Shift+M - Open memory configuration
  Ctrl+Shift+V - Open VSIX manager
  Ctrl+Shift+S - Open AI shell

Activity Bar:
  Click icons to switch sidebar views
  Hover for tooltips

Secondary Sidebar (Right):
  AI Chat / Copilot area
  Model selector
  Max tokens slider
  Context size slider
  Mode toggles

Panel (Bottom):
  Terminal, Output, Problems, Debug Console
  Click tabs to switch
  Use toolbar buttons for actions

Status Bar:
  Shows remote, branch, errors, warnings, cursor, encoding, language, Copilot status
  Click items for quick actions

For more help: https://github.com/ItsMehRAWRXD/RawrXD/wiki
)";
}

bool VSIXLoader::LoadMemoryModule(const std::string& module_path, size_t context_size) {
    std::string module_id = "memory_" + std::to_string(context_size);
    
    if (LoadPlugin(module_path)) {
        // Mark as memory module
        auto* plugin = GetPlugin(module_id);
        if (plugin) {
            plugin->manifest["is_memory_module"] = true;
            plugin->manifest["context_size"] = context_size;
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
                size_t ctx_size = (unsigned int)plugin->manifest["context_size"];
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