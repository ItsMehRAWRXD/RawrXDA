#pragma once
#include "gguf_loader.h"
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <cstdint>
#include <unordered_map>
#include <span>
#include <cstddef>

// ============================================================================
// STREAMING GGUF LOADER - Memory-efficient tensor loading with zone-based
// streaming (game engine style)
// ============================================================================

struct TensorZoneInfo {
    std::string zone_name;              // "embedding", "layers_0", "layers_1", etc.
    std::vector<std::string> tensors;   // Tensor names in this zone
    uint64_t total_bytes;               // Total size of all tensors in zone
    bool is_loaded;                     // Currently in RAM?
    std::vector<uint8_t> data;          // Actual tensor data (when loaded)
};

struct TensorRef {
    std::string name;
    std::string zone_name;              // Which zone does this belong to?
    uint64_t offset;                    // Byte offset in file
    uint64_t size;                      // Size of this tensor
    GGMLType type;
    std::vector<uint64_t> shape;
};

class StreamingGGUFLoader : public IGGUFLoader {
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
    bool LoadTensorRange(size_t start_idx, size_t count, std::vector<uint8_t>& data) override;
    size_t GetTensorByteSize(const TensorInfo& tensor) const override;
    std::string GetTypeString(GGMLType type) const override;
    
    // ---- Index Building (builds offset map, no tensor data loaded) ----
    bool BuildTensorIndex() override;   // Read all tensor info, calculate offsets
    std::vector<TensorRef> GetTensorIndex() const;  // Just metadata!
    
    // ---- ZONE-BASED LOADING (the core innovation) ----
    
    // Get which zone a tensor belongs to
    std::string GetTensorZone(const std::string& tensor_name) const;
    
    // Load a zone into RAM (unloads other zones if needed)
    bool LoadZone(const std::string& zone_name, uint64_t max_memory_mb = 512) override;
    
    // Unload a zone from RAM
    bool UnloadZone(const std::string& zone_name) override;
    
    // Access tensor data (loads zone if needed) - implements base class LoadTensorZone
    bool LoadTensorZone(const std::string& tensor_name, std::vector<uint8_t>& data) override;
    bool GetTensorData(const std::string& tensor_name, std::vector<uint8_t>& data); // Non-virtual alias
    
    // ---- ZERO-COPY ACCESS (Critical for 50μs target) ----
    // Returns view into zone memory without copy - zone must already be loaded
    std::span<const std::byte> GetTensorView(
        const std::string& tensor_name,
        size_t offset = 0,
        size_t length = SIZE_MAX
    );
    
    // Check if tensor is currently resident in memory
    bool IsTensorResident(const std::string& tensor_name) const;
    
    // Get raw pointer to zone data (for low-level access)
    const std::byte* GetZonePointer(const std::string& zone_name) const;
    
    // Get zone info (what's loaded, what's not)
    TensorZoneInfo GetZoneInfo(const std::string& zone_name) const;
    
    // ---- Utility ----
    uint64_t GetFileSize() const override;
    uint64_t GetTotalFileSize() const; // Non-virtual alias for compatibility
    uint64_t GetCurrentMemoryUsage() const override;
    std::vector<std::string> GetLoadedZones() const override;
    std::vector<std::string> GetAllZones() const override;
    
    // For compatibility with old loader
    std::vector<TensorInfo> GetAllTensorInfo() const override;
    
private:
    // ---- File Handle (kept open for streaming) ----
    std::string filepath_;
    mutable std::ifstream file_;
    bool is_open_;
    
    // ---- Metadata (always in RAM, ~50-100 MB) ----
    GGUFHeader header_;
    GGUFMetadata metadata_;
    
    // ---- Tensor Index (always in RAM, ~40 MB) ----
    // Maps tensor_name → {offset, size, type, shape}
    std::map<std::string, TensorRef> tensor_index_;
    
    // ---- Zone Information ----
    // Maps zone_name → {tensors, total_bytes, is_loaded, data}
    std::map<std::string, TensorZoneInfo> zones_;
    
    // Currently loaded zones (can have multiple active zones for pre-loading)
    std::map<std::string, bool> active_zones_;
    std::string current_zone_;
    uint64_t current_zone_memory_;
    
    // ---- Configuration ----
    uint64_t max_zone_memory_mb_;       // How much RAM per zone? (512 MB default)
    bool verbose_{false};
    
    // ---- Internal Helpers ----
    
    // Assign tensors to zones based on name patterns
    void AssignTensorsToZones();
    void AdjustMemoryBudget();
    
    // Load zone data from disk
    bool StreamZoneFromDisk(const std::string& zone_name);
    
    // Calculate which layer a tensor belongs to
    int32_t ExtractLayerNumber(const std::string& tensor_name) const;
    
    // Get zone for tensor name
    std::string GetZoneForTensor(const std::string& tensor_name) const;
    
    // Template reading
    template<typename T>
    bool ReadValue(T& value);
    bool ReadString(std::string& value);
    uint64_t CalculateTensorSize(const std::vector<uint64_t>& shape, GGMLType type) const;
    bool ReadTensorSlice(const TensorRef& ref, std::vector<uint8_t>& data) const;
    uint64_t GetZoneBudgetBytes(uint64_t override_mb = 0) const;
    uint64_t ParseBudgetEnv() const;
};
