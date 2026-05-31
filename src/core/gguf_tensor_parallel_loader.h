#pragma once
// ============================================================================
// gguf_tensor_parallel_loader.h — GGUF Physical Shard Loader
//
// Given a GGUF file and a tensor parallel partition spec, seeks to the correct
// byte offset in the file and reads only the shard owned by this rank.
//
// Features:
//   - File I/O: seeks to tensor data offset + per-rank row slice offset
//   - Row-parallel (output dim split) and column-parallel (input dim split)
//   - Type-aware byte width (F32, Q4_0, Q4_K, Q6_K, etc.)
//   - allreduce / allgather stubs (single-node: operate in-place)
//   - DML / GPU upload hook
//   - Async prefetch per shard
// ============================================================================
#include <cstdint>
#include <string>
#include <vector>
#include <functional>
#include <optional>
#include <memory>
#include <mutex>

namespace rawrxd {

// ============================================================================
// ShardSpec — describes one rank's slice of a tensor
// ============================================================================
struct ShardSpec {
    int32_t rank;           // which rank this shard belongs to
    int32_t total_ranks;    // total number of ranks

    // Partition type
    enum class Mode {
        Row,    // slice output dimension  (rows of weight matrix)
        Col,    // slice input  dimension  (columns of weight matrix)
    } mode = Mode::Row;

    // Resolved coordinates (filled by the loader)
    int64_t row_start = 0;
    int64_t n_rows    = 0;   // number of rows this rank owns
    int64_t n_cols    = 0;   // full column count (Row mode) or shard cols (Col mode)
    int64_t byte_offset = 0; // offset within the tensor's raw data
    int64_t byte_size   = 0; // bytes to read for this shard
};

// ============================================================================
// TensorParallelInfo — per-tensor load result
// ============================================================================
struct TensorParallelInfo {
    std::string name;
    uint32_t    ggml_rxd_type;
    int64_t     n_rows;
    int64_t     n_cols;
    ShardSpec   shard;
    std::vector<uint8_t> raw_bytes; // shard data in original quantization format
};

// ============================================================================
// AllReduceResult — returned by collective operations
// ============================================================================
struct AllReduceResult {
    bool     success = false;
    int32_t  rank    = 0;
    int64_t  n_elems = 0;
};

// ============================================================================
// GgufTensorParallelLoader
//
// Usage:
//   GgufTensorParallelLoader loader("model.gguf", this_rank, total_ranks);
//   auto info = loader.loadShard("blk.0.attn_q.weight", ShardSpec::Mode::Row);
//   // info.raw_bytes holds the quantized shard for this rank
//   loader.allReduce(output_buf, n_elems);        // sum across ranks
//   loader.allGather(partial, full, n_partial);   // gather across ranks
// ============================================================================
class GgufTensorParallelLoader {
public:
    struct Config {
        std::string gguf_path;
        int32_t     this_rank    = 0;
        int32_t     total_ranks  = 1;
        bool        prefetch_async = false; // async prefetch next tensor
        size_t      io_buf_size  = 4 * 1024 * 1024; // 4 MB read buffer
    };

    explicit GgufTensorParallelLoader(const Config& cfg);
    ~GgufTensorParallelLoader();

    // Open and parse the GGUF header (call once).
    // Returns false on I/O or format error.
    bool open();

    // Load a shard for a named tensor.
    // Returns nullopt if the tensor is not found.
    std::optional<TensorParallelInfo> loadShard(
        const std::string& tensor_name,
        ShardSpec::Mode    mode = ShardSpec::Mode::Row);

    // Prefetch a shard asynchronously (result available in prefetch cache).
    void prefetch(const std::string& tensor_name,
                  ShardSpec::Mode    mode = ShardSpec::Mode::Row);

    // -------------------------------------------------------------------------
    // Collective operations (single-node: in-place sum / identity gather)
    // -------------------------------------------------------------------------

    // allReduce: sum fp32 partials across ranks.
    // Single-node: no-op (caller should already have partial sums from local shards).
    // Multi-node: would call MPI/NCCL allreduce — stubbed as in-place for now.
    AllReduceResult allReduce(float* buf, int64_t n_elems,
                              int root_rank = 0);

    // allGather: collect row-parallel output shards into full output.
    // Single-node: copies partial to full at its rank offset.
    AllReduceResult allGather(const float* partial, float* full,
                              int64_t n_partial, int64_t n_full);

    // -------------------------------------------------------------------------
    // GPU upload hook
    // -------------------------------------------------------------------------
    // Called after shard load if registered; intended for async DML upload.
    using GpuUploadFunc = std::function<void(
        const std::string& tensor_name,
        const uint8_t* data, int64_t bytes,
        int32_t rank)>;
    void onGpuUpload(GpuUploadFunc fn) { gpu_upload_fn_ = std::move(fn); }

    // -------------------------------------------------------------------------
    // Inspection
    // -------------------------------------------------------------------------
    bool          isOpen()    const noexcept { return is_open_; }
    int32_t        thisRank()  const noexcept { return cfg_.this_rank; }
    int32_t        totalRanks()const noexcept { return cfg_.total_ranks; }
    size_t         tensorCount() const;
    std::vector<std::string> tensorNames() const;

private:
    struct TensorMeta {
        std::string name;
        uint32_t    ggml_rxd_type;
        uint32_t    n_dims;
        uint64_t    dims[4];
        uint64_t    file_offset; // bytes from start of data section
    };

    bool parseHeader();
    bool computeShardBounds(const TensorMeta& meta, ShardSpec& spec);
    int64_t bytesPerElement(uint32_t ggml_rxd_type) const;
    int64_t blockSizeBytes(uint32_t ggml_rxd_type) const;
    int64_t elementsPerBlock(uint32_t ggml_rxd_type) const;

    Config cfg_;
    bool   is_open_ = false;

    uint64_t data_section_offset_ = 0;
    std::vector<TensorMeta> tensor_meta_;
    std::unordered_map<std::string, size_t> tensor_index_;

    // Optional async prefetch cache
    mutable std::mutex prefetch_mu_;
    std::unordered_map<std::string, TensorParallelInfo> prefetch_cache_;

    GpuUploadFunc gpu_upload_fn_;

    // File handle kept open for repeated reads
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace rawrxd
