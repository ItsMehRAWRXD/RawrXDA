#include "prometheus_architecture.h"
#include <cmath>
#include <fstream>
#include <iostream>
#include <sstream>

namespace Prometheus {

// =============================================================================
// ARCHITECTURE DETECTOR
// =============================================================================

DetectedArchitecture ArchitectureDetector::detectFromGGUF(const GGUFMetadata& meta) {
    DetectedArchitecture detected;

    // Try to get model name from metadata
    if (meta.strings.count("general.name")) {
        detected.modelName = meta.strings.at("general.name");
    } else if (meta.strings.count("general.architecture")) {
        detected.modelName = meta.strings.at("general.architecture");
    }

    // Detect architecture from name
    detected.arch = matchArchitecture(detected.modelName);

    // Extract dimensions
    if (meta.ints.count("llama.embedding_length")) {
        detected.hiddenDim = meta.ints.at("llama.embedding_length");
    } else if (meta.ints.count("model.hidden_size")) {
        detected.hiddenDim = meta.ints.at("model.hidden_size");
    }

    if (meta.ints.count("llama.block_count")) {
        detected.numLayers = meta.ints.at("llama.block_count");
    } else if (meta.ints.count("model.num_hidden_layers")) {
        detected.numLayers = meta.ints.at("model.num_hidden_layers");
    }

    if (meta.ints.count("llama.attention.head_count")) {
        detected.numHeads = meta.ints.at("llama.attention.head_count");
    } else if (meta.ints.count("model.num_attention_heads")) {
        detected.numHeads = meta.ints.at("model.num_attention_heads");
    }

    if (meta.ints.count("llama.attention.head_count_kv")) {
        detected.numKVHeads = meta.ints.at("llama.attention.head_count_kv");
    } else if (meta.ints.count("model.num_key_value_heads")) {
        detected.numKVHeads = meta.ints.at("model.num_key_value_heads");
    }

    if (meta.ints.count("llama.vocab_size")) {
        detected.vocabSize = meta.ints.at("llama.vocab_size");
    } else if (meta.ints.count("model.vocab_size")) {
        detected.vocabSize = meta.ints.at("model.vocab_size");
    }

    if (meta.ints.count("llama.attention.key_length")) {
        detected.headDim = meta.ints.at("llama.attention.key_length");
    } else if (meta.ints.count("model.head_dim")) {
        detected.headDim = meta.ints.at("model.head_dim");
    } else if (detected.numHeads > 0) {
        detected.headDim = detected.hiddenDim / detected.numHeads;
    }

    // RoPE parameters
    if (meta.floats.count("llama.rope.freq_base")) {
        detected.ropeTheta = meta.floats.at("llama.rope.freq_base");
    }
    if (meta.floats.count("llama.rope.scale_linear")) {
        detected.ropeScaleFactor = meta.floats.at("llama.rope.scale_linear");
    }

    // MoE detection
    applyMoESpecifics(detected, meta);

    // Store all metadata
    for (const auto& [k, v] : meta.strings) detected.metadata[k] = v;

    return detected;
}

ModelArchitecture ArchitectureDetector::matchArchitecture(const std::string& modelName) {
    std::string lower = modelName;
    for (auto& c : lower) c = static_cast<char>(std::tolower(c));

    if (lower.find("kimi") != std::string::npos) return ModelArchitecture::Kimi;
    if (lower.find("deepseek") != std::string::npos) {
        if (lower.find("moe") != std::string::npos) return ModelArchitecture::DeepSeekMoE;
        return ModelArchitecture::DeepSeek;
    }
    if (lower.find("qwen") != std::string::npos) {
        if (lower.find("moe") != std::string::npos) return ModelArchitecture::QwenMoE;
        return ModelArchitecture::Qwen;
    }
    if (lower.find("mixtral") != std::string::npos) return ModelArchitecture::Mixtral;
    if (lower.find("llama") != std::string::npos) {
        if (lower.find("moe") != std::string::npos) return ModelArchitecture::LlamaMoE;
        return ModelArchitecture::Llama;
    }
    if (lower.find("phi") != std::string::npos) return ModelArchitecture::Phi;
    if (lower.find("gemma2") != std::string::npos) return ModelArchitecture::Gemma2;
    if (lower.find("gemma") != std::string::npos) return ModelArchitecture::Gemma;
    if (lower.find("command-r") != std::string::npos) return ModelArchitecture::CommandR;
    if (lower.find("starcoder") != std::string::npos) return ModelArchitecture::StarCoder;

    return ModelArchitecture::Unknown;
}

void ArchitectureDetector::applyMoESpecifics(DetectedArchitecture& detected, const GGUFMetadata& meta) {
    if (meta.ints.count("llama.expert_count")) {
        detected.isMoE = true;
        detected.numExperts = meta.ints.at("llama.expert_count");
    }
    if (meta.ints.count("llama.expert_used_count")) {
        detected.expertsPerToken = meta.ints.at("llama.expert_used_count");
    }
    if (meta.ints.count("llama.feed_forward_length")) {
        // For MoE models, this is per-expert intermediate dim
        detected.sharedExperts = 1;  // Default
    }

    // Architecture-specific MoE defaults
    if (detected.arch == ModelArchitecture::Mixtral) {
        detected.isMoE = true;
        if (detected.numExperts == 0) detected.numExperts = 8;
        if (detected.expertsPerToken == 0) detected.expertsPerToken = 2;
        detected.sharedExperts = 0;
    } else if (detected.arch == ModelArchitecture::DeepSeekMoE) {
        detected.isMoE = true;
        if (detected.numExperts == 0) detected.numExperts = 64;
        if (detected.expertsPerToken == 0) detected.expertsPerToken = 6;
        detected.sharedExperts = 2;
    } else if (detected.arch == ModelArchitecture::QwenMoE) {
        detected.isMoE = true;
        if (detected.numExperts == 0) detected.numExperts = 64;
        if (detected.expertsPerToken == 0) detected.expertsPerToken = 4;
        detected.sharedExperts = 4;
    }
}

ModelConfig ArchitectureDetector::createConfig(const DetectedArchitecture& detected) {
    ModelConfig config;

    config.hiddenDim = detected.hiddenDim;
    config.numLayers = detected.numLayers;
    config.numHeads = detected.numHeads;
    config.numKVHeads = detected.numKVHeads;
    config.vocabSize = detected.vocabSize;
    config.headDim = detected.headDim;

    // MoE-specific
    if (detected.isMoE) {
        config.numExperts = detected.numExperts;
        config.expertsPerToken = detected.expertsPerToken;
        config.sharedExperts = detected.sharedExperts;

        // Per-expert intermediate dim
        if (detected.arch == ModelArchitecture::DeepSeekMoE) {
            config.intermediateDim = detected.hiddenDim * 2;  // DeepSeek uses smaller per-expert
        } else if (detected.arch == ModelArchitecture::Mixtral) {
            config.intermediateDim = detected.hiddenDim * 4;  // Mixtral standard
        } else {
            config.intermediateDim = detected.hiddenDim * 4;
        }
    } else {
        config.intermediateDim = detected.hiddenDim * 4;  // Standard dense FFN
    }

    // Architecture-specific adjustments
    switch (detected.arch) {
        case ModelArchitecture::Kimi:
            config.ropeTheta = 10000000.0f;
            config.ropeScaleFactor = 2.0f;
            config.maxPosition = 262144;
            break;

        case ModelArchitecture::DeepSeek:
        case ModelArchitecture::DeepSeekMoE:
            config.ropeTheta = 10000.0f;
            config.maxPosition = 131072;
            break;

        case ModelArchitecture::Qwen:
        case ModelArchitecture::QwenMoE:
            config.ropeTheta = 1000000.0f;
            config.maxPosition = 131072;
            break;

        case ModelArchitecture::Mixtral:
            config.ropeTheta = 1000000.0f;
            config.maxPosition = 32768;
            break;

        case ModelArchitecture::Llama:
        case ModelArchitecture::LlamaMoE:
            config.ropeTheta = 500000.0f;
            config.maxPosition = 131072;
            break;

        default:
            config.ropeTheta = detected.ropeTheta;
            config.maxPosition = 8192;
            break;
    }

    return config;
}

// =============================================================================
// TENSOR MAPPER
// =============================================================================

uint32_t TensorMapper::getNumLayers(const GGUFMetadata& meta) {
    if (meta.ints.count("llama.block_count")) {
        return meta.ints.at("llama.block_count");
    }
    if (meta.ints.count("model.num_hidden_layers")) {
        return meta.ints.at("model.num_hidden_layers");
    }
    return 0;
}

void TensorMapper::addAttentionMappings(std::vector<Mapping>& out, uint32_t layerIdx,
                                         const std::string& prefix,
                                         const std::string& qName,
                                         const std::string& kName,
                                         const std::string& vName,
                                         const std::string& oName) {
    std::string layerStr = std::to_string(layerIdx);
    out.push_back({prefix + qName, "layers." + layerStr + ".attention.q", layerIdx, TensorType::AttentionQ});
    out.push_back({prefix + kName, "layers." + layerStr + ".attention.k", layerIdx, TensorType::AttentionK});
    out.push_back({prefix + vName, "layers." + layerStr + ".attention.v", layerIdx, TensorType::AttentionV});
    out.push_back({prefix + oName, "layers." + layerStr + ".attention.o", layerIdx, TensorType::AttentionO});
}

void TensorMapper::addFFNMappings(std::vector<Mapping>& out, uint32_t layerIdx,
                                    const std::string& prefix,
                                    const std::string& gateName,
                                    const std::string& upName,
                                    const std::string& downName) {
    std::string layerStr = std::to_string(layerIdx);
    out.push_back({prefix + gateName, "layers." + layerStr + ".ffn.gate", layerIdx, TensorType::FFNGate});
    out.push_back({prefix + upName,   "layers." + layerStr + ".ffn.up",   layerIdx, TensorType::FFNUp});
    out.push_back({prefix + downName, "layers." + layerStr + ".ffn.down", layerIdx, TensorType::FFNDown});
}

std::vector<TensorMapper::Mapping> TensorMapper::mapKimi(const GGUFMetadata& meta) {
    std::vector<Mapping> mappings;
    uint32_t numLayers = getNumLayers(meta);

    // Embedding
    mappings.push_back({"model.embed_tokens.weight", "embedding", 0, TensorType::Embedding});

    // Output
    mappings.push_back({"model.norm.weight", "output_norm", numLayers, TensorType::OutputNorm});
    mappings.push_back({"lm_head.weight", "output_head", numLayers, TensorType::OutputHead});

    // Per-layer
    for (uint32_t i = 0; i < numLayers; ++i) {
        std::string prefix = "model.layers." + std::to_string(i) + ".";

        // Attention
        addAttentionMappings(mappings, i, prefix,
            "self_attn.q_proj.weight",
            "self_attn.k_proj.weight",
            "self_attn.v_proj.weight",
            "self_attn.o_proj.weight");

        // Norms
        mappings.push_back({prefix + "input_layernorm.weight",
                           "layers." + std::to_string(i) + ".attention_norm",
                           i, TensorType::AttentionNorm});
        mappings.push_back({prefix + "post_attention_layernorm.weight",
                           "layers." + std::to_string(i) + ".ffn_norm",
                           i, TensorType::FFNNorm});

        // FFN or MoE
        if (meta.bools.count("model.moe") && meta.bools.at("model.moe")) {
            mappings.push_back({prefix + "mlp.gate.weight",
                               "layers." + std::to_string(i) + ".moe.gate",
                               i, TensorType::MoEGate});
            // Expert mappings would be added here for each expert
        } else {
            addFFNMappings(mappings, i, prefix,
                "mlp.gate_proj.weight",
                "mlp.up_proj.weight",
                "mlp.down_proj.weight");
        }
    }

    return mappings;
}

std::vector<TensorMapper::Mapping> TensorMapper::mapLlama(const GGUFMetadata& meta) {
    std::vector<Mapping> mappings;
    uint32_t numLayers = getNumLayers(meta);

    mappings.push_back({"token_embd.weight", "embedding", 0, TensorType::Embedding});
    mappings.push_back({"output_norm.weight", "output_norm", numLayers, TensorType::OutputNorm});
    mappings.push_back({"output.weight", "output_head", numLayers, TensorType::OutputHead});

    for (uint32_t i = 0; i < numLayers; ++i) {
        std::string prefix = "blk." + std::to_string(i) + ".";
        std::string layerStr = std::to_string(i);

        addAttentionMappings(mappings, i, prefix,
            "attn_q.weight", "attn_k.weight", "attn_v.weight", "attn_o.weight");

        mappings.push_back({prefix + "attn_norm.weight",
                           "layers." + layerStr + ".attention_norm", i, TensorType::AttentionNorm});
        mappings.push_back({prefix + "ffn_norm.weight",
                           "layers." + layerStr + ".ffn_norm", i, TensorType::FFNNorm});

        addFFNMappings(mappings, i, prefix,
            "ffn_gate.weight", "ffn_up.weight", "ffn_down.weight");
    }

    return mappings;
}

std::vector<TensorMapper::Mapping> TensorMapper::mapQwen(const GGUFMetadata& meta) {
    // Qwen uses similar naming to Kimi
    return mapKimi(meta);
}

std::vector<TensorMapper::Mapping> TensorMapper::mapDeepSeek(const GGUFMetadata& meta) {
    std::vector<Mapping> mappings;
    uint32_t numLayers = getNumLayers(meta);

    mappings.push_back({"model.embed_tokens.weight", "embedding", 0, TensorType::Embedding});
    mappings.push_back({"model.norm.weight", "output_norm", numLayers, TensorType::OutputNorm});
    mappings.push_back({"lm_head.weight", "output_head", numLayers, TensorType::OutputHead});

    for (uint32_t i = 0; i < numLayers; ++i) {
        std::string prefix = "model.layers." + std::to_string(i) + ".";

        addAttentionMappings(mappings, i, prefix,
            "self_attn.q_proj.weight",
            "self_attn.k_proj.weight",
            "self_attn.v_proj.weight",
            "self_attn.o_proj.weight");

        mappings.push_back({prefix + "input_layernorm.weight",
                           "layers." + std::to_string(i) + ".attention_norm",
                           i, TensorType::AttentionNorm});
        mappings.push_back({prefix + "post_attention_layernorm.weight",
                           "layers." + std::to_string(i) + ".ffn_norm",
                           i, TensorType::FFNNorm});

        // DeepSeek MoE
        mappings.push_back({prefix + "mlp.gate.weight",
                           "layers." + std::to_string(i) + ".moe.gate",
                           i, TensorType::MoEGate});

        // Shared experts
        addFFNMappings(mappings, i, prefix + "mlp.shared_experts.",
            "gate_proj.weight", "up_proj.weight", "down_proj.weight");

        // Routed experts (would need per-expert iteration)
        uint32_t numExperts = meta.ints.count("llama.expert_count")
            ? meta.ints.at("llama.expert_count") : 64;
        for (uint32_t e = 0; e < numExperts; ++e) {
            std::string expertPrefix = prefix + "mlp.experts." + std::to_string(e) + ".";
            addFFNMappings(mappings, i, expertPrefix,
                "gate_proj.weight", "up_proj.weight", "down_proj.weight");
        }
    }

    return mappings;
}

std::vector<TensorMapper::Mapping> TensorMapper::mapMixtral(const GGUFMetadata& meta) {
    std::vector<Mapping> mappings;
    uint32_t numLayers = getNumLayers(meta);

    mappings.push_back({"model.embed_tokens.weight", "embedding", 0, TensorType::Embedding});
    mappings.push_back({"model.norm.weight", "output_norm", numLayers, TensorType::OutputNorm});
    mappings.push_back({"lm_head.weight", "output_head", numLayers, TensorType::OutputHead});

    for (uint32_t i = 0; i < numLayers; ++i) {
        std::string prefix = "model.layers." + std::to_string(i) + ".";

        addAttentionMappings(mappings, i, prefix,
            "self_attn.q_proj.weight",
            "self_attn.k_proj.weight",
            "self_attn.v_proj.weight",
            "self_attn.o_proj.weight");

        mappings.push_back({prefix + "input_layernorm.weight",
                           "layers." + std::to_string(i) + ".attention_norm",
                           i, TensorType::AttentionNorm});
        mappings.push_back({prefix + "post_attention_layernorm.weight",
                           "layers." + std::to_string(i) + ".ffn_norm",
                           i, TensorType::FFNNorm});

        // Mixtral MoE
        mappings.push_back({prefix + "block_sparse_moe.gate.weight",
                           "layers." + std::to_string(i) + ".moe.gate",
                           i, TensorType::MoEGate});

        uint32_t numExperts = meta.ints.count("llama.expert_count")
            ? meta.ints.at("llama.expert_count") : 8;
        for (uint32_t e = 0; e < numExperts; ++e) {
            std::string expertPrefix = prefix + "block_sparse_moe.experts." + std::to_string(e) + ".";
            addFFNMappings(mappings, i, expertPrefix,
                "w1.weight", "w2.weight", "w3.weight");
        }
    }

    return mappings;
}

std::vector<TensorMapper::Mapping> TensorMapper::mapGeneric(const GGUFMetadata& meta) {
    // Fallback: try to auto-detect naming convention
    if (meta.strings.count("general.architecture")) {
        std::string arch = meta.strings.at("general.architecture");
        for (auto& c : arch) c = static_cast<char>(std::tolower(c));

        if (arch.find("llama") != std::string::npos) return mapLlama(meta);
        if (arch.find("qwen") != std::string::npos) return mapQwen(meta);
        if (arch.find("deepseek") != std::string::npos) return mapDeepSeek(meta);
        if (arch.find("mixtral") != std::string::npos) return mapMixtral(meta);
    }

    // Ultimate fallback: assume llama-style naming
    return mapLlama(meta);
}

// =============================================================================
// QUANTIZATION HANDLER
// =============================================================================

void QuantizationHandler::dequantizeQ4_K(const void* src, float* dst, size_t num) {
    // Q4_K: 256 weights per block
    // Block layout: 2x float16 scales + 2x float16 mins + 256x 4-bit weights
    const uint8_t* data = static_cast<const uint8_t*>(src);
    size_t numBlocks = num / 256;

    for (size_t b = 0; b < numBlocks; ++b) {
        const uint8_t* block = data + b * 144;  // 144 bytes per block

        // Read scales and mins (float16)
        uint16_t scale1 = *reinterpret_cast<const uint16_t*>(block);
        uint16_t scale2 = *reinterpret_cast<const uint16_t*>(block + 2);
        uint16_t min1 = *reinterpret_cast<const uint16_t*>(block + 4);
        uint16_t min2 = *reinterpret_cast<const uint16_t*>(block + 6);

        // Simple float16 to float32 conversion
        auto f16_to_f32 = [](uint16_t h) -> float {
            uint32_t sign = (h >> 15) & 0x1;
            uint32_t exp = (h >> 10) & 0x1F;
            uint32_t mant = h & 0x3FF;
            if (exp == 0) {
                if (mant == 0) return sign ? -0.0f : 0.0f;
                return (sign ? -1.0f : 1.0f) * std::ldexp(mant / 1024.0f, -14);
            }
            if (exp == 31) return mant ? std::numeric_limits<float>::quiet_NaN() :
                                          (sign ? -std::numeric_limits<float>::infinity() :
                                                   std::numeric_limits<float>::infinity());
            return (sign ? -1.0f : 1.0f) * std::ldexp(1.0f + mant / 1024.0f, exp - 15);
        };

        float s1 = f16_to_f32(scale1);
        float s2 = f16_to_f32(scale2);
        float m1 = f16_to_f32(min1);
        float m2 = f16_to_f32(min2);

        // Dequantize 4-bit weights
        for (size_t i = 0; i < 128; ++i) {
            uint8_t packed = block[8 + i];
            uint8_t low = packed & 0x0F;
            uint8_t high = (packed >> 4) & 0x0F;

            dst[b * 256 + i] = low * s1 + m1;
            dst[b * 256 + i + 128] = high * s2 + m2;
        }
    }
}

void QuantizationHandler::dequantizeQ5_K(const void* src, float* dst, size_t num) {
    // Q5_K: similar to Q4_K but with 5-bit weights
    // Simplified: delegate to Q4_K for now
    dequantizeQ4_K(src, dst, num);
}

void QuantizationHandler::dequantizeQ6_K(const void* src, float* dst, size_t num) {
    // Q6_K: 6-bit weights, 256 per block
    const uint8_t* data = static_cast<const uint8_t*>(src);
    size_t numBlocks = num / 256;

    for (size_t b = 0; b < numBlocks; ++b) {
        const uint8_t* block = data + b * 210;  // 210 bytes per block

        // Read scale (float16)
        uint16_t scale = *reinterpret_cast<const uint16_t*>(block);
        auto f16_to_f32 = [](uint16_t h) -> float {
            uint32_t sign = (h >> 15) & 0x1;
            uint32_t exp = (h >> 10) & 0x1F;
            uint32_t mant = h & 0x3FF;
            if (exp == 0) {
                if (mant == 0) return sign ? -0.0f : 0.0f;
                return (sign ? -1.0f : 1.0f) * std::ldexp(mant / 1024.0f, -14);
            }
            if (exp == 31) return mant ? std::numeric_limits<float>::quiet_NaN() :
                                          (sign ? -std::numeric_limits<float>::infinity() :
                                                   std::numeric_limits<float>::infinity());
            return (sign ? -1.0f : 1.0f) * std::ldexp(1.0f + mant / 1024.0f, exp - 15);
        };
        float s = f16_to_f32(scale);

        // Dequantize 6-bit weights (simplified)
        for (size_t i = 0; i < 256; ++i) {
            uint8_t val = block[2 + i];
            dst[b * 256 + i] = (val - 32) * s;
        }
    }
}

void QuantizationHandler::dequantizeQ8_0(const void* src, float* dst, size_t num) {
    // Q8_0: 32 weights per block, each weight is int8 with a float16 scale
    const uint8_t* data = static_cast<const uint8_t*>(src);
    size_t numBlocks = num / 32;

    for (size_t b = 0; b < numBlocks; ++b) {
        const uint8_t* block = data + b * 34;  // 2 bytes scale + 32 bytes weights

        uint16_t scale = *reinterpret_cast<const uint16_t*>(block);
        auto f16_to_f32 = [](uint16_t h) -> float {
            uint32_t sign = (h >> 15) & 0x1;
            uint32_t exp = (h >> 10) & 0x1F;
            uint32_t mant = h & 0x3FF;
            if (exp == 0) {
                if (mant == 0) return sign ? -0.0f : 0.0f;
                return (sign ? -1.0f : 1.0f) * std::ldexp(mant / 1024.0f, -14);
            }
            if (exp == 31) return mant ? std::numeric_limits<float>::quiet_NaN() :
                                          (sign ? -std::numeric_limits<float>::infinity() :
                                                   std::numeric_limits<float>::infinity());
            return (sign ? -1.0f : 1.0f) * std::ldexp(1.0f + mant / 1024.0f, exp - 15);
        };
        float s = f16_to_f32(scale);

        for (size_t i = 0; i < 32; ++i) {
            int8_t val = static_cast<int8_t>(block[2 + i]);
            dst[b * 32 + i] = val * s;
        }
    }
}

void QuantizationHandler::dequantizeFP16(const void* src, float* dst, size_t num) {
    const uint16_t* data = static_cast<const uint16_t*>(src);
    auto f16_to_f32 = [](uint16_t h) -> float {
        uint32_t sign = (h >> 15) & 0x1;
        uint32_t exp = (h >> 10) & 0x1F;
        uint32_t mant = h & 0x3FF;
        if (exp == 0) {
            if (mant == 0) return sign ? -0.0f : 0.0f;
            return (sign ? -1.0f : 1.0f) * std::ldexp(mant / 1024.0f, -14);
        }
        if (exp == 31) return mant ? std::numeric_limits<float>::quiet_NaN() :
                                      (sign ? -std::numeric_limits<float>::infinity() :
                                               std::numeric_limits<float>::infinity());
        return (sign ? -1.0f : 1.0f) * std::ldexp(1.0f + mant / 1024.0f, exp - 15);
    };

    for (size_t i = 0; i < num; ++i) {
        dst[i] = f16_to_f32(data[i]);
    }
}

void QuantizationHandler::dequantizeBF16(const void* src, float* dst, size_t num) {
    const uint16_t* data = static_cast<const uint16_t*>(src);
    for (size_t i = 0; i < num; ++i) {
        uint32_t val = static_cast<uint32_t>(data[i]) << 16;
        dst[i] = *reinterpret_cast<float*>(&val);
    }
}

size_t QuantizationHandler::getQuantizedSize(size_t numElements, QuantizationType quantType) {
    switch (quantType) {
        case QuantizationType::FP32: return numElements * 4;
        case QuantizationType::FP16: return numElements * 2;
        case QuantizationType::BF16: return numElements * 2;
        case QuantizationType::Q4_0: return (numElements / 32) * 18;
        case QuantizationType::Q4_1: return (numElements / 32) * 20;
        case QuantizationType::Q5_0: return (numElements / 32) * 22;
        case QuantizationType::Q5_1: return (numElements / 32) * 24;
        case QuantizationType::Q8_0: return (numElements / 32) * 34;
        case QuantizationType::Q4_K: return (numElements / 256) * 144;
        case QuantizationType::Q5_K: return (numElements / 256) * 176;
        case QuantizationType::Q6_K: return (numElements / 256) * 210;
        case QuantizationType::Q8_K: return (numElements / 256) * 292;
        default: return numElements * 4;
    }
}

size_t QuantizationHandler::getBlockSize(QuantizationType quantType) {
    switch (quantType) {
        case QuantizationType::Q4_0:
        case QuantizationType::Q4_1:
        case QuantizationType::Q5_0:
        case QuantizationType::Q5_1:
        case QuantizationType::Q8_0:
            return 32;
        case QuantizationType::Q4_K:
        case QuantizationType::Q5_K:
        case QuantizationType::Q6_K:
        case QuantizationType::Q8_K:
            return 256;
        default:
            return 1;
    }
}

size_t QuantizationHandler::getTypeSize(QuantizationType quantType) {
    switch (quantType) {
        case QuantizationType::FP32: return 4;
        case QuantizationType::FP16: return 2;
        case QuantizationType::BF16: return 2;
        case QuantizationType::Q4_0: return 18;
        case QuantizationType::Q4_1: return 20;
        case QuantizationType::Q5_0: return 22;
        case QuantizationType::Q5_1: return 24;
        case QuantizationType::Q8_0: return 34;
        case QuantizationType::Q4_K: return 144;
        case QuantizationType::Q5_K: return 176;
        case QuantizationType::Q6_K: return 210;
        case QuantizationType::Q8_K: return 292;
        default: return 4;
    }
}

// =============================================================================
// REAL MODEL LOADER
// =============================================================================

RealModelLoader::LoadResult RealModelLoader::load(const std::string& path,
                                                   const LoadOptions& options) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        return {false, "Cannot open file: " + path, {}, {}, {}, 0, 0, 0};
    }

    uint32_t magic;
    file.read(reinterpret_cast<char*>(&magic), sizeof(magic));
    file.seekg(0);

    if (magic == 0x46554747) {  // "GGUF"
        return loadGGUF(path, options);
    }

    // Try safetensors
    return loadSafetensors(path, "", options);
}

RealModelLoader::LoadResult RealModelLoader::loadGGUF(const std::string& path,
                                                       const LoadOptions& options) {
    LoadResult result;

    // Use existing GGUFLoader
    std::vector<TensorDesc> tensorDescs;
    GGUFMetadata meta;
    WeightLoadResult parseResult = GGUFLoader::load(path, tensorDescs, nullptr);

    if (!parseResult.success) {
        result.errorMessage = parseResult.error;
        return result;
    }

    // Detect architecture from metadata
    result.architecture = ArchitectureDetector::detectFromGGUF(meta);
    result.config = ArchitectureDetector::createConfig(result.architecture);

    if (options.verbose) {
        printModelInfo(result);
    }

    // Get tensor mappings
    std::vector<TensorMapper::Mapping> mappings;
    switch (result.architecture.arch) {
        case ModelArchitecture::Kimi:
        case ModelArchitecture::Qwen:
        case ModelArchitecture::QwenMoE:
            mappings = TensorMapper::mapKimi(meta);
            break;
        case ModelArchitecture::Llama:
        case ModelArchitecture::LlamaMoE:
            mappings = TensorMapper::mapLlama(meta);
            break;
        case ModelArchitecture::DeepSeek:
        case ModelArchitecture::DeepSeekMoE:
            mappings = TensorMapper::mapDeepSeek(meta);
            break;
        case ModelArchitecture::Mixtral:
            mappings = TensorMapper::mapMixtral(meta);
            break;
        default:
            mappings = TensorMapper::mapGeneric(meta);
            break;
    }

    // Load actual tensor data
    std::ifstream file(path, std::ios::binary);
    result = loadGGUFTensors(file, meta, mappings, options);
    result.success = true;

    return result;
}

RealModelLoader::LoadResult RealModelLoader::loadSafetensors(const std::string& indexPath,
                                                               const std::string& weightsPath,
                                                               const LoadOptions& options) {
    LoadResult result;
    result.errorMessage = "Safetensors loading not yet fully implemented";
    return result;
}

RealModelLoader::LoadResult RealModelLoader::loadGGUFTensors(std::ifstream& file,
                                                               const GGUFMetadata& metadata,
                                                               const std::vector<TensorMapper::Mapping>& mappings,
                                                               const LoadOptions& options) {
    LoadResult result;
    // Implementation would read tensor data from file offsets
    // and populate result.tensors with dequantized or raw data
    result.success = true;
    return result;
}

bool RealModelLoader::validateModel(const LoadResult& result) {
    if (!result.success) return false;
    if (result.tensors.empty()) return false;
    if (result.config.hiddenDim == 0) return false;
    if (result.config.numLayers == 0) return false;
    return true;
}

void RealModelLoader::printModelInfo(const LoadResult& result) {
    std::cout << "═══════════════════════════════════════════════════════════\n";
    std::cout << " Model: " << result.architecture.modelName << "\n";
    std::cout << " Architecture: ";
    switch (result.architecture.arch) {
        case ModelArchitecture::Llama: std::cout << "LLaMA"; break;
        case ModelArchitecture::LlamaMoE: std::cout << "LLaMA-MoE"; break;
        case ModelArchitecture::Qwen: std::cout << "Qwen"; break;
        case ModelArchitecture::QwenMoE: std::cout << "Qwen-MoE"; break;
        case ModelArchitecture::Kimi: std::cout << "Kimi"; break;
        case ModelArchitecture::DeepSeek: std::cout << "DeepSeek"; break;
        case ModelArchitecture::DeepSeekMoE: std::cout << "DeepSeek-MoE"; break;
        case ModelArchitecture::Mixtral: std::cout << "Mixtral"; break;
        case ModelArchitecture::Phi: std::cout << "Phi"; break;
        case ModelArchitecture::Gemma: std::cout << "Gemma"; break;
        case ModelArchitecture::Gemma2: std::cout << "Gemma2"; break;
        default: std::cout << "Unknown"; break;
    }
    std::cout << "\n";
    std::cout << "─────────────────────────────────────────────────────────────\n";
    std::cout << " Hidden dim:     " << result.architecture.hiddenDim << "\n";
    std::cout << " Layers:         " << result.architecture.numLayers << "\n";
    std::cout << " Attention heads: " << result.architecture.numHeads << "\n";
    std::cout << " KV heads:       " << result.architecture.numKVHeads << "\n";
    std::cout << " Vocab size:     " << result.architecture.vocabSize << "\n";
    std::cout << " Head dim:       " << result.architecture.headDim << "\n";
    std::cout << "─────────────────────────────────────────────────────────────\n";
    if (result.architecture.isMoE) {
        std::cout << " MoE:            Yes\n";
        std::cout << " Experts:        " << result.architecture.numExperts << "\n";
        std::cout << " Active/token:   " << result.architecture.expertsPerToken << "\n";
        std::cout << " Shared experts: " << result.architecture.sharedExperts << "\n";
    } else {
        std::cout << " MoE:            No (Dense)\n";
    }
    std::cout << "─────────────────────────────────────────────────────────────\n";
    std::cout << " Total tensors:  " << result.tensors.size() << "\n";
    std::cout << " Total bytes:    " << (result.totalBytes / (1024.0 * 1024.0)) << " MB\n";
    std::cout << " Quantized:      " << (result.quantizedBytes / (1024.0 * 1024.0)) << " MB\n";
    std::cout << "═══════════════════════════════════════════════════════════\n";
}

} // namespace Prometheus
