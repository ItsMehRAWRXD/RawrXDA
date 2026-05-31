#pragma once
// ============================================================================
// lora_hotswap_manager.h — Multi-adapter LoRA registry with hot-swap
//
// Features:
//   - Named adapter registry (load from GGUF LoRA file or raw tensors)
//   - activate() / deactivate() per adapter
//   - Multi-adapter stacking: delta = Σ (alpha_i/r_i) * B_i * A_i * x
//   - Per-layer delta hooks: adapter only affects requested layers
//   - Real weight loading (searches GGUF metadata for lora_a/lora_b tensors)
// ============================================================================
#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>

namespace rawrxd {

// ============================================================================
// LoRA adapter weight pair for one layer / module
// ============================================================================
struct LoRAWeights {
    std::string layer_name;   // e.g. "blk.0.attn_q"
    int32_t  rank          = 8;
    float    alpha         = 8.0f;  // scaling factor (alpha/rank applied at inference)
    int32_t  rows_a        = 0;     // A: [rank × in_features]
    int32_t  cols_a        = 0;
    int32_t  rows_b        = 0;     // B: [out_features × rank]
    int32_t  cols_b        = 0;
    std::vector<float> A;           // lora_a weights (rank × in)
    std::vector<float> B;           // lora_b weights (out × rank)
};

// ============================================================================
// LoRAAdapter — a loaded and named adapter with multiple layer weights
// ============================================================================
struct LoRAAdapter {
    std::string name;
    std::string source_path;    // GGUF file or empty for inline
    float       scale = 1.0f;   // global scale multiplier
    bool        active = false;

    std::vector<LoRAWeights> layers;

    // Fast lookup: layer_name → index in layers[]
    std::unordered_map<std::string, size_t> layer_index;
};

// ============================================================================
// LoRADeltaResult — output of applyAdapterDelta for one call site
// ============================================================================
struct LoRADeltaResult {
    std::string layer_name;
    bool        applied   = false;
    float       magnitude = 0.0f;  // Frobenius norm of delta (for monitoring)
};

// ============================================================================
// LoRAHotswapManager
//
// Usage:
//   manager.load("myAdapter", "path/to/lora.gguf");
//   manager.activate("myAdapter");
//   // At each linear layer in the model forward pass:
//   manager.applyAdapterDelta("blk.0.attn_q", x, out, in_dim, out_dim);
// ============================================================================
class LoRAHotswapManager {
public:
    LoRAHotswapManager() = default;
    ~LoRAHotswapManager() = default;

    // -------------------------------------------------------------------------
    // Registry management
    // -------------------------------------------------------------------------

    // Load a LoRA adapter from a GGUF file.
    // Parses lora_a / lora_b tensors and stores them in the registry.
    // Returns false on I/O or format error.
    bool load(const std::string& name, const std::string& gguf_path);

    // Register an adapter directly from pre-built LoRAWeights.
    // Useful for unit tests or synthetic adapters.
    void registerAdapter(const std::string& name, LoRAAdapter adapter);

    // Unload an adapter from memory (must be inactive).
    bool unload(const std::string& name);

    // Activate / deactivate by name. Thread-safe.
    bool activate(const std::string& name, float scale = 1.0f);
    bool deactivate(const std::string& name);

    // Activate exactly one adapter, deactivate all others.
    bool activateExclusive(const std::string& name, float scale = 1.0f);

    // -------------------------------------------------------------------------
    // Inference hook
    // -------------------------------------------------------------------------

    // Apply the summed LoRA delta for a given layer to an input tensor.
    //
    //   layer_name : identifies which weight matrix (e.g. "blk.5.attn_q")
    //   x          : input vector  [in_dim]
    //   out        : accumulation target [out_dim]  (out += delta * x)
    //   in_dim     : input feature dimension
    //   out_dim    : output feature dimension
    //
    // Returns a summary struct for observability.
    LoRADeltaResult applyAdapterDelta(
        const std::string& layer_name,
        const float*       x,
        float*             out,
        int32_t            in_dim,
        int32_t            out_dim);

    // Batch variant: process a [seq_len × in_dim] activation matrix.
    void applyAdapterDeltaBatch(
        const std::string& layer_name,
        const float*       x,       // [seq_len × in_dim]
        float*             out,     // [seq_len × out_dim]  (accumulated)
        int32_t            in_dim,
        int32_t            out_dim,
        int32_t            seq_len);

    // -------------------------------------------------------------------------
    // Status
    // -------------------------------------------------------------------------
    bool                isActive(const std::string& name) const;
    std::vector<std::string> activeAdapters() const;
    std::vector<std::string> loadedAdapters() const;
    bool                hasLayer(const std::string& adapter, const std::string& layer) const;

    // Register a callback fired when an adapter is activated or deactivated
    using SwapCallback = std::function<void(const std::string& name, bool activated)>;
    void onSwap(SwapCallback cb) { swap_cb_ = std::move(cb); }

private:
    // Compute delta for a single adapter's layer: out += (alpha/rank) * B * A * x
    void computeDelta(const LoRAWeights& w, float scale,
                      const float* x, float* out,
                      int32_t in_dim, int32_t out_dim);

    // Load GGUF binary and extract lora_a/lora_b pairs.
    bool parseGgufLoraFile(const std::string& path, LoRAAdapter& adapter);

    mutable std::mutex mu_;
    std::unordered_map<std::string, LoRAAdapter> registry_;
    SwapCallback swap_cb_;
};

} // namespace rawrxd
