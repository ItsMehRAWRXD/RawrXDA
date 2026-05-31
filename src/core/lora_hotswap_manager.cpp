// ============================================================================
// lora_hotswap_manager.cpp — Implementation
// ============================================================================
#include "lora_hotswap_manager.h"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstring>
#include <fstream>

#ifdef _MSC_VER
#include <intrin.h>
#endif
#ifdef __AVX2__
#include <immintrin.h>
#endif

namespace rawrxd {

// ============================================================================
// GGUF minimal parser — enough to find lora_a/lora_b tensors
// We need:
//  - Header magic/version check
//  - Tensor info list (name, type, dims, offset)
//  - Data section seek + read
// ============================================================================
namespace gguf_lora {

static const uint32_t GGUF_MAGIC   = 0x46554747u; // "GGUF"
static const uint32_t GGUF_VER2    = 2;
static const uint32_t GGUF_VER3    = 3;

enum GgufType : uint32_t {
    GGUF_TYPE_UINT8   = 0,
    GGUF_TYPE_INT8    = 1,
    GGUF_TYPE_UINT16  = 2,
    GGUF_TYPE_INT16   = 3,
    GGUF_TYPE_UINT32  = 4,
    GGUF_TYPE_INT32   = 5,
    GGUF_TYPE_FLOAT32 = 6,
    GGUF_TYPE_BOOL    = 7,
    GGUF_TYPE_STRING  = 8,
    GGUF_TYPE_ARRAY   = 9,
    GGUF_TYPE_UINT64  = 10,
    GGUF_TYPE_INT64   = 11,
    GGUF_TYPE_FLOAT64 = 12,
};

struct TensorInfo {
    std::string name;
    uint32_t    n_dims;
    uint64_t    dims[4];
    uint32_t    ggml_rxd_type;
    uint64_t    offset;     // byte offset from start of data section
};

static bool read_u32(std::ifstream& f, uint32_t& out) {
    return (bool)(f.read((char*)&out, 4));
}
static bool read_u64(std::ifstream& f, uint64_t& out) {
    return (bool)(f.read((char*)&out, 8));
}
static bool read_str(std::ifstream& f, std::string& out) {
    uint64_t len = 0;
    if (!f.read((char*)&len, 8)) return false;
    if (len > 65536) return false; // sanity
    out.resize((size_t)len);
    return (bool)(f.read(out.data(), (std::streamsize)len));
}

// Skip a metadata value of known type (version 2/3 compatible)
static bool skip_value(std::ifstream& f, uint32_t vtype) {
    switch (vtype) {
    case GGUF_TYPE_UINT8: case GGUF_TYPE_INT8: case GGUF_TYPE_BOOL:
        f.seekg(1, std::ios::cur); break;
    case GGUF_TYPE_UINT16: case GGUF_TYPE_INT16:
        f.seekg(2, std::ios::cur); break;
    case GGUF_TYPE_UINT32: case GGUF_TYPE_INT32: case GGUF_TYPE_FLOAT32:
        f.seekg(4, std::ios::cur); break;
    case GGUF_TYPE_UINT64: case GGUF_TYPE_INT64: case GGUF_TYPE_FLOAT64:
        f.seekg(8, std::ios::cur); break;
    case GGUF_TYPE_STRING: {
        uint64_t len = 0;
        if (!f.read((char*)&len, 8)) return false;
        f.seekg((std::streamoff)len, std::ios::cur);
        break;
    }
    case GGUF_TYPE_ARRAY: {
        uint32_t elem_type = 0;
        uint64_t count = 0;
        if (!f.read((char*)&elem_type, 4)) return false;
        if (!f.read((char*)&count, 8)) return false;
        for (uint64_t i = 0; i < count; ++i)
            if (!skip_value(f, elem_type)) return false;
        break;
    }
    default: return false;
    }
    return f.good();
}

// Parse a GGUF file and return tensor infos for lora_a / lora_b tensors
bool parse_lora_tensors(const std::string& path,
                        uint64_t& out_data_offset,
                        std::vector<TensorInfo>& tensors)
{
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;

    uint32_t magic = 0, version = 0;
    if (!read_u32(f, magic) || magic != GGUF_MAGIC) return false;
    if (!read_u32(f, version)) return false;
    if (version < GGUF_VER2 || version > GGUF_VER3) return false;

    uint64_t n_tensors = 0, n_kvs = 0;
    if (!read_u64(f, n_tensors)) return false;
    if (!read_u64(f, n_kvs))    return false;

    // Skip all KV metadata
    for (uint64_t i = 0; i < n_kvs; ++i) {
        std::string key;
        uint32_t vtype = 0;
        if (!read_str(f, key)) return false;
        if (!f.read((char*)&vtype, 4)) return false;
        if (!skip_value(f, vtype)) return false;
    }

    // Read tensor infos
    tensors.clear();
    tensors.reserve((size_t)n_tensors);

    for (uint64_t i = 0; i < n_tensors; ++i) {
        TensorInfo ti{};
        ti.n_dims = 0;
        if (!read_str(f, ti.name)) return false;
        if (!f.read((char*)&ti.n_dims, 4)) return false;
        if (ti.n_dims > 4) return false;
        for (uint32_t d = 0; d < ti.n_dims; ++d)
            if (!read_u64(f, ti.dims[d])) return false;
        if (!f.read((char*)&ti.ggml_rxd_type, 4)) return false;
        if (!read_u64(f, ti.offset)) return false;
        tensors.push_back(std::move(ti));
    }

    // Data section starts at next 32-byte aligned position
    uint64_t header_end = (uint64_t)f.tellg();
    out_data_offset = (header_end + 31) & ~uint64_t(31);

    return f.good();
}

// Read a F32 tensor from the data section into a float vector
bool read_f32_tensor(const std::string& path,
                     uint64_t data_offset,
                     const TensorInfo& ti,
                     std::vector<float>& out)
{
    if (ti.ggml_rxd_type != GGUF_TYPE_FLOAT32) return false; // only F32 lora for now

    uint64_t n_elems = 1;
    for (uint32_t d = 0; d < ti.n_dims; ++d) n_elems *= ti.dims[d];

    std::ifstream f(path, std::ios::binary);
    if (!f) return false;

    uint64_t file_off = data_offset + ti.offset;
    f.seekg((std::streamoff)file_off, std::ios::beg);
    if (!f) return false;

    out.resize((size_t)n_elems);
    f.read((char*)out.data(), (std::streamsize)(n_elems * sizeof(float)));
    return f.good();
}

} // namespace gguf_lora

// ============================================================================
// parseGgufLoraFile
// ============================================================================
bool LoRAHotswapManager::parseGgufLoraFile(const std::string& path,
                                            LoRAAdapter& adapter)
{
    using namespace gguf_lora;

    std::vector<TensorInfo> tensors;
    uint64_t data_offset = 0;

    if (!parse_lora_tensors(path, data_offset, tensors))
        return false;

    // Map tensor names to LoRAWeights
    // Expected naming convention (llama.cpp compatible):
    //   blk.0.attn_q.weight.lora_a
    //   blk.0.attn_q.weight.lora_b

    std::unordered_map<std::string, std::pair<TensorInfo*, TensorInfo*>> pairs;

    for (auto& ti : tensors) {
        // Determine if A or B and what the base layer name is
        bool is_a = false, is_b = false;
        std::string base;

        auto ends_with = [&](const std::string& suffix) {
            if (ti.name.size() < suffix.size()) return false;
            return ti.name.compare(ti.name.size() - suffix.size(),
                                   suffix.size(), suffix) == 0;
        };

        if (ends_with(".lora_a")) {
            is_a = true;
            base = ti.name.substr(0, ti.name.size() - 7); // remove ".lora_a"
        } else if (ends_with(".lora_b")) {
            is_b = true;
            base = ti.name.substr(0, ti.name.size() - 7); // remove ".lora_b"
        }

        // Strip trailing ".weight" from base name to get the layer name
        if (ends_with(".weight")) base = base.substr(0, base.size() - 7);

        if (!is_a && !is_b) continue;

        pairs[base]; // ensure entry exists
        if (is_a) pairs[base].first  = &ti;
        else       pairs[base].second = &ti;
    }

    // Build LoRAWeights per layer
    for (auto& [layer, ab_pair] : pairs) {
        TensorInfo* a_ti = ab_pair.first;
        TensorInfo* b_ti = ab_pair.second;
        if (!a_ti || !b_ti) continue;  // incomplete pair

        LoRAWeights w;
        w.layer_name = layer;

        // A shape: [rank, in_features]
        w.rank   = (a_ti->n_dims >= 1) ? (int32_t)a_ti->dims[0] : 8;
        w.cols_a = (a_ti->n_dims >= 2) ? (int32_t)a_ti->dims[1] : 0;
        w.rows_a = w.rank;

        // B shape: [out_features, rank]
        w.rows_b = (b_ti->n_dims >= 1) ? (int32_t)b_ti->dims[0] : 0;
        w.cols_b = (b_ti->n_dims >= 2) ? (int32_t)b_ti->dims[1] : w.rank;

        w.alpha  = (float)w.rank; // default alpha == rank

        if (!gguf_lora::read_f32_tensor(path, data_offset, *a_ti, w.A)) continue;
        if (!gguf_lora::read_f32_tensor(path, data_offset, *b_ti, w.B)) continue;

        size_t idx = adapter.layers.size();
        adapter.layer_index[layer] = idx;
        adapter.layers.push_back(std::move(w));
    }

    return !adapter.layers.empty();
}

// ============================================================================
// load
// ============================================================================
bool LoRAHotswapManager::load(const std::string& name, const std::string& gguf_path)
{
    LoRAAdapter adapter;
    adapter.name        = name;
    adapter.source_path = gguf_path;
    adapter.active      = false;
    adapter.scale       = 1.0f;

    if (!parseGgufLoraFile(gguf_path, adapter))
        return false;

    std::lock_guard<std::mutex> lk(mu_);
    registry_.emplace(name, std::move(adapter));
    return true;
}

void LoRAHotswapManager::registerAdapter(const std::string& name, LoRAAdapter adapter)
{
    std::lock_guard<std::mutex> lk(mu_);
    adapter.name = name;
    // Rebuild index
    adapter.layer_index.clear();
    for (size_t i = 0; i < adapter.layers.size(); ++i)
        adapter.layer_index[adapter.layers[i].layer_name] = i;
    registry_.emplace(name, std::move(adapter));
}

bool LoRAHotswapManager::unload(const std::string& name)
{
    std::lock_guard<std::mutex> lk(mu_);
    auto it = registry_.find(name);
    if (it == registry_.end()) return false;
    if (it->second.active) return false; // must deactivate first
    registry_.erase(it);
    return true;
}

// ============================================================================
// activate / deactivate
// ============================================================================
bool LoRAHotswapManager::activate(const std::string& name, float scale)
{
    std::lock_guard<std::mutex> lk(mu_);
    auto it = registry_.find(name);
    if (it == registry_.end()) return false;
    it->second.active = true;
    it->second.scale  = scale;
    if (swap_cb_) swap_cb_(name, true);
    return true;
}

bool LoRAHotswapManager::deactivate(const std::string& name)
{
    std::lock_guard<std::mutex> lk(mu_);
    auto it = registry_.find(name);
    if (it == registry_.end()) return false;
    it->second.active = false;
    if (swap_cb_) swap_cb_(name, false);
    return true;
}

bool LoRAHotswapManager::activateExclusive(const std::string& name, float scale)
{
    std::lock_guard<std::mutex> lk(mu_);
    for (auto& [n, a] : registry_) {
        if (n == name) {
            a.active = true;
            a.scale  = scale;
            if (swap_cb_) swap_cb_(n, true);
        } else {
            bool was = a.active;
            a.active = false;
            if (was && swap_cb_) swap_cb_(n, false);
        }
    }
    return registry_.count(name) > 0;
}

// ============================================================================
// computeDelta: out += (alpha/rank) * scale * B * A * x
// AVX2 accelerated for large matrices
// ============================================================================
void LoRAHotswapManager::computeDelta(
    const LoRAWeights& w, float global_scale,
    const float* x, float* out,
    int32_t in_dim, int32_t out_dim)
{
    if (w.A.empty() || w.B.empty()) return;
    if (in_dim <= 0 || out_dim <= 0 || w.rank <= 0) return;

    float lora_scale = (w.alpha / (float)w.rank) * global_scale;

    // Intermediate: h = A * x  [rank]
    std::vector<float> h(w.rank, 0.0f);

    int32_t rank = w.rank;
    int32_t cols = std::min(in_dim, w.cols_a);

    // h = A * x
    for (int32_t r = 0; r < rank; ++r) {
        float acc = 0.0f;
        const float* Ar = w.A.data() + r * cols;
#ifdef __AVX2__
        int32_t si = 0;
        __m256 vacc = _mm256_setzero_ps();
        for (; si + 8 <= cols; si += 8) {
            __m256 va = _mm256_loadu_ps(Ar + si);
            __m256 vx = _mm256_loadu_ps(x   + si);
            vacc = _mm256_fmadd_ps(va, vx, vacc);
        }
        float tmp[8];
        _mm256_storeu_ps(tmp, vacc);
        for (int t = 0; t < 8; ++t) acc += tmp[t];
        for (; si < cols; ++si) acc += Ar[si] * x[si];
#else
        for (int32_t i = 0; i < cols; ++i) acc += Ar[i] * x[i];
#endif
        h[r] = acc;
    }

    // Apply scale
    for (int32_t r = 0; r < rank; ++r) h[r] *= lora_scale;

    // out += B * h
    int32_t rows_b = std::min(out_dim, w.rows_b);
    for (int32_t o = 0; o < rows_b; ++o) {
        float acc = 0.0f;
        const float* Br = w.B.data() + o * rank;
        for (int32_t r = 0; r < rank; ++r) acc += Br[r] * h[r];
        out[o] += acc;
    }
}

// ============================================================================
// applyAdapterDelta
// ============================================================================
LoRADeltaResult LoRAHotswapManager::applyAdapterDelta(
    const std::string& layer_name,
    const float* x,
    float* out,
    int32_t in_dim,
    int32_t out_dim)
{
    LoRADeltaResult result;
    result.layer_name = layer_name;

    std::lock_guard<std::mutex> lk(mu_);

    float delta_sq = 0.0f;

    for (auto& [name, adapter] : registry_) {
        if (!adapter.active) continue;

        auto it = adapter.layer_index.find(layer_name);
        if (it == adapter.layer_index.end()) continue;

        const LoRAWeights& w = adapter.layers[it->second];
        computeDelta(w, adapter.scale, x, out, in_dim, out_dim);
        result.applied = true;

        // Compute partial Frobenius norm of delta for observability
        for (int32_t i = 0; i < out_dim; ++i) delta_sq += out[i] * out[i];
    }

    result.magnitude = (result.applied) ? std::sqrt(delta_sq) : 0.0f;
    return result;
}

void LoRAHotswapManager::applyAdapterDeltaBatch(
    const std::string& layer_name,
    const float* x,
    float* out,
    int32_t in_dim,
    int32_t out_dim,
    int32_t seq_len)
{
    for (int32_t t = 0; t < seq_len; ++t)
        applyAdapterDelta(layer_name,
                          x   + t * in_dim,
                          out + t * out_dim,
                          in_dim, out_dim);
}

// ============================================================================
// Status
// ============================================================================
bool LoRAHotswapManager::isActive(const std::string& name) const
{
    std::lock_guard<std::mutex> lk(mu_);
    auto it = registry_.find(name);
    return it != registry_.end() && it->second.active;
}

std::vector<std::string> LoRAHotswapManager::activeAdapters() const
{
    std::lock_guard<std::mutex> lk(mu_);
    std::vector<std::string> out;
    for (auto& [n, a] : registry_) if (a.active) out.push_back(n);
    return out;
}

std::vector<std::string> LoRAHotswapManager::loadedAdapters() const
{
    std::lock_guard<std::mutex> lk(mu_);
    std::vector<std::string> out;
    for (auto& [n, a] : registry_) out.push_back(n);
    return out;
}

bool LoRAHotswapManager::hasLayer(const std::string& adapter,
                                   const std::string& layer) const
{
    std::lock_guard<std::mutex> lk(mu_);
    auto it = registry_.find(adapter);
    if (it == registry_.end()) return false;
    return it->second.layer_index.count(layer) > 0;
}

} // namespace rawrxd
