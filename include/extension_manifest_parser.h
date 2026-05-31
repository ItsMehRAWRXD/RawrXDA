// ============================================================================
// extension_manifest_parser.h — Extension Manifest (package.json) Parser
// ============================================================================
// PURPOSE:
//   Parses VS Code extension package.json manifests to extract:
//   - Metadata (name, version, author, publisher)
//   - Activation events
//   - Contribution points (commands, languages, keybindings, etc.)
//   - Dependencies and development dependencies
//   - Engine version requirements
//   - Extension type (UI, workspace)
//
// Architecture: C++20 | Win32 | nlohmann/json | No exceptions | Qt-free
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED
// ============================================================================

#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <nlohmann/json.hpp>

namespace RawrXD {
namespace Extensions {

using json = nlohmann::json;

// ============================================================================
// Manifest Structures
// ============================================================================

struct CommandContribution {
    std::string id;                  // e.g., "myExt.helloWorld"
    std::string title;               // Display name
    std::string category;            // Command category
    std::string description;         // Short description
    std::unordered_map<std::string, std::string> keybinding;  // Optional
};

struct LanguageContribution {
    std::string id;                  // Language ID (e.g., "python", "rust")
    std::vector<std::string> aliases;
    std::vector<std::string> extensions;
    std::string mimeType;
};

struct KeybindingContribution {
    std::string command;
    std::string key;
    std::string mac;                 // macOS-specific
    std::string linux;               // Linux-specific
    std::string win;                 // Windows-specific
};

struct ThemeContribution {
    std::string id;
    std::string label;
    std::string uiTheme;             // "vs", "vs-dark", "hc-black"
    std::string path;
};

struct ViewContribution {
    std::string id;
    std::string title;
    std::string icon;
    std::string priority;            // "top", "middle", "bottom"
};

struct ExtensionContributions {
    std::vector<CommandContribution>      commands;
    std::vector<LanguageContribution>     languages;
    std::vector<KeybindingContribution>   keybindings;
    std::vector<ThemeContribution>        themes;
    std::vector<ViewContribution>         views;
    std::vector<std::string>              grammars;
    std::unordered_map<std::string, std::string> configuration;
    
    bool isEmpty() const {
        return commands.empty() && languages.empty() && keybindings.empty()
            && themes.empty() && views.empty() && grammars.empty()
            && configuration.empty();
    }
};

// ============================================================================
// Extension Manifest
// ============================================================================

struct ExtensionManifest {
    // Core metadata
    std::string name;                // Package name (e.g., "python")
    std::string version;             // Semantic version
    std::string displayName;         // Display name in UI
    std::string description;         // Detailed description
    std::string publisher;           // Publisher ID
    std::string author;              // Author name(s)
    std::string license;             // License type
    
    std::string main;                // Main entry point (JS/TS file)
    std::string icon;                // Icon path
    std::string galleryBanner;       // Banner for marketplace
    std::string repository;          // Repository URL
    std::string bugs;                // Bug tracker URL
    std::string homepage;            // Homepage URL
    
    // Version constraints
    std::string engineVersion;       // VS Code version requirement (e.g., "^1.64.0")
    std::string vscodeVersion;       // Alias for engineVersion
    
    // Categories
    std::vector<std::string> categories;
    
    // Keywords for marketplace search
    std::vector<std::string> keywords;
    
    // Activation events
    std::vector<std::string> activationEvents;
    
    // Contributions
    ExtensionContributions contributes;
    
    // Dependencies
    std::unordered_map<std::string, std::string> dependencies;
    std::unordered_map<std::string, std::string> devDependencies;
    
    // Extension type ("ui" or "workspace")
    std::string extensionKind;
    
    // Flags
    bool preview = false;            // Preview extension
    bool deprecated = false;         // Deprecated extension
    
    std::string ToString() const {
        return publisher + "." + name + "@" + version;
    }
};

// ============================================================================
// Parse Result
// ============================================================================

struct ManifestParseResult {
    bool                       success = false;
    std::string                errorMessage;
    std::unique_ptr<ExtensionManifest> manifest;
    
    static ManifestParseResult Ok(std::unique_ptr<ExtensionManifest> m) {
        ManifestParseResult r;
        r.success = true;
        r.manifest = std::move(m);
        return r;
    }
    
    static ManifestParseResult Error(const std::string& msg) {
        ManifestParseResult r;
        r.success = false;
        r.errorMessage = msg;
        return r;
    }
};

// ============================================================================
// Extension Manifest Parser
// ============================================================================

class ExtensionManifestParser {
public:
    // Parse from JSON object
    static ManifestParseResult Parse(const json& packageJson);
    
    // Parse from JSON string
    static ManifestParseResult ParseString(const std::string& jsonString);
    
    // Parse from file
    static ManifestParseResult ParseFile(const std::string& filePath);
    
    // Validate manifest against requirements
    static bool Validate(const ExtensionManifest& manifest,
                         std::string& outErrorMessage);
    
    // Extract specific fields
    static std::vector<std::string> ExtractActivationEvents(const json& packageJson);
    static ExtensionContributions ExtractContributions(const json& packageJson);
    static std::vector<std::string> ExtractDependencies(const json& packageJson);

private:
    static void ParseMetadata(const json& root, ExtensionManifest& out);
    static void ParseActivationEvents(const json& root, ExtensionManifest& out);
    static void ParseContributions(const json& root, ExtensionManifest& out);
    static void ParseDependencies(const json& root, ExtensionManifest& out);
    
    static CommandContribution ParseCommand(const json& obj);
    static LanguageContribution ParseLanguage(const json& obj);
    static KeybindingContribution ParseKeybinding(const json& obj);
    static ThemeContribution ParseTheme(const json& obj);
    static ViewContribution ParseView(const json& obj);
};

// ============================================================================
// Global Helper
// ============================================================================

// Parse extension from directory (looks for package.json)
ManifestParseResult ParseExtensionDirectory(const std::string& extensionDir);

}  // namespace Extensions
}  // namespace RawrXD

#endif  // EXTENSION_MANIFEST_PARSER_H
