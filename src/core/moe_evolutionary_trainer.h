// ============================================================================
// moe_evolutionary_trainer.h - Evolution Strategy for Expert Training
// Zero dependencies. C++17. Replaces stochastic perturbation with selection.
// ============================================================================
#pragma once

#include <cstdint>
#include <cstddef>
#include <cmath>
#include <string>
#include <array>
#include <vector>
#include <algorithm>
#include <atomic>
#include <mutex>

namespace moe {

// ----------------------------------------------------------------------------
// Fast xorshift64* PRNG (deterministic, no std::random overhead)
// ----------------------------------------------------------------------------
struct FastRNG {
    uint64_t state;
    explicit FastRNG(uint64_t seed = 0x9E3779B97F4A7C15ULL) : state(seed) {}
    
    uint64_t next() {
        state ^= state >> 12;
        state ^= state << 25;
        state ^= state >> 27;
        return state * 0x2545F4914F6CDD1DULL;
    }
    
    float uniform_f() {
        return float(next() >> 40) / float(1ULL << 24);
    }
    
    float normal_f() {
        static float spare;
        static bool has_spare = false;
        if (has_spare) {
            has_spare = false;
            return spare;
        }
        float u1 = uniform_f();
        float u2 = uniform_f();
        if (u1 < 1e-7f) u1 = 1e-7f;
        float mag = std::sqrt(-2.0f * std::log(u1));
        spare = mag * std::cos(6.283185307f * u2);
        has_spare = true;
        return mag * std::sin(6.283185307f * u2);
    }
};

// ----------------------------------------------------------------------------
// Q4_1 block structure (matches GGML/GGUF format)
// ----------------------------------------------------------------------------
struct Q4_1_Block {
    uint16_t d;      // scale (fp16)
    uint16_t m;      // min (fp16)
    uint8_t qs[32];  // 32x 4-bit weights
};

// ----------------------------------------------------------------------------
// Fitness evaluation for a code completion task
// ----------------------------------------------------------------------------
struct TaskExample {
    std::vector<int> input_tokens;
    std::vector<int> expected_tokens;
    float importance;  // 1.0 = standard, 2.0 = critical bug fix
    int category;      // 0-5: CODE, BUG_FIX, REFACTOR, DOC, TEST, UNKNOWN
    
    TaskExample() : importance(1.0f), category(0) {}
};

struct FitnessResult {
    float score;           // 0.0 = terrible, 1.0 = perfect
    float token_accuracy;  // exact token match rate
    float compile_score;   // 1.0 if compiles, 0.0 if not
    float lcs_bonus;       // longest common subsequence ratio
    int tokens_generated;
    bool success;          // overall success flag
    
    FitnessResult() : score(0.0f), token_accuracy(0.0f), compile_score(0.0f),
                        lcs_bonus(0.0f), tokens_generated(0), success(false) {}
};

// ----------------------------------------------------------------------------
// Task Evaluator - NOT just token match
// ----------------------------------------------------------------------------
class TaskEvaluator {
public:
    static FitnessResult evaluate(
        const std::vector<int>& generated,
        const TaskExample& task,
        bool can_compile = false
    ) {
        FitnessResult result;
        result.tokens_generated = int(generated.size());
        result.compile_score = can_compile ? 1.0f : 0.0f;
        
        if (task.expected_tokens.empty()) {
            // Open-ended generation
            result.token_accuracy = 0.5f;
            result.lcs_bonus = 0.5f;
            result.score = 0.3f * task.importance;
            return result;
        }
        
        // Exact token match rate
        int matches = 0;
        int min_len = std::min(int(generated.size()), int(task.expected_tokens.size()));
        for (int i = 0; i < min_len; i++) {
            if (generated[i] == task.expected_tokens[i]) matches++;
        }
        result.token_accuracy = (task.expected_tokens.empty()) ? 0.0f :
            float(matches) / float(task.expected_tokens.size());
        
        // Longest common subsequence bonus
        result.lcs_bonus = compute_lcs_ratio(generated, task.expected_tokens);
        
        // Weighted composite
        result.score = result.token_accuracy * 0.6f + 
                       result.lcs_bonus * 0.3f + 
                       result.compile_score * 0.1f;
        result.score *= task.importance;
        result.success = result.score > 0.6f;
        
        return result;
    }
    
private:
    static float compute_lcs_ratio(
        const std::vector<int>& a,
        const std::vector<int>& b
    ) {
        int m = int(a.size()), n = int(b.size());
        if (m == 0 || n == 0) return 0.0f;
        
        // Space-optimized LCS (O(min(m,n)) space)
        std::vector<int> prev(n + 1, 0), curr(n + 1, 0);
        for (int i = 1; i <= m; i++) {
            for (int j = 1; j <= n; j++) {
                curr[j] = (a[i-1] == b[j-1]) ? prev[j-1] + 1 
                                             : std::max(prev[j], curr[j-1]);
            }
            std::swap(prev, curr);
        }
        return float(prev[n]) / float(std::max(m, n));
    }
};

// ----------------------------------------------------------------------------
// Evolution Strategy: (mu/rho, lambda) selection
// ----------------------------------------------------------------------------
template<int N_PARAMS = 32>
class EvolutionStrategy {
public:
    static constexpr int MU = 4;       // Parents
    static constexpr int LAMBDA = 16;  // Offspring per parent
    static constexpr int RHO = 2;        // Parents per recombination
    
    struct Individual {
        std::array<float, N_PARAMS> params{};  // Perturbation deltas
        float fitness;
        uint32_t generation;
        
        Individual() : fitness(0.0f), generation(0) {}
    };
    
    FastRNG rng;
    std::vector<Individual> population;
    float sigma;           // Mutation strength (adaptive)
    float best_fitness;
    int generation;
    
    EvolutionStrategy() : sigma(0.1f), best_fitness(0.0f), generation(0) {
        population.reserve(MU);
        for (int i = 0; i < MU; i++) {
            Individual ind;
            for (int j = 0; j < N_PARAMS; j++) {
                ind.params[j] = rng.normal_f() * 0.01f;
            }
            ind.fitness = 0.0f;
            ind.generation = 0;
            population.push_back(ind);
        }
    }
    
    // Generate offspring by recombination + mutation
    std::vector<Individual> generate_offspring() {
        std::vector<Individual> offspring;
        offspring.reserve(MU * LAMBDA);
        
        for (int p = 0; p < MU; p++) {
            for (int l = 0; l < LAMBDA; l++) {
                Individual child;
                
                // Intermediate recombination: average RHO random parents
                int parent_idx[2];
                for (int r = 0; r < RHO; r++) {
                    parent_idx[r] = int(rng.next() % MU);
                }
                
                for (int i = 0; i < N_PARAMS; i++) {
                    float sum = 0.0f;
                    for (int r = 0; r < RHO; r++) {
                        sum += population[parent_idx[r]].params[i];
                    }
                    child.params[i] = sum / RHO;
                }
                
                // Self-adaptive mutation
                float tau = 1.0f / std::sqrt(2.0f * N_PARAMS);
                float tau0 = 1.0f / std::sqrt(2.0f * N_PARAMS);
                float global_sigma = sigma * std::exp(tau0 * rng.normal_f());
                
                for (int i = 0; i < N_PARAMS; i++) {
                    float local_sigma = global_sigma * std::exp(tau * rng.normal_f());
                    child.params[i] += local_sigma * rng.normal_f();
                    // Clamp to prevent explosion
                    child.params[i] = std::max(-1.0f, std::min(1.0f, child.params[i]));
                }
                
                child.fitness = -1.0f;  // Not evaluated yet
                child.generation = generation;
                offspring.push_back(child);
            }
        }
        
        return offspring;
    }
    
    // Select top MU by fitness (mu,lambda) - comma selection, no elitism
    void select_survivors(std::vector<Individual>& offspring) {
        std::sort(offspring.begin(), offspring.end(),
            [](const Individual& a, const Individual& b) {
                return a.fitness > b.fitness;
            });
        
        population.clear();
        for (int i = 0; i < MU && i < int(offspring.size()); i++) {
            population.push_back(offspring[i]);
        }
        
        if (!population.empty() && population[0].fitness > best_fitness) {
            best_fitness = population[0].fitness;
        }
        
        // Adaptive sigma
        static float prev_best = 0.0f;
        if (best_fitness > prev_best + 0.001f) {
            sigma *= 1.1f;  // Exploit
        } else {
            sigma *= 0.9f;  // Explore
        }
        sigma = std::max(0.001f, std::min(1.0f, sigma));
        prev_best = best_fitness;
        
        generation++;
    }
    
    const float* best_params() const {
        if (population.empty()) return nullptr;
        return population[0].params.data();
    }
    
    float get_best_fitness() const { return best_fitness; }
    int get_generation() const { return generation; }
};

// ----------------------------------------------------------------------------
// Expert Trainer - Applies ES to a single expert's Q4_1 weights
// ----------------------------------------------------------------------------
class ExpertTrainer {
public:
    static constexpr int BLOCKS_PER_STEP = 32;
    
    EvolutionStrategy<BLOCKS_PER_STEP> es;
    std::vector<TaskExample> training_tasks;
    std::atomic<bool> training_active{false};
    std::mutex task_mutex;
    
    int current_block_offset;
    int total_blocks;
    
    ExpertTrainer() : current_block_offset(0), total_blocks(0) {}
    
    void add_training_task(const TaskExample& task) {
        std::lock_guard<std::mutex> lock(task_mutex);
        if (training_tasks.size() < 1000) {  // Limit memory
            training_tasks.push_back(task);
        }
    }
    
    // Apply current best perturbation to weights
    void apply_perturbation(Q4_1_Block* weights, size_t num_blocks);
    
    // Evaluate fitness of a perturbation
    float evaluate_perturbation(
        Q4_1_Block* original_weights,
        size_t num_blocks,
        const float* perturbation,
        int (*inference_fn)(const std::vector<int>&, void*),
        void* model_ctx
    );
    
    // One generation of evolution
    void evolve_generation(
        Q4_1_Block* weights,
        size_t num_blocks,
        int (*inference_fn)(const std::vector<int>&, void*),
        void* model_ctx
    );
    
    // Background training loop
    void start_training(Q4_1_Block* weights, size_t num_blocks);
    void stop_training();
    bool is_training() const { return training_active.load(); }
    
private:
    static float fp16_to_fp32(uint16_t h);
    static uint16_t fp32_to_fp16(float f);
};

// ----------------------------------------------------------------------------
// Global trainer registry
// ----------------------------------------------------------------------------
class TrainerRegistry {
public:
    static constexpr int MAX_EXPERTS = 32;
    std::array<ExpertTrainer, MAX_EXPERTS> trainers;
    
    ExpertTrainer* get_trainer(int expert_id) {
        if (expert_id < 0 || expert_id >= MAX_EXPERTS) return nullptr;
        return &trainers[expert_id];
    }
};

extern TrainerRegistry g_trainer_registry;

} // namespace moe
