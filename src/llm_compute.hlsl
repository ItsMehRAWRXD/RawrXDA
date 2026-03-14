// llm_compute.hlsl — GPU compute kernels for LLM inference
// Compiled at runtime via D3DCompile and executed through d3d12_compute.cpp.
//
// Entry points:
//   CSMatVecQ4   — Q4_0 matrix × float vector  (256 threads per group, 1 group per N rows)
//   CSRMSNorm    — RMS normalisation in-place   (1 thread group per row, up to 1024-wide)
//   CSSoftmax    — Numerically-stable softmax   (1 thread group, up to 1024 elements)
//   CSRoPE       — Rotary position embedding    (1 thread per pair, groups of 256)
//
// Dispatch conventions:
//   CSMatVecQ4 : Dispatch((rows + 255) / 256, 1, 1)
//   CSRMSNorm  : Dispatch(1, 1, 1)  with cbDim set to row width
//   CSSoftmax  : Dispatch(1, 1, 1)  with cbDim set to count
//   CSRoPE     : Dispatch((dim/2 + 255) / 256, 1, 1)
//
// Constant buffer layout (all kernels share slot b0):
//   uint   cbRows         row count   (CSMatVecQ4)
//   uint   cbCols         col count   (CSMatVecQ4 — must be multiple of 32)
//   uint   cbBlocksPerRow cols / 32   (CSMatVecQ4)
//   uint   cbDim          vector dim  (CSRMSNorm / CSSoftmax / CSRoPE)
//   uint   cbPosition     token pos   (CSRoPE)
//   uint   cbThetaBase    RoPE base   (CSRoPE, typically 10000)
//   float  cbEps          RMSNorm ε   (CSRMSNorm, typically 1e-5)
//   uint   cbPad          padding
// — 32 bytes total.

cbuffer Constants : register(b0) {
    uint  cbRows;
    uint  cbCols;
    uint  cbBlocksPerRow;
    uint  cbDim;
    uint  cbPosition;
    uint  cbThetaBase;
    float cbEps;
    uint  cbPad;
};

// Q4_0 packed matrix: ByteAddressBuffer (each block = 18 bytes: 16 nibble-bytes + 2 fp16-scale)
ByteAddressBuffer g_matrix       : register(t0);

// Input float vector
StructuredBuffer<float> g_vec    : register(t1);

// Input/output for in-place kernels (RMSNorm, Softmax, RoPE)
RWStructuredBuffer<float> g_inout : register(u0);

// Output for MatVecQ4
RWStructuredBuffer<float> g_out   : register(u1);

// Gamma weights for RMSNorm
StructuredBuffer<float> g_gamma  : register(t2);

// ── Shared memory for parallel reductions ──────────────────────────────────────
groupshared float gs_scratch[1024];

// ── fp16 → fp32 decode (software, no fp16 hardware requirement) ───────────────
float fp16_to_float(uint h) {
    const uint sign  = (h >> 15) & 1u;
    const uint exp16 = (h >> 10) & 0x1Fu;
    const uint mant  = h & 0x3FFu;
    float f;

    if (exp16 == 0u) {
        f = asfloat((sign << 31) | (mant << 13));   // subnormal / zero
        f *= 6.103515625e-05f;                       // 2^(-14) for denorm scale
    } else if (exp16 == 31u) {
        f = asfloat((sign << 31) | 0x7F800000u | (mant << 13)); // ±Inf / NaN
    } else {
        f = asfloat((sign << 31) | ((exp16 + 112u) << 23) | (mant << 13));
    }
    return f;
}

// ── CSMatVecQ4 ─────────────────────────────────────────────────────────────────
// One thread = one output row.
// Uses ByteAddressBuffer loads for unaligned 18-byte blocks.
[numthreads(256, 1, 1)]
void CSMatVecQ4(uint3 tid : SV_DispatchThreadID) {
    const uint row = tid.x;
    if (row >= cbRows) return;

    float sum = 0.0f;

    for (uint b = 0; b < cbBlocksPerRow; ++b) {
        // Byte offset into the matrix buffer for this block (20 bytes per block, strictly aligned)
        const uint blockBase = (row * cbBlocksPerRow + b) * 20u;

        // ── Load 16 nibble bytes (4 × uint32) ──
        // Since blockBase is a multiple of 20, it is ALWAYS 4-byte aligned (20 % 4 == 0)
        uint4 rawNibbles = g_matrix.Load4(blockBase);
        uint nibbles[4] = { rawNibbles.x, rawNibbles.y, rawNibbles.z, rawNibbles.w };

        // ── Read fp16 scale at bytes [16, 17] within block ──
        // blockBase + 16u is also 4-byte aligned!
        const uint scRaw = g_matrix.Load(blockBase + 16u);
        const float scale = fp16_to_float(scRaw & 0xFFFFu);

        // ── Dequantize 32 nibbles and accumulate ──
        const uint vecBase = b * 32u;

        [unroll]
        for (uint wi2 = 0; wi2 < 4u; ++wi2) {
            const uint packed = nibbles[wi2];
            [unroll]
            for (uint bi = 0; bi < 8u; ++bi) {
                // Extract nibble (4 bits)
                const uint nibShift = bi * 4u;
                const uint nib = (packed >> nibShift) & 0xFu;
                // Center: Q4_0 zero-point = 8  →  signed value ∈ [-8, +7]
                const float dqVal = (float)((int)nib - 8) * scale;
                const uint  vecIdx = vecBase + wi2 * 8u + bi;
                sum += dqVal * g_vec[vecIdx];
            }
        }
    }

    g_out[row] = sum;
}

// ── CSRMSNorm ──────────────────────────────────────────────────────────────────
// In-place: g_inout[0..cbDim-1] → normalised values.
// Single thread group; up to 1024 elements (sum-of-squares via tree reduction).
[numthreads(1024, 1, 1)]
void CSRMSNorm(uint3 tid : SV_DispatchThreadID, uint gtid : SV_GroupIndex) {
    const uint lane = gtid;

    // ── Pass 1: partial sum of squares per lane ──
    float ss = 0.0f;
    for (uint i = lane; i < cbDim; i += 1024u) {
        const float v = g_inout[i];
        ss += v * v;
    }
    gs_scratch[lane] = ss;
    GroupMemoryBarrierWithGroupSync();

    // Tree reduction over 1024 → 1
    for (uint stride = 512u; stride > 0u; stride >>= 1u) {
        if (lane < stride) gs_scratch[lane] += gs_scratch[lane + stride];
        GroupMemoryBarrierWithGroupSync();
    }

    const float rms = rsqrt(gs_scratch[0] / (float)cbDim + cbEps);

    // ── Pass 2: normalise and scale ──
    for (uint i = lane; i < cbDim; i += 1024u) {
        g_inout[i] = g_inout[i] * rms * g_gamma[i];
    }
}

// ── CSSoftmax ─────────────────────────────────────────────────────────────────
// In-place: g_inout[0..cbDim-1] → softmax probabilities.
// Three-pass (max, exp+sum, div) with tree reductions.
[numthreads(1024, 1, 1)]
void CSSoftmax(uint3 tid : SV_DispatchThreadID, uint gtid : SV_GroupIndex) {
    const uint lane = gtid;

    // ── Pass 1: max ──
    float lmax = -1e38f;
    for (uint i = lane; i < cbDim; i += 1024u) {
        lmax = max(lmax, g_inout[i]);
    }
    gs_scratch[lane] = lmax;
    GroupMemoryBarrierWithGroupSync();

    for (uint stride = 512u; stride > 0u; stride >>= 1u) {
        if (lane < stride) gs_scratch[lane] = max(gs_scratch[lane], gs_scratch[lane + stride]);
        GroupMemoryBarrierWithGroupSync();
    }
    const float globalMax = gs_scratch[0];

    // ── Pass 2: exp(x - max). HLSL exp() is faithful to ±1 ULP on SM5+. ──
    float lsum = 0.0f;
    for (uint i = lane; i < cbDim; i += 1024u) {
        const float e = exp(g_inout[i] - globalMax);
        g_inout[i] = e;
        lsum += e;
    }
    gs_scratch[lane] = lsum;
    GroupMemoryBarrierWithGroupSync();

    for (uint stride = 512u; stride > 0u; stride >>= 1u) {
        if (lane < stride) gs_scratch[lane] += gs_scratch[lane + stride];
        GroupMemoryBarrierWithGroupSync();
    }
    const float invSum = 1.0f / gs_scratch[0];

    // ── Pass 3: normalise ──
    for (uint i = lane; i < cbDim; i += 1024u) {
        g_inout[i] *= invSum;
    }
}

// ── CSRoPE ────────────────────────────────────────────────────────────────────
// In-place rotary position embedding.
// g_inout layout: [re0, im0, re1, im1, ...] — cbDim total floats (cbDim/2 pairs).
// Each thread handles one (re, im) pair.
[numthreads(256, 1, 1)]
void CSRoPE(uint3 tid : SV_DispatchThreadID) {
    const uint pairIdx = tid.x;
    if (pairIdx >= cbDim / 2u) return;

    // freq = position / (theta_base ^ (2*pair / dim))
    // Use: freq = exp(-2*pair/dim * log(theta_base)) * position
    const float twoI_over_dim = 2.0f * (float)pairIdx / (float)cbDim;
    const float freq = (float)cbPosition
                     * exp(-twoI_over_dim * log((float)cbThetaBase));

    const float c = cos(freq);
    const float s = sin(freq);

    const uint reIdx = pairIdx * 2u;
    const uint imIdx = pairIdx * 2u + 1u;

    const float re = g_inout[reIdx];
    const float im = g_inout[imIdx];

    g_inout[reIdx] = re * c - im * s;
    g_inout[imIdx] = re * s + im * c;
}

// ── Phase D: Fused Layer Kernels ──────────────────────────────────────────────

// ── CSSiLU ────────────────────────────────────────────────────────────────────
// In-place SiLU (Swish) activation: x * sigmoid(x)
// g_inout[0..cbDim-1] → activated values.
[numthreads(256, 1, 1)]
void CSSiLU(uint3 tid : SV_DispatchThreadID) {
    const uint idx = tid.x;
    if (idx >= cbDim) return;
    const float x = g_inout[idx];
    g_inout[idx] = x / (1.0f + exp(-x));
}

// ── CSResidualAdd ─────────────────────────────────────────────────────────────
// g_inout[i] += g_vec[i] for i in [0..cbDim)
// Residual connection: adds the skip-connection vector to the current tensor.
[numthreads(256, 1, 1)]
void CSResidualAdd(uint3 tid : SV_DispatchThreadID) {
    const uint idx = tid.x;
    if (idx >= cbDim) return;
    g_inout[idx] += g_vec[idx];
}

// ── CSMatVecFP32 ──────────────────────────────────────────────────────────────
// Full-precision matrix × vector for Q/K/V/O/FFN projections.
// g_matrix is reinterpreted as StructuredBuffer<float> (not Q4_0).
// One thread = one output row. Uses ByteAddressBuffer with float loads.
// cbRows = output dim, cbCols = input dim.
[numthreads(256, 1, 1)]
void CSMatVecFP32(uint3 tid : SV_DispatchThreadID) {
    const uint row = tid.x;
    if (row >= cbRows) return;

    float sum = 0.0f;
    const uint base = row * cbCols;

    // Load from ByteAddressBuffer as float (4 bytes per element)
    for (uint i = 0; i < cbCols; ++i) {
        const float mval = asfloat(g_matrix.Load((base + i) * 4u));
        sum += mval * g_vec[i];
    }

    g_out[row] = sum;
}

// ── CSElementwiseMul ──────────────────────────────────────────────────────────
// g_out[i] = g_inout[i] * g_vec[i] for i in [0..cbDim)
// Used for gated FFN: gate_proj output * up_proj output
[numthreads(256, 1, 1)]
void CSElementwiseMul(uint3 tid : SV_DispatchThreadID) {
    const uint idx = tid.x;
    if (idx >= cbDim) return;
    g_out[idx] = g_inout[idx] * g_vec[idx];
}

// ════════════════════════════════════════════════════════════════════════════════
// Phase E: Persistent KV Cache Kernels
// ════════════════════════════════════════════════════════════════════════════════
//
// GPU KV cache layout (ByteAddressBuffer g_matrix):
//   Interleaved K/V per head per position:
//     offset(pos, head, isV) = ((pos * n_heads + head) * 2 + isV) * head_dim * 4
//
// Constants mapping for KV cache ops:
//   cbRows      = seq_len (number of valid positions in cache)
//   cbCols      = head_dim (dimension per head)
//   cbDim       = n_heads
//   cbPosition  = write position (for append) or query position
//   cbThetaBase = (unused, reserved)
//   cbEps       = attention scale factor = 1.0/sqrt(head_dim)

// ── CSKVCacheWrite ────────────────────────────────────────────────────────────
// Write a single head's K or V vector into the GPU KV cache.
// g_vec[0..cbCols-1] = the vector to write
// g_inout is used as the KV cache (RW)
// Writing at: g_inout[offset..offset+cbCols-1]
// where offset = (cbPosition * cbDim + head_idx) * 2 * cbCols + isV * cbCols
//
// We dispatch one thread per head×isV pair: total threads = cbDim * 2
// Each thread copies cbCols floats from g_vec to the cache position.
// Actually simpler: one thread per element. Dispatch threads = cbCols.
// The head index and isV flag come from cbBlocksPerRow:
//   cbBlocksPerRow = head_idx * 2 + isV
[numthreads(256, 1, 1)]
void CSKVCacheWrite(uint3 tid : SV_DispatchThreadID) {
    const uint elem = tid.x;
    if (elem >= cbCols) return;

    const uint headAndKV = cbBlocksPerRow; // head_idx * 2 + isV
    const uint stride    = cbDim * 2u * cbCols; // floats per position
    const uint offset    = cbPosition * stride + headAndKV * cbCols + elem;

    g_inout[offset] = g_vec[elem];
}

// ── CSAttentionHead ──────────────────────────────────────────────────────────
// Single-head attention: computes one head's contribution.
//   Q vector: g_vec[0..cbCols-1]          (head_dim floats, the query)
//   KV cache: g_inout[...]                (interleaved K/V for all positions)
//   Output:   g_out[0..cbCols-1]          (head_dim floats, context vector)
//
// Constants:
//   cbRows      = seq_len (positions to attend over: 0..seq_len-1)
//   cbCols      = head_dim
//   cbDim       = n_heads
//   cbBlocksPerRow = head_index (which head we're computing)
//   cbEps       = 1.0/sqrt(head_dim) (attention scale)
//
// Algorithm:
//   1. Compute scores[pos] = dot(Q, K[pos]) * scale  for pos in [0..seq_len)
//   2. Softmax scores
//   3. Output[d] = sum(scores[pos] * V[pos][d]) for d in [0..head_dim)
//
// Thread model: one thread group (1024 threads) handles the full computation.
// Limitation: seq_len <= 1024 (sufficient for single-token generation context).
[numthreads(1024, 1, 1)]
void CSAttentionHead(uint3 tid : SV_DispatchThreadID, uint gtid : SV_GroupIndex) {
    const uint lane     = gtid;
    const uint seq_len  = cbRows;
    const uint head_dim = cbCols;
    const uint n_heads  = cbDim;
    const uint head_idx = cbBlocksPerRow;
    const float scale   = cbEps; // reuse eps as attention scale

    // Cache stride: floats per position = n_heads * 2 * head_dim
    const uint posStride = n_heads * 2u * head_dim;

    // ── Step 1: Compute attention scores ──
    // Each lane handles one position (if lane < seq_len)
    float score = -1e38f;
    if (lane < seq_len) {
        // K offset for this head at position `lane`:
        // (lane * n_heads + head_idx) * 2 * head_dim + 0 (K, not V)
        const uint kBase = lane * posStride + head_idx * 2u * head_dim;

        float dot = 0.0f;
        for (uint d = 0; d < head_dim; ++d) {
            dot += g_vec[d] * g_inout[kBase + d];
        }
        score = dot * scale;
    }

    // ── Step 2: Softmax over scores ──
    // Tree reduction for max
    gs_scratch[lane] = score;
    GroupMemoryBarrierWithGroupSync();

    for (uint stride = 512u; stride > 0u; stride >>= 1u) {
        if (lane < stride)
            gs_scratch[lane] = max(gs_scratch[lane], gs_scratch[lane + stride]);
        GroupMemoryBarrierWithGroupSync();
    }
    const float globalMax = gs_scratch[0];

    // Exp + sum reduction
    float expVal = 0.0f;
    if (lane < seq_len) {
        expVal = exp(score - globalMax);
    }
    gs_scratch[lane] = expVal;
    GroupMemoryBarrierWithGroupSync();

    for (uint stride2 = 512u; stride2 > 0u; stride2 >>= 1u) {
        if (lane < stride2)
            gs_scratch[lane] += gs_scratch[lane + stride2];
        GroupMemoryBarrierWithGroupSync();
    }
    const float invSum = 1.0f / max(gs_scratch[0], 1e-10f);

    // Normalized attention weight for this lane's position
    float weight = (lane < seq_len) ? expVal * invSum : 0.0f;

    // Store weight in scratch for step 3
    gs_scratch[lane] = weight;
    GroupMemoryBarrierWithGroupSync();

    // ── Step 3: Weighted sum of V vectors ──
    // Each lane handles one output dimension (if lane < head_dim)
    if (lane < head_dim) {
        float acc = 0.0f;
        for (uint pos = 0; pos < seq_len; ++pos) {
            // V offset for this head at position `pos`:
            const uint vBase = pos * posStride + head_idx * 2u * head_dim + head_dim;
            acc += gs_scratch[pos] * g_inout[vBase + lane];
        }
        g_out[lane] = acc;
    }
}
