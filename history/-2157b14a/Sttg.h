#ifndef CLOUD_SETTINGS_MANAGER_H
#define CLOUD_SETTINGS_MANAGER_H

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <mutex>

class ModelRouterAdapter;

/**
 * @class CloudSettingsManager
 * @brief Configuration manager for cloud AI providers and API keys.
 * Handles storage and retrieval of API keys and provider settings.
 * Replaces the Qt-based CloudSettingsDialog with a logic-only implementation.
 */
class CloudSettingsManager
{
public:
    struct ProviderConfig {
        std::string name;
        std::string apiKey;
        bool enabled = true;
        std::string endpoint; // Optional custom endpoint
    };

    struct RouterConfig {
        std::string defaultModel = "local";
        bool preferLocal = true;
        bool enableStreaming = true;
        bool enableFallback = true;
        int requestTimeoutMs = 30000;
        int maxRetries = 3;
        int retryDelayMs = 1000;
        double costLimitUsd = 5.0;
        double costAlertThresholdUsd = 50.0;
    };

    // Constructor with optional adapter reference
    explicit CloudSettingsManager(ModelRouterAdapter* adapter = nullptr);
    ~CloudSettingsManager();

    // Provider Management
    void setProviderConfig(const std::string& providerId, const ProviderConfig& config);
    ProviderConfig getProviderConfig(const std::string& providerId) const;
    std::vector<std::string> getSupportedProviders() const;

    // Global Router Config
    void setRouterConfig(const RouterConfig& config);
    RouterConfig getRouterConfig() const;

    // Persistence
    bool loadSettings(const std::string& filePath = "cloud_settings.json");
    bool saveSettings(const std::string& filePath = "cloud_settings.json") const;

    // Integration
    bool validateProvider(const std::string& providerId); // Performs a connectivity check
    void applyToAdapter(); // Pushes settings to the linked ModelRouterAdapter

private:
    ModelRouterAdapter* m_adapter;
    mutable std::mutex m_mutex;
    
    std::map<std::string, ProviderConfig> m_providers;
    RouterConfig m_routerConfig;

    // secure storage helper (mocked for now)
    std::string encrypt(const std::string& input) const;
    std::string decrypt(const std::string& input) const;
};

// Typedef for backward compatibility if needed, though strictly we are changing the class name conceptually
using CloudSettingsDialog = CloudSettingsManager;

#endif // CLOUD_SETTINGS_MANAGER_H


