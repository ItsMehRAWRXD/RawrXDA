#pragma once

#include <optional>
#include <regex>
#include <string>
#include <vector>

namespace ide {

// C++ translation of the JS OLLAMA_CONFIG + helpers in IDEre2.html
// This is a logical mirror of the JS "Ollama integration" section, without any UI/DOM.

struct OllamaConfig {
    std::string baseUrl{"http://localhost:11442"}; // Orchestra proxy to Ollama
    std::vector<std::string> availableModels{};
    std::optional<std::string> selectedModel; // equivalent of localStorage.getItem('ollamaSelectedModel')
};

struct ConnectionStatus {
    bool backendConnected{false};   // /api/health on port 9000
    bool orchestraConnected{false}; // /health + /v1/models on port 11442
};

// A thin wrapper around the Orchestra/Ollama HTTP endpoints used in IDEre2.html.
// NOTE: Network calls are stubbed by default; wire them to a real HTTP client later.
class OllamaClient {
public:
    OllamaClient();

    // Mirror JS: checkBackendConnection()
    // In JS this hits http://localhost:9000/api/health with a 3s timeout and
    // updates status icons. Here we just update the ConnectionStatus flags
    // and optionally log.
    bool checkBackendConnection();

    // Mirror JS: checkOllamaConnection() + checkOrchestraConnection()
    bool checkOllamaConnection();
    bool checkOrchestraConnection();

    // Mirror JS: fetchOllamaModels()
    // Returns the discovered model names and updates config_.availableModels.
    std::vector<std::string> fetchOllamaModels();

    // Mirror JS: window.callOllamaAPI(modelName, prompt, systemPrompt = null)
    // Returns the assistant message content or a fallback error string.
    std::string callOllamaAPI(const std::string& modelName,
                              const std::string& prompt,
                              const std::optional<std::string>& systemPrompt = std::nullopt);

    // Mirror JS: window.isOllamaModel(modelName)
    bool isOllamaModel(const std::string& modelName) const;

    const OllamaConfig& config() const noexcept { return config_; }
    OllamaConfig& config() noexcept { return config_; }

    const ConnectionStatus& status() const noexcept { return status_; }

private:
    static bool matchesOllamaPattern(const std::string& modelName);

private:
    OllamaConfig config_{};
    ConnectionStatus status_{};
};

} // namespace ide
