// RawrXD Inference Stubs — Compilation Fallback
// These ONLY activate when the real implementation headers cannot be found.
// Real implementations:
//   rawrxd_model_loader.h  + rawrxd_model_loader.cpp
//   rawrxd_transformer.h   + rawrxd_transformer.cpp
//   rawrxd_sampler.h       + rawrxd_sampler.cpp
//   rawrxd_tokenizer.h     + rawrxd_tokenizer.cpp

#pragma once

// Guard: Only define stubs if the real headers haven't been included
#if !defined(RAWRXD_REAL_MODEL_LOADER_INCLUDED)
#if __has_include("rawrxd_model_loader.h")
    // Real header available — include it instead of stubs
    #include "rawrxd_model_loader.h"
    #define RAWRXD_REAL_MODEL_LOADER_INCLUDED 1
#else
    #include <string>
    #include <vector>
    // Minimal fallback — Load always fails, forcing Ollama path
    class RawrXDModelLoader {
    public:
        struct Tensor {
            std::string name;
            uint32_t type = 0;
            std::vector<uint64_t> dims;
            void* data = nullptr;
            bool onGPU = false;
            std::vector<float> cpuFloatData;
        };
        std::unordered_map<std::string, Tensor> tensors;
        inline bool Load(const wchar_t*, void*, void*) { return false; }
        inline float* GetTensor(const std::string&) { return nullptr; }
        int getDim() const { return 0; }
        int getLayers() const { return 0; }
        int getHeads() const { return 0; }
        int getKVHeads() const { return 0; }
        int getVocabSize() const { return 0; }
    };
    #define RAWRXD_REAL_MODEL_LOADER_INCLUDED 1
#endif
#endif

#if !defined(RAWRXD_REAL_TRANSFORMER_INCLUDED)
#if __has_include("rawrxd_transformer.h")
    #include "rawrxd_transformer.h"
    #define RAWRXD_REAL_TRANSFORMER_INCLUDED 1
#else
    class RawrXDTransformer {
    public:
        struct Config {
            int dim = 0; int hidden_dim = 0; int n_layers = 0;
            int n_heads = 0; int n_kv_heads = 0; int vocab_size = 0;
            int n_ctx = 4096; int seq_len = 4096;
            float rope_theta = 10000.0f; float rms_norm_eps = 1e-5f;
        };
        inline void Initialize(void*, void*, Config, RawrXDModelLoader*) {}
        inline std::vector<float> Forward(const std::vector<uint32_t>&, int) {
            return {}; // Empty logits — signals error to caller
        }
    };
    #define RAWRXD_REAL_TRANSFORMER_INCLUDED 1
#endif
#endif

#if !defined(RAWRXD_REAL_SAMPLER_INCLUDED)
#if __has_include("rawrxd_sampler.h")
    #include "rawrxd_sampler.h"
    #define RAWRXD_REAL_SAMPLER_INCLUDED 1
#else
    class RawrXDSampler {
    public:
        inline uint32_t Sample(float* logits, int size, const std::vector<uint32_t>&) {
            if (!logits || size <= 0) return 2; // EOS
            // Argmax fallback
            int best = 0;
            for (int i = 1; i < size; i++) {
                if (logits[i] > logits[best]) best = i;
            }
            return (uint32_t)best;
        }
    };
    #define RAWRXD_REAL_SAMPLER_INCLUDED 1
#endif
#endif
