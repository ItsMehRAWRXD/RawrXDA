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
        // Byte offset into the matrix buffer for this block
        const uint blockBase = (row * cbBlocksPerRow + b) * 18u;

        // ── Load 16 nibble bytes (4 × uint32, then mask) ──
        // ByteAddressBuffer.Load4 requires 4-byte aligned addresses.
        // Block nibble data: bytes 0-15 (unaligned in general → use byte-by-byte Load)
        // We load as two Load4 at offsets aligned down.
        const uint alignedBase = blockBase & ~3u;
        const uint byteShift   = blockBase & 3u;

        // Load the 16 nibble bytes as a uint4 pair (may straddle alignment)
        // Strategy: load 5 uint32 words covering bytes [blockBase, blockBase+15]
        uint raw[5];
        raw[0] = g_matrix.Load(alignedBase);
        raw[1] = g_matrix.Load(alignedBase + 4u);
        raw[2] = g_matrix.Load(alignedBase + 8u);
        raw[3] = g_matrix.Load(alignedBase + 12u);
        raw[4] = g_matrix.Load(alignedBase + 16u);

        // Extract 16 contiguous bytes starting at byte byteShift within raw[0..4]
        // Pack them into nibbles[0..3] as packed uint32 (4 bytes per uint)
        uint nibbles[4];
        [unroll]
        for (uint wi = 0; wi < 4u; ++wi) {
            const uint bitOff = byteShift * 8u + wi * 32u;
            const uint lo     = raw[bitOff / 32u    ];
            const uint hi     = raw[bitOff / 32u + 1u];
            nibbles[wi] = (lo >> (bitOff & 31u)) | (hi << (32u - (bitOff & 31u)));
        }

        // ── Read fp16 scale at bytes [16, 17] within block ──
        const uint scByteOff  = blockBase + 16u;
        const uint scAligned  = scByteOff & ~3u;
        const uint scRaw      = g_matrix.Load(scAligned);
        const uint scShift    = (scByteOff & 3u) * 8u;
        const uint scHalf     = (scRaw >> scShift) & 0xFFFFu;
        const float scale     = fp16_to_float(scHalf);

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
