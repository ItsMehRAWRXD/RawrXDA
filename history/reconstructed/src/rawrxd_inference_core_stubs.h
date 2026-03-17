// RawrXD Inference Core — Unified Header
// Includes all components needed for the local GGUF inference pipeline.
// When Vulkan SDK is absent, stubs cause vkCreateInstance to fail,
// and callModel() falls through to the Ollama HTTP backend.

#pragma once
#include <vector>
#include <string>
#include <atomic>
#include <random>
#include <functional>

// Pull in the real implementations (or stubs if headers missing)
#if __has_include("rawrxd_inference.h")
    #include "rawrxd_inference.h"
#else
    // Minimal orchestrator when full inference isn't available
    #include "rawrxd_inference_stubs.hpp"
    #include "rawrxd_tokenizer.h"

    class RawrXDInference {
        RawrXDTokenizer tokenizer;
        bool m_initialized = false;
    public:
        bool Initialize(const wchar_t*, const char* vocabPath, const char*) {
            tokenizer.Load(vocabPath ? vocabPath : "");
            // No Vulkan, no local model — will fail, triggering Ollama fallback
            return false;
        }
        bool IsInitialized() const { return m_initialized; }
        std::vector<uint32_t> Tokenize(const std::string& text) {
            return tokenizer.Encode(text);
        }
        std::string Detokenize(const std::vector<uint32_t>& tokens) {
            return tokenizer.Decode(tokens);
        }
        std::string Generate(const std::string&, uint32_t = 512,
                            std::function<void(const std::string&)> = nullptr) {
            return ""; // Empty = error, caller falls back to Ollama
        }
    };
#endif
