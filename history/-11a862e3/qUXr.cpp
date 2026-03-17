#include "ollama_client.hpp"

#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <system_error>

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
    //
    // Here we perform a real HTTP call using the system `curl` binary so the
    // feature is actually end-to-end functional without extra C++ libraries.

    // Build JSON body with minimal escaping for quotes and backslashes.
    auto escapeJson = [](const std::string& s) {
        std::string out;
        out.reserve(s.size() + 16);
        for (char ch : s) {
            switch (ch) {
            case '\\': out += "\\\\"; break;
            case '"':  out += "\\\""; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:    out += ch; break;
            }
        }
        return out;
    };

    std::ostringstream body;
    body << "{\n";
    body << "  \"model\": \"" << escapeJson(modelName) << "\",\n";
    body << "  \"messages\": [";
    bool first = true;
    if (systemPrompt && !systemPrompt->empty()) {
        body << "{\"role\":\"system\",\"content\":\"" << escapeJson(*systemPrompt) << "\"}";
        first = false;
    }
    if (!first) body << ",";
    body << "{\"role\":\"user\",\"content\":\"" << escapeJson(prompt) << "\"}";
    body << "],\n";
    body << "  \"temperature\": 0.7,\n";
    body << "  \"max_tokens\": 2048\n";
    body << "}";

    const std::string requestPath = "ollama_request.json";
    const std::string responsePath = "ollama_response.json";

    {
        std::ofstream req(requestPath, std::ios::binary | std::ios::trunc);
        if (!req) {
            return "[OllamaClient] Failed to write request body to " + requestPath;
        }
        req << body.str();
    }

    // Use curl to POST the request body and capture JSON response.
    std::string cmd = "curl -s -X POST \"" + config_.baseUrl + "/v1/chat/completions\" "
                      "-H \"Content-Type: application/json\" "
                      "-H \"Authorization: Bearer bigdaddyg-secret-key-2024\" "
                      "--data @" + requestPath + " > " + responsePath + " 2>nul";

    int rc = std::system(cmd.c_str());
    if (rc != 0) {
        return "[OllamaClient] curl failed with exit code " + std::to_string(rc) +
               "; ensure curl and Orchestra (port 11442) are available.";
    }

    std::ifstream resp(responsePath, std::ios::binary);
    if (!resp) {
        return "[OllamaClient] Failed to read response from " + responsePath;
    }

    std::string json((std::istreambuf_iterator<char>(resp)), std::istreambuf_iterator<char>());
    if (json.empty()) {
        return "[OllamaClient] Empty response from Orchestra/Ollama.";
    }

    // Very small JSON extractor: look for the first \"content\": " field and
    // return its string value, handling basic escapes.
    const std::string key = "\"content\"";
    auto pos = json.find(key);
    if (pos == std::string::npos) {
        return "[OllamaClient] Unexpected JSON (no content field):\n" + json;
    }
    pos = json.find(':', pos);
    if (pos == std::string::npos) {
        return "[OllamaClient] Malformed JSON after content field.";
    }
    auto firstQuote = json.find('"', pos);
    if (firstQuote == std::string::npos) {
        return "[OllamaClient] Malformed JSON: missing opening quote for content.";
    }

    std::string result;
    bool escape = false;
    for (std::size_t i = firstQuote + 1; i < json.size(); ++i) {
        char ch = json[i];
        if (escape) {
            switch (ch) {
            case 'n': result.push_back('\n'); break;
            case 'r': result.push_back('\r'); break;
            case 't': result.push_back('\t'); break;
            case '"': result.push_back('"'); break;
            case '\\': result.push_back('\\'); break;
            default: result.push_back(ch); break;
            }
            escape = false;
            continue;
        }
        if (ch == '\\') {
            escape = true;
            continue;
        }
        if (ch == '"') {
            break;
        }
        result.push_back(ch);
    }

    if (result.empty()) {
        return "[OllamaClient] Parsed empty content; raw JSON:\n" + json;
    }

    return result;
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
