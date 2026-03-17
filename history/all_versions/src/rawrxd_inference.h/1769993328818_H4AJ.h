// rawrxd_inference.h
// Full inference pipeline with all components

#pragma once

#include <string>
#include <vector>
#include <memory>
#include <cstdint>

// Forward declarations
class RawrXDModelLoader;
class RawrXDTransformer;
class RawrXDTokenizer;
class RawrXDSampler;

class RawrXDInference {
    std::unique_ptr<RawrXDModelLoader> modelLoader;
    std::unique_ptr<RawrXDTransformer> transformer;
    std::unique_ptr<RawrXDTokenizer> tokenizer;
    std::unique_ptr<RawrXDSampler> sampler;

    // EOS ID constant
    const uint32_t EOS_ID = 2; // Typically 2 for LLaMA

public:
    RawrXDInference();
    ~RawrXDInference();

    bool Initialize(const wchar_t* modelPath,
                   const char* vocabPath,
                   const char* mergesPath);

    std::string Generate(const std::string& prompt,
                        uint32_t maxTokens = 512);

    // Accessors for new implementation
    RawrXDModelLoader* GetLoader() const { return modelLoader.get(); }
    RawrXDTransformer* GetTransformer() const { return transformer.get(); }
};
