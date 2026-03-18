// SCAFFOLD_241: ExtensionLoader load and unload

#pragma once
#include <string>
#include <vector>
#include <iostream>
#include <filesystem>
#include <fstream>
#include <map>
#include <windows.h>
#include <shlobj.h>
#include "../../include/plugin_signature.h"

// Extension Loader Logic (VSIX -> Native)
// Uses %APPDATA%\RawrXD\extensions (same as VSIXInstaller) so installs work without runtime changes.
namespace RawrXD {

struct ExtensionInfo {
    std::string name;
    bool isActive;
    bool isNative;
    std::string path;
    HMODULE nativeModule = nullptr;  // Loaded DLL handle (null if not loaded)
};

inline std::string GetExtensionsRoot() {
    wchar_t appData[MAX_PATH] = {};
    if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_APPDATA, nullptr, 0, appData))) {
        char buf[MAX_PATH * 2] = {};
        WideCharToMultiByte(CP_UTF8, 0, appData, -1, buf, sizeof(buf), nullptr, nullptr);
        std::string root(buf);
        if (!root.empty() && root.back() != '\\') root += '\\';
        root += "RawrXD\\extensions\\";
        return root;
    }
    return "RawrXD_extensions\\";
}

class ExtensionLoader {
private:
    std::map<std::string, ExtensionInfo> m_extensions;
    std::string m_extensionsDir;

public:
    ExtensionLoader() : m_extensionsDir(GetExtensionsRoot()) {
        if (!std::filesystem::exists(m_extensionsDir)) {
            std::filesystem::create_directories(m_extensionsDir);
        }
    }

    ~ExtensionLoader() {
        // Unload all native modules on destruction
        for (auto& kv : m_extensions) {
            if (kv.second.nativeModule) {
                // Call extension cleanup entry point if it exists
                auto fnShutdown = reinterpret_cast<void(*)()>(
                    GetProcAddress(kv.second.nativeModule, "ExtensionShutdown"));
                if (fnShutdown) fnShutdown();

                FreeLibrary(kv.second.nativeModule);
                kv.second.nativeModule = nullptr;
                std::cout << "[ExtensionLoader] Unloaded native module on shutdown: "
                          << kv.first << std::endl;
            }
        }
    }

    void Scan() {
        m_extensions.clear();
        for (const auto& entry : std::filesystem::directory_iterator(m_extensionsDir)) {
            if (entry.is_directory()) {
                std::string name = entry.path().filename().string();
                bool native = std::filesystem::exists(entry.path() / "native_manifest.json");
                m_extensions[name] = { name, true, native, entry.path().string(), nullptr };
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

    // Load native modules (DLLs) for extensions that have them
    void LoadNativeModules() {
        for (auto& kv : m_extensions) {
            if (kv.second.isNative && !kv.second.nativeModule) {
                // Look for a .dll in the extension directory
                std::string dllPath;
                try {
                    for (const auto& f : std::filesystem::directory_iterator(kv.second.path)) {
                        if (f.path().extension() == ".dll") {
                            dllPath = f.path().string();
                            break;
                        }
                    }
                } catch (...) {}

                if (!dllPath.empty()) {
                    // Enforce signature policy for native extension modules.
                    auto& verifier = RawrXD::Plugin::PluginSignatureVerifier::instance();
                    if (!verifier.isInitialized()) {
                        verifier.initialize(RawrXD::Plugin::PluginSignatureVerifier::createStandardPolicy());
                    }
                    std::wstring wDllPath;
                    try { wDllPath = std::filesystem::path(dllPath).wstring(); } catch (...) {}
                    if (wDllPath.empty()) {
                        // Fallback conversion for odd paths.
                        wchar_t wbuf[MAX_PATH * 4] = {};
                        const int wlen = MultiByteToWideChar(CP_UTF8, 0, dllPath.c_str(), -1, wbuf, (int)(sizeof(wbuf) / sizeof(wbuf[0])));
                        if (wlen > 0) wDllPath.assign(wbuf);
                    }
                    if (wDllPath.empty() || !verifier.shouldAllowInstall(verifier.verify(wDllPath.c_str()))) {
                        std::cout << "[ExtensionLoader] Blocked by signature policy: "
                                  << dllPath << std::endl;
                        continue;
                    }

                    HMODULE hMod = LoadLibraryA(dllPath.c_str());
                    if (hMod) {
                        kv.second.nativeModule = hMod;

                        // Call extension init entry point if it exists
                        auto fnInit = reinterpret_cast<int(*)()>(
                            GetProcAddress(hMod, "ExtensionInit"));
                        if (fnInit) fnInit();

                        std::cout << "[ExtensionLoader] Loaded native module: "
                                  << kv.first << " (" << dllPath << ")" << std::endl;
                    } else {
                        std::cout << "[ExtensionLoader] Failed to load: "
                                  << dllPath << " (error " << GetLastError() << ")" << std::endl;
                    }
                } else {
                    std::cout << "[ExtensionLoader] No DLL found for native extension: "
                              << kv.first << std::endl;
                }
            }
        }
    }

    // Unload a specific extension by name
    // Returns true if successfully unloaded, false if not found or not loaded
    bool UnloadExtension(const std::string& name) {
        auto it = m_extensions.find(name);
        if (it == m_extensions.end()) {
            return false; // Extension not found
        }

        ExtensionInfo& ext = it->second;

        // Unload native DLL if loaded
        if (ext.nativeModule) {
            // Call extension cleanup entry point if it exists
            auto fnShutdown = reinterpret_cast<void(*)()>(
                GetProcAddress(ext.nativeModule, "ExtensionShutdown"));
            if (fnShutdown) {
                fnShutdown();
            }

            BOOL freed = FreeLibrary(ext.nativeModule);
            if (!freed) {
                std::cout << "[ExtensionLoader] FreeLibrary failed for: " << name
                          << " (error " << GetLastError() << ")" << std::endl;
                return false;
            }
            ext.nativeModule = nullptr;
        }

        ext.isActive = false;

        std::cout << "[ExtensionLoader] Unloaded extension: " << name << std::endl;
        return true;
    }

    // Check if a specific extension is loaded
    bool IsExtensionLoaded(const std::string& name) const {
        auto it = m_extensions.find(name);
        if (it == m_extensions.end()) return false;
        return it->second.isActive && (it->second.nativeModule != nullptr || !it->second.isNative);
    }
};

}
