// MoEDispatcherWithValidation.hpp
// Integrates CorrectnessValidator into expert routing
// Replaces noise-based "learning" with correctness-signal optimization

#ifndef MOE_DISPATCHER_WITH_VALIDATION_HPP
#define MOE_DISPATCHER_WITH_VALIDATION_HPP

#include "CorrectednessValidator.hpp"
#include <vector>
#include <memory>
#include <algorithm>

// Stub: dummy expert for testing (replace with real MoE experts)
class DummyExpert {
public:
    explicit DummyExpert(int id) : id(id) {}
    
    std::string generate(const std::string& prompt) {
        // Stub: return a simple C++ snippet
        return "int main() {\n  int x = 42;\n  return x;\n}\n";
    }
    
private:
    int id;
};

// ---------------------------------------------------------------------------
// 1. TASK DEFINITION (with correctness ground truth)
// ---------------------------------------------------------------------------

struct TaskDefinition {
    std::string description;
    std::string input_prompt;
    std::string category;                   // "math", "code", "refactor", etc.
    std::string expected_output;            // ground truth for validation
    std::vector<std::string> validation_rules;  // e.g., "must compile", "must have main()"
    float difficulty_estimate;              // 0.0 to 1.0
    int token_budget;                       // max tokens to generate
};

// ---------------------------------------------------------------------------
// 2. EXPERT RESPONSE (with correctness metadata)
// ---------------------------------------------------------------------------

struct ExpertResponse {
    int expert_id;
    std::string generated_output;
    ValidationResult validation;
    float latency_ms;
    bool was_accepted;                      // by user or validation
};

// ---------------------------------------------------------------------------
// 3. DISPATCH DECISION (route experts based on correctness history)
// ---------------------------------------------------------------------------

class MoEDispatcherWithValidation {
public:
    MoEDispatcherWithValidation(int num_experts)
        : num_experts(num_experts), 
          validator(std::make_unique<CorrectnessValidator>()),
          tracker(std::make_unique<ExpertPerformanceTracker>()) {
        // Initialize expert pool (stub: in production these are loaded from files)
        expert_pool.resize(num_experts);
        for (int i = 0; i < num_experts; i++) {
            expert_pool[i] = std::make_unique<DummyExpert>(i);
        }
    }
    
    // Main dispatch: try experts in order of correctness weight until one succeeds
    ExpertResponse dispatch_task_intelligent(const TaskDefinition& task) {
        std::vector<ExpertResponse> attempts;
        
        // Get ordered list of experts by correctness weight for this category
        std::vector<int> expert_order = rank_experts_by_correctness(task.category);
        
        // Try each expert (up to max attempts)
        int max_attempts = std::min((int)expert_order.size(), 5);  // try top 5
        for (int attempt = 0; attempt < max_attempts; attempt++) {
            int expert_id = expert_order[attempt];
            
            // Generate
            ExpertResponse response;
            response.expert_id = expert_id;
            response.generated_output = expert_pool[expert_id]->generate(task.input_prompt);
            
            // Validate
            response.validation = validator->validate_cpp(response.generated_output);
            
            // Check semantic correctness (if ground truth exists)
            if (!task.expected_output.empty()) {
                float semantic_sim = validator->semantic_distance(
                    response.generated_output, 
                    task.expected_output
                );
                response.validation.correctness_score = 
                    0.7f * response.validation.correctness_score + 
                    0.3f * semantic_sim;
            }
            
            // Record in performance tracker
            tracker->record_attempt(expert_id, response.validation, task.category);
            
            attempts.push_back(response);
            
            // Success: valid + sufficient correctness score
            if (response.validation.is_valid && response.validation.correctness_score > 0.75f) {
                response.was_accepted = true;
                return response;
            }
        }
        
        // All failed: return best attempt
        ExpertResponse best = attempts[0];
        float best_score = 0;
        for (const auto& resp : attempts) {
            if (resp.validation.correctness_score > best_score) {
                best_score = resp.validation.correctness_score;
                best = resp;
            }
        }
        
        best.was_accepted = false;
        return best;
    }
    
    // When task fails, optionally trigger expert synthesis
    void handle_task_failure(const TaskDefinition& task, const ExpertResponse& response) {
        printf("[MoE] Expert %d failed on task (%s)\n", response.expert_id, task.category.c_str());
        printf("      Correctness: %.2f, Errors: %s\n", 
               response.validation.correctness_score,
               response.validation.error_message.c_str());
        
        // Could trigger expert synthesis here:
        // if (failure_count > threshold) create_new_expert_for_category(task.category);
    }
    
    // Query expert stats (for monitoring/tuning)
    void print_expert_rankings(const std::string& category) {
        printf("\n=== Expert Rankings for '%s' ===\n", category.c_str());
        auto ranks = rank_experts_by_correctness(category);
        for (int i = 0; i < (int)ranks.size() && i < 5; i++) {
            int exp_id = ranks[i];
            auto stats = tracker->get_stats(exp_id);
            printf("  [%d] Expert %d: %.1f%% success, avg score=%.3f, weight=%.3f\n",
                   i, exp_id, 
                   (stats.total_attempts > 0 ? 100.0f * stats.successful_completions / stats.total_attempts : 0),
                   stats.avg_correctness_score,
                   stats.routing_weight);
        }
    }
    
private:
    int num_experts;
    std::vector<std::unique_ptr<DummyExpert>> expert_pool;
    std::unique_ptr<CorrectnessValidator> validator;
    std::unique_ptr<ExpertPerformanceTracker> tracker;
    
    // Rank experts by weighted correctness score
    std::vector<int> rank_experts_by_correctness(const std::string& category) {
        std::vector<int> experts;
        for (int i = 0; i < num_experts; i++) experts.push_back(i);
        
        // Sort by routing weight (correctness-based)
        std::sort(experts.begin(), experts.end(), [this](int a, int b) {
            return tracker->get_routing_weight(a) > tracker->get_routing_weight(b);
        });
        
        return experts;
    }
};

// ---------------------------------------------------------------------------
// 4. ORCHESTRATOR: Ties correctness feedback into IDE ghost-text
// ---------------------------------------------------------------------------

class MoEGhostTextOrchestrator {
public:
    MoEGhostTextOrchestrator(int num_experts)
        : dispatcher(std::make_unique<MoEDispatcherWithValidation>(num_experts)) {}
    
    // Called from IDE ghost-text insertion point
    struct CompletionRequest {
        std::string current_file_content;
        int cursor_line, cursor_col;
        std::string current_language;
        std::string context_hint;            // e.g., "implement quicksort"
    };
    
    struct CompletionResponse {
        std::string suggested_code;
        float confidence;                   // based on correctness_score
        int recommending_expert_id;
        std::string rejection_reason;       // if not confident
    };
    
    CompletionResponse get_completion(const CompletionRequest& req) {
        CompletionResponse resp;
        
        // Build task from request
        TaskDefinition task;
        task.input_prompt = req.current_file_content;
        task.category = categorize_context(req.context_hint, req.current_language);
        task.description = req.context_hint;
        task.token_budget = 256;
        
        // Dispatch to MoE
        auto expert_response = dispatcher->dispatch_task_intelligent(task);
        
        resp.suggested_code = expert_response.generated_output;
        resp.confidence = expert_response.validation.correctness_score;
        resp.recommending_expert_id = expert_response.expert_id;
        
        if (!expert_response.was_accepted) {
            resp.rejection_reason = expert_response.validation.error_message;
        }
        
        return resp;
    }
    
    // Feedback: user accepted or rejected completion
    void record_user_feedback(int expert_id, bool accepted, float user_satisfaction) {
        // This becomes training signal for future decisions
        // Currently: tracked in performance stats
        // Future: could trigger expert fine-tuning
    }
    
private:
    std::unique_ptr<MoEDispatcherWithValidation> dispatcher;
    
    std::string categorize_context(const std::string& hint, const std::string& lang) {
        if (hint.find("sort") != std::string::npos) return "algorithm";
        if (hint.find("parse") != std::string::npos) return "parsing";
        if (hint.find("hash") != std::string::npos) return "data-structure";
        if (lang == "c++" || lang == "c") return "systems";
        return "general";
    }
};

#endif // MOE_DISPATCHER_WITH_VALIDATION_HPP
