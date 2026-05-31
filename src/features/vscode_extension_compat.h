#pragma once

#include <string>
#include <vector>
#include <map>
#include <functional>

struct Extension {
    std::string id;
    std::string version;
    std::string displayName;
    std::string description;
    std::string path;
    bool enabled;
};

struct ExtensionAPI {
    std::map<std::string, std::function<void(const std::string&)>> commands;
    std::vector<std::string> languages;
    std::vector<std::string> grammars;
};

class VSCodeExtensionHost {
public:
    VSCodeExtensionHost();
    ~VSCodeExtensionHost();
    
    // Initialize extension host
    bool initialize();
    
    // Extension path configuration
    void setExtensionsPath(const std::string& path);
    
    // Extension management
    std::vector<Extension> getExtensions() const;
    bool loadExtension(const std::string& path);
    bool enableExtension(const std::string& extensionId);
    bool disableExtension(const std::string& extensionId);
    Extension getExtension(const std::string& extensionId) const;
    
private:
    class Impl;
    Impl* m_impl;
};
