// RawrXD_HybridCloudMgr.hpp - Orchestrator for Local/Cloud Inference
// Pure C++20 - No Qt Dependencies
// Fallback logic: Titan Core -> Cloud API

#pragma once

#include "RawrXD_CloudClient.hpp"
#include <string>
#include <iostream>

namespace RawrXD {

class HybridCloudManager {
public:
    struct Config {
        bool preferLocal = true;
        std::string cloudProvider = "openai";
        std::string cloudModel = "gpt-4o";
        std::string apiKey = "";
    };

    explicit HybridCloudManager(const Config& config) : m_config(config) {}

    std::string Generate(const std::string& prompt) {
        if (m_config.preferLocal) {
            // Check if local Titan is active
            if (IsLocalTitanAvailable()) {
                std::cout << "[HybridCloud] Using local Titan inference...\n";
                return RunLocalInference(prompt);
            }
            std::cout << "[HybridCloud] Local Titan unavailable, falling back to cloud...\n";
        }

        auto res = CloudClient::Generate(m_config.cloudProvider, m_config.cloudModel, m_config.apiKey, prompt);
        if (res.success) {
            // Parse OpenAI style response
            auto json = JSONParser::Parse(res.content);
            if (json.has("choices") && json["choices"].size() > 0) {
                return json["choices"][0]["message"]["content"].asString();
            }
        }
        return "Error: " + res.error;
    }

private:
    Config m_config;

    bool IsLocalTitanAvailable() {
        // Hypothetical check for RawrXD_Titan_Kernel.dll presence
        DWORD attr = GetFileAttributesA("RawrXD_Titan_Kernel.dll");
        return (attr != INVALID_FILE_ATTRIBUTES);
    }

    std::string RunLocalInference(const std::string& prompt) {
        // Placeholder for calling RawrXD_InferenceEngine.dll
        return "Local Titan: " + prompt; // Mock
    }
};

} // namespace RawrXD
