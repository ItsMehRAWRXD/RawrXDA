// ExtensionLoader.hpp — Extension discovery, native DLL load/unload
//
// Uses %APPDATA%\RawrXD\extensions\ so installs work without runtime changes.
// Each subdirectory is an extension. If it contains native_manifest.json the
// extension is treated as native and the first .dll found is loaded after
// Authenticode / plugin-signature verification.

#pragma once

#include <windows.h>
#include <shlobj.h>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include "../../include/plugin_signature.h"

namespace RawrXD {

struct ExtensionInfo {
    std::string name;
    bool isActive = false;
    bool isNative = false;
    std::string path;
    HMODULE nativeModule = nullptr;
};

inline std::string GetExtensionsRoot() {
    wchar_t appData[MAX_PATH] = {};
    if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_APPDATA, nullptr, 0, appData))) {
        char buf[MAX_PATH * 2] = {};
        WideCharToMultiByte(CP_UTF8, 0, appData, -1, buf, sizeof(buf), nullptr, nullptr);
        std::string root(buf);
        if (!root.empty() && root.back() != '\\') {
            root += '\\';
        }
        root += "RawrXD\\extensions\\";
        return root;
    }
    return "RawrXD_extensions\\";
}

class ExtensionLoader {
private:
    std::map<std::string, ExtensionInfo> m_extensions;
    std::string m_extensionsDir;

    // Shared helper: call ExtensionShutdown then FreeLibrary on a single ext.
    // Returns true if FreeLibrary succeeded (or module was already null).
    static bool ShutdownAndFree(ExtensionInfo& ext, const char* context) {
        if (!ext.nativeModule) {
            return true;
        }

        auto fnShutdown = reinterpret_cast<void(*)()>(
            GetProcAddress(ext.nativeModule, "ExtensionShutdown"));
        if (fnShutdown) {
            fnShutdown();
        }

        const BOOL freed = FreeLibrary(ext.nativeModule);
        if (!freed) {
            std::cout << "[ExtensionLoader] FreeLibrary failed (" << context
                      << "): " << ext.name
                      << " (error " << GetLastError() << ")" << std::endl;
        }

        ext.nativeModule = nullptr;
        ext.isActive = false;
        return freed != 0;
    }

    // Build the wide path for a DLL, trying std::filesystem first and falling
    // back to MultiByteToWideChar for non-BMP or exotic paths.
    static std::wstring ToWidePath(const std::string& narrowPath) {
        std::wstring wPath;
        try {
            wPath = std::filesystem::path(narrowPath).wstring();
        } catch (...) {
        }

        if (wPath.empty()) {
            wchar_t wbuf[MAX_PATH * 4] = {};
            const int wlen = MultiByteToWideChar(
                CP_UTF8, 0, narrowPath.c_str(), -1, wbuf, MAX_PATH * 4);
            if (wlen > 0) {
                wPath.assign(wbuf);
            }
        }
        return wPath;
    }

    // Locate the first .dll inside an extension directory.
    static std::string FindDll(const std::string& extDir) {
        try {
            for (const auto& f : std::filesystem::directory_iterator(extDir)) {
                if (f.path().extension() == ".dll") {
                    return f.path().string();
                }
            }
        } catch (...) {
        }
        return {};
    }

    // Verify the DLL signature using the project-wide plugin verifier.
    static bool VerifyDllSignature(const std::wstring& wDllPath) {
        auto& verifier = RawrXD::Plugin::PluginSignatureVerifier::instance();
        if (!verifier.isInitialized()) {
            verifier.initialize(
                RawrXD::Plugin::PluginSignatureVerifier::createStandardPolicy());
        }
        return verifier.shouldAllowInstall(verifier.verify(wDllPath.c_str()));
    }

    // Load a single native extension DLL after signature check.
    // Returns true if the module was loaded and ExtensionInit succeeded.
    bool LoadSingleNative(ExtensionInfo& ext) {
        const std::string dllPath = FindDll(ext.path);
        if (dllPath.empty()) {
            std::cout << "[ExtensionLoader] No DLL found for native extension: "
                      << ext.name << std::endl;
            return false;
        }

        const std::wstring wDllPath = ToWidePath(dllPath);
        if (wDllPath.empty()) {
            std::cout << "[ExtensionLoader] Failed to convert path to wide: "
                      << dllPath << std::endl;
            return false;
        }

        if (!VerifyDllSignature(wDllPath)) {
            std::cout << "[ExtensionLoader] Blocked by signature policy: "
                      << dllPath << std::endl;
            return false;
        }

        // Use LoadLibraryExW with the wide path we already built.
        // LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR lets the extension resolve its own
        // sibling DLLs; LOAD_LIBRARY_SEARCH_SYSTEM32 ensures system DLLs are
        // still reachable.
        HMODULE hMod = LoadLibraryExW(
            wDllPath.c_str(), nullptr,
            LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32);
        if (!hMod) {
            std::cout << "[ExtensionLoader] Failed to load: "
                      << dllPath << " (error " << GetLastError() << ")" << std::endl;
            return false;
        }

        ext.nativeModule = hMod;

        auto fnInit = reinterpret_cast<int(*)()>(
            GetProcAddress(hMod, "ExtensionInit"));
        if (fnInit) {
            const int initResult = fnInit();
            if (initResult != 0) {
                std::cout << "[ExtensionLoader] ExtensionInit failed for: "
                          << ext.name << " (code " << initResult << ")" << std::endl;
                FreeLibrary(hMod);
                ext.nativeModule = nullptr;
                ext.isActive = false;
                return false;
            }
        }

        ext.isActive = true;
        std::cout << "[ExtensionLoader] Loaded native module: "
                  << ext.name << " (" << dllPath << ")" << std::endl;
        return true;
    }

public:
    ExtensionLoader() : m_extensionsDir(GetExtensionsRoot()) {
        if (!std::filesystem::exists(m_extensionsDir)) {
            std::filesystem::create_directories(m_extensionsDir);
        }
    }

    ~ExtensionLoader() {
        for (auto& kv : m_extensions) {
            if (ShutdownAndFree(kv.second, "shutdown")) {
                std::cout << "[ExtensionLoader] Unloaded native module on shutdown: "
                          << kv.first << std::endl;
            }
        }
    }

    // Non-copyable / non-movable — the loaded HMODULEs are process-global.
    ExtensionLoader(const ExtensionLoader&) = delete;
    ExtensionLoader& operator=(const ExtensionLoader&) = delete;

    const std::string& GetExtensionsDir() const { return m_extensionsDir; }

    // Re-scan the extensions directory.  Any previously loaded native modules
    // are shut down first so that a fresh Scan() + LoadNativeModules() cycle
    // never leaks HMODULEs.
    void Scan() {
        // Unload every native module that is still alive before we clear the map.
        for (auto& kv : m_extensions) {
            ShutdownAndFree(kv.second, "rescan");
        }
        m_extensions.clear();

        try {
            for (const auto& entry : std::filesystem::directory_iterator(m_extensionsDir)) {
                if (!entry.is_directory()) {
                    continue;
                }

                const std::string name = entry.path().filename().string();
                const bool native =
                    std::filesystem::exists(entry.path() / "native_manifest.json");

                ExtensionInfo info;
                info.name = name;
                info.isActive = true;
                info.isNative = native;
                info.path = entry.path().string();

                m_extensions[name] = std::move(info);
            }
        } catch (const std::filesystem::filesystem_error& e) {
            std::cerr << "[ExtensionLoader] Failed to scan extensions directory '"
                      << m_extensionsDir << "': " << e.what() << std::endl;
        }
    }

    std::string GetHelp(const std::string& name) const {
        auto it = m_extensions.find(name);
        if (it == m_extensions.end()) {
            return "Extension not found.";
        }

        const std::string readmePath = it->second.path + "\\README.md";
        if (!std::filesystem::exists(readmePath)) {
            return "No help available for " + name;
        }

        std::ifstream f(readmePath);
        std::stringstream buffer;
        buffer << f.rdbuf();
        return buffer.str();
    }

    std::vector<ExtensionInfo> GetExtensions() const {
        std::vector<ExtensionInfo> list;
        list.reserve(m_extensions.size());
        for (const auto& kv : m_extensions) {
            list.push_back(kv.second);
        }
        return list;
    }

    const ExtensionInfo* GetExtension(const std::string& name) const {
        auto it = m_extensions.find(name);
        return it != m_extensions.end() ? &it->second : nullptr;
    }

    void LoadNativeModules() {
        for (auto& kv : m_extensions) {
            ExtensionInfo& ext = kv.second;
            if (!ext.isNative || ext.nativeModule) {
                continue;
            }
            LoadSingleNative(ext);
        }
    }

    bool UnloadExtension(const std::string& name) {
        auto it = m_extensions.find(name);
        if (it == m_extensions.end()) {
            return false;
        }

        ExtensionInfo& ext = it->second;
        const bool ok = ShutdownAndFree(ext, "unload");
        ext.isActive = false;
        std::cout << "[ExtensionLoader] Unloaded extension: " << name << std::endl;
        return ok;
    }

    bool ReloadExtension(const std::string& name) {
        auto it = m_extensions.find(name);
        if (it == m_extensions.end()) {
            return false;
        }

        ExtensionInfo& ext = it->second;
        ShutdownAndFree(ext, "reload");

        if (!ext.isNative) {
            ext.isActive = true;
            return true;
        }

        return LoadSingleNative(ext);
    }

    bool EnableExtension(const std::string& name) {
        auto it = m_extensions.find(name);
        if (it == m_extensions.end()) {
            return false;
        }

        ExtensionInfo& ext = it->second;
        if (ext.isActive) {
            return true;
        }

        if (ext.isNative && !ext.nativeModule) {
            return LoadSingleNative(ext);
        }

        ext.isActive = true;
        return true;
    }

    bool DisableExtension(const std::string& name) {
        auto it = m_extensions.find(name);
        if (it == m_extensions.end()) {
            return false;
        }

        ExtensionInfo& ext = it->second;
        ShutdownAndFree(ext, "disable");
        ext.isActive = false;
        return true;
    }

    bool IsExtensionLoaded(const std::string& name) const {
        auto it = m_extensions.find(name);
        if (it == m_extensions.end()) {
            return false;
        }

        const ExtensionInfo& ext = it->second;
        if (!ext.isActive) {
            return false;
        }

        if (!ext.isNative) {
            return true;
        }

        return ext.nativeModule != nullptr;
    }
};

} // namespace RawrXD
