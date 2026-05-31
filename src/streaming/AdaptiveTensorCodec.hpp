#pragma once

#include <cstdint>
#include <vector>
#include <string>
#include <map>
#include <windows.h>

namespace rawrxd {

enum class TensorLOD { L0_Q4, L1_REFINED, L2_FULL };

struct TileMetadata {
    uint64_t offset_l0;
    uint32_t size_l0;
    uint64_t offset_l1;
    uint32_t size_l1;
    float    scale;
    int32_t  zero_point;
};

class AdaptiveTensorCodec {
public:
    AdaptiveTensorCodec() {}
    ~AdaptiveTensorCodec() {}

    bool OpenModel(const std::wstring& model_path) { return true; }
    void CloseModel() {}
    const void* GetTileData(const std::string& tensor_name, uint32_t tile_index, TensorLOD lod) { return nullptr; }
    const TileMetadata* GetTileMetadata(const std::string& tensor_name, uint32_t tile_index) const { return nullptr; }
    void PrefetchTensor(const std::string& tensor_name) {}
    void DiscardTensor(const std::string& tensor_name) {}

private:
    bool BuildTileIndex() { return true; }
};

} // namespace rawrxd
