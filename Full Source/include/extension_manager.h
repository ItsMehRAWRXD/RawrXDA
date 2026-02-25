#ifndef EXTENSION_MANAGER_H
#define EXTENSION_MANAGER_H

#include <string>
#include <vector>
#include <map>
#include <memory>

namespace IDE {

struct Extension {
    std::string name;
    std::string type;
    std::string path;
    bool installed;
    bool enabled;
    std::string created;
    std::map<std::string, std::string> parameters;
};

class ExtensionManager {
public:
    ExtensionManager();
    ~ExtensionManager();

    // Core extension operations
    bool createExtension(const std::string& name, const std::string& type, 
                        const std::map<std::string, std::string>& params = {});
    bool installExtension(const std::string& name);
    bool uninstallExtension(const std::string& name);
    bool enableExtension(const std::string& name);
    bool disableExtension(const std::string& name);
    bool removeExtension(const std::string& name);

    // Query operations
    std::vector<Extension> listExtensions() const;
    Extension getExtension(const std::string& name) const;
    std::vector<Extension> getEnabledExtensions() const;
    std::vector<Extension> getInstalledExtensions() const;

    // PowerShell bridge
    bool invokePowerShellExtension(const std::string& name, const std::string& function, 
                                   const std::vector<std::string>& args = {});
    
    // Registry operations
    bool loadRegistry();
    bool saveRegistry();
    std::string getRegistryPath() const;

private:
    std::string extensionRoot_;
    std::string moduleStore_;
    std::string registryPath_;
    std::map<std::string, Extension> registry_;

    bool executePowerShellCommand(const std::string& command, std::string& output);
    bool parseRegistryJson(const std::string& json);
    std::string generateRegistryJson() const;
};

// Helper to get singleton instance
ExtensionManager& GetExtensionManager();

} // namespace IDE

#endif // EXTENSION_MANAGER_H
