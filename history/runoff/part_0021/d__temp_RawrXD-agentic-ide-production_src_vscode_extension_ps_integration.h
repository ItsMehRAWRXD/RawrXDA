// ============================================
// VSCode Extension PowerShell Integration Header
// ============================================

#ifndef VSCODE_EXTENSION_PS_INTEGRATION_H
#define VSCODE_EXTENSION_PS_INTEGRATION_H

#include <string>
#include <vector>
#include <functional>

// PowerShell command function signatures
std::string GetVSCodeExtensionStatus(const std::vector<std::string>& args);
std::string InstallVSCodeExtension(const std::vector<std::string>& args);
std::string UninstallVSCodeExtension(const std::vector<std::string>& args);
std::string LoadVSCodeExtension(const std::vector<std::string>& args);
std::string SearchVSCodeMarketplace(const std::vector<std::string>& args);
std::string GetVSCodeExtensionInfo(const std::vector<std::string>& args);

// Command registry
struct VSCodeExtensionCommand {
    std::string name;
    std::string description;
    std::function<std::string(const std::vector<std::string>&)> function;
};

std::vector<VSCodeExtensionCommand> GetVSCodeExtensionCommands();
void RegisterVSCodeExtensionCommands();

#endif // VSCODE_EXTENSION_PS_INTEGRATION_H