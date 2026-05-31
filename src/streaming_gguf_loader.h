#pragma once
#include "gguf_loader.h"
#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <unordered_set>
#include <vector>

namespace RawrXD
{

class MappedWindowStreamer {};  // Placeholder until a concrete mapped streamer is wired in

struct TensorZoneInfo
{
    std::string zone_name;             // "embedding", "layers_0", "layers_1", etc.
    std::vector<std::string> tensors;  // Tensor names in this zone
    uint64_t total_bytes;              // Total size of all tensors in zone
    bool is_loaded;                    // Currently in RAM?
    std::vector<uint8_t> data;         // Actual tensor data (when loaded)
};

struct TensorRef
{
    std::string name;
    std::string zone_name;  // Which zone does this belong to?
    uint64_t offset;        // Byte offset in file
    uint64_t size;          // Size of this tensor
    uint64_t index;         // Index in tensor list
    GGMLType type;
    std::vector<uint64_t> shape;
};

class StreamingGGUFLoader : public IGGUFLoader
{
  public:
    StreamingGGUFLoader();
    ~StreamingGGUFLoader();

    // ---- File Opening (streams header, not data) ----
    bool Open(const std::string& filepath) override;
    bool Close() override;

    // ---- Header & Metadata (always in RAM) ----
    bool ParseHeader() override;
    GGUFHeader GetHeader() const override;
    bool ParseMetadata() override;
    GGUFMetadata GetMetadata() const override;

    // ---- Required interface methods ----
    std::vector<TensorInfo> GetTensorInfo() const override;
    bool LoadTensorZone(const std::string& tensor_name, std::vector<uint8_t>& data) override;
    bool LoadTensorRange(size_t start_idx, size_t count, std::vector<uint8_t>& data) override;
    const std::vector<std::string>& GetVocabulary() const override { return m_vocab; }

    size_t GetTensorByteSize(const TensorInfo& tensor) const override;
    std::string GetTypeString(GGMLType type) const override;
    uint64_t GetFileSize() const override;

    // ---- Streaming specific ----
    bool BuildTensorIndex();
    bool LoadZone(const std::string& zone_name, uint64_t max_memory_mb = 512);
    bool UnloadZone(const std::string& zone_name);
    std::vector<std::string> GetLoadedZones() const;
    std::vector<std::string> GetAllZones() const;
    std::vector<TensorInfo> GetAllTensorInfo() const override;
    uint64_t GetCurrentMemoryUsage() const override;

    // Internal access for debugging
    TensorZoneInfo GetZoneInfo(const std::string& zone_name) const;
    /// Tensor list in **GGUF file enumeration order** (same as `index` field), not name-sorted.
    std::vector<TensorRef> GetTensorIndex() const;
    std::string GetTensorZone(const std::string& tensor_name) const;

    // Access raw tensor data
    bool GetTensorData(const std::string& tensor_name, std::vector<uint8_t>& data);
    // Probe that tensor bytes are readable without materializing the full tensor in RAM.
    bool ProbeTensorData(const std::string& tensor_name, size_t sample_bytes = 4096);
    // Get total file size
    uint64_t GetTotalFileSize();

  private:
    std::string filepath_;
    std::ifstream file_;
    bool is_open_;
    
    // AUDIT_FIX_4: Memory-mapped fallback for large zones
    std::unique_ptr<class MappedWindowStreamer> mmap_streamer_;
    bool using_mmap_;

    GGUFHeader header_;
    GGUFMetadata metadata_;
    uint64_t tensor_info_offset_;  // Byte offset where tensor info section begins (after metadata)

    // Maps tensor_name → {offset, size, type, shape}
    std::map<std::string, TensorRef> tensor_index_;
    /// Same tensors as `tensor_index_`, in **on-disk GGUF order** (for custom loaders / layer walk).
    std::vector<TensorRef> tensor_enumeration_order_;
    std::vector<std::string> m_vocab;

    // ---- Zone Information ----
    // Maps zone_name → {tensors, total_bytes, is_loaded, data}
    std::map<std::string, TensorZoneInfo> zones_;

    // Currently loaded zones (can have multiple active zones for pre-loading)
    std::map<std::string, bool> active_zones_;
    std::map<std::string, uint64_t> tensor_generation_ids_;
    std::unordered_set<std::string> dirty_tensors_;
    std::string current_zone_;
    uint64_t current_zone_memory_;

    // ---- Configuration ----
    uint64_t max_zone_memory_mb_;  // How much RAM per zone? (512 MB default)
    uint64_t data_section_offset_; // GGUF tensor offsets are relative to tensor data section

    // ---- Internal Helpers ----

    // Assign tensors to zones based on name patterns
    void AssignTensorsToZones();

    // Load zone data from disk
    bool StreamZoneFromDisk(const std::string& zone_name);
    
    // AUDIT_FIX_4: Memory-mapped fallback when buffer allocation fails
    bool LoadZoneMapped(const std::string& zone_name);

    // Calculate which layer a tensor belongs to
    int32_t ExtractLayerNumber(const std::string& tensor_name) const;

    // Get zone for tensor name
    std::string GetZoneForTensor(const std::string& tensor_name) const;

    // Template reading
    template <typename T> bool ReadValue(T& value);
    bool ReadString(std::string& value);
    uint64_t CalculateTensorSize(const std::vector<uint64_t>& shape, GGMLType type) const;

    // ========================================================================
    // OVERLAPPED I/O PREFETCH ENGINE (Phase 7a — 4 GB/s NVMe saturation)
    // ========================================================================
    // Prefetch a zone using Win32 FILE_FLAG_OVERLAPPED + FILE_FLAG_NO_BUFFERING.
    // Falls back to synchronous LoadZone() if async handle cannot be opened.
    // Targets 3.98 GB/s sustained throughput on NVMe SSDs.
    // ========================================================================
    bool PrefetchZoneAsync(const std::string& zone_name, uint64_t max_memory_mb = 512);
};

}  // namespace RawrXD
