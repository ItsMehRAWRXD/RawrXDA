#pragma once

#include "model_invoker.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <string>

enum class ModelRole {
    GhostText,
    PlanSynthesis,
    CodeGeneration,
    Verification,
    Summarization,
};

struct ModelRoute {
    ModelRole role = ModelRole::PlanSynthesis;
    ProviderType provider = ProviderType::Unknown;
    std::string modelName;
    std::string endpoint;
    std::string apiKey;
    int maxTokens = 2048;
    double temperature = 0.2;
    bool requiresStructuredOutput = false;
    bool enableSwarm = false;

    bool isConfigured() const {
        switch (provider) {
            case ProviderType::OpenAICompatible:
            case ProviderType::AnthropicNative:
                return !endpoint.empty() && !apiKey.empty() && !modelName.empty();
            case ProviderType::Ollama:
            case ProviderType::LocalGGUF:
                return !endpoint.empty() && !modelName.empty();
            case ProviderType::SwarmDistributed:
                return true;
            case ProviderType::Unknown:
            default:
                return false;
        }
    }

    ModelProviderConfig toProviderConfig() const {
        ModelProviderConfig config;
        config.type = provider;
        config.endpoint = endpoint;
        config.apiKey = apiKey;
        config.model = modelName;
        config.enableSwarm = enableSwarm;
        return config;
    }
};

class ModelPolicyRouter {
public:
    static ModelRoute Select(const std::string& systemPrompt,
                             const std::string& userMessage,
                             const std::string& modelHint) {
        const ModelRole role = Classify(systemPrompt, userMessage);
        return ResolveRoute(role, modelHint);
    }

private:
    static std::string ToLowerCopy(const std::string& value) {
        std::string lower = value;
        std::transform(lower.begin(), lower.end(), lower.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return lower;
    }

    static std::string EnvOrEmpty(const char* name) {
        const char* value = std::getenv(name);
        return value ? std::string(value) : std::string();
    }

    static bool EnvFlagEnabled(const char* name) {
        const std::string value = ToLowerCopy(EnvOrEmpty(name));
        return value == "1" || value == "true" || value == "yes" || value == "on";
    }

    static ProviderType ProviderFromString(const std::string& value) {
        const std::string lower = ToLowerCopy(value);
        if (lower == "ollama") {
            return ProviderType::Ollama;
        }
        if (lower == "localgguf" || lower == "local-gguf" || lower == "llama.cpp") {
            return ProviderType::LocalGGUF;
        }
        if (lower == "anthropic" || lower == "claude") {
            return ProviderType::AnthropicNative;
        }
        if (lower == "swarm" || lower == "swarm-distributed" || lower == "distributed-swarm") {
            return ProviderType::SwarmDistributed;
        }
        if (lower == "openai-compatible" || lower == "openai" || lower == "openrouter" ||
            lower == "together" || lower == "deepinfra") {
            return ProviderType::OpenAICompatible;
        }
        return ProviderType::Unknown;
    }

    static ModelRole Classify(const std::string& systemPrompt,
                              const std::string& userMessage) {
        const std::string combined = ToLowerCopy(systemPrompt + "\n" + userMessage);
        if (combined.find("return only completion text") != std::string::npos ||
            combined.find("inline completion") != std::string::npos) {
            return ModelRole::GhostText;
        }
        if (combined.find("rewrite the selected text") != std::string::npos ||
            combined.find("return only rewritten text") != std::string::npos ||
            combined.find("proposed change") != std::string::npos) {
            return ModelRole::CodeGeneration;
        }
        if (combined.find("build failed") != std::string::npos ||
            combined.find("fatal error") != std::string::npos ||
            combined.find("error c") != std::string::npos ||
            combined.find("lnk") != std::string::npos ||
            combined.find("verify") != std::string::npos ||
            combined.find("validation") != std::string::npos) {
            return ModelRole::Verification;
        }
        if (combined.find("summary") != std::string::npos ||
            combined.find("summarize") != std::string::npos ||
            combined.find("compress") != std::string::npos) {
            return ModelRole::Summarization;
        }
        return ModelRole::PlanSynthesis;
    }

    static ModelRoute ResolveRoute(ModelRole role, const std::string& modelHint) {
        switch (role) {
            case ModelRole::GhostText:
                return LoadRoute(role,
                                 "RAWRXD_GHOST_PROVIDER",
                                 "RAWRXD_GHOST_ENDPOINT",
                                 "RAWRXD_GHOST_API_KEY",
                                 "RAWRXD_GHOST_MODEL",
                                 ProviderType::OpenAICompatible,
                                 modelHint.empty() ? "gpt-4o-mini" : modelHint,
                                 256,
                                 0.15,
                                 false);
            case ModelRole::CodeGeneration:
                return LoadRoute(role,
                                 "RAWRXD_CODE_PROVIDER",
                                 "RAWRXD_CODE_ENDPOINT",
                                 "RAWRXD_CODE_API_KEY",
                                 "RAWRXD_CODE_MODEL",
                                 ProviderType::OpenAICompatible,
                                 modelHint.empty() ? "gpt-4.1" : modelHint,
                                 8192,
                                 0.10,
                                 true);
            case ModelRole::Verification:
                return LoadRoute(role,
                                 "RAWRXD_VERIFY_PROVIDER",
                                 "RAWRXD_VERIFY_ENDPOINT",
                                 "RAWRXD_VERIFY_API_KEY",
                                 "RAWRXD_VERIFY_MODEL",
                                 ProviderType::AnthropicNative,
                                 modelHint.empty() ? "claude-3-7-sonnet" : modelHint,
                                 4096,
                                 0.05,
                                 true);
            case ModelRole::Summarization:
                return LoadRoute(role,
                                 "RAWRXD_SUMMARY_PROVIDER",
                                 "RAWRXD_SUMMARY_ENDPOINT",
                                 "RAWRXD_SUMMARY_API_KEY",
                                 "RAWRXD_SUMMARY_MODEL",
                                 ProviderType::OpenAICompatible,
                                 modelHint.empty() ? "gpt-4o-mini" : modelHint,
                                 2048,
                                 0.20,
                                 false);
            case ModelRole::PlanSynthesis:
            default:
                return LoadRoute(role,
                                 "RAWRXD_PLAN_PROVIDER",
                                 "RAWRXD_PLAN_ENDPOINT",
                                 "RAWRXD_PLAN_API_KEY",
                                 "RAWRXD_PLAN_MODEL",
                                 ProviderType::OpenAICompatible,
                                 modelHint.empty() ? "gpt-4.1" : modelHint,
                                 4096,
                                 0.10,
                                 true);
        }
    }

    static ModelRoute LoadRoute(ModelRole role,
                                const char* providerEnv,
                                const char* endpointEnv,
                                const char* apiKeyEnv,
                                const char* modelEnv,
                                ProviderType fallbackProvider,
                                const std::string& fallbackModel,
                                int maxTokens,
                                double temperature,
                                bool requiresStructuredOutput) {
        ModelRoute route;
        route.role = role;
        route.provider = ProviderFromString(EnvOrEmpty(providerEnv));
        if (route.provider == ProviderType::Unknown) {
            route.provider = fallbackProvider;
        }
        route.endpoint = EnvOrEmpty(endpointEnv);
        if (route.endpoint.empty()) {
            route.endpoint = EnvOrEmpty("RAWRXD_PROVIDER_ENDPOINT");
        }
        route.apiKey = EnvOrEmpty(apiKeyEnv);
        if (route.apiKey.empty()) {
            route.apiKey = EnvOrEmpty("RAWRXD_PROVIDER_KEY");
        }
        route.modelName = EnvOrEmpty(modelEnv);
        if (route.modelName.empty()) {
            route.modelName = fallbackModel;
        }
        route.maxTokens = maxTokens;
        route.temperature = temperature;
        route.requiresStructuredOutput = requiresStructuredOutput;
        route.enableSwarm = EnvFlagEnabled("RAWRXD_ENABLE_SWARM") || route.provider == ProviderType::SwarmDistributed;
        return route;
    }
};
