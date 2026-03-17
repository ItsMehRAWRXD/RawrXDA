# Backward Compatibility Matrix & Testing Scenarios

## Problem Context

**User Concern:** "Current GGUF spec assumes uniform quantization. Need to add metadata flags per tensor while maintaining backward compatibility."

**Solution:** GGUF v4 extension with per-tensor metadata + fallback mechanisms.

This document defines **exhaustive backward compatibility tests** to ensure safe adoption.

---

## 1. Compatibility Matrix Overview

```
┌──────────────────┬────────────────┬────────────────┬────────────────┐
│ Reader / Writer  │ GGUF v3        │ GGUF v4        │ GGUF v4        │
│                  │ Uniform        │ Uniform        │ Hybrid         │
├──────────────────┼────────────────┼────────────────┼────────────────┤
│ v3 Reader        │ ✅ Full        │ ✅ Forward*    │ ⚠️  Partial    │
│                  │ Support        │ Compat         │ (Name Only)    │
├──────────────────┼────────────────┼────────────────┼────────────────┤
│ v4 Reader        │ ✅ Full        │ ✅ Full        │ ✅ Full        │
│                  │ Compat         │ Support        │ Support        │
│                  │ (Fallback)     │                │                │
├──────────────────┼────────────────┼────────────────┼────────────────┤
│ v3 Writer        │ ✅ Native      │ ❌ Cannot      │ ❌ Cannot      │
│                  │                │ produce        │ produce        │
├──────────────────┼────────────────┼────────────────┼────────────────┤
│ v4 Writer        │ ✅ Native      │ ✅ Native      │ ✅ Native      │
│                  │ (via flag)     │                │                │
└──────────────────┴────────────────┴────────────────┴────────────────┘

Legend:
✅ Full Support:   Works perfectly, no data loss
✅ Forward Compat: Works, but ignores new features (safe)
⚠️  Partial:       Works with fallback, may load wrong format
❌ Cannot:        Not supported (expected, don't attempt)
```

---

## 2. Critical Backward Compatibility Requirements

### 2.1 GGUF v3 Readers Loading v4 Files

**Requirement:** v3 readers must NOT crash on v4 files

**Implementation Strategy:**
1. **Version Detection**: Check file version field early
2. **Safe Degradation**: Skip unknown metadata keys
3. **Name-Based Fallback**: Use tensor names to infer quantization type
4. **Clear Warnings**: Log obvious messages about version mismatch

**Code Pattern:**
```cpp
bool GGUFParserV3::parseHeader(const std::string& filePath) {
    // ... existing parsing ...
    
    uint32_t version;
    file.read((char*)&version, sizeof(uint32_t));
    
    if (version > 3) {
        qWarning() << "GGUF file version" << version 
                   << "is newer than this parser (v3)";
        qWarning() << "Consider upgrading to GGUF v4 tools";
        
        // Continue parsing as v3 (ignore new sections)
        // Tensor quantization will be inferred from names
    }
    
    // ... rest of v3 parsing ...
}
```

### 2.2 GGUF v4 Readers Loading v3 Files

**Requirement:** v4 readers must handle v3 files seamlessly

**Implementation Strategy:**
1. **Version Check**: Detect v3 format
2. **Fallback Parsing**: Use legacy v3 parser path
3. **Uniform Mode**: Treat all tensors as same quantization format
4. **No Errors**: Load successfully with v3 semantics

**Code Pattern:**
```cpp
bool GGUFParserV4::parseHeader(const std::string& filePath) {
    uint32_t version;
    // ... read version ...
    
    if (version < 4) {
        // Use v3 legacy parser
        return parseV3Legacy(file, version);
    } else {
        // Use v4 extended parser
        return parseV4Extended(file, version);
    }
}

bool GGUFParserV4::parseV3Legacy(std::ifstream& file, uint32_t version) {
    // Standard GGUF v3 parsing
    // Sets m_quantMode = QuantizationMode::UNIFORM
    // Clears m_tensorQuantMap
    return true;
}
```

### 2.3 Tensor Name Inference for Hybrid Models

**Fallback Strategy:** When tensor quantization map unavailable, infer from names

```cpp
QuantizationType inferQuantTypeFromName(const std::string& tensorName) {
    // Attention layers → Q4_K (critical path, needs precision)
    if (tensorName.find("attn") != std::string::npos ||
        tensorName.find("attention") != std::string::npos) {
        return QuantizationType::Q4_K;
    }
    
    // FFN layers → Q2_K (can tolerate compression)
    if (tensorName.find("ffn") != std::string::npos ||
        tensorName.find("feed_forward") != std::string::npos ||
        tensorName.find("mlp") != std::string::npos) {
        return QuantizationType::Q2_K;
    }
    
    // Embeddings → Q2_K (rarely accessed during inference)
    if (tensorName.find("embed") != std::string::npos) {
        return QuantizationType::Q2_K;
    }
    
    // Normalization → Q2_K (low precision sufficient)
    if (tensorName.find("norm") != std::string::npos) {
        return QuantizationType::Q2_K;
    }
    
    // Default: assume uniform quantization
    return QuantizationType::Q4_K;
}
```

---

## 3. Test Scenarios

### Scenario A: v3 Reader + GGUF v3 Uniform File

```
Input:
  Reader:   llama.cpp v1.45 (GGUF v3)
  File:     llama-7b-q4k-v3.gguf (version=3, uniform Q4_K)

Expected:
  ✅ FULL SUCCESS
  - All tensors load in Q4_K format
  - Inference produces correct output
  - Load time: ~716 ms (baseline)
  - Zero warnings
```

### Scenario B: v3 Reader + GGUF v4 Uniform File

```
Input:
  Reader:   llama.cpp v1.45 (GGUF v3)
  File:     llama-7b-q4k-v4-uniform.gguf (version=4, uniform Q4_K)

Expected:
  ✅ FORWARD COMPATIBLE
  - Warning: "GGUF version 4 is newer than parser v3"
  - Tensors load successfully
  - Quantization type inferred from metadata (Q4_K)
  - Inference works correctly
  - Load time: ~720 ms (minimal overhead)
  
Risk: LOW (no per-tensor metadata in uniform file)
```

### Scenario C: v3 Reader + GGUF v4 Hybrid File

```
Input:
  Reader:   llama.cpp v1.45 (GGUF v3)
  File:     llama-7b-hybrid.gguf (version=4, hybrid Q2_K+Q4_K)

Expected:
  ⚠️  PARTIAL (Name-Based Inference)
  - Warning: "GGUF version 4 is newer than parser v3"
  - v3 reader ignores tensor quantization map
  - Falls back to name-based type inference
  - Infers: attention=Q4_K, FFN=Q2_K (happens to be correct!)
  - Inference produces reasonable output
  
Risk: MEDIUM (correctness depends on naming conventions)

Mitigation:
  1. Document naming standard (layers.N.attn, layers.N.mlp, etc.)
  2. v3 readers can work if naming conventions followed
  3. v4 readers verify against explicit map for safety
  4. Warning message directs users to upgrade
```

### Scenario D: v4 Reader + All File Versions

```
Input:
  Reader:   v4 InferenceEngine (GGUF v4 compatible)
  Files:    v3 uniform, v4 uniform, v4 hybrid

Expected:
  ✅ FULL SUPPORT - All versions work
  
  v3 Uniform:     Uses legacy parser path, works perfectly
  v4 Uniform:     Uses v4 parser, works perfectly
  v4 Hybrid:      Uses per-tensor routing, works perfectly
  
  Zero errors, automatic format detection, correct loading
```

---

## 4. Metadata Compatibility

### Global Metadata Keys

**GGUF v3 (Legacy):**
```json
{
  "quantization_version": "001"
}
```

**GGUF v4 (New, Backward Compatible):**
```json
{
  "quantization_version": "001",           // Keep for v3 readers
  "quantization_schema_version": "002",    // NEW: indicates v4 support
  "quantization_mode": "UNIFORM",          // NEW: "UNIFORM" or "HYBRID"
  "quantization_tensor_map_offset": 0x200000,  // NEW: file offset
  "quantization_tensor_map_count": 500        // NEW: number of entries
}
```

**v3 Reader Behavior:**
- Sees "quantization_version" → Uses it
- Ignores "quantization_schema_version" → Treats as v3
- Ignores new keys → Graceful degradation
- Assumes uniform quantization → Correct for uniform files, wrong for hybrid

**v4 Reader Behavior:**
- Checks "quantization_schema_version" first
- If present and >= 2 → Reads tensor map
- If absent → Falls back to v3 mode
- Infers from names if map unavailable
- Always succeeds with best-effort approach

---

## 5. Test Suite Structure

### 5.1 Unit Tests

```cpp
// tests/test_compat_v3_reader.cpp
TEST(CompatV3Reader, ParseV3UniformFile) {
    // v3 reader loads v3 uniform file
    EXPECT_SUCCESS();
    EXPECT_LOAD_TIME_LT(750ms);
}

TEST(CompatV3Reader, ParseV4UniformFileWithWarning) {
    // v3 reader loads v4 uniform file
    EXPECT_SUCCESS();
    EXPECT_WARNING_LOGGED("version 4");
}

TEST(CompatV3Reader, ParseV4HybridFileWithFallback) {
    // v3 reader attempts v4 hybrid file
    EXPECT_SUCCESS();
    EXPECT_USES_NAME_INFERENCE();
}

// tests/test_compat_v4_reader.cpp
TEST(CompatV4Reader, ParseAllVersions) {
    EXPECT_PARSE_SUCCESS(v3_uniform);
    EXPECT_PARSE_SUCCESS(v4_uniform);
    EXPECT_PARSE_SUCCESS(v4_hybrid);
}

TEST(CompatV4Reader, CorrectQuantTypeInference) {
    auto quantType = parser.getTensorQuantType("layers.0.self_attn.q_proj");
    EXPECT_EQ(quantType, QuantizationType::Q4_K);
}
```

### 5.2 Integration Tests

```cpp
// tests/test_compat_inference.cpp
TEST(CompatInference, V3FileInference) {
    InferenceEngine engine;
    EXPECT_TRUE(engine.loadModel("llama-7b-v3.gguf"));
    auto tokens = engine.generate({1, 2, 3}, 10);
    EXPECT_GT(tokens.size(), 3);
}

TEST(CompatInference, V4HybridInference) {
    InferenceEngine engine;
    EXPECT_TRUE(engine.loadModel("llama-7b-hybrid.gguf"));
    auto tokens = engine.generate({1, 2, 3}, 10);
    EXPECT_GT(tokens.size(), 3);
}

TEST(CompatInference, AccuracyConsistency) {
    // Same input produces same output (deterministic)
    InferenceEngine e1, e2;
    e1.loadModel("llama-7b-v3.gguf");
    e2.loadModel("llama-7b-v3.gguf");
    
    auto tokens1 = e1.generate({1, 2, 3}, 20);
    auto tokens2 = e2.generate({1, 2, 3}, 20);
    EXPECT_EQ(tokens1, tokens2);
}
```

### 5.3 Stress Tests

```cpp
// tests/test_compat_stress.cpp
TEST(CompatStress, MixedVersionDirectory) {
    // Load many models of different versions
    // Verify no cross-contamination
    
    for (const auto& file : getAllModelFiles()) {
        InferenceEngine engine;
        EXPECT_TRUE(engine.loadModel(file));
        
        // Verify metadata matches expected version
        EXPECT_EQ(engine.getQuantMode(), expectedMode(file));
    }
}

TEST(CompatStress, LargeHybridModel) {
    // Load 70B hybrid model (full size)
    InferenceEngine engine;
    EXPECT_TRUE(engine.loadModel("llama-70b-hybrid.gguf"));
    EXPECT_TRUE(engine.isHybridQuantized());
    EXPECT_LT(engine.getLoadTimeMs(), 9500);  // Should load < 9.5 sec
}

TEST(CompatStress, CorruptedMetadata) {
    // Corrupted tensor map should gracefully degrade
    InferenceEngine engine;
    EXPECT_TRUE(engine.loadModel("corrupted_map.gguf"));  // Don't crash
    EXPECT_FALSE(engine.isHybridQuantized());  // Falls back to uniform
}
```

---

## 6. Deployment Phases

### Phase 1: RC Release (Weeks 1-2)
```
Release both v3 and v4 parsers
- Default: v3 parser (safest, tested)
- Fallback: v4 parser for testing
- Clear: Version detection and warnings
- Goal: Get community feedback
```

### Phase 2: Full v4 Support (Weeks 3-4)
```
Promote v4 parser to default
- Keep v3 parser for backward compat
- Update all documentation
- Announce ecosystem upgrade timeline
- Support period: 24 months for v3
```

### Phase 3: Deprecation (12+ months later)
```
Mark v3 parser as "legacy"
- Still functional
- Warning: "Please upgrade to v4 tools"
- Plan removal in v5.0
```

---

## 7. Success Criteria Checklist

- [ ] v3 readers can load v4 uniform files (forward compatible)
- [ ] v4 readers can load v3 files (backward compatible)
- [ ] v3 readers gracefully handle v4 hybrid files (name inference)
- [ ] No crashes on any file version combination
- [ ] Performance regression < 2% for existing models
- [ ] Hybrid models load 8-12% faster than Q2_K
- [ ] All backward compatibility tests pass
- [ ] Clear warning/error messages for users
- [ ] No data corruption in any scenario
- [ ] Documentation complete and accurate

---

## 8. Implementation Checklist

- [ ] Add version detection to parser
- [ ] Implement safe fallback for unknown versions
- [ ] Add name-based quantization inference
- [ ] Implement per-tensor metadata parsing (v4)
- [ ] Update InferenceEngine to use new parser
- [ ] Create all test files (v3 uniform, v4 uniform, v4 hybrid)
- [ ] Write comprehensive test suite
- [ ] Verify performance metrics
- [ ] Document for users and tool maintainers
- [ ] Community review period
- [ ] Release RC with clear versioning notes

---

*Complete backward compatibility ensures safe adoption of hybrid quantization across the entire GGUF ecosystem.*
