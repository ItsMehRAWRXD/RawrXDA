#pragma once
#include <string>
#include <vector>
#include <iostream>
#include <filesystem>
#include <fstream>
#include <map>

// Extension Loader Logic (VSIX -> Native)
namespace RawrXD {

struct ExtensionInfo {
    std::string name;
    bool isActive;
    bool isNative;
    std::string path;
};

class ExtensionLoader {
private:
    std::map<std::string, ExtensionInfo> m_extensions;
    const std::string m_extensionsDir = "E:\\RawrXD\\extensions\\";

public:
    ExtensionLoader() {
        if (!std::filesystem::exists(m_extensionsDir)) {
            std::filesystem::create_directories(m_extensionsDir);
        }
    }

    void Scan() {
        m_extensions.clear();
        for (const auto& entry : std::filesystem::directory_iterator(m_extensionsDir)) {
            if (entry.is_directory()) {
                std::string name = entry.path().filename().string();
                bool native = std::filesystem::exists(entry.path() / "native_manifest.json");
                m_extensions[name] = { name, true, native, entry.path().string() };
            }
        }
    }

    std::string GetHelp(const std::string& name) {
        if (m_extensions.count(name)) {
            // Try to read README or manifest
            std::string readmePath = m_extensions[name].path + "\\README.md";
            if (std::filesystem::exists(readmePath)) {
                std::ifstream f(readmePath);
                std::stringstream buffer;
                buffer << f.rdbuf();
                return buffer.str();
            }
            return "No help available for " + name;
        }
        return "Extension not found.";
    }

    std::vector<ExtensionInfo> GetExtensions() const {
        std::vector<ExtensionInfo> list;
        for (const auto& kv : m_extensions) list.push_back(kv.second);
        return list;
    }
    
    // Autoload Native Modules (DLLs or scripts)
    void LoadNativeModules() {
        // In a real implementation this would LoadLibrary or run scripts
        // For now we just log
        for (const auto& kv : m_extensions) {
            if (kv.second.isNative) {
                std::cout << "[ExtensionLoader] Loading Native Module: " << kv.first << std::endl;
            }
        }
    }
};

}
