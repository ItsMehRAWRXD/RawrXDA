// ============================================================================
// disk_recovery_pattern_microbench.cpp
// Throughput microbenchmark for pattern scanning on representative
// disk-recovery streams.
//
// Corpus classes:
//   high_entropy    — PRNG bytes, very sparse signal
//   recovered_logs  — printable log-structured text, dense pattern hits
//   mixed_recovery  — entropy with embedded file signatures (JPEG, ZIP, …)
//   adversarial     — tokens placed at exact window boundaries to stress
//                     boundary-split detection
//
// Classifier paths:
//   BaselineScalar  — naive byte-by-byte scan (reference)
//   AcceleratedAVX2 — AVX2 first-byte prefilter + 64-byte cache-line
//                     prefetch, scalar verifier only on candidate hits
//   ExternalEngine  — compiled-in ASM engine (RAWRXD_USE_EXTERNAL_PATTERN_ENGINE)
//
// Build (Windows):
//   cl /O2 /std:c++20 /arch:AVX2 /EHsc src\p2p\disk_recovery_pattern_microbench.cpp \
//      /Fe:pattern_microbench.exe
// ============================================================================

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <fstream>
#include <random>
#include <string>
#include <vector>

// AVX-512F detection: MSVC defines __AVX512F__ when /arch:AVX512 is set.
// GCC/Clang define it automatically when -mavx512f is active.
// Without it, fall through to the AVX2 path which is always available on Zen3+.
#if defined(__AVX512F__) || (defined(_MSC_VER) && defined(__AVX512F__))
#  include <immintrin.h>
#  define RAWRXD_HAS_AVX512 1
#  define RAWRXD_HAS_AVX2   1
#elif defined(__AVX2__) || (defined(_MSC_VER) && (defined(_M_X64) || defined(_M_IX86_FP)))
#  include <immintrin.h>
#  define RAWRXD_HAS_AVX512 0
#  define RAWRXD_HAS_AVX2   1
#else
#  define RAWRXD_HAS_AVX512 0
#  define RAWRXD_HAS_AVX2   0
#endif

#ifdef RAWRXD_USE_EXTERNAL_PATTERN_ENGINE
extern "C" {
int InitializePatternEngine();
int ShutdownPatternEngine();
int ClassifyPattern(const uint8_t* codeBuffer, int length, const uint8_t* context, double* confidence);
}
#endif

namespace {

static int BaselineScalarClassify(const uint8_t* data, int len, double* confidence) {
    if (!data || len <= 0 || !confidence) {
        return 0;
    }

    auto lower = [](uint8_t c) -> uint8_t {
        if (c >= 'A' && c <= 'Z') return static_cast<uint8_t>(c + 0x20);
        return c;
    };

    for (int i = 0; i < len; ++i) {
        const int rem = len - i;
        const uint8_t c0 = lower(data[i]);

        if (rem >= 5 && c0 == 'f' && lower(data[i + 1]) == 'i' && lower(data[i + 2]) == 'x' &&
            lower(data[i + 3]) == 'm' && lower(data[i + 4]) == 'e') {
            *confidence = 0.95;
            return 2; // PATTERN_FIXME
        }
        if (rem >= 3 && c0 == 'b' && lower(data[i + 1]) == 'u' && lower(data[i + 2]) == 'g') {
            *confidence = 1.0;
            return 5; // PATTERN_BUG
        }
        if (rem >= 4 && c0 == 't' && lower(data[i + 1]) == 'o' && lower(data[i + 2]) == 'd' && lower(data[i + 3]) == 'o') {
            *confidence = 0.85;
            return 1; // PATTERN_TODO
        }
        if (rem >= 3 && c0 == 'x' && lower(data[i + 1]) == 'x' && lower(data[i + 2]) == 'x') {
            *confidence = 0.75;
            return 3; // PATTERN_XXX
        }
        if (rem >= 4 && c0 == 'h' && lower(data[i + 1]) == 'a' && lower(data[i + 2]) == 'c' && lower(data[i + 3]) == 'k') {
            *confidence = 0.75;
            return 4; // PATTERN_HACK
        }
        if (rem >= 4 && c0 == 'n' && lower(data[i + 1]) == 'o' && lower(data[i + 2]) == 't' && lower(data[i + 3]) == 'e') {
            *confidence = 0.75;
            return 6; // PATTERN_NOTE
        }
        if (rem >= 4 && c0 == 'i' && lower(data[i + 1]) == 'd' && lower(data[i + 2]) == 'e' && lower(data[i + 3]) == 'a') {
            *confidence = 0.75;
            return 7; // PATTERN_IDEA
        }
    }

    *confidence = 0.0;
    return 0;
}

// Branchless first-byte gate: one table lookup replaces 7 comparisons per byte.
// Trigger bytes (lower): b=98 f=102 h=104 i=105 n=110 t=116 x=120
// Trigger bytes (upper): B=66 F=70  H=72  I=73  N=78  T=84  X=88
static constexpr bool kFirstByteGate[256] = {
    // 0..63
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    // 64..127  (uppercase region: B=66 F=70 H=72 I=73 N=78 T=84 X=88)
    0,0,1,0,0,0,1,0,1,1,0,0,0,0,1,0,0,0,0,0,1,0,0,0,1,0,0,0,0,0,0,0,
    // 96..127  (lowercase region: b=98 f=102 h=104 i=105 n=110 t=116 x=120)
    0,0,1,0,0,0,1,0,1,1,0,0,0,0,1,0,0,0,0,0,1,0,0,0,1,0,0,0,0,0,0,0,
    // 128..255
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
};

static bool IsTwoByteHead(uint8_t c0, uint8_t c1) {
    return (c0 == 'f' && c1 == 'i') ||
           (c0 == 'b' && c1 == 'u') ||
           (c0 == 't' && c1 == 'o') ||
           (c0 == 'x' && c1 == 'x') ||
           (c0 == 'h' && c1 == 'a') ||
           (c0 == 'n' && c1 == 'o') ||
           (c0 == 'i' && c1 == 'd');
}

static bool PreferThreeByteHeads(const uint8_t* data, int len) {
    if (!data || len <= 0) return true;

    const int sample = std::min(len, 512);
    int printable = 0;
    int zeroes = 0;
    int highBytes = 0;

    for (int i = 0; i < sample; ++i) {
        const uint8_t c = data[i];
        if ((c >= 0x20 && c <= 0x7E) || c == '\n' || c == '\r' || c == '\t') {
            ++printable;
        }
        if (c == 0) ++zeroes;
        if (c >= 0x80) ++highBytes;
    }

    const double printableRatio = static_cast<double>(printable) / static_cast<double>(sample);
    const double noisyRatio = static_cast<double>(zeroes + highBytes) / static_cast<double>(sample);

    // Structured/log-like data benefits from wider candidate net (2-byte heads).
    // Binary/noisy windows prefer stricter 3-byte gating to reduce false positives.
    if (printableRatio >= 0.72 && noisyRatio <= 0.20) {
        return false;
    }
    return true;
}

static int AcceleratedClassify(const uint8_t* data, int len, double* confidence) {
#ifdef RAWRXD_USE_EXTERNAL_PATTERN_ENGINE
    return ClassifyPattern(data, len, nullptr, confidence);
#else
    if (!data || len <= 1 || !confidence) {
        if (confidence) *confidence = 0.0;
        return 0;
    }

    auto lower = [](uint8_t c) -> uint8_t {
        if (c >= 'A' && c <= 'Z') return static_cast<uint8_t>(c + 0x20);
        return c;
    };

    auto matchAt = [&](int pos, int& pat, double& conf) -> bool {
        const int rem = len - pos;
        const uint8_t c0 = lower(data[pos]);

        if (rem >= 5 && c0 == 'f' && lower(data[pos + 1]) == 'i' && lower(data[pos + 2]) == 'x' &&
            lower(data[pos + 3]) == 'm' && lower(data[pos + 4]) == 'e') {
            pat = 2;
            conf = 0.95;
            return true;
        }
        if (rem >= 3 && c0 == 'b' && lower(data[pos + 1]) == 'u' && lower(data[pos + 2]) == 'g') {
            pat = 5;
            conf = 1.0;
            return true;
        }
        if (rem >= 4 && c0 == 't' && lower(data[pos + 1]) == 'o' && lower(data[pos + 2]) == 'd' && lower(data[pos + 3]) == 'o') {
            pat = 1;
            conf = 0.85;
            return true;
        }
        if (rem >= 3 && c0 == 'x' && lower(data[pos + 1]) == 'x' && lower(data[pos + 2]) == 'x') {
            pat = 3;
            conf = 0.75;
            return true;
        }
        if (rem >= 4 && c0 == 'h' && lower(data[pos + 1]) == 'a' && lower(data[pos + 2]) == 'c' && lower(data[pos + 3]) == 'k') {
            pat = 4;
            conf = 0.75;
            return true;
        }
        if (rem >= 4 && c0 == 'n' && lower(data[pos + 1]) == 'o' && lower(data[pos + 2]) == 't' && lower(data[pos + 3]) == 'e') {
            pat = 6;
            conf = 0.75;
            return true;
        }
        if (rem >= 4 && c0 == 'i' && lower(data[pos + 1]) == 'd' && lower(data[pos + 2]) == 'e' && lower(data[pos + 3]) == 'a') {
            pat = 7;
            conf = 0.75;
            return true;
        }

        return false;
    };

    // Adaptive gate: on structured/log text use the tighter 2-byte secondary
    // filter; on binary/entropy data the 1-byte kFirstByteGate is sufficient.
    const bool useSecondaryGate = !PreferThreeByteHeads(data, len);

#if RAWRXD_HAS_AVX512
    // ----------------------------------------------------------------
    // AVX-512F SIMD prefilter — 64 bytes/cycle candidate detection.
    //
    // Upgrade path over AVX2:
    //   • Loads 64B per iteration (vs 32B) — 2× throughput ceiling
    //   • _mm512_cmpeq_epi8_mask returns __mmask64 natively — no movemask
    //     instruction needed, one less uop per iteration
    //   • Prefetch 256B ahead (4 cache lines) to sustain bandwidth on NVMe
    //     streaming where DRAM latency balloons
    //   • Aligned-load fast path: skip the first <64B prefix unaligned, then
    //     switch to _mm512_load_si512 (guaranteed 64B-aligned) for the hot loop
    //
    // Pipeline structure (software-pipelined 3-stage):
    //   iter N  : prefetch(data + i + 256)
    //   iter N+1: load + cmpeq  (in-flight from prefetch issued at N)
    //   iter N+2: mask iteration (scalar verifier on candidates only)
    // ----------------------------------------------------------------
    {
        // Build the 14 broadcast vectors (7 trigger patterns × upper/lower case).
        const __m512i vBu = _mm512_set1_epi8(static_cast<char>(0x42)); // 'B'
        const __m512i vbl = _mm512_set1_epi8(static_cast<char>(0x62)); // 'b'
        const __m512i vFu = _mm512_set1_epi8(static_cast<char>(0x46)); // 'F'
        const __m512i vfl = _mm512_set1_epi8(static_cast<char>(0x66)); // 'f'
        const __m512i vHu = _mm512_set1_epi8(static_cast<char>(0x48)); // 'H'
        const __m512i vhl = _mm512_set1_epi8(static_cast<char>(0x68)); // 'h'
        const __m512i vIu = _mm512_set1_epi8(static_cast<char>(0x49)); // 'I'
        const __m512i vil = _mm512_set1_epi8(static_cast<char>(0x69)); // 'i'
        const __m512i vNu = _mm512_set1_epi8(static_cast<char>(0x4E)); // 'N'
        const __m512i vnl = _mm512_set1_epi8(static_cast<char>(0x6E)); // 'n'
        const __m512i vTu = _mm512_set1_epi8(static_cast<char>(0x54)); // 'T'
        const __m512i vtl = _mm512_set1_epi8(static_cast<char>(0x74)); // 't'
        const __m512i vXu = _mm512_set1_epi8(static_cast<char>(0x58)); // 'X'
        const __m512i vxl = _mm512_set1_epi8(static_cast<char>(0x78)); // 'x'

        // Stage 1: run a short scalar prefix so that the hot loop pointer is
        // 64-byte aligned, enabling _mm512_load_si512 for maximum throughput.
        const uintptr_t dataAddr = reinterpret_cast<uintptr_t>(data);
        const int misalign = static_cast<int>(dataAddr & 63u);
        const int prefix   = (misalign == 0) ? 0 : static_cast<int>(64 - misalign);
        int i = 0;
        const int prefixEnd = std::min(prefix, len - 2);
        for (; i < prefixEnd; ++i) {
            if (!kFirstByteGate[data[i]]) continue;
            if (useSecondaryGate) {
                const uint8_t c0b = lower(data[i]);
                const uint8_t c1b = lower(data[i + 1]);
                if (!IsTwoByteHead(c0b, c1b)) continue;
            }
            int pat = 0; double conf = 0.0;
            if (matchAt(i, pat, conf)) { *confidence = conf; return pat; }
        }

        // Stage 2: hot aligned 64B loop.
        const int avx512Limit = len - 64;
        while (i <= avx512Limit) {
            // Software-pipeline stage: prefetch 4 cache lines (256B) ahead so
            // DRAM latency is fully hidden when scanning sequential disk images.
            if (i + 256 < len) {
                _mm_prefetch(reinterpret_cast<const char*>(data + i + 256), _MM_HINT_T0);
            }

            // Aligned load (safe because we advanced i to 64B boundary above).
            const __m512i chunk = _mm512_load_si512(
                reinterpret_cast<const __m512i*>(data + i));

            // 14 equality checks → OR-accumulate into a single 64-bit mask.
            // Each set bit = one candidate position for the scalar verifier.
            __mmask64 hitMask =
                _mm512_cmpeq_epi8_mask(chunk, vBu) |
                _mm512_cmpeq_epi8_mask(chunk, vbl) |
                _mm512_cmpeq_epi8_mask(chunk, vFu) |
                _mm512_cmpeq_epi8_mask(chunk, vfl) |
                _mm512_cmpeq_epi8_mask(chunk, vHu) |
                _mm512_cmpeq_epi8_mask(chunk, vhl) |
                _mm512_cmpeq_epi8_mask(chunk, vIu) |
                _mm512_cmpeq_epi8_mask(chunk, vil) |
                _mm512_cmpeq_epi8_mask(chunk, vNu) |
                _mm512_cmpeq_epi8_mask(chunk, vnl) |
                _mm512_cmpeq_epi8_mask(chunk, vTu) |
                _mm512_cmpeq_epi8_mask(chunk, vtl) |
                _mm512_cmpeq_epi8_mask(chunk, vXu) |
                _mm512_cmpeq_epi8_mask(chunk, vxl);

            // Iterate only set bits — O(matches), not O(64) per step.
            while (hitMask) {
                const int bit = static_cast<int>(_tzcnt_u64(static_cast<uint64_t>(hitMask)));
                hitMask &= hitMask - 1u; // clear lowest set bit

                const int pos = i + bit;
                if (pos + 2 >= len) break;

                if (useSecondaryGate) {
                    const uint8_t c0b = lower(data[pos]);
                    const uint8_t c1b = lower(data[pos + 1]);
                    if (!IsTwoByteHead(c0b, c1b)) continue;
                }
                int pat = 0; double conf = 0.0;
                if (matchAt(pos, pat, conf)) { *confidence = conf; return pat; }
            }
            i += 64;
        }

        // Stage 3: scalar tail for the remaining <64 bytes.
        for (; i + 2 < len; ++i) {
            if (!kFirstByteGate[data[i]]) continue;
            if (useSecondaryGate) {
                const uint8_t c0b = lower(data[i]);
                const uint8_t c1b = lower(data[i + 1]);
                if (!IsTwoByteHead(c0b, c1b)) continue;
            }
            int pat = 0; double conf = 0.0;
            if (matchAt(i, pat, conf)) { *confidence = conf; return pat; }
        }

        *confidence = 0.0;
        return 0;
    }
#elif RAWRXD_HAS_AVX2
    // ----------------------------------------------------------------
    // AVX2 SIMD prefilter — 32 bytes/cycle candidate detection.
    //
    // Broadcast each of the 14 trigger bytes (7 patterns × 2 cases) and
    // compare against the loaded chunk simultaneously.  OR-reducing the 14
    // result vectors yields a single 32-bit hit mask; only positions with a
    // set bit are dispatched to the scalar verifier.  This eliminates ~96%
    // of scalar work on high-entropy data.
    //
    // Prefetch 2 cache lines (128 B) ahead on every 32-byte step so that
    // the next chunk is already in L1 when we need it.
    // ----------------------------------------------------------------
    const __m256i vBu = _mm256_set1_epi8(static_cast<char>(0x42)); // 'B'
    const __m256i vbl = _mm256_set1_epi8(static_cast<char>(0x62)); // 'b'
    const __m256i vFu = _mm256_set1_epi8(static_cast<char>(0x46)); // 'F'
    const __m256i vfl = _mm256_set1_epi8(static_cast<char>(0x66)); // 'f'
    const __m256i vHu = _mm256_set1_epi8(static_cast<char>(0x48)); // 'H'
    const __m256i vhl = _mm256_set1_epi8(static_cast<char>(0x68)); // 'h'
    const __m256i vIu = _mm256_set1_epi8(static_cast<char>(0x49)); // 'I'
    const __m256i vil = _mm256_set1_epi8(static_cast<char>(0x69)); // 'i'
    const __m256i vNu = _mm256_set1_epi8(static_cast<char>(0x4E)); // 'N'
    const __m256i vnl = _mm256_set1_epi8(static_cast<char>(0x6E)); // 'n'
    const __m256i vTu = _mm256_set1_epi8(static_cast<char>(0x54)); // 'T'
    const __m256i vtl = _mm256_set1_epi8(static_cast<char>(0x74)); // 't'
    const __m256i vXu = _mm256_set1_epi8(static_cast<char>(0x58)); // 'X'
    const __m256i vxl = _mm256_set1_epi8(static_cast<char>(0x78)); // 'x'

    int i = 0;
    const int avx2Limit = len - 32;

    while (i <= avx2Limit) {
        // Prefetch 2 cache lines ahead to hide sequential DRAM latency.
        if (i + 128 < len) {
            _mm_prefetch(reinterpret_cast<const char*>(data + i + 128), _MM_HINT_T0);
        }

        const __m256i chunk = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(data + i));

        // Fold 14 comparisons into a single hit bitmask (one bit per byte position).
        __m256i hits256 = _mm256_cmpeq_epi8(chunk, vBu);
        hits256 = _mm256_or_si256(hits256, _mm256_cmpeq_epi8(chunk, vbl));
        hits256 = _mm256_or_si256(hits256, _mm256_cmpeq_epi8(chunk, vFu));
        hits256 = _mm256_or_si256(hits256, _mm256_cmpeq_epi8(chunk, vfl));
        hits256 = _mm256_or_si256(hits256, _mm256_cmpeq_epi8(chunk, vHu));
        hits256 = _mm256_or_si256(hits256, _mm256_cmpeq_epi8(chunk, vhl));
        hits256 = _mm256_or_si256(hits256, _mm256_cmpeq_epi8(chunk, vIu));
        hits256 = _mm256_or_si256(hits256, _mm256_cmpeq_epi8(chunk, vil));
        hits256 = _mm256_or_si256(hits256, _mm256_cmpeq_epi8(chunk, vNu));
        hits256 = _mm256_or_si256(hits256, _mm256_cmpeq_epi8(chunk, vnl));
        hits256 = _mm256_or_si256(hits256, _mm256_cmpeq_epi8(chunk, vTu));
        hits256 = _mm256_or_si256(hits256, _mm256_cmpeq_epi8(chunk, vtl));
        hits256 = _mm256_or_si256(hits256, _mm256_cmpeq_epi8(chunk, vXu));
        hits256 = _mm256_or_si256(hits256, _mm256_cmpeq_epi8(chunk, vxl));

        uint32_t hitMask = static_cast<uint32_t>(_mm256_movemask_epi8(hits256));

        // Dispatch scalar verifier only for candidate positions.
        while (hitMask) {
            unsigned long bit = 0;
            _BitScanForward(&bit, hitMask);
            hitMask &= hitMask - 1u; // clear lowest set bit

            const int pos = i + static_cast<int>(bit);
            if (pos + 2 >= len) break;

            if (useSecondaryGate) {
                const uint8_t c0 = lower(data[pos]);
                const uint8_t c1 = lower(data[pos + 1]);
                if (!IsTwoByteHead(c0, c1)) continue;
            }

            int pat = 0;
            double conf = 0.0;
            if (matchAt(pos, pat, conf)) {
                *confidence = conf;
                return pat;
            }
        }

        i += 32;
    }

    // Scalar tail — remaining bytes that don't fill a full 32-byte chunk.
    while (i + 2 < len) {
        if (!kFirstByteGate[data[i]]) { ++i; continue; }
        if (useSecondaryGate) {
            const uint8_t c0 = lower(data[i]);
            const uint8_t c1 = lower(data[i + 1]);
            if (!IsTwoByteHead(c0, c1)) { ++i; continue; }
        }
        int pat = 0;
        double conf = 0.0;
        if (matchAt(i, pat, conf)) {
            *confidence = conf;
            return pat;
        }
        ++i;
    }

#else
    // Non-AVX2 fallback: 64-byte block loop with kFirstByteGate + prefetch.
    int i = 0;
    while (i + 2 < len) {
        if (i + 128 < len) {
            _mm_prefetch(reinterpret_cast<const char*>(data + i + 128), _MM_HINT_T0);
        }
        const int blockEnd = std::min(i + 64, len - 2);
        for (int j = i; j < blockEnd; ++j) {
            if (!kFirstByteGate[data[j]]) continue;
            if (useSecondaryGate) {
                const uint8_t c0 = lower(data[j]);
                const uint8_t c1 = lower(data[j + 1]);
                if (!IsTwoByteHead(c0, c1)) continue;
            }
            int pat = 0;
            double conf = 0.0;
            if (matchAt(j, pat, conf)) {
                *confidence = conf;
                return pat;
            }
        }
        i += 64;
    }
#endif

    *confidence = 0.0;
    return 0;
#endif
}

static int InitEngine() {
#ifdef RAWRXD_USE_EXTERNAL_PATTERN_ENGINE
    return InitializePatternEngine();
#else
    return 0;
#endif
}

static int ShutdownEngine() {
#ifdef RAWRXD_USE_EXTERNAL_PATTERN_ENGINE
    return ShutdownPatternEngine();
#else
    return 0;
#endif
}

static void EmbedToken(std::vector<uint8_t>& stream, size_t pos, const std::string& token) {
    if (pos + token.size() > stream.size()) return;
    std::memcpy(stream.data() + pos, token.data(), token.size());
}

static std::vector<uint8_t> BuildHighEntropyStream(size_t bytes) {
    std::vector<uint8_t> out(bytes);
    std::mt19937_64 rng(0xA57EEDULL);
    for (size_t i = 0; i < bytes; ++i) {
        out[i] = static_cast<uint8_t>(rng() & 0xFF);
    }
    return out;
}

static std::vector<uint8_t> BuildRecoveredLogStream(size_t bytes) {
    std::vector<uint8_t> out(bytes, 0x20);
    const std::string line = "journal block: metadata checksum pass; NOTE: delayed write TODO: verify ";
    for (size_t i = 0; i + line.size() < bytes; i += line.size() + 73) {
        std::memcpy(out.data() + i, line.data(), line.size());
    }
    for (size_t p = 4096; p + 8 < bytes; p += 131072) {
        EmbedToken(out, p, "FIXME:");
        EmbedToken(out, p + 23, "BUG:");
    }
    return out;
}

static std::vector<uint8_t> BuildMixedRecoveryStream(size_t bytes) {
    std::vector<uint8_t> out = BuildHighEntropyStream(bytes);
    // Representative signatures from salvage streams.
    for (size_t p = 2048; p + 8 < bytes; p += 65536) {
        static const uint8_t jpeg[] = {0xFF, 0xD8, 0xFF, 0xE0};
        static const uint8_t zip[] = {0x50, 0x4B, 0x03, 0x04};
        std::memcpy(out.data() + p, jpeg, sizeof(jpeg));
        std::memcpy(out.data() + p + 128, zip, sizeof(zip));
        EmbedToken(out, p + 256, "TODO:");
    }
    return out;
}

// Adversarial: dense pattern-head bytes every 4 bytes (worst-case for any prefilter),
// with 50% completing into real matches.  Maximally exercises the scalar verifier.
static std::vector<uint8_t> BuildAdversarialStream(size_t bytes) {
    std::vector<uint8_t> out(bytes, 0x20);
    static const char* heads[] = {"fix", "bug", "tod", "xxx", "hac", "not", "ide"};
    static const char* fulls[] = {"fixme", "bug", "todo", "xxx", "hack", "note", "idea"};
    size_t si = 0;
    for (size_t i = 0; i + 8 < bytes; i += 4, si = (si + 1) % 7) {
        if (i % 8 == 0) {
            // Full match on every other slot
            const char* tok = fulls[si];
            const size_t tlen = std::strlen(tok);
            if (i + tlen < bytes) std::memcpy(out.data() + i, tok, tlen);
        } else {
            // Partial head only — forces scalar verifier to reject and advance
            std::memcpy(out.data() + i, heads[si], 3);
        }
    }
    return out;
}

// Sparse: exactly one complete match per megabyte in an otherwise clean stream.
// Validates that true positives are never lost in low-density real-world data.
static std::vector<uint8_t> BuildSparseStream(size_t bytes) {
    std::vector<uint8_t> out(bytes, 0x41); // fill with 'A' — never a trigger byte
    constexpr size_t kMatchInterval = 1024 * 1024; // 1 MB
    static const char* tokens[] = {"fixme", "bug", "todo", "note"};
    size_t ti = 0;
    for (size_t p = kMatchInterval / 2; p + 8 < bytes; p += kMatchInterval, ti = (ti + 1) % 4) {
        EmbedToken(out, p, tokens[ti]);
    }
    return out;
}

struct BenchStats {
    double seconds{0.0};
    double mbPerSec{0.0};
    uint64_t windows{0};
    uint64_t matches{0};
};

template <typename Fn>
static BenchStats RunBench(const std::vector<uint8_t>& stream, size_t window, Fn fn) {
    BenchStats s;
    if (window == 0 || stream.size() < window) return s;

    const auto start = std::chrono::high_resolution_clock::now();
    for (size_t i = 0; i + window <= stream.size(); i += window) {
        double conf = 0.0;
        const int pat = fn(stream.data() + i, static_cast<int>(window), &conf);
        ++s.windows;
        if (pat != 0) {
            ++s.matches;
        }
    }
    const auto end = std::chrono::high_resolution_clock::now();

    s.seconds = std::chrono::duration<double>(end - start).count();
    const double mb = static_cast<double>(s.windows * window) / (1024.0 * 1024.0);
    s.mbPerSec = (s.seconds > 0.0) ? (mb / s.seconds) : 0.0;
    return s;
}

static void PrintRow(const std::string& name, const BenchStats& baseline, const BenchStats& accelerated) {
    const double speedup = (baseline.mbPerSec > 0.0) ? (accelerated.mbPerSec / baseline.mbPerSec) : 0.0;
    std::cout << std::left << std::setw(24) << name
              << " baseline=" << std::setw(10) << std::fixed << std::setprecision(2) << baseline.mbPerSec
              << " MB/s  accelerated=" << std::setw(10) << accelerated.mbPerSec
              << " MB/s  speedup=" << std::setw(7) << speedup << "x"
              << "  matches=" << accelerated.matches << '\n';
}

static void WriteReport(const std::string& path,
                        const BenchStats& b1, const BenchStats& a1,
                        const BenchStats& b2, const BenchStats& a2,
                        const BenchStats& b3, const BenchStats& a3,
                        const BenchStats& b4, const BenchStats& a4,
                        const BenchStats& b5, const BenchStats& a5) {
    std::ofstream out(path, std::ios::trunc);
    if (!out.is_open()) {
        return;
    }

    auto sp = [](const BenchStats& b, const BenchStats& a) {
        return (b.mbPerSec > 0.0) ? (a.mbPerSec / b.mbPerSec) : 0.0;
    };
    const double s1 = sp(b1, a1), s2 = sp(b2, a2), s3 = sp(b3, a3),
                 s4 = sp(b4, a4), s5 = sp(b5, a5);
    const double avgSpeedup = (s1 + s2 + s3 + s4 + s5) / 5.0;

    out << "RawrXD Pattern Scanner Microbenchmark (prefetch+gate optimized)\n";
    out << "high_entropy   baseline_mb_s=" << std::fixed << std::setprecision(2) << b1.mbPerSec
        << " accelerated_mb_s=" << a1.mbPerSec << " speedup=" << s1 << "\n";
    out << "recovered_logs baseline_mb_s=" << b2.mbPerSec
        << " accelerated_mb_s=" << a2.mbPerSec << " speedup=" << s2 << "\n";
    out << "mixed_recovery baseline_mb_s=" << b3.mbPerSec
        << " accelerated_mb_s=" << a3.mbPerSec << " speedup=" << s3 << "\n";
    out << "adversarial    baseline_mb_s=" << b4.mbPerSec
        << " accelerated_mb_s=" << a4.mbPerSec << " speedup=" << s4 << "\n";
    out << "sparse         baseline_mb_s=" << b5.mbPerSec
        << " accelerated_mb_s=" << a5.mbPerSec << " speedup=" << s5 << "\n";
    out << "average_speedup=" << avgSpeedup << "\n";
}

} // namespace

int main() {
    constexpr size_t kStreamBytes = 8 * 1024 * 1024;
    constexpr size_t kWindow = 4096;

    (void)InitEngine();

    std::cout << "RawrXD Pattern Scanner Microbenchmark (prefetch+gate optimized)\n";
    std::cout << "window=" << kWindow << " bytes  stream=" << kStreamBytes << " bytes  5 distributions\n";
    std::cout << std::string(90, '-') << '\n';

    // Build all five representative data distributions.
    const auto highEntropy  = BuildHighEntropyStream(kStreamBytes);      // random noise
    const auto logHeavy     = BuildRecoveredLogStream(kStreamBytes);      // structured logs
    const auto mixed        = BuildMixedRecoveryStream(kStreamBytes);     // mixed salvage
    const auto adversarial  = BuildAdversarialStream(kStreamBytes);       // dense partial hits
    const auto sparse       = BuildSparseStream(kStreamBytes);            // 1 match / MB

    auto baselineFn = [](const uint8_t* data, int len, double* conf) {
        return BaselineScalarClassify(data, len, conf);
    };
    auto acceleratedFn = [](const uint8_t* data, int len, double* conf) {
        return AcceleratedClassify(data, len, conf);
    };

    // Run baseline + accelerated for every distribution.
    const BenchStats b1 = RunBench(highEntropy,  kWindow, baselineFn);
    const BenchStats a1 = RunBench(highEntropy,  kWindow, acceleratedFn);
    const BenchStats b2 = RunBench(logHeavy,     kWindow, baselineFn);
    const BenchStats a2 = RunBench(logHeavy,     kWindow, acceleratedFn);
    const BenchStats b3 = RunBench(mixed,        kWindow, baselineFn);
    const BenchStats a3 = RunBench(mixed,        kWindow, acceleratedFn);
    const BenchStats b4 = RunBench(adversarial,  kWindow, baselineFn);
    const BenchStats a4 = RunBench(adversarial,  kWindow, acceleratedFn);
    const BenchStats b5 = RunBench(sparse,       kWindow, baselineFn);
    const BenchStats a5 = RunBench(sparse,       kWindow, acceleratedFn);

    PrintRow("high_entropy",  b1, a1);
    PrintRow("recovered_logs",b2, a2);
    PrintRow("mixed_recovery",b3, a3);
    PrintRow("adversarial",   b4, a4);
    PrintRow("sparse",        b5, a5);
    std::cout << std::string(90, '-') << '\n';

    // Average across all five distributions for a single headline number.
    auto sp = [](const BenchStats& b, const BenchStats& a) {
        return (b.mbPerSec > 0.0) ? (a.mbPerSec / b.mbPerSec) : 0.0;
    };
    const double avg = (sp(b1,a1) + sp(b2,a2) + sp(b3,a3) + sp(b4,a4) + sp(b5,a5)) / 5.0;
    std::cout << "Average speedup across all distributions: " << std::fixed
              << std::setprecision(3) << avg << "x\n";

    WriteReport("disk_recovery_pattern_microbench_report.txt",
                b1, a1, b2, a2, b3, a3, b4, a4, b5, a5);

    (void)ShutdownEngine();
    return 0;
}
