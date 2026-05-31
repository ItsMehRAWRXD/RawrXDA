#include "tensor_filter.h"

namespace RawrXD
{
namespace
{

static bool containsZone_(const std::unordered_set<std::string>& s, const std::string& z)
{
    return s.find(z) != s.end();
}

}  // namespace

bool globMatch(const std::string& pattern, const std::string& text)
{
    // Non-recursive wildcard match.
    // '*' matches any sequence (including empty), '?' matches exactly one char.
    size_t p = 0;
    size_t t = 0;
    size_t star = std::string::npos;
    size_t starText = 0;

    while (t < text.size())
    {
        if (p < pattern.size() && (pattern[p] == '?' || pattern[p] == text[t]))
        {
            ++p;
            ++t;
            continue;
        }
        if (p < pattern.size() && pattern[p] == '*')
        {
            star = p++;
            starText = t;
            continue;
        }
        if (star != std::string::npos)
        {
            p = star + 1;
            t = ++starText;
            continue;
        }
        return false;
    }

    while (p < pattern.size() && pattern[p] == '*')
        ++p;
    return p == pattern.size();
}

const std::vector<ZoneClassRule>& getDefaultZoneRules()
{
    static const std::vector<ZoneClassRule> rules = {
        {TensorZoneType::EMBEDDING,
         {"token_embd.weight", "model.embed_tokens.weight", "position_embd.weight", "rope_*.weight"},
         LoadDecision::LOAD,
         0},

        {TensorZoneType::OUTPUT_NORM,
         {"output_norm.weight", "model.norm.weight", "ln_f.weight"},
         LoadDecision::LOAD,
         1},
        {TensorZoneType::OUTPUT_HEAD, {"output.weight", "lm_head.weight"}, LoadDecision::LOAD, 2},

        {TensorZoneType::ATTENTION_NORM, {"blk.*.attn_norm.weight", "blk.*.ln1.weight"}, LoadDecision::LOAD, 3},
        {TensorZoneType::ATTENTION_QKV,
         {"blk.*.attn_q.weight", "blk.*.attn_k.weight", "blk.*.attn_v.weight", "blk.*.attn_qkv.weight"},
         LoadDecision::LOAD,
         4},
        {TensorZoneType::ATTENTION_OUT, {"blk.*.attn_output.weight", "blk.*.attn_out.weight"}, LoadDecision::LOAD, 5},

        {TensorZoneType::FFN_NORM, {"blk.*.ffn_norm.weight", "blk.*.ln2.weight"}, LoadDecision::LOAD, 6},
        {TensorZoneType::FFN_UP, {"blk.*.ffn_up.weight", "blk.*.fc1.weight"}, LoadDecision::LAZY_LOAD, 7},
        {TensorZoneType::FFN_GATE, {"blk.*.ffn_gate.weight"}, LoadDecision::LAZY_LOAD, 7},
        {TensorZoneType::FFN_DOWN, {"blk.*.ffn_down.weight", "blk.*.fc2.weight"}, LoadDecision::LAZY_LOAD, 8},
    };
    return rules;
}

TensorZoneType classifyTensor(const std::string& tensorName, const std::vector<ZoneClassRule>& rules,
                              uint8_t& outPriority, LoadDecision& outDefaultDecision)
{
    outPriority = 255;
    outDefaultDecision = LoadDecision::LAZY_LOAD;

    for (const auto& r : rules)
    {
        for (const auto& pat : r.patterns)
        {
            if (globMatch(pat, tensorName))
            {
                outPriority = r.priority;
                outDefaultDecision = r.defaultDecision;
                return r.type;
            }
        }
    }

    return TensorZoneType::UNKNOWN;
}

LoadDecision TensorFilter::decide(const std::string& /*tensorName*/, const std::string& zoneName, uint64_t /*bytes*/,
                                  int32_t layerIdx) const
{
    if (!allowZones.empty() && !containsZone_(allowZones, zoneName))
        return LoadDecision::SKIP;
    if (!denyZones.empty() && containsZone_(denyZones, zoneName))
        return LoadDecision::SKIP;

    if (hasLayerRange)
    {
        if (layerIdx < 0)
        {
            // Non-layer tensors are still allowed unless zones are filtered out.
        }
        else
        {
            if (layerIdx < layerStartInclusive)
                return LoadDecision::SKIP;
            if (layerIdx > layerEndInclusive)
                return LoadDecision::SKIP;
        }
    }

    return defaultDecision;
}

}  // namespace RawrXD
