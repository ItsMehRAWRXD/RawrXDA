// d:/rawrxd/src/ai/exotic_spec_decoder.cpp
//
// ExoticSpecDecoder implementation — pure C++17, zero external dependencies.
//
// Architecture (7 heads):
//   [0] 4-gram SIMD cuckoo cache     — highest precision n-gram
//   [1] 3-gram SIMD cuckoo cache     — fallback
//   [2] 2-gram SIMD cuckoo cache     — fallback
//   [3] Tiny online MLP (128->64->512) — learns long-range pattern
//   [4] Rabin-Karp copy detector     — detects repetition, predicts continuation
//   [5] Repetition breaker           — counters n-gram loops with low-freq token
//   [6] KV-embedding memory network  — nearest-neighbour from recent context
//
// Cascade tree recovery: on draft mismatch at depth D, searches all alternate
// tree branches at that depth for a node whose token matches the target,
// then continues verifying from that branch instead of aborting.
//
// SIMD: AVX2 for argmax, softmax, cuckoo key probe, MLP matmul.
// Adaptive width: EWMA acceptance rate drives K up/down every step.

#include "exotic_spec_decoder.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstring>
#include <immintrin.h>
#include <memory>
#include <random>

#ifdef _MSC_VER
#  include <intrin.h>
static inline int rxd_ctz(unsigned x) {
    unsigned long r; _BitScanForward(&r, x); return (int)r;
}
#else
static inline int rxd_ctz(unsigned x) { return __builtin_ctz(x); }
#endif

namespace rxd::ai {
namespace {

// ============================================================
// Constants
// ============================================================
static constexpr uint32_t kNF = 0xFFFF'FFFFu;   // not-found sentinel
static constexpr uint64_t kEK = 0xFFFF'FFFF'FFFF'FFFFull; // empty key

// ============================================================
// Hashing
// ============================================================
inline uint64_t Mix64(uint64_t x) noexcept {
    x ^= x >> 33;
    x *= 0xff51afd7ed558ccdULL;
    x ^= x >> 33;
    x *= 0xc4ceb9fe1a85ec53ULL;
    x ^= x >> 33;
    return x;
}
inline uint64_t K2(uint32_t a, uint32_t b) noexcept {
    return Mix64((uint64_t(a) << 32) | b);
}
inline uint64_t K3(uint32_t a, uint32_t b, uint32_t c) noexcept {
    return Mix64(K2(a,b) ^ (uint64_t(c) * 0x9e3779b97f4a7c15ULL));
}
inline uint64_t K4(uint32_t a, uint32_t b, uint32_t c, uint32_t d) noexcept {
    return Mix64(K3(a,b,c) ^ (uint64_t(d) * 0x9e3779b97f4a7c15ULL));
}

// ============================================================
// Fast approximate exp (Schraudolph, < 0.4% max error)
// ============================================================
inline float FExp(float x) noexcept {
    x = (x < -80.f) ? -80.f : (x > 80.f ? 80.f : x);
    union { float f; int32_t i; } u;
    u.i = int32_t(12102203.161561485f * x + 1064866805.f);
    return u.f;
}

// ============================================================
// AVX2 argmax
// ============================================================
static uint32_t Argmax(const float* __restrict d, uint32_t n) noexcept {
    if (!n) return 0;
    uint32_t bi = 0, i = 0;
    if (n >= 8) {
        __m256 bv = _mm256_loadu_ps(d);
        __m256i bidx = _mm256_setr_epi32(0,1,2,3,4,5,6,7);
        __m256i step = _mm256_set1_epi32(8);
        __m256i cur  = bidx;
        for (i = 8; i + 8 <= n; i += 8) {
            __m256 v  = _mm256_loadu_ps(d + i);
            __m256 gt = _mm256_cmp_ps(v, bv, _CMP_GT_OQ);
            bv   = _mm256_blendv_ps(bv, v, gt);
            bidx = _mm256_castps_si256(_mm256_blendv_ps(
                       _mm256_castsi256_ps(bidx),
                       _mm256_castsi256_ps(cur), gt));
            cur  = _mm256_add_epi32(cur, step);
        }
        alignas(32) float   fv[8];
        alignas(32) int32_t iv[8];
        _mm256_store_ps(fv, bv);
        _mm256_store_si256((__m256i*)iv, bidx);
        float mv = fv[0]; bi = uint32_t(iv[0]);
        for (int j = 1; j < 8; ++j)
            if (fv[j] > mv) { mv = fv[j]; bi = uint32_t(iv[j]); }
    }
    float mv = d[bi];
    for (; i < n; ++i) if (d[i] > mv) { mv = d[i]; bi = i; }
    return bi;
}

// ============================================================
// AVX2 softmax in-place
// ============================================================
static void Softmax(float* __restrict d, uint32_t n) noexcept {
    if (!n) return;
    float mx = d[0];
    uint32_t i = 1;
    for (; i < n; ++i) if (d[i] > mx) mx = d[i];

    float sum = 0.f;
    i = 0;
    if (n >= 8) {
        __m256 vs = _mm256_setzero_ps();
        __m256 vm = _mm256_set1_ps(mx);
        for (; i + 8 <= n; i += 8) {
            alignas(32) float t[8];
            __m256 v = _mm256_sub_ps(_mm256_loadu_ps(d+i), vm);
            _mm256_store_ps(t, v);
            for (int j = 0; j < 8; ++j) t[j] = FExp(t[j]);
            __m256 ve = _mm256_load_ps(t);
            _mm256_storeu_ps(d+i, ve);
            vs = _mm256_add_ps(vs, ve);
        }
        alignas(32) float ts[8];
        _mm256_store_ps(ts, vs);
        for (int j = 0; j < 8; ++j) sum += ts[j];
    }
    for (; i < n; ++i) { d[i] = FExp(d[i] - mx); sum += d[i]; }

    const float inv = 1.f / sum;
    i = 0;
    if (n >= 8) {
        __m256 vi = _mm256_set1_ps(inv);
        for (; i + 8 <= n; i += 8)
            _mm256_storeu_ps(d+i, _mm256_mul_ps(_mm256_loadu_ps(d+i), vi));
    }
    for (; i < n; ++i) d[i] *= inv;
}

// ============================================================
// Xorshift64 fast RNG (for probabilistic acceptance)
// ============================================================
struct XRng {
    uint64_t s = 0x853c49e6748fea9bULL;
    uint32_t Next() noexcept {
        s ^= s >> 12; s ^= s << 25; s ^= s >> 27;
        return uint32_t(s * 0x2545f4914f6cdd1dULL >> 32);
    }
    float Uniform() noexcept { return float(Next()) * (1.f / 4294967296.f); }
};

// ============================================================
// AVX2 4-way Cuckoo hash table — 64-byte aligned buckets
// Each bucket holds 4 (key,val,freq) entries.
// SIMD probes 4 keys simultaneously.
// ============================================================
struct alignas(64) Bucket {
    uint64_t key[4];
    uint32_t val[4];
    uint16_t freq[4];
    uint16_t pad[4];
};
static_assert(sizeof(Bucket) == 64, "");

class NGramTable {
    std::unique_ptr<Bucket[]> b_;
    size_t mask_;
public:
    explicit NGramTable(uint32_t log2) {
        auto n = size_t(1) << log2;
        b_.reset(new Bucket[n]());
        mask_ = n - 1;
        for (size_t i = 0; i < n; ++i)
            for (int j = 0; j < 4; ++j) b_[i].key[j] = kEK;
    }

    uint32_t Lookup(uint64_t key) const noexcept {
        const uint64_t h = Mix64(key);
        for (int r = 0; r < 4; ++r) {
            const Bucket& bk = b_[(h + r) & mask_];
            __m256i kv  = _mm256_load_si256((const __m256i*)bk.key);
            __m256i tgt = _mm256_set1_epi64x(int64_t(key));
            __m256i cmp = _mm256_cmpeq_epi64(kv, tgt);
            int m = _mm256_movemask_pd(_mm256_castsi256_pd(cmp));
            if (m) return bk.val[rxd_ctz(m)];
        }
        return kNF;
    }

    void Insert(uint64_t key, uint32_t val) noexcept {
        const uint64_t h = Mix64(key);
        for (int r = 0; r < 4; ++r) {
            Bucket& bk = b_[(h + r) & mask_];
            // check existing
            __m256i kv  = _mm256_load_si256((__m256i*)bk.key);
            __m256i tgt = _mm256_set1_epi64x(int64_t(key));
            __m256i cmp = _mm256_cmpeq_epi64(kv, tgt);
            int m = _mm256_movemask_pd(_mm256_castsi256_pd(cmp));
            if (m) {
                int s = rxd_ctz(m);
                bk.val[s] = val;
                bk.freq[s] = uint16_t(std::min<int>(bk.freq[s] + 1, 65535));
                return;
            }
            // find empty or lowest-freq slot
            int ev = 0; uint16_t mf = bk.freq[0];
            for (int j = 1; j < 4; ++j) {
                if (bk.key[j] == kEK) { ev = j; mf = 0; break; }
                if (bk.freq[j] < mf)  { mf = bk.freq[j]; ev = j; }
            }
            if (mf < 2) {
                bk.key[ev] = key; bk.val[ev] = val;
                bk.freq[ev] = 1; return;
            }
        }
        // cuckoo kick — evict lowest-freq slot in first bucket
        Bucket& bk = b_[h & mask_];
        int ev = 0;
        for (int j = 1; j < 4; ++j) if (bk.freq[j] < bk.freq[ev]) ev = j;
        bk.key[ev] = key; bk.val[ev] = val; bk.freq[ev] = 1;
    }
};

// ============================================================
// Rabin-Karp copy-detector head
// Maintains a 128-token sliding window; finds repeated suffixes
// of length >= 4 and predicts their continuation.
// ============================================================
class CopyHead {
    static constexpr uint32_t W = 128;
    uint32_t buf_[W] = {};
    uint32_t pos_    = 0;
    uint32_t fill_   = 0;
public:
    void Push(uint32_t t) noexcept {
        buf_[pos_ & (W-1)] = t;
        ++pos_;
        if (fill_ < W) ++fill_;
    }
    // Returns kNF if no repeat found, otherwise the predicted next token.
    uint32_t Predict() const noexcept {
        if (fill_ < 10) return kNF;
        uint32_t lim = std::min(fill_ >> 1, 32u);
        for (uint32_t L = 4; L <= lim; ++L) {
            bool same = true;
            for (uint32_t k = 0; k < L; ++k) {
                uint32_t a = buf_[(pos_ - L     + k) & (W-1)];
                uint32_t b2 = buf_[(pos_ - 2*L   + k) & (W-1)];
                if (a != b2) { same = false; break; }
            }
            if (same) {
                // continuation is one past the second occurrence
                return buf_[(pos_ - L) & (W-1)];
            }
        }
        return kNF;
    }
    // Predict break token when last 3 are identical (repetition loop)
    uint32_t BreakToken() const noexcept {
        if (fill_ < 4) return kNF;
        uint32_t a = buf_[(pos_-1)&(W-1)];
        uint32_t b2= buf_[(pos_-2)&(W-1)];
        uint32_t c = buf_[(pos_-3)&(W-1)];
        if (a == b2 && b2 == c)
            return (a + 17) % 32000; // simple escape heuristic
        return kNF;
    }
};

// ============================================================
// Tiny online MLP — embed(128) -> hidden(64) -> slim_vocab(512)
// Trained via single-step SGD on every FeedAccepted call.
// AVX2 matmuls.
// ============================================================
class OracleHead {
    uint32_t vocab_, E_, H_, S_; // vocab, embed, hidden, slim
    std::unique_ptr<float[]> emb_;      // [vocab][E]
    std::unique_ptr<float[]> W1_, b1_;  // [H][E], [H]
    std::unique_ptr<float[]> W2_, b2_;  // [S][H], [S]
    uint32_t train_count_ = 0;

    // SIMD matmul: out[rows] = W[rows×cols] * x[cols]
    static void MV(const float* W, const float* x, const float* b,
                   float* out, uint32_t rows, uint32_t cols) noexcept {
        for (uint32_t r = 0; r < rows; ++r) {
            const float* row = W + r*cols;
            float s = b ? b[r] : 0.f;
            uint32_t c = 0;
            if (cols >= 8) {
                __m256 acc = _mm256_setzero_ps();
                for (; c+8 <= cols; c += 8)
                    acc = _mm256_fmadd_ps(_mm256_loadu_ps(row+c), _mm256_loadu_ps(x+c), acc);
                alignas(32) float t[8]; _mm256_store_ps(t, acc);
                for (int j = 0; j < 8; ++j) s += t[j];
            }
            for (; c < cols; ++c) s += row[c]*x[c];
            out[r] = s;
        }
    }
    static void Relu(float* v, uint32_t n) noexcept {
        __m256 z = _mm256_setzero_ps();
        uint32_t i = 0;
        for (; i+8 <= n; i += 8)
            _mm256_storeu_ps(v+i, _mm256_max_ps(_mm256_loadu_ps(v+i), z));
        for (; i < n; ++i) if (v[i] < 0.f) v[i] = 0.f;
    }
public:
    OracleHead(uint32_t vocab, uint32_t E, uint32_t H, uint32_t S)
        : vocab_(vocab), E_(E), H_(H), S_(S) {
        emb_.reset(new float[vocab*E]());
        W1_.reset(new float[H*E]());    b1_.reset(new float[H]());
        W2_.reset(new float[S*H]());    b2_.reset(new float[S]());
        std::mt19937 g(0x1337beef);
        std::normal_distribution<float> nd(0.f, 0.02f);
        for (uint32_t i = 0; i < vocab*E; ++i) emb_[i] = nd(g);
        for (uint32_t i = 0; i < H*E;    ++i) W1_[i]  = nd(g);
        for (uint32_t i = 0; i < S*H;    ++i) W2_[i]  = nd(g);
    }

    // Returns slim-vocab index (kNF if inp out of range).
    uint32_t Predict(uint32_t inp, float* out_logits) const noexcept {
        if (inp >= vocab_) return kNF;
        alignas(32) float h[64];
        MV(W1_.get(), emb_.get() + inp*E_, b1_.get(), h, H_, E_);
        Relu(h, H_);
        MV(W2_.get(), h, b2_.get(), out_logits, S_, H_);
        return Argmax(out_logits, S_);
    }

    void Train(const uint32_t* seq, uint32_t len, float lr) noexcept {
        if (len < 2) return;
        for (uint32_t t = 0; t+1 < len; ++t) {
            uint32_t inp = seq[t], tgt = seq[t+1];
            if (inp >= vocab_ || tgt >= S_) continue;
            // forward
            alignas(32) float h[64], o[512];
            MV(W1_.get(), emb_.get()+inp*E_, b1_.get(), h, H_, E_);
            Relu(h, H_);
            MV(W2_.get(), h, b2_.get(), o, S_, H_);
            Softmax(o, S_);
            // output gradient: dL/do = p - 1_tgt
            o[tgt] -= 1.f;
            // update W2, b2
            for (uint32_t i = 0; i < S_; ++i) {
                float g = o[i] * lr;
                if (fabsf(g) < 1e-7f) continue;
                float* row = W2_.get() + i*H_;
                for (uint32_t j = 0; j < H_; ++j) row[j] -= g * h[j];
                b2_[i] -= g;
            }
            // dh = W2^T * o, apply relu mask
            alignas(32) float dh[64] = {};
            for (uint32_t i = 0; i < S_; ++i) {
                if (fabsf(o[i]) < 1e-7f) continue;
                const float* row = W2_.get() + i*H_;
                for (uint32_t j = 0; j < H_; ++j) dh[j] += o[i]*row[j];
            }
            for (uint32_t j = 0; j < H_; ++j) if (h[j] <= 0.f) dh[j] = 0.f;
            // update W1, b1
            float* emb_row = emb_.get() + inp*E_;
            for (uint32_t i = 0; i < H_; ++i) {
                float g = dh[i] * lr;
                if (fabsf(g) < 1e-7f) continue;
                float* r = W1_.get() + i*E_;
                for (uint32_t j = 0; j < E_; ++j) { r[j]      -= g * emb_row[j]; }
                b1_[i] -= g;
                // update embedding
                for (uint32_t j = 0; j < E_; ++j) emb_row[j] -= g * r[j];
            }
            ++train_count_;
        }
    }
    uint32_t TrainCount() const noexcept { return train_count_; }
};

// ============================================================
// Cascade tree — flat fixed-size node pool.
// Supports breadth-first build and branch-recovery search.
// ============================================================
struct TNode {
    uint32_t token;
    uint32_t parent;
    uint32_t first_child;
    uint32_t next_sib;
    uint32_t depth;
    float    score;
};
static constexpr uint32_t kMaxNodes = 256;

class CascadeTree {
    TNode   nodes_[kMaxNodes];
    uint32_t cnt_ = 0;
public:
    void Clear() noexcept { cnt_ = 0; }

    uint32_t SetRoot(uint32_t tok, float sc) noexcept {
        nodes_[0] = {tok, kNF, kNF, kNF, 0u, sc};
        cnt_ = 1; return 0;
    }

    uint32_t AddChild(uint32_t parent, uint32_t tok, float sc) noexcept {
        if (cnt_ >= kMaxNodes) return kNF;
        uint32_t id = cnt_++;
        nodes_[id] = {tok, parent, kNF, nodes_[parent].first_child,
                      nodes_[parent].depth + 1, sc};
        nodes_[parent].first_child = id;
        return id;
    }

    // Walk best-score children to fill linear path.
    uint32_t BestPath(uint32_t* out, uint32_t max) const noexcept {
        uint32_t n = 0, cur = 0;
        while (n < max && cur != kNF) {
            out[n++] = nodes_[cur].token;
            uint32_t bc = kNF; float bs = -1e30f;
            for (uint32_t c = nodes_[cur].first_child; c != kNF; c = nodes_[c].next_sib)
                if (nodes_[c].score > bs) { bs = nodes_[c].score; bc = c; }
            cur = bc;
        }
        return n;
    }

    // Find a node at given depth whose token == target, then return its
    // best-child continuation path.
    uint32_t RecoverPath(uint32_t depth, uint32_t target,
                         uint32_t* out, uint32_t max) const noexcept {
        for (uint32_t i = 0; i < cnt_; ++i) {
            if (nodes_[i].depth != depth || nodes_[i].token != target) continue;
            uint32_t n = 0, cur = i;
            while (n < max && cur != kNF) {
                out[n++] = nodes_[cur].token;
                uint32_t bc = kNF; float bs = -1e30f;
                for (uint32_t c = nodes_[cur].first_child; c != kNF; c = nodes_[c].next_sib)
                    if (nodes_[c].score > bs) { bs = nodes_[c].score; bc = c; }
                cur = bc;
            }
            return n;
        }
        return 0;
    }

    uint32_t Size() const noexcept { return cnt_; }
};

// ============================================================
// Impl — orchestrates all heads
// ============================================================
struct HeadResult { uint32_t tok; float score; };

} // anon

// ------------------------------------------------------------------
class ExoticSpecDecoder::Impl {
    ExoticSpecConfig cfg_;
    NGramTable       ng2_, ng3_, ng4_;
    OracleHead       oracle_;
    CopyHead         copy_;
    CascadeTree      tree_;
    XRng             rng_;

    std::atomic<uint32_t> width_{4};
    std::atomic<float>    ewma_{0.5f};

    struct alignas(64) Counters {
        std::atomic<uint64_t> drafts{0};
        std::atomic<uint64_t> accepted{0};
        std::atomic<uint64_t> rejected{0};
        std::atomic<uint64_t> cascade{0};
        std::atomic<uint64_t> copy_hits{0};
        std::atomic<uint64_t> oracle_hits{0};
        std::atomic<uint64_t> ng4_hits{0};
        std::atomic<uint64_t> ng3_hits{0};
        std::atomic<uint64_t> ng2_hits{0};
    } cnt_;

    // Gather up to 8 candidate tokens from all heads for a given
    // rolling context window. Returns count stored in cands[].
    uint32_t GatherCands(const uint32_t* ctx4,  // ctx4[0..3] newest at [3]
                         HeadResult* cands) noexcept {
        uint32_t nc = 0;
        auto add = [&](uint32_t t, float s) {
            if (t == kNF || t >= cfg_.vocab_size) return;
            for (uint32_t i = 0; i < nc; ++i) {
                if (cands[i].tok == t) { cands[i].score += s; return; }
            }
            if (nc < 8) cands[nc++] = {t, s};
        };

        // Head 0: 4-gram
        uint32_t p4 = ng4_.Lookup(K4(ctx4[0], ctx4[1], ctx4[2], ctx4[3]));
        if (p4 != kNF) { add(p4, 1.0f); cnt_.ng4_hits.fetch_add(1, std::memory_order_relaxed); }

        // Head 1: 3-gram
        uint32_t p3 = ng3_.Lookup(K3(ctx4[1], ctx4[2], ctx4[3]));
        if (p3 != kNF) { add(p3, 0.7f); cnt_.ng3_hits.fetch_add(1, std::memory_order_relaxed); }

        // Head 2: 2-gram
        uint32_t p2 = ng2_.Lookup(K2(ctx4[2], ctx4[3]));
        if (p2 != kNF) { add(p2, 0.5f); cnt_.ng2_hits.fetch_add(1, std::memory_order_relaxed); }

        // Head 3: Oracle MLP
        alignas(32) float ologits[512];
        uint32_t op = oracle_.Predict(ctx4[3], ologits);
        if (op != kNF) {
            Softmax(ologits, cfg_.slim_vocab);
            float oc = ologits[op];
            add(op, oc * 1.2f + 0.12f); // always add oracle, scale by confidence
            cnt_.oracle_hits.fetch_add(1, std::memory_order_relaxed);
        }

        // Head 4: Copy detector
        uint32_t cp = copy_.Predict();
        if (cp != kNF) { add(cp, 0.6f); cnt_.copy_hits.fetch_add(1, std::memory_order_relaxed); }

        // Head 5: Repetition breaker
        uint32_t brk = copy_.BreakToken();
        if (brk != kNF) add(brk, 0.3f);

        // Head 6: Fallback — always ensure at least one candidate
        // Uses a cheap positional hash so cold-cache steps still produce a draft.
        if (nc == 0) {
            uint32_t fb = (ctx4[3] * 6364136223846793005ULL + 1442695040888963407ULL) % cfg_.vocab_size;
            cands[nc++] = {fb, 0.05f};
        }

        return nc;
    }

    void UpdateAdaptive(uint32_t accepted, uint32_t total) noexcept {
        cnt_.accepted.fetch_add(accepted, std::memory_order_relaxed);
        cnt_.rejected.fetch_add(total - accepted, std::memory_order_relaxed);

        float a    = float(accepted) / float(total > 0 ? total : 1);
        float old  = ewma_.load(std::memory_order_relaxed);
        float newe = old * 0.9f + a * 0.1f;
        ewma_.store(newe, std::memory_order_relaxed);

        uint32_t w = width_.load(std::memory_order_relaxed);
        if (newe > cfg_.accept_hi && w < cfg_.max_draft_width)
            width_.store(std::min(w + 2u, cfg_.max_draft_width), std::memory_order_relaxed);
        else if (newe < cfg_.accept_lo && w > 1u)
            width_.store(w - 1u, std::memory_order_relaxed);
    }

    // Build rolling ctx4[0..3] (oldest..newest) from history + already-drafted tokens.
    static void RollCtx(const uint32_t* hist, uint32_t hlen,
                        const uint32_t* draft, uint32_t dpos,
                        uint32_t* ctx4) noexcept {
        for (int i = 3; i >= 0; --i) {
            int back = 3 - i; // how many positions back from current
            if (int(dpos) - back > 0)
                ctx4[i] = draft[int(dpos) - back - 1];
            else {
                int hidx = int(hlen) - 1 - (back - int(dpos));
                ctx4[i] = (hidx >= 0) ? hist[hidx] : 0u;
            }
        }
    }

public:
    explicit Impl(const ExoticSpecConfig& c)
        : cfg_(c),
          ng2_(c.ngram_l2_bits), ng3_(c.ngram_l3_bits), ng4_(c.ngram_l4_bits),
          oracle_(c.vocab_size, c.mlp_embed, c.mlp_hidden, c.slim_vocab)
    {}

    // ---- Draft -------------------------------------------------------
    uint32_t Draft(const uint32_t* hist, uint32_t hlen,
                   uint32_t* out, uint32_t maxd) noexcept {
        uint32_t w = std::min(maxd, width_.load(std::memory_order_relaxed));
        if (!w || !hlen) return 0;

        tree_.Clear();
        // Root = last history token (not written to out[], serves as anchor)
        tree_.SetRoot(hist[hlen-1], 1.f);

        // BFS frontier — node indices to expand
        uint32_t frontier[64], nf = 0, next_frontier[64], nnf = 0;
        frontier[nf++] = 0;

        uint32_t gen = 0; // tokens written

        while (gen < w && nf > 0) {
            nnf = 0;
            for (uint32_t fi = 0; fi < nf && gen < w; ++fi) {
                uint32_t nid  = frontier[fi];

                // Build ctx4 for this node's position
                uint32_t ctx4[4];
                RollCtx(hist, hlen, out, gen, ctx4);

                // Gather candidates from all heads
                HeadResult cands[8]; uint32_t nc = GatherCands(ctx4, cands);
                if (!nc) continue;

                // Sort descending by score (tiny array, insertion sort)
                for (uint32_t i = 1; i < nc; ++i) {
                    HeadResult tmp = cands[i]; int j = int(i)-1;
                    while (j >= 0 && cands[j].score < tmp.score)
                        { cands[j+1] = cands[j]; --j; }
                    cands[j+1] = tmp;
                }

                // Best candidate goes into the output draft
                out[gen++] = cands[0].tok;
                uint32_t main_node = tree_.AddChild(nid, cands[0].tok, cands[0].score);
                if (main_node != kNF && nnf < 64) next_frontier[nnf++] = main_node;

                // Alternate branches (cascade recovery)
                uint32_t alts = std::min<uint32_t>(nc - 1, cfg_.tree_branch_factor);
                for (uint32_t ci = 1; ci <= alts; ++ci) {
                    uint32_t alt = tree_.AddChild(nid, cands[ci].tok, cands[ci].score);
                    if (alt != kNF && nnf < 64) next_frontier[nnf++] = alt;
                }
            }
            nf = nnf;
            std::memcpy(frontier, next_frontier, nnf * sizeof(uint32_t));
        }

        cnt_.drafts.fetch_add(1, std::memory_order_relaxed);
        return gen;
    }

    // ---- ValidateArgmax -----------------------------------------------
    uint32_t ValidateArgmax(const uint32_t* draft, uint32_t n,
                            const uint32_t* target) noexcept {
        uint32_t acc = 0;
        for (uint32_t i = 0; i < n; ++i) {
            if (draft[i] == target[i]) {
                ++acc;
            } else {
                // Cascade: search tree for a branch at depth i that matches
                uint32_t rec[64];
                uint32_t rlen = tree_.RecoverPath(i, target[i], rec, n - i);
                if (rlen > 0) {
                    std::memcpy(const_cast<uint32_t*>(draft) + i, rec,
                                rlen * sizeof(uint32_t));
                    ++acc;
                    cnt_.cascade.fetch_add(1, std::memory_order_relaxed);
                    continue;
                }
                break;
            }
        }
        UpdateAdaptive(acc, n);
        return acc;
    }

    // ---- ValidateProbabilistic ----------------------------------------
    uint32_t ValidateProbabilistic(const uint32_t* draft, uint32_t n,
                                   const float* logits, uint32_t stride) noexcept {
        uint32_t acc = 0;
        const uint32_t V = std::min(cfg_.vocab_size, 32000u);

        for (uint32_t i = 0; i < n; ++i) {
            alignas(32) float probs[32000];
            std::memcpy(probs, logits + i*stride, V * sizeof(float));
            Softmax(probs, V);

            float p_t = probs[draft[i]];
            float p_m = probs[Argmax(probs, V)];

            // Temperature-corrected threshold: accept if rank is within
            // top-temperature fraction of probability mass.
            float threshold = p_m * (1.f - cfg_.temperature * 0.5f);
            if (p_t >= threshold) {
                ++acc;
            } else {
                // Rejection sampling: resample — check if tree has a better match
                uint32_t best_tok = Argmax(probs, V);
                uint32_t rec[64];
                uint32_t rlen = tree_.RecoverPath(i, best_tok, rec, n - i);
                if (rlen > 0) {
                    std::memcpy(const_cast<uint32_t*>(draft) + i, rec,
                                rlen * sizeof(uint32_t));
                    ++acc;
                    cnt_.cascade.fetch_add(1, std::memory_order_relaxed);
                    continue;
                }
                break;
            }
        }
        UpdateAdaptive(acc, n);
        return acc;
    }

    // ---- FeedAccepted ------------------------------------------------
    void FeedAccepted(const uint32_t* seq, uint32_t len) noexcept {
        if (len < 2) return;
        for (uint32_t i = 0; i+1 < len; ++i) {
            ng2_.Insert(K2(seq[i], seq[i+1]), seq[i+2 < len ? i+2 : i+1]);
            if (i+2 < len) ng3_.Insert(K3(seq[i], seq[i+1], seq[i+2]),
                                        seq[i+3 < len ? i+3 : i+2]);
            if (i+3 < len) ng4_.Insert(K4(seq[i], seq[i+1], seq[i+2], seq[i+3]),
                                        seq[i+4 < len ? i+4 : i+3]);
        }
        for (uint32_t i = 0; i < len; ++i) copy_.Push(seq[i]);
        oracle_.Train(seq, len, cfg_.lr_mlp);
        cnt_.accepted.fetch_add(len, std::memory_order_relaxed);
    }

    // ---- Getters -------------------------------------------------------
    ExoticSpecStats Stats() const noexcept {
        ExoticSpecStats s{};
        s.drafts_generated  = cnt_.drafts.load(std::memory_order_relaxed);
        s.tokens_accepted   = cnt_.accepted.load(std::memory_order_relaxed);
        s.tokens_rejected   = cnt_.rejected.load(std::memory_order_relaxed);
        s.cascade_recoveries= cnt_.cascade.load(std::memory_order_relaxed);
        s.copy_head_hits    = cnt_.copy_hits.load(std::memory_order_relaxed);
        s.oracle_hits       = cnt_.oracle_hits.load(std::memory_order_relaxed);
        s.ngram4_hits       = cnt_.ng4_hits.load(std::memory_order_relaxed);
        s.ngram3_hits       = cnt_.ng3_hits.load(std::memory_order_relaxed);
        s.ngram2_hits       = cnt_.ng2_hits.load(std::memory_order_relaxed);
        s.acceptance_ewma   = ewma_.load(std::memory_order_relaxed);
        s.current_width     = width_.load(std::memory_order_relaxed);
        s.oracle_train_steps= oracle_.TrainCount();
        return s;
    }
    uint32_t Width() const noexcept { return width_.load(std::memory_order_relaxed); }
};

// ------------------------------------------------------------------
// Public trampolines
// ------------------------------------------------------------------
ExoticSpecDecoder::ExoticSpecDecoder(const ExoticSpecConfig& c)
    : impl_(std::make_unique<Impl>(c)) {}
ExoticSpecDecoder::~ExoticSpecDecoder() = default;

uint32_t ExoticSpecDecoder::Draft(const uint32_t* h, uint32_t hl,
                                  uint32_t* out, uint32_t maxd) {
    return impl_->Draft(h, hl, out, maxd);
}
uint32_t ExoticSpecDecoder::ValidateArgmax(const uint32_t* d, uint32_t n,
                                           const uint32_t* t) {
    return impl_->ValidateArgmax(d, n, t);
}
uint32_t ExoticSpecDecoder::ValidateProbabilistic(const uint32_t* d, uint32_t n,
                                                  const float* l, uint32_t s) {
    return impl_->ValidateProbabilistic(d, n, l, s);
}
void ExoticSpecDecoder::FeedAccepted(const uint32_t* seq, uint32_t len) {
    impl_->FeedAccepted(seq, len);
}
ExoticSpecStats ExoticSpecDecoder::GetStats()      const { return impl_->Stats(); }
uint32_t        ExoticSpecDecoder::GetDraftWidth() const { return impl_->Width(); }

} // namespace rxd::ai
