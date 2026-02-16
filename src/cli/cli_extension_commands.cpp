// ============================================================================
// cli_extension_commands.cpp — CLI Extension/Plugin Commands
// ============================================================================
//
// PURPOSE:
//   Handles "!plugin" commands in CLI mode for loading/managing extensions
//   Parses commands like:
//     !plugin load amazon-q-vscode-latest.vsix
//     !plugin list
//     !plugin enable GitHub.copilot
//     !plugin install amazonwebservices.amazon-q-vscode
//
// Architecture: C++20 | No exceptions | No Qt
// Pattern:      PatchResult-compatible returns
// Rule:         NO SOURCE FILE IS TO BE SIMPLIFIED
// ============================================================================

#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <algorithm>
#include <filesystem>
#include "../marketplace/extension_marketplace.hpp"
#include "../marketplace/extension_auto_installer.hpp"
#include "../win32app/VSCodeMarketplaceAPI.hpp"
#include "../win32app/VSIXInstaller.hpp"

using namespace RawrXD::Extensions;

namespace RawrXD {
namespace CLI {

// ============================================================================
// Command Parser
// ============================================================================

struct PluginCommand {
    std::string verb;              // load, install, enable, disable, list, etc.
    std::string target;            // extension ID or file path
    std::vector<std::string> args; // additional arguments

    static PluginCommand parse(const std::string& input) {
        PluginCommand cmd;
        std::istringstream iss(input);
        std::string word;

        // Skip "!plugin" prefix
        std::getline(iss, word, ' ');

        // Get verb
        if (std::getline(iss, word, ' ')) {
            cmd.verb = word;
        }

        // Get target
        if (std::getline(iss, word, ' ')) {
            cmd.target = word;
        }

        // Get remaining args
        while (std::getline(iss, word, ' ')) {
            cmd.args.push_back(word);
        }

        return cmd;
    }
};

// ============================================================================
// Command Handlers
// ============================================================================

static void cmdPluginLoad(const std::string& target) {
    std::cout << "[Plugin] Loading VSIX: " << target << "\n";

    // Check if file exists
    if (!std::filesystem::exists(target)) {
        std::cout << "[Error] File not found: " << target << "\n";
        std::cout << "[Info] Place .vsix files in the current directory or provide full path\n";
        return;
    }

    // Install VSIX
    if (RawrXD::VSIXInstaller::Install(target)) {
        std::cout << "[Success] Extension installed from " << target << "\n";
    } else {
        std::cout << "[Error] Failed to install extension\n";
    }
}

static void cmdPluginInstall(const std::string& extensionId) {
    std::cout << "[Plugin] Installing from marketplace: " << extensionId << "\n";

    // Progress callback
    auto progressCallback = [](const InstallProgress& progress) {
        switch (progress.stage) {
            case InstallProgress::Stage::Querying:
                std::cout << "  [" << progress.currentIndex + 1 << "/" << progress.totalExtensions
                          << "] Querying marketplace for " << progress.extensionId << "...\n";
                break;
            case InstallProgress::Stage::Downloading:
                std::cout << "  [" << progress.currentIndex + 1 << "/" << progress.totalExtensions
                          << "] Downloading " << progress.extensionId << "...\n";
                break;
            case InstallProgress::Stage::Installing:
                std::cout << "  [" << progress.currentIndex + 1 << "/" << progress.totalExtensions
                          << "] Installing " << progress.extensionId << "...\n";
                break;
            case InstallProgress::Stage::Verifying:
                std::cout << "  [" << progress.currentIndex + 1 << "/" << progress.totalExtensions
                          << "] Verifying " << progress.extensionId << "...\n";
                break;
            case InstallProgress::Stage::Complete:
                std::cout << "  [✓] " << progress.extensionId << " installed successfully\n";
                break;
            case InstallProgress::Stage::Failed:
                std::cout << "  [✗] " << progress.extensionId << " failed: " << progress.detail << "\n";
                break;
        }
    };

    AutoInstallResult result = ExtensionAutoInstaller::instance().installExtension(
        extensionId, progressCallback
    );

    if (result.success) {
        std::cout << "[Success] " << result.detail << "\n";
    } else {
        std::cout << "[Error] " << result.detail << " (code: " << result.errorCode << ")\n";
    }
}

static void cmdPluginInstallAI() {
    std::cout << "[Plugin] Installing AI extensions (Amazon Q, GitHub Copilot, Continue)...\n";

    auto progressCallback = [](const InstallProgress& progress) {
        switch (progress.stage) {
            case InstallProgress::Stage::Querying:
                std::cout << "  [" << progress.currentIndex + 1 << "/" << progress.totalExtensions
                          << "] Querying " << progress.extensionId << "...\n";
                break;
            case InstallProgress::Stage::Downloading:
                std::cout << "  [" << progress.currentIndex + 1 << "/" << progress.totalExtensions
                          << "] Downloading " << progress.extensionId << "...\n";
                break;
            case InstallProgress::Stage::Installing:
                std::cout << "  [" << progress.currentIndex + 1 << "/" << progress.totalExtensions
                          << "] Installing " << progress.extensionId << "...\n";
                break;
            case InstallProgress::Stage::Complete:
                std::cout << "  [✓] " << progress.extensionId << "\n";
                break;
            case InstallProgress::Stage::Failed:
                std::cout << "  [✗] " << progress.extensionId << ": " << progress.detail << "\n";
                break;
            default:
                break;
        }
    };

    AutoInstallResult result = installAIExtensions(progressCallback);

    std::cout << "\n[Summary] Installed: " << result.installedCount
              << ", Failed: " << result.failedCount << "\n";

    if (result.installedCount > 0) {
        std::cout << "[Installed Extensions]\n";
        for (const auto& id : result.installedIds) {
            std::cout << "  - " << id << "\n";
        }
    }

    if (result.failedCount > 0) {
        std::cout << "[Failed Extensions]\n";
        for (const auto& id : result.failedIds) {
            std::cout << "  - " << id << "\n";
        }
    }
}

static void cmdPluginList() {
    std::cout << "[Plugin] Installed extensions:\n";

    auto installed = ExtensionAutoInstaller::instance().getInstalledExtensions();

    if (installed.empty()) {
        std::cout << "  (none)\n";
        std::cout << "\n[Hint] Use '!plugin install <id>' to install extensions\n";
        std::cout << "  Examples:\n";
        std::cout << "    !plugin install amazonwebservices.amazon-q-vscode\n";
        std::cout << "    !plugin install GitHub.copilot\n";
        std::cout << "    !plugin install-ai  (installs all AI extensions)\n";
        return;
    }

    for (const auto& id : installed) {
        std::cout << "  - " << id << "\n";
    }

    std::cout << "\nTotal: " << installed.size() << " extensions\n";
}

static void cmdPluginSearch(const std::string& query) {
    std::cout << "[Plugin] Searching marketplace for: " << query << "\n";

    std::vector<VSCodeMarketplace::MarketplaceEntry> results;
    if (!VSCodeMarketplace::Query(query, 20, 1, results)) {
        std::cout << "[Error] Failed to query marketplace\n";
        return;
    }

    if (results.empty()) {
        std::cout << "[Info] No results found\n";
        return;
    }

    std::cout << "\n[Search Results]\n";
    for (const auto& entry : results) {
        std::cout << "  " << entry.id << " v" << entry.version << "\n";
        std::cout << "    " << entry.displayName << "\n";
        std::cout << "    " << entry.shortDescription << "\n";
        std::cout << "    Installs: " << entry.installCount
                  << " | Rating: " << entry.averageRating << "/5.0\n";
        std::cout << "\n";
    }

    std::cout << "[Hint] Install with: !plugin install <id>\n";
}

static void cmdPluginSync() {
    std::cout << "[Plugin] Syncing marketplace catalog...\n";
    std::cout << "[Info] This will download up to 5000 extensions and may take several minutes\n";

    auto progressCallback = [](const InstallProgress& progress) {
        if (progress.stage == InstallProgress::Stage::Querying) {
            std::cout << "  Fetching catalog...\n";
        } else if (progress.stage == InstallProgress::Stage::Installing) {
            std::cout << "  [" << progress.currentIndex + 1 << "/" << progress.totalExtensions
                      << "] Installing " << progress.extensionId << "...\n";
        }
    };

    AutoInstallResult result = ExtensionAutoInstaller::instance().syncMarketplaceCatalog(
        5000, progressCallback
    );

    std::cout << "\n[Summary] Installed: " << result.installedCount
              << ", Failed: " << result.failedCount << "\n";
}

static void cmdPluginHelp() {
    std::cout << R"(
[Plugin Commands]
  !plugin load <file.vsix>          Load a VSIX package from file
  !plugin install <id>               Install extension from marketplace
  !plugin install-ai                 Install all AI extensions (Q, Copilot, Continue)
  !plugin list                       List installed extensions
  !plugin search <query>             Search marketplace
  !plugin sync                       Sync entire marketplace catalog (offline mode)
  !plugin help                       Show this help

[Priority Extensions]
  amazonwebservices.amazon-q-vscode  Amazon Q Developer
  GitHub.copilot                     GitHub Copilot
  GitHub.copilot-chat                GitHub Copilot Chat
  Continue.continue                  Continue (Local AI)
  TabNine.tabnine-vscode            TabNine AI
  ms-vscode.cpptools                 C/C++ Tools
  ms-python.python                   Python

[Examples]
  !plugin install amazonwebservices.amazon-q-vscode
  !plugin install GitHub.copilot
  !plugin install-ai
  !plugin load amazon-q-vscode-latest.vsix
  !plugin search copilot
  !plugin list
)";
}

// ============================================================================
// Main Dispatcher
// ============================================================================

bool handlePluginCommand(const std::string& input) {
    // Check if this is a plugin command
    if (input.rfind("!plugin", 0) != 0) {
        return false;
    }

    PluginCommand cmd = PluginCommand::parse(input);

    if (cmd.verb.empty() || cmd.verb == "help") {
        cmdPluginHelp();
    }
    else if (cmd.verb == "load") {
        if (cmd.target.empty()) {
            std::cout << "[Error] Usage: !plugin load <file.vsix>\n";
        } else {
            cmdPluginLoad(cmd.target);
        }
    }
    else if (cmd.verb == "install") {
        if (cmd.target.empty()) {
            std::cout << "[Error] Usage: !plugin install <extension-id>\n";
        } else {
            cmdPluginInstall(cmd.target);
        }
    }
    else if (cmd.verb == "install-ai") {
        cmdPluginInstallAI();
    }
    else if (cmd.verb == "list") {
        cmdPluginList();
    }
    else if (cmd.verb == "search") {
        if (cmd.target.empty()) {
            std::cout << "[Error] Usage: !plugin search <query>\n";
        } else {
            cmdPluginSearch(cmd.target);
        }
    }
    else if (cmd.verb == "sync") {
        cmdPluginSync();
    }
    else {
        std::cout << "[Error] Unknown plugin command: " << cmd.verb << "\n";
        std::cout << "[Hint] Type '!plugin help' for usage\n";
    }

    return true;
}

} // namespace CLI
} // namespace RawrXD
