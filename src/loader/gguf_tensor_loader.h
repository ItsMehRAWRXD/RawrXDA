#pragma once

#include "../streaming_gguf_loader.h"
#include "../utils/Expected.h"
#include "memory_budget.h"
#include "tensor_filter.h"


#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace RawrXD
{

enum class GGufTensorLoadErrorCode : uint8_t
{
    Ok = 0,
    InvalidArgument,
    TensorNotFound,
    FilteredOut,
    ZoneConflict,
    BudgetExceeded,
    LoaderError
};

struct GGufTensorLoadError
{
    GGufTensorLoadErrorCode code = GGufTensorLoadErrorCode::Ok;
    std::string message;
};

// Bunny-hop tensor loader: makes load decisions using TensorFilter + MemoryBudget,
// and drives zone residency via StreamingGGUFLoader::LoadZone/UnloadZone.
class GGufTensorLoader
{
  public:
    struct Config
    {
        TensorFilter filter;
        MemoryBudget budget;

        // Optional classification rules. If empty, defaults are used.
        std::vector<ZoneClassRule> zoneRules;

        // If true, LoadDecision::LAZY_LOAD means "do not pre-load in plan stage".
        bool lazyLoad = true;

        // When true, emits decision logs.
        bool logDecisions = true;
    };

    struct LoadPlanItem
    {
        std::string tensorName;
        std::string zoneName;
        uint64_t bytes = 0;
        int32_t layerIdx = -1;
        LoadDecision decision = LoadDecision::SKIP;

        TensorZoneType zoneType = TensorZoneType::UNKNOWN;
        uint8_t priority = 255;
        uint64_t zoneTotalBytes = 0;
    };

    struct PreloadResult
    {
        size_t zonesLoaded = 0;
        size_t zonesSkipped = 0;
        uint64_t bytesResident = 0;
        uint64_t bytesLazy = 0;
        std::vector<std::string> failedZones;
        std::vector<std::string> demotedTensors;
    };

    explicit GGufTensorLoader(StreamingGGUFLoader& loader, Config config);

    RawrXD::Expected<std::vector<LoadPlanItem>, GGufTensorLoadError> buildPlan();

    RawrXD::Expected<PreloadResult, GGufTensorLoadError> executePreload();

    // Ensures the tensor's zone is resident according to budget (bunny-hop eviction).
    RawrXD::Expected<void, GGufTensorLoadError> ensureTensorAvailable(const std::string& tensorName);

    // Convenience: loads tensor bytes (still via StreamingGGUFLoader for actual I/O).
    RawrXD::Expected<std::vector<uint8_t>, GGufTensorLoadError> loadTensorBytes(const std::string& tensorName);

  private:
    StreamingGGUFLoader& m_loader;
    Config m_cfg;

    // Cached plan by tensor name.
    std::unordered_map<std::string, LoadPlanItem> m_plan;
    std::unordered_map<std::string, uint64_t> m_zoneTotalBytes;

    // Resident zone tracking (best effort; StreamingGGUFLoader also tracks this internally).
    std::unordered_map<std::string, uint64_t> m_residentZonesBytes;

    static int32_t extractLayerIdx_(const std::string& tensorName);
    static uint64_t mbToBytes_(uint64_t mb) { return mb * 1024ull * 1024ull; }

    RawrXD::Expected<LoadPlanItem, GGufTensorLoadError> getPlanFor_(const std::string& tensorName);
    RawrXD::Expected<void, GGufTensorLoadError> ensureZoneResident_(const std::string& zoneName, uint64_t zoneBytes);
    uint64_t currentResidentBytes_() const;
};

}  // namespace RawrXD
