#pragma once

#include <cstdint>
#include <string>
#include <unordered_set>
#include <vector>

namespace RawrXD
{

enum class LoadDecision : uint8_t
{
    LOAD,
    LAZY_LOAD,
    SKIP
};

enum class TensorZoneType : uint8_t
{
    EMBEDDING = 0,
    OUTPUT_HEAD = 1,
    OUTPUT_NORM = 2,
    ATTENTION_QKV = 3,
    ATTENTION_OUT = 4,
    ATTENTION_NORM = 5,
    FFN_UP = 6,
    FFN_DOWN = 7,
    FFN_GATE = 8,
    FFN_NORM = 9,
    UNKNOWN = 255
};

struct ZoneClassRule
{
    TensorZoneType type = TensorZoneType::UNKNOWN;
    std::vector<std::string> patterns;  // glob-style
    LoadDecision defaultDecision = LoadDecision::LAZY_LOAD;
    uint8_t priority = 255;  // lower preloads earlier
};

// Default rules for LLaMA-style models (RawrXD naming patterns).
const std::vector<ZoneClassRule>& getDefaultZoneRules();

// Glob match: '*' any sequence, '?' single char. (case sensitive)
bool globMatch(const std::string& pattern, const std::string& text);

TensorZoneType classifyTensor(const std::string& tensorName, const std::vector<ZoneClassRule>& rules,
                              uint8_t& outPriority, LoadDecision& outDefaultDecision);

// Zone-first filtering policy (bunny-hop).
// This filters *tensors* but is expressed in terms of zones and layer ranges.
struct TensorFilter
{
    // If non-empty, only these zones are considered.
    std::unordered_set<std::string> allowZones;

    // If non-empty, these zones are always skipped.
    std::unordered_set<std::string> denyZones;

    // Optional layer range gate. Interprets "blk.<n>." pattern as layer tensors.
    // (Avoid std::optional for toolchain/clangd compatibility across lanes.)
    bool hasLayerRange = false;
    int32_t layerStartInclusive = 0;
    int32_t layerEndInclusive = 0;

    // Defaults
    LoadDecision defaultDecision = LoadDecision::LAZY_LOAD;

    // Basic helper used by GGufTensorLoader.
    LoadDecision decide(const std::string& tensorName, const std::string& zoneName, uint64_t bytes,
                        int32_t layerIdx) const;
};

}  // namespace RawrXD
