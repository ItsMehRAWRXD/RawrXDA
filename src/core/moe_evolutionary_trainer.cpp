// ============================================================================
// moe_evolutionary_trainer.cpp - Evolution Strategy Implementation
// ============================================================================
#include "moe_evolutionary_trainer.h"
#include <cstring>
#include <thread>
#include <chrono>

namespace moe {

TrainerRegistry g_trainer_registry;

// ----------------------------------------------------------------------------
// FP16 conversions
// ----------------------------------------------------------------------------
float ExpertTrainer::fp16_to_fp32(uint16_t h) {
    uint32_t sign = (h >> 15) & 0x1;
    uint32_t exp = (h >> 10) & 0x1F;
    uint32_t mant = h & 0x3FF;
    
    if (exp == 0) {
        if (mant == 0) return sign ? -0.0f : 0.0f;
        return (sign ? -1.0f : 1.0f) * std::ldexp(mant / 1024.0f, -14);
    }
    if (exp == 31) {
        if (mant) return std::numeric_limits<float>::quiet_NaN();
        return sign ? -std::numeric_limits<float>::infinity()
                    : std::numeric_limits<float>::infinity();
    }
    return (sign ? -1.0f : 1.0f) * std::ldexp(1.0f + mant / 1024.0f, exp - 15);
}

uint16_t ExpertTrainer::fp32_to_fp16(float f) {
    uint32_t x;
    std::memcpy(&x, &f, 4);
    uint32_t sign = (x >> 31) & 0x1;
    int exp = int((x >> 23) & 0xFF) - 127;
    uint32_t mant = x & 0x7FFFFF;
    
    if (std::abs(f) < 0.000061035f) return sign << 15;
    if (exp > 15) return (sign << 15) | (31 << 10);
    if (exp < -14) return (sign << 15) | (1 << 10) | (mant >> 13);
    return uint16_t((sign << 15) | ((exp + 15) << 10) | (mant >> 13));
}

// ----------------------------------------------------------------------------
// Apply perturbation to weights
// ----------------------------------------------------------------------------
void ExpertTrainer::apply_perturbation(Q4_1_Block* weights, size_t num_blocks) {
    const float* perturbation = es.best_params();
    if (!perturbation) return;
    
    for (int i = 0; i < BLOCKS_PER_STEP; i++) {
        size_t idx = (current_block_offset + i) % num_blocks;
        float scale = fp16_to_fp32(weights[idx].d);
        scale *= (1.0f + perturbation[i] * 0.1f);
        weights[idx].d = fp32_to_fp16(scale);
    }
    current_block_offset = (current_block_offset + BLOCKS_PER_STEP) % num_blocks;
}

// ----------------------------------------------------------------------------
// Evaluate fitness
// ----------------------------------------------------------------------------
float ExpertTrainer::evaluate_perturbation(
    Q4_1_Block* original_weights,
    size_t num_blocks,
    const float* perturbation,
    int (*inference_fn)(const std::vector<int>&, void*),
    void* model_ctx
) {
    // Create temporary copy
    std::vector<Q4_1_Block> temp_weights(
        original_weights, 
        original_weights + num_blocks
    );
    
    // Apply perturbation
    for (int i = 0; i < BLOCKS_PER_STEP; i++) {
        size_t idx = (current_block_offset + i) % num_blocks;
        float scale = fp16_to_fp32(temp_weights[idx].d);
        scale *= (1.0f + perturbation[i] * 0.1f);
        temp_weights[idx].d = fp32_to_fp16(scale);
    }
    
    float total_score = 0.0f;
    int valid_tasks = 0;
    
    {
        std::lock_guard<std::mutex> lock(task_mutex);
        
        // Sample up to 10 tasks for evaluation
        int eval_count = std::min(10, (int)training_tasks.size());
        for (int t = 0; t < eval_count; t++) {
            const auto& task = training_tasks[t];
            
            // Run inference (callback pattern)
            // In real integration, this calls your forward pass
            // For now, simulate with placeholder
            std::vector<int> output;
            if (inference_fn) {
                // output would be filled by inference_fn
                // Placeholder: assume some output
                output = {100, 200, 300};  // Dummy
            }
            
            auto result = TaskEvaluator::evaluate(output, task);
            total_score += result.score;
            valid_tasks++;
        }
    }
    
    return valid_tasks > 0 ? total_score / valid_tasks : 0.0f;
}

// ----------------------------------------------------------------------------
// One generation of evolution
// ----------------------------------------------------------------------------
void ExpertTrainer::evolve_generation(
    Q4_1_Block* weights,
    size_t num_blocks,
    int (*inference_fn)(const std::vector<int>&, void*),
    void* model_ctx
) {
    auto offspring = es.generate_offspring();
    
    for (auto& child : offspring) {
        child.fitness = evaluate_perturbation(
            weights, num_blocks, child.params.data(), inference_fn, model_ctx
        );
    }
    
    es.select_survivors(offspring);
    
    // Apply best perturbation
    const float* best = es.best_params();
    if (best) {
        apply_perturbation(weights, num_blocks);
    }
}

// ----------------------------------------------------------------------------
// Background training
// ----------------------------------------------------------------------------
void ExpertTrainer::start_training(Q4_1_Block* weights, size_t num_blocks) {
    if (training_active.exchange(true)) return;  // Already running
    
    total_blocks = (int)num_blocks;
    
    std::thread([this, weights, num_blocks]() {
        while (training_active.load()) {
            // Evolve one generation
            evolve_generation(weights, num_blocks, nullptr, nullptr);
            
            // Sleep between generations
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            
            // Stop if no improvement for 100 generations
            if (es.get_generation() > 100 && es.get_best_fitness() < 0.3f) {
                break;
            }
        }
        training_active.store(false);
    }).detach();
}

void ExpertTrainer::stop_training() {
    training_active.store(false);
}

} // namespace moe
