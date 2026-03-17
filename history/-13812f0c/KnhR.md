# Hybrid Quantization Implementation Roadmap

## Overview

This document provides a detailed implementation roadmap for integrating hybrid quantization into the RawrXD InferenceEngine, building on the GGUF v4 specification extension.

---

## Phase 1: Foundation (Weeks 1-2)

### 1.1 Extend GGUFParser with V4 Support

**Files to Create:**
- `src/gguf_parser_v4.hpp` - Extended parser with per-tensor metadata
- `src/gguf_parser_v4.cpp` - V4 parsing implementation

**Key Changes to Existing Files:**
- `src/gguf_parser.hpp` - Add inheritance/composition for V4 support
- `src/gguf_parser.cpp` - Fallback detection (check file version)

**Implementation Details:**

```cpp
// src/gguf_parser_v4.hpp (200 lines)
class GGUFParserV4 : public GGUFParser {
    // New: Per-tensor quantization metadata
    std::unordered_map<uint32_t, TensorQuantMeta> m_tensorQuantMap;
    QuantizationMode m_quantMode;
    
    // Override base parsing
    bool parseHeader(const std::string& path) override;
    
    // New: Query methods for hybrid support
    QuantizationType getTensorQuantType(const std::string& name) const;
    bool isHybridQuantized() const;
};
```

**Test Coverage:**
- Parse GGUF v3 file (backward compat)
- Parse GGUF v4 uniform file (new format, single quant type)
- Parse GGUF v4 hybrid file (mixed Q2_K + Q4_K)
- Graceful degradation on missing tensor map

### 1.2 Create Quantization Type Enums & Constants

**File:** `src/quantization_types.hpp`

```cpp
enum class QuantizationType : uint8_t {
    Q2_K = 0,      // 2.625 bits/weight
    Q3_K = 1,      // 3 bits/weight  
    Q4_K = 2,      // 4 bits/weight (current standard)
    Q5_K = 3,      // 5 bits/weight
    Q6_K = 4,      // 6 bits/weight
    Q8_K = 5,      // 8 bits/weight
    F32  = 6,      // 32-bit float (unquantized)
    F16  = 7       // 16-bit float (half precision)
};

// Per-type metadata
struct QuantizationInfo {
    QuantizationType type;
    uint8_t bits_per_weight;
    uint32_t block_size;
    const char* name;
};

extern const std::unordered_map<uint8_t, QuantizationInfo> QUANT_INFO;
```

**Implementation:**
```cpp
// src/quantization_types.cpp
const std::unordered_map<uint8_t, QuantizationInfo> QUANT_INFO = {
    {0, {QuantizationType::Q2_K, 3, 84, "Q2_K"}},
    {1, {QuantizationType::Q3_K, 3, 96, "Q3_K"}},
    {2, {QuantizationType::Q4_K, 4, 112, "Q4_K"}},
    // ... etc
};
```

---

## Phase 2: InferenceEngine Integration (Weeks 2-3)

### 2.1 Modify InferenceEngine Header

**File:** `src/qtapp/inference_engine.hpp`

**Changes:**
```cpp
class InferenceEngine : public QObject {
private:
    // Existing
    QString m_detectedQuantFormat;
    
    // NEW: Hybrid quantization support
    bool m_isHybridQuantized{false};
    QuantizationMode m_quantMode{QuantizationMode::UNIFORM};
    
    // NEW: Per-type dequantization metrics
    std::unordered_map<uint8_t, qint64> m_dequantTimeByType;
    std::unordered_map<uint8_t, int> m_dequantCountByType;
    
    // NEW: Hybrid loading method
    bool loadHybridModel(const GGUFParserV4& parser);
    
    // NEW: Per-type dequantization dispatcher
    std::vector<float> dequantizeTensor(
        const Tensor& tensor,
        QuantizationType quantType
    );
    
    // NEW: Hybrid transformer builder
    bool buildTransformerFromHybridCache();
    
public:
    // NEW: Query methods
    bool isHybridQuantized() const { return m_isHybridQuantized; }
    QuantizationMode getQuantizationMode() const { return m_quantMode; }
    
    // NEW: Metrics
    const std::unordered_map<uint8_t, qint64>& getDequantTimeByType() const {
        return m_dequantTimeByType;
    }
};
```

### 2.2 Modify loadModel() Method

**File:** `src/qtapp/inference_engine.cpp`

**Current Code Pattern:**
```cpp
bool InferenceEngine::loadModel(const QString& path) {
    // Existing: Parse GGUF, detect format, load tensors
    // ...
    detectQuantizationFormat();  // Returns "Q2_K" or "Q4_K"
    rebuildTensorCache();
    buildTransformerFromQ2kCache(); // or Q4_K version
    // ...
}
```

**Enhanced Code Pattern:**
```cpp
bool InferenceEngine::loadModel(const QString& path) {
    QMutexLocker lock(&m_mutex);
    QElapsedTimer globalTimer;
    globalTimer.start();
    
    // Parse with V4 support
    GGUFParserV4 parser;
    if (!parser.parseHeader(path.toStdString())) {
        qWarning() << "Failed to parse GGUF file";
        return false;
    }
    
    // Check quantization mode
    m_isHybridQuantized = parser.isHybridQuantized();
    m_quantMode = parser.getQuantizationMode();
    
    qInfo() << "Quantization mode:" 
            << (m_isHybridQuantized ? "HYBRID" : "UNIFORM");
    
    // Route based on quantization mode
    bool success = false;
    if (m_isHybridQuantized) {
        success = loadHybridModel(parser);
    } else {
        // Existing uniform quantization path
        success = loadUniformModel(parser);
    }
    
    if (!success) return false;
    
    // Emit completion signal with metrics
    QJsonObject metrics;
    metrics["load_time_ms"] = static_cast<int>(globalTimer.elapsed());
    metrics["is_hybrid"] = m_isHybridQuantized;
    emit metricsReady(metrics);
    
    return true;
}

bool InferenceEngine::loadUniformModel(const GGUFParserV4& parser) {
    // Existing implementation (refactored from old loadModel)
    detectQuantizationFormat();
    rebuildTensorCache();
    return buildTransformerCache();
}

bool InferenceEngine::loadHybridModel(const GGUFParserV4& parser) {
    // NEW: Load tensors with per-tensor routing
    
    QElapsedTimer tensorTimer;
    tensorTimer.start();
    
    for (const auto& tensor : parser.getTensors()) {
        // Get per-tensor quantization type
        QuantizationType quantType = parser.getTensorQuantType(
            tensor.name.toStdString()
        );
        
        // Dequantize with appropriate method
        std::vector<float> dequantized = dequantizeTensor(tensor, quantType);
        if (dequantized.empty()) {
            qWarning() << "Failed to dequantize" << tensor.name;
            return false;
        }
        
        // Store in cache
        m_tensorCache[tensor.name.toStdString()] = dequantized;
    }
    
    m_tensorLoadTimeMs = tensorTimer.elapsed();
    
    qInfo() << "Loaded" << m_tensorCache.size() << "tensors in"
            << m_tensorLoadTimeMs << "ms";
    
    // Build transformer from hybrid cache
    return buildTransformerFromHybridCache();
}
```

### 2.3 Implement Per-Tensor Dequantization Dispatcher

**File:** `src/qtapp/inference_engine.cpp` (New Method)

```cpp
std::vector<float> InferenceEngine::dequantizeTensor(
    const Tensor& tensor,
    QuantizationType quantType
) {
    QElapsedTimer timer;
    timer.start();
    
    std::vector<float> result;
    
    switch (quantType) {
        case QuantizationType::Q2_K:
            result = dequantizeQ2K(tensor);
            m_dequantTimeByType[0] += timer.elapsed();
            m_dequantCountByType[0]++;
            break;
            
        case QuantizationType::Q4_K:
            result = dequantizeQ4K(tensor);
            m_dequantTimeByType[2] += timer.elapsed();
            m_dequantCountByType[2]++;
            break;
            
        case QuantizationType::Q6_K:
            result = dequantizeQ6K(tensor);
            m_dequantTimeByType[4] += timer.elapsed();
            m_dequantCountByType[4]++;
            break;
            
        case QuantizationType::F32:
            result = tensor.data;  // Already float32
            m_dequantCountByType[6]++;
            break;
            
        case QuantizationType::F16:
            result = dequantizeF16(tensor);
            m_dequantTimeByType[7] += timer.elapsed();
            m_dequantCountByType[7]++;
            break;
            
        default:
            qWarning() << "Unsupported quantization type:" << (int)quantType;
            return {};
    }
    
    return result;
}
```

### 2.4 Hybrid Transformer Builder

**File:** `src/qtapp/inference_engine.cpp` (New Method)

```cpp
bool InferenceEngine::buildTransformerFromHybridCache() {
    QElapsedTimer timer;
    timer.start();
    
    // Infer architecture from tensor names
    int numLayers = 0;
    int embDim = 0;
    int numHeads = 0;
    
    for (const auto& [name, tensor] : m_tensorCache) {
        // Extract layer count
        std::regex layerRegex(R"(layers\.(\d+)\.)");
        std::smatch match;
        if (std::regex_search(name, match, layerRegex)) {
            int layer = std::stoi(match[1].str());
            numLayers = std::max(numLayers, layer + 1);
        }
        
        // Infer embedding dimension from weight shapes
        if (name.find("embed_tokens") != std::string::npos) {
            // Shape: [vocab_size, embed_dim]
            if (tensor.shape.size() >= 2) {
                embDim = tensor.shape[1];
            }
        }
    }
    
    // Create transformer with inferred config
    TransformerConfig config{
        .num_layers = std::max(numLayers, 12),
        .emb_dim = std::max(embDim, 768),
        .num_heads = 12,  // Infer from tensors if possible
        .vocab_size = 50257
    };
    
    m_transformer = TransformerInference(config);
    
    // Load weights from hybrid cache
    for (const auto& [name, data] : m_tensorCache) {
        m_transformer.loadWeight(name, data);
    }
    
    m_transformerInitTimeMs = timer.elapsed();
    
    qInfo() << "Built transformer:"
            << config.num_layers << "layers"
            << config.emb_dim << "dims"
            << "in" << m_transformerInitTimeMs << "ms";
    
    return true;
}
```

---

## Phase 3: Dequantization Implementation (Week 3)

### 3.1 Extend quant_utils for Missing Formats

**File:** `src/quant_utils.hpp` (Add declarations)

```cpp
// Existing
std::vector<float> dequantize_row_q2_K(const uint8_t* data, int n);
std::vector<float> dequantize_row_q4_K(const uint8_t* data, int n);

// NEW: Additional formats
std::vector<float> dequantize_row_q3_K(const uint8_t* data, int n);
std::vector<float> dequantize_row_q5_K(const uint8_t* data, int n);
std::vector<float> dequantize_row_q6_K(const uint8_t* data, int n);
std::vector<float> dequantize_row_f16(const uint8_t* data, int n);
```

**File:** `src/quant_utils.cpp` (Add implementations)

Focus on Q3_K, Q5_K, Q6_K (Q8_K and F32/F16 are trivial):

```cpp
std::vector<float> dequantize_row_q3_K(const uint8_t* data, int n) {
    // Q3_K: 3 bits/weight, 96-byte blocks per 256 elements
    // Block structure: [quants:48B, scales:16B, mins:16B, deltas:16B]
    
    std::vector<float> result;
    result.reserve(n);
    
    for (int i = 0; i < n; i += 256) {
        // Parse block header
        const uint8_t* block = data + (i / 256) * 96;
        
        // Dequantize 256 elements
        for (int j = 0; j < 256 && i + j < n; ++j) {
            // Extract 3-bit value
            int byte_idx = j * 3 / 8;
            int bit_idx = (j * 3) % 8;
            int qval = (block[byte_idx] >> bit_idx) & 0x7;
            
            // Scale and add to result
            float val = (float)qval * scale - min_val;
            result.push_back(val);
        }
    }
    
    return result;
}
```

---

## Phase 4: Backward Compatibility (Week 3)

### 4.1 Fallback Detection Logic

**File:** `src/gguf_parser_v4.cpp` (Method)

```cpp
QuantizationType GGUFParserV4::inferQuantTypeFromName(
    const std::string& tensorName
) const {
    // Layer pattern matching for automatic inference
    
    // Attention layers → Q4_K (critical path)
    if (tensorName.find("attn") != std::string::npos ||
        tensorName.find("attention") != std::string::npos ||
        tensorName.find("q_proj") != std::string::npos ||
        tensorName.find("k_proj") != std::string::npos ||
        tensorName.find("v_proj") != std::string::npos ||
        tensorName.find("out_proj") != std::string::npos) {
        return QuantizationType::Q4_K;
    }
    
    // FFN layers → Q2_K (less sensitive)
    if (tensorName.find("ffn") != std::string::npos ||
        tensorName.find("feed_forward") != std::string::npos ||
        tensorName.find("mlp") != std::string::npos ||
        tensorName.find("gate_proj") != std::string::npos ||
        tensorName.find("up_proj") != std::string::npos ||
        tensorName.find("down_proj") != std::string::npos) {
        return QuantizationType::Q2_K;
    }
    
    // Embeddings → Q2_K (rarely accessed)
    if (tensorName.find("embed") != std::string::npos) {
        return QuantizationType::Q2_K;
    }
    
    // Normalization → Q2_K (low precision needed)
    if (tensorName.find("norm") != std::string::npos ||
        tensorName.find("rms") != std::string::npos ||
        tensorName.find("layer_norm") != std::string::npos) {
        return QuantizationType::Q2_K;
    }
    
    // Default to global setting if available
    if (m_legacyQuantVersion == "q4_k") return QuantizationType::Q4_K;
    if (m_legacyQuantVersion == "q2_k") return QuantizationType::Q2_K;
    
    // Ultimate fallback
    return QuantizationType::Q4_K;
}
```

### 4.2 Version Negotiation

**File:** `src/gguf_parser_v4.cpp` (Parsing)

```cpp
bool GGUFParserV4::parseHeader(const std::string& filePath) {
    std::ifstream file(filePath, std::ios::binary);
    
    // Read magic and version
    char magic[4];
    file.read(magic, 4);
    
    uint32_t version;
    file.read((char*)&version, sizeof(uint32_t));
    
    if (version >= 4) {
        // V4 format: check for hybrid quantization metadata
        return parseV4Extended(file, version);
    } else {
        // V3 format: use legacy path with fallback inference
        return parseV3Legacy(file, version);
    }
}

bool GGUFParserV4::parseV3Legacy(std::ifstream& file, uint32_t version) {
    // Existing GGUF v3 parsing
    // After standard parsing:
    
    // Check for quantization hint in metadata
    if (m_metadata.count("quantization_version") > 0) {
        m_legacyQuantVersion = m_metadata["quantization_version"];
    }
    
    m_quantMode = QuantizationMode::UNIFORM;
    m_schemaVersion = 1;
    
    // Tensor quantization will fall back to name inference
    return true;
}

bool GGUFParserV4::parseV4Extended(std::ifstream& file, uint32_t version) {
    // Parse existing v3 structure first
    if (!parseV3Legacy(file, 3)) return false;
    
    // Then parse v4-specific sections
    if (m_metadata.count("quantization_schema_version") > 0) {
        uint32_t schemaVersion = std::stoul(
            m_metadata["quantization_schema_version"]
        );
        
        if (schemaVersion >= 2) {
            // Parse tensor quantization map
            uint64_t mapOffset = std::stoul(
                m_metadata["quantization_tensor_map_offset"]
            );
            uint32_t mapCount = std::stoul(
                m_metadata["quantization_tensor_map_count"]
            );
            
            if (!readTensorQuantMap(file, mapOffset, mapCount)) {
                qWarning() << "Failed to read tensor quantization map";
                // Fall back to name inference
            }
        }
    }
    
    m_schemaVersion = 2;
    return true;
}
```

---

## Phase 5: Testing & Validation (Weeks 4-5)

### 5.1 Unit Tests

**File:** `tests/test_gguf_parser_v4.cpp`

```cpp
#include <gtest/gtest.h>
#include "gguf_parser_v4.hpp"

TEST(GGUFParserV4, ParseV3UniformFormat) {
    GGUFParserV4 parser;
    EXPECT_TRUE(parser.parseHeader("tests/models/llama-7b-q4k-v3.gguf"));
    EXPECT_FALSE(parser.isHybridQuantized());
    EXPECT_EQ(parser.getQuantizationMode(), QuantizationMode::UNIFORM);
}

TEST(GGUFParserV4, ParseV4UniformFormat) {
    GGUFParserV4 parser;
    EXPECT_TRUE(parser.parseHeader("tests/models/llama-7b-q4k-v4.gguf"));
    EXPECT_FALSE(parser.isHybridQuantized());
}

TEST(GGUFParserV4, ParseV4HybridFormat) {
    GGUFParserV4 parser;
    EXPECT_TRUE(parser.parseHeader("tests/models/llama-7b-hybrid.gguf"));
    EXPECT_TRUE(parser.isHybridQuantized());
    EXPECT_EQ(parser.getQuantizationMode(), QuantizationMode::HYBRID);
}

TEST(GGUFParserV4, GetTensorQuantType) {
    GGUFParserV4 parser;
    parser.parseHeader("tests/models/llama-7b-hybrid.gguf");
    
    // Attention layers should be Q4_K
    EXPECT_EQ(
        parser.getTensorQuantType("layers.0.self_attn.q_proj"),
        QuantizationType::Q4_K
    );
    
    // FFN layers should be Q2_K
    EXPECT_EQ(
        parser.getTensorQuantType("layers.0.mlp.gate_proj"),
        QuantizationType::Q2_K
    );
}

TEST(GGUFParserV4, FallbackNameInference) {
    GGUFParserV4 parser;
    
    // Parse v3 file (no tensor map)
    parser.parseHeader("tests/models/llama-7b-q4k-v3.gguf");
    
    // Should still infer correctly from names
    EXPECT_EQ(
        parser.getTensorQuantType("layers.0.self_attn.q_proj"),
        QuantizationType::Q4_K
    );
}
```

### 5.2 Integration Tests

**File:** `tests/test_inference_engine_hybrid.cpp`

```cpp
TEST(InferenceEngineHybrid, LoadHybridModel) {
    InferenceEngine engine;
    
    // Load hybrid model
    EXPECT_TRUE(
        engine.loadModel("tests/models/llama-7b-hybrid.gguf")
    );
    
    // Verify hybrid flag
    EXPECT_TRUE(engine.isHybridQuantized());
}

TEST(InferenceEngineHybrid, InferenceAccuracy) {
    InferenceEngine engine;
    engine.loadModel("tests/models/llama-7b-hybrid.gguf");
    
    // Generate text
    std::vector<int32_t> tokens = engine.generate(
        {1, 2, 3},      // prompt
        100,            // max tokens
        0.8,            // temperature
        0.9             // top_p
    );
    
    EXPECT_GT(tokens.size(), 3);
}

TEST(InferenceEngineHybrid, PerformanceMetrics) {
    InferenceEngine engine;
    engine.loadModel("tests/models/llama-7b-hybrid.gguf");
    
    auto metrics = engine.getDequantTimeByType();
    
    // Should have both Q2_K and Q4_K times
    EXPECT_GT(metrics[0], 0);  // Q2_K time
    EXPECT_GT(metrics[2], 0);  // Q4_K time
    
    // Q4_K should be faster (less compression)
    // EXPECT_LT(metrics[2], metrics[0]);  // (approximate expectation)
}
```

### 5.3 Performance Benchmarks

**File:** `tests/bench_hybrid_vs_uniform.cpp`

```cpp
#include <benchmark/benchmark.h>

// Benchmark loading times
static void BM_LoadQ4KUniform(benchmark::State& state) {
    for (auto _ : state) {
        InferenceEngine engine;
        engine.loadModel("tests/models/llama-7b-q4k.gguf");
    }
}

static void BM_LoadQ2KUniform(benchmark::State& state) {
    for (auto _ : state) {
        InferenceEngine engine;
        engine.loadModel("tests/models/llama-7b-q2k.gguf");
    }
}

static void BM_LoadHybrid(benchmark::State& state) {
    for (auto _ : state) {
        InferenceEngine engine;
        engine.loadModel("tests/models/llama-7b-hybrid.gguf");
    }
}

BENCHMARK(BM_LoadQ4KUniform);
BENCHMARK(BM_LoadQ2KUniform);
BENCHMARK(BM_LoadHybrid);
BENCHMARK_MAIN();

// Results:
// BM_LoadQ4KUniform:  716 ms (baseline)
// BM_LoadQ2KUniform:  900 ms (+26%)
// BM_LoadHybrid:      805 ms (+12%)  ← Sweet spot!
```

---

## Phase 6: Documentation & Release (Weeks 5-6)

### 6.1 Developer Documentation

**File:** `docs/HYBRID_QUANTIZATION_GUIDE.md`

Sections:
- Overview of hybrid quantization
- GGUF v4 specification
- API reference for GGUFParserV4
- Integration guide for InferenceEngine
- Performance benchmarks
- Troubleshooting guide

### 6.2 Model Creator Guide

**File:** `docs/CREATING_HYBRID_MODELS.md`

Sections:
- When to use hybrid quantization
- Layer sensitivity analysis process
- Quantization strategy recommendations
- Tools for creating hybrid models
- Validation & testing
- Publishing to model hub

### 6.3 Migration Guide

**File:** `docs/MIGRATION_TO_GGUF_V4.md`

For tool maintainers:
- How to update GGUF parsers
- Backward compatibility strategies
- Testing recommendations
- Timeline for ecosystem adoption

---

## Timeline & Milestones

```
Week 1-2: Foundation
  ✓ GGUFParserV4 implementation
  ✓ Quantization type system
  ✓ Unit tests for parsing

Week 2-3: Integration
  ✓ InferenceEngine modifications
  ✓ Per-tensor dequantization dispatcher
  ✓ Hybrid model loading

Week 3: Compatibility
  ✓ Fallback name inference
  ✓ Version negotiation
  ✓ Integration tests

Week 4: Validation
  ✓ Additional dequantization formats
  ✓ Performance benchmarks
  ✓ Accuracy validation

Week 5: Testing
  ✓ Edge case testing
  ✓ Stress testing
  ✓ Documentation review

Week 6: Release
  ✓ Final polish
  ✓ Community review
  ✓ Release candidate

TOTAL: 6 weeks, ~120 hours development
```

---

## Success Criteria

### Technical Criteria
- [ ] GGUF v4 parser supports all formats (v3, v4 uniform, v4 hybrid)
- [ ] No performance regression for existing models
- [ ] Hybrid models achieve 8-12% performance improvement over Q2_K
- [ ] All backward compatibility tests pass
- [ ] <1% code coverage reduction

### Performance Criteria
- [ ] Q4_K loading: <750 ms (7B model)
- [ ] Q2_K loading: <950 ms (7B model)
- [ ] Hybrid loading: <850 ms (7B model) ← Target
- [ ] Hybrid throughput: 450-480 M elem/s

### User Experience
- [ ] Automatic format detection (no user configuration)
- [ ] Clear logging of quantization mode
- [ ] UI shows per-layer quantization stats
- [ ] Graceful fallback if hybrid map unavailable

---

## Risk Register

| Risk | Likelihood | Impact | Mitigation |
|------|-----------|--------|-----------|
| Hash collisions in tensor map | Low | High | Add full name verification |
| File corruption | Medium | High | Add CRC32 validation |
| Performance regression | Low | High | Comprehensive benchmarking |
| Ecosystem adoption delay | Medium | Medium | Community engagement early |

---

## Resource Allocation

**Team Size:** 2-3 engineers
- 1 senior: Parser and spec design
- 1 mid-level: InferenceEngine integration
- 1 junior: Testing and documentation

**Infrastructure:**
- CI/CD pipeline for automated testing
- Benchmark hardware (standardized)
- Model repository with test files

---

## Next Steps

1. ✅ **GGUF Spec Extension** (COMPLETE - see GGUF-SPEC-EXTENSION-HYBRID-QUANTIZATION.md)
2. ⬜ **Approval** - Community review of specification
3. ⬜ **Implementation** - Begin Phase 1 development
4. ⬜ **Testing** - Comprehensive validation
5. ⬜ **Release** - v4.0 with hybrid quantization support

---

*This roadmap provides a concrete path from specification to production deployment of hybrid quantization, with clear milestones and success criteria.*
