// ============================================================================
// vscode_extension_compat.cpp — VS Code Extension Compatibility Layer
// ============================================================================
// Enables VS Code extensions to run in RawrXD IDE
// Implements VS Code Extension API compatibility
// ============================================================================

#include "vscode_extension_compat.h"
#include "logging/logger.h"
#include <filesystem>
#include <fstream>
#include <regex>

static Logger s_logger("VSCodeCompat");

class VSCodeExtensionHost::Impl {
public:
    std::vector<Extension> m_loadedExtensions;
    std::map<std::string, ExtensionAPI> m_extensionAPIs;
    std::string m_extensionsPath;
    std::mutex m_mutex;
    bool m_initialized;
    
    Impl() : m_initialized(false) {
        // Default extensions path
#ifdef _WIN32
        char* userProfile = getenv("USERPROFILE");
        if (userProfile) {
            m_extensionsPath = std::string(userProfile) + "\\.vscode\\extensions";
        }
#else
        char* home = getenv("HOME");
        if (home) {
            m_extensionsPath = std::string(home) + "/.vscode/extensions";
        }
#endif
    }
    
    bool initialize() {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        if (m_initialized) return true;
        
        // Scan extensions directory
        if (!std::filesystem::exists(m_extensionsPath)) {
            s_logger.warn("VS Code extensions directory not found: {}", m_extensionsPath);
            m_initialized = true;  // Continue anyway
            return true;
        }
        
        int loadedCount = 0;
        for (const auto& entry : std::filesystem::directory_iterator(m_extensionsPath)) {
            if (entry.is_directory()) {
                if (loadExtension(entry.path().string())) {
                    loadedCount++;
                }
            }
        }
        
        m_initialized = true;
        s_logger.info("VS Code extension host initialized: {} extensions loaded", loadedCount);
        
        return true;
    }
    
    bool loadExtension(const std::string& extensionPath) {
        // Read package.json
        std::string packagePath = extensionPath + "/package.json";
        if (!std::filesystem::exists(packagePath)) {
            return false;
        }
        
        std::ifstream file(packagePath);
        if (!file.is_open()) return false;
        
        std::string content((std::istreambuf_iterator<char>(file)),
                           std::istreambuf_iterator<char>());
        file.close();
        
        // Parse package.json (simplified)
        Extension ext;
        ext.path = extensionPath;
        ext.id = extractJsonString(content, "name");
        ext.version = extractJsonString(content, "version");
        ext.displayName = extractJsonString(content, "displayName");
        ext.description = extractJsonString(content, "description");
        ext.enabled = true;
        
        if (ext.id.empty()) return false;
        
        m_loadedExtensions.push_back(ext);
        s_logger.debug("Loaded extension: {} v{}", ext.id, ext.version);
        
        return true;
    }
    
    std::string extractJsonString(const std::string& json, const std::string& key) {
        std::regex pattern("\"" + key + "\"\\s*:\\s*\"([^\"]+)\"");
        std::smatch match;
        if (std::regex_search(json, match, pattern)) {
            return match[1].str();
        }
        return "";
    }
    
    std::vector<Extension> getExtensions() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_loadedExtensions;
    }
    
    bool enableExtension(const std::string& extensionId) {
        std::lock_guard<std::mutex> lock(m_mutex);
        for (auto& ext : m_loadedExtensions) {
            if (ext.id == extensionId) {
                ext.enabled = true;
                s_logger.info("Enabled extension: {}", extensionId);
                return true;
            }
        }
        return false;
    }
    
    bool disableExtension(const std::string& extensionId) {
        std::lock_guard<std::mutex> lock(m_mutex);
        for (auto& ext : m_loadedExtensions) {
            if (ext.id == extensionId) {
                ext.enabled = false;
                s_logger.info("Disabled extension: {}", extensionId);
                return true;
            }
        }
        return false;
    }
};

// ============================================================================
// Public API
// ============================================================================

VSCodeExtensionHost::VSCodeExtensionHost() : m_impl(new Impl()) {}
VSCodeExtensionHost::~VSCodeExtensionHost() { delete m_impl; }

bool VSCodeExtensionHost::initialize() {
    return m_impl->initialize();
}

void VSCodeExtensionHost::setExtensionsPath(const std::string& path) {
    std::lock_guard<std::mutex> lock(m_impl->m_mutex);
    m_impl->m_extensionsPath = path;
}

std::vector<Extension> VSCodeExtensionHost::getExtensions() const {
    return m_impl->getExtensions();
}

bool VSCodeExtensionHost::loadExtension(const std::string& path) {
    return m_impl->loadExtension(path);
}

bool VSCodeExtensionHost::enableExtension(const std::string& extensionId) {
    return m_impl->enableExtension(extensionId);
}

bool VSCodeExtensionHost::disableExtension(const std::string& extensionId) {
    return m_impl->disableExtension(extensionId);
}

Extension VSCodeExtensionHost::getExtension(const std::string& extensionId) const {
    auto exts = m_impl->getExtensions();
    for (const auto& ext : exts) {
        if (ext.id == extensionId) return ext;
    }
    return Extension{};
}
