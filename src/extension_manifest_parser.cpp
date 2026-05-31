// ============================================================================
// extension_manifest_parser.cpp — Extension Manifest (package.json) Parser Implementation
// ============================================================================
// Architecture: C++20 | Win32 | nlohmann/json | Function-pointer callbacks | No exceptions
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED
// ============================================================================

#include "extension_manifest_parser.h"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <regex>

namespace fs = std::filesystem;

namespace RawrXD {
namespace Extensions {

// ============================================================================
// Helper Functions
// ============================================================================

static std::string TrimWhitespace(const std::string& str) {
    size_t start = str.find_first_not_of(" \t\r\n");
    size_t end = str.find_last_not_of(" \t\r\n");
    
    if (start == std::string::npos) return "";
    return str.substr(start, end - start + 1);
}

static std::string ToLowerAscii(const std::string& str) {
    std::string result = str;
    for (auto& c : result) {
        if (c >= 'A' && c <= 'Z') {
            c = c - 'A' + 'a';
        }
    }
    return result;
}

static bool SafeGetString(const json& obj, const std::string& key, std::string& outValue) {
    try {
        if (obj.contains(key) && obj[key].is_string()) {
            outValue = obj[key].get<std::string>();
            return true;
        }
    } catch (...) {
        fprintf(stderr, "[ExtensionManifest] JSON access exception in SafeGetString\n");
    }
    return false;
}

static bool SafeGetArray(const json& obj, const std::string& key, std::vector<std::string>& outArray) {
    try {
        if (obj.contains(key) && obj[key].is_array()) {
            for (const auto& item : obj[key]) {
                if (item.is_string()) {
                    outArray.push_back(item.get<std::string>());
                }
            }
            return true;
        }
    } catch (...) {
        fprintf(stderr, "[ExtensionManifest] JSON access exception in SafeGetArray\n");
    }
    return false;
}

static bool SafeGetObject(const json& obj, const std::string& key, json& outObj) {
    try {
        if (obj.contains(key) && obj[key].is_object()) {
            outObj = obj[key];
            return true;
        }
    } catch (...) {
        fprintf(stderr, "[ExtensionManifest] JSON access exception in SafeGetObject\n");
    }
    return false;
}

static bool SafeGetBool(const json& obj, const std::string& key, bool& outValue) {
    try {
        if (obj.contains(key) && obj[key].is_boolean()) {
            outValue = obj[key].get<bool>();
            return true;
        }
    } catch (...) {
        fprintf(stderr, "[ExtensionManifest] JSON access exception in SafeGetBool\n");
    }
    return false;
}

// ============================================================================
// Parsing Helpers
// ============================================================================

CommandContribution ExtensionManifestParser::ParseCommand(const json& obj) {
    CommandContribution cmd;
    SafeGetString(obj, "command", cmd.id);
    SafeGetString(obj, "title", cmd.title);
    SafeGetString(obj, "category", cmd.category);
    SafeGetString(obj, "description", cmd.description);

    // Parse keybinding if present
    if (obj.contains("keybinding") && obj["keybinding"].is_object()) {
        const auto& kb = obj["keybinding"];
        std::string val;
        if (SafeGetString(kb, "key", val)) cmd.keybinding["key"] = val;
        if (SafeGetString(kb, "mac", val)) cmd.keybinding["mac"] = val;
        if (SafeGetString(kb, "linux", val)) cmd.keybinding["linux"] = val;
        if (SafeGetString(kb, "win", val)) cmd.keybinding["win"] = val;
        if (SafeGetString(kb, "when", val)) cmd.keybinding["when"] = val;
    }
    // Also support flat keybinding fields at command level
    if (obj.contains("key") && obj["key"].is_string()) {
        cmd.keybinding["key"] = obj["key"].get<std::string>();
    }

    return cmd;
}

LanguageContribution ExtensionManifestParser::ParseLanguage(const json& obj) {
    LanguageContribution lang;
    SafeGetString(obj, "id", lang.id);
    SafeGetArray(obj, "aliases", lang.aliases);
    SafeGetArray(obj, "extensions", lang.extensions);
    SafeGetString(obj, "mimeType", lang.mimeType);
    return lang;
}

KeybindingContribution ExtensionManifestParser::ParseKeybinding(const json& obj) {
    KeybindingContribution kb;
    SafeGetString(obj, "command", kb.command);
    SafeGetString(obj, "key", kb.key);
    SafeGetString(obj, "mac", kb.mac);
    SafeGetString(obj, "linux", kb.linux);
    SafeGetString(obj, "win", kb.win);
    return kb;
}

ThemeContribution ExtensionManifestParser::ParseTheme(const json& obj) {
    ThemeContribution theme;
    SafeGetString(obj, "id", theme.id);
    SafeGetString(obj, "label", theme.label);
    SafeGetString(obj, "uiTheme", theme.uiTheme);
    SafeGetString(obj, "path", theme.path);
    return theme;
}

ViewContribution ExtensionManifestParser::ParseView(const json& obj) {
    ViewContribution view;
    SafeGetString(obj, "id", view.id);
    SafeGetString(obj, "title", view.title);
    SafeGetString(obj, "icon", view.icon);
    SafeGetString(obj, "priority", view.priority);
    return view;
}

void ExtensionManifestParser::ParseMetadata(const json& root, ExtensionManifest& out) {
    SafeGetString(root, "name", out.name);
    SafeGetString(root, "version", out.version);
    SafeGetString(root, "displayName", out.displayName);
    SafeGetString(root, "description", out.description);
    SafeGetString(root, "publisher", out.publisher);
    SafeGetString(root, "author", out.author);
    SafeGetString(root, "license", out.license);
    SafeGetString(root, "main", out.main);
    SafeGetString(root, "icon", out.icon);
    SafeGetString(root, "galleryBanner", out.galleryBanner);
    SafeGetString(root, "repository", out.repository);
    SafeGetString(root, "bugs", out.bugs);
    SafeGetString(root, "homepage", out.homepage);
    
    // Engine / VS Code version
    SafeGetString(root, "engines.vscode", out.vscodeVersion);
    if (out.vscodeVersion.empty()) {
        SafeGetString(root, "engines.VSCode", out.vscodeVersion);
    }
    out.engineVersion = out.vscodeVersion;
    
    // Categories and keywords
    SafeGetArray(root, "categories", out.categories);
    SafeGetArray(root, "keywords", out.keywords);
    
    // Flags
    SafeGetBool(root, "preview", out.preview);
    SafeGetBool(root, "deprecated", out.deprecated);
    
    // Extension kind
    SafeGetString(root, "extensionKind", out.extensionKind);
}

void ExtensionManifestParser::ParseActivationEvents(const json& root, ExtensionManifest& out) {
    SafeGetArray(root, "activationEvents", out.activationEvents);
}

void ExtensionManifestParser::ParseContributions(const json& root, ExtensionManifest& out) {
    json contrib;
    if (!SafeGetObject(root, "contributes", contrib)) {
        return;  // No contributions
    }

    // Parse commands
    try {
        if (contrib.contains("commands") && contrib["commands"].is_array()) {
            for (const auto& cmd : contrib["commands"]) {
                out.contributes.commands.push_back(ParseCommand(cmd));
            }
        }
    } catch (...) {
        fprintf(stderr, "[ExtensionManifest] Skipped malformed command\n");
    }

    // Parse languages
    try {
        if (contrib.contains("languages") && contrib["languages"].is_array()) {
            for (const auto& lang : contrib["languages"]) {
                out.contributes.languages.push_back(ParseLanguage(lang));
            }
        }
    } catch (...) {
        fprintf(stderr, "[ExtensionManifest] Skipped malformed language\n");
    }

    // Parse keybindings
    try {
        if (contrib.contains("keybindings") && contrib["keybindings"].is_array()) {
            for (const auto& kb : contrib["keybindings"]) {
                out.contributes.keybindings.push_back(ParseKeybinding(kb));
            }
        }
    } catch (...) {
        fprintf(stderr, "[ExtensionManifest] Skipped malformed keybinding\n");
    }

    // Parse themes
    try {
        if (contrib.contains("themes") && contrib["themes"].is_array()) {
            for (const auto& theme : contrib["themes"]) {
                out.contributes.themes.push_back(ParseTheme(theme));
            }
        }
    } catch (...) {
        fprintf(stderr, "[ExtensionManifest] Skipped malformed themes\n");
    }

    // Parse configuration
    try {
        if (contrib.contains("configuration") && contrib["configuration"].is_object()) {
            for (auto& [key, val] : contrib["configuration"].items()) {
                if (val.is_string()) {
                    out.contributes.configuration[key] = val.get<std::string>();
                }
            }
        }
    } catch (...) {
        fprintf(stderr, "[ExtensionManifest] Skipped malformed configuration\n");
    }

    // Parse grammars
    try {
        SafeGetArray(contrib, "grammars", out.contributes.grammars);
    } catch (...) {
        fprintf(stderr, "[ExtensionManifest] Skipped malformed grammars\n");
    }
}

void ExtensionManifestParser::ParseDependencies(const json& root, ExtensionManifest& out) {
    json depsObj;
    if (SafeGetObject(root, "dependencies", depsObj)) {
        try {
            for (auto& [key, val] : depsObj.items()) {
                if (val.is_string()) {
                    out.dependencies[key] = val.get<std::string>();
                }
            }
        } catch (...) {
            fprintf(stderr, "[ExtensionManifest] Skipped malformed dependency\n");
        }
    }

    json devDepsObj;
    if (SafeGetObject(root, "devDependencies", devDepsObj)) {
        try {
            for (auto& [key, val] : devDepsObj.items()) {
                if (val.is_string()) {
                    out.devDependencies[key] = val.get<std::string>();
                }
            }
        } catch (...) {
            fprintf(stderr, "[ExtensionManifest] Skipped malformed devDependency\n");
        }
    }
}

// ============================================================================
// Public Parse Methods
// ============================================================================

ManifestParseResult ExtensionManifestParser::Parse(const json& packageJson) {
    auto manifest = std::make_unique<ExtensionManifest>();

    try {
        ParseMetadata(packageJson, *manifest);
        ParseActivationEvents(packageJson, *manifest);
        ParseContributions(packageJson, *manifest);
        ParseDependencies(packageJson, *manifest);

        return ManifestParseResult::Ok(std::move(manifest));
    } catch (const std::exception& e) {
        return ManifestParseResult::Error(std::string("Parse error: ") + e.what());
    }
}

ManifestParseResult ExtensionManifestParser::ParseString(const std::string& jsonString) {
    if (jsonString.empty()) {
        return ManifestParseResult::Error("JSON string is empty");
    }

    try {
        json obj = json::parse(jsonString);
        return Parse(obj);
    } catch (const json::parse_error& e) {
        return ManifestParseResult::Error(std::string("JSON parse error: ") + e.what());
    } catch (const std::exception& e) {
        return ManifestParseResult::Error(std::string("Error: ") + e.what());
    }
}

ManifestParseResult ExtensionManifestParser::ParseFile(const std::string& filePath) {
    if (filePath.empty()) {
        return ManifestParseResult::Error("File path is empty");
    }

    try {
        std::ifstream file(filePath);
        if (!file.is_open()) {
            return ManifestParseResult::Error("Cannot open file: " + filePath);
        }

        json obj;
        file >> obj;
        file.close();

        return Parse(obj);
    } catch (const std::exception& e) {
        return ManifestParseResult::Error(
            std::string("File parse error for ") + filePath + ": " + e.what()
        );
    }
}

bool ExtensionManifestParser::Validate(const ExtensionManifest& manifest,
                                       std::string& outErrorMessage) {
    // Check required fields
    if (manifest.name.empty()) {
        outErrorMessage = "Missing required field: name";
        return false;
    }

    if (manifest.version.empty()) {
        outErrorMessage = "Missing required field: version";
        return false;
    }

    if (manifest.publisher.empty() && manifest.author.empty()) {
        outErrorMessage = "Missing required field: publisher or author";
        return false;
    }

    // Validate version format (semver-like: major.minor.patch)
    {
        std::regex semverRegex(R"((\d+)\.(\d+)\.(\d+)(?:-([\w.-]+))?(?:\+([\w.-]+))?)");
        if (!std::regex_match(manifest.version, semverRegex)) {
            outErrorMessage = "Invalid version format: '" + manifest.version + "' (expected semver)";
            return false;
        }
    }

    // Validate engine version if present
    if (!manifest.engines.empty()) {
        auto it = manifest.engines.find("vscode");
        if (it != manifest.engines.end()) {
            std::regex engineRegex(R"(^\^(\d+)\.(\d+)\.(\d+)$)");
            if (!std::regex_match(it->second, engineRegex)) {
                outErrorMessage = "Invalid engine version format: '" + it->second + "'";
                return false;
            }
        }
    }

    // Validate activation events are non-empty
    if (!manifest.activationEvents.empty()) {
        for (const auto& evt : manifest.activationEvents) {
            if (evt.empty()) {
                outErrorMessage = "Empty activation event found";
                return false;
            }
        }
    }

    // Validate contribution command IDs are non-empty
    for (const auto& cmd : manifest.contributes.commands) {
        if (cmd.id.empty()) {
            outErrorMessage = "Command contribution with empty ID";
            return false;
        }
    }

    // Validate keybinding format
    for (const auto& kb : manifest.contributes.keybindings) {
        if (kb.command.empty()) {
            outErrorMessage = "Keybinding with empty command";
            return false;
        }
        if (kb.key.empty() && kb.mac.empty() && kb.linux.empty() && kb.win.empty()) {
            outErrorMessage = "Keybinding for '" + kb.command + "' has no key specified";
            return false;
        }
    }

    return true;
}

std::vector<std::string> ExtensionManifestParser::ExtractActivationEvents(const json& packageJson) {
    std::vector<std::string> events;
    SafeGetArray(packageJson, "activationEvents", events);
    return events;
}

ExtensionContributions ExtensionManifestParser::ExtractContributions(const json& packageJson) {
    ExtensionContributions contrib;
    ExtensionManifest temp;
    ParseContributions(packageJson, temp);
    return temp.contributes;
}

std::vector<std::string> ExtensionManifestParser::ExtractDependencies(const json& packageJson) {
    std::vector<std::string> deps;
    json depsObj;
    if (SafeGetObject(packageJson, "dependencies", depsObj)) {
        try {
            for (auto& [key, val] : depsObj.items()) {
                deps.push_back(key);
            }
        } catch (...) {
            fprintf(stderr, "[ExtensionManifest] Skipped malformed dependency extraction\n");
        }
    }
    return deps;
}

// ============================================================================
// Global Helper
// ============================================================================

ManifestParseResult ParseExtensionDirectory(const std::string& extensionDir) {
    if (extensionDir.empty()) {
        return ManifestParseResult::Error("Extension directory is empty");
    }

    fs::path packageJsonPath = extensionDir;
    packageJsonPath /= "package.json";

    if (!fs::exists(packageJsonPath)) {
        return ManifestParseResult::Error("package.json not found in: " + extensionDir);
    }

    return ExtensionManifestParser::ParseFile(packageJsonPath.string());
}

}  // namespace Extensions
}  // namespace RawrXD

// ============================================================================
// End of extension_manifest_parser.cpp
// ============================================================================
