// test_ollama_plugin.cpp - Simple test for Ollama plugin compilation
#include "ollama_model_provider.h"
#include "ollama_chat_integration.h"
#include "ollama_plugin_loader.h"

int main() {
    // Test basic compilation
    auto provider = RawrXD::Extensions::Ollama::CreateOllamaProvider();
    if (provider) {
        provider->Initialize("{}");
        auto models = provider->DiscoverModels();
        provider->Shutdown();
        RawrXD::Extensions::Ollama::DestroyOllamaProvider(provider);
    }
    
    return 0;
}
