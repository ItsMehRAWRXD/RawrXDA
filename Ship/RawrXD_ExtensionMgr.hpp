// RawrXD_ExtensionMgr.hpp - Production-Ready Extension Management
// Pure C++20 - No Qt Dependencies
// Manages: Registry tracking, PowerShell invocation, Marketplace integration

#pragma once

#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

#include <windows.h>
#include <string>
#include <vector>
#include <map>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <iostream>

namespace fs = std::filesystem;

namespace RawrXD {

struct Extension {
    std::string name;
    std::string type;
    std::string path;
    bool installed = false;
    bool enabled = false;
    std::string created;
};

class ExtensionManager {
public:
    ExtensionManager() {
        extensionRoot_ = "d:\\RawrXD\\extensions";
        moduleStore_ = "d:\\RawrXD\\scripts\\modules";
        registryPath_ = "d:\\RawrXD\\extensions\\registry.json";

        try {
            if (!fs::exists(extensionRoot_)) fs::create_directories(extensionRoot_);
            if (!fs::exists(moduleStore_)) fs::create_directories(moduleStore_);
        } catch (...) {}
        
        loadRegistry();
    }

    ~ExtensionManager() {
        saveRegistry();
    }

    bool loadRegistry() {
        std::ifstream file(registryPath_);
        if (!file.is_open()) return false;

        std::stringstream buffer;
        buffer << file.rdbuf();
        return parseRegistryJson(buffer.str());
    }

    bool saveRegistry() {
        std::ofstream file(registryPath_);
        if (!file.is_open()) return false;

        file << generateRegistryJson();
        return true;
    }

    bool installExtension(const std::string& name) {
        if (registry_.find(name) == registry_.end()) return false;
        
        std::string scriptPath = "d:\\RawrXD\\ExtensionManager.ps1";
        std::string cmd = ". '" + scriptPath + "'; Install-Extension -Name '" + name + "'";
        
        std::string output;
        if (executePowerShell(cmd, output)) {
            registry_[name].installed = true;
            saveRegistry();
            return true;
        }
        return false;
    }

    bool enableExtension(const std::string& name) {
        if (registry_.find(name) == registry_.end()) return false;
        
        if (!registry_[name].installed) {
            if (!installExtension(name)) return false;
        }
        
        std::string scriptPath = "d:\\RawrXD\\ExtensionManager.ps1";
        std::string cmd = ". '" + scriptPath + "'; Enable-Extension -Name '" + name + "'";
        
        std::string output;
        if (executePowerShell(cmd, output)) {
            registry_[name].enabled = true;
            saveRegistry();
            return true;
        }
        return false;
    }

    bool disableExtension(const std::string& name) {
        if (registry_.find(name) == registry_.end()) return false;
        
        std::string scriptPath = "d:\\RawrXD\\ExtensionManager.ps1";
        std::string cmd = ". '" + scriptPath + "'; Disable-Extension -Name '" + name + "'";
        
        std::string output;
        if (executePowerShell(cmd, output)) {
            registry_[name].enabled = false;
            saveRegistry();
            return true;
        }
        return false;
    }

    std::vector<Extension> listExtensions() const {
        std::vector<Extension> result;
        for (const auto& [name, ext] : registry_) result.push_back(ext);
        return result;
    }

    bool executePowerShell(const std::string& command, std::string& output) {
        std::string fullCmd = "powershell.exe -NoProfile -ExecutionPolicy Bypass -Command \"" + command + "\"";
        
        FILE* pipe = _popen(fullCmd.c_str(), "r");
        if (!pipe) return false;
        
        char buffer[1024];
        while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
            output += buffer;
        }
        
        return _pclose(pipe) == 0;
    }

private:
    std::string extensionRoot_;
    std::string moduleStore_;
    std::string registryPath_;
    std::map<std::string, Extension> registry_;

    bool parseRegistryJson(const std::string& json) {
        registry_.clear();
        // Simple manual parsing for no-dependency implementation
        size_t pos = 0;
        while ((pos = json.find("\"", pos)) != std::string::npos) {
            size_t nameStart = pos + 1;
            size_t nameEnd = json.find("\"", nameStart);
            if (nameEnd == std::string::npos) break;
            
            std::string name = json.substr(nameStart, nameEnd - nameStart);
            size_t objStart = json.find("{", nameEnd);
            if (objStart == std::string::npos) break;
            
            Extension ext;
            ext.name = name;
            
            auto extractValue = [&](const std::string& key) -> std::string {
                size_t kPos = json.find("\"" + key + "\"", objStart);
                if (kPos == std::string::npos || kPos > json.find("}", objStart)) return "";
                size_t vStart = json.find("\"", kPos + key.length() + 2) + 1;
                size_t vEnd = json.find("\"", vStart);
                return json.substr(vStart, vEnd - vStart);
            };

            auto extractBool = [&](const std::string& key) -> bool {
                size_t kPos = json.find("\"" + key + "\"", objStart);
                if (kPos == std::string::npos || kPos > json.find("}", objStart)) return false;
                size_t vStart = json.find(":", kPos) + 1;
                while (vStart < json.length() && (json[vStart] == ' ' || json[vStart] == '\t')) vStart++;
                return json.compare(vStart, 4, "true") == 0;
            };

            ext.type = extractValue("Type");
            ext.path = extractValue("Path");
            ext.created = extractValue("Created");
            ext.installed = extractBool("Installed");
            ext.enabled = extractBool("Enabled");
            
            registry_[name] = ext;
            pos = json.find("}", objStart) + 1;
        }
        return true;
    }

    std::string generateRegistryJson() const {
        std::stringstream json;
        json << "{\n";
        bool first = true;
        for (const auto& [name, ext] : registry_) {
            if (!first) json << ",\n";
            first = false;
            json << "  \"" << name << "\": {\n";
            json << "    \"Type\": \"" << ext.type << "\",\n";
            json << "    \"Path\": \"" << ext.path.substr(0, ext.path.find_last_of("\\") == std::string::npos ? 0 : ext.path.find_last_of("\\")) << "\\\\...\",\n"; // Sanitized
            json << "    \"Installed\": " << (ext.installed ? "true" : "false") << ",\n";
            json << "    \"Enabled\": " << (ext.enabled ? "true" : "false") << ",\n";
            json << "    \"Created\": \"" << ext.created << "\"\n";
            json << "  }";
        }
        json << "\n}\n";
        return json.str();
    }
};

static ExtensionManager& GetExtensionManager() {
    static ExtensionManager instance;
    return instance;
}

} // namespace RawrXD
