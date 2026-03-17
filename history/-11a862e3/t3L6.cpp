#include "ollama_client.hpp"

#include <iostream>

namespace ide {

OllamaClient::OllamaClient() {
    // In JS, selectedModel is read from localStorage; here we start empty.
    config_.selectedModel = std::nullopt;
}

bool OllamaClient::checkBackendConnection() {
    // JS equivalent:
    //   fetch('http://localhost:9000/api/health', { method: 'GET', signal: AbortSignal.timeout(3000) })
    // For now we just mark it as unknown/false and print a stub message.
    // Wire this up to a real HTTP GET in the future.

    std::cout << "[OllamaClient] checkBackendConnection(): stub - call /api/health on :9000 here.\n";
    status_.backendConnected = false; // unknown in stub mode
    return status_.backendConnected;
}

bool OllamaClient::checkOllamaConnection() {
    // JS equivalent: checkOllamaConnection() hits `${baseUrl}/v1/models` and inspects data.data.length.
    std::cout << "[OllamaClient] checkOllamaConnection(): stub - call /v1/models on Orchestra here.\n";
    status_.orchestraConnected = false; // unknown in stub mode
    return status_.orchestraConnected;
}

bool OllamaClient::checkOrchestraConnection() {
    // JS equivalent: checkOrchestraConnection() hits `${baseUrl}/health`.
    std::cout << "[OllamaClient] checkOrchestraConnection(): stub - call /health on Orchestra here.\n";
    // We reuse the same flag as checkOllamaConnection in this skeleton.
    status_.orchestraConnected = false;
    return status_.orchestraConnected;
}

std::vector<std::string> OllamaClient::fetchOllamaModels() {
    // JS equivalent:
    //   const res = await fetch(`${baseUrl}/v1/models`, ...);
    //   const models = data.data?.map(m => m.id) || data.models?.map(m => m.name) || [];
    // For now, we just clear the list and return an empty vector.

    std::cout << "[OllamaClient] fetchOllamaModels(): stub - call /v1/models and parse JSON here.\n";
    config_.availableModels.clear();
    return config_.availableModels;
}

std::string OllamaClient::callOllamaAPI(const std::string& modelName,
                                        const std::string& prompt,
                                        const std::optional<std::string>& systemPrompt) {
    // JS equivalent builds an OpenAI-style /v1/chat/completions request:
    //   body: { model, messages: [{role: 'system'}, {role:'user'}], temperature, max_tokens }
    // and returns the first choice.message.content.
    // Here we only mirror behavior textually as a stub.

    if (!status_.orchestraConnected && !status_.backendConnected) {
        return "[OllamaClient] Offline: Orchestra/backend not connected. "
               "Ensure port 11442 or 9000 is running for full AI capabilities.";
    }

    std::string description;
    description += "[OllamaClient] Would call \"" + config_.baseUrl + "/v1/chat/completions\" with model=\"";
    description += modelName + "\"\n";
    if (systemPrompt && !systemPrompt->empty()) {
        description += "System: " + *systemPrompt + "\n";
    }
    description += "User: " + prompt + "\n";
    description += "(This is a stub implementation; wire a real HTTP client and JSON parser here.)";

    return description;
}

bool OllamaClient::isOllamaModel(const std::string& modelName) const {
    // JS equivalent checks OLLAMA_CONFIG.availableModels and a list of regex patterns.

    for (const auto& m : config_.availableModels) {
        if (m == modelName) {
            return true;
        }
    }

    return matchesOllamaPattern(modelName);
}

bool OllamaClient::matchesOllamaPattern(const std::string& modelName) {
    static const std::vector<std::regex> patterns = {
        std::regex{"^llama", std::regex::icase},
        std::regex{"^gemma", std::regex::icase},
        std::regex{"^mistral", std::regex::icase},
        std::regex{"^codellama", std::regex::icase},
        std::regex{"^deepseek", std::regex::icase},
        std::regex{"^qwen", std::regex::icase},
        std::regex{"^phi", std::regex::icase},
        std::regex{"^neural-chat", std::regex::icase},
        std::regex{"^star-coder", std::regex::icase},
        std::regex{"^nous-hermes", std::regex::icase},
        std::regex{"^vicuna", std::regex::icase},
        std::regex{"^wizard", std::regex::icase},
        std::regex{"^orca", std::regex::icase},
        std::regex{"^falcon", std::regex::icase},
        std::regex{"^dolphin", std::regex::icase},
        std::regex{"^openchat", std::regex::icase},
        std::regex{"^solar", std::regex::icase},
        std::regex{"^yi", std::regex::icase},
        std::regex{"^tinyllama", std::regex::icase},
        std::regex{"^starcoder", std::regex::icase},
        std::regex{":latest$", std::regex::icase},
        std::regex{":.*b$", std::regex::icase},
        std::regex{"cheetah-stealth", std::regex::icase},
        std::regex{"code-supernova", std::regex::icase},
    };

    for (const auto& pat : patterns) {
        if (std::regex_search(modelName, pat)) {
            return true;
        }
    }
    return false;
}

} // namespace ide
