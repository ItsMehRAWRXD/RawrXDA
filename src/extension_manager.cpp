#include "extension_manager.h"
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <iostream>
#include <cstdlib>

#ifdef _WIN32
#include <windows.h>
#endif

namespace IDE {

// JSON parsing helpers (simple implementation)
namespace {
    std::string trimQuotes(const std::string& str) {
        if (str.length() >= 2 && str.front() == '"' && str.back() == '"') {
            return str.substr(1, str.length() - 2);
        }
        return str;
    }

    std::string escapeJson(const std::string& str) {
        std::string result;
        for (char c : str) {
            if (c == '"' || c == '\\') result += '\\';
            result += c;
        }
        return result;
    }
}

ExtensionManager::ExtensionManager() 
    : extensionRoot_("d:\\lazy init ide\\extensions"),
      moduleStore_("d:\\lazy init ide\\scripts\\modules"),
      registryPath_("d:\\lazy init ide\\extensions\\registry.json")
{
    // Create directories if they don't exist
#ifdef _WIN32
    CreateDirectoryA(extensionRoot_.c_str(), NULL);
    CreateDirectoryA(moduleStore_.c_str(), NULL);
#endif
    
    loadRegistry();
}

ExtensionManager::~ExtensionManager() {
    saveRegistry();
}

bool ExtensionManager::loadRegistry() {
    std::ifstream file(registryPath_);
    if (!file.is_open()) {
        return false;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    file.close();

    return parseRegistryJson(buffer.str());
}

bool ExtensionManager::saveRegistry() {
    std::ofstream file(registryPath_);
    if (!file.is_open()) {
        return false;
    }

    file << generateRegistryJson();
    file.close();
    return true;
}

bool ExtensionManager::parseRegistryJson(const std::string& json) {
    // Simple JSON parsing - in production, use a proper JSON library
    registry_.clear();
    
    size_t pos = 0;
    while ((pos = json.find("\"", pos)) != std::string::npos) {
        size_t nameStart = pos + 1;
        size_t nameEnd = json.find("\"", nameStart);
        if (nameEnd == std::string::npos) break;
        
        std::string name = json.substr(nameStart, nameEnd - nameStart);
        
        // Find the object for this extension
        size_t objStart = json.find("{", nameEnd);
        if (objStart == std::string::npos) break;
        
        Extension ext;
        ext.name = name;
        
        // Parse Type
        size_t typePos = json.find("\"Type\"", objStart);
        if (typePos != std::string::npos && typePos < json.find("}", objStart)) {
            size_t valueStart = json.find("\"", typePos + 6) + 1;
            size_t valueEnd = json.find("\"", valueStart);
            ext.type = json.substr(valueStart, valueEnd - valueStart);
        }
        
        // Parse Installed
        size_t installedPos = json.find("\"Installed\"", objStart);
        if (installedPos != std::string::npos) {
            ext.installed = json.find("true", installedPos) != std::string::npos;
        }
        
        // Parse Enabled
        size_t enabledPos = json.find("\"Enabled\"", objStart);
        if (enabledPos != std::string::npos) {
            ext.enabled = json.find("true", enabledPos) != std::string::npos;
        }
        
        registry_[name] = ext;
        pos = nameEnd + 1;
    }
    
    return true;
}

std::string ExtensionManager::generateRegistryJson() const {
    std::stringstream json;
    json << "{\n";
    
    bool first = true;
    for (const auto& pair : registry_) {
        if (!first) json << ",\n";
        first = false;
        
        const Extension& ext = pair.second;
        json << "  \"" << escapeJson(ext.name) << "\": {\n";
        json << "    \"Type\": \"" << escapeJson(ext.type) << "\",\n";
        json << "    \"Path\": \"" << escapeJson(ext.path) << "\",\n";
        json << "    \"Installed\": " << (ext.installed ? "true" : "false") << ",\n";
        json << "    \"Enabled\": " << (ext.enabled ? "true" : "false") << ",\n";
        json << "    \"Created\": \"" << escapeJson(ext.created) << "\"\n";
        json << "  }";
    }
    
    json << "\n}\n";
    return json.str();
}

bool ExtensionManager::executePowerShellCommand(const std::string& command, std::string& output) {
#ifdef _WIN32
    std::string fullCmd = "powershell.exe -NoProfile -ExecutionPolicy Bypass -Command \"" + command + "\"";
    
    FILE* pipe = _popen(fullCmd.c_str(), "r");
    if (!pipe) return false;
    
    char buffer[256];
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        output += buffer;
    }
    
    int exitCode = _pclose(pipe);
    return exitCode == 0;
#else
    return false; // Unix support would go here
#endif
}

bool ExtensionManager::createExtension(const std::string& name, const std::string& type,
                                       const std::map<std::string, std::string>& params) {
    std::string scriptPath = "d:\\lazy init ide\\ExtensionManager.ps1";
    
    // Build PowerShell command
    std::stringstream cmd;
    cmd << "$null = . '" << scriptPath << "'; ";
    cmd << "New-Extension -Name '" << name << "' -Type '" << type << "'";
    
    std::string output;
    if (!executePowerShellCommand(cmd.str(), output)) {
        return false;
    }
    
    // Reload registry to pick up new extension
    loadRegistry();
    return registry_.find(name) != registry_.end();
}

bool ExtensionManager::installExtension(const std::string& name) {
    if (registry_.find(name) == registry_.end()) {
        return false;
    }
    
    std::string scriptPath = "d:\\lazy init ide\\ExtensionManager.ps1";
    std::stringstream cmd;
    cmd << "$null = . '" << scriptPath << "'; ";
    cmd << "Install-Extension -Name '" << name << "'";
    
    std::string output;
    bool success = executePowerShellCommand(cmd.str(), output);
    
    if (success) {
        registry_[name].installed = true;
        saveRegistry();
    }
    
    return success;
}

bool ExtensionManager::enableExtension(const std::string& name) {
    if (registry_.find(name) == registry_.end()) {
        return false;
    }
    
    // Install first if not installed
    if (!registry_[name].installed) {
        if (!installExtension(name)) {
            return false;
        }
    }
    
    std::string scriptPath = "d:\\lazy init ide\\ExtensionManager.ps1";
    std::stringstream cmd;
    cmd << "$null = . '" << scriptPath << "'; ";
    cmd << "Enable-Extension -Name '" << name << "'";
    
    std::string output;
    bool success = executePowerShellCommand(cmd.str(), output);
    
    if (success) {
        registry_[name].enabled = true;
        saveRegistry();
    }
    
    return success;
}

bool ExtensionManager::disableExtension(const std::string& name) {
    if (registry_.find(name) == registry_.end()) {
        return false;
    }
    
    std::string scriptPath = "d:\\lazy init ide\\ExtensionManager.ps1";
    std::stringstream cmd;
    cmd << "$null = . '" << scriptPath << "'; ";
    cmd << "Disable-Extension -Name '" << name << "'";
    
    std::string output;
    bool success = executePowerShellCommand(cmd.str(), output);
    
    if (success) {
        registry_[name].enabled = false;
        saveRegistry();
    }
    
    return success;
}

bool ExtensionManager::uninstallExtension(const std::string& name) {
    if (registry_.find(name) == registry_.end()) {
        return false;
    }
    
    // Disable first if enabled
    if (registry_[name].enabled) {
        disableExtension(name);
    }
    
    std::string scriptPath = "d:\\lazy init ide\\ExtensionManager.ps1";
    std::stringstream cmd;
    cmd << "$null = . '" << scriptPath << "'; ";
    cmd << "Uninstall-Extension -Name '" << name << "'";
    
    std::string output;
    bool success = executePowerShellCommand(cmd.str(), output);
    
    if (success) {
        registry_[name].installed = false;
        saveRegistry();
    }
    
    return success;
}

bool ExtensionManager::removeExtension(const std::string& name) {
    if (registry_.find(name) == registry_.end()) {
        return false;
    }
    
    // Uninstall first if installed
    if (registry_[name].installed) {
        uninstallExtension(name);
    }
    
    std::string scriptPath = "d:\\lazy init ide\\ExtensionManager.ps1";
    std::stringstream cmd;
    cmd << "$null = . '" << scriptPath << "'; ";
    cmd << "Remove-Extension -Name '" << name << "' -Force";
    
    std::string output;
    bool success = executePowerShellCommand(cmd.str(), output);
    
    if (success) {
        registry_.erase(name);
        saveRegistry();
    }
    
    return success;
}

std::vector<Extension> ExtensionManager::listExtensions() const {
    std::vector<Extension> result;
    for (const auto& pair : registry_) {
        result.push_back(pair.second);
    }
    return result;
}

Extension ExtensionManager::getExtension(const std::string& name) const {
    auto it = registry_.find(name);
    if (it != registry_.end()) {
        return it->second;
    }
    return Extension();
}

std::vector<Extension> ExtensionManager::getEnabledExtensions() const {
    std::vector<Extension> result;
    for (const auto& pair : registry_) {
        if (pair.second.enabled) {
            result.push_back(pair.second);
        }
    }
    return result;
}

std::vector<Extension> ExtensionManager::getInstalledExtensions() const {
    std::vector<Extension> result;
    for (const auto& pair : registry_) {
        if (pair.second.installed) {
            result.push_back(pair.second);
        }
    }
    return result;
}

bool ExtensionManager::invokePowerShellExtension(const std::string& name, 
                                                 const std::string& function,
                                                 const std::vector<std::string>& args) {
    if (registry_.find(name) == registry_.end() || !registry_[name].enabled) {
        return false;
    }
    
    std::stringstream cmd;
    cmd << "Import-Module -Name '" << name << "' -Force; ";
    cmd << function;
    
    if (!args.empty()) {
        cmd << " ";
        for (size_t i = 0; i < args.size(); ++i) {
            if (i > 0) cmd << " ";
            cmd << "'" << args[i] << "'";
        }
    }
    
    std::string output;
    return executePowerShellCommand(cmd.str(), output);
}

std::string ExtensionManager::getRegistryPath() const {
    return registryPath_;
}

// Singleton implementation
ExtensionManager& GetExtensionManager() {
    static ExtensionManager instance;
    return instance;
}

} // namespace IDE
