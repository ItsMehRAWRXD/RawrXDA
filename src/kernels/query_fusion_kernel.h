#pragma once

#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

namespace RawrXD {
namespace Fusion {

extern "C" int rawr_weighted_fusion_fma_avx512(
    const float* score_matrix,
    const float* weights,
    unsigned int sub_query_count,
    unsigned int candidate_count,
    float* out_scores);

extern "C" int rawr_apply_penalty_phrase_gate_avx512(
    float* scores,
    const uint32_t* missing_term_counts,
    const uint32_t* phrase_gate_failures,
    unsigned int candidate_count,
    float penalty_per_missing,
    float clamp_max);

extern "C" int rawr_weighted_fusion_ew75_p15_avx512(
    const float* score_row0,
    const float* score_row1,
    unsigned int candidate_count,
    float* out_scores);

bool WeightedFusion(
    const float* score_matrix,
    const float* weights,
    uint32_t sub_query_count,
    uint32_t candidate_count,
    std::vector<float>& out_scores) noexcept;

bool ApplyPenaltyAndPhraseGate(
    std::span<float> fused_scores,
    std::span<const uint32_t> missing_term_counts,
    std::span<const uint32_t> phrase_gate_failures,
    float penalty_per_missing,
    float clamp_max) noexcept;

void ApplyHardMatchPenalty(
    std::span<float> fused_scores,
    std::span<const std::string_view> candidate_texts,
    std::span<const std::string_view> required_terms,
    float penalty_per_missing) noexcept;

}  // namespace Fusion
}  // namespace RawrXD
