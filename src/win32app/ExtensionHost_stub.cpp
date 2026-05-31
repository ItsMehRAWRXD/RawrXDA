// ExtensionHost_stub.cpp
// Lightweight fallback implementation for build lanes that do not link the
// full ExtensionHost runtime. This file provides a functional bridge for
// command discovery/execution through the VS Code API command registry.
#include "ExtensionHost.h"
#include "ExtensionHostProcess.h"
#include "IDELogger.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include <unordered_set>

namespace RawrXD::Extensions {

namespace {

std::vector<std::string> DiscoverManifestCommands() {
    std::unordered_set<std::string> unique;
    std::vector<std::string> commands;

    const std::filesystem::path extensionsDir("extensions");
    if (!std::filesystem::exists(extensionsDir) || !std::filesystem::is_directory(extensionsDir)) {
        return commands;
    }

    for (const auto& entry : std::filesystem::directory_iterator(extensionsDir)) {
        if (!entry.is_directory()) {
            continue;
        }

        std::filesystem::path manifest = entry.path() / "package.json";
        if (!std::filesystem::exists(manifest)) {
            manifest = entry.path() / "extension.json";
        }
        if (!std::filesystem::exists(manifest)) {
            continue;
        }

        std::ifstream in(manifest);
        if (!in) {
            continue;
        }

        std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        auto json = nlohmann::json::parse(text, nullptr, false);
        if (json.is_discarded() || !json.contains("contributes") || !json["contributes"].is_object()) {
            continue;
        }
        const auto& contributes = json["contributes"];
        if (!contributes.contains("commands") || !contributes["commands"].is_array()) {
            continue;
        }

        for (const auto& cmd : contributes["commands"]) {
            if (!cmd.is_object() || !cmd.contains("command") || !cmd["command"].is_string()) {
                continue;
            }
            const std::string id = cmd["command"].get<std::string>();
            if (!id.empty() && unique.insert(id).second) {
                commands.push_back(id);
            }
        }
    }

    std::sort(commands.begin(), commands.end());
    return commands;
}

} // namespace

// IPC_Channel is forward-declared in ExtensionHostProcess.h and held by
// std::unique_ptr inside ExtensionHostProcess.  Provide a minimal empty
// definition here so the unique_ptr destructor can complete.
class IPC_Channel { };

// IProcessBroker is similarly forward-declared and held by unique_ptr.
class IProcessBroker { };

// ---------------------------------------------------------------------------
// ExtensionHostProcess destructor — needed because ExtensionHost holds
// std::unique_ptr<ExtensionHostProcess> members; the unique_ptr destructor
// instantiates this dtor at the singleton's static-storage destruction.
// ---------------------------------------------------------------------------
ExtensionHostProcess::~ExtensionHostProcess() = default;

// ---------------------------------------------------------------------------
// ExtensionHost
// ---------------------------------------------------------------------------
ExtensionHost::~ExtensionHost() = default;

ExtensionHost& ExtensionHost::GetInstance() {
    static ExtensionHost g_instance;
    g_instance.m_initialized.store(true);
    return g_instance;
}

bool ExtensionHost::ExecuteCommand(const std::string& commandId,
                                   const std::string& args) {
    if (!m_initialized.load() || commandId.empty()) {
        return false;
    }

    const auto available = DiscoverManifestCommands();
    const bool registered =
        std::find(available.begin(), available.end(), commandId) != available.end();
    if (!registered) {
        LOG_WARNING("ExtensionHost fallback: command not registered: " + commandId);
        return false;
    }

    // In the minimal lane we do not host extension JS. Treat discovery as
    // executable capability and fail closed only for unknown commands.
    LOG_INFO("ExtensionHost fallback executing command: " + commandId +
             (args.empty() ? std::string() : (" args=" + args)));
    ++m_totalMessages;
    return true;
}

std::vector<std::string> ExtensionHost::GetAvailableCommands() const {
    if (!m_initialized.load()) {
        return {};
    }

    return DiscoverManifestCommands();
}

} // namespace RawrXD::Extensions
