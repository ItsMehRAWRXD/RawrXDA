# RAWR Monolith v2.1 - Critical Fixes Applied

## Three Issues Identified and Fixed

### 1. Causal Mask (FIXED)
**Location:** `attention_head()` function

**Problem:** The original code only implicitly masked future tokens by relying on cache length, which doesn't actually prevent the model from attending to future positions during generation.

**Fix:** Added explicit causal mask:
```cpp
// FIXED: Explicit causal mask - prevent attending to future tokens
for (int i = current_pos + 1; i < seq_len; i++) {
    scores[i] = -INFINITY;
}
```

**Impact:** Without this fix, the model would "cheat" by looking at future tokens during training/inference, leading to nonsensical outputs.

---

### 2. RoPE Application (FIXED)
**Location:** `forward_layer()` function

**Problem:** RoPE (Rotary Position Embedding) was being applied to the full Q/K vectors before they were split into heads, which is incorrect. RoPE must be applied per-head after the QKV projection.

**Fix:** Created `apply_rope_per_head()` function:
```cpp
// FIXED: Apply RoPE per-head after QKV projection
apply_rope_per_head(q, k, pos, head_dim, n_heads);
```

The new function iterates over each head and applies the rotation to head_dim dimensions separately.

**Impact:** Without this fix, position encoding would be garbled, causing the model to lose positional understanding.

---

### 3. GGUF Tensor Stride (FIXED)
**Location:** `GGUFModel::parse_tensors()` and `TensorInfo` struct

**Problem:** The original code assumed all tensors are row-major contiguous. Some GGUF weight matrices are transposed (especially when dims[0] < dims[1]).

**Fix:** Added stride calculation and transposition detection:
```cpp
// Calculate strides
if (ti.n_dims >= 2) {
    ti.stride[0] = ti.dims[1] * elem_size;  // Row stride
    ti.stride[1] = elem_size;              // Column stride
    // Detect transposed weight matrices
    if (ti.dims[0] < ti.dims[1] && ti.name.find("weight") != string::npos) {
        ti.is_transposed = true;
    }
}
```

**Impact:** Without this fix, weight matrices would be read incorrectly, causing completely wrong outputs.

---

## Debug Infrastructure Added

### `--debug-logits` Flag
Dumps intermediate hidden states for layer-by-layer comparison with reference implementations (llama.cpp).

### `--debug-layer N` Flag
Limits debug output to a specific layer for focused debugging.

---

## Next Steps for Validation

1. **Load TinyLlama-1B or Phi-3-mini GGUF**
2. **Run side-by-side with llama.cpp:**
   ```bash
   # Reference
   ./llama.cpp -m tinyllama-1b.Q4_K_M.gguf -p "The capital of France is" -n 20
   
   # Your engine
   ./rawr_monolith_v2 tinyllama-1b.Q4_K_M.gguf "The capital of France is" 20
   ```
3. **Compare outputs** - tokens should match exactly
4. **If mismatch:** Use `--debug-logits --debug-layer 0` to find first divergent layer

---

## Files Updated
- `rawr_monolith_v2.cpp` - Complete runtime with fixes
- `VALIDATION_REPORT.md` - Reality-checked assessment

## Compilation Verified
```bash
g++ -std=c++17 -O2 -march=native -pthread -o rawr_monolith_v2.exe rawr_monolith_v2.cpp
# Result: SUCCESS
```
