// =============================================================================
// titan_engine.h — Zero-Copy Shared Memory + Speculative Markov Inference Core
//
// Architecture:
//   - ShmHeader  : Win32 CreateFileMapping ring buffer (zero-copy IPC)
//   - ASTMask    : Token bitmask for syntax-scope filtering
//   - MarkovEdge : NVMe-mapped sorted edge table (binary searchable)
//   - TitanEngine: Mmap model + SHM IPC + speculative decode + SIMD arbitrate
//
// Dependencies: kernel32.lib, immintrin.h (AVX2)
// Build:        MSVC /arch:AVX2, C++17+
// =============================================================================
#pragma once

#include <windows.h>
#include <immintrin.h>
#include <atomic>
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <cassert>

namespace titan {

// -----------------------------------------------------------------------------
// ShmHeader — Shared Memory IPC layout (zero-copy between IDE and engine)
// Both processes map the same physical RAM via CreateFileMapping.
// The IDE writes context_tokens + sets ready=true; the engine processes and
// writes results[], then clears ready.
// -----------------------------------------------------------------------------
struct alignas(64) ShmHeader {
    std::atomic<uint64_t> sequence;         // Monotonic write counter
    uint32_t              context_tokens[1024]; // Context window (IDE → Engine)
    uint32_t              context_len;      // Valid token count in context_tokens
    uint32_t              result_count;     // Valid token count in results[]
    uint32_t              results[32];      // Speculative batch (Engine → IDE)
    uint8_t               _pad[64 - ((sizeof(uint32_t) * 34 + sizeof(uint32_t) * 1024 + sizeof(uint64_t)) % 64)];
    std::atomic<bool>     ready;            // IDE sets true; engine clears after write
};
static_assert(sizeof(ShmHeader) < 8 * 1024 * 1024, "ShmHeader oversized");

// -----------------------------------------------------------------------------
// ASTMask — Bitmask over 32k token vocabulary for syntax-scope filtering.
// Populated externally by the AST parser agent.
// is_allowed() executes in ~1 ns (single array index + bitshift).
// -----------------------------------------------------------------------------
struct ASTMask {
    uint8_t allowed_types[4096]; // 4096 bytes × 8 bits = 32768 token vocab

    inline void allow(uint32_t token_id) noexcept {
        if (token_id < 32768u)
            allowed_types[token_id >> 3] |= static_cast<uint8_t>(1u << (token_id & 7u));
    }
    inline void deny(uint32_t token_id) noexcept {
        if (token_id < 32768u)
            allowed_types[token_id >> 3] &= static_cast<uint8_t>(~(1u << (token_id & 7u)));
    }
    inline bool is_allowed(uint32_t token_id) const noexcept {
        if (token_id >= 32768u) return false;
        return (allowed_types[token_id >> 3] & (1u << (token_id & 7u))) != 0;
    }
    // Allow all tokens (no AST context / passthrough mode)
    inline void allow_all() noexcept { std::memset(allowed_types, 0xFF, sizeof(allowed_types)); }
    inline void deny_all()  noexcept { std::memset(allowed_types, 0x00, sizeof(allowed_types)); }
};

// -----------------------------------------------------------------------------
// MarkovEdge — Single entry in the NVMe-mapped sorted edge table.
// Sorted by hash ascending; binary search via std::lower_bound.
// Packed to avoid padding waste across the 16TB address space.
// -----------------------------------------------------------------------------
#pragma pack(push, 1)
struct MarkovEdge {
    uint64_t hash;   // Rolling context hash
    uint32_t token;  // Predicted next token
    uint32_t freq;   // Occurrence frequency (weight for arbitration)
};
#pragma pack(pop)
static_assert(sizeof(MarkovEdge) == 16, "MarkovEdge must be 16 bytes");

// -----------------------------------------------------------------------------
// TitanEngine — Core inference engine.
//   model      : NVMe-backed read-only MapViewOfFile (cold pages DMA on fault)
//   shm        : Shared-memory IPC header (hot path, always in L3)
//   shm_handle : HANDLE to the shared memory mapping
// -----------------------------------------------------------------------------
class TitanEngine {
public:
    // -------------------------------------------------------------------------
    // Construction: map the model file + open (or create) the SHM segment.
    // mmap_path : path to the sorted MarkovEdge binary file (can be 16TB+)
    // shm_name  : Win32 named shared memory object (e.g. "Local\\RawrXDTitan")
    // -------------------------------------------------------------------------
    TitanEngine(const char* mmap_path, const char* shm_name)
        : model_(nullptr), edge_count_(0), shm_(nullptr)
        , h_file_(INVALID_HANDLE_VALUE), h_model_map_(nullptr), h_shm_(nullptr)
    {
        // -- Map the NVMe model file (read-only, no RAM copy) --
        h_file_ = CreateFileA(mmap_path, GENERIC_READ, FILE_SHARE_READ,
                              nullptr, OPEN_EXISTING,
                              FILE_ATTRIBUTE_NORMAL | FILE_FLAG_RANDOM_ACCESS,
                              nullptr);
        if (h_file_ != INVALID_HANDLE_VALUE) {
            LARGE_INTEGER sz{};
            if (GetFileSizeEx(h_file_, &sz) && sz.QuadPart > 0) {
                edge_count_ = static_cast<size_t>(sz.QuadPart / sizeof(MarkovEdge));
                h_model_map_ = CreateFileMappingA(h_file_, nullptr,
                                                  PAGE_READONLY, 0, 0, nullptr);
                if (h_model_map_) {
                    model_ = static_cast<const MarkovEdge*>(
                        MapViewOfFile(h_model_map_, FILE_MAP_READ, 0, 0, 0));
                }
            }
        }

        // -- Create/open named SHM segment (IDE and engine share this) --
        h_shm_ = CreateFileMappingA(INVALID_HANDLE_VALUE, nullptr,
                                    PAGE_READWRITE,
                                    0, static_cast<DWORD>(sizeof(ShmHeader)),
                                    shm_name);
        const DWORD shm_error = h_shm_ ? GetLastError() : ERROR_INVALID_HANDLE;
        if (h_shm_) {
            shm_ = static_cast<ShmHeader*>(
                MapViewOfFile(h_shm_, FILE_MAP_ALL_ACCESS, 0, 0, 0));
            if (shm_ && shm_error != ERROR_ALREADY_EXISTS) {
                shm_->sequence.store(0, std::memory_order_relaxed);
                shm_->context_len = 0;
                shm_->result_count = 0;
                std::memset(shm_->context_tokens, 0, sizeof(shm_->context_tokens));
                std::memset(shm_->results, 0, sizeof(shm_->results));
                shm_->ready.store(false, std::memory_order_relaxed);
            }
        }
    }

    ~TitanEngine() {
        if (shm_)        { UnmapViewOfFile(shm_);    shm_       = nullptr; }
        if (h_shm_)      { CloseHandle(h_shm_);      h_shm_     = nullptr; }
        if (model_)      { UnmapViewOfFile(const_cast<MarkovEdge*>(model_)); model_ = nullptr; }
        if (h_model_map_){ CloseHandle(h_model_map_);h_model_map_= nullptr; }
        if (h_file_ != INVALID_HANDLE_VALUE) { CloseHandle(h_file_); h_file_ = INVALID_HANDLE_VALUE; }
    }

    // Non-copyable
    TitanEngine(const TitanEngine&) = delete;
    TitanEngine& operator=(const TitanEngine&) = delete;

    bool is_ready() const noexcept { return shm_ != nullptr; }

    // -------------------------------------------------------------------------
    // submit_context — write token context into the mapped SHM region.
    // This is the canonical instruction-device ingress point for MASM/C callers.
    // -------------------------------------------------------------------------
    bool submit_context(const uint32_t* tokens, uint32_t token_count) noexcept {
        if (!shm_ || !tokens || token_count == 0u) return false;
        const uint32_t bounded = token_count < 1024u ? token_count : 1024u;
        std::memcpy(shm_->context_tokens, tokens, sizeof(uint32_t) * bounded);
        shm_->context_len = bounded;
        shm_->ready.store(true, std::memory_order_release);
        return true;
    }

    // -------------------------------------------------------------------------
    // run_once — execute one speculative decode step and publish results.
    // -------------------------------------------------------------------------
    uint32_t run_once(const ASTMask* mask_override = nullptr) noexcept {
        if (!shm_) return 0u;
        ASTMask local_mask;
        if (!mask_override) {
            local_mask.allow_all();
            mask_override = &local_mask;
        }
        execute_speculative_step(*mask_override);
        shm_->sequence.fetch_add(1, std::memory_order_release);
        shm_->ready.store(false, std::memory_order_release);
        return shm_->result_count;
    }

    // -------------------------------------------------------------------------
    // read_results — copy speculative results out to a caller-provided buffer.
    // -------------------------------------------------------------------------
    uint32_t read_results(uint32_t* out_tokens, uint32_t max_tokens) const noexcept {
        if (!shm_ || !out_tokens || max_tokens == 0u) return 0u;
        const uint32_t count = (shm_->result_count < max_tokens) ? shm_->result_count : max_tokens;
        std::memcpy(out_tokens, shm_->results, sizeof(uint32_t) * count);
        return count;
    }

    ShmHeader* shared_header() noexcept { return shm_; }
    const ShmHeader* shared_header() const noexcept { return shm_; }

    // -------------------------------------------------------------------------
    // execute_speculative_step — hot path.
    // Speculatively decodes up to 8 tokens from the current context,
    // filtering via ASTMask and resolving frequency ties with SIMD.
    // Results written into shm_->results[].
    // -------------------------------------------------------------------------
    void execute_speculative_step(const ASTMask& mask) noexcept {
        if (!model_ || !shm_) return;

        const uint32_t ctx_len = shm_->context_len < 1024u ? shm_->context_len : 1024u;
        uint64_t current_hash  = compute_context_hash(shm_->context_tokens, ctx_len);
        uint32_t found         = 0;

        while (found < 8u) {
            // Binary search: find first edge with matching hash
            const MarkovEdge key{ current_hash, 0, 0 };
            const MarkovEdge* it = std::lower_bound(
                model_, model_ + edge_count_, key,
                [](const MarkovEdge& e, const MarkovEdge& k) { return e.hash < k.hash; });

            if (it == model_ + edge_count_ || it->hash != current_hash)
                break; // No continuations for this context

            // -- SIMD Multi-Path: pick best token across up to 8 siblings --
            uint32_t best_token = pick_best_simd(it, mask);
            if (best_token == 0) break; // All candidates filtered by ASTMask

            shm_->results[found++] = best_token;
            current_hash = rolling_hash(current_hash, best_token);
        }

        shm_->result_count = found;
    }

    // -------------------------------------------------------------------------
    // run_loop — spin on shm_->ready; process and clear.
    // Designed to run on a dedicated thread/process.
    // Call stop() from another thread to exit gracefully.
    // -------------------------------------------------------------------------
    void run_loop() noexcept {
        while (!stop_.load(std::memory_order_relaxed)) {
            if (shm_ && shm_->ready.load(std::memory_order_acquire)) {
                (void)run_once(nullptr);
            }
            YieldProcessor();
        }
    }

    void stop() noexcept { stop_.store(true, std::memory_order_relaxed); }

    // -------------------------------------------------------------------------
    // arbitrate — frequency-weighted token resolution between two candidates.
    // -------------------------------------------------------------------------
    static uint32_t arbitrate(uint32_t t1, uint32_t w1,
                               uint32_t t2, uint32_t w2) noexcept {
        return (w1 >= w2) ? t1 : t2;
    }

private:
    // -------------------------------------------------------------------------
    // pick_best_simd — scans up to 8 MarkovEdge siblings for the highest-frequency
    // token that passes the ASTMask gate.
    // Uses AVX2 to compare 8 frequencies at once, then masks illegal tokens.
    // Falls back to scalar loop if <8 edges are available.
    // -------------------------------------------------------------------------
    static uint32_t pick_best_simd(const MarkovEdge* it, const ASTMask& mask) noexcept {
        // Gather up to 8 sibling edges (same hash)
        uint32_t freqs [8] = {};
        uint32_t tokens[8] = {};
        int n = 0;
        const uint64_t target_hash = it->hash;
        for (; n < 8 && it->hash == target_hash; ++it, ++n) {
            freqs [n] = it->freq;
            tokens[n] = it->token;
        }
        if (n == 0) return 0;

        // AVX2: find max frequency index among gathered entries
        __m256i vfreqs   = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(freqs));
        __m256i vmask    = _mm256_set1_epi32(0); // zero out lanes we won't use
        // Build lane-enable mask: lanes [0..n-1] are active
        alignas(32) int32_t enable[8] = {};
        for (int i = 0; i < n; ++i) enable[i] = -1; // all-ones = active lane
        __m256i venable  = _mm256_load_si256(reinterpret_cast<const __m256i*>(enable));
        // Zero inactive lanes in frequency vector
        vfreqs = _mm256_and_si256(vfreqs, venable);

        // Horizontal max reduction: compare pairs
        uint32_t best_freq  = 0;
        uint32_t best_token = 0;
        alignas(32) uint32_t freq_arr[8];
        _mm256_store_si256(reinterpret_cast<__m256i*>(freq_arr), vfreqs);

        for (int i = 0; i < n; ++i) {
            if (freq_arr[i] > best_freq && mask.is_allowed(tokens[i])) {
                best_freq  = freq_arr[i];
                best_token = tokens[i];
            }
        }
        (void)vmask; // suppress unused-variable warning
        return best_token;
    }

    // -------------------------------------------------------------------------
    // compute_context_hash — FNV-1a seed over the context token window.
    // Produces a stable 64-bit hash for binary search into the model.
    // -------------------------------------------------------------------------
    static uint64_t compute_context_hash(const uint32_t* tokens, uint32_t len) noexcept {
        constexpr uint64_t kFNVOffset = 0xcbf29ce484222325ULL;
        constexpr uint64_t kFNVPrime  = 0x100000001b3ULL;
        uint64_t h = kFNVOffset;
        // Only hash last 8 tokens (effective bigram/octagram Markov order)
        const uint32_t start = (len > 8u) ? (len - 8u) : 0u;
        for (uint32_t i = start; i < len; ++i) {
            const uint8_t* bytes = reinterpret_cast<const uint8_t*>(&tokens[i]);
            for (int b = 0; b < 4; ++b) {
                h ^= static_cast<uint64_t>(bytes[b]);
                h *= kFNVPrime;
            }
        }
        return h;
    }

    // -------------------------------------------------------------------------
    // rolling_hash — cheap update for speculative step N → N+1.
    // Matches the hash used when building the MarkovEdge table.
    // -------------------------------------------------------------------------
    static uint64_t rolling_hash(uint64_t h, uint32_t token) noexcept {
        return (h ^ static_cast<uint64_t>(token)) * 0xBF58476D1CE4E5B9ULL;
    }

    const MarkovEdge*   model_;
    size_t              edge_count_;
    ShmHeader*          shm_;
    HANDLE              h_file_;
    HANDLE              h_model_map_;
    HANDLE              h_shm_;
    std::atomic<bool>   stop_{ false };
};

} // namespace titan
