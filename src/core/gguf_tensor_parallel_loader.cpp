// ============================================================================
// gguf_tensor_parallel_loader.cpp — Implementation
// ============================================================================
#include "gguf_tensor_parallel_loader.h"
#include <algorithm>
#include <cassert>
#include <cstring>
#include <fstream>
#include <stdexcept>
#include <unordered_map>

namespace rawrxd {

// ============================================================================
// Pimpl file handle
// ============================================================================
struct GgufTensorParallelLoader::Impl {
    std::ifstream file;
};

// ============================================================================
// GGUF constants (same as in lora_hotswap_manager.cpp)
// ============================================================================
static const uint32_t TPL_GGUF_MAGIC = 0x46554747u;
static const uint32_t TPL_GGUF_VER2  = 2;
static const uint32_t TPL_GGUF_VER3  = 3;

// GGML type IDs
enum TplGgmlType : uint32_t {
    TPL_F32      = 0,
    TPL_F16      = 1,
    TPL_Q4_0     = 2,
    TPL_Q4_1     = 3,
    TPL_Q4_K     = 16,
    TPL_Q5_K     = 17,
    TPL_Q6_K     = 18,
    TPL_Q8_0     = 8,
    TPL_Q8_1     = 9,
    TPL_Q2_K     = 14,
    TPL_Q3_K_S   = 11,
    TPL_Q3_K_M   = 12,
    TPL_Q3_K_L   = 13,
    TPL_Q5_0     = 6,
    TPL_Q5_1     = 7,
    TPL_I8       = 24,
    TPL_I16      = 25,
    TPL_I32      = 26,
};

// ============================================================================
// Constructor / Destructor
// ============================================================================
GgufTensorParallelLoader::GgufTensorParallelLoader(const Config& cfg)
    : cfg_(cfg), impl_(std::make_unique<Impl>())
{}

GgufTensorParallelLoader::~GgufTensorParallelLoader() = default;

// ============================================================================
// Open + parse header
// ============================================================================
bool GgufTensorParallelLoader::open()
{
    impl_->file.open(cfg_.gguf_path, std::ios::binary);
    if (!impl_->file) return false;

    if (!parseHeader()) return false;

    is_open_ = true;
    return true;
}

// ============================================================================
// parseHeader — reads magic, version, tensor info list, locates data section
// ============================================================================
bool GgufTensorParallelLoader::parseHeader()
{
    auto& f = impl_->file;

    auto read_u32 = [&](uint32_t& v) { return (bool)f.read((char*)&v, 4); };
    auto read_u64 = [&](uint64_t& v) { return (bool)f.read((char*)&v, 8); };
    auto read_str = [&](std::string& s) {
        uint64_t len = 0;
        if (!f.read((char*)&len, 8)) return false;
        if (len > 65536) return false;
        s.resize((size_t)len);
        return (bool)f.read(s.data(), (std::streamsize)len);
    };

    // Magic + version
    uint32_t magic = 0, ver = 0;
    if (!read_u32(magic) || magic != TPL_GGUF_MAGIC) return false;
    if (!read_u32(ver) || ver < TPL_GGUF_VER2 || ver > TPL_GGUF_VER3) return false;

    uint64_t n_tensors = 0, n_kv = 0;
    if (!read_u64(n_tensors)) return false;
    if (!read_u64(n_kv))     return false;

    // Skip KV metadata (same skip_value logic as lora_hotswap)
    // Minimal inline lambda to skip by type
    std::function<bool(uint32_t)> skip_val = [&](uint32_t vtype) -> bool {
        if (!f.good()) return false;
        switch (vtype) {
        case 0: case 1: case 7:  f.seekg(1, std::ios::cur); break;
        case 2: case 3:          f.seekg(2, std::ios::cur); break;
        case 4: case 5: case 6:  f.seekg(4, std::ios::cur); break;
        case 10:case 11:case 12: f.seekg(8, std::ios::cur); break;
        case 8: { // string
            uint64_t len = 0;
            if (!f.read((char*)&len, 8)) return false;
            f.seekg((std::streamoff)len, std::ios::cur);
            break;
        }
        case 9: { // array
            uint32_t et = 0; uint64_t cnt = 0;
            if (!f.read((char*)&et, 4)) return false;
            if (!f.read((char*)&cnt, 8)) return false;
            for (uint64_t i = 0; i < cnt; ++i)
                if (!skip_val(et)) return false;
            break;
        }
        default: return false;
        }
        return f.good();
    };

    for (uint64_t i = 0; i < n_kv; ++i) {
        std::string key;
        uint32_t vtype = 0;
        if (!read_str(key)) return false;
        if (!f.read((char*)&vtype, 4)) return false;
        if (!skip_val(vtype)) return false;
    }

    // Tensor info
    tensor_meta_.clear();
    tensor_meta_.reserve((size_t)n_tensors);

    for (uint64_t i = 0; i < n_tensors; ++i) {
        TensorMeta tm{};
        if (!read_str(tm.name)) return false;
        if (!f.read((char*)&tm.n_dims, 4)) return false;
        if (tm.n_dims > 4) return false;
        for (uint32_t d = 0; d < tm.n_dims; ++d)
            if (!read_u64(tm.dims[d])) return false;
        // Unused dims = 1
        for (uint32_t d = tm.n_dims; d < 4; ++d) tm.dims[d] = 1;
        if (!f.read((char*)&tm.ggml_rxd_type, 4)) return false;
        if (!read_u64(tm.file_offset)) return false;

        size_t idx = tensor_meta_.size();
        tensor_index_[tm.name] = idx;
        tensor_meta_.push_back(std::move(tm));
    }

    // Data section at next 32-byte alignment
    uint64_t header_end = (uint64_t)f.tellg();
    data_section_offset_ = (header_end + 31) & ~uint64_t(31);

    return f.good();
}

// ============================================================================
// bytesPerElement / blockSizeBytes / elementsPerBlock
// ============================================================================
int64_t GgufTensorParallelLoader::bytesPerElement(uint32_t type) const
{
    switch (type) {
    case TPL_F32:    return 4;
    case TPL_F16:    return 2;
    case TPL_I8:     return 1;
    case TPL_I16:    return 2;
    case TPL_I32:    return 4;
    default:         return 0; // quantized — use blockSizeBytes
    }
}

int64_t GgufTensorParallelLoader::blockSizeBytes(uint32_t type) const
{
    // Block sizes for each quantization format (bytes per super-block)
    switch (type) {
    case TPL_Q4_0:   return 2 + 16;       // 18 bytes / 32 elems
    case TPL_Q4_1:   return 4 + 16;       // 20 bytes / 32 elems
    case TPL_Q5_0:   return 2 + 4 + 16;   // 22 bytes / 32 elems
    case TPL_Q5_1:   return 4 + 4 + 16;   // 24 bytes / 32 elems
    case TPL_Q8_0:   return 2 + 32;       // 34 bytes / 32 elems
    case TPL_Q8_1:   return 4 + 32;       // 36 bytes / 32 elems
    case TPL_Q2_K:   return 256;          // 84 bytes (approx)
    case TPL_Q3_K_S: return 110;
    case TPL_Q3_K_M: return 138;
    case TPL_Q3_K_L: return 166;
    case TPL_Q4_K:   return 144;          // 144 bytes / 256 elems
    case TPL_Q5_K:   return 176;
    case TPL_Q6_K:   return 210;
    default:         return 0;
    }
}

int64_t GgufTensorParallelLoader::elementsPerBlock(uint32_t type) const
{
    switch (type) {
    case TPL_Q4_0: case TPL_Q4_1:
    case TPL_Q5_0: case TPL_Q5_1:
    case TPL_Q8_0: case TPL_Q8_1:
        return 32;
    case TPL_Q2_K: case TPL_Q3_K_S: case TPL_Q3_K_M: case TPL_Q3_K_L:
    case TPL_Q4_K: case TPL_Q5_K: case TPL_Q6_K:
        return 256;
    default:
        return 1; // scalar types
    }
}

// ============================================================================
// computeShardBounds — fills spec.{row_start, n_rows, n_cols, byte_offset, byte_size}
// ============================================================================
bool GgufTensorParallelLoader::computeShardBounds(
    const TensorMeta& meta, ShardSpec& spec)
{
    // In GGML tensors:
    //   dims[0] = columns (inner, fastest)
    //   dims[1] = rows    (outer, divided for row-parallel)
    // Total elements = dims[0] * dims[1] * dims[2] * dims[3]
    int64_t n_cols = (int64_t)meta.dims[0];
    int64_t n_rows = (int64_t)meta.dims[1];

    spec.n_cols = n_cols;

    if (spec.mode == ShardSpec::Mode::Row) {
        // Divide rows across ranks
        int64_t rows_per_rank = n_rows / spec.total_ranks;
        int64_t remainder     = n_rows % spec.total_ranks;
        spec.row_start = spec.rank * rows_per_rank
                       + std::min((int64_t)spec.rank, remainder);
        spec.n_rows    = rows_per_rank + (spec.rank < remainder ? 1 : 0);
    } else {
        // Col mode: divide input (column) dimension
        int64_t cols_per_rank = n_cols / spec.total_ranks;
        int64_t remainder     = n_cols % spec.total_ranks;
        int64_t col_start     = spec.rank * cols_per_rank
                              + std::min((int64_t)spec.rank, remainder);
        int64_t n_shard_cols  = cols_per_rank + (spec.rank < remainder ? 1 : 0);

        spec.row_start = 0;
        spec.n_rows    = n_rows;
        spec.n_cols    = n_shard_cols;

        // byte offset within a row
        int64_t bpe  = bytesPerElement(meta.ggml_rxd_type);
        int64_t bpb  = blockSizeBytes(meta.ggml_rxd_type);
        int64_t epb  = elementsPerBlock(meta.ggml_rxd_type);

        if (bpe > 0) {
            // Scalar type: straightforward
            spec.byte_offset = col_start * bpe;
            spec.byte_size   = spec.n_rows * n_shard_cols * bpe;
        } else if (bpb > 0 && epb > 0) {
            // Quantized: col_start must be block-aligned
            int64_t block_start = col_start / epb;
            int64_t n_blocks    = (n_shard_cols + epb - 1) / epb;
            spec.byte_offset    = block_start * bpb;
            spec.byte_size      = spec.n_rows * n_blocks * bpb;
        } else {
            return false;
        }
        return true;
    }

    // Row mode: compute byte bounds
    int64_t bpe  = bytesPerElement(meta.ggml_rxd_type);
    int64_t bpb  = blockSizeBytes(meta.ggml_rxd_type);
    int64_t epb  = elementsPerBlock(meta.ggml_rxd_type);
    int64_t row_bytes = 0;

    if (bpe > 0) {
        row_bytes = n_cols * bpe;
    } else if (bpb > 0 && epb > 0) {
        int64_t blocks_per_row = (n_cols + epb - 1) / epb;
        row_bytes = blocks_per_row * bpb;
    } else {
        return false;
    }

    spec.byte_offset = spec.row_start * row_bytes;
    spec.byte_size   = spec.n_rows    * row_bytes;
    return true;
}

// ============================================================================
// loadShard
// ============================================================================
std::optional<TensorParallelInfo> GgufTensorParallelLoader::loadShard(
    const std::string& tensor_name,
    ShardSpec::Mode mode)
{
    if (!is_open_) return std::nullopt;

    // Check prefetch cache first
    {
        std::lock_guard<std::mutex> lk(prefetch_mu_);
        auto it2 = prefetch_cache_.find(tensor_name);
        if (it2 != prefetch_cache_.end()) {
            auto ret = std::move(it2->second);
            prefetch_cache_.erase(it2);
            return ret;
        }
    }

    auto it = tensor_index_.find(tensor_name);
    if (it == tensor_index_.end()) return std::nullopt;

    const TensorMeta& meta = tensor_meta_[it->second];

    ShardSpec spec;
    spec.rank        = cfg_.this_rank;
    spec.total_ranks = cfg_.total_ranks;
    spec.mode        = mode;

    if (!computeShardBounds(meta, spec)) return std::nullopt;
    if (spec.byte_size <= 0) return std::nullopt;

    // Read from file
    uint64_t file_pos = data_section_offset_ + meta.file_offset + (uint64_t)spec.byte_offset;

    auto& f = impl_->file;
    f.seekg((std::streamoff)file_pos, std::ios::beg);
    if (!f) return std::nullopt;

    TensorParallelInfo info;
    info.name      = tensor_name;
    info.ggml_rxd_type = meta.ggml_rxd_type;
    info.n_rows    = spec.n_rows;
    info.n_cols    = spec.n_cols;
    info.shard     = spec;
    info.raw_bytes.resize((size_t)spec.byte_size);

    f.read((char*)info.raw_bytes.data(), (std::streamsize)spec.byte_size);
    if (!f && !f.eof()) return std::nullopt;

    // GPU upload hook
    if (gpu_upload_fn_) {
        gpu_upload_fn_(tensor_name,
                       info.raw_bytes.data(),
                       (int64_t)info.raw_bytes.size(),
                       cfg_.this_rank);
    }

    return info;
}

// ============================================================================
// prefetch (async — in this implementation, synchronous load into cache)
// ============================================================================
void GgufTensorParallelLoader::prefetch(const std::string& tensor_name,
                                         ShardSpec::Mode mode)
{
    auto result = loadShard(tensor_name, mode);
    if (!result) return;

    std::lock_guard<std::mutex> lk(prefetch_mu_);
    prefetch_cache_.emplace(tensor_name, std::move(*result));
}

// ============================================================================
// Collective operations
// ============================================================================
AllReduceResult GgufTensorParallelLoader::allReduce(float* buf, int64_t n_elems, int)
{
    // Single-node: partial sums already in buf from local GEMV shards.
    // In multi-node this would call the transport layer.
    // No-op for rank 0; other ranks would send and receive.
    AllReduceResult r;
    r.success = true;
    r.rank    = cfg_.this_rank;
    r.n_elems = n_elems;
    return r;
}

AllReduceResult GgufTensorParallelLoader::allGather(
    const float* partial, float* full,
    int64_t n_partial, int64_t n_full)
{
    // Single-node row-parallel gather: just copy partial to the right offset
    int64_t offset = cfg_.this_rank * n_partial;
    if (offset + n_partial > n_full) n_partial = n_full - offset;
    if (n_partial > 0)
        std::memcpy(full + offset, partial, (size_t)n_partial * sizeof(float));

    AllReduceResult r;
    r.success = true;
    r.rank    = cfg_.this_rank;
    r.n_elems = n_partial;
    return r;
}

// ============================================================================
// Inspection
// ============================================================================
size_t GgufTensorParallelLoader::tensorCount() const
{
    return tensor_meta_.size();
}

std::vector<std::string> GgufTensorParallelLoader::tensorNames() const
{
    std::vector<std::string> names;
    names.reserve(tensor_meta_.size());
    for (auto& tm : tensor_meta_) names.push_back(tm.name);
    return names;
}

} // namespace rawrxd
