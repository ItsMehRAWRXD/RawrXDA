#include "query_fusion_kernel.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <string>

namespace RawrXD {
namespace Fusion {

static void weighted_fusion_fallback(
    const float* score_matrix,
    const float* weights,
    uint32_t sub_query_count,
    uint32_t candidate_count,
    float* out_scores) noexcept {
    std::fill(out_scores, out_scores + candidate_count, 0.0f);
    for (uint32_t i = 0; i < sub_query_count; ++i) {
        const float w = weights[i];
        const float* row = score_matrix + (static_cast<size_t>(i) * candidate_count);
        for (uint32_t c = 0; c < candidate_count; ++c) {
            out_scores[c] += row[c] * w;
        }
    }
}

static void apply_penalty_phrase_gate_fallback(
    float* scores,
    const uint32_t* missing_term_counts,
    const uint32_t* phrase_gate_failures,
    uint32_t candidate_count,
    float penalty_per_missing,
    float clamp_max) noexcept {
    for (uint32_t i = 0; i < candidate_count; ++i) {
        const float penalty = static_cast<float>(missing_term_counts[i]) * penalty_per_missing;
        float score = std::max(0.0f, scores[i] - penalty);
        if (phrase_gate_failures[i] != 0) {
            score = std::min(score, clamp_max);
        }
        scores[i] = score;
    }
}

static bool weighted_fusion_ew75_p15_fallback(
    const float* score_matrix,
    uint32_t candidate_count,
    float* out_scores) noexcept {
    if (!score_matrix || !out_scores || candidate_count == 0) {
        return false;
    }

    const float* row0 = score_matrix;
    const float* row1 = score_matrix + candidate_count;
    for (uint32_t c = 0; c < candidate_count; ++c) {
        out_scores[c] = row0[c] * 0.75f + row1[c] * 0.25f;
    }
    return true;
}

static bool is_ew75_p15_profile(
    const float* weights,
    uint32_t sub_query_count) noexcept {
    if (!weights || sub_query_count != 2) {
        return false;
    }

    constexpr float kTol = 1.0e-3f;
    return (std::fabs(weights[0] - 0.75f) <= kTol) &&
           (std::fabs(weights[1] - 0.25f) <= kTol);
}

bool WeightedFusion(
    const float* score_matrix,
    const float* weights,
    uint32_t sub_query_count,
    uint32_t candidate_count,
    std::vector<float>& out_scores) noexcept {
    if (!score_matrix || !weights || sub_query_count == 0 || candidate_count == 0) {
        return false;
    }

    out_scores.assign(candidate_count, 0.0f);
    if (out_scores.empty()) {
        return false;
    }

    if (is_ew75_p15_profile(weights, sub_query_count)) {
        if (rawr_weighted_fusion_ew75_p15_avx512(
                score_matrix,
                score_matrix + candidate_count,
                candidate_count,
                out_scores.data()) == 0) {
            if (!weighted_fusion_ew75_p15_fallback(score_matrix, candidate_count, out_scores.data())) {
                return false;
            }
        }
        return true;
    }

    if (rawr_weighted_fusion_fma_avx512(
            score_matrix,
            weights,
            sub_query_count,
            candidate_count,
            out_scores.data()) == 0) {
        weighted_fusion_fallback(score_matrix, weights, sub_query_count, candidate_count, out_scores.data());
    }

    return true;
}

bool ApplyPenaltyAndPhraseGate(
    std::span<float> fused_scores,
    std::span<const uint32_t> missing_term_counts,
    std::span<const uint32_t> phrase_gate_failures,
    float penalty_per_missing,
    float clamp_max) noexcept {
    if (fused_scores.empty()) {
        return true;
    }
    if (missing_term_counts.size() < fused_scores.size() || phrase_gate_failures.size() < fused_scores.size()) {
        return false;
    }

    if (rawr_apply_penalty_phrase_gate_avx512(
            fused_scores.data(),
            missing_term_counts.data(),
            phrase_gate_failures.data(),
            static_cast<uint32_t>(fused_scores.size()),
            penalty_per_missing,
            clamp_max) == 0) {
        apply_penalty_phrase_gate_fallback(
            fused_scores.data(),
            missing_term_counts.data(),
            phrase_gate_failures.data(),
            static_cast<uint32_t>(fused_scores.size()),
            penalty_per_missing,
            clamp_max);
    }

    return true;
}

static bool is_word_char(char ch) noexcept {
    const unsigned char c = static_cast<unsigned char>(ch);
    return std::isalnum(c) != 0 || c == '_';
}

static std::string to_lower_copy(std::string_view in) {
    std::string out;
    out.reserve(in.size());
    for (char ch : in) {
        out.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
    }
    return out;
}

static bool contains_exact_term_ci(std::string_view text, std::string_view term) {
    if (term.empty()) {
        return true;
    }

    std::string hay = to_lower_copy(text);
    std::string needle = to_lower_copy(term);

    size_t pos = 0;
    while (true) {
        pos = hay.find(needle, pos);
        if (pos == std::string::npos) {
            return false;
        }

        const bool left_ok = (pos == 0) || !is_word_char(hay[pos - 1]);
        const size_t end = pos + needle.size();
        const bool right_ok = (end >= hay.size()) || !is_word_char(hay[end]);
        if (left_ok && right_ok) {
            return true;
        }

        ++pos;
    }
}

void ApplyHardMatchPenalty(
    std::span<float> fused_scores,
    std::span<const std::string_view> candidate_texts,
    std::span<const std::string_view> required_terms,
    float penalty_per_missing) noexcept {
    if (fused_scores.empty() || candidate_texts.empty()) {
        return;
    }

    const size_t n = std::min(fused_scores.size(), candidate_texts.size());
    std::vector<uint32_t> missing_counts(n, 0);
    std::vector<uint32_t> phrase_gate_failures(n, 0);
    for (size_t i = 0; i < n; ++i) {
        unsigned int missing = 0;
        for (const std::string_view term : required_terms) {
            if (!contains_exact_term_ci(candidate_texts[i], term)) {
                ++missing;
            }
        }
        missing_counts[i] = missing;
    }

    ApplyPenaltyAndPhraseGate(
        fused_scores.subspan(0, n),
        std::span<const uint32_t>(missing_counts.data(), missing_counts.size()),
        std::span<const uint32_t>(phrase_gate_failures.data(), phrase_gate_failures.size()),
        penalty_per_missing,
        1.0f);
}

}  // namespace Fusion
}  // namespace RawrXD
