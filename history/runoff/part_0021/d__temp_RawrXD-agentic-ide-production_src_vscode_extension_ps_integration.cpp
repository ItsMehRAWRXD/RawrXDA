// ============================================
// VS Code Extension PowerShell Integration
// ============================================
// Purpose: Add VS Code extension commands to RawrXD PowerShell system
// Author: RawrXD
// Version: 1.0.0
// ============================================

#include "vscode_extension_integration.h"
#include <string>
#include <vector>

// PowerShell command implementations for VS Code extensions
std::string GetVSCodeExtensionStatus(const std::vector<std::string>& args) {
    initializeVSCodeExtensionManager();
    
    std::string result = "VS Code Extension Manager Status:\n";
    result += "===============================\n";
    
    auto extensions = getInstalledVSCodeExtensions();
    result += "Installed Extensions: " + std::to_string(extensions.size()) + "\n";
    
    for (const auto& ext : extensions) {
        result += "  - " + ext + "\n";
    }
    
    return result;
}

std::string InstallVSCodeExtension(const std::vector<std::string>& args) {
    if (args.size() < 1) {
        return "Error: Extension ID required. Usage: Install-VSCodeExtension -ExtensionId 'ms-python.python'";
    }
    
    std::string extensionId = args[0];
    std::string version = "latest";
    
    if (args.size() > 1) {
        version = args[1];
    }
    
    initializeVSCodeExtensionManager();
    
    if (installVSCodeExtension(extensionId, version)) {
        return "Successfully installed extension: " + extensionId;
    } else {
        return "Failed to install extension: " + extensionId;
    }
}

std::string UninstallVSCodeExtension(const std::vector<std::string>& args) {
    if (args.size() < 1) {
        return "Error: Extension ID required. Usage: Uninstall-VSCodeExtension -ExtensionId 'ms-python.python'";
    }
    
    std::string extensionId = args[0];
    
    initializeVSCodeExtensionManager();
    
    if (uninstallVSCodeExtension(extensionId)) {
        return "Successfully uninstalled extension: " + extensionId;
    } else {
        return "Failed to uninstall extension: " + extensionId;
    }
}

std::string LoadVSCodeExtension(const std::vector<std::string>& args) {
    if (args.size() < 1) {
        return "Error: Extension ID required. Usage: Load-VSCodeExtension -ExtensionId 'ms-python.python'";
    }
    
    std::string extensionId = args[0];
    
    initializeVSCodeExtensionManager();
    
    if (loadVSCodeExtension(extensionId)) {
        return "Successfully loaded extension: " + extensionId;
    } else {
        return "Failed to load extension: " + extensionId;
    }
}

std::string SearchVSCodeMarketplace(const std::vector<std::string>& args) {
    std::string query = "";
    std::string category = "";
    int page = 1;
    int pageSize = 20;
    
    if (args.size() > 0) query = args[0];
    if (args.size() > 1) category = args[1];
    if (args.size() > 2) page = std::stoi(args[2]);
    if (args.size() > 3) pageSize = std::stoi(args[3]);
    
    initializeVSCodeExtensionManager();
    
    auto results = searchVSCodeMarketplace(query, category, page, pageSize);
    
    std::string result = "Marketplace Search Results:\n";
    result += "=========================\n";
    result += "Query: " + query + "\n";
    result += "Results: " + std::to_string(results.size()) + "\n\n";
    
    for (const auto& ext : results) {
        result += "- " + ext + "\n";
    }
    
    return result;
}

std::string GetVSCodeExtensionInfo(const std::vector<std::string>& args) {
    if (args.size() < 1) {
        return "Error: Extension ID required. Usage: Get-VSCodeExtensionInfo -ExtensionId 'ms-python.python'";
    }
    
    std::string extensionId = args[0];
    
    initializeVSCodeExtensionManager();
    
    // This would get detailed extension information
    std::string result = "Extension Information: " + extensionId + "\n";
    result += "=============================\n";
    result += "ID: " + extensionId + "\n";
    result += "Status: Installed\n";
    result += "Version: 1.0.0\n";
    result += "Enabled: Yes\n";
    result += "Loaded: Yes\n";
    
    return result;
}

// Function registry for PowerShell integration
struct VSCodeExtensionCommand {
    std::string name;
    std::string description;
    std::function<std::string(const std::vector<std::string>&)> function;
};

std::vector<VSCodeExtensionCommand> GetVSCodeExtensionCommands() {
    return {
        {"Get-VSCodeExtensionStatus", "Get VS Code extension manager status", GetVSCodeExtensionStatus},
        {"Install-VSCodeExtension", "Install a VS Code extension", InstallVSCodeExtension},
        {"Uninstall-VSCodeExtension", "Uninstall a VS Code extension", UninstallVSCodeExtension},
        {"Load-VSCodeExtension", "Load a VS Code extension", LoadVSCodeExtension},
        {"Search-VSCodeMarketplace", "Search VS Code marketplace", SearchVSCodeMarketplace},
        {"Get-VSCodeExtensionInfo", "Get detailed extension information", GetVSCodeExtensionInfo}
    };
}

// Integration with existing PowerShell system
void RegisterVSCodeExtensionCommands() {
    auto commands = GetVSCodeExtensionCommands();
    
    // This would register commands with the existing PowerShell system
    // For now, just log the registration
    
    std::cout << "Registering VS Code Extension Commands:" << std::endl;
    for (const auto& cmd : commands) {
        std::cout << "  - " << cmd.name << ": " << cmd.description << std::endl;
    }
}

// Auto-register commands on initialization
class VSCodeExtensionCommandRegistrar {
public:
    VSCodeExtensionCommandRegistrar() {
        RegisterVSCodeExtensionCommands();
    }
};

static VSCodeExtensionCommandRegistrar g_vscodeCommandRegistrar;