#pragma once

#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "RawrXD_Interfaces.h"
#include "real_time_completion_engine.h"

class TestInferenceEngine : public RawrXD::InferenceEngine {
public:
    explicit TestInferenceEngine(std::string completionText = "captured_result", bool captureFirstPrompt = true)
        : completionText_(std::move(completionText)), captureFirstPrompt_(captureFirstPrompt) {}

    bool LoadModel(const std::string& model_path) override {
        loaded_ = true;
        modelPath_ = model_path;
        return true;
    }

    bool IsModelLoaded() const override {
        return loaded_;
    }

    std::vector<int32_t> Tokenize(const std::string& text) override {
        if (captureFirstPrompt_ && capturedPrompt_.empty()) {
            capturedPrompt_ = text;
        }
        std::vector<int32_t> tokens;
        tokens.reserve(text.size());
        for (unsigned char ch : text) {
            tokens.push_back(static_cast<int32_t>(ch));
        }
        return tokens;
    }

    std::string Detokenize(const std::vector<int32_t>& tokens) override {
        std::string text;
        text.reserve(tokens.size());
        for (int32_t token : tokens) {
            text.push_back(static_cast<char>(token));
        }
        return text;
    }

    std::vector<int32_t> Generate(const std::vector<int32_t>& input_tokens, int max_tokens) override {
        std::vector<int32_t> output = input_tokens;
        const auto completionTokens = Tokenize(
            completionText_.substr(0, static_cast<size_t>(std::min<int>(max_tokens, static_cast<int>(completionText_.size())))));
        output.insert(output.end(), completionTokens.begin(), completionTokens.end());
        return output;
    }

    std::vector<float> Eval(const std::vector<int32_t>& input_tokens) override {
        return std::vector<float>(input_tokens.size(), 1.0f);
    }

    void GenerateStreaming(
        const std::vector<int32_t>& input_tokens,
        int max_tokens,
        std::function<void(const std::string&)> token_callback,
        std::function<void()> complete_callback,
        std::function<void(int32_t)> token_id_callback = nullptr) override {
        const auto output = Generate(input_tokens, max_tokens);
        for (size_t i = input_tokens.size(); i < output.size(); ++i) {
            const std::string tokenText(1, static_cast<char>(output[i]));
            if (token_callback) {
                token_callback(tokenText);
            }
            if (token_id_callback) {
                token_id_callback(output[i]);
            }
        }
        if (complete_callback) {
            complete_callback();
        }
    }

    int GetVocabSize() const override { return 256; }
    int GetEmbeddingDim() const override { return 64; }
    int GetNumLayers() const override { return 2; }
    int GetNumHeads() const override { return 4; }

    void SetMaxMode(bool enabled) override { maxMode_ = enabled; }
    void SetDeepThinking(bool enabled) override { deepThinking_ = enabled; }
    void SetDeepResearch(bool enabled) override { deepResearch_ = enabled; }
    bool IsMaxMode() const override { return maxMode_; }
    bool IsDeepThinking() const override { return deepThinking_; }
    bool IsDeepResearch() const override { return deepResearch_; }

    size_t GetMemoryUsage() const override { return 0; }
    void ClearCache() override {}
    const char* GetEngineName() const override { return "TestInferenceEngine"; }

    void setCaptureFirstPrompt(bool enabled) { captureFirstPrompt_ = enabled; }
    const std::string& capturedPrompt() const { return capturedPrompt_; }
    const std::string& lastPrompt() const { return capturedPrompt_; }

private:
    bool loaded_ = false;
    bool maxMode_ = false;
    bool deepThinking_ = false;
    bool deepResearch_ = false;
    std::string modelPath_;
    std::string completionText_;
    bool captureFirstPrompt_ = false;
    std::string capturedPrompt_;
};

class StaticExecutionStateProvider : public IExecutionStateProvider {
public:
    explicit StaticExecutionStateProvider(ExecutionStateSnapshot snapshot)
        : snapshot_(std::move(snapshot)) {}

    bool getLatestSnapshot(ExecutionStateSnapshot& outSnapshot) override {
        outSnapshot = snapshot_;
        return true;
    }

private:
    ExecutionStateSnapshot snapshot_;
};

inline int64_t testNowUnixMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}

inline bool testContains(const std::string& text, const std::string& needle) {
    return text.find(needle) != std::string::npos;
}