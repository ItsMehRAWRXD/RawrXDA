#include "cpu_inference_engine.h"
#include "engine/bpe_tokenizer.h"
#include "engine/inference_kernels.h"
#include "engine/transformer.h"
#include "loader/gguf_tensor_loader.h"
#include "loader/memory_budget.h"
#include "loader/tensor_filter.h"
#include "rawrxd_tokenizer.h"
#include "streaming_gguf_loader.h"
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <limits>
#include <map>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <vector>


#if defined(_MSC_VER)
#include <intrin.h>
#endif


namespace CPUInference
{

// Process-wide tokenizer backed by GGUF vocab (read-only once loaded).
static RawrXDTokenizer g_tokenizer;
static std::atomic<bool> g_tokenizerReady{false};

// Tensor type registry for loaded weights (by canonical tensor name).
// Kept in this TU to avoid touching multiple headers while iterating.
static std::unordered_map<std::string, RawrXD::GGMLType> g_weightTypes;

static bool g_hasAvx2 = false;
static std::once_flag g_avx2Once;

static inline bool runtimeHasAvx2_()
{
#if defined(__AVX2__)
#if defined(_MSC_VER)
    int regs[4] = {0, 0, 0, 0};
    __cpuidex(regs, 1, 0);
    const bool osxsave = (regs[2] & (1 << 27)) != 0;
    const bool avx = (regs[2] & (1 << 28)) != 0;
    if (!(osxsave && avx))
        return false;

    const unsigned long long xcr0 = _xgetbv(0);
    const bool xmm_ymm_ok = ((xcr0 & 0x6u) == 0x6u);
    if (!xmm_ymm_ok)
        return false;

    __cpuidex(regs, 7, 0);
    const bool avx2 = (regs[1] & (1 << 5)) != 0;
    return avx2;
#else
    // If we were built with AVX2 and we're not on MSVC, assume toolchain/runtime
    // environment guarantees availability. (Clang/GCC users can add a fuller gate.)
    return true;
#endif
#else
    return false;
#endif
}

static inline bool hasAvx2_()
{
    std::call_once(g_avx2Once, []() { g_hasAvx2 = runtimeHasAvx2_(); });
    return g_hasAvx2;
}

// ============================================================================
// Q8_0 DEQUANTIZATION + MATVEC (GGML layout)
//
// GGML Q8_0 layout (per 32 elements):
//   [0..1]  ggml_rxd_half d  (f16 scale)
//   [2..33] int8 qs[32]
// Total: 34 bytes per 32 weights
// ============================================================================
struct Q8_0Block
{
    uint16_t d;     // f16 scale (ggml_rxd_half)
    int8_t qs[32];  // 32 int8 values
};
static_assert(sizeof(Q8_0Block) == 34, "Q8_0 block must be 34 bytes");

static inline float f16_to_f32_q8_(uint16_t h)
{
    // Minimal FP16->FP32 (matches style used elsewhere in repo).
    const int exp = (h >> 10) & 0x1F;
    const int frac = h & 0x3FF;
    float v = (exp == 0) ? (frac / 1024.0f / 16384.0f) : std::ldexp(1.0f + frac / 1024.0f, exp - 15);
    return (h & 0x8000) ? -v : v;
}

static inline float q8_0_dot_scalar_(const Q8_0Block* w, const float* x, int nBlocks)
{
    float sum = 0.0f;
    for (int b = 0; b < nBlocks; ++b)
    {
        const float d = f16_to_f32_q8_(w[b].d);
        const int8_t* qs = w[b].qs;
        const float* xb = x + (size_t)b * 32u;
        // Fully unrolled: keeps this path debuggable & deterministic.
        sum += (float)qs[0] * xb[0] * d;
        sum += (float)qs[1] * xb[1] * d;
        sum += (float)qs[2] * xb[2] * d;
        sum += (float)qs[3] * xb[3] * d;
        sum += (float)qs[4] * xb[4] * d;
        sum += (float)qs[5] * xb[5] * d;
        sum += (float)qs[6] * xb[6] * d;
        sum += (float)qs[7] * xb[7] * d;
        sum += (float)qs[8] * xb[8] * d;
        sum += (float)qs[9] * xb[9] * d;
        sum += (float)qs[10] * xb[10] * d;
        sum += (float)qs[11] * xb[11] * d;
        sum += (float)qs[12] * xb[12] * d;
        sum += (float)qs[13] * xb[13] * d;
        sum += (float)qs[14] * xb[14] * d;
        sum += (float)qs[15] * xb[15] * d;
        sum += (float)qs[16] * xb[16] * d;
        sum += (float)qs[17] * xb[17] * d;
        sum += (float)qs[18] * xb[18] * d;
        sum += (float)qs[19] * xb[19] * d;
        sum += (float)qs[20] * xb[20] * d;
        sum += (float)qs[21] * xb[21] * d;
        sum += (float)qs[22] * xb[22] * d;
        sum += (float)qs[23] * xb[23] * d;
        sum += (float)qs[24] * xb[24] * d;
        sum += (float)qs[25] * xb[25] * d;
        sum += (float)qs[26] * xb[26] * d;
        sum += (float)qs[27] * xb[27] * d;
        sum += (float)qs[28] * xb[28] * d;
        sum += (float)qs[29] * xb[29] * d;
        sum += (float)qs[30] * xb[30] * d;
        sum += (float)qs[31] * xb[31] * d;
    }
    return sum;
}

static inline float q8_0_dot_scalar_masked_(const Q8_0Block* w, const float* x, int cols)
{
    if (cols <= 0)
        return 0.0f;

    const int fullBlocks = cols / 32;
    const int rem = cols - fullBlocks * 32;

    float sum = 0.0f;
    if (fullBlocks > 0)
    {
        sum += q8_0_dot_scalar_(w, x, fullBlocks);
    }
    if (rem > 0)
    {
        const int b = fullBlocks;
        const float d = f16_to_f32_q8_(w[b].d);
        const int8_t* qs = w[b].qs;
        const float* xb = x + (size_t)b * 32u;
        for (int i = 0; i < rem; ++i)
        {
            sum += (float)qs[i] * xb[i] * d;
        }
    }
    return sum;
}

// AVX2 Q8_0 dot for full 32-wide blocks. Tail is handled by masked scalar.
static inline float q8_0_dot_avx2_full_(const Q8_0Block* w, const float* x, int nFullBlocks)
{
#if defined(__AVX2__)
    __m256 acc = _mm256_setzero_ps();

    for (int b = 0; b < nFullBlocks; ++b)
    {
        const float d = f16_to_f32_q8_(w[b].d);
        const __m256 dvec = _mm256_set1_ps(d);

        const __m128i q0_16 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(w[b].qs + 0));   // 0..15
        const __m128i q1_16 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(w[b].qs + 16));  // 16..31

        // Expand 8 int8 -> 8 int32 -> 8 float, four chunks of 8.
        const __m256 f0 = _mm256_cvtepi32_ps(_mm256_cvtepi8_epi32(q0_16));                     // bytes 0..7
        const __m256 f1 = _mm256_cvtepi32_ps(_mm256_cvtepi8_epi32(_mm_srli_si128(q0_16, 8)));  // bytes 8..15
        const __m256 f2 = _mm256_cvtepi32_ps(_mm256_cvtepi8_epi32(q1_16));                     // bytes 16..23
        const __m256 f3 = _mm256_cvtepi32_ps(_mm256_cvtepi8_epi32(_mm_srli_si128(q1_16, 8)));  // bytes 24..31

        const float* xb = x + (size_t)b * 32u;
        const __m256 x0 = _mm256_loadu_ps(xb + 0);
        const __m256 x1 = _mm256_loadu_ps(xb + 8);
        const __m256 x2 = _mm256_loadu_ps(xb + 16);
        const __m256 x3 = _mm256_loadu_ps(xb + 24);

        // acc += (f * d) * x
        acc = _mm256_fmadd_ps(_mm256_mul_ps(f0, dvec), x0, acc);
        acc = _mm256_fmadd_ps(_mm256_mul_ps(f1, dvec), x1, acc);
        acc = _mm256_fmadd_ps(_mm256_mul_ps(f2, dvec), x2, acc);
        acc = _mm256_fmadd_ps(_mm256_mul_ps(f3, dvec), x3, acc);
    }

    // Horizontal sum acc.
    __m128 lo = _mm256_castps256_ps128(acc);
    __m128 hi = _mm256_extractf128_ps(acc, 1);
    lo = _mm_add_ps(lo, hi);
    lo = _mm_add_ps(lo, _mm_movehl_ps(lo, lo));
    lo = _mm_add_ss(lo, _mm_movehdup_ps(lo));
    return _mm_cvtss_f32(lo);
#else
    (void)w;
    (void)x;
    (void)nFullBlocks;
    return 0.0f;
#endif
}

static inline float q8_0_dot_row_(const Q8_0Block* rowBlocks, const float* x, int cols)
{
    if (cols <= 0)
        return 0.0f;

    const int fullBlocks = cols / 32;
    const int rem = cols - fullBlocks * 32;

    float sum = 0.0f;
    if (fullBlocks > 0)
    {
        if (hasAvx2_())
            sum += q8_0_dot_avx2_full_(rowBlocks, x, fullBlocks);
        else
            sum += q8_0_dot_scalar_(rowBlocks, x, fullBlocks);
    }
    if (rem > 0)
    {
        sum += q8_0_dot_scalar_masked_(rowBlocks + fullBlocks, x + (size_t)fullBlocks * 32u, rem);
    }
    return sum;
}

// Optional self-test (compile-time opt-in): validates AVX2-vs-scalar on a non-multiple-of-32 width.
#if defined(RAWRXD_Q8_0_SELFTEST)
#include <cassert>
#include <random>


static inline uint16_t f32_to_f16_bits_(float f)
{
    uint32_t x;
    std::memcpy(&x, &f, sizeof(x));

    const uint32_t sign = (x >> 31) & 1u;
    int32_t exp = (int32_t)((x >> 23) & 0xFFu) - 127;
    uint32_t mant = x & 0x7FFFFFu;

    // Handle NaN/Inf
    if (((x >> 23) & 0xFFu) == 0xFFu)
    {
        const uint16_t h_exp = 0x1Fu;
        const uint16_t h_mant = (mant != 0) ? 0x200u : 0u;  // quiet NaN if mant!=0
        return (uint16_t)((sign << 15) | (h_exp << 10) | h_mant);
    }

    // Clamp to half range
    if (exp > 15)
    {
        return (uint16_t)((sign << 15) | (0x1Fu << 10));  // Inf
    }
    if (exp < -14)
    {
        // Subnormal or zero
        if (exp < -24)
            return (uint16_t)(sign << 15);
        mant |= 0x800000u;
        const int shift = (-14 - exp);
        uint32_t mant_h = mant >> (shift + 13);
        // round
        const uint32_t round_bit = (mant >> (shift + 12)) & 1u;
        mant_h += round_bit;
        return (uint16_t)((sign << 15) | (uint16_t)mant_h);
    }

    // Normal
    uint16_t h_exp = (uint16_t)(exp + 15);
    uint32_t mant_h = mant >> 13;
    // round to nearest
    mant_h += (mant >> 12) & 1u;
    if (mant_h == 0x400u)
    {
        mant_h = 0;
        ++h_exp;
        if (h_exp >= 0x1Fu)
            return (uint16_t)((sign << 15) | (0x1Fu << 10));
    }
    return (uint16_t)((sign << 15) | (h_exp << 10) | (uint16_t)(mant_h & 0x3FFu));
}

static inline void* aligned_alloc_portable_(size_t alignment, size_t size)
{
#if defined(_MSC_VER)
    return _aligned_malloc(size, alignment);
#else
    return std::aligned_alloc(alignment, ((size + alignment - 1) / alignment) * alignment);
#endif
}

static inline void aligned_free_portable_(void* p)
{
#if defined(_MSC_VER)
    _aligned_free(p);
#else
    std::free(p);
#endif
}

static void rawrxd_q8_0_selftest_()
{
    const int cols = 123;  // forces tail
    const int nBlocks = (cols + 31) / 32;

    auto* w = reinterpret_cast<Q8_0Block*>(aligned_alloc_portable_(32, (size_t)nBlocks * sizeof(Q8_0Block)));
    auto* x = reinterpret_cast<float*>(aligned_alloc_portable_(32, (size_t)cols * sizeof(float)));
    if (!w || !x)
    {
        std::fprintf(stderr, "[Q8_0 selftest] alloc failed\n");
        aligned_free_portable_(w);
        aligned_free_portable_(x);
        return;
    }

    std::mt19937 rng(42);
    std::uniform_int_distribution<int> qdist(-127, 127);
    std::uniform_real_distribution<float> ddist(0.5f, 2.0f);
    std::uniform_real_distribution<float> xdist(-4.0f, 4.0f);

    for (int b = 0; b < nBlocks; ++b)
    {
        const float d = ddist(rng);
        w[b].d = f32_to_f16_bits_(d);
        for (int i = 0; i < 32; ++i)
            w[b].qs[i] = (int8_t)qdist(rng);
    }
    for (int i = 0; i < cols; ++i)
        x[i] = xdist(rng);

    // Reference scalar: uses the exact same f16_to_f32_q8_ conversion.
    float ref = 0.0f;
    for (int b = 0; b < nBlocks; ++b)
    {
        const float d = f16_to_f32_q8_(w[b].d);
        for (int j = 0; j < 32; ++j)
        {
            const int idx = b * 32 + j;
            if (idx >= cols)
                break;
            ref += (float)w[b].qs[j] * d * x[idx];
        }
    }

    // Value: uses the shipped code path (AVX2 if cached+available).
    const float val = q8_0_dot_row_(w, x, cols);
    const float diff = std::fabs(ref - val);
    assert(diff < 1e-3f);
    std::fprintf(stderr, "[Q8_0 selftest] ok (diff=%.6g)\n", diff);

    aligned_free_portable_(w);
    aligned_free_portable_(x);
}
#endif  // RAWRXD_Q8_0_SELFTEST

static inline void matvec_q8_0_(const void* w, const float* x, float* y, int rows, int cols)
{
    // Q8_0 is stored as blocks in row-major order.
    const int blocksPerRow = (cols + 31) / 32;
    const Q8_0Block* wb = reinterpret_cast<const Q8_0Block*>(w);
    for (int r = 0; r < rows; ++r)
    {
        const Q8_0Block* rowBlocks = wb + (size_t)r * (size_t)blocksPerRow;
        y[r] = q8_0_dot_row_(rowBlocks, x, cols);
    }
}

static inline void matvec_f32_(const float* w, const float* x, float* y, int rows, int cols)
{
    for (int r = 0; r < rows; ++r)
    {
        const float* row = w + (size_t)r * (size_t)cols;
        float sum = 0.0f;
        for (int c = 0; c < cols; ++c)
        {
            sum += row[c] * x[c];
        }
        y[r] = sum;
    }
}

static inline RawrXD::GGMLType getTensorType_(StreamingGGUFLoader* loader, const std::string& name)
{
    if (!loader)
        return RawrXD::GGMLType::COUNT;
    const auto idx = loader->GetTensorIndex();
    for (const auto& ref : idx)
    {
        if (ref.name == name)
            return ref.type;
    }
    return RawrXD::GGMLType::COUNT;
}

static inline bool isSupportedWeightType_(RawrXD::GGMLType t)
{
    return t == RawrXD::GGMLType::F32 || t == RawrXD::GGMLType::F16 || t == RawrXD::GGMLType::Q8_0;
}

// ============================================================================
// Kernel registry (minimal, data-driven)
//
// At load time we bind each tensor name -> best available matvec kernel.
// Forward pass calls the bound kernel by name; no per-call type checks needed.
// ============================================================================
using MatVecFn_ = bool (*)(float* y, const void* w, const float* x, int rows, int cols);

static bool matvec_f32_kernel_(float* y, const void* w, const float* x, int rows, int cols)
{
    if (!y || !w || !x)
        return false;
    matvec_f32_(reinterpret_cast<const float*>(w), x, y, rows, cols);
    return true;
}

static bool matvec_q8_0_kernel_(float* y, const void* w, const float* x, int rows, int cols)
{
    if (!y || !w || !x)
        return false;
    matvec_q8_0_(w, x, y, rows, cols);
    return true;
}

struct KernelInfo_
{
    const char* name = nullptr;
    RawrXD::GGMLType weightType = RawrXD::GGMLType::COUNT;
    bool requiresAvx2 = false;
    int priority = 0;  // higher wins
    MatVecFn_ fn = nullptr;
};

static const KernelInfo_ kKernels_[] = {
    {"f32_matvec_scalar", RawrXD::GGMLType::F32, false, 10, &matvec_f32_kernel_},
    {"q8_0_matvec_scalar", RawrXD::GGMLType::Q8_0, false, 10, &matvec_q8_0_kernel_},
    // Q8_0 kernel auto-uses AVX2 internally when available; keep an AVX2-tagged entry
    // so the factory can prefer it when AVX2 is present.
    {"q8_0_matvec_avx2", RawrXD::GGMLType::Q8_0, true, 20, &matvec_q8_0_kernel_},
};

static std::unordered_map<std::string, const KernelInfo_*> g_tensorMatVecKernel;
struct LayerKernelConfig_
{
    const KernelInfo_* wq = nullptr;
    const KernelInfo_* wk = nullptr;
    const KernelInfo_* wv = nullptr;
    const KernelInfo_* wo = nullptr;
    const KernelInfo_* w1 = nullptr;  // ffn_gate
    const KernelInfo_* w3 = nullptr;  // ffn_up
    const KernelInfo_* w2 = nullptr;  // ffn_down
};
static std::vector<LayerKernelConfig_> g_layerKernels;

static inline const KernelInfo_* selectBestMatVecKernel_(RawrXD::GGMLType type)
{
    const bool avx2 = hasAvx2_();
    const KernelInfo_* best = nullptr;
    int bestPrio = -1;

    for (const auto& k : kKernels_)
    {
        if (k.weightType != type)
            continue;
        if (k.requiresAvx2 && !avx2)
            continue;
        if (k.priority > bestPrio)
        {
            bestPrio = k.priority;
            best = &k;
        }
    }
    return best;
}

static inline bool matvec_kernel_call_(const KernelInfo_* k, float* y, const void* w, const float* x, int rows,
                                       int cols)
{
    if (!k || !k->fn)
        return false;
    return k->fn(y, w, x, rows, cols);
}

static inline bool matvec_by_tensor_(const std::string& tensorName, const void* w, const float* x, float* y, int rows,
                                     int cols)
{
    if (!w || !x || !y || rows <= 0 || cols <= 0)
        return false;

    const auto it = g_tensorMatVecKernel.find(tensorName);
    if (it == g_tensorMatVecKernel.end() || !it->second || !it->second->fn)
    {
        return false;
    }
    return it->second->fn(y, w, x, rows, cols);
}

// Helper to load tensor and store in map
static uint8_t* LoadTensorData(StreamingGGUFLoader* loader, RawrXD::GGufTensorLoader* planLoader,
                               std::map<std::string, std::vector<uint8_t>>& store, const std::string& name)
{
    std::vector<uint8_t> data;
    if (planLoader)
    {
        (void)planLoader->ensureTensorAvailable(name);
    }
    if (loader->GetTensorData(name, data))
    {
        store[name] = std::move(data);
        return store[name].data();
    }
    // Alternate naming
    std::string alt = name;
    if (name.find("token_embd.weight") != std::string::npos)
        alt = "model.embed_tokens.weight";
    else if (name.find("output_norm.weight") != std::string::npos)
        alt = "model.norm.weight";
    else if (name.find("output.weight") != std::string::npos)
        alt = "lm_head.weight";

    if (alt != name)
    {
        if (planLoader)
        {
            (void)planLoader->ensureTensorAvailable(alt);
        }
        if (loader->GetTensorData(alt, data))
        {
            store[alt] = std::move(data);
            return store[alt].data();
        }
    }
    return nullptr;
}

CPUInferenceEngine::CPUInferenceEngine()
    : m_numLayers(0), m_numHeads(0), m_embeddingDim(0), m_vocabSize(0),
      m_threadCount(std::thread::hardware_concurrency()), m_modelLoaded(false), m_contextLimit(4096), m_currentPos(0)
{
    m_loader = std::make_unique<StreamingGGUFLoader>();
}

CPUInferenceEngine::~CPUInferenceEngine() {}

bool CPUInferenceEngine::LoadModel(const std::string& path)
{
    if (!m_loader->Open(path))
        return false;

    // Bunny-hop loader: zone-based residency decisions.
    RawrXD::GGufTensorLoader::Config ggufCfg;
    ggufCfg.budget.maxZoneBytes = 512ull * 1024ull * 1024ull;
    ggufCfg.budget.maxResidentBytes = 2ull * 1024ull * 1024ull * 1024ull;
    ggufCfg.budget.singleZoneResident = true;
    ggufCfg.filter.defaultDecision = RawrXD::LoadDecision::LAZY_LOAD;
    ggufCfg.lazyLoad = true;
    ggufCfg.logDecisions = false;  // keep noisy logs off by default in engine load
    RawrXD::GGufTensorLoader ggufPlan(*m_loader, ggufCfg);
    (void)ggufPlan.buildPlan();
    (void)ggufPlan.executePreload();

    // Cache CPU feature flags once per process (and optionally run a tiny Q8_0 sanity test).
    (void)hasAvx2_();
    g_tensorMatVecKernel.clear();
    g_layerKernels.clear();
#if defined(RAWRXD_Q8_0_SELFTEST)
    rawrxd_q8_0_selftest_();
#endif

    GGUFMetadata meta = m_loader->GetMetadata();
    m_vocabSize = meta.vocab_size;
    m_embeddingDim = meta.embedding_dim;
    m_numLayers = meta.layer_count;
    m_numHeads = meta.head_count;
    m_contextLimit = meta.context_length;

    // --------------------------------------------------------------------
    // Behavioral toggles (GGUF metadata KV pairs)
    // --------------------------------------------------------------------
    // This engine only honors a small, safe subset of toggles that map to
    // existing runtime flags. Unknown keys are ignored.
    const auto& kv = !meta.kv_pairs.empty() ? meta.kv_pairs : meta.properties;
    const auto kvGet = [&](const char* key) -> const std::string*
    {
        auto it = kv.find(key);
        if (it == kv.end())
            return nullptr;
        return &it->second;
    };
    const auto kvBool = [&](const char* key, bool def) -> bool
    {
        const std::string* v = kvGet(key);
        if (!v)
            return def;
        return (*v == "1" || *v == "true" || *v == "TRUE" || *v == "yes" || *v == "on");
    };
    const auto kvFloat = [&](const char* key, float def) -> float
    {
        const std::string* v = kvGet(key);
        if (!v)
            return def;
        try
        {
            return std::stof(*v);
        }
        catch (...)
        {
            return def;
        }
    };
    const auto kvInt = [&](const char* key, int def) -> int
    {
        const std::string* v = kvGet(key);
        if (!v)
            return def;
        try
        {
            return std::stoi(*v);
        }
        catch (...)
        {
            return def;
        }
    };

    // Core mode flags
    m_maxMode = kvBool("sovereign.max_mode", m_maxMode);
    m_deepThinking = kvBool("sovereign.deepthinking", m_deepThinking);
    m_deepResearch = kvBool("sovereign.deepresearch", m_deepResearch);

    // Sampling overrides (optional)
    const bool forceTemp = kvBool("sovereign.force_temperature", false);
    const bool forceTopK = kvBool("sovereign.force_top_k", false);
    const bool forceTopP = kvBool("sovereign.force_top_p", false);
    if (forceTemp || forceTopK || forceTopP)
    {
        const float temp = forceTemp ? kvFloat("sovereign.temperature_override", m_sampler.temp) : m_sampler.temp;
        const int topK = forceTopK ? kvInt("sovereign.top_k_override", m_sampler.top_k) : m_sampler.top_k;
        const float topP = forceTopP ? kvFloat("sovereign.top_p_override", m_sampler.top_p) : m_sampler.top_p;
        ConfigureSampling(temp, topP, topK, m_sampler.repeat_penalty);
    }

    int n_kv_heads = meta.head_count;  // Fallback if head_count_kv not available
    if (n_kv_heads == 0)
        n_kv_heads = m_numHeads;

    if (!meta.tokens.empty())
    {
        m_vocab = meta.tokens;
        g_tokenizerReady.store(g_tokenizer.LoadFromVocab(meta.tokens), std::memory_order_release);
    }
    else
    {
        for (int i = 0; i < std::max(100, m_vocabSize); i++)
            m_vocab.push_back("tok" + std::to_string(i));
        g_tokenizerReady.store(false, std::memory_order_release);
    }

    m_transformerLayers.clear();
    m_weight_store.clear();
    g_weightTypes.clear();

    // Global weights
    m_tok_embeddings = (float*)LoadTensorData(m_loader.get(), &ggufPlan, m_weight_store, "token_embd.weight");
    m_output_norm = (float*)LoadTensorData(m_loader.get(), &ggufPlan, m_weight_store, "output_norm.weight");
    m_output_weight_ptr = LoadTensorData(m_loader.get(), &ggufPlan, m_weight_store, "output.weight");

    // Record types (use canonical gguf names; if alt naming was used we still get a useful "missing" type).
    g_weightTypes["token_embd.weight"] = getTensorType_(m_loader.get(), "token_embd.weight");
    g_weightTypes["output_norm.weight"] = getTensorType_(m_loader.get(), "output_norm.weight");
    g_weightTypes["output.weight"] = getTensorType_(m_loader.get(), "output.weight");

    if (!m_tok_embeddings || !m_output_norm || !m_output_weight_ptr)
    {
        std::cerr << "Warning: Critical weights missing, inference will fail/crash.\n";
    }

    int hidden_dim = meta.feed_forward_length;

    for (int i = 0; i < m_numLayers; i++)
    {
        auto layer = std::make_unique<::TransformerLayer>(m_embeddingDim, m_numHeads, n_kv_heads, hidden_dim);
        std::string pre = "blk." + std::to_string(i) + ".";

        layer->attn_norm = (float*)LoadTensorData(m_loader.get(), &ggufPlan, m_weight_store, pre + "attn_norm.weight");
        layer->wq = LoadTensorData(m_loader.get(), &ggufPlan, m_weight_store, pre + "attn_q.weight");
        layer->wk = LoadTensorData(m_loader.get(), &ggufPlan, m_weight_store, pre + "attn_k.weight");
        layer->wv = LoadTensorData(m_loader.get(), &ggufPlan, m_weight_store, pre + "attn_v.weight");
        layer->wo = LoadTensorData(m_loader.get(), &ggufPlan, m_weight_store, pre + "attn_output.weight");

        layer->ffn_norm = (float*)LoadTensorData(m_loader.get(), &ggufPlan, m_weight_store, pre + "ffn_norm.weight");
        layer->w1 = LoadTensorData(m_loader.get(), &ggufPlan, m_weight_store, pre + "ffn_gate.weight");
        layer->w3 = LoadTensorData(m_loader.get(), &ggufPlan, m_weight_store, pre + "ffn_up.weight");
        layer->w2 = LoadTensorData(m_loader.get(), &ggufPlan, m_weight_store, pre + "ffn_down.weight");

        // Store per-tensor types for dispatch.
        g_weightTypes[pre + "attn_q.weight"] = getTensorType_(m_loader.get(), pre + "attn_q.weight");
        g_weightTypes[pre + "attn_k.weight"] = getTensorType_(m_loader.get(), pre + "attn_k.weight");
        g_weightTypes[pre + "attn_v.weight"] = getTensorType_(m_loader.get(), pre + "attn_v.weight");
        g_weightTypes[pre + "attn_output.weight"] = getTensorType_(m_loader.get(), pre + "attn_output.weight");
        g_weightTypes[pre + "ffn_gate.weight"] = getTensorType_(m_loader.get(), pre + "ffn_gate.weight");
        g_weightTypes[pre + "ffn_up.weight"] = getTensorType_(m_loader.get(), pre + "ffn_up.weight");
        g_weightTypes[pre + "ffn_down.weight"] = getTensorType_(m_loader.get(), pre + "ffn_down.weight");

        // Fast sanity: if any weight is unsupported, fail cleanly (no silent corruption).
        if (!isSupportedWeightType_(g_weightTypes[pre + "attn_q.weight"]) ||
            !isSupportedWeightType_(g_weightTypes[pre + "attn_k.weight"]) ||
            !isSupportedWeightType_(g_weightTypes[pre + "attn_v.weight"]) ||
            !isSupportedWeightType_(g_weightTypes[pre + "attn_output.weight"]) ||
            !isSupportedWeightType_(g_weightTypes[pre + "ffn_gate.weight"]) ||
            !isSupportedWeightType_(g_weightTypes[pre + "ffn_up.weight"]) ||
            !isSupportedWeightType_(g_weightTypes[pre + "ffn_down.weight"]))
        {
            std::cerr << "Unsupported quantization type detected at layer " << i << ".\n";
            return false;
        }

        // Bind tensor -> best matvec kernel (data-driven selection).
        LayerKernelConfig_ lk{};
        for (const auto& name :
             {pre + "attn_q.weight", pre + "attn_k.weight", pre + "attn_v.weight", pre + "attn_output.weight",
              pre + "ffn_gate.weight", pre + "ffn_up.weight", pre + "ffn_down.weight"})
        {
            const RawrXD::GGMLType t = g_weightTypes[name];
            const KernelInfo_* best = selectBestMatVecKernel_(t);
            if (!best)
            {
                std::cerr << "No matvec kernel available for tensor " << name << "\n";
                return false;
            }
            g_tensorMatVecKernel[name] = best;

            if (name == pre + "attn_q.weight")
                lk.wq = best;
            else if (name == pre + "attn_k.weight")
                lk.wk = best;
            else if (name == pre + "attn_v.weight")
                lk.wv = best;
            else if (name == pre + "attn_output.weight")
                lk.wo = best;
            else if (name == pre + "ffn_gate.weight")
                lk.w1 = best;
            else if (name == pre + "ffn_up.weight")
                lk.w3 = best;
            else if (name == pre + "ffn_down.weight")
                lk.w2 = best;
        }
        g_layerKernels.push_back(lk);

#if defined(RAWRXD_Q8_0_SELFTEST)
        // Smoke-check on *real* loaded Q8_0 weights: first row of Wq against scalar reference.
        // This catches stride/layout regressions that synthetic tests might miss.
        if (i == 0 && g_weightTypes[pre + "attn_q.weight"] == RawrXD::GGMLType::Q8_0 && layer->wq)
        {
            std::vector<float> x((size_t)m_embeddingDim);
            std::mt19937 rng(1337);
            std::uniform_real_distribution<float> xdist(-2.0f, 2.0f);
            for (int j = 0; j < m_embeddingDim; ++j)
                x[(size_t)j] = xdist(rng);

            const auto* row0 = reinterpret_cast<const Q8_0Block*>(layer->wq);
            const float ref = q8_0_dot_scalar_masked_(row0, x.data(), m_embeddingDim);
            const float val = q8_0_dot_row_(row0, x.data(), m_embeddingDim);
            const float diff = std::fabs(ref - val);
            assert(diff < 1e-3f);
            std::fprintf(stderr, "[Q8_0 selftest] real-weight row0 ok (diff=%.6g)\n", diff);
        }
#endif

        m_transformerLayers.push_back(std::move(layer));
        if (i % 5 == 0)
            std::cout << "Loaded layer " << i << "/" << m_numLayers << "\r" << std::flush;
    }
    std::cout << "Model Loaded Successfully.\n";

    m_modelLoaded = true;
    InitKVCache();
    return true;
}

void CPUInferenceEngine::InitKVCache()
{
    m_currentPos = 0;
    // Iterate over m_transformerLayers (pointers)
    for (auto& layer : m_transformerLayers)
    {
        if (layer)
        {
            layer->cache_pos = 0;
            layer->total_tokens_seen = 0;
        }
    }
}

std::vector<int32_t> CPUInferenceEngine::Generate(const std::vector<int32_t>& input_tokens, int max_tokens)
{
    std::vector<int32_t> result = input_tokens;
    if (!m_modelLoaded)
        return result;

    InitKVCache();

    // Prefill
    for (size_t i = 0; i < input_tokens.size(); ++i)
    {
        m_currentPos = i;
        std::vector<int32_t> single_tok = {input_tokens[i]};
        Eval(single_tok);
    }

    int current_token = result.back();
    m_currentPos = input_tokens.size();

    for (int i = 0; i < max_tokens; ++i)
    {
        std::vector<int32_t> ctx = {current_token};
        std::vector<float> logits = Eval(ctx);
        if (logits.empty())
            break;

        int next_token = 0;
        float max_val = -1e9f;
        for (size_t j = 0; j < logits.size(); ++j)
        {
            if (logits[j] > max_val)
            {
                max_val = logits[j];
                next_token = (int)j;
            }
        }

        result.push_back(next_token);
        current_token = next_token;
        m_currentPos++;

        if (current_token == 2)
            break;
    }
    return result;
}

void CPUInferenceEngine::GenerateStreaming(const std::vector<int32_t>& input_tokens, int max_tokens,
                                           std::function<void(const std::string&)> token_callback,
                                           std::function<void()> complete_callback,
                                           std::function<void(int32_t)> token_id_callback)
{
    if (!m_modelLoaded || input_tokens.empty())
        return;

    InitKVCache();
    for (size_t i = 0; i < input_tokens.size(); ++i)
    {
        m_currentPos = i;
        std::vector<int32_t> single_tok = {input_tokens[i]};
        Eval(single_tok);
    }

    int current_token = input_tokens.back();
    m_currentPos = input_tokens.size();

    for (int step = 0; step < max_tokens; ++step)
    {
        std::vector<int32_t> ctx = {current_token};
        std::vector<float> logits = Eval(ctx);
        if (logits.empty())
            break;

        int next_token = 0;
        float max_val = -1e9f;
        for (size_t j = 0; j < logits.size(); ++j)
        {
            if (logits[j] > max_val)
            {
                max_val = logits[j];
                next_token = (int)j;
            }
        }

        if (token_id_callback)
            token_id_callback(next_token);
        if (token_callback && next_token < (int)m_vocab.size())
            token_callback(m_vocab[next_token]);

        current_token = next_token;
        m_currentPos++;

        if (current_token == 2)
            break;
    }

    if (complete_callback)
        complete_callback();
}

std::vector<float> CPUInferenceEngine::Eval(const std::vector<int32_t>& input_tokens)
{
    if (!m_modelLoaded || input_tokens.empty())
        return {};

    const int token = input_tokens[0];

    // Reuse state buffers to avoid per-token allocations.
    static thread_local std::vector<float> x;
    static thread_local std::vector<float> y;
    x.assign((size_t)m_embeddingDim, 0.0f);
    y.assign((size_t)m_embeddingDim, 0.0f);

    // Embedding
    if (m_tok_embeddings)
    {
        float* emb = m_tok_embeddings + token * m_embeddingDim;
        std::memcpy(x.data(), emb, m_embeddingDim * sizeof(float));
    }

    // Layers (single-token step)
    for (int layerIdx = 0; layerIdx < (int)m_transformerLayers.size(); ++layerIdx)
    {
        if (!m_transformerLayers[layerIdx])
            continue;
        TransformerLayerMain(x.data(), y.data(), layerIdx, /*seq_len=*/1);
        x.swap(y);
    }

    // Final Norm
    if (m_output_norm)
    {
        RMSNorm(x.data(), m_embeddingDim, 1e-5f);
        for (int d = 0; d < m_embeddingDim; ++d)
        {
            x[d] *= m_output_norm[d];
        }
    }

    // LM Head
    std::vector<float> logits(m_vocabSize);

    if (m_output_weight_ptr)
    {
        float* w = (float*)m_output_weight_ptr;
        for (int i = 0; i < m_vocabSize; ++i)
        {
            float sum = 0.0f;
            for (int j = 0; j < m_embeddingDim; ++j)
            {
                sum += x[j] * w[i * m_embeddingDim + j];
            }
            logits[i] = sum;
        }
    }

    return logits;
}

// ============================================================================
// Missing API Implementation
// ============================================================================

bool CPUInferenceEngine::loadModel(const std::string& path)
{
    return LoadModel(path);
}

std::vector<int32_t> CPUInferenceEngine::Tokenize(const std::string& text)
{
    if (g_tokenizerReady.load(std::memory_order_acquire))
    {
        const auto ids = g_tokenizer.Encode(text);
        std::vector<int32_t> out;
        out.reserve(ids.size());
        for (auto id : ids)
            out.push_back((int32_t)id);
        return out;
    }

    std::vector<int32_t> tokens;
    size_t i = 0;
    while (i < text.size())
    {
        int best_id = -1;
        size_t best_len = 0;
        size_t max_search = std::min(text.size() - i, (size_t)64);

        for (int v = 0; v < (int)m_vocab.size(); ++v)
        {
            const auto& vocab_token = m_vocab[v];
            if (vocab_token.size() > best_len && vocab_token.size() <= max_search)
            {
                if (text.compare(i, vocab_token.size(), vocab_token) == 0)
                {
                    best_len = vocab_token.size();
                    best_id = v;
                }
            }
        }

        if (best_id != -1)
        {
            tokens.push_back(best_id);
            i += best_len;
        }
        else
        {
            i++;
        }
    }
    return tokens;
}

std::string CPUInferenceEngine::Detokenize(const std::vector<int32_t>& tokens)
{
    if (g_tokenizerReady.load(std::memory_order_acquire))
    {
        std::vector<uint32_t> ids;
        ids.reserve(tokens.size());
        for (auto id : tokens)
            ids.push_back((uint32_t)std::max(0, id));
        return g_tokenizer.DecodeSafe(ids);
    }

    std::string result;
    for (auto id : tokens)
    {
        if (id >= 0 && id < (int)m_vocab.size())
        {
            result += m_vocab[id];
        }
    }
    return result;
}

void CPUInferenceEngine::SetContextLimit(size_t limit)
{
    m_contextLimit = limit;
}
void CPUInferenceEngine::SetThreadCount(int count)
{
    m_threadCount = count;
}
void CPUInferenceEngine::SetMaxMode(bool enabled)
{
    m_maxMode = enabled;
}
void CPUInferenceEngine::SetDeepThinking(bool enabled)
{
    m_deepThinking = enabled;
}
void CPUInferenceEngine::SetDeepResearch(bool enabled)
{
    m_deepResearch = enabled;
}

void CPUInferenceEngine::ConfigureSampling(float temperature, float top_p, int top_k, float repeat_penalty)
{
    m_sampler.temp = temperature;
    m_sampler.top_p = top_p;
    m_sampler.top_k = top_k;
    m_sampler.repeat_penalty = repeat_penalty;
}

size_t CPUInferenceEngine::GetMemoryUsage() const
{
    size_t total = 0;
    for (const auto& kv : m_weight_store)
        total += kv.second.size();
    return total;
}

// Math Wrappers
void CPUInferenceEngine::MatMul(const float* A, const float* B, float* C, int m, int n, int k)
{
    for (int i = 0; i < m; i++)
    {
        for (int j = 0; j < n; j++)
        {
            float sum = 0.0f;
            for (int l = 0; l < k; l++)
            {
                sum += A[i * k + l] * B[j * k + l];
            }
            C[i * n + j] = sum;
        }
    }
}

void CPUInferenceEngine::Softmax(float* data, int size)
{
    float max_val = -1e9f;
    for (int i = 0; i < size; i++)
        max_val = std::max(max_val, data[i]);
    float sum = 0.0f;
    for (int i = 0; i < size; i++)
    {
        data[i] = std::exp(data[i] - max_val);
        sum += data[i];
    }
    float scale = 1.0f / sum;
    for (int i = 0; i < size; i++)
        data[i] *= scale;
}

void CPUInferenceEngine::RMSNorm(float* data, int size, float epsilon)
{
    InferenceKernels::rmsnorm_avx512(data, data, nullptr, size, epsilon);
}
void CPUInferenceEngine::LayerNorm(float* data, int size, float epsilon)
{
    RMSNorm(data, size, epsilon);
}
void CPUInferenceEngine::RoPE(float* data, int dim, int pos, int rotary_dim)
{
    InferenceKernels::rope_avx512(data, data, dim, pos, 10000.0f, 1.0f);
}
void CPUInferenceEngine::SiLU(float* data, int size)
{
    for (int i = 0; i < size; i++)
        data[i] = data[i] / (1.0f + std::exp(-data[i]));
}
void CPUInferenceEngine::GELU(float* data, int size)
{
    SiLU(data, size);
}
void CPUInferenceEngine::FeedForward(const float* input, float* output, int layer_idx)
{
    if (!input || !output || layer_idx < 0 || layer_idx >= static_cast<int>(m_transformerLayers.size()))
        return;
    auto& layer = m_transformerLayers[layer_idx];
    if (!layer->w1 || !layer->w2 || !layer->w3)
        return;
    if ((size_t)layer_idx >= g_layerKernels.size())
        return;

    const int hidden = layer->hiddenDim;
    const int dim = m_embeddingDim;
    // Reuse buffers to avoid per-token allocations.
    static thread_local std::vector<float> gate;
    static thread_local std::vector<float> up;
    gate.assign((size_t)hidden, 0.0f);
    up.assign((size_t)hidden, 0.0f);

    const LayerKernelConfig_& lk = g_layerKernels[(size_t)layer_idx];

    // gate = W1 @ input (gate projection)
    if (!matvec_kernel_call_(lk.w1, gate.data(), layer->w1, input, hidden, dim))
        return;
    // up = W3 @ input (up projection)
    if (!matvec_kernel_call_(lk.w3, up.data(), layer->w3, input, hidden, dim))
        return;

    // SiLU(gate) * up
    for (int i = 0; i < hidden; ++i)
    {
        gate[i] = gate[i] / (1.0f + std::exp(-gate[i]));
        gate[i] *= up[i];
    }

    // output = W2 @ gate (down projection)
    if (!matvec_kernel_call_(lk.w2, output, layer->w2, gate.data(), dim, hidden))
        return;
}

void CPUInferenceEngine::MultiHeadAttention(const float* Q, const float* K, const float* V, float* output, int seq_len,
                                            int head_dim, int num_heads, int layer_idx)
{
    if (!Q || !K || !V || !output || layer_idx < 0)
        return;
    const int dim = num_heads * head_dim;
    const float scale = 1.0f / std::sqrt(static_cast<float>(head_dim));

    for (int h = 0; h < num_heads; ++h)
    {
        // Reuse score buffer to avoid per-position allocations.
        static thread_local std::vector<float> scores;
        scores.assign((size_t)seq_len, 0.0f);
        for (int i = 0; i < seq_len; ++i)
        {
            // Compute attention scores for this query position
            float max_score = -std::numeric_limits<float>::infinity();

            for (int j = 0; j < seq_len; ++j)
            {
                // Causal mask: disallow attending to future tokens.
                if (j > i)
                {
                    scores[j] = -std::numeric_limits<float>::infinity();
                    continue;
                }
                float dot = 0.0f;
                for (int d = 0; d < head_dim; ++d)
                {
                    dot += Q[i * dim + h * head_dim + d] * K[j * dim + h * head_dim + d];
                }
                scores[j] = dot * scale;
                if (scores[j] > max_score)
                    max_score = scores[j];
            }

            // Softmax
            float sum_exp = 0.0f;
            for (int j = 0; j < seq_len; ++j)
            {
                scores[j] = std::exp(scores[j] - max_score);
                sum_exp += scores[j];
            }
            for (int j = 0; j < seq_len; ++j)
            {
                scores[j] /= sum_exp;
            }

            // Weighted sum of values
            for (int d = 0; d < head_dim; ++d)
            {
                float sum = 0.0f;
                for (int j = 0; j < seq_len; ++j)
                {
                    sum += scores[j] * V[j * dim + h * head_dim + d];
                }
                output[i * dim + h * head_dim + d] = sum;
            }
        }
    }
}

bool CPUInferenceEngine::LoadWeights(const std::unordered_map<std::string, Tensor>& tensors)
{
    if (tensors.empty())
        return false;
    m_weight_store.clear();
    for (const auto& [name, tensor] : tensors)
    {
        std::vector<uint8_t> data(tensor.data, tensor.data + tensor.size);
        m_weight_store[name] = std::move(data);
    }
    return true;
}

void CPUInferenceEngine::UpdateWeights(const std::vector<std::vector<float>>& gradients, float lr)
{
    if (gradients.empty() || lr <= 0.0f)
        return;
    // Apply SGD update to layer weights (simplified)
    for (size_t l = 0; l < m_transformerLayers.size() && l < gradients.size(); ++l)
    {
        auto& layer = m_transformerLayers[l];
        if (layer->wq && gradients[l].size() >= static_cast<size_t>(m_embeddingDim * m_embeddingDim))
        {
            float* wq = reinterpret_cast<float*>(layer->wq);
            for (size_t i = 0; i < gradients[l].size() && i < static_cast<size_t>(m_embeddingDim * m_embeddingDim); ++i)
            {
                wq[i] -= lr * gradients[l][i];
            }
        }
    }
}

void CPUInferenceEngine::UpdateOutputWeights(const std::vector<float>& gradients, float lr)
{
    if (gradients.empty() || lr <= 0.0f || !m_output_weight_ptr)
        return;
    float* w = reinterpret_cast<float*>(m_output_weight_ptr);
    const size_t count = std::min(gradients.size(), static_cast<size_t>(m_vocabSize * m_embeddingDim));
    for (size_t i = 0; i < count; ++i)
    {
        w[i] -= lr * gradients[i];
    }
}

void CPUInferenceEngine::TransformerLayerMain(const float* input, float* output, int layer_idx, int seq_len)
{
    if (!input || !output || layer_idx < 0 || layer_idx >= static_cast<int>(m_transformerLayers.size()))
        return;
    auto& layer = m_transformerLayers[layer_idx];
    const int dim = m_embeddingDim;
    const int head_dim = dim / m_numHeads;
    if ((size_t)layer_idx >= g_layerKernels.size())
        return;
    const LayerKernelConfig_& lk = g_layerKernels[(size_t)layer_idx];

    // Scratch arena (reused per-thread) — avoids heap churn in the hot path.
    static thread_local std::vector<float> normed;
    static thread_local std::vector<float> q;
    static thread_local std::vector<float> k;
    static thread_local std::vector<float> v;
    static thread_local std::vector<float> attn_out;
    static thread_local std::vector<float> ff_out;
    const size_t n = (size_t)dim * (size_t)seq_len;
    normed.assign(n, 0.0f);
    q.assign(n, 0.0f);
    k.assign(n, 0.0f);
    v.assign(n, 0.0f);
    attn_out.assign(n, 0.0f);
    ff_out.assign(n, 0.0f);

    for (int i = 0; i < seq_len; ++i)
    {
        // Attention norm
        for (int d = 0; d < dim; ++d)
        {
            normed[i * dim + d] = input[i * dim + d];
        }
        if (layer->attn_norm)
        {
            RMSNorm(&normed[i * dim], dim, 1e-5f);
            for (int d = 0; d < dim; ++d)
            {
                normed[i * dim + d] *= reinterpret_cast<float*>(layer->attn_norm)[d];
            }
        }

        // Q, K, V projections (type-dispatched)
        if (!matvec_kernel_call_(lk.wq, &q[i * dim], layer->wq, &normed[i * dim], dim, dim))
            return;
        if (!matvec_kernel_call_(lk.wk, &k[i * dim], layer->wk, &normed[i * dim], dim, dim))
            return;
        if (!matvec_kernel_call_(lk.wv, &v[i * dim], layer->wv, &normed[i * dim], dim, dim))
            return;

        // Apply RoPE
        RoPE(&q[i * dim], dim, i, head_dim);
        RoPE(&k[i * dim], dim, i, head_dim);
    }

    // Multi-head attention
    MultiHeadAttention(q.data(), k.data(), v.data(), attn_out.data(), seq_len, head_dim, m_numHeads, layer_idx);

    // Output projection + residual
    for (int i = 0; i < seq_len; ++i)
    {
        for (int d = 0; d < dim; ++d)
        {
            output[i * dim + d] = input[i * dim + d];
        }

        // proj = WO @ attn_out; output = input + proj
        static thread_local std::vector<float> proj;
        proj.assign((size_t)dim, 0.0f);
        if (!matvec_kernel_call_(lk.wo, proj.data(), layer->wo, &attn_out[i * dim], dim, dim))
            return;
        for (int d = 0; d < dim; ++d)
        {
            output[i * dim + d] += proj[d];
        }
    }

    // FFN norm + feedforward + residual
    for (int i = 0; i < seq_len; ++i)
    {
        for (int d = 0; d < dim; ++d)
        {
            normed[i * dim + d] = output[i * dim + d];
        }
        if (layer->ffn_norm)
        {
            RMSNorm(&normed[i * dim], dim, 1e-5f);
            for (int d = 0; d < dim; ++d)
            {
                normed[i * dim + d] *= reinterpret_cast<float*>(layer->ffn_norm)[d];
            }
        }
    }

    for (int i = 0; i < seq_len; ++i)
    {
        FeedForward(&normed[i * dim], &ff_out[i * dim], layer_idx);
        for (int d = 0; d < dim; ++d)
        {
            output[i * dim + d] += ff_out[i * dim + d];
        }
    }
}

}  // namespace CPUInference
