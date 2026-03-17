#pragma once
#include <string>
#include <memory>
#include <expected>
#include <spdlog/spdlog.h>
#include <nlohmann/json.hpp>

// Windows types for HWND
#ifdef _WIN32
#include <windows.h>
#else
using HWND = void*;
#endif

namespace RawrXD {

enum class MonacoVariant { Enterprise };
enum class MonacoThemePreset { Default };

struct MonacoConfig {
    MonacoVariant variant;
    MonacoThemePreset themePreset;
    bool enableIntelliSense;
    bool enableDebugging;
    std::string workspaceRoot;
};

class MonacoEditor {
public:
    RawrXD::Expected<void, std::string> initialize(HWND parent) {
        spdlog::info("Monaco Editor initialized (Shim)");
        return {};
    }
    RawrXD::Expected<void, std::string> loadFile(const std::string& path) { return {}; }
    RawrXD::Expected<void, std::string> saveFile(const std::string& path) { return {}; }
    std::string getCurrentFile() const { return ""; }
    void setText(const std::string& text) {}
    void shutdown() {}
    nlohmann::json getStatus() const { return {{"status", "shim"}}; }
    RawrXD::Expected<bool, std::string> setLanguageServer(const std::string& cmd) { return true; }
    
};

class MonacoFactory {
public:
    static std::unique_ptr<MonacoEditor> createEditor(MonacoVariant v) {
        return std::make_unique<MonacoEditor>();
    }
};

}


