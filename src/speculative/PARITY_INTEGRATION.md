# RAWR Parity Integration Guide
## How to wire the parity harness into your monolith

---

## Step 1: Add Divergence Detection to Your Monolith

Add this function to `rawr_monolith_v2.cpp`:

```cpp
// Add near the top with other debug flags
extern bool g_enable_divergence_detection;
extern float g_abs_tolerance;
extern float g_rel_tolerance;

// Add this function
void check_divergence(const std::vector<float>& expected,
                      const std::vector<float>& actual,
                      int layer,
                      const char* name) {
    if (!g_enable_divergence_detection) return;
    
    if (expected.size() != actual.size()) {
        std::cerr << "[DIVERGENCE] Size mismatch at layer " << layer 
                  << " " << name << ": " << expected.size() 
                  << " vs " << actual.size() << std::endl;
        exit(1);
    }
    
    for (size_t i = 0; i < expected.size(); i++) {
        float abs_err = fabsf(expected[i] - actual[i]);
        float rel_err = abs_err / (fabsf(expected[i]) + 1e-8f);
        
        if (abs_err > g_abs_tolerance && rel_err > g_rel_tolerance) {
            std::cerr << "\n🔴 DIVERGENCE at layer " << layer 
                      << " index " << i << " (" << name << ")" << std::endl;
            std::cerr << "   Expected: " << expected[i] << std::endl;
            std::cerr << "   Actual:   " << actual[i] << std::endl;
            std::cerr << "   Abs err:  " << abs_err << std::endl;
            std::cerr << "   Rel err:  " << rel_err << std::endl;
            exit(1);
        }
    }
}
```

---

## Step 2: Export Logits for External Comparison

Add to your `Transformer::logits()` function:

```cpp
vector<float> logits(const vector<float>& hidden) {
    vector<float> out;
    if (w_output) {
        out = matmul(w_output, hidden, model->n_vocab, dim);
    } else {
        out.resize(model->n_vocab, 0);
        for (int i = 0; i < dim; i++)
            out[i % model->n_vocab] += hidden[i];
    }
    
    // DEBUG: Export logits for comparison
    if (g_debug_logits) {
        static int token_count = 0;
        std::ofstream dump("logits_token_" + std::to_string(token_count) + ".bin", 
                          std::ios::binary);
        int size = out.size();
        dump.write(reinterpret_cast<const char*>(&size), sizeof(int));
        dump.write(reinterpret_cast<const char*>(out.data()), size * sizeof(float));
        token_count++;
    }
    
    return out;
}
```

---

## Step 3: Create Deterministic Sampling

Replace your current sampling with greedy (deterministic):

```cpp
int sample_token(const vector<float>& logits) {
    // DETERMINISTIC: Always pick highest logit
    // Temperature = 0, top_k = 1
    return std::max_element(logits.begin(), logits.end()) - logits.begin();
}
```

---

## Step 4: Add Layer-by-Layer Debug Output

In `forward_layer()`, add after each major operation:

```cpp
vector<float> forward_layer(...) {
    // ... existing code ...
    
    // After attention
    if (g_debug_layer == layer || g_debug_layer == -1) {
        cerr << "[L" << layer << "-ATTN] first 5 values: ";
        for (int i = 0; i < 5 && i < attn_residual.size(); i++) 
            cerr << attn_residual[i] << " ";
        cerr << endl;
    }
    
    // After FFN
    if (g_debug_layer == layer || g_debug_layer == -1) {
        cerr << "[L" << layer << "-FFN] first 5 values: ";
        for (int i = 0; i < 5 && i < output.size(); i++) 
            cerr << output[i] << " ";
        cerr << endl;
    }
    
    return output;
}
```

---

## Step 5: Integration Checklist

### Before running parity test:

- [ ] Temperature locked to 0.0
- [ ] Top-k locked to 1
- [ ] Fixed seed (42)
- [ ] Deterministic sampling (argmax, not random)
- [ ] Debug flags working (--debug-logits, --debug-layer)
- [ ] Logit export working

### Running the test:

```bash
# Build with debug support
g++ -std=c++17 -O2 -DDEBUG_PARITY -o rawr_monolith_v2.exe rawr_monolith_v2.cpp

# Run with first token only
./rawr_monolith_v2.exe tinyllama.gguf "Hello" 1 --debug-logits --debug-layer 0

# Compare exported logits with llama.cpp
# (Use the compare_logits_files function from parity_harness)
```

---

## Step 6: Common First Failures

When you run the real test, expect these to fail first:

### 1. Embedding lookup
**Symptom:** First divergence at layer -1 (embedding)
**Fix:** Check token ID mapping, BOS/EOS handling

### 2. RMSNorm epsilon
**Symptom:** Divergence at layer 0, small error (~1e-5)
**Fix:** Ensure epsilon matches GGUF metadata (usually 1e-5 or 1e-6)

### 3. Attention scaling
**Symptom:** Divergence at layer 0, growing error
**Fix:** Check `sqrt(head_dim)` vs `head_dim` scaling

### 4. Causal mask
**Symptom:** Works for token 1, fails for token 2+
**Fix:** Ensure mask is applied correctly for each position

### 5. RoPE base
**Symptom:** Divergence after attention, specific pattern
**Fix:** Verify base = 10000.0 (or model-specific value)

---

## Step 7: Binary Search Debug

If you find divergence at layer N:

```bash
# Test layer 0 only
./rawr_monolith_v2.exe --debug-layer 0 ...

# If pass, test layer 1
./rawr_monolith_v2.exe --debug-layer 1 ...

# Continue until find first failing layer
```

Then inspect:
- RMSNorm output
- QKV projections
- RoPE application
- Attention scores
- Softmax distribution
- FFN output

---

## Step 8: Success Criteria

You have parity when:

1. **Logits match:** First token logits identical to llama.cpp (within 1e-4)
2. **Tokens match:** Generated sequence identical
3. **Layer outputs match:** All intermediate tensors match

**Not "close enough" - exact match.**

---

## Files to Modify

```
rawr_monolith_v2.cpp:
  - Add divergence detection
  - Add logit export
  - Add layer debug output
  - Lock sampling to deterministic

parity_harness.cpp:
  - Reference implementations (already done)
  - Comparison functions (already done)

NEW: integrate_parity.sh (automated comparison script)
```

---

## Next Actions

1. **Add divergence detection to monolith** (30 min)
2. **Test with synthetic data** (15 min)
3. **Download TinyLlama** (5 min)
4. **Run first token comparison** (15 min)
5. **Fix first divergence** (varies)
6. **Repeat until parity** (1-3 days)

Then: Integrate AVX-512, measure speedup.
