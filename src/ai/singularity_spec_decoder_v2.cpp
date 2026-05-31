#include "singularity_spec_decoder_v2.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstring>
#include <immintrin.h>
#include <memory>
#include <random>
#include <thread>
#include <condition_variable>
#include <mutex>

#ifdef _MSC_VER
#include <intrin.h>
inline int sg2_ctz(int x) { unsigned long r; _BitScanForward(&r, (unsigned long)x); return (int)r; }
inline int sg2_clz(int x) { unsigned long r; _BitScanReverse(&r, (unsigned long)x); return 31 - (int)r; }
#else
inline int sg2_ctz(int x) { return __builtin_ctz(x); }
inline int sg2_clz(int x) { return __builtin_clz(x); }
#endif
static inline uint32_t Log2Floor(uint32_t v) {
    unsigned long r = 0;
#ifdef _MSC_VER
    _BitScanReverse(&r, v);
#else
    r = 31 - __builtin_clz(v);
#endif
    return (uint32_t)r;
}

namespace rxd::ai {
namespace {

constexpr uint32_t kNF = 0xFFFFFFFFu;
constexpr uint64_t kEmpty = 0xFFFFFFFFFFFFFFFFull;

// ------------------------------------------------------------------
// Hash mixing (MurmurHash3 finalizer)
// ------------------------------------------------------------------
inline uint64_t Mix(uint64_t x) {
    x ^= x >> 33; x *= 0xff51afd7ed558ccdULL;
    x ^= x >> 33; x *= 0xc4ceb9fe1a85ec53ULL;
    x ^= x >> 33; return x;
}
inline uint64_t Key2(uint32_t a, uint32_t b) { return Mix((uint64_t(a) << 32) | b); }
inline uint64_t Key3(uint32_t a, uint32_t b, uint32_t c) { return Mix(Key2(a,b) ^ (c * 0x9e3779b97f4a7c15ULL)); }
inline uint64_t Key4(uint32_t a, uint32_t b, uint32_t c, uint32_t d) { return Mix(Key3(a,b,c) ^ (d * 0x9e3779b97f4a7c15ULL)); }
inline uint64_t Key5(uint32_t a, uint32_t b, uint32_t c, uint32_t d, uint32_t e) { return Mix(Key4(a,b,c,d) ^ (e * 0x9e3779b97f4a7c15ULL)); }

// ------------------------------------------------------------------
// 16-gram key: pack into two 64-bit hashes
// ------------------------------------------------------------------
inline std::pair<uint64_t,uint64_t> Key16(const uint32_t* seq) {
    uint64_t h1 = 0x9e3779b97f4a7c15ULL, h2 = 0xbf58476d1ce4e5b9ULL;
    for (int i = 0; i < 8; ++i)  h1 = Mix(h1 ^ (seq[i] + 0x9e3779b97f4a7c15ULL));
    for (int i = 8; i < 16; ++i) h2 = Mix(h2 ^ (seq[i] + 0xbf58476d1ce4e5b9ULL));
    return {h1, h2};
}

// ------------------------------------------------------------------
// SIMD detection & primitives
// ------------------------------------------------------------------
inline bool HasAVX512() {
#ifdef __AVX512F__
    return true;
#else
    static int c = -1;
    if (c == -1) {
        int cpui[4] = {0};
        __cpuidex(cpui, 7, 0);
        c = (cpui[1] & (1 << 16)) ? 1 : 0;
    }
    return c == 1;
#endif
}

inline float FastExp(float x) {
    x = std::max(-80.0f, std::min(80.0f, x));
    union { float f; int32_t i; } u;
    u.i = (int32_t)(12102203.161561485f * x + 1064866805.0f);
    return u.f;
}

// AVX-512 argmax: 16-wide
uint32_t ArgmaxSIMD(const float* data, uint32_t n) {
    if (!n) return 0;
    if (HasAVX512() && n >= 16) {
        __m512 best = _mm512_loadu_ps(data);
        __m512i idx = _mm512_setr_epi32(0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15);
        __m512i inc = _mm512_set1_epi32(16), cur = idx;
        uint32_t i = 0;
        for (; i + 16 <= n; i += 16) {
            __m512 v = _mm512_loadu_ps(data + i);
            __mmask16 gt = _mm512_cmp_ps_mask(v, best, _CMP_GT_OQ);
            best = _mm512_mask_blend_ps(gt, best, v);
            idx = _mm512_mask_blend_epi32(gt, idx, cur);
            cur = _mm512_add_epi32(cur, inc);
        }
        alignas(64) float bf[16]; alignas(64) int32_t bi[16];
        _mm512_store_ps(bf, best); _mm512_store_epi32(bi, idx);
        float mv = bf[0]; uint32_t mi = bi[0];
        for (int j = 1; j < 16; ++j) if (bf[j] > mv) { mv = bf[j]; mi = bi[j]; }
        for (; i < n; ++i) if (data[i] > mv) { mv = data[i]; mi = i; }
        return mi;
    }
    uint32_t best = 0;
    for (uint32_t i = 1; i < n; ++i) if (data[i] > data[best]) best = i;
    return best;
}

// AVX-512 softmax with 16-wide exp
void SoftmaxSIMD(float* data, uint32_t n) {
    if (!n) return;
    float mx = data[0];
    for (uint32_t i = 1; i < n; ++i) mx = std::max(mx, data[i]);
    float sum = 0.0f;
    for (uint32_t i = 0; i < n; ++i) { data[i] = FastExp(data[i] - mx); sum += data[i]; }
    float inv = 1.0f / sum;
    if (HasAVX512() && n >= 16) {
        __m512 vinv = _mm512_set1_ps(inv);
        uint32_t i = 0;
        for (; i + 16 <= n; i += 16)
            _mm512_storeu_ps(data + i, _mm512_mul_ps(_mm512_loadu_ps(data + i), vinv));
        for (; i < n; ++i) data[i] *= inv;
    } else {
        for (uint32_t i = 0; i < n; ++i) data[i] *= inv;
    }
}

// ------------------------------------------------------------------
// 8-way SIMD Cuckoo table (128-byte bucket, two cache lines)
// ------------------------------------------------------------------
struct alignas(128) Bucket8 {
    uint64_t key[8];
    uint32_t val[8];
    uint32_t freq[8];
};

class CuckooTable8 {
    std::unique_ptr<Bucket8[]> b_;
    size_t mask_;
public:
    explicit CuckooTable8(uint32_t log2) {
        size_t n = 1u << log2; b_.reset(new Bucket8[n]()); mask_ = n - 1;
        for (size_t i = 0; i < n; ++i) for (int j = 0; j < 8; ++j) b_[i].key[j] = kEmpty;
    }
    uint32_t Lookup(uint64_t key) const {
        uint64_t h = Mix(key);
        for (int r = 0; r < 4; ++r) {
            size_t idx = (h + r) & mask_;
            const Bucket8& b = b_[idx];
            __m256i kv0 = _mm256_load_si256((__m256i*)b.key);
            __m256i kv1 = _mm256_load_si256((__m256i*)(b.key + 4));
            __m256i t = _mm256_set1_epi64x(key);
            __m256i c0 = _mm256_cmpeq_epi64(kv0, t);
            __m256i c1 = _mm256_cmpeq_epi64(kv1, t);
            int m0 = _mm256_movemask_pd(_mm256_castsi256_pd(c0));
            int m1 = _mm256_movemask_pd(_mm256_castsi256_pd(c1));
            if (m0) return b.val[sg2_ctz(m0)];
            if (m1) return b.val[4 + sg2_ctz(m1)];
        }
        return kNF;
    }
    void Insert(uint64_t key, uint32_t val) {
        uint64_t h = Mix(key);
        for (int r = 0; r < 4; ++r) {
            size_t idx = (h + r) & mask_;
            Bucket8& b = b_[idx];
            __m256i kv0 = _mm256_load_si256((__m256i*)b.key);
            __m256i kv1 = _mm256_load_si256((__m256i*)(b.key + 4));
            __m256i t = _mm256_set1_epi64x(key);
            int m0 = _mm256_movemask_pd(_mm256_castsi256_pd(_mm256_cmpeq_epi64(kv0, t)));
            int m1 = _mm256_movemask_pd(_mm256_castsi256_pd(_mm256_cmpeq_epi64(kv1, t)));
            if (m0) { int s = sg2_ctz(m0); b.val[s] = val; b.freq[s]++; return; }
            if (m1) { int s = 4 + sg2_ctz(m1); b.val[s] = val; b.freq[s]++; return; }
            int ev = 0; uint32_t mf = b.freq[0];
            for (int j = 1; j < 8; ++j) if (b.freq[j] < mf) { mf = b.freq[j]; ev = j; }
            if (mf < 3) { b.key[ev] = key; b.val[ev] = val; b.freq[ev] = 1; return; }
        }
        size_t idx = h & mask_; int slot = int(h & 7);
        b_[idx].key[slot] = key; b_[idx].val[slot] = val; b_[idx].freq[slot] = 1;
    }
};

// ------------------------------------------------------------------
// 16-gram cache: two-hash cuckoo (catches function bodies)
// ------------------------------------------------------------------
class SedecimGramCache {
    CuckooTable8 tbl1_, tbl2_;
public:
    explicit SedecimGramCache(uint32_t log2) : tbl1_(log2), tbl2_(log2) {}
    uint32_t Lookup(const uint32_t* seq) const {
        auto [h1, h2] = Key16(seq);
        uint32_t r = tbl1_.Lookup(h1);
        if (r != kNF) return r;
        return tbl2_.Lookup(h2);
    }
    void Insert(const uint32_t* seq, uint32_t nxt) {
        auto [h1, h2] = Key16(seq);
        tbl1_.Insert(h1, nxt);
        tbl2_.Insert(h2, nxt);
    }
};

// ------------------------------------------------------------------
// Octo-Gram: 8-token exact match
// ------------------------------------------------------------------
class OctoGramCache {
    CuckooTable8 tbl_;
public:
    explicit OctoGramCache(uint32_t log2) : tbl_(log2) {}
    static uint64_t Hash8(const uint32_t* seq) {
        uint64_t h = 0x9e3779b97f4a7c15ULL;
        for (int i = 0; i < 8; ++i) h = Mix(h ^ (seq[i] + 0x9e3779b97f4a7c15ULL));
        return h;
    }
    uint32_t Lookup(const uint32_t* seq) const { return tbl_.Lookup(Hash8(seq)); }
    void Insert(const uint32_t* seq, uint32_t nxt) { tbl_.Insert(Hash8(seq), nxt); }
};

// ------------------------------------------------------------------
// MinHash Semantic Cache v2: 128-token fingerprint, 16 signatures
// ------------------------------------------------------------------
class MinHashCacheV2 {
    struct alignas(64) Row {
        uint32_t sig[16];
        uint32_t topk[16];
        float    prob[16];
        uint32_t freq;
    };
    std::unique_ptr<Row[]> mem_;
    uint32_t mask_;
public:
    explicit MinHashCacheV2(uint32_t log2_slots) {
        uint32_t n = 1u << log2_slots;
        mem_.reset(new Row[n]()); mask_ = n - 1;
        for (uint32_t i = 0; i < n; ++i) mem_[i].freq = 0;
    }
    static void Signature(const uint32_t* seq, uint32_t len, uint32_t* out_sig) {
        for (int p = 0; p < 16; ++p) {
            uint32_t mn = 0xFFFFFFFFu;
            uint32_t prime = 0x01000193u;
            uint32_t h = p * 0x811c9dc5u;
            for (uint32_t i = 0; i < len; ++i) {
                h = (h * prime) ^ (seq[i] + p * 0x9e3779b9u);
                mn = std::min(mn, h);
            }
            out_sig[p] = mn;
        }
    }
    void Store(const uint32_t* seq, uint32_t len, const uint32_t* topk, const float* prob) {
        uint32_t sig[16]; Signature(seq, len, sig);
        uint32_t h = sig[0] ^ sig[1] ^ sig[2] ^ sig[3] ^ sig[4] ^ sig[5] ^ sig[6] ^ sig[7];
        uint32_t idx = h & mask_;
        Row& r = mem_[idx];
        if (r.freq < 2) {
            std::memcpy(r.sig, sig, sizeof(sig));
            for (int i = 0; i < 16; ++i) { r.topk[i] = topk[i]; r.prob[i] = prob[i]; }
            r.freq = 1;
        } else {
            int match = 0;
            for (int i = 0; i < 16; ++i) if (r.sig[i] == sig[i]) ++match;
            if (match >= 12) {
                for (int i = 0; i < 16; ++i) { r.topk[i] = topk[i]; r.prob[i] = prob[i]; }
                r.freq++;
            } else if (r.freq > 1) r.freq--;
        }
    }
    uint32_t Retrieve(const uint32_t* seq, uint32_t len, uint32_t* out_tok, float* out_prob, uint32_t max_out) const {
        uint32_t sig[16]; Signature(seq, len, sig);
        uint32_t h = sig[0] ^ sig[1] ^ sig[2] ^ sig[3] ^ sig[4] ^ sig[5] ^ sig[6] ^ sig[7];
        uint32_t idx = h & mask_;
        const Row& r = mem_[idx];
        if (r.freq == 0) return 0;
        int match = 0; for (int i = 0; i < 16; ++i) if (r.sig[i] == sig[i]) ++match;
        if (match < 12) return 0;
        uint32_t n = std::min(max_out, 16u);
        for (uint32_t i = 0; i < n; ++i) { out_tok[i] = r.topk[i]; out_prob[i] = r.prob[i]; }
        return n;
    }
};

// ------------------------------------------------------------------
// KV-Cache v2: differentiable key-value memory with LFU + time decay
// ------------------------------------------------------------------
class KVCacheV2 {
    struct alignas(64) Row {
        float key[128];
        uint32_t val;
        uint32_t freq;
        uint32_t timestamp;
    };
    std::unique_ptr<Row[]> mem_;
    uint32_t mask_;
    std::atomic<uint32_t> tick_{0};
public:
    explicit KVCacheV2(uint32_t slots) {
        uint32_t n = 1; while (n < slots) n <<= 1;
        mem_.reset(new Row[n]()); mask_ = n - 1;
        for (uint32_t i = 0; i < n; ++i) { mem_[i].val = kNF; mem_[i].freq = 0; }
    }
    void Store(const float* key, uint32_t tok) {
        uint32_t h = 0;
        for (uint32_t i = 0; i < 128; ++i) h = h * 31 + (uint32_t)(key[i] * 10000.0f);
        uint32_t idx = h & mask_;
        Row& r = mem_[idx];
        uint32_t t = tick_.fetch_add(1, std::memory_order_relaxed);
        if (r.val == kNF || r.freq < 2 || (t - r.timestamp) > 10000) {
            std::memcpy(r.key, key, 128 * sizeof(float));
            r.val = tok; r.freq = 1; r.timestamp = t;
        } else {
            r.freq = std::max(1u, r.freq - (t - r.timestamp) / 1000);
            if (r.freq < 2) {
                std::memcpy(r.key, key, 128 * sizeof(float));
                r.val = tok; r.freq = 1; r.timestamp = t;
            }
        }
    }
    uint32_t Retrieve(const float* key) const {
        uint32_t h = 0;
        for (uint32_t i = 0; i < 128; ++i) h = h * 31 + (uint32_t)(key[i] * 10000.0f);
        uint32_t idx = h & mask_;
        const Row& r = mem_[idx];
        if (r.val == kNF) return kNF;
        float sim = 0.0f;
        for (uint32_t i = 0; i < 128; ++i) sim += r.key[i] * key[i];
        return (sim > 0.85f) ? r.val : kNF;
    }
};

// ------------------------------------------------------------------
// BFloat16 MLP v2: 3-layer with residual, GELU approx, 512 hidden
// ------------------------------------------------------------------
class BFloat16MLPV2 {
    uint32_t embed_, hidden_, out_;
    std::unique_ptr<uint16_t[]> w1_, w2_, w3_;
    std::unique_ptr<float[]> b1_, b2_, b3_;
    std::unique_ptr<float[]> emb_;

    static inline float Bf16ToF32(uint16_t v) {
        union { float f; uint32_t u; } x; x.u = uint32_t(v) << 16; return x.f;
    }
    static inline uint16_t F32ToBf16(float v) {
        union { float f; uint32_t u; } x; x.f = v; return uint16_t(x.u >> 16);
    }
    static inline float GELU(float x) {
        float c = 0.044715f * x * x * x;
        float t = 0.79788456f * (x + c);
        return x * 0.5f * (1.0f + (t > 4.0f ? 1.0f : (t < -4.0f ? -1.0f : t / (1.0f + FastExp(-2.0f * t)))));
    }

    void MatVecBf16(const uint16_t* w, const float* x, float* out,
                    uint32_t rows, uint32_t cols) {
        for (uint32_t r = 0; r < rows; ++r) {
            float sum = 0.0f;
            uint32_t c = 0;
            const uint16_t* row = w + r * cols;
            if (HasAVX512() && cols >= 16) {
                __m512 acc = _mm512_setzero_ps();
                for (; c + 16 <= cols; c += 16) {
                    __m256i w16 = _mm256_loadu_si256((__m256i*)(row + c));
                    __m512 w32 = _mm512_castsi512_ps(_mm512_slli_epi32(
                        _mm512_cvtepu16_epi32(w16), 16));
                    acc = _mm512_fmadd_ps(w32, _mm512_loadu_ps(x + c), acc);
                }
                alignas(64) float t[16]; _mm512_store_ps(t, acc);
                for (int j = 0; j < 16; ++j) sum += t[j];
            }
            for (; c < cols; ++c) sum += Bf16ToF32(row[c]) * x[c];
            out[r] = sum;
        }
    }
public:
    BFloat16MLPV2(uint32_t vocab, uint32_t embed, uint32_t hidden, uint32_t out)
        : embed_(embed), hidden_(hidden), out_(out) {
        w1_.reset(new uint16_t[hidden * embed]());
        b1_.reset(new float[hidden]());
        w2_.reset(new uint16_t[hidden * hidden]());
        b2_.reset(new float[hidden]());
        w3_.reset(new uint16_t[out * hidden]());
        b3_.reset(new float[out]());
        emb_.reset(new float[vocab * embed]());
        std::mt19937 rng(42);
        std::normal_distribution<float> d(0.0f, 0.02f);
        for (uint32_t i = 0; i < vocab * embed; ++i) emb_[i] = d(rng);
        for (uint32_t i = 0; i < hidden * embed; ++i) w1_[i] = F32ToBf16(d(rng));
        for (uint32_t i = 0; i < hidden * hidden; ++i) w2_[i] = F32ToBf16(d(rng));
        for (uint32_t i = 0; i < out * hidden; ++i) w3_[i] = F32ToBf16(d(rng));
    }
    uint32_t Predict(const uint32_t* hist, uint32_t hist_len, float* out_logits) {
        if (!hist_len) return kNF;
        uint32_t last = hist[hist_len - 1];
        if (last >= embed_) return kNF;
        alignas(64) float x[512], h1[512], h2[512];
        std::memcpy(x, emb_.get() + last * embed_, embed_ * sizeof(float));
        MatVecBf16(w1_.get(), x, h1, hidden_, embed_);
        for (uint32_t i = 0; i < hidden_; ++i) h1[i] = GELU(h1[i] + b1_[i]);
        MatVecBf16(w2_.get(), h1, h2, hidden_, hidden_);
        for (uint32_t i = 0; i < hidden_; ++i) {
            h2[i] = GELU(h2[i] + b2_[i]);
            if (i < embed_) h2[i] += x[i];
        }
        MatVecBf16(w3_.get(), h2, out_logits, out_, hidden_);
        for (uint32_t i = 0; i < out_; ++i) out_logits[i] += b3_[i];
        return ArgmaxSIMD(out_logits, out_);
    }
    void Train(const uint32_t* seq, uint32_t len, float lr) {
        if (len < 2) return;
        alignas(64) float x[512], h1[512], h2[512], o[2048], grad_h2[512], grad_h1[512];
        for (uint32_t t = 0; t + 1 < len; ++t) {
            uint32_t inp = seq[t], tgt = seq[t + 1];
            if (inp >= embed_ || tgt >= out_) continue;
            std::memcpy(x, emb_.get() + inp * embed_, embed_ * sizeof(float));
            MatVecBf16(w1_.get(), x, h1, hidden_, embed_);
            for (uint32_t i = 0; i < hidden_; ++i) h1[i] = GELU(h1[i] + b1_[i]);
            MatVecBf16(w2_.get(), h1, h2, hidden_, hidden_);
            for (uint32_t i = 0; i < hidden_; ++i) {
                h2[i] = GELU(h2[i] + b2_[i]);
                if (i < embed_) h2[i] += x[i];
            }
            MatVecBf16(w3_.get(), h2, o, out_, hidden_);
            for (uint32_t i = 0; i < out_; ++i) o[i] += b3_[i];
            SoftmaxSIMD(o, out_);
            o[tgt] -= 1.0f;
            std::memset(grad_h2, 0, hidden_ * sizeof(float));
            for (uint32_t i = 0; i < out_; ++i) {
                float g = o[i] * lr;
                uint16_t* row = w3_.get() + i * hidden_;
                for (uint32_t j = 0; j < hidden_; ++j) {
                    grad_h2[j] += g * Bf16ToF32(row[j]);
                    float old = Bf16ToF32(row[j]);
                    row[j] = F32ToBf16(old - g * h2[j]);
                }
                b3_[i] -= g;
            }
            for (uint32_t i = 0; i < hidden_; ++i) {
                if (h2[i] <= 0.0f && GELU(h2[i]) <= 0.0f) grad_h2[i] = 0.0f;
            }
            std::memset(grad_h1, 0, hidden_ * sizeof(float));
            for (uint32_t i = 0; i < hidden_; ++i) {
                float g = grad_h2[i] * lr;
                uint16_t* row = w2_.get() + i * hidden_;
                for (uint32_t j = 0; j < hidden_; ++j) {
                    grad_h1[j] += g * Bf16ToF32(row[j]);
                    float old = Bf16ToF32(row[j]);
                    row[j] = F32ToBf16(old - g * h1[j]);
                }
                b2_[i] -= g;
            }
            if (embed_ <= hidden_) for (uint32_t i = 0; i < embed_; ++i) grad_h1[i] += grad_h2[i];
            for (uint32_t i = 0; i < hidden_; ++i) {
                if (h1[i] <= 0.0f && GELU(h1[i]) <= 0.0f) grad_h1[i] = 0.0f;
            }
            for (uint32_t i = 0; i < hidden_; ++i) {
                float g = grad_h1[i] * lr;
                uint16_t* row = w1_.get() + i * embed_;
                for (uint32_t j = 0; j < embed_; ++j) {
                    float old = Bf16ToF32(row[j]);
                    row[j] = F32ToBf16(old - g * x[j]);
                }
                b1_[i] -= g;
            }
            float* erow = emb_.get() + inp * embed_;
            for (uint32_t i = 0; i < hidden_; ++i) {
                float g = grad_h1[i] * lr;
                const uint16_t* row = w1_.get() + i * embed_;
                for (uint32_t j = 0; j < embed_; ++j) erow[j] -= g * Bf16ToF32(row[j]);
            }
        }
    }
};

// ------------------------------------------------------------------
// Fast RNG
// ------------------------------------------------------------------
class FastRNG {
    uint64_t state;
public:
    FastRNG(uint64_t seed) : state(seed) {}
    uint32_t Next() { state ^= state >> 12; state ^= state << 25; state ^= state >> 27; return (uint32_t)(state * 0x2545F4914F6CDD1DULL); }
    float Uniform() { return Next() / (float)UINT32_MAX; }
};

// ------------------------------------------------------------------
// Thompson Bandit v2: 12 heads, momentum decay
// ------------------------------------------------------------------
class ThompsonBanditV2 {
    float alpha_[12];
    float beta_[12];
    float momentum_[12];
public:
    ThompsonBanditV2() {
        for (int i = 0; i < 12; ++i) { alpha_[i] = 1.0f; beta_[i] = 1.0f; momentum_[i] = 0.0f; }
    }
    void Update(int head, bool success) {
        if (head < 0 || head >= 12) return;
        if (success) alpha_[head] += 1.0f; else beta_[head] += 1.0f;
        if (alpha_[head] > 2000.0f) { alpha_[head] *= 0.5f; beta_[head] *= 0.5f; }
        momentum_[head] = momentum_[head] * 0.9f + (success ? 0.1f : -0.1f);
    }
    float SampleWeight(int head, FastRNG& rng) const {
        float mean = alpha_[head] / (alpha_[head] + beta_[head]);
        float var = (alpha_[head] * beta_[head]) /
                    ((alpha_[head] + beta_[head]) * (alpha_[head] + beta_[head]) * (alpha_[head] + beta_[head] + 1.0f));
        float std = std::sqrt(var);
        float z = (rng.Uniform() - 0.5f) * 2.0f;
        float w = mean + z * std * 2.0f + momentum_[head] * 0.5f;
        return std::max(0.01f, std::min(10.0f, w));
    }
    float MeanWeight(int head) const {
        return alpha_[head] / (alpha_[head] + beta_[head]);
    }
};

// ------------------------------------------------------------------
// Cascade Tree v2: 512 nodes, forest of alternatives
// ------------------------------------------------------------------
struct TNodeV2 {
    uint32_t token;
    uint32_t parent;
    uint32_t child;
    uint32_t sibling;
    uint32_t depth;
    float score;
    uint32_t head_id;
};

class CascadeTreeV2 {
    std::array<TNodeV2, 512> nodes_;
    uint32_t count_ = 0;
public:
    void Clear() { count_ = 0; }
    uint32_t Root(uint32_t tok, float sc, int head) {
        nodes_[0] = {tok, kNF, kNF, kNF, 0, sc, (uint32_t)head}; count_ = 1; return 0;
    }
    uint32_t Add(uint32_t p, uint32_t tok, float sc, int head) {
        if (count_ >= 512) return kNF;
        uint32_t id = count_++;
        nodes_[id] = {tok, p, kNF, kNF, nodes_[p].depth + 1, sc, (uint32_t)head};
        nodes_[id].sibling = nodes_[p].child;
        nodes_[p].child = id;
        return id;
    }
    uint32_t BestPath(uint32_t* out, uint32_t max_len) const {
        if (!count_) return 0;
        uint32_t n = 0, cur = 0;
        while (n < max_len && cur != kNF) {
            out[n++] = nodes_[cur].token;
            uint32_t bc = kNF; float bs = -1e30f;
            for (uint32_t c = nodes_[cur].child; c != kNF; c = nodes_[c].sibling)
                if (nodes_[c].score > bs) { bs = nodes_[c].score; bc = c; }
            cur = bc;
        }
        return n;
    }
    uint32_t Recover(uint32_t diverge_depth, uint32_t target_tok,
                     uint32_t* out, uint32_t max_len, int* out_head) const {
        for (uint32_t i = 0; i < count_; ++i) {
            if (nodes_[i].depth == diverge_depth && nodes_[i].token == target_tok) {
                uint32_t n = 0, cur = i;
                while (n < max_len && cur != kNF) {
                    out[n] = nodes_[cur].token;
                    if (out_head) out_head[n] = (int)nodes_[cur].head_id;
                    ++n;
                    uint32_t bc = kNF; float bs = -1e30f;
                    for (uint32_t c = nodes_[cur].child; c != kNF; c = nodes_[c].sibling)
                        if (nodes_[c].score > bs) { bs = nodes_[c].score; bc = c; }
                    cur = bc;
                }
                return n;
            }
        }
        return 0;
    }
    const TNodeV2& Node(uint32_t i) const { return nodes_[i]; }
};

// ------------------------------------------------------------------
// Copy-Detect v3: 512-token window
// ------------------------------------------------------------------
class CopyHeadV3 {
    static constexpr uint32_t kWin = 512;
    uint32_t buf_[kWin];
    uint32_t pos_ = 0, fill_ = 0;
public:
    void Push(uint32_t tok) {
        buf_[pos_ & (kWin - 1)] = tok;
        ++pos_; if (fill_ < kWin) ++fill_;
    }
    uint32_t Predict(uint32_t* out_len) const {
        if (fill_ < 64) { *out_len = 0; return kNF; }
        for (uint32_t L = std::min(fill_ / 2, 128u); L >= 8; --L) {
            uint32_t i1 = (pos_ - L) & (kWin - 1);
            uint32_t i2 = (pos_ - 2 * L) & (kWin - 1);
            bool same = true;
            for (uint32_t k = 0; k < L; ++k) {
                if (buf_[(i1 + k) & (kWin - 1)] != buf_[(i2 + k) & (kWin - 1)]) { same = false; break; }
            }
            if (same) {
                uint32_t nxt = buf_[(pos_ - L) & (kWin - 1)];
                *out_len = L;
                return nxt;
            }
        }
        *out_len = 0;
        return kNF;
    }
};

// ------------------------------------------------------------------
// Syntax Oracle v2: multi-language state machine
// ------------------------------------------------------------------
class SyntaxOracleV2 {
    int brace_depth_ = 0, paren_depth_ = 0, bracket_depth_ = 0, angle_depth_ = 0;
    int indent_level_ = 0;
    uint32_t pending_close[16];
    int pending_n_ = 0;
    bool in_string_ = false, in_comment_ = false;
public:
    void Observe(uint32_t tok) {
        if (in_string_) {
            if (tok == 34 || tok == 39) in_string_ = false;
            return;
        }
        if (in_comment_) {
            if (tok == 10 || tok == 13) in_comment_ = false;
            return;
        }
        if (tok == 47 || tok == 35) { in_comment_ = true; return; }
        if (tok == 34 || tok == 39) { in_string_ = true; return; }
        if (tok == 299 || tok == 321 || tok == 512 || tok == 1024 || tok == 2000) brace_depth_++;
        if (tok == 300 || tok == 322 || tok == 513 || tok == 1025 || tok == 2001) brace_depth_--;
        if (tok == 301 || tok == 323 || tok == 600) paren_depth_++;
        if (tok == 302 || tok == 324 || tok == 601) paren_depth_--;
        if (tok == 303 || tok == 325) bracket_depth_++;
        if (tok == 304 || tok == 326) bracket_depth_--;
        // Note: angle brackets intentionally excluded here — ambiguity with >> in C++ templates
        (void)angle_depth_;
    }
    uint32_t Predict(uint32_t* confidence, uint32_t* indent_delta) const {
        *indent_delta = 0;
        if (pending_n_ > 0) { *confidence = 255; return pending_close[pending_n_ - 1]; }
        if (brace_depth_ > 0) {
            *confidence = 220;
            *indent_delta = (brace_depth_ > indent_level_) ? 1 : 0;
            return 300;
        }
        if (paren_depth_ > 0) { *confidence = 200; return 302; }
        if (bracket_depth_ > 0) { *confidence = 200; return 304; }
        *confidence = 0;
        return kNF;
    }
};

// ------------------------------------------------------------------
// Variable Predictor v2
// ------------------------------------------------------------------
class VarPredictorV2 {
    static constexpr uint32_t kScopeMax = 128;
    struct VarInfo {
        uint32_t tok;
        uint32_t type_proxy;
        uint32_t decl_depth;
    };
    struct Scope {
        VarInfo vars[32];
        uint32_t count;
    };
    Scope scopes_[kScopeMax];
    uint32_t scope_sp_ = 0;
public:
    void EnterScope() { if (scope_sp_ < kScopeMax) { scopes_[scope_sp_].count = 0; ++scope_sp_; } }
    void ExitScope() { if (scope_sp_ > 0) --scope_sp_; }
    void Declare(uint32_t var_tok, uint32_t type_tok) {
        if (scope_sp_ == 0) EnterScope();
        Scope& s = scopes_[scope_sp_ - 1];
        if (s.count < 32) { s.vars[s.count++] = {var_tok, type_tok, scope_sp_}; }
    }
    uint32_t Predict(const uint32_t* prefix, uint32_t prefix_len, uint32_t* confidence, uint32_t* type_out) const {
        if (!prefix_len || scope_sp_ == 0) { *confidence = 0; return kNF; }
        for (int si = (int)scope_sp_ - 1; si >= 0; --si) {
            const Scope& s = scopes_[si];
            for (int i = (int)s.count - 1; i >= 0; --i) {
                if (s.vars[i].tok == prefix[prefix_len - 1]) {
                    *confidence = 160 + (scope_sp_ - si) * 10;
                    *type_out = s.vars[i].type_proxy;
                    return s.vars[i].tok;
                }
            }
        }
        *confidence = 0;
        return kNF;
    }
};

// ------------------------------------------------------------------
// Entropy-driven width with momentum and temperature adaptation
// ------------------------------------------------------------------
class EntropyWidthV2 {
    float hist_entropy_[32] = {0};
    float hist_accept_[32] = {0};
    uint32_t pos_ = 0;
    float temperature_ = 0.8f;
public:
    uint32_t ComputeWidth(float current_entropy, float current_accept,
                          uint32_t min_w, uint32_t max_w, float ewma_acc,
                          float* out_temp) {
        hist_entropy_[pos_ & 31] = current_entropy;
        hist_accept_[pos_ & 31] = current_accept;
        ++pos_;
        float avg_ent = 0.0f, avg_acc = 0.0f;
        for (int i = 0; i < 32; ++i) { avg_ent += hist_entropy_[i]; avg_acc += hist_accept_[i]; }
        avg_ent /= 32.0f; avg_acc /= 32.0f;
        temperature_ = 0.5f + avg_ent * 0.5f;
        *out_temp = temperature_;
        float score = avg_acc * (1.0f - avg_ent * 0.5f);
        uint32_t w = min_w + (uint32_t)(score * (max_w - min_w) * 2.0f);
        return std::min(max_w, std::max(min_w, w));
    }
};

// ------------------------------------------------------------------
// Restless Draft Buffer: quadruple-buffered async generation
// ------------------------------------------------------------------
template<uint32_t N>
class RestlessBuffer {
    struct alignas(64) Slot {
        uint32_t tokens[N];
        uint32_t count;
        std::atomic<bool> ready{false};
        std::atomic<bool> consumed{true};
    };
    Slot slots_[4];
    std::atomic<uint32_t> write_idx_{0};
    std::atomic<uint32_t> read_idx_{0};
public:
    bool TryWrite(const uint32_t* tokens, uint32_t count) {
        uint32_t idx = write_idx_.load(std::memory_order_relaxed);
        for (int attempts = 0; attempts < 4; ++attempts) {
            if (slots_[idx].consumed.load(std::memory_order_acquire)) {
                slots_[idx].count = std::min(count, N);
                std::memcpy(slots_[idx].tokens, tokens, slots_[idx].count * sizeof(uint32_t));
                slots_[idx].consumed.store(false, std::memory_order_release);
                slots_[idx].ready.store(true, std::memory_order_release);
                write_idx_.store((idx + 1) & 3, std::memory_order_relaxed);
                return true;
            }
            idx = (idx + 1) & 3;
        }
        return false;
    }
    bool TryRead(uint32_t* out, uint32_t* count) {
        uint32_t idx = read_idx_.load(std::memory_order_relaxed);
        for (int attempts = 0; attempts < 4; ++attempts) {
            if (slots_[idx].ready.load(std::memory_order_acquire) &&
                !slots_[idx].consumed.load(std::memory_order_acquire)) {
                *count = slots_[idx].count;
                std::memcpy(out, slots_[idx].tokens, *count * sizeof(uint32_t));
                slots_[idx].ready.store(false, std::memory_order_release);
                slots_[idx].consumed.store(true, std::memory_order_release);
                read_idx_.store((idx + 1) & 3, std::memory_order_relaxed);
                return true;
            }
            idx = (idx + 1) & 3;
        }
        return false;
    }
};

} // anonymous namespace

// ------------------------------------------------------------------
// Main Implementation
// ------------------------------------------------------------------
class SingularitySpecDecoderV2::Impl {
public:
    SingularityV2Config cfg_;
    CuckooTable8 n2_, n3_, n4_, n5_;
    OctoGramCache octo_;
    SedecimGramCache sedecim_;
    MinHashCacheV2 minhash_;
    KVCacheV2 kvcache_;
    SyntaxOracleV2 syntax_;
    VarPredictorV2 var_;
    BFloat16MLPV2 mlp_;
    ThompsonBanditV2 bandit_;
    CascadeTreeV2 tree_;
    CopyHeadV3 copy_;
    EntropyWidthV2 ewidth_;
    FastRNG rng_;
    RestlessBuffer<512> restless_;

    struct {
        std::atomic<uint64_t> drafts{0};
        std::atomic<uint64_t> acc{0};
        std::atomic<uint64_t> rej{0};
        std::atomic<uint64_t> cascade{0};
        std::atomic<uint64_t> syntax_hits{0};
        std::atomic<uint64_t> var_hits{0};
        std::atomic<uint64_t> octo_hits{0};
        std::atomic<uint64_t> sedecim_hits{0};
        std::atomic<uint64_t> minhash_hits{0};
        std::atomic<uint64_t> kv_hits{0};
        std::atomic<uint64_t> restless_hits{0};
    } stats;

    alignas(64) std::atomic<float> ewma_{0.5f};
    alignas(64) std::atomic<uint32_t> width_{4};
    alignas(64) std::atomic<float> temperature_{0.8f};

    // Restless thread has its own tree/rng to avoid data races
    CascadeTreeV2 restless_tree_;
    FastRNG restless_rng_;

    std::thread restless_thread_;
    std::atomic<bool> restless_stop_{false};
    std::mutex restless_mtx_;
    std::condition_variable restless_cv_;
    uint32_t restless_history_[256];
    uint32_t restless_hist_len_ = 0;

    explicit Impl(const SingularityV2Config& cfg)
        : cfg_(cfg),
          n2_(std::min(cfg.octo_gram_bits, 18u)), n3_(std::min(cfg.octo_gram_bits, 18u)),
          n4_(std::min(cfg.octo_gram_bits, 18u)), n5_(std::min(cfg.octo_gram_bits, 18u)),
          octo_(std::min(cfg.octo_gram_bits, 18u)),
          sedecim_(std::min(cfg.sedecim_gram_bits, 18u)),
          minhash_(Log2Floor(std::max(1u, cfg.minhash_slots))),
          kvcache_(cfg.kv_cache_rows),
          mlp_(cfg.vocab_size, cfg.mlp_embed, cfg.mlp_hidden, cfg.slim_vocab),
          rng_(0xDEADBEEFCAFEBABEULL),
          restless_rng_(0xCAFEBABEDEAD1234ULL) {
        width_.store(4, std::memory_order_relaxed);
        std::memset(restless_history_, 0, sizeof(restless_history_));
        restless_thread_ = std::thread([this]() {
            uint32_t draft_buf[512];
            while (!restless_stop_.load(std::memory_order_relaxed)) {
                std::unique_lock<std::mutex> lk(restless_mtx_);
                restless_cv_.wait_for(lk, std::chrono::milliseconds(1));
                uint32_t hlen = restless_hist_len_;
                if (hlen > 0) {
                    uint32_t n = DraftInternalPrivate(restless_history_, hlen, draft_buf, 512,
                                                       restless_tree_, restless_rng_);
                    if (n > 0) restless_.TryWrite(draft_buf, n);
                }
            }
        });
    }

    ~Impl() {
        restless_stop_.store(true, std::memory_order_relaxed);
        restless_cv_.notify_all();
        if (restless_thread_.joinable()) restless_thread_.join();
    }

    uint32_t Draft(const uint32_t* history, uint32_t hist_len,
                   uint32_t* out_draft, uint32_t max_draft) {
        uint32_t restless_count = 0;
        if (restless_.TryRead(out_draft, &restless_count)) {
            stats.restless_hits.fetch_add(restless_count, std::memory_order_relaxed);
            uint32_t cpy = std::min(hist_len, 256u);
            std::memcpy(restless_history_, history + hist_len - cpy, cpy * sizeof(uint32_t));
            restless_hist_len_ = cpy;
            restless_cv_.notify_one();
            return std::min(restless_count, max_draft);
        }
        return DraftInternal(history, hist_len, out_draft, max_draft, false);
    }

    // Called from restless thread with its own tree/rng — no shared mutable state
    uint32_t DraftInternalPrivate(const uint32_t* history, uint32_t hist_len,
                                   uint32_t* out_draft, uint32_t max_draft,
                                   CascadeTreeV2& local_tree, FastRNG& local_rng) {
        return DraftCore(history, hist_len, out_draft, max_draft, local_tree, local_rng, true);
    }

    uint32_t DraftInternal(const uint32_t* history, uint32_t hist_len,
                           uint32_t* out_draft, uint32_t max_draft, bool /*unused*/) {
        return DraftCore(history, hist_len, out_draft, max_draft, tree_, rng_, false);
    }

    uint32_t DraftCore(const uint32_t* history, uint32_t hist_len,
                       uint32_t* out_draft, uint32_t max_draft,
                       CascadeTreeV2& tree, FastRNG& rng, bool is_restless) {
        uint32_t base_w = std::min(max_draft, width_.load(std::memory_order_relaxed));
        if (!base_w || !hist_len) return 0;

        tree.Clear();
        uint32_t root_tok = history[hist_len - 1];
        tree.Root(root_tok, 1.0f, -1);

        alignas(64) float mlp_logits[2048];
        uint32_t mlp_pred = mlp_.Predict(history, hist_len, mlp_logits);
        float mlp_conf = 0.0f;
        if (mlp_pred < cfg_.slim_vocab) {
            SoftmaxSIMD(mlp_logits, cfg_.slim_vocab);
            mlp_conf = mlp_logits[mlp_pred];
        }

        uint32_t copy_len = 0;
        uint32_t copy_pred = copy_.Predict(&copy_len);

        uint32_t syn_conf = 0, syn_indent = 0;
        uint32_t syn_pred = syntax_.Predict(&syn_conf, &syn_indent);

        uint32_t mh_tok[16]; float mh_prob[16];
        uint32_t mh_start = (hist_len > 128) ? hist_len - 128 : 0;
        uint32_t mh_n = minhash_.Retrieve(history + mh_start,
                                          hist_len - mh_start, mh_tok, mh_prob, 16);

        uint32_t var_conf = 0, var_type = 0;
        uint32_t var_start = (hist_len > 16) ? hist_len - 16 : 0;
        uint32_t var_pred = var_.Predict(history + var_start,
                                         hist_len - var_start, &var_conf, &var_type);

        alignas(64) float kv_key[128];
        uint32_t kv_copy = std::min(128u, cfg_.slim_vocab);
        std::memcpy(kv_key, mlp_logits, kv_copy * sizeof(float));
        if (kv_copy < 128) std::memset(kv_key + kv_copy, 0, (128 - kv_copy) * sizeof(float));
        uint32_t kv_pred = kvcache_.Retrieve(kv_key);

        uint32_t gen = 0;
        uint32_t frontier[128];
        uint32_t frontier_n = 1;
        frontier[0] = 0;

        while (gen < base_w && frontier_n > 0) {
            uint32_t next_frontier[128];
            uint32_t next_n = 0;

            for (uint32_t fi = 0; fi < frontier_n && gen < base_w; ++fi) {
                uint32_t node_id = frontier[fi];
                uint32_t depth = tree.Node(node_id).depth;

                uint32_t ctx[16] = {0};
                for (int i = 0; i < 16; ++i) {
                    int idx = (int)hist_len - 16 + i + (int)depth;
                    if (idx >= 0 && (uint32_t)idx < hist_len) ctx[i] = history[idx];
                    else if (depth > 0 && (int)gen + i - (int)depth >= 0 && (uint32_t)(gen + i - depth) < gen)
                        ctx[i] = out_draft[gen + i - depth];
                }

                uint32_t preds[12]; float pconf[12];
                for (int h = 0; h < 12; ++h) { preds[h] = kNF; pconf[h] = 0.0f; }

                preds[0] = n5_.Lookup(Key5(ctx[11],ctx[12],ctx[13],ctx[14],ctx[15]));
                if (preds[0] != kNF) pconf[0] = 1.0f;
                preds[1] = n4_.Lookup(Key4(ctx[12],ctx[13],ctx[14],ctx[15]));
                if (preds[1] != kNF) pconf[1] = 0.9f;
                preds[2] = n3_.Lookup(Key3(ctx[13],ctx[14],ctx[15]));
                if (preds[2] != kNF) pconf[2] = 0.75f;
                preds[3] = n2_.Lookup(Key2(ctx[14],ctx[15]));
                if (preds[3] != kNF) pconf[3] = 0.6f;

                if (hist_len + depth >= 8) {
                    uint32_t octo_ctx[8];
                    for (int i = 0; i < 8; ++i) octo_ctx[i] = ctx[8 + i];
                    preds[4] = octo_.Lookup(octo_ctx);
                    if (preds[4] != kNF) { pconf[4] = 1.3f; stats.octo_hits.fetch_add(1, std::memory_order_relaxed); }
                }

                if (hist_len + depth >= 16) {
                    preds[5] = sedecim_.Lookup(ctx);
                    if (preds[5] != kNF) { pconf[5] = 1.5f; stats.sedecim_hits.fetch_add(1, std::memory_order_relaxed); }
                }

                if (mlp_pred < cfg_.slim_vocab && mlp_conf > 0.1f) {
                    preds[6] = mlp_pred; pconf[6] = mlp_conf;
                }

                if (copy_pred != kNF && gen > 0 && copy_pred != out_draft[gen-1]) {
                    preds[7] = copy_pred; pconf[7] = 0.7f;
                }

                if (syn_pred != kNF && syn_conf > 80) {
                    preds[8] = syn_pred; pconf[8] = syn_conf / 255.0f;
                    stats.syntax_hits.fetch_add(1, std::memory_order_relaxed);
                }

                if (var_pred != kNF && var_conf > 60) {
                    preds[9] = var_pred; pconf[9] = var_conf / 255.0f;
                    stats.var_hits.fetch_add(1, std::memory_order_relaxed);
                }

                if (kv_pred != kNF) {
                    preds[10] = kv_pred; pconf[10] = 0.8f;
                    stats.kv_hits.fetch_add(1, std::memory_order_relaxed);
                }

                if (mh_n > 0 && gen < mh_n) {
                    preds[11] = mh_tok[gen]; pconf[11] = mh_prob[gen];
                    stats.minhash_hits.fetch_add(1, std::memory_order_relaxed);
                }

                struct Cand { uint32_t tok; float score; int head; };
                Cand cands[24];
                uint32_t nc = 0;

                auto add = [&](uint32_t t, float s, int h) {
                    if (t == kNF || t >= cfg_.vocab_size) return;
                    for (uint32_t i = 0; i < nc; ++i)
                        if (cands[i].tok == t) { cands[i].score += s; return; }
                    if (nc < 24) cands[nc++] = {t, s, h};
                };

                for (int h = 0; h < 12; ++h) {
                    if (preds[h] != kNF) {
                        float w = bandit_.SampleWeight(h, rng);
                        add(preds[h], pconf[h] * w, h);
                    }
                }

                if (!nc) break;

                for (uint32_t i = 0; i < nc; ++i)
                    for (uint32_t j = i + 1; j < nc; ++j)
                        if (cands[j].score > cands[i].score) std::swap(cands[i], cands[j]);

                uint32_t chosen = cands[0].tok;
                out_draft[gen++] = chosen;
                uint32_t new_node = tree.Add(node_id, chosen, cands[0].score, cands[0].head);
                if (new_node != kNF && next_n < 128) next_frontier[next_n++] = new_node;

                for (uint32_t ci = 1; ci < nc && ci < 6; ++ci) {
                    uint32_t alt = tree.Add(node_id, cands[ci].tok, cands[ci].score, cands[ci].head);
                    if (alt != kNF && next_n < 128) next_frontier[next_n++] = alt;
                }
            }
            frontier_n = next_n;
        }

        if (!is_restless) stats.drafts.fetch_add(1, std::memory_order_relaxed);
        return gen;
    }

    uint32_t ValidateArgmax(const uint32_t* draft, uint32_t n,
                            const uint32_t* target_argmax) {
        uint32_t accepted = 0;
        int recover_head[512];
        std::memset(recover_head, -1, sizeof(recover_head));
        for (uint32_t i = 0; i < n; ++i) {
            if (draft[i] == target_argmax[i]) {
                ++accepted;
            } else {
                uint32_t recover[512];
                int rhead[512];
                uint32_t rlen = tree_.Recover(i, target_argmax[i], recover, 512, rhead);
                if (rlen > 0) {
                    uint32_t cl = std::min(rlen, n - i);
                    std::memcpy(const_cast<uint32_t*>(draft) + i, recover, cl * sizeof(uint32_t));
                    std::memcpy(recover_head + i, rhead, cl * sizeof(int));
                    ++accepted;
                    stats.cascade.fetch_add(1, std::memory_order_relaxed);
                    if (rhead[0] >= 0) bandit_.Update(rhead[0], true);
                    continue;
                }
                if (i > 0 && recover_head[i-1] >= 0) bandit_.Update(recover_head[i-1], false);
                break;
            }
        }
        UpdateStats(accepted, n);
        return accepted;
    }

    uint32_t ValidateProbabilistic(const uint32_t* draft, uint32_t n,
                                   const float* target_logits, uint32_t vocab_stride) {
        uint32_t accepted = 0;
        float temp = temperature_.load(std::memory_order_relaxed);
        for (uint32_t i = 0; i < n; ++i) {
            const float* logits = target_logits + i * vocab_stride;
            uint32_t v = std::min(cfg_.vocab_size, 32000u);
            alignas(64) float probs[32000];
            std::memcpy(probs, logits, v * sizeof(float));
            SoftmaxSIMD(probs, v);
            float p_target = probs[draft[i]];
            float maxp = probs[ArgmaxSIMD(probs, v)];
            if (p_target >= maxp * temp * 0.9f) {
                ++accepted;
            } else {
                uint32_t recover[512];
                int rhead[512];
                uint32_t argmax_tok = ArgmaxSIMD(probs, v);
                uint32_t rlen = tree_.Recover(i, argmax_tok, recover, 512, rhead);
                if (rlen > 0) {
                    uint32_t cl = std::min(rlen, n - i);
                    std::memcpy(const_cast<uint32_t*>(draft) + i, recover, cl * sizeof(uint32_t));
                    ++accepted;
                    stats.cascade.fetch_add(1, std::memory_order_relaxed);
                    if (rhead[0] >= 0) bandit_.Update(rhead[0], true);
                    continue;
                }
                break;
            }
        }
        UpdateStats(accepted, n);
        return accepted;
    }

    void FeedAccepted(const uint32_t* seq, uint32_t len) {
        if (len < 2) return;
        for (uint32_t i = 0; i + 1 < len; ++i) {
            if (i + 16 < len) sedecim_.Insert(seq + i, seq[i + 16]);
            if (i + 8 < len) octo_.Insert(seq + i, seq[i + 8]);
            if (i + 5 < len) n5_.Insert(Key5(seq[i],seq[i+1],seq[i+2],seq[i+3],seq[i+4]), seq[i+5]);
            if (i + 4 < len) n4_.Insert(Key4(seq[i],seq[i+1],seq[i+2],seq[i+3]), seq[i+4]);
            if (i + 3 < len) n3_.Insert(Key3(seq[i],seq[i+1],seq[i+2]), seq[i+3]);
            if (i + 2 < len) n2_.Insert(Key2(seq[i],seq[i+1]), seq[i+2]);
        }
        for (uint32_t i = 0; i < len; ++i) copy_.Push(seq[i]);
        for (uint32_t i = 0; i < len; ++i) {
            syntax_.Observe(seq[i]);
            if (seq[i] == 299 || seq[i] == 321 || seq[i] == 2000) var_.EnterScope();
            if (seq[i] == 300 || seq[i] == 322 || seq[i] == 2001) var_.ExitScope();
            if (i + 1 < len && (seq[i] == 1000 || seq[i] == 1001)) var_.Declare(seq[i+1], seq[i]);
        }
        if (len >= 16) {
            uint32_t topk[16]; float prob[16];
            for (int i = 0; i < 16; ++i) { topk[i] = seq[len - 1 - i]; prob[i] = 1.0f / (i + 1); }
            uint32_t mh_start = (len > 128) ? len - 128 : 0;
            minhash_.Store(seq + mh_start, len - mh_start, topk, prob);
        }
        alignas(64) float kv_key[128];
        uint32_t kv_len = std::min(len, 128u);
        for (uint32_t i = 0; i < kv_len; ++i) kv_key[i] = seq[len - 1 - i] * 0.001f;
        if (kv_len < 128) std::memset(kv_key + kv_len, 0, (128 - kv_len) * sizeof(float));
        kvcache_.Store(kv_key, seq[len - 1]);
        mlp_.Train(seq, len, 0.005f);
        stats.acc.fetch_add(len, std::memory_order_relaxed);
    }

    SingularityV2Stats GetStats() const {
        SingularityV2Stats s{};
        s.drafts_total = stats.drafts.load();
        s.tokens_accepted = stats.acc.load();
        s.tokens_rejected = stats.rej.load();
        s.cascade_recoveries = stats.cascade.load();
        s.syntax_oracle_hits = stats.syntax_hits.load();
        s.var_predict_hits = stats.var_hits.load();
        s.octo_gram_hits = stats.octo_hits.load();
        s.sedecim_gram_hits = stats.sedecim_hits.load();
        s.minhash_hits = stats.minhash_hits.load();
        s.kv_cache_hits = stats.kv_hits.load();
        s.restless_prefetch_hits = stats.restless_hits.load();
        s.acceptance_ewma = ewma_.load();
        s.current_width = width_.load();
        for (int i = 0; i < 12; ++i) s.head_bandit_weights[i] = bandit_.MeanWeight(i);
        return s;
    }

    uint32_t GetWidth() const { return width_.load(); }

private:
    void UpdateStats(uint32_t accepted, uint32_t total) {
        if (!total) return;
        uint32_t rejected = total - accepted;
        stats.rej.fetch_add(rejected, std::memory_order_relaxed);

        float a = accepted / (float)total;
        float old_ewma = ewma_.load(std::memory_order_relaxed);
        float new_ewma = old_ewma * 0.82f + a * 0.18f;
        ewma_.store(new_ewma, std::memory_order_relaxed);

        float temp;
        float ent = (a > 0.92f) ? 0.05f : (a > 0.7f ? 0.15f : (a > 0.4f ? 0.35f : 0.7f));
        uint32_t w = ewidth_.ComputeWidth(ent, a, 2, cfg_.max_draft_width, new_ewma, &temp);
        width_.store(w, std::memory_order_relaxed);
        temperature_.store(temp, std::memory_order_relaxed);
    }
};

// ------------------------------------------------------------------
// Public trampolines
// ------------------------------------------------------------------
SingularitySpecDecoderV2::SingularitySpecDecoderV2(const SingularityV2Config& cfg)
    : impl_(std::make_unique<Impl>(cfg)) {}
SingularitySpecDecoderV2::~SingularitySpecDecoderV2() = default;

uint32_t SingularitySpecDecoderV2::Draft(const uint32_t* h, uint32_t hl,
                                         uint32_t* out, uint32_t max_draft) {
    return impl_->Draft(h, hl, out, max_draft);
}
uint32_t SingularitySpecDecoderV2::ValidateArgmax(const uint32_t* d, uint32_t n,
                                                  const uint32_t* t) {
    return impl_->ValidateArgmax(d, n, t);
}
uint32_t SingularitySpecDecoderV2::ValidateProbabilistic(const uint32_t* d, uint32_t n,
                                                         const float* l, uint32_t s) {
    return impl_->ValidateProbabilistic(d, n, l, s);
}
void SingularitySpecDecoderV2::FeedAccepted(const uint32_t* seq, uint32_t len) {
    impl_->FeedAccepted(seq, len);
}
SingularityV2Stats SingularitySpecDecoderV2::GetStats() const { return impl_->GetStats(); }
uint32_t SingularitySpecDecoderV2::GetDraftWidth() const { return impl_->GetWidth(); }

} // namespace rxd::ai
