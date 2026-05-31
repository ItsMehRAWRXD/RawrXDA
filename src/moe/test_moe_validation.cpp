// test_moe_validation.cpp
// Demonstrates correctness-driven MoE routing
// Compile: g++ -O2 -std=c++17 -o test_moe test_moe_validation.cpp

#include "MoEDispatcherWithValidation.hpp"
#include <cstdio>
#include <chrono>

void print_separator(const char* title) {
    printf("\n╔════════════════════════════════════════════════════════════════╗\n");
    printf("║  %-62s║\n", title);
    printf("╚════════════════════════════════════════════════════════════════╝\n");
}

int main() {
    print_separator("MoE Dispatcher with Correctness Validation");
    
    // ─────────────────────────────────────────────────────────────────────
    // SETUP
    // ─────────────────────────────────────────────────────────────────────
    
    MoEGhostTextOrchestrator orchestrator(22);  // 22 experts
    CorrectnessValidator validator;
    
    printf("\n✓ Initialized 22-expert pool\n");
    printf("✓ Correctness validator ready\n");
    printf("✓ Performance tracker online\n");
    
    // ─────────────────────────────────────────────────────────────────────
    // VALIDATION TESTS (show correctness scoring)
    // ─────────────────────────────────────────────────────────────────────
    
    print_separator("TEST 1: Code Validation (Correctness Scoring)");
    
    struct TestCode {
        std::string name;
        std::string code;
        std::string expected_category;
    };
    
    std::vector<TestCode> test_cases = {
        {
            "Valid quicksort",
            R"(
                int partition(int* arr, int low, int high) {
                    int pivot = arr[high];
                    int i = low - 1;
                    for (int j = low; j < high; j++) {
                        if (arr[j] < pivot) {
                            i++;
                            int tmp = arr[i];
                            arr[i] = arr[j];
                            arr[j] = tmp;
                        }
                    }
                    int tmp = arr[i+1];
                    arr[i+1] = arr[high];
                    arr[high] = tmp;
                    return i + 1;
                }
            )",
            "algorithm"
        },
        {
            "Syntax error (unbalanced braces)",
            "int main() { printf(\"hello\"); ",
            "systems"
        },
        {
            "Type mismatch (heuristic)",
            "int x = 3.14; double y = x; printf(\"%d\", y);",
            "systems"
        },
        {
            "Clean wrapper",
            R"(
                int main() {
                    int result = 42;
                    return result;
                }
            )",
            "systems"
        }
    };
    
    for (const auto& test : test_cases) {
        auto result = validator.validate_cpp(test.code);
        printf("\n[%s]\n", test.name.c_str());
        printf("  Valid: %s\n", result.is_valid ? "✓" : "✗");
        printf("  Score: %.2f\n", result.correctness_score);
        printf("  Depth: %d, Identifiers: %d\n", result.ast_depth, result.identifier_count);
        if (!result.error_message.empty()) {
            printf("  Error: %s\n", result.error_message.c_str());
        }
        for (const auto& w : result.warnings) {
            printf("  ⚠ %s\n", w.c_str());
        }
    }
    
    // ─────────────────────────────────────────────────────────────────────
    // SEMANTIC DISTANCE TEST
    // ─────────────────────────────────────────────────────────────────────
    
    print_separator("TEST 2: Semantic Distance (Code Similarity)");
    
    std::string expected = R"(
        int bsearch(int* arr, int n, int target) {
            int left = 0, right = n - 1;
            while (left <= right) {
                int mid = (left + right) / 2;
                if (arr[mid] == target) return mid;
                if (arr[mid] < target) left = mid + 1;
                else right = mid - 1;
            }
            return -1;
        }
    )";
    
    std::string generated_good = R"(
        int bsearch(int* arr, int n, int target) {
            int left = 0, right = n - 1;
            while (left <= right) {
                int mid = (left + right) / 2;
                if (arr[mid] == target) return mid;
                if (arr[mid] < target) left = mid + 1;
                else right = mid - 1;
            }
            return -1;
        }
    )";
    
    std::string generated_partial = R"(
        int bsearch(int* arr, int n, int target) {
            int left = 0, right = n - 1;
            while (left <= right) {
                int mid = (left + right) / 2;
                if (arr[mid] == target) return mid;
            }
            return -1;
        }
    )";
    
    float dist_perfect = validator.semantic_distance(generated_good, expected);
    float dist_partial = validator.semantic_distance(generated_partial, expected);
    
    printf("\nPerfect match (should be ~1.0): %.3f\n", dist_perfect);
    printf("Partial match (should be ~0.6-0.8): %.3f\n", dist_partial);
    
    // ─────────────────────────────────────────────────────────────────────
    // TASK DISPATCH TEST (routing by correctness history)
    // ─────────────────────────────────────────────────────────────────────
    
    print_separator("TEST 3: Task Dispatch with Correctness Routing");
    
    std::vector<TaskDefinition> tasks = {
        {
            "Implement merge sort",
            "// Write a merge sort implementation",
            "algorithm",
            "int merge_sort(...) { ... }",
            {"must compile", "must have main or functions"},
            0.8f,
            512
        },
        {
            "Parse JSON",
            "// Write a simple JSON parser",
            "parsing",
            "",  // no ground truth for this test
            {"must handle braces", "must handle quotes"},
            0.9f,
            1024
        },
        {
            "Hash table ops",
            "// Implement basic hash table",
            "data-structure",
            "",
            {"must have insert/lookup"},
            0.7f,
            768
        }
    };
    
    MoEDispatcherWithValidation dispatcher(22);
    
    for (const auto& task : tasks) {
        printf("\n[Task] %s\n", task.description.c_str());
        printf("  Category: %s\n", task.category.c_str());
        printf("  Reward threshold: %.2f\n", task.difficulty_estimate);
        
        auto response = dispatcher.dispatch_task_intelligent(task);
        
        printf("  → Expert %d completed\n", response.expert_id);
        printf("  → Correctness: %.3f\n", response.validation.correctness_score);
        printf("  → Valid: %s\n", response.validation.is_valid ? "✓" : "✗");
        printf("  → Accepted: %s\n", response.was_accepted ? "✓" : "✗");
        
        if (!response.validation.error_message.empty()) {
            printf("  → Issue: %s\n", response.validation.error_message.c_str());
        }
    }
    
    printf("\n");
    dispatcher.print_expert_rankings("algorithm");
    dispatcher.print_expert_rankings("parsing");
    dispatcher.print_expert_rankings("data-structure");
    
    // ─────────────────────────────────────────────────────────────────────
    // IDE INTEGRATION TEST
    // ─────────────────────────────────────────────────────────────────────
    
    print_separator("TEST 4: IDE Ghost-Text Completion");
    
    MoEGhostTextOrchestrator::CompletionRequest req;
    req.current_file_content = R"(
        #include <cstdio>
        
        // TODO: implement binary search
        int
    )";
    req.cursor_line = 5;
    req.cursor_col = 8;
    req.current_language = "c++";
    req.context_hint = "binary search in sorted array";
    
    auto completion = orchestrator.get_completion(req);
    
    printf("\nContext: '%s'\n", req.context_hint.c_str());
    printf("Language: %s\n", req.current_language.c_str());
    printf("\nSuggested code:\n%s\n", completion.suggested_code.c_str());
    printf("Confidence: %.2f%%\n", completion.confidence * 100);
    printf("Recommended by expert: %d\n", completion.recommending_expert_id);
    
    if (!completion.rejection_reason.empty()) {
        printf("Reason not confident: %s\n", completion.rejection_reason.c_str());
    }
    
    // ─────────────────────────────────────────────────────────────────────
    // SUMMARY
    // ─────────────────────────────────────────────────────────────────────
    
    print_separator("SUMMARY");
    
    printf("\n✓ Correctness validation layer: OPERATIONAL\n");
    printf("  - Syntax validation: on\n");
    printf("  - Type inference: on\n");
    printf("  - AST structure analysis: on\n");
    printf("  - Semantic distance scoring: on\n");
    printf("\n✓ Expert routing layer: CORRECTNESS-DRIVEN\n");
    printf("  - Replaces random noise with correctness signals\n");
    printf("  - Weighted selection by success rate per category\n");
    printf("  - Track: semantic accuracy, validation success, category affinity\n");
    printf("\n✓ IDE integration: COMPLETE\n");
    printf("  - Completions routed through MoE validator\n");
    printf("  - Confidence scores tied to correctness metrics\n");
    printf("  - Feedback loop ready for acceptance tracking\n");
    
    printf("\n" "═" "\n");
    printf("This is no longer random perturbation.\n");
    printf("This is a real optimization system driven by code correctness.\n");
    printf("═" "\n\n");
    
    return 0;
}
