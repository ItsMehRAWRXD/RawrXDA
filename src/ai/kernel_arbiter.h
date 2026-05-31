// kernel_arbiter.h - Runtime kernel selection for latency-aware inference
// Selects optimal quantization kernel (Q4_K, Q5_K, Q6_K) based on:
//   - Task type (autocomplete, inline-edit, full-generation)
//   - Latency budget (time-to-first-token constraints)
//   - Confidence thresholds (early-exit heuristics)
// Part of the Copilot-like inference pipeline.

#pragma once

#include <cstdint>
#include <chrono>
#include <functional>

namespace RawrXD {

// Kernel performance tiers (from benchmark data on RX 7800 XT)
struct KernelPerfProfile {
    float gflops_4096;      // GFLOPs at 4096x4096
    float gflops_11008;     // GFLOPs at 11008x4096
    float latency_factor;   // Relative latency (1.0 = baseline Q4_K)
    const char* spv_path;   // SPIR-V binary path
};

// Task types for kernel selection
enum class TaskType : uint8_t {
    AUTOCOMPLETE = 0,       // Real-time typing, <100ms budget
    INLINE_EDIT = 1,        // Code modification, <300ms budget
    FULL_GENERATION = 2,    // Complete function/file, quality priority
    REFINEMENT = 3,         // Post-processing pass, accuracy priority
    SPECULATIVE_DRAFT = 4,  // Fast draft for speculative decode
};

// Latency budget constraints
struct LatencyBudget {
    std::chrono::microseconds first_token;   // Max time to first token
    std::chrono::microseconds per_token;     // Max time per subsequent token
    float min_confidence;                     // Minimum confidence for early exit
};

// Kernel selection result
struct KernelSelection {
    int kernel_mode;           // MatMulKernelMode enum value
    const char* kernel_name;   // Human-readable name
    bool speculative;          // Whether to use speculative decoding
    int draft_kernel;          // Draft kernel for speculative (if applicable)
};

// Performance database (populated from benchmarks)
// Q4_K sg_u32: 95 GFLOPs baseline
// Q4_0 u32:    85 GFLOPs (-10%)
// Q5_K u32:    73 GFLOPs (-23%)
// Q6_K u32:    58 GFLOPs (-39%)

class KernelArbiter {
public:
    // Initialize with benchmark data
    KernelArbiter();
    
    // Select kernel based on task and latency budget
    KernelSelection SelectKernel(TaskType task, const LatencyBudget& budget) const noexcept;
    
    // Select kernel based on confidence (for early-exit)
    KernelSelection SelectByConfidence(float confidence, TaskType task) const noexcept;
    
    // Get recommended kernel for speculative decoding
    KernelSelection GetSpeculativePair() const noexcept;
    
    // Adaptive selection based on runtime metrics
    KernelSelection SelectAdaptive(
        TaskType task,
        std::chrono::microseconds elapsed,
        int tokens_generated,
        float current_confidence
    ) const noexcept;
    
    // Performance profiles
    static constexpr KernelPerfProfile Q4K_PROFILE = {
        .gflops_4096 = 95.08f,
        .gflops_11008 = 95.38f,
        .latency_factor = 1.0f,
        .spv_path = "fused_q4k_q8_1_sg_u32.spv"
    };
    
    static constexpr KernelPerfProfile Q40_PROFILE = {
        .gflops_4096 = 85.55f,
        .gflops_11008 = 89.08f,
        .latency_factor = 1.11f,
        .spv_path = "fused_q4_0_u32.spv"
    };
    
    static constexpr KernelPerfProfile Q5K_PROFILE = {
        .gflops_4096 = 73.68f,
        .gflops_11008 = 71.63f,
        .latency_factor = 1.29f,
        .spv_path = "fused_q5_k_u32.spv"
    };
    
    static constexpr KernelPerfProfile Q6K_PROFILE = {
        .gflops_4096 = 57.81f,
        .gflops_11008 = 60.16f,
        .latency_factor = 1.64f,
        .spv_path = "fused_q6_k_u32.spv"
    };

private:
    // Latency thresholds (microseconds) derived from benchmarks
    static constexpr int64_t AUTOCOMPLETE_FIRST_TOKEN_US = 50000;   // 50ms
    static constexpr int64_t AUTOCOMPLETE_PER_TOKEN_US = 2000;      // 2ms
    static constexpr int64_t INLINE_EDIT_FIRST_TOKEN_US = 150000;   // 150ms
    static constexpr int64_t INLINE_EDIT_PER_TOKEN_US = 5000;       // 5ms
    static constexpr int64_t FULL_GEN_FIRST_TOKEN_US = 300000;     // 300ms
    static constexpr int64_t FULL_GEN_PER_TOKEN_US = 10000;       // 10ms
    
    // Confidence thresholds for early exit
    static constexpr float HIGH_CONFIDENCE_THRESHOLD = 0.92f;
    static constexpr float MEDIUM_CONFIDENCE_THRESHOLD = 0.80f;
    static constexpr float LOW_CONFIDENCE_THRESHOLD = 0.65f;
};

// Inline implementation for hot path
inline KernelSelection KernelArbiter::SelectKernel(TaskType task, const LatencyBudget& budget) const noexcept {
    // Fast path: autocomplete always uses Q4_K for lowest latency
    if (task == TaskType::AUTOCOMPLETE) {
        return {
            .kernel_mode = 1,  // Q4KQ81U32
            .kernel_name = "q4k_q8_1_u32",
            .speculative = false,
            .draft_kernel = -1
        };
    }
    
    // Speculative draft: use fastest kernel
    if (task == TaskType::SPECULATIVE_DRAFT) {
        return {
            .kernel_mode = 1,
            .kernel_name = "q4k_q8_1_u32",
            .speculative = false,
            .draft_kernel = -1
        };
    }
    
    // Inline edit: balance speed and quality
    if (task == TaskType::INLINE_EDIT) {
        // Check if budget allows Q5_K
        if (budget.first_token.count() >= INLINE_EDIT_FIRST_TOKEN_US) {
            return {
                .kernel_mode = 3,  // Q5KU32
                .kernel_name = "q5_k_u32",
                .speculative = false,
                .draft_kernel = -1
            };
        }
        // Fall back to Q4_K for speed
        return {
            .kernel_mode = 1,
            .kernel_name = "q4k_q8_1_u32",
            .speculative = false,
            .draft_kernel = -1
        };
    }
    
    // Full generation: use speculative decoding
    if (task == TaskType::FULL_GENERATION) {
        return {
            .kernel_mode = 4,  // Q6KU32 (verifier)
            .kernel_name = "q6_k_u32",
            .speculative = true,
            .draft_kernel = 1  // Q4_K as draft
        };
    }
    
    // Refinement: highest quality
    return {
        .kernel_mode = 4,
        .kernel_name = "q6_k_u32",
        .speculative = false,
        .draft_kernel = -1
    };
}

inline KernelSelection KernelArbiter::SelectByConfidence(float confidence, TaskType task) const noexcept {
    // High confidence: can use faster kernel
    if (confidence >= HIGH_CONFIDENCE_THRESHOLD) {
        return SelectKernel(TaskType::AUTOCOMPLETE, {});
    }
    
    // Medium confidence: balanced kernel
    if (confidence >= MEDIUM_CONFIDENCE_THRESHOLD) {
        return SelectKernel(TaskType::INLINE_EDIT, {});
    }
    
    // Low confidence: need quality
    return SelectKernel(TaskType::FULL_GENERATION, {});
}

inline KernelSelection KernelArbiter::GetSpeculativePair() const noexcept {
    return {
        .kernel_mode = 4,  // Q6_K verifier
        .kernel_name = "q6_k_u32",
        .speculative = true,
        .draft_kernel = 1  // Q4_K draft
    };
}

inline KernelSelection KernelArbiter::SelectAdaptive(
    TaskType task,
    std::chrono::microseconds elapsed,
    int tokens_generated,
    float current_confidence
) const noexcept {
    // Adaptive switching based on runtime metrics
    
    // If we're ahead of schedule and confidence is high, stay fast
    if (elapsed.count() < AUTOCOMPLETE_FIRST_TOKEN_US && 
        current_confidence >= HIGH_CONFIDENCE_THRESHOLD) {
        return SelectKernel(TaskType::AUTOCOMPLETE, {});
    }
    
    // If we're behind schedule, drop to faster kernel
    if (elapsed.count() > FULL_GEN_FIRST_TOKEN_US) {
        return SelectKernel(TaskType::AUTOCOMPLETE, {});
    }
    
    // If confidence dropped, upgrade kernel
    if (current_confidence < LOW_CONFIDENCE_THRESHOLD && tokens_generated > 5) {
        return SelectKernel(TaskType::REFINEMENT, {});
    }
    
    // Default: use task-appropriate kernel
    return SelectKernel(task, {});
}

} // namespace RawrXD