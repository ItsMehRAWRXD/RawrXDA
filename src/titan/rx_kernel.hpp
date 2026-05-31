// =============================================================================
// rx_kernel.hpp — Single-Header Extension Kernel
//
// Collapses IPC + speculative decode + AST grounding + arbitration into one
// hardware-aligned shared-memory contract.
// =============================================================================
#pragma once

#include <windows.h>
#include <immintrin.h>

#include <atomic>
#include <cstdint>
#include <cstring>

namespace titan {

// -----------------------------------------------------------------------------
// RxChannel — Zero-copy SPSC memory contract between IDE producer and kernel.
// head increments per producer write; gate transitions to 1 when draft tokens are
// committed by the kernel.
// -----------------------------------------------------------------------------
struct alignas(64) RxChannel {
    std::atomic<uint64_t> head;      // Sequence ID
    char context_buf[2048];          // Input context bytes
    uint32_t draft_tokens[8];        // Speculative output IDs
    float confidence[8];             // Arbitration confidences [0..1]
    uint32_t ast_constraints[256];   // 8192-token legality bitmask (LSP-written)
    std::atomic<uint32_t> gate;      // 0 = pending, 1 = committed
};
static_assert(sizeof(RxChannel) <= 64 * 1024, "RxChannel contract must stay <=64KB");

// -----------------------------------------------------------------------------
// AST grounding state
// -----------------------------------------------------------------------------
enum class Syntax : uint32_t {
    GLOBAL = 0,
    SCOPE_BLOCK = 1,
    MEMBER_ACCESS = 2
};

struct ASTState {
    Syntax current_scope = Syntax::GLOBAL;

    // Lightweight scope scanner for low-latency hot path.
    void scan_scope(const char* context) noexcept {
        if (!context) {
            current_scope = Syntax::GLOBAL;
            return;
        }

        // Fast-path: leading '.' usually implies member completion.
        if (context[0] == '.') {
            current_scope = Syntax::MEMBER_ACCESS;
            return;
        }

        int depth = 0;
        bool saw_dot = false;
        for (const char* p = context; *p != '\0'; ++p) {
            if (*p == '{') {
                ++depth;
            } else if (*p == '}') {
                if (depth > 0) {
                    --depth;
                }
            } else if (*p == '.') {
                saw_dot = true;
            }
        }

        if (saw_dot) {
            current_scope = Syntax::MEMBER_ACCESS;
        } else if (depth > 0) {
            current_scope = Syntax::SCOPE_BLOCK;
        } else {
            current_scope = Syntax::GLOBAL;
        }
    }

    // Minimal legality check by token class heuristic.
    // token[15:12] is treated as a coarse class nibble in this fast path.
    bool is_valid_tail(uint32_t token) const noexcept {
        const uint32_t token_class = (token >> 12) & 0xFu;
        switch (current_scope) {
            case Syntax::GLOBAL:
                // Reject member-only and block-tail classes globally.
                return token_class != 0xDu;
            case Syntax::SCOPE_BLOCK:
                return token_class != 0xEu;
            case Syntax::MEMBER_ACCESS:
                // Prefer identifiers/member-like classes.
                return token_class == 0x1u || token_class == 0x2u || token_class == 0x3u;
            default:
                return true;
        }
    }
};

// -----------------------------------------------------------------------------
// ExtensionKernel — memory-mapped speculative loop
// -----------------------------------------------------------------------------
class ExtensionKernel {
public:
    explicit ExtensionKernel(const char* map_name)
        : channel_(nullptr), h_map_(nullptr), stop_(false)
    {
        if (!map_name) {
            return;
        }

        h_map_ = CreateFileMappingA(
            INVALID_HANDLE_VALUE,
            nullptr,
            PAGE_READWRITE,
            0,
            static_cast<DWORD>(sizeof(RxChannel)),
            map_name);

        const DWORD map_error = h_map_ ? GetLastError() : ERROR_INVALID_HANDLE;
        if (!h_map_) {
            return;
        }

        channel_ = static_cast<RxChannel*>(
            MapViewOfFile(h_map_, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(RxChannel)));

        if (channel_ && map_error != ERROR_ALREADY_EXISTS) {
            channel_->head.store(0, std::memory_order_relaxed);
            std::memset(channel_->context_buf, 0, sizeof(channel_->context_buf));
            std::memset(channel_->draft_tokens, 0, sizeof(channel_->draft_tokens));
            std::memset(channel_->confidence, 0, sizeof(channel_->confidence));
            for (uint32_t& word : channel_->ast_constraints) {
                word = 0xFFFFFFFFu;
            }
            channel_->gate.store(0, std::memory_order_relaxed);
        }
    }

    ~ExtensionKernel() {
        if (channel_) {
            UnmapViewOfFile(channel_);
            channel_ = nullptr;
        }
        if (h_map_) {
            CloseHandle(h_map_);
            h_map_ = nullptr;
        }
    }

    ExtensionKernel(const ExtensionKernel&) = delete;
    ExtensionKernel& operator=(const ExtensionKernel&) = delete;

    bool is_ready() const noexcept { return channel_ != nullptr; }
    RxChannel* channel() noexcept { return channel_; }
    const RxChannel* channel() const noexcept { return channel_; }

    bool submit_context(const char* context) noexcept {
        if (!channel_ || !context) {
            return false;
        }
        std::strncpy(channel_->context_buf, context, sizeof(channel_->context_buf) - 1);
        channel_->context_buf[sizeof(channel_->context_buf) - 1] = '\0';
        channel_->gate.store(0, std::memory_order_release);
        channel_->head.fetch_add(1, std::memory_order_release);
        return true;
    }

    uint32_t step_once() noexcept {
        if (!channel_) {
            return 0;
        }

        const uint64_t current = channel_->head.load(std::memory_order_acquire);
        if (current == last_seen_head_) {
            return 0;
        }

        ast_.scan_scope(channel_->context_buf);

        uint32_t raw_predictions[8] = {};
        float raw_confidence[8] = {};
        infer_batch(channel_->context_buf, raw_predictions, raw_confidence);

        uint32_t committed = 0;
        for (uint32_t i = 0; i < 8; ++i) {
            if (is_allowed_by_constraints(raw_predictions[i], channel_->ast_constraints) &&
                ast_.is_valid_tail(raw_predictions[i])) {
                channel_->draft_tokens[i] = raw_predictions[i];
                channel_->confidence[i] = raw_confidence[i];
                ++committed;
            } else {
                channel_->draft_tokens[i] = 0;
                channel_->confidence[i] = 0.0f;
            }
        }

        channel_->gate.store(1u, std::memory_order_release);
        last_seen_head_ = current;
        return committed;
    }

    void run() noexcept {
        while (!stop_.load(std::memory_order_relaxed)) {
            if (channel_ && channel_->head.load(std::memory_order_acquire) != last_seen_head_) {
                (void)step_once();
            }
            _mm_pause();
        }
    }

    void stop() noexcept {
        stop_.store(true, std::memory_order_relaxed);
    }

private:
    void infer_batch(const char* context, uint32_t* results, float* conf) noexcept {
        // Lightweight deterministic draft generator. In production this call site
        // can be replaced by a real SIMD Markov/transformer backend without changing
        // the shared-memory contract.
        uint64_t h = 1469598103934665603ULL; // FNV-1a offset
        const unsigned char* p = reinterpret_cast<const unsigned char*>(context);
        while (*p) {
            h ^= static_cast<uint64_t>(*p++);
            h *= 1099511628211ULL;
        }

        alignas(32) uint32_t lanes[8];
        for (int i = 0; i < 8; ++i) {
            h = (h ^ static_cast<uint64_t>(i + 1)) * 0xBF58476D1CE4E5B9ULL;
            lanes[i] = static_cast<uint32_t>(h & 0x1FFFu);
        }

        // Keep an explicit AVX2 pass so arbitration is inlined into SIMD hot path.
        __m256i v = _mm256_load_si256(reinterpret_cast<const __m256i*>(lanes));
        __m256i class_bias = _mm256_set1_epi32(
            ast_.current_scope == Syntax::MEMBER_ACCESS ? 0x100u : 0x040u);
        v = _mm256_add_epi32(v, class_bias);
        _mm256_store_si256(reinterpret_cast<__m256i*>(lanes), v);

        for (int i = 0; i < 8; ++i) {
            results[i] = lanes[i] & 0x1FFFu;
            conf[i] = static_cast<float>((lanes[i] & 0x3FFu) / 1023.0f);
        }
    }

    static bool is_allowed_by_constraints(uint32_t token, const uint32_t* constraints) noexcept {
        if (!constraints) {
            return true;
        }
        const uint32_t word = token >> 5;
        if (word >= 256u) {
            return false;
        }
        const uint32_t bit = 1u << (token & 31u);
        return (constraints[word] & bit) != 0u;
    }

    RxChannel* channel_;
    HANDLE h_map_;
    ASTState ast_{};
    uint64_t last_seen_head_ = 0;
    std::atomic<bool> stop_;
};

} // namespace titan
