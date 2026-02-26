# Implementation Roadmap: From Current State to Production

## What We Just Fixed ✅
- **GGUFLoader Linking Conflict** - Removed stub, linked real implementation
- **InferenceEngine Destructor** - Added proper cleanup
- **Hardcoded Parameter Crash** - Removed premature weight loading
- **Include Path Issues** - Fixed CMake configuration

**Result:** Models can now be read from disk, but inference pipeline is incomplete.

---

## Critical Path to Functional Model Loading

### WEEK 1: Stop Crashes (Prevent Segmentation Faults)

**Task 1.1: Fix Tensor Type Mismatch** [2-3 days]
- **Impact:** Currently loads Q4_0 data into F32 tensors → garbage math
- **Files:** `src/qtapp/inference_engine.hpp`, `transformer_inference.cpp`
- **Changes:**
  ```cpp
  // Change from: QHash<QString, QByteArray> m_tensorCache;
  // To:
  struct CachedTensorData {
      QByteArray data;
      int ggml_type_id;  // e.g., 8 for Q4_0, 0 for F32
  };
  QHash<QString, CachedTensorData> m_tensorCache;
  ```
- **Testing:** Load Q4_0 quantized models without crashes
- **Success Criteria:** No segfaults during inference

**Task 1.2: Add Bias Term Loading** [1-2 days]
- **Impact:** Model outputs are skewed without bias terms
- **Files:** `src/qtapp/transformer_inference.cpp` (lines 90-110)
- **Changes:**
  ```cpp
  // Add to LayerWeights struct
  ggml_tensor* attn_q_bias;
  ggml_tensor* mlp_fc1_bias;
  // etc.
  
  // Load in loadWeights()
  layer.attn_q_bias = createTensorFromCache(prefix + "attn_q.bias", ...);
  ```
- **Testing:** Models with/without bias load correctly
- **Success Criteria:** Output quality improves noticeably

**Task 1.3: Add Size Validation** [1 day]
- **Impact:** Prevent memory corruption from mismatched data
- **Files:** `src/qtapp/transformer_inference.cpp` (lines 176-183)
- **Changes:**
  ```cpp
  // Before memcpy, verify:
  if (expected_size != data.size()) {
      qCritical() << "Size mismatch - tensor corrupt";
      return nullptr;  // Fail loudly
  }
  ```
- **Testing:** Wrong data sizes are caught and logged
- **Success Criteria:** Clear error messages, no silent corruption

---

### WEEK 2-3: Fix Tokenization (Make Output Readable)

**Task 2.1: Implement Proper Tokenizer Initialization** [4-5 days]
- **Impact:** Currently falls back to broken hash-based tokenizer
- **Files:** `src/qtapp/inference_engine.cpp` (lines 470-530)
- **Changes:**
  1. Implement `GGUFLoaderQt::getTokenizerMetadata()` properly
  2. Load BPE merges or SentencePiece model
  3. Initialize BPE/SP tokenizers with real data
- **Dependencies:** GGUF must have tokenizer metadata
- **Testing:** Model produces same tokens as reference implementation
- **Success Criteria:** Tokenizer initialized without fallback

**Task 2.2: Implement Vocabulary Loading** [2-3 days]
- **Impact:** Detokenization currently produces "tok_123 tok_456"
- **Files:** `src/qtapp/vocabulary_loader.cpp`, `inference_engine.cpp`
- **Changes:**
  1. Load vocab from GGUF or BPE/SP tokenizer
  2. Build token_id → text mapping
  3. Fix detokenize() to use real vocab
- **Testing:** Model output is readable English/language text
- **Success Criteria:** Output is human-readable sentences

**Task 2.3: Add Fallback Vocab** [1 day]
- **Impact:** For models without embedded vocab
- **Files:** Create `src/qtapp/fallback_vocab.hpp`
- **Changes:**
  - Build minimal 1k-token vocab for basic models
  - Map common token ranges to approximate strings
- **Testing:** Works on models without vocab metadata
- **Success Criteria:** Something readable instead of "tok_123"

---

### WEEK 3: Fix Inference Pipeline (Add Queuing)

**Task 3.1: Implement Inference Request Queue** [2-3 days]
- **Impact:** Currently silently ignores requests until transformer ready
- **Files:** `src/qtapp/inference_engine.cpp`, `inference_engine.hpp`
- **Changes:**
  ```cpp
  // Add to InferenceEngine
  class InferenceRequestQueue {
      void enqueue(const QString& prompt, qint64 reqId);
      void process();  // Called when transformer ready
  };
  ```
- **Testing:** Multiple inference requests are queued and processed
- **Success Criteria:** All requests eventually get processed

**Task 3.2: Add Transformer Ready Signal** [1 day]
- **Impact:** UI can wait for model to be ready
- **Files:** `src/qtapp/inference_engine.hpp`
- **Changes:**
  ```cpp
  signals:
      void transformerReady();  // Emit when m_transformer.isReady()
  ```
- **Testing:** Signal fires when transformer initialization complete
- **Success Criteria:** UI responds to model ready state

---

## Verification Checklist for Phase 1 (Stop Crashes + Readable Output)

- [ ] Load GGUF file without crashes
- [ ] Model dimensions read correctly
- [ ] Tensors loaded with correct types (Q4_0, F32, etc.)
- [ ] No memory corruption or truncation warnings
- [ ] Tokenization produces valid token IDs
- [ ] Detokenization produces readable text
- [ ] Multiple inference requests queued and processed
- [ ] Model produces some coherent output
- [ ] No "[Transformer weights still loading...]" messages

**Timeline:** 2-3 weeks
**Effort:** 1-2 developers
**Success Metric:** Can load a GGUF model and get readable output

---

## Phase 2: Performance & Advanced Features (Weeks 4-8)

### 4.1: GPU Acceleration [5-10 days]
- [ ] Implement Vulkan kernels
- [ ] CUDA support (if NVIDIA target)
- [ ] Benchmark vs CPU

### 4.2: Streaming Inference [3-4 days]
- [ ] Token-by-token generation
- [ ] Async generator interface
- [ ] Output buffering

### 4.3: Optimization [3-5 days]
- [ ] KV-cache proper management
- [ ] Batch inference support
- [ ] Memory reduction

---

## Phase 3: Production Hardening (Weeks 8-12)

### 5.1: Error Handling [2-3 days]
- [ ] Graceful fallbacks for missing features
- [ ] Detailed error diagnostics
- [ ] Recovery from partial loads

### 5.2: Testing [3-5 days]
- [ ] Unit tests for each component
- [ ] Integration tests (end-to-end)
- [ ] Model compatibility testing

### 5.3: Documentation [2-3 days]
- [ ] Tokenizer implementation guide
- [ ] GPU setup instructions
- [ ] Performance tuning guide

---

## Files That Need Changes (Priority Order)

### CRITICAL (Must change to make it work):
1. `src/qtapp/inference_engine.hpp` - Update cache structure
2. `src/qtapp/inference_engine.cpp` - Fix tokenization & queuing
3. `src/qtapp/transformer_inference.cpp` - Fix tensor loading
4. `src/qtapp/vocabulary_loader.cpp` - Implement vocab loading
5. `src/qtapp/bpe_tokenizer.cpp` - Ensure initialization works
6. `src/qtapp/sentencepiece_tokenizer.cpp` - Ensure initialization works

### HIGH PRIORITY (Should change soon):
7. `src/qtapp/gguf_loader.cpp` - Implement tokenizer metadata extraction
8. `src/qtapp/gguf_server.cpp` - Add queue/async support
9. `include/gguf_loader.h` - Extend interface if needed

### MEDIUM PRIORITY (Nice to have):
10. `src/qtapp/gpu_backend.cpp` - GPU acceleration
11. `src/qtapp/streaming_inference.cpp` - Streaming output
12. `src/qtapp/model_monitor.cpp` - Performance monitoring

---

## Dependencies Between Tasks

```
Task 1.1 (Tensor Type) ─→ Task 2.1 (Tokenizer Init)
Task 1.2 (Bias Terms)  ─┘
Task 1.3 (Size Check)  ─→ Task 2.2 (Vocab Loading)
                           ├─→ Task 3.1 (Request Queue)
Task 2.3 (Fallback)    ─┘   └─→ Task 3.2 (Ready Signal)
```

**Critical Path:** 1.1 → 2.1 → 2.2 → 3.1 (minimum 2 weeks)

---

## How to Track Progress

1. Create branches for each task: `feature/task-1-1-tensor-types`
2. Use checklist above to verify completion
3. Test against real GGUF models at each step
4. Keep detailed logs of what works/doesn't

---

## Risk Assessment

**Highest Risk:**
- Tokenizer format varies by model (BPE vs SentencePiece)
- Some models may not have vocab embedded
- Quantization format compatibility

**Mitigation:**
- Test with 3-5 different models (Llama, Mistral, etc.)
- Build fallbacks for missing components
- Log detailed diagnostics

---

## Success Indicators

**Week 1:** No crashes, basic tensor loading works
**Week 2:** Real tokens produced, not all garbage
**Week 3:** Human-readable output, multi-request handling
**Week 4:** Speed acceptable for single-threaded CPU
**Week 8:** GPU working, streaming output functional
