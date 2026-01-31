#ifndef CLOUD_SETTINGS_DIALOG_H
#define CLOUD_SETTINGS_DIALOG_H


#include <memory>

class ModelRouterAdapter;

/**
 * @class CloudSettingsDialog
 * @brief Configuration dialog for cloud AI providers and API keys
 * 
 * Provides comprehensive settings for:
 * - API key management (8 cloud providers)
 * - Model selection and configuration
 * - Cost limits and alerts
 * - Request timeout settings
 * - Retry policies
 * - Provider health status
 */
class CloudSettingsDialog : public void {

public:
    explicit CloudSettingsDialog(ModelRouterAdapter *adapter, void *parent = nullptr);
    ~CloudSettingsDialog();

    /**
     * Show modal dialog and return result
     */
    int exec() override;

    /**
     * Get the adapter being configured
     */
    ModelRouterAdapter* getAdapter() const { return m_adapter; }

protected:
    void closeEvent(void* event) override;

private:
    // API Key Management
    void onOpenAIKeyChanged(const std::string& key);
    void onAnthropicKeyChanged(const std::string& key);
    void onGoogleKeyChanged(const std::string& key);
    void onMoonshotKeyChanged(const std::string& key);
    void onAzureOpenAIKeyChanged(const std::string& key);
    void onAwsAccessKeyChanged(const std::string& key);
    void onAwsSecretKeyChanged(const std::string& key);
    
    // Key visibility toggle
    void onToggleKeyVisibility(int provider_index);
    
    // Key validation
    void onTestOpenAIKey();
    void onTestAnthropicKey();
    void onTestGoogleKey();
    void onTestMoonshotKey();
    void onTestAzureOpenAIKey();
    void onTestAwsKey();
    
    // Configuration
    void onTimeoutChanged(int ms);
    void onMaxRetriesChanged(int retries);
    void onRetryDelayChanged(int ms);
    void onCostLimitChanged(double limit);
    void onCostAlertThresholdChanged(double threshold);
    
    // Model preferences
    void onDefaultModelChanged(const std::string& model);
    void onPreferLocalModelsChanged(bool checked);
    void onEnableStreamingChanged(bool checked);
    void onEnableFallbackChanged(bool checked);
    
    // Dialog actions
    void onSaveSettings();
    void onLoadDefaults();
    void onLoadEnvironmentVariables();
    void onExportConfiguration();
    void onImportConfiguration();
    void onTestAllKeys();
    
    // Provider health
    void onCheckProviderHealth();
    void onProviderHealthUpdated();

private:
    void createUI();
    void createApiKeyTab();
    void createConfigurationTab();
    void createProvidersTab();
    void createAdvancedTab();
    void setupConnections();
    void loadSettings();
    void applySettings();
    
    // Helper methods
    void updateProviderStatus();
    void validateApiKeys();
    void saveApiKeyToEnvironment(const std::string& provider, const std::string& key);
    void loadApiKeyFromEnvironment(const std::string& provider);
    std::string maskApiKey(const std::string& key) const;
    bool testApiKey(const std::string& provider, const std::string& key);

    ModelRouterAdapter *m_adapter;
    
    // Main tabs
    void *m_tabs;
    
    // ===== API Keys Tab =====
    // OpenAI
    void *m_openai_key_input;
    void *m_openai_test_button;
    void *m_openai_status_label;
    void *m_openai_visible_checkbox;
    
    // Anthropic
    void *m_anthropic_key_input;
    void *m_anthropic_test_button;
    void *m_anthropic_status_label;
    void *m_anthropic_visible_checkbox;
    
    // Google
    void *m_google_key_input;
    void *m_google_test_button;
    void *m_google_status_label;
    void *m_google_visible_checkbox;
    
    // Moonshot
    void *m_moonshot_key_input;
    void *m_moonshot_test_button;
    void *m_moonshot_status_label;
    void *m_moonshot_visible_checkbox;
    
    // Azure OpenAI
    void *m_azure_key_input;
    void *m_azure_test_button;
    void *m_azure_status_label;
    void *m_azure_visible_checkbox;
    
    // AWS
    void *m_aws_access_key_input;
    void *m_aws_secret_key_input;
    void *m_aws_test_button;
    void *m_aws_status_label;
    void *m_aws_visible_checkbox;
    
    // ===== Configuration Tab =====
    void *m_default_model_combo;
    void *m_prefer_local_models_checkbox;
    void *m_enable_streaming_checkbox;
    void *m_enable_fallback_checkbox;
    
    void *m_timeout_spinbox;      // Request timeout (ms)
    void *m_max_retries_spinbox;
    void *m_retry_delay_spinbox;  // Retry delay (ms)
    
    QDoubleSpinBox *m_cost_limit_spinbox;           // Max cost per request ($)
    QDoubleSpinBox *m_cost_alert_threshold_spinbox; // Alert threshold ($)
    
    // ===== Providers Tab =====
    QTableWidget *m_providers_table;  // Status of all providers
    void *m_check_health_button;
    void *m_health_status_label;
    
    // ===== Advanced Tab =====
    void *m_custom_endpoint_input;
    void *m_connection_pool_size_spinbox;
    void *m_enable_caching_checkbox;
    void *m_enable_metrics_checkbox;
    void *m_metrics_retention_spinbox;  // Days
    
    // Dialog buttons
    void *m_save_button;
    void *m_test_all_button;
    void *m_load_env_button;
    void *m_export_button;
    void *m_import_button;
    void *m_defaults_button;
    void *m_cancel_button;
    
    // State
    bool m_keys_visible[6] = {false};
    std::map<std::string, std::string> m_current_keys;
    bool m_settings_changed = false;
};

#endif // CLOUD_SETTINGS_DIALOG_H

