#include "ExtensionAPI_VSCode_Internal.h"
#include "ExtensionInstance.h"
#include <iostream>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace vscode {

// Global bridge objects
Window window;
Workspace workspace;
Commands commands;

void Window::showInformationMessage(const std::string& message) {
    std::cout << "[VSCodeAPI] Info: " << message << std::endl;
}

void Window::showErrorMessage(const std::string& message) {
    std::cerr << "[VSCodeAPI] Error: " << message << std::endl;
}

void Commands::registerCommand(const std::string& command, std::function<void()> callback) {
    std::cout << "[VSCodeAPI] Registered Command: " << command << std::endl;
}

void Commands::executeCommand(const std::string& command) {
    std::cout << "[VSCodeAPI] Executing Command: " << command << std::endl;
}

}

namespace RawrXD::Extensions {

class APIDispatcher {
public:
    static void Dispatch(ExtensionInstance* instance, const IPCHeader& header, const std::vector<uint8_t>& payload) {
        if (payload.empty()) return;

        try {
            auto j = json::parse(payload.begin(), payload.end());
            std::string method = j.value("method", "");

            if (method == "window.showInformationMessage") {
                std::string msg = j.at("params").at("message");
                vscode::window.showInformationMessage(msg);
            }
            else if (method == "commands.executeCommand") {
                std::string cmd = j.at("params").at("command");
                vscode::commands.executeCommand(cmd);
            }
            // Add more dispatch logic as APIs grow
        } catch (const std::exception& e) {
            std::cerr << "[APIDispatcher] JSON Parse/Dispatch Error: " << e.what() << std::endl;
        }
    }
};

}
