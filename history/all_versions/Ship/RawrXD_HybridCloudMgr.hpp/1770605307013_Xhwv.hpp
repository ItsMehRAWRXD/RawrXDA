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
        // Load and call RawrXD_InferenceEngine.dll for local GGUF inference
        HMODULE hEngine = GetModuleHandleA("RawrXD_InferenceEngine.dll");
        if (!hEngine) {
            hEngine = LoadLibraryA("RawrXD_InferenceEngine.dll");
        }
        if (!hEngine) {
            std::cerr << "[HybridCloud] Failed to load RawrXD_InferenceEngine.dll: " 
                      << GetLastError() << std::endl;
            return "Error: Could not load local inference engine";
        }
        
        // Get function pointers for inference
        typedef int (*InitModelFn)(const char* modelPath);
        typedef int (*GenerateFn)(const char* prompt, char* output, int maxLen, float temp, float topP);
        typedef void (*ShutdownFn)();
        
        auto pInit = (InitModelFn)GetProcAddress(hEngine, "InitModel");
        auto pGenerate = (GenerateFn)GetProcAddress(hEngine, "Generate");
        
        if (!pGenerate) {
            // Try alternative export names
            pGenerate = (GenerateFn)GetProcAddress(hEngine, "RunInference");
        }
        
        if (!pGenerate) {
            std::cerr << "[HybridCloud] Missing Generate/RunInference export in engine DLL" << std::endl;
            return "Error: Inference engine DLL missing required exports";
        }
        
        // Generate response
        constexpr int maxOutputLen = 4096;
        std::vector<char> outputBuf(maxOutputLen, '\0');
        
        int result = pGenerate(prompt.c_str(), outputBuf.data(), maxOutputLen, 0.7f, 0.95f);
        
        if (result < 0) {
            std::cerr << "[HybridCloud] Local inference failed with code: " << result << std::endl;
            return "Error: Local inference failed (code " + std::to_string(result) + ")";
        }
        
        std::string output(outputBuf.data());
        if (output.empty()) {
            return "Error: Empty response from local inference engine";
        }
        
        std::cout << "[HybridCloud] Local inference complete: " << output.size() << " chars" << std::endl;
        return output;
    }
};

} // namespace RawrXD
