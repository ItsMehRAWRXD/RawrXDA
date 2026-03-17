#include "cloud_settings_dialog.h"
#include "model_router_adapter.h"
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <windows.h>
#include <wincrypt.h>
#pragma comment(lib, "crypt32.lib")

using json = nlohmann::json;

// Helper for DPAPI
static std::string DPAPI_Encrypt(const std::string& input) {
    DATA_BLOB DataIn;
    DATA_BLOB DataOut;
    DATA_BLOB Entropy;

    DataIn.pbData = (BYTE*)input.data();
    DataIn.cbData = (DWORD)input.length();
    
    // Optional: Add entropy for additional security (e.g. machine specific GUID)
    // For now we rely on user credentials implicit in DPAPI
    
    if (CryptProtectData(
        &DataIn,
        L"RawrXD_Cloud_Credentials", // Description
        NULL,                        // Optional Entropy
        NULL,                        // Reserved
        NULL,                        // PromptStruct
        0,                           // Flags
        &DataOut))
    {
        std::string result((char*)DataOut.pbData, DataOut.cbData);
        LocalFree(DataOut.pbData);
        return result;
    }
    return "";
}

static std::string DPAPI_Decrypt(const std::string& input) {
    if (input.empty()) return "";
    
    DATA_BLOB DataIn;
    DATA_BLOB DataOut;
    
    DataIn.pbData = (BYTE*)input.data();
    DataIn.cbData = (DWORD)input.length();
    
    if (CryptUnprotectData(
        &DataIn,
        NULL, // Description
        NULL, // Entropy
        NULL, 
        NULL, 
        0, 
        &DataOut))
    {
        std::string result((char*)DataOut.pbData, DataOut.cbData);
        LocalFree(DataOut.pbData);
        return result;
    }
    return "";
}

// Helper to hex encode binary blobs for JSON storage
static std::string HexEncode(const std::string& input) {
    static const char hex_digits[] = "0123456789ABCDEF";
    std::string output;
    output.reserve(input.length() * 2);
    for (unsigned char c : input) {
        output.push_back(hex_digits[c >> 4]);
        output.push_back(hex_digits[c & 0x0F]);
    }
    return output;
}

static std::string HexDecode(const std::string& input) {
    std::string output;
    if (input.length() % 2 != 0) return "";
    output.reserve(input.length() / 2);
    for (size_t i = 0; i < input.length(); i += 2) {
        std::string byteString = input.substr(i, 2);
        char byte = (char)strtol(byteString.c_str(), nullptr, 16);
        output.push_back(byte);
    }
    return output;
}

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

std::string CloudSettingsManager::encrypt(const std::string& input) const {
    // Start with DPAPI
    std::string encrypted = DPAPI_Encrypt(input);
    if (!encrypted.empty()) {
        // Hex encode for text file storage validity
        return "DPAPI:" + HexEncode(encrypted); 
    }
    
    // Fallback? Retain old XOR logic if DPAPI fails (rare) but mark it
    // Or just fail. Better to fail than be insecure, but for logic completeness:
    // We return empty string on failure.
    return "";
}

std::string CloudSettingsManager::decrypt(const std::string& input) const {
    if (input.rfind("DPAPI:", 0) == 0) {
        // It's a DPAPI string
        std::string hexData = input.substr(6);
        std::string binaryData = HexDecode(hexData);
        return DPAPI_Decrypt(binaryData);
    }
    
    // Legacy support for previously XOR'd configs
    std::string output = input; // copy
    char key = 0x5A;
    // Decrypt hex encoded wrapper first?
    // Old implementation: Hex Encode -> XOR
    // So we need to hex decode first
    // Replicating logic from old implementation reading:
    // ... loop i+=2 ... 
    
    std::string raw;
    try {
        for (size_t i = 0; i < input.length(); i += 2) {
            std::string byteString = input.substr(i, 2);
            char byte = (char)strtol(byteString.c_str(), nullptr, 16);
            raw += byte;
        }
    } catch (...) { return ""; }
    
    std::string decrypted = raw;
    for (size_t i = 0; i < raw.length(); i++) {
        decrypted[i] = raw[i] ^ key;
    }
    return decrypted;
}



