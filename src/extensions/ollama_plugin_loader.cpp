// ollama_plugin_loader.cpp - Plugin loader for Ollama model provider
#include "plugin_loader.h"
#include "ollama_model_provider.h"
#include "ollama_chat_integration.h"
#include <map>
#include <memory>

using namespace RawrXD::Extensions::Ollama;

namespace RawrXD::Extensions {

class OllamaPlugin {
private:
    PluginInfo m_info;
    OllamaModelProvider* m_provider;
    std::map<HWND, std::unique_ptr<OllamaChatIntegration>> m_chatIntegrations;
    
public:
    OllamaPlugin() : m_provider(nullptr) {
        m_info.name = "ollama-model-provider";
        m_info.version = "1.0.0";
        m_info.description = "Unified model provider for Ollama local and cloud models";
        m_info.author = "RawrXD Extension Team";
        m_info.apiVersion = 1;
        m_info.valid = true;
        m_info.permissions = {
            "model:discover", "model:select", "chat:create", "chat:manage", 
            "ui:dropdown", "network:access"
        };
    }

    ~OllamaPlugin() {
        if (m_provider) {
            m_provider->Shutdown();
            DestroyOllamaProvider(m_provider);
            m_provider = nullptr;
        }
    }

    bool Load() {
        m_provider = CreateOllamaProvider();
        if (!m_provider) {
            return false;
        }

        // Initialize with default config
        nlohmann::json config;
        config["ollamaEndpoint"] = "http://localhost:11435";
        config["cloudModels"] = std::vector<std::string>();
        config["autoDiscover"] = true;

        return m_provider->Initialize(config.dump());
    }

    void Unload() {
        m_chatIntegrations.clear();
        if (m_provider) {
            m_provider->Shutdown();
            DestroyOllamaProvider(m_provider);
            m_provider = nullptr;
        }
    }

    PluginInfo GetInfo() const {
        return m_info;
    }

    OllamaModelProvider* GetProvider() const {
        return m_provider;
    }

    bool CreateChatIntegration(HWND chatWindow) {
        if (!m_provider) return false;

        auto integration = std::make_unique<OllamaChatIntegration>(chatWindow, m_provider);
        if (integration->Initialize()) {
            m_chatIntegrations[chatWindow] = std::move(integration);
            return true;
        }
        
        return false;
    }

    void RemoveChatIntegration(HWND chatWindow) {
        m_chatIntegrations.erase(chatWindow);
    }

    OllamaChatIntegration* GetChatIntegration(HWND chatWindow) {
        auto it = m_chatIntegrations.find(chatWindow);
        if (it != m_chatIntegrations.end()) {
            return it->second.get();
        }
        return nullptr;
    }
};

// Global plugin instance
static std::unique_ptr<OllamaPlugin> g_ollamaPlugin;

// Plugin registration
extern "C" __declspec(dllexport) bool RegisterOllamaPlugin(PluginLoader* loader) {
    if (!g_ollamaPlugin) {
        g_ollamaPlugin = std::make_unique<OllamaPlugin>();
    }

    if (g_ollamaPlugin->Load()) {
        loader->registerPlugin(g_ollamaPlugin->GetInfo());
        return true;
    }

    return false;
}

extern "C" __declspec(dllexport) void UnregisterOllamaPlugin() {
    if (g_ollamaPlugin) {
        g_ollamaPlugin->Unload();
        g_ollamaPlugin.reset();
    }
}

extern "C" __declspec(dllexport) OllamaModelProvider* GetOllamaProvider() {
    if (g_ollamaPlugin) {
        return g_ollamaPlugin->GetProvider();
    }
    return nullptr;
}

extern "C" __declspec(dllexport) bool CreateOllamaChatIntegration(HWND chatWindow) {
    if (g_ollamaPlugin) {
        return g_ollamaPlugin->CreateChatIntegration(chatWindow);
    }
    return false;
}

extern "C" __declspec(dllexport) void RemoveOllamaChatIntegration(HWND chatWindow) {
    if (g_ollamaPlugin) {
        g_ollamaPlugin->RemoveChatIntegration(chatWindow);
    }
}

extern "C" __declspec(dllexport) OllamaChatIntegration* GetOllamaChatIntegration(HWND chatWindow) {
    if (g_ollamaPlugin) {
        return g_ollamaPlugin->GetChatIntegration(chatWindow);
    }
    return nullptr;
}

} // namespace RawrXD::Extensions
