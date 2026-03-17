#include "cloud_settings_dialog.h"
#include "model_router_adapter.h"
#include <fstream>
#include <iostream>
#include <filesystem>
#include <sstream>
#include <iomanip>
#include "nlohmann/json.hpp"

// Optional: Include WinHttp for validation if needed, but keeping it clean for now
// #include <winhttp.h>

using json = nlohmann::json;

CloudSettingsManager::CloudSettingsManager(ModelRouterAdapter* adapter)
    : m_adapter(adapter)
{
    // Initialize default supported providers
    // Real endpoints
    m_providers["openai"] = {"openai", "", true, "https://api.openai.com/v1"};
    m_providers["anthropic"] = {"anthropic", "", true, "https://api.anthropic.com/v1"};
    m_providers["google"] = {"google", "", true, "https://generativelanguage.googleapis.com/v1beta"};
    
    // Providers needing configuration
    m_providers["azure"] = {"azure", "", false, ""};
    m_providers["aws"] = {"aws", "", false, ""};
    m_providers["moonshot"] = {"moonshot", "", false, "https://api.moonshot.cn/v1"};
}

CloudSettingsManager::~CloudSettingsManager()
{
}

void CloudSettingsManager::setProviderConfig(const std::string& providerId, const ProviderConfig& config)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_providers[providerId] = config;
}

CloudSettingsManager::ProviderConfig CloudSettingsManager::getProviderConfig(const std::string& providerId) const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_providers.find(providerId);
    if (it != m_providers.end()) {
        return it->second;
    }
    return ProviderConfig{providerId, "", false, ""};
}

std::vector<std::string> CloudSettingsManager::getSupportedProviders() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<std::string> keys;
    for (const auto& [key, val] : m_providers) {
        keys.push_back(key);
    }
    return keys;
}

void CloudSettingsManager::setRouterConfig(const RouterConfig& config)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_routerConfig = config;
}

CloudSettingsManager::RouterConfig CloudSettingsManager::getRouterConfig() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_routerConfig;
}

bool CloudSettingsManager::validateProvider(const std::string& providerId)
{
    // Check if configuration exists and has API key
    // In a future update, we can add real WinHttp connectivity checks here
    auto config = getProviderConfig(providerId);
    if (!config.enabled) return false;
    if (config.apiKey.empty()) return false;
    
    // Basic format validation
    if (providerId == "openai" && config.apiKey.find("sk-") != 0) return false;
    
    return true; 
}

void CloudSettingsManager::applyToAdapter()
{
    if (!m_adapter) return;

    std::lock_guard<std::mutex> lock(m_mutex);
    
    // Push settings to the adapter if methods exist
    // Based on previous code analysis, these methods were called:
    m_adapter->setCostAlertThreshold(m_routerConfig.costAlertThresholdUsd);
    m_adapter->setLatencyThreshold(m_routerConfig.requestTimeoutMs);
    m_adapter->setRetryPolicy(m_routerConfig.maxRetries, m_routerConfig.retryDelayMs);
    
    // We could also push provider configs if the adapter supports it
    // m_adapter->updateProviders(m_providers);
}

bool CloudSettingsManager::loadSettings(const std::string& filePath)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    try {
        std::ifstream file(filePath);
        if (!file.is_open()) return false;

        json j;
        file >> j;

        if (j.contains("providers")) {
            for (auto& [key, val] : j["providers"].items()) {
                if (m_providers.count(key)) {
                    m_providers[key].apiKey = decrypt(val.value("apiKey", ""));
                    m_providers[key].endpoint = val.value("endpoint", m_providers[key].endpoint);
                    m_providers[key].enabled = val.value("enabled", true);
                }
            }
        }

        if (j.contains("router")) {
            auto& r = j["router"];
            m_routerConfig.defaultModel = r.value("defaultModel", "local");
            m_routerConfig.preferLocal = r.value("preferLocal", true);
            m_routerConfig.enableStreaming = r.value("enableStreaming", true);
            m_routerConfig.enableFallback = r.value("enableFallback", true);
            m_routerConfig.requestTimeoutMs = r.value("requestTimeoutMs", 30000);
            m_routerConfig.maxRetries = r.value("maxRetries", 3);
            m_routerConfig.retryDelayMs = r.value("retryDelayMs", 1000);
            m_routerConfig.costLimitUsd = r.value("costLimitUsd", 5.0);
            m_routerConfig.costAlertThresholdUsd = r.value("costAlertThresholdUsd", 50.0);
        }

        return true;
    } catch (...) {
        return false;
    }
}

bool CloudSettingsManager::saveSettings(const std::string& filePath) const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    try {
        json j;
        
        json providersJson;
        for (const auto& [key, val] : m_providers) {
            providersJson[key] = {
                {"apiKey", encrypt(val.apiKey)}, // Encrypt before saving
                {"endpoint", val.endpoint},
                {"enabled", val.enabled}
            };
        }
        j["providers"] = providersJson;

        j["router"] = {
            {"defaultModel", m_routerConfig.defaultModel},
            {"preferLocal", m_routerConfig.preferLocal},
            {"enableStreaming", m_routerConfig.enableStreaming},
            {"enableFallback", m_routerConfig.enableFallback},
            {"requestTimeoutMs", m_routerConfig.requestTimeoutMs},
            {"maxRetries", m_routerConfig.maxRetries},
            {"retryDelayMs", m_routerConfig.retryDelayMs},
            {"costLimitUsd", m_routerConfig.costLimitUsd},
            {"costAlertThresholdUsd", m_routerConfig.costAlertThresholdUsd}
        };

        std::ofstream file(filePath);
        file << j.dump(4);
        return true;
    } catch (...) {
        return false;
    }
}

// Simple XOR 'encryption' for demonstration
std::string CloudSettingsManager::encrypt(const std::string& input) const
{
    if (input.empty()) return "";
    std::string output = input;
    char key = 0x5A; // Simple static key
    for (size_t i = 0; i < input.length(); i++) {
        output[i] = input[i] ^ key;
    }
    // Encode to hex to ensure it's JSON safe string
    std::stringstream ss;
    for (unsigned char c : output) {
        ss << std::hex << std::setw(2) << std::setfill('0') << (int)c;
    }
    return ss.str();
}

std::string CloudSettingsManager::decrypt(const std::string& input) const
{
    if (input.empty()) return "";
    // Decode hex
    std::string raw;
    try {
        for (size_t i = 0; i < input.length(); i += 2) {
            std::string byteString = input.substr(i, 2);
            char byte = (char)strtol(byteString.c_str(), nullptr, 16);
            raw += byte;
        }
    } catch (...) { return ""; }
    
    std::string output = raw;
    char key = 0x5A;
    for (size_t i = 0; i < raw.length(); i++) {
        output[i] = raw[i] ^ key;
    }
    return output;
}



