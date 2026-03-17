// RawrXD_APIKeyManager.hpp - Centralized API Key & Extension Management
// Shared between Win32 IDE and CLI

#pragma once

#include <Windows.h>
#include <string>
#include <fstream>
#include <sstream>

namespace RawrXD {

class APIKeyManager {
public:
    static APIKeyManager& Get() {
        static APIKeyManager instance;
        return instance;
    }

    void setApiKey(const std::string& key) {
        apiKey_ = key;
        saveToConfig();
    }

    std::string getApiKey() const {
        return apiKey_;
    }

    bool hasValidApiKey() const {
        return !apiKey_.empty() && apiKey_.find("key_") == 0;
    }

    std::string getMaskedKey() const {
        if (apiKey_.empty()) return "Not set";
        if (apiKey_.length() < 12) return "****";
        return apiKey_.substr(0, 8) + "****" + apiKey_.substr(apiKey_.length() - 8);
    }

    void enableAmazonQ(bool enable) {
        amazonQEnabled_ = enable;
        saveToConfig();
        if (enable && hasValidApiKey()) {
            installAmazonQ();
        }
    }

    void enableGitHubCopilot(bool enable) {
        copilotEnabled_ = enable;
        saveToConfig();
        if (enable && hasValidApiKey()) {
            installGitHubCopilot();
        }
    }

    bool isAmazonQEnabled() const { return amazonQEnabled_; }
    bool isCopilotEnabled() const { return copilotEnabled_; }

    void loadFromConfig() {
        std::string configPath = getConfigPath();
        std::ifstream file(configPath);
        if (!file.is_open()) return;

        std::string line;
        while (std::getline(file, line)) {
            if (line.find("api_key=") == 0) {
                apiKey_ = line.substr(8);
            } else if (line.find("amazonq_enabled=") == 0) {
                amazonQEnabled_ = line.substr(16) == "1";
            } else if (line.find("copilot_enabled=") == 0) {
                copilotEnabled_ = line.substr(16) == "1";
            }
        }
    }

    void saveToConfig() {
        std::string configPath = getConfigPath();
        
        // Ensure directory exists
        std::string dir = configPath.substr(0, configPath.find_last_of("\\/"));
        CreateDirectoryA(dir.c_str(), NULL);

        std::ofstream file(configPath);
        if (!file.is_open()) return;

        file << "api_key=" << apiKey_ << std::endl;
        file << "amazonq_enabled=" << (amazonQEnabled_ ? "1" : "0") << std::endl;
        file << "copilot_enabled=" << (copilotEnabled_ ? "1" : "0") << std::endl;
    }

private:
    APIKeyManager() {
        apiKey_ = "key_1bbe2f4d33423a095fc03d9f873eb4a161a680df099e82410be7bb19e65c319f";
        amazonQEnabled_ = false;
        copilotEnabled_ = false;
        loadFromConfig();
    }

    std::string getConfigPath() {
        char appData[MAX_PATH];
        if (GetEnvironmentVariableA("APPDATA", appData, MAX_PATH) > 0) {
            return std::string(appData) + "\\RawrXD\\api_config.txt";
        }
        return ".\\rawrxd_api_config.txt";
    }

    void installAmazonQ() {
        std::string cmd = "powershell.exe -NoProfile -ExecutionPolicy Bypass -Command \"";
        cmd += "$env:RAWRXD_API_KEY='" + apiKey_ + "'; ";
        cmd += "Install-VSCodeExtension -ExtensionId 'amazonwebservices.amazon-q-vscode'";
        cmd += "\"";
        system(cmd.c_str());
    }

    void installGitHubCopilot() {
        std::string cmd = "powershell.exe -NoProfile -ExecutionPolicy Bypass -Command \"";
        cmd += "$env:RAWRXD_API_KEY='" + apiKey_ + "'; ";
        cmd += "Install-VSCodeExtension -ExtensionId 'github.copilot'";
        cmd += "\"";
        system(cmd.c_str());
    }

    std::string apiKey_;
    bool amazonQEnabled_;
    bool copilotEnabled_;
};

} // namespace RawrXD
