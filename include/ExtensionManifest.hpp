// ExtensionManifest.hpp — Schema for RawrXD extension packages
//
// Each extension is a directory containing:
//   manifest.json   — this schema
//   README.md       — human-readable docs
//   (optional) native/ — native DLLs + native_manifest.json
//   (optional) scripts/ — .js or .ps1 scripts
//   (optional) assets/ — icons, themes, etc.
//
// Manifest version: 1.0.0

#pragma once
#include <nlohmann/json.hpp>
#include <string>
#include <vector>
#include <optional>

namespace RawrXD {

// Extension capability flags — what this extension can do
enum class ExtensionCapability : uint32_t {
    None = 0,
    ToolProvider = 1 << 0,      // Registers agentic tools
    ThemeProvider = 1 << 2,     // Provides UI themes
    LanguageSupport = 1 << 3,   // Syntax highlighting, LSP bridge
    NativeModule = 1 << 4,      // Loads native DLL
    ScriptModule = 1 << 5,      // Loads JS/PS1 scripts
    All = 0xFFFFFFFF
};

inline ExtensionCapability operator|(ExtensionCapability a, ExtensionCapability b) {
    return static_cast<ExtensionCapability>(
        static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}
inline bool hasCapability(ExtensionCapability flags, ExtensionCapability cap) {
    return (static_cast<uint32_t>(flags) & static_cast<uint32_t>(cap)) != 0;
}

// Runtime requirement — what the host must provide
struct ExtensionRequirement {
    std::string name;           // e.g. "rawrxd", "win32ide", "lsp"
    std::string minVersion;     // semver, e.g. "14.7.0"
    std::string maxVersion;     // optional, e.g. "15.0.0"
    bool optional = false;      // soft dependency
};

// Single entry point — what the host calls to activate this extension
struct ExtensionEntryPoint {
    std::string type;           // "native_dll", "script_js", "script_ps1", "tool_register"
    std::string path;           // relative to extension root
    std::string exportName;     // for native: exported function name
    std::vector<std::string> args; // startup arguments
};

// Tool registration — extensions can register agentic tools
struct ExtensionToolRegistration {
    std::string name;
    std::string description;
    std::string schemaPath;     // JSON schema for tool args
    std::string handlerPath;    // script or native export
};

// Full manifest — parsed from manifest.json
struct ExtensionManifest {
    // Identity
    std::string id;             // unique reverse-domain, e.g. "com.rawrxd.theme-dark"
    std::string name;           // human-readable
    std::string version;        // semver
    std::string description;
    std::string author;
    std::string license;
    std::string homepage;
    std::string repository;

    // Capabilities
    ExtensionCapability capabilities = ExtensionCapability::None;

    // Requirements
    std::vector<ExtensionRequirement> requirements;

    // Entry points
    std::vector<ExtensionEntryPoint> entryPoints;

    // Tool registrations
    std::vector<ExtensionToolRegistration> tools;

    // Native-specific
    std::optional<std::string> nativeDllPath;   // relative path to DLL
    std::optional<std::string> nativeEntryPoint; // exported init function

    // Metadata
    std::vector<std::string> keywords;
    std::vector<std::string> categories;
    std::optional<std::string> iconPath;
    std::optional<std::string> previewImagePath;

    // Parse from JSON string
    static ExtensionManifest fromJson(const std::string& jsonStr);
    static ExtensionManifest fromFile(const std::string& path);

    // Serialize to JSON
    std::string toJson() const;

    // Validate manifest completeness
    bool isValid(std::string* outError = nullptr) const;

    // Check if this extension is compatible with host version
    bool isCompatibleWithHost(const std::string& hostVersion) const;
};

// Download source — where to fetch an extension from
struct ExtensionSource {
    std::string name;           // e.g. "rawrxd-marketplace", "github-releases"
    std::string url;            // base URL or API endpoint
    std::string type;           // "marketplace", "github", "direct"
    bool trusted = false;       // pre-verified source
};

// Download task state
enum class ExtensionDownloadState {
    Pending,
    Downloading,
    Verifying,
    Installing,
    Completed,
    Failed,
    Cancelled
};

struct ExtensionDownloadProgress {
    std::string taskId;
    std::string extensionId;
    ExtensionDownloadState state = ExtensionDownloadState::Pending;
    uint64_t bytesTotal = 0;
    uint64_t bytesDone = 0;
    double speedBps = 0.0;
    double etaSeconds = 0.0;
    std::string errorMessage;
    int retryCount = 0;
};

} // namespace RawrXD
