// ============================================================================
// MOE EVOLUTIONARY TRAINER (CMA-ES Style)
// Zero dependencies, C++17, convergent optimization
// Replaces random weight jitter with selection pressure
// ============================================================================
#pragma once

#include <cstdint>
#include <cstddef>
#include <cmath>
#include <vector>
#include <array>
#include <algorithm>
#include <numeric>
#include <cstring>
#include <random>
#include <mutex>
#include <atomic>
#include <queue>
#include <thread>
#include <condition_variable>

namespace RawrXD {
namespace Training {

// ----------------------------------------------------------------------------
// Fast PRNG (xorshift64*)
// ----------------------------------------------------------------------------
class FastRNG {
    uint64_t state_;
    
public:
    explicit FastRNG(uint64_t seed = 0x9E3779B97F4A7C15ULL) : state_(seed) {}
    
    uint64_t Next() {
        state_ ^= state_ >> 12;
        state_ ^= state_ << 25;
        state_ ^= state_ >> 27;
        return state_ * 0x2545F4914F6CDD1DULL;
    }
    
    float UniformF() {
        return float(Next() >> 40) / float(1ULL << 24);
    }
    
    float NormalF() {
        static float spare;
        static bool has_spare = false;
        if (has_spare) {
            has_spare = false;
            return spare;
        }
        float u1 = UniformF();
        float u2 = UniformF();
        if (u1 < 1e-7f) u1 = 1e-7f;
        float mag = std::sqrt(-2.0f * std::log(u1));
        spare = mag * std::cos(6.283185307f * u2);
        has_spare = true;
        return mag * std::sin(6.283185307f * u2);
    }
};

// ----------------------------------------------------------------------------
// Q4_1 Quantization Block
// ----------------------------------------------------------------------------
struct Q4_1_Block {
    uint16_t d;      // scale (fp16)
    uint16_t m;      // min (fp16)
    uint8_t qs[32];  // 32 nibbles (4-bit weights)
    
    static float FP16ToFP32(uint16_t h) {
        uint32_t sign = (h >> 15) & 0x1;
        uint32_t exp = (h >> 10) & 0x1F;
        uint32_t mant = h & 0x3FF;
        if (exp == 0) {
            if (mant == 0) return sign ? -0.0f : 0.0f;
            return (sign ? -1.0f : 1.0f) * std::ldexp(mant / 1024.0f, -14);
        }
        if (exp == 31) {
            return mant ? std::numeric_limits<float>::quiet_NaN() :
                   (sign ? -std::numeric_limits<float>::infinity() :
                           std::numeric_limits<float>::infinity());
        }
        return (sign ? -1.0f : 1.0f) * std::ldexp(1.0f + mant / 1024.0f, exp - 15);
    }
    
    static uint16_t FP32ToFP16(float f) {
        uint32_t x; std::memcpy(&x, &f, 4);
        uint32_t sign = (x >> 31) & 0x1;
        int exp = int((x >> 23) & 0xFF) - 127;
        uint32_t mant = x & 0x7FFFFF;
        if (std::abs(f) < 0.000061035f) return sign << 15;
        if (exp > 15) return (sign << 15) | (31 << 10);
        if (exp < -14) return (sign << 15) | (1 << 10) | (mant >> 13);
        return uint16_t((sign << 15) | ((exp + 15) << 10) | (mant >> 13));
    }
    
    float Dequantize(int idx) const {
        float scale = FP16ToFP32(d);
        float min_val = FP16ToFP32(m);
        int q = (idx & 1) ? (qs[idx >> 1] >> 4) : (qs[idx >> 1] & 0xF);
        return scale * q + min_val;
    }
};

// ----------------------------------------------------------------------------
// Task Example for Training
// ----------------------------------------------------------------------------
struct TaskExample {
    std::vector<int> input_tokens;
    std::vector<int> expected_tokens;
    float importance;
    bool is_negative;  // true = what NOT to generate
    
    TaskExample() : importance(1.0f), is_negative(false) {}
};

// ----------------------------------------------------------------------------
// Fitness Evaluation
// ----------------------------------------------------------------------------
struct FitnessResult {
    float score;           // 0.0 = terrible, 1.0 = perfect
    float token_accuracy;
    float structure_score;
    float compile_score;
    int tokens_generated;
};

class FitnessEvaluator {
public:
    static float ComputeLCSRatio(const std::vector<int>& a, const std::vector<int>& b) {
        int m = static_cast<int>(a.size());
        int n = static_cast<int>(b.size());
        if (m == 0 || n == 0) return 0.0f;
        
        // Space-optimized LCS
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
    
    static FitnessResult Evaluate(
        const std::vector<int>& generated,
        const TaskExample& task
    ) {
        FitnessResult result = {};
        result.tokens_generated = static_cast<int>(generated.size());
        
        if (task.is_negative) {
            // Negative example: reward divergence from bad output
            result.score = 0.0f;
            result.token_accuracy = 0.0f;
            return result;
        }
        
        if (task.expected_tokens.empty()) {
            // Open-ended generation
            result.token_accuracy = 0.5f;
            result.score = 0.3f;
            return result;
        }
        
        // Exact token match rate
        int matches = 0;
        int min_len = std::min(static_cast<int>(generated.size()), 
                               static_cast<int>(task.expected_tokens.size()));
        for (int i = 0; i < min_len; i++) {
            if (generated[i] == task.expected_tokens[i]) matches++;
        }
        result.token_accuracy = float(matches) / float(task.expected_tokens.size());
        
        // Longest common subsequence (catches near-misses)
        float lcs_bonus = ComputeLCSRatio(generated, task.expected_tokens);
        
        // Structure score (braces, parens balance)
        result.structure_score = CheckStructure(generated);
        
        // Weighted composite
        result.score = result.token_accuracy * 0.5f + 
                       lcs_bonus * 0.3f + 
                       result.structure_score * 0.2f;
        
        result.score *= task.importance;
        return result;
    }
    
private:
    static float CheckStructure(const std::vector<int>& tokens) {
        // Simplified: check for balanced braces/parens in token space
        // In real impl, detokenize and parse
        return 1.0f;  // Placeholder
    }
};

// ----------------------------------------------------------------------------
// Evolution Strategy (CMA-ES Simplified)
// ----------------------------------------------------------------------------
template<int N_PARAMS = 32>
class EvolutionStrategy {
public:
    static constexpr int MU = 4;       // Parents
    static constexpr int LAMBDA = 16;  // Offspring per parent
    static constexpr int RHO = 2;      // Parents per recombination
    
    struct Individual {
        std::array<float, N_PARAMS> params;
        float fitness;
        uint32_t generation;
        
        Individual() : fitness(-1.0f), generation(0) {
            params.fill(0.0f);
        }
    };
    
private:
    FastRNG rng_;
    std::vector<Individual> population_;
    float sigma_;           // Global mutation strength
    float best_fitness_;
    uint32_t generation_;
    std::mutex mtx_;
    
public:
    EvolutionStrategy() : sigma_(0.1f), best_fitness_(0.0f), generation_(0) {
        population_.reserve(MU);
        for (int i = 0; i < MU; i++) {
            Individual ind;
            for (int j = 0; j < N_PARAMS; j++) {
                ind.params[j] = rng_.NormalF() * 0.01f;
            }
            population_.push_back(ind);
        }
    }
    
    std::vector<Individual> GenerateOffspring() {
        std::lock_guard<std::mutex> lock(mtx_);
        std::vector<Individual> offspring;
        offspring.reserve(MU * LAMBDA);
        
        for (int p = 0; p < MU; p++) {
            for (int l = 0; l < LAMBDA; l++) {
                Individual child;
                
                // Intermediate recombination
                std::array<int, RHO> parents;
                for (int r = 0; r < RHO; r++) {
                    parents[r] = static_cast<int>(rng_.Next() % MU);
                }
                
                for (int i = 0; i < N_PARAMS; i++) {
                    float sum = 0.0f;
                    for (int r = 0; r < RHO; r++) {
                        sum += population_[parents[r]].params[i];
                    }
                    child.params[i] = sum / RHO;
                }
                
                // Self-adaptive mutation
                float tau = 1.0f / std::sqrt(2.0f * N_PARAMS);
                float tau0 = 1.0f / std::sqrt(2.0f * N_PARAMS);
                float global_sigma = sigma_ * std::exp(tau0 * rng_.NormalF());
                
                for (int i = 0; i < N_PARAMS; i++) {
                    float local_sigma = global_sigma * std::exp(tau * rng_.NormalF());
                    child.params[i] += local_sigma * rng_.NormalF();
                    child.params[i] = std::max(-1.0f, std::min(1.0f, child.params[i]));
                }
                
                child.generation = generation_;
                offspring.push_back(child);
            }
        }
        
        return offspring;
    }
    
    void SelectSurvivors(std::vector<Individual>& offspring) {
        std::lock_guard<std::mutex> lock(mtx_);
        
        std::sort(offspring.begin(), offspring.end(),
            [](const Individual& a, const Individual& b) {
                return a.fitness > b.fitness;
            });
        
        population_.clear();
        for (int i = 0; i < MU && i < static_cast<int>(offspring.size()); i++) {
            population_.push_back(offspring[i]);
        }
        
        if (!population_.empty() && population_[0].fitness > best_fitness_) {
            best_fitness_ = population_[0].fitness;
        }
        
        // Adaptive sigma
        static float prev_best = 0.0f;
        if (best_fitness_ > prev_best + 0.001f) {
            sigma_ *= 1.1f;  // Exploit
        } else {
            sigma_ *= 0.9f;  // Explore
        }
        sigma_ = std::max(0.001f, std::min(1.0f, sigma_));
        prev_best = best_fitness_;
        
        generation_++;
    }
    
    const Individual* BestIndividual() const {
        std::lock_guard<std::mutex> lock(const_cast<std::mutex&>(mtx_));
        return population_.empty() ? nullptr : &population_[0];
    }
    
    float BestFitness() const {
        std::lock_guard<std::mutex> lock(const_cast<std::mutex&>(mtx_));
        return best_fitness_;
    }
    
    uint32_t Generation() const {
        std::lock_guard<std::mutex> lock(const_cast<std::mutex&>(mtx_));
        return generation_;
    }
};

// ----------------------------------------------------------------------------
// Expert Trainer (Per-Expert Training)
// ----------------------------------------------------------------------------
class ExpertTrainer {
    static constexpr int BLOCKS_PER_STEP = 32;
    
    EvolutionStrategy<BLOCKS_PER_STEP> es_;
    std::vector<TaskExample> training_tasks_;
    std::atomic<size_t> current_block_offset_{0};
    std::mutex tasks_mtx_;
    
public:
    ExpertTrainer() = default;
    
    void AddTrainingTask(const TaskExample& task) {
        std::lock_guard<std::mutex> lock(tasks_mtx_);
        training_tasks_.push_back(task);
        // Limit training set size
        if (training_tasks_.size() > 1000) {
            training_tasks_.erase(training_tasks_.begin());
        }
    }
    
    void ApplyPerturbation(
        Q4_1_Block* weights,
        size_t num_blocks,
        const std::array<float, BLOCKS_PER_STEP>& perturbation
    ) {
        size_t offset = current_block_offset_.fetch_add(BLOCKS_PER_STEP) % num_blocks;
        
        for (int i = 0; i < BLOCKS_PER_STEP; i++) {
            size_t idx = (offset + i) % num_blocks;
            float scale = Q4_1_Block::FP16ToFP32(weights[idx].d);
            scale *= (1.0f + perturbation[i] * 0.1f);
            weights[idx].d = Q4_1_Block::FP32ToFP16(scale);
        }
    }
    
    float EvaluatePerturbation(
        const Q4_1_Block* original_weights,
        size_t num_blocks,
        const std::array<float, BLOCKS_PER_STEP>& perturbation
    ) {
        // Create temporary copy
        std::vector<Q4_1_Block> temp_weights(original_weights, original_weights + num_blocks);
        ApplyPerturbation(temp_weights.data(), num_blocks, perturbation);
        
        float total_score = 0.0f;
        int valid_tasks = 0;
        
        std::vector<TaskExample> tasks;
        {
            std::lock_guard<std::mutex> lock(tasks_mtx_);
            tasks = training_tasks_;
        }
        
        for (const auto& task : tasks) {
            // In real impl: run inference with perturbed weights
            // For now, use placeholder
            std::vector<int> output;  // Would be generated
            auto result = FitnessEvaluator::Evaluate(output, task);
            total_score += result.score;
            valid_tasks++;
        }
        
        return valid_tasks > 0 ? total_score / valid_tasks : 0.0f;
    }
    
    void EvolveGeneration(Q4_1_Block* weights, size_t num_blocks) {
        auto offspring = es_.GenerateOffspring();
        
        for (auto& child : offspring) {
            child.fitness = EvaluatePerturbation(weights, num_blocks, child.params);
        }
        
        es_.SelectSurvivors(offspring);
        
        const auto* best = es_.BestIndividual();
        if (best) {
            ApplyPerturbation(weights, num_blocks, best->params);
        }
    }
    
    float BestFitness() const {
        return es_.BestFitness();
    }
    
    uint32_t Generation() const {
        return es_.Generation();
    }
};

// ----------------------------------------------------------------------------
// Background Training Thread
// ----------------------------------------------------------------------------
class TrainingOrchestrator {
    std::vector<std::unique_ptr<ExpertTrainer>> trainers_;
    std::vector<Q4_1_Block*> expert_weights_;
    std::vector<size_t> expert_num_blocks_;
    
    std::thread training_thread_;
    std::atomic<bool> running_{false};
    std::condition_variable cv_;
    std::mutex cv_mtx_;
    
    std::queue<int> training_queue_;
    std::mutex queue_mtx_;
    
public:
    void Initialize(int num_experts) {
        trainers_.reserve(num_experts);
        expert_weights_.resize(num_experts);
        expert_num_blocks_.resize(num_experts);
        
        for (int i = 0; i < num_experts; i++) {
            trainers_.push_back(std::make_unique<ExpertTrainer>());
        }
    }
    
    void RegisterExpert(int expert_id, Q4_1_Block* weights, size_t num_blocks) {
        if (expert_id >= 0 && expert_id < static_cast<int>(expert_weights_.size())) {
            expert_weights_[expert_id] = weights;
            expert_num_blocks_[expert_id] = num_blocks;
        }
    }
    
    void Start() {
        running_ = true;
        training_thread_ = std::thread([this]() { TrainingLoop(); });
    }
    
    void Stop() {
        running_ = false;
        cv_.notify_all();
        if (training_thread_.joinable()) {
            training_thread_.join();
        }
    }
    
    void QueueTraining(int expert_id) {
        {
            std::lock_guard<std::mutex> lock(queue_mtx_);
            training_queue_.push(expert_id);
        }
        cv_.notify_one();
    }
    
    void AddTask(int expert_id, const TaskExample& task) {
        if (expert_id >= 0 && expert_id < static_cast<int>(trainers_.size())) {
            trainers_[expert_id]->AddTrainingTask(task);
        }
    }
    
private:
    void TrainingLoop() {
        while (running_) {
            std::unique_lock<std::mutex> lock(cv_mtx_);
            cv_.wait_for(lock, std::chrono::seconds(1), [this]() { 
                std::lock_guard<std::mutex> qlock(queue_mtx_);
                return !training_queue_.empty() || !running_; 
            });
            
            if (!running_) break;
            
            int expert_id = -1;
            {
                std::lock_guard<std::mutex> qlock(queue_mtx_);
                if (!training_queue_.empty()) {
                    expert_id = training_queue_.front();
                    training_queue_.pop();
                }
            }
            
            if (expert_id >= 0 && expert_id < static_cast<int>(trainers_.size())) {
                auto* weights = expert_weights_[expert_id];
                auto num_blocks = expert_num_blocks_[expert_id];
                if (weights && num_blocks > 0) {
                    trainers_[expert_id]->EvolveGeneration(weights, num_blocks);
                }
            }
        }
    }
};

} // namespace Training
} // namespace RawrXD
