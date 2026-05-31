#pragma once

#include "../RawrXD_Interfaces.h"
#include "../memory/ai_memory_strategies.hpp"
#include "../memory/memory_morph_controller.h"
#include "../memory/memory_oracle.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace RawrXD::ExtensionKernel {

struct ControllerRequestContext {
    uint64_t context_hash = 0;
    uint64_t file_hash = 0;
    uint64_t symbol_hash = 0;
    std::vector<int32_t> prompt_tokens;
    std::vector<int32_t> ast_allowed_tokens;
    std::vector<int32_t> prefix_hint_tokens;
    uint32_t max_tokens = 32;
    uint32_t flags = 0;
};

struct ControllerMetrics {
    uint32_t cache_hit = 0;
    uint32_t kv_stitch_count = 0;
    uint32_t tokens_generated = 0;
    uint32_t tokens_accepted = 0;
    uint32_t verify_rejects = 0;
    uint32_t spec_depth = 0;
    uint32_t spec_heads = 0;
    uint32_t spec_heads_pruned = 0;
};

struct ControllerResult {
    std::vector<int32_t> tokens;
    float acceptance_rate = 0.0f;
    float speedup_estimate = 1.0f;
    bool cache_hit = false;
    uint32_t stitched_segments = 0;
    ControllerMetrics metrics;
};

class InferenceController {
public:
    InferenceController(InferenceEngine* draft, InferenceEngine* target)
        : m_draft(draft), m_target(target) {}

    std::vector<ControllerMetrics> telemetrySnapshot() const {
        std::vector<ControllerMetrics> out;
        const uint64_t count = std::min<uint64_t>(m_metrics_count, kMetricsRingSize);
        out.reserve(static_cast<size_t>(count));
        const uint64_t start = (m_metrics_count > kMetricsRingSize)
            ? (m_metrics_write + kMetricsRingSize - count) % kMetricsRingSize
            : 0;
        for (uint64_t i = 0; i < count; ++i) {
            out.push_back(m_metrics_ring[(start + i) % kMetricsRingSize]);
        }
        return out;
    }

    ControllerResult run(const ControllerRequestContext& ctx) {
        ControllerResult out;
        if (!m_draft || !m_target || ctx.prompt_tokens.empty()) {
            return out;
        }

        ++m_morph.totalRequests;

        // Feed request-level access signal into global memory orchestrator.
        m_mem_orchestrator.onAccess(
            ctx.context_hash,
            ctx.prompt_tokens.size() * sizeof(int32_t),
            0.85f,
            0.25f);

        if (auto hit = lookupTokenCache(ctx.context_hash)) {
            out.tokens.assign(hit->tokens.begin(), hit->tokens.begin() + hit->length);
            out.cache_hit = true;
            ++m_morph.cacheHits;
            out.acceptance_rate = 1.0f;
            out.speedup_estimate = 4.0f;
            out.metrics.cache_hit = 1;
            out.metrics.tokens_generated = hit->length;
            out.metrics.tokens_accepted = hit->length;
            out.metrics.spec_depth = static_cast<uint32_t>(std::max(0, m_spec.current_depth));
            pushMetrics(out.metrics);
            maybeRunMorphTick();
            return out;
        }

        std::vector<int32_t> working = ctx.prompt_tokens;
        out.stitched_segments = stitchContext(ctx.file_hash, ctx.symbol_hash, working);
        out.metrics.kv_stitch_count = out.stitched_segments;

        if ((ctx.flags & kFlagSyntaxNoise) != 0) {
            m_spec.current_depth = std::max(2, m_spec.current_depth / 2);
        }

        int depth = speculationEnabled() ? std::clamp(m_spec.current_depth, 2, 16) : 0;
        int spec_heads = depth > 0 ? activeSpecHeads(ctx, depth) : 0;
        const int hard_max = static_cast<int>(std::max<uint32_t>(1, std::min<uint32_t>(ctx.max_tokens, 32)));

        while (static_cast<int>(out.tokens.size()) < hard_max) {
            if (depth <= 0) {
                std::vector<int32_t> fallback = m_target->Generate(working, 1);
                if (fallback.empty()) break;
                out.tokens.push_back(fallback[0]);
                working.push_back(fallback[0]);
                out.metrics.tokens_generated += 1;
                out.metrics.tokens_accepted += 1;
                break;
            }

            std::vector<int32_t> draft = m_draft->Generate(working, depth);
            if (draft.empty()) {
                std::vector<int32_t> fallback = m_target->Generate(working, 1);
                if (fallback.empty()) break;
                out.tokens.push_back(fallback[0]);
                working.push_back(fallback[0]);
                out.metrics.tokens_generated += 1;
                out.metrics.tokens_accepted += 1;
                break;
            }
            if (static_cast<int>(draft.size()) > depth) {
                draft.resize(depth);
            }

            applyPrefixTrieBias(ctx.prefix_hint_tokens, draft, working);
            applyAstMaskAndFastPath(ctx.ast_allowed_tokens, working, draft);

            uint32_t pruned_heads = 0;
            std::vector<SpecHead> candidate_heads = makeParallelHeads(ctx, working, draft, spec_heads, pruned_heads);
            std::vector<int32_t> target = m_target->Generate(working, static_cast<int>(draft.size()) + 1);
            size_t best_head = 0;
            const int accepted = chooseBestHead(candidate_heads, target, best_head);
            draft = candidate_heads.empty() ? draft : std::move(candidate_heads[best_head].tokens);
            out.metrics.spec_heads = std::max<uint32_t>(out.metrics.spec_heads,
                static_cast<uint32_t>(std::max<size_t>(1, candidate_heads.size())));
            out.metrics.spec_heads_pruned += pruned_heads;

            out.metrics.tokens_generated += static_cast<uint32_t>(draft.size());
            out.metrics.tokens_accepted += static_cast<uint32_t>(std::max(0, accepted));
            out.metrics.verify_rejects += static_cast<uint32_t>(draft.size() - static_cast<size_t>(std::max(0, accepted)));

            for (int i = 0; i < accepted && static_cast<int>(out.tokens.size()) < hard_max; ++i) {
                out.tokens.push_back(draft[i]);
                working.push_back(draft[i]);
            }

            m_spec.draft_total += static_cast<uint32_t>(draft.size());
            m_spec.accepted_total += static_cast<uint32_t>(std::max(0, accepted));

            if (static_cast<int>(out.tokens.size()) >= hard_max) {
                break;
            }

            if (!target.empty()) {
                const int idx = std::min<int>(accepted, static_cast<int>(target.size()) - 1);
                const int32_t correction = chooseMaskedCorrection(target[idx], ctx.ast_allowed_tokens, working);
                out.tokens.push_back(correction);
                working.push_back(correction);
                out.metrics.tokens_generated += 1;
            }

            adaptDepth();
            depth = speculationEnabled() ? std::clamp(m_spec.current_depth, 2, 16) : 0;
            spec_heads = depth > 0 ? activeSpecHeads(ctx, depth) : 0;
        }

        if (!out.tokens.empty()) {
            insertTrie(out.tokens);
            updateTokenCache(ctx.context_hash, out.tokens);
            saveSlice(ctx.file_hash, ctx.symbol_hash, out.tokens);
        }

        if (m_spec.draft_total > 0) {
            out.acceptance_rate = static_cast<float>(m_spec.accepted_total) /
                                  static_cast<float>(m_spec.draft_total);
        }
        if (out.metrics.tokens_generated > 0) {
            out.acceptance_rate = static_cast<float>(out.metrics.tokens_accepted) /
                                  static_cast<float>(out.metrics.tokens_generated);
        }
        const float step_count = static_cast<float>(std::max<size_t>(1, out.tokens.size()));
        out.speedup_estimate = 1.0f + (out.acceptance_rate * 3.0f) + (out.cache_hit ? 2.0f : 0.0f)
                             + (static_cast<float>(out.stitched_segments) / step_count);
        applyLatencyProtector(out.metrics);
        out.metrics.spec_depth = static_cast<uint32_t>(std::max(0, m_spec.current_depth));
        if (out.metrics.spec_heads == 0 && out.metrics.spec_depth > 0) {
            out.metrics.spec_heads = 1;
        }
        m_morph.tokensGenerated += out.metrics.tokens_generated;
        m_morph.tokensAccepted += out.metrics.tokens_accepted;
        m_morph.verifyRejects += out.metrics.verify_rejects;
        m_morph.branchCowFaults += out.metrics.spec_heads_pruned;
        // Update entropy proxy used by saveSlice's QGCC write path.
        m_last_entropy = out.acceptance_rate * 3.5f + 0.5f; // map [0,1] → [0.5, 4.0]
        // Decay TATT heat periodically so cold tensors migrate down tiers.
        if ((m_tick % 64) == 0) m_tatt.decayAll(0.5f);
        maybeRunMorphTick();
        pushMetrics(out.metrics);
        return out;
    }

private:
    struct SpecStats {
        uint32_t draft_total = 0;
        uint32_t accepted_total = 0;
        int current_depth = 8;
        int current_heads = 2;
        uint32_t disabled_requests = 0;
        uint32_t low_acceptance_streak = 0;
    };

    struct TokenCacheEntry {
        uint64_t context_hash = 0;
        std::array<int32_t, 16> tokens{};
        uint8_t length = 0;
        uint64_t last_used = 0;
        bool valid = false;
    };

    struct KvSlice {
        uint64_t file_hash = 0;
        uint64_t symbol_hash = 0;
        std::array<int32_t, 32> tokens{};
        uint8_t length = 0;
        uint64_t last_used = 0;
        bool valid = false;
        float   entropy   = 1.f;   // attention entropy proxy, updated from acceptance_rate
        uint32_t heat_id  = 0;     // TATT tensor ID for this slice
    };

    struct TrieNode {
        std::unordered_map<int32_t, std::unique_ptr<TrieNode>> children;
    };

    struct SpecHead {
        std::vector<int32_t> tokens;
        float confidence = 1.0f;
        uint32_t kind = 0;
    };

    static constexpr size_t kTokenCacheSize = 64;
    static constexpr size_t kKvSliceSize = 128;
    static constexpr size_t kMetricsRingSize = 512;
    static constexpr uint32_t kFlagSyntaxNoise = 0x1;
    static constexpr uint32_t kFlagContextChurn = 0x2;
    static constexpr uint32_t kFlagColdStart = 0x4;
    static constexpr uint32_t kFlagFileSwitch = 0x8;

    uint64_t nextTick() {
        return ++m_tick;
    }

    static uint64_t fnv1a64(uint64_t seed, int32_t v) {
        uint64_t h = seed;
        const uint64_t prime = 1099511628211ull;
        for (int i = 0; i < 4; ++i) {
            const uint8_t b = static_cast<uint8_t>((v >> (i * 8)) & 0xFF);
            h ^= b;
            h *= prime;
        }
        return h;
    }

    TokenCacheEntry* lookupTokenCache(uint64_t context_hash) {
        for (size_t i = 0; i < m_token_cache.size(); ++i) {
            auto& e = m_token_cache[i];
            if (e.valid && e.context_hash == context_hash) {
                e.last_used = nextTick();
                // Warm this slot in TATT so it survives future eviction pressure
                m_tatt.touch(static_cast<uint32_t>(i));
                m_mem_orchestrator.onAccess(static_cast<uint64_t>(i), sizeof(TokenCacheEntry), 0.9f, 0.10f);
                return &e;
            }
        }
        return nullptr;
    }

    void updateTokenCache(uint64_t context_hash, const std::vector<int32_t>& tokens) {
        TokenCacheEntry* slot = nullptr;
        size_t slotIdx = 0;
        for (size_t i = 0; i < m_token_cache.size(); ++i) {
            if (!m_token_cache[i].valid) { slot = &m_token_cache[i]; slotIdx = i; break; }
        }
        if (!slot) {
            // TATT-informed eviction: prefer the slot whose TATT handle is coldest
            // (lowest heat), breaking ties with LRU tick.
            size_t coldest = 0;
            uint64_t coldestHeat = UINT64_MAX;
            for (size_t i = 0; i < m_token_cache.size(); ++i) {
                auto* h = m_tatt.handleOrNull(static_cast<uint32_t>(i));
                uint64_t heat = h ? h->heat : m_token_cache[i].last_used;
                if (heat < coldestHeat) { coldestHeat = heat; coldest = i; }
            }
            slot = &m_token_cache[coldest];
            slotIdx = coldest;
        }

        // Oracle-driven action selection for token-cache residency.
        const auto cacheDecision = m_mem_orchestrator.decide(static_cast<uint64_t>(slotIdx));
        if (cacheDecision.action == RawrXD::Memory::MemoryAction::Evict) {
            m_tatt.free(static_cast<uint32_t>(slotIdx));
        }
        // Allocate/refresh TATT entry for this cache slot
        m_tatt.free(static_cast<uint32_t>(slotIdx));
        m_tatt.allocate(sizeof(TokenCacheEntry), static_cast<uint32_t>(slotIdx));

        slot->valid = true;
        slot->context_hash = context_hash;
        slot->length = static_cast<uint8_t>(std::min<size_t>(slot->tokens.size(), tokens.size()));
        for (size_t i = 0; i < slot->length; ++i) {
            slot->tokens[i] = tokens[i];
        }
        slot->last_used = nextTick();
    }

    uint32_t stitchContext(uint64_t file_hash, uint64_t symbol_hash, std::vector<int32_t>& working) {
        uint32_t stitched = 0;
        for (size_t i = 0; i < m_kv_slices.size(); ++i) {
            auto& s = m_kv_slices[i];
            if (!s.valid) continue;
            if (s.file_hash != file_hash && (symbol_hash == 0 || s.symbol_hash != symbol_hash))
                continue;
            const size_t n = std::min<size_t>(s.length, s.tokens.size());
            working.insert(working.end(), s.tokens.begin(), s.tokens.begin() + n);
            s.last_used = nextTick();
            // Heat up this slice in TATT so it is retained longer.
            m_tatt.touch(s.heat_id);
            const float temp = std::clamp(s.entropy / 4.0f, 0.0f, 1.0f);
            m_mem_orchestrator.onAttentionSample(s.heat_id, temp, sizeof(KvSlice));
            ++stitched;
            if (stitched >= 4) break;
        }
        return stitched;
    }

    void saveSlice(uint64_t file_hash, uint64_t symbol_hash,
                   const std::vector<int32_t>& tokens) {
        KvSlice* slot = nullptr;
        size_t slotIdx = 0;
        for (size_t i = 0; i < m_kv_slices.size(); ++i) {
            if (!m_kv_slices[i].valid) { slot = &m_kv_slices[i]; slotIdx = i; break; }
        }
        if (!slot) {
            // TATT-informed eviction: evict the coldest KV slice first.
            size_t coldest = 0;
            uint64_t coldestHeat = UINT64_MAX;
            for (size_t i = 0; i < m_kv_slices.size(); ++i) {
                auto* h = m_tatt.handleOrNull(kKvSliceBase + static_cast<uint32_t>(i));
                uint64_t heat = h ? h->heat : m_kv_slices[i].last_used;
                if (heat < coldestHeat) { coldestHeat = heat; coldest = i; }
            }
            slot = &m_kv_slices[coldest];
            slotIdx = coldest;
        }
        slot->valid      = true;
        slot->file_hash  = file_hash;
        slot->symbol_hash= symbol_hash;
        slot->entropy    = m_last_entropy;
        slot->heat_id    = kKvSliceBase + static_cast<uint32_t>(slotIdx);
        slot->length     = static_cast<uint8_t>(
            std::min<size_t>(slot->tokens.size(), tokens.size()));
        for (size_t i = 0; i < slot->length; ++i)
            slot->tokens[i] = tokens[i];
        slot->last_used  = nextTick();

        // Register this slice with TATT so heat accumulates on future stitches.
        m_tatt.free(slot->heat_id);
        m_tatt.allocate(sizeof(KvSlice), slot->heat_id);

        // Orchestrator hooks: access signal + heat-zone update + pressure update.
        m_mem_orchestrator.onAccess(slot->heat_id, sizeof(KvSlice), 0.75f, 0.30f);
        const float temp = std::clamp(slot->entropy / 4.0f, 0.0f, 1.0f);
        m_mem_orchestrator.onAttentionSample(slot->heat_id, temp, sizeof(KvSlice));

        const float vramUsedRatio = static_cast<float>(std::count_if(
            m_kv_slices.begin(), m_kv_slices.end(), [](const KvSlice& s) { return s.valid; })) /
            static_cast<float>(kKvSliceSize);
        const float ramUsedRatio = static_cast<float>(std::count_if(
            m_token_cache.begin(), m_token_cache.end(), [](const TokenCacheEntry& e) { return e.valid; })) /
            static_cast<float>(kTokenCacheSize);
        m_mem_orchestrator.onPressure(vramUsedRatio, ramUsedRatio);

        // QGCC: encode a synthetic FP16 projection of this slice's tokens so the
        // entropy-adaptive compression window can throttle older entries.
        // One virtual head per slice position (capped at the QGCC sequence limit).
        uint32_t head = static_cast<uint32_t>(slotIdx) % kQGCCHeads;
        uint32_t pos  = static_cast<uint32_t>(m_tick % kQGCCSeq);
        std::array<uint16_t, 8> synth;
        for (int d = 0; d < 8; ++d) {
            // Hash token values to FP16 range [-1, 1]
            uint32_t t = (d < slot->length) ? static_cast<uint32_t>(slot->tokens[d]) : 0u;
            uint32_t h = (t * 2654435761u) ^ static_cast<uint32_t>(d * 0xDEADBEEFu);
            float fv   = (static_cast<float>(h & 0xFFFF) / 32767.5f) - 1.f;
            uint32_t bits; memcpy(&bits, &fv, 4);
            synth[d] = static_cast<uint16_t>(bits >> 16);
        }
        m_qgcc.write(head, pos, synth.data(), slot->entropy);
    }

    void insertTrie(const std::vector<int32_t>& tokens) {
        TrieNode* node = &m_trie_root;
        const size_t n = std::min<size_t>(tokens.size(), 16);
        for (size_t i = 0; i < n; ++i) {
            auto& child = node->children[tokens[i]];
            if (!child) child = std::make_unique<TrieNode>();
            node = child.get();
        }
    }

    TrieNode* trieNodeForPrefix(const std::vector<int32_t>& prefix) {
        TrieNode* node = &m_trie_root;
        for (int32_t t : prefix) {
            auto it = node->children.find(t);
            if (it == node->children.end()) return nullptr;
            node = it->second.get();
        }
        return node;
    }

    void applyPrefixTrieBias(const std::vector<int32_t>& prefix_hint,
                             std::vector<int32_t>& draft,
                             const std::vector<int32_t>& working) {
        std::vector<int32_t> prefix = prefix_hint;
        if (prefix.empty()) {
            const size_t tail = std::min<size_t>(working.size(), 6);
            prefix.assign(working.end() - tail, working.end());
        }

        TrieNode* node = trieNodeForPrefix(prefix);
        if (!node || node->children.empty() || draft.empty()) {
            return;
        }

        int32_t best_token = draft[0];
        uint64_t best_score = 0;
        for (const auto& kv : node->children) {
            const uint64_t score = static_cast<uint64_t>(kv.second->children.size()) + 1;
            if (score > best_score) {
                best_score = score;
                best_token = kv.first;
            }
        }
        draft[0] = best_token;
    }

    static int findTopIndex(const std::vector<float>& logits, int skip = -1) {
        int idx = -1;
        float best = -std::numeric_limits<float>::infinity();
        for (size_t i = 0; i < logits.size(); ++i) {
            if (static_cast<int>(i) == skip) continue;
            if (logits[i] > best) {
                best = logits[i];
                idx = static_cast<int>(i);
            }
        }
        return idx;
    }

    int activeSpecHeads(const ControllerRequestContext& ctx, int depth) const {
        int heads = std::clamp(m_spec.current_heads, 1, 4);
        const bool entropy = (ctx.flags & (kFlagSyntaxNoise | kFlagContextChurn | kFlagColdStart | kFlagFileSwitch)) != 0;
        if (entropy) {
            heads = std::max(heads, 2);
        }
        if ((ctx.flags & kFlagSyntaxNoise) != 0 || (ctx.flags & kFlagContextChurn) != 0) {
            heads = std::max(heads, 3);
        }
        if (!ctx.ast_allowed_tokens.empty()) {
            heads = std::max(heads, 2);
        }
        if (depth <= 2) {
            heads = std::min(heads, 2);
        }
        return std::clamp(heads, 1, 4);
    }

    static bool hasHead(const std::vector<std::vector<int32_t>>& heads,
                        const std::vector<int32_t>& candidate) {
        return std::find(heads.begin(), heads.end(), candidate) != heads.end();
    }

    static bool hasHead(const std::vector<SpecHead>& heads,
                        const std::vector<int32_t>& candidate) {
        return std::find_if(heads.begin(), heads.end(), [&](const SpecHead& h) {
            return h.tokens == candidate;
        }) != heads.end();
    }

    static int topAllowedIndex(const std::vector<float>& logits,
                               const std::vector<int32_t>& allowed,
                               const std::vector<int32_t>& skip) {
        int best = -1;
        float score = -std::numeric_limits<float>::infinity();
        if (!allowed.empty()) {
            for (int32_t id : allowed) {
                if (id < 0 || id >= static_cast<int32_t>(logits.size())) continue;
                if (std::find(skip.begin(), skip.end(), id) != skip.end()) continue;
                if (logits[id] > score) {
                    score = logits[id];
                    best = static_cast<int>(id);
                }
            }
            return best;
        }

        for (size_t i = 0; i < logits.size(); ++i) {
            const int id = static_cast<int>(i);
            if (std::find(skip.begin(), skip.end(), id) != skip.end()) continue;
            if (logits[i] > score) {
                score = logits[i];
                best = id;
            }
        }
        return best;
    }

    static float logitMarginForToken(const std::vector<float>& logits, int32_t token) {
        if (token < 0 || token >= static_cast<int32_t>(logits.size())) return 1.0f;
        float next_best = -std::numeric_limits<float>::infinity();
        for (size_t i = 0; i < logits.size(); ++i) {
            if (static_cast<int32_t>(i) == token) continue;
            next_best = std::max(next_best, logits[i]);
        }
        if (next_best == -std::numeric_limits<float>::infinity()) return 16.0f;
        return logits[static_cast<size_t>(token)] - next_best;
    }

    static bool shouldKeepHead(uint32_t kind, float confidence, int head_count) {
        if (kind == 0) return true;
        const float threshold = (head_count >= 4) ? -7.5f : -10.0f;
        if (kind == 1) return confidence >= threshold;
        if (kind == 2) return confidence >= (threshold - 2.0f);
        return confidence >= (threshold - 3.5f);
    }

    static void specializeHead(SpecHead& head,
                               const std::vector<int32_t>& allowed,
                               const std::vector<int32_t>& prefix_hint) {
        if (head.tokens.empty()) return;

        // kind 1: conservative grammar head. Keep all tokens inside AST mask when possible.
        if (head.kind == 1 && !allowed.empty()) {
            size_t mask_idx = 0;
            for (size_t i = 1; i < head.tokens.size() && mask_idx < allowed.size(); ++i) {
                if (std::find(allowed.begin(), allowed.end(), head.tokens[i]) == allowed.end()) {
                    head.tokens[i] = allowed[mask_idx++];
                }
            }
            return;
        }

        // kind 2: balanced prefix-continuation head. Bias the second token toward the known prefix lane.
        if (head.kind == 2 && head.tokens.size() > 1 && !prefix_hint.empty()) {
            head.tokens[1] = prefix_hint.back();
            return;
        }

        // kind 3: aggressive exploration. Keep first token strong but perturb tail to probe another lane.
        if (head.kind == 3 && head.tokens.size() > 2) {
            std::swap(head.tokens[1], head.tokens[2]);
        }
    }

    int32_t trieNextToken(const std::vector<int32_t>& prefix_hint,
                          const std::vector<int32_t>& working,
                          int32_t avoid) {
        std::vector<int32_t> prefix = prefix_hint;
        if (prefix.empty() && !working.empty()) {
            const size_t tail = std::min<size_t>(working.size(), 6);
            prefix.assign(working.end() - tail, working.end());
        }

        TrieNode* node = trieNodeForPrefix(prefix);
        if (!node || node->children.empty()) return avoid;

        int32_t best_token = avoid;
        uint64_t best_score = 0;
        for (const auto& kv : node->children) {
            if (kv.first == avoid && node->children.size() > 1) continue;
            const uint64_t score = static_cast<uint64_t>(kv.second->children.size()) + 1;
            if (score > best_score) {
                best_score = score;
                best_token = kv.first;
            }
        }
        return best_token;
    }

    std::vector<SpecHead> makeParallelHeads(const ControllerRequestContext& ctx,
                                            const std::vector<int32_t>& working,
                                            const std::vector<int32_t>& base,
                                            int head_count,
                                            uint32_t& pruned_out) {
        std::vector<SpecHead> heads;
        pruned_out = 0;
        if (base.empty()) return heads;
        heads.reserve(static_cast<size_t>(std::clamp(head_count, 1, 4)));
        std::vector<float> logits = m_target->Eval(working);
        heads.push_back(SpecHead{base, logitMarginForToken(logits, base[0]), 0});
        if (head_count <= 1) return heads;

        std::vector<int32_t> skipped;
        skipped.reserve(4);
        skipped.push_back(base[0]);

        for (int h = 1; h < head_count; ++h) {
            std::vector<int32_t> alt = base;
            int candidate = -1;
            if (h == 1 && !logits.empty()) {
                candidate = topAllowedIndex(logits, ctx.ast_allowed_tokens, skipped);
            } else if (h == 2) {
                const int32_t trie_token = trieNextToken(ctx.prefix_hint_tokens, working, alt[0]);
                if (trie_token != alt[0]) candidate = trie_token;
            } else if (!logits.empty()) {
                candidate = topAllowedIndex(logits, {}, skipped);
            }

            if (candidate < 0 || candidate == alt[0]) continue;
            alt[0] = static_cast<int32_t>(candidate);
            SpecHead head{std::move(alt), logitMarginForToken(logits, static_cast<int32_t>(candidate)), static_cast<uint32_t>(h)};
            specializeHead(head, ctx.ast_allowed_tokens, ctx.prefix_hint_tokens);
            if (!shouldKeepHead(head.kind, head.confidence, head_count)) {
                ++pruned_out;
                continue;
            }
            if (hasHead(heads, head.tokens)) continue;
            skipped.push_back(head.tokens[0]);
            heads.push_back(std::move(head));
        }
        return heads;
    }

    static int chooseBestHead(const std::vector<SpecHead>& heads,
                              const std::vector<int32_t>& target,
                              size_t& best_head) {
        best_head = 0;
        if (heads.empty()) return 0;
        int best_accepted = -1;
        float best_confidence = -std::numeric_limits<float>::infinity();
        size_t best_rejects = std::numeric_limits<size_t>::max();
        for (size_t i = 0; i < heads.size(); ++i) {
            const int accepted = verifyBatch(heads[i].tokens, target);
            const size_t rejects = heads[i].tokens.size() - static_cast<size_t>(std::max(0, accepted));
            if (accepted > best_accepted ||
                (accepted == best_accepted && heads[i].confidence > best_confidence) ||
                (accepted == best_accepted && heads[i].confidence == best_confidence && rejects < best_rejects)) {
                best_accepted = accepted;
                best_confidence = heads[i].confidence;
                best_rejects = rejects;
                best_head = i;
            }
        }
        return std::max(0, best_accepted);
    }

    void applyAstMaskAndFastPath(const std::vector<int32_t>& allowed,
                                 const std::vector<int32_t>& working,
                                 std::vector<int32_t>& draft) {
        if (allowed.empty() || working.empty() || draft.empty()) return;

        std::vector<float> logits = m_target->Eval(working);
        if (logits.empty()) return;

        int top1 = findTopIndex(logits);
        int top2 = findTopIndex(logits, top1);
        if (top1 < 0) return;

        const float v1 = logits[top1];
        const float v2 = (top2 >= 0 ? logits[top2] : -1000.0f);
        const float confidence = v1 - v2;

        const bool top_allowed = std::find(allowed.begin(), allowed.end(), top1) != allowed.end();
        if (confidence > 12.0f && top_allowed) {
            draft[0] = top1;
            return;
        }

        int best_allowed = -1;
        float best_val = -std::numeric_limits<float>::infinity();
        for (int32_t id : allowed) {
            if (id < 0 || id >= static_cast<int32_t>(logits.size())) continue;
            if (logits[id] > best_val) {
                best_val = logits[id];
                best_allowed = id;
            }
        }
        if (best_allowed >= 0) {
            draft[0] = best_allowed;
        }
    }

    int32_t chooseMaskedCorrection(int32_t fallback,
                                   const std::vector<int32_t>& allowed,
                                   const std::vector<int32_t>& working) {
        if (allowed.empty() || working.empty()) return fallback;
        std::vector<float> logits = m_target->Eval(working);
        if (logits.empty()) return fallback;

        int best = -1;
        float score = -std::numeric_limits<float>::infinity();
        for (int32_t id : allowed) {
            if (id < 0 || id >= static_cast<int32_t>(logits.size())) continue;
            if (logits[id] > score) {
                score = logits[id];
                best = id;
            }
        }
        return best >= 0 ? static_cast<int32_t>(best) : fallback;
    }

    static int verifyBatch(const std::vector<int32_t>& draft, const std::vector<int32_t>& target) {
        if (draft.empty() || target.empty()) return 0;

        const int n = static_cast<int>(std::min(draft.size(), target.size()));
        int accepted = 0;

        int i = 0;
        for (; i + 7 < n; i += 8) {
            bool all_eq = true;
            for (int k = 0; k < 8; ++k) {
                if (draft[i + k] != target[i + k]) {
                    all_eq = false;
                    break;
                }
            }
            if (!all_eq) break;
            accepted += 8;
        }
        for (; i < n; ++i) {
            if (draft[i] != target[i]) break;
            ++accepted;
        }
        return accepted;
    }

    void adaptDepth() {
        if (m_spec.draft_total < 8) return;
        const float rate = static_cast<float>(m_spec.accepted_total) /
                           static_cast<float>(m_spec.draft_total);
        if (rate > 0.85f) {
            m_spec.current_depth = std::min(16, m_spec.current_depth + 1);
            m_spec.current_heads = std::max(1, m_spec.current_heads - 1);
        } else if (rate < 0.5f) {
            m_spec.current_depth = std::max(2, m_spec.current_depth - 1);
            m_spec.current_heads = std::min(4, m_spec.current_heads + 1);
        } else if (rate < 0.72f) {
            m_spec.current_heads = std::min(4, m_spec.current_heads + 1);
        }
    }

    bool speculationEnabled() {
        if (m_spec.disabled_requests == 0) {
            return true;
        }
        --m_spec.disabled_requests;
        return false;
    }

    void applyLatencyProtector(const ControllerMetrics& metrics) {
        if (metrics.tokens_generated == 0) return;
        const float rate = static_cast<float>(metrics.tokens_accepted) /
                           static_cast<float>(metrics.tokens_generated);
        if (rate < 0.4f) {
            m_spec.current_depth = 2;
            m_spec.current_heads = std::min(4, std::max(2, m_spec.current_heads + 1));
            ++m_spec.low_acceptance_streak;
            m_spec.disabled_requests = std::max<uint32_t>(m_spec.disabled_requests, 2);
        } else {
            m_spec.low_acceptance_streak = 0;
        }

        const float reject_rate = static_cast<float>(metrics.verify_rejects) /
                                  static_cast<float>(metrics.tokens_generated);
        if (m_spec.low_acceptance_streak >= 3 || reject_rate > 0.65f) {
            m_spec.current_depth = 2;
            m_spec.current_heads = std::min(4, std::max(3, m_spec.current_heads));
            m_spec.disabled_requests = std::max<uint32_t>(m_spec.disabled_requests, 8);
        }
    }

    void pushMetrics(const ControllerMetrics& metrics) {
        m_metrics_ring[m_metrics_write] = metrics;
        m_metrics_write = (m_metrics_write + 1) % kMetricsRingSize;
        ++m_metrics_count;
    }

    size_t validTokenCacheCount() const {
        return static_cast<size_t>(std::count_if(
            m_token_cache.begin(), m_token_cache.end(),
            [](const TokenCacheEntry& e) { return e.valid; }));
    }

    size_t validKvSliceCount() const {
        return static_cast<size_t>(std::count_if(
            m_kv_slices.begin(), m_kv_slices.end(),
            [](const KvSlice& s) { return s.valid; }));
    }

    RawrXD::Memory::MemoryTelemetry sampleMorphTelemetry() const {
        RawrXD::Memory::MemoryTelemetry t{};
        const size_t liveCache = validTokenCacheCount();
        const size_t liveSlices = validKvSliceCount();
        const uint64_t cacheBytes = static_cast<uint64_t>(liveCache * sizeof(TokenCacheEntry));
        const uint64_t sliceBytes = static_cast<uint64_t>(liveSlices * sizeof(KvSlice));

        t.vramUsed = cacheBytes + sliceBytes;
        t.kvBytes = sliceBytes;
        t.cacheHitRate = (m_morph.totalRequests > 0)
            ? static_cast<float>(m_morph.cacheHits) / static_cast<float>(m_morph.totalRequests)
            : 0.0f;
        t.branchCowFaults = m_morph.branchCowFaults;
        t.migrationBytes = m_morph.migrationBytes;
        t.prefetchWaste = m_morph.prefetchWasteBytes;

        // Normalize local telemetry into simple IO pressure signals.
        const float vramRatio = static_cast<float>(t.vramUsed) /
            static_cast<float>(std::max<uint64_t>(1, m_morph.vramBudgetBytes));
        t.pcieRxGbps = std::min(24.0f, vramRatio * 24.0f);
        t.pcieTxGbps = std::min(24.0f, (1.0f - t.cacheHitRate) * 24.0f);
        t.nvlinkUtil = std::clamp(vramRatio, 0.0f, 1.0f);
        return t;
    }

    size_t prefetchHotEntries(size_t byteBudget) {
        if (byteBudget == 0) return 0;

        size_t touchedBytes = 0;
        // Prefetch token cache entries by touching most recently used first.
        for (size_t pass = 0; pass < m_token_cache.size() && touchedBytes < byteBudget; ++pass) {
            size_t best = m_token_cache.size();
            uint64_t bestTick = 0;
            for (size_t i = 0; i < m_token_cache.size(); ++i) {
                const auto& e = m_token_cache[i];
                if (!e.valid) continue;
                if (e.last_used >= bestTick) {
                    bestTick = e.last_used;
                    best = i;
                }
            }
            if (best == m_token_cache.size()) break;
            m_tatt.touch(static_cast<uint32_t>(best));
            touchedBytes += sizeof(TokenCacheEntry);
            // Mark consumed so we don't repeatedly pick the same slot in this call.
            m_token_cache[best].last_used = 0;
        }
        return touchedBytes;
    }

    size_t pruneColdEntries(size_t byteBudget) {
        if (byteBudget == 0) return 0;

        size_t reclaimed = 0;
        // Prune oldest token cache entries first.
        for (size_t pass = 0; pass < m_token_cache.size() && reclaimed < byteBudget; ++pass) {
            size_t victim = m_token_cache.size();
            uint64_t oldest = UINT64_MAX;
            for (size_t i = 0; i < m_token_cache.size(); ++i) {
                const auto& e = m_token_cache[i];
                if (!e.valid) continue;
                if (e.last_used < oldest) {
                    oldest = e.last_used;
                    victim = i;
                }
            }
            if (victim == m_token_cache.size()) break;
            m_tatt.free(static_cast<uint32_t>(victim));
            m_token_cache[victim].valid = false;
            reclaimed += sizeof(TokenCacheEntry);
        }

        // Then prune oldest KV slices.
        for (size_t pass = 0; pass < m_kv_slices.size() && reclaimed < byteBudget; ++pass) {
            size_t victim = m_kv_slices.size();
            uint64_t oldest = UINT64_MAX;
            for (size_t i = 0; i < m_kv_slices.size(); ++i) {
                const auto& s = m_kv_slices[i];
                if (!s.valid) continue;
                if (s.last_used < oldest) {
                    oldest = s.last_used;
                    victim = i;
                }
            }
            if (victim == m_kv_slices.size()) break;
            m_tatt.free(kKvSliceBase + static_cast<uint32_t>(victim));
            m_kv_slices[victim].valid = false;
            reclaimed += sizeof(KvSlice);
        }
        return reclaimed;
    }

    void applyMorphActions(const std::vector<RawrXD::Memory::MorphAction>& actions,
                           const RawrXD::Memory::MemoryTelemetry& telemetry) {
        for (const auto& action : actions) {
            switch (action.type) {
            case RawrXD::Memory::MorphAction::Type::Compress: {
                // Cool all tensors to nudge medium/cold entries toward lower tiers.
                m_tatt.decayAll(0.8f);
                break;
            }
            case RawrXD::Memory::MorphAction::Type::Migrate: {
                const auto migrations = m_mem_orchestrator.rebalanceHeatZones();
                const uint64_t migrated = static_cast<uint64_t>(migrations.size()) * sizeof(KvSlice);
                m_morph.migrationBytes += std::min<uint64_t>(migrated, static_cast<uint64_t>(action.bytes));
                break;
            }
            case RawrXD::Memory::MorphAction::Type::Prefetch: {
                const size_t prefetched = prefetchHotEntries(action.bytes);
                m_morph.prefetchBytes += static_cast<uint64_t>(prefetched);
                // Rejected speculative tokens approximate prefetch overrun/waste.
                m_morph.prefetchWasteBytes = static_cast<uint64_t>(m_morph.verifyRejects) * sizeof(int32_t);
                break;
            }
            case RawrXD::Memory::MorphAction::Type::Prune: {
                const size_t reclaimed = pruneColdEntries(action.bytes);
                m_morph.prunedBytes += static_cast<uint64_t>(reclaimed);
                break;
            }
            }
        }

        const float vramRatio = static_cast<float>(telemetry.vramUsed) /
            static_cast<float>(std::max<uint64_t>(1, m_morph.vramBudgetBytes));
        const float ramRatio = static_cast<float>(validTokenCacheCount()) /
            static_cast<float>(kTokenCacheSize);
        m_mem_orchestrator.onPressure(std::clamp(vramRatio, 0.0f, 1.0f), std::clamp(ramRatio, 0.0f, 1.0f));
    }

    void maybeRunMorphTick() {
        const uint64_t nowMs = static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count());
        if ((nowMs - m_morph.lastTickMs) < m_morph.tickIntervalMs) {
            return;
        }
        m_morph.lastTickMs = nowMs;

        const auto telemetry = sampleMorphTelemetry();
        const auto mode = m_morphController.selectMode(telemetry);
        m_morphController.applyMode(mode);
        const auto actions = m_morphController.tick(telemetry, nowMs);
        applyMorphActions(actions, telemetry);
    }

private:
    // TATT base IDs: token cache slots [0, kTokenCacheSize)
    //                KV slice slots    [kKvSliceBase, kKvSliceBase + kKvSliceSize)
    static constexpr uint32_t kKvSliceBase = static_cast<uint32_t>(kTokenCacheSize);
    // QGCC configuration: one virtual head per KV slice bucket, 4096-token sequence window
    static constexpr uint32_t kQGCCHeads = 32;
    static constexpr uint32_t kQGCCSeq   = 4096;

    InferenceEngine* m_draft  = nullptr;
    InferenceEngine* m_target = nullptr;

    SpecStats m_spec;
    std::array<TokenCacheEntry, kTokenCacheSize> m_token_cache{};
    std::array<KvSlice, kKvSliceSize> m_kv_slices{};
    std::array<ControllerMetrics, kMetricsRingSize> m_metrics_ring{};
    TrieNode m_trie_root;
    uint64_t m_tick         = 0;
    uint64_t m_metrics_write= 0;
    uint64_t m_metrics_count= 0;
    float    m_last_entropy = 1.5f;

    struct MorphRuntime {
        uint64_t totalRequests = 0;
        uint64_t cacheHits = 0;
        uint64_t tokensGenerated = 0;
        uint64_t tokensAccepted = 0;
        uint64_t verifyRejects = 0;
        uint64_t branchCowFaults = 0;
        uint64_t migrationBytes = 0;
        uint64_t prefetchBytes = 0;
        uint64_t prefetchWasteBytes = 0;
        uint64_t prunedBytes = 0;
        uint64_t vramBudgetBytes = 16ull << 30;
        uint64_t tickIntervalMs = 50;
        uint64_t lastTickMs = 0;
    };

    // Memory management subsystems
    ai_mem::ThermalTieredAllocator m_tatt;
    ai_mem::QuantGatedKVCache      m_qgcc{kQGCCHeads, 128, kQGCCSeq};
    RawrXD::Memory::MemoryOrchestrator m_mem_orchestrator;
    RawrXD::Memory::MemoryMorphController m_morphController;
    MorphRuntime m_morph;
};

} // namespace RawrXD::ExtensionKernel
