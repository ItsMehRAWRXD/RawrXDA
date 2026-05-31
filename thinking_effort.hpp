/*
 * Thinking Effort Selection System
 * Pure C++ Implementation - No Dependencies
 * 
 * Levels:
 * 0 - OFF (no thinking, direct output)
 * 1 - LOW (minimal computation, fast responses)
 * 2 - MEDIUM (balanced approach, default)
 * 3 - HIGH (detailed analysis, more iterations)
 * 4 - EXTRA (exhaustive exploration, deep reasoning)
 * 5 - MAX (full depth, no limits, maximum computation)
 * 
 * Max lines: 3000
 */

#ifndef THINKING_EFFORT_HPP
#define THINKING_EFFORT_HPP

#include <cstdint>
#include <cstddef>
#include <cmath>
#include <algorithm>
#include <memory>
#include <vector>
#include <string>
#include <functional>
#include <chrono>
#include <atomic>
#include <mutex>
#include <unordered_map>

namespace ThinkingEffort {

// ============================================================================
// CONFIGURATION CONSTANTS
// ============================================================================

constexpr size_t MAX_ITERATIONS_OFF = 1;
constexpr size_t MAX_ITERATIONS_LOW = 10;
constexpr size_t MAX_ITERATIONS_MEDIUM = 100;
constexpr size_t MAX_ITERATIONS_HIGH = 1000;
constexpr size_t MAX_ITERATIONS_EXTRA = 10000;
constexpr size_t MAX_ITERATIONS_MAX = SIZE_MAX;

constexpr size_t MAX_TOKENS_OFF = 0;
constexpr size_t MAX_TOKENS_LOW = 100;
constexpr size_t MAX_TOKENS_MEDIUM = 500;
constexpr size_t MAX_TOKENS_HIGH = 2000;
constexpr size_t MAX_TOKENS_EXTRA = 8000;
constexpr size_t MAX_TOKENS_MAX = 32000;

constexpr double MAX_TIME_OFF_MS = 0.1;
constexpr double MAX_TIME_LOW_MS = 100.0;
constexpr double MAX_TIME_MEDIUM_MS = 1000.0;
constexpr double MAX_TIME_HIGH_MS = 5000.0;
constexpr double MAX_TIME_EXTRA_MS = 20000.0;
constexpr double MAX_TIME_MAX_MS = 300000.0;  // 5 minutes

constexpr size_t MAX_DEPTH_OFF = 0;
constexpr size_t MAX_DEPTH_LOW = 2;
constexpr size_t MAX_DEPTH_MEDIUM = 5;
constexpr size_t MAX_DEPTH_HIGH = 10;
constexpr size_t MAX_DEPTH_EXTRA = 20;
constexpr size_t MAX_DEPTH_MAX = 100;

constexpr size_t MAX_BRANCHING_OFF = 1;
constexpr size_t MAX_BRANCHING_LOW = 3;
constexpr size_t MAX_BRANCHING_MEDIUM = 10;
constexpr size_t MAX_BRANCHING_HIGH = 30;
constexpr size_t MAX_BRANCHING_EXTRA = 100;
constexpr size_t MAX_BRANCHING_MAX = 1000;

// ============================================================================
// THINKING LEVEL ENUMERATION
// ============================================================================

enum class Level : uint8_t {
    OFF = 0,
    LOW = 1,
    MEDIUM = 2,
    HIGH = 3,
    EXTRA = 4,
    MAX = 5
};

inline const char* levelToString(Level level) {
    switch (level) {
        case Level::OFF: return "OFF";
        case Level::LOW: return "LOW";
        case Level::MEDIUM: return "MEDIUM";
        case Level::HIGH: return "HIGH";
        case Level::EXTRA: return "EXTRA";
        case Level::MAX: return "MAX";
        default: return "UNKNOWN";
    }
}

inline Level stringToLevel(const std::string& str) {
    if (str == "OFF" || str == "off" || str == "0") return Level::OFF;
    if (str == "LOW" || str == "low" || str == "1") return Level::LOW;
    if (str == "MEDIUM" || str == "medium" || str == "2") return Level::MEDIUM;
    if (str == "HIGH" || str == "high" || str == "3") return Level::HIGH;
    if (str == "EXTRA" || str == "extra" || str == "4") return Level::EXTRA;
    if (str == "MAX" || str == "max" || str == "5") return Level::MAX;
    return Level::MEDIUM;  // Default
}

// ============================================================================
// RESOURCE BUDGET STRUCTURE
// ============================================================================

struct ResourceBudget {
    size_t maxIterations;
    size_t maxTokens;
    size_t maxDepth;
    size_t maxBranching;
    double maxTimeMs;
    size_t maxMemoryMB;
    double temperature;
    double explorationRate;
    bool enableParallelism;
    bool enableCaching;
    
    ResourceBudget()
        : maxIterations(100)
        , maxTokens(500)
        , maxDepth(5)
        , maxBranching(10)
        , maxTimeMs(1000.0)
        , maxMemoryMB(256)
        , temperature(0.7)
        , explorationRate(0.3)
        , enableParallelism(false)
        , enableCaching(true) {}
    
    static ResourceBudget fromLevel(Level level) {
        ResourceBudget budget;
        
        switch (level) {
            case Level::OFF:
                budget.maxIterations = MAX_ITERATIONS_OFF;
                budget.maxTokens = MAX_TOKENS_OFF;
                budget.maxDepth = MAX_DEPTH_OFF;
                budget.maxBranching = MAX_BRANCHING_OFF;
                budget.maxTimeMs = MAX_TIME_OFF_MS;
                budget.maxMemoryMB = 1;
                budget.temperature = 0.0;
                budget.explorationRate = 0.0;
                budget.enableParallelism = false;
                budget.enableCaching = false;
                break;
                
            case Level::LOW:
                budget.maxIterations = MAX_ITERATIONS_LOW;
                budget.maxTokens = MAX_TOKENS_LOW;
                budget.maxDepth = MAX_DEPTH_LOW;
                budget.maxBranching = MAX_BRANCHING_LOW;
                budget.maxTimeMs = MAX_TIME_LOW_MS;
                budget.maxMemoryMB = 16;
                budget.temperature = 0.3;
                budget.explorationRate = 0.1;
                budget.enableParallelism = false;
                budget.enableCaching = true;
                break;
                
            case Level::MEDIUM:
                budget.maxIterations = MAX_ITERATIONS_MEDIUM;
                budget.maxTokens = MAX_TOKENS_MEDIUM;
                budget.maxDepth = MAX_DEPTH_MEDIUM;
                budget.maxBranching = MAX_BRANCHING_MEDIUM;
                budget.maxTimeMs = MAX_TIME_MEDIUM_MS;
                budget.maxMemoryMB = 64;
                budget.temperature = 0.7;
                budget.explorationRate = 0.3;
                budget.enableParallelism = true;
                budget.enableCaching = true;
                break;
                
            case Level::HIGH:
                budget.maxIterations = MAX_ITERATIONS_HIGH;
                budget.maxTokens = MAX_TOKENS_HIGH;
                budget.maxDepth = MAX_DEPTH_HIGH;
                budget.maxBranching = MAX_BRANCHING_HIGH;
                budget.maxTimeMs = MAX_TIME_HIGH_MS;
                budget.maxMemoryMB = 256;
                budget.temperature = 0.9;
                budget.explorationRate = 0.5;
                budget.enableParallelism = true;
                budget.enableCaching = true;
                break;
                
            case Level::EXTRA:
                budget.maxIterations = MAX_ITERATIONS_EXTRA;
                budget.maxTokens = MAX_TOKENS_EXTRA;
                budget.maxDepth = MAX_DEPTH_EXTRA;
                budget.maxBranching = MAX_BRANCHING_EXTRA;
                budget.maxTimeMs = MAX_TIME_EXTRA_MS;
                budget.maxMemoryMB = 1024;
                budget.temperature = 1.0;
                budget.explorationRate = 0.7;
                budget.enableParallelism = true;
                budget.enableCaching = true;
                break;
                
            case Level::MAX:
                budget.maxIterations = MAX_ITERATIONS_MAX;
                budget.maxTokens = MAX_TOKENS_MAX;
                budget.maxDepth = MAX_DEPTH_MAX;
                budget.maxBranching = MAX_BRANCHING_MAX;
                budget.maxTimeMs = MAX_TIME_MAX_MS;
                budget.maxMemoryMB = 8192;  // 8GB
                budget.temperature = 1.2;
                budget.explorationRate = 1.0;
                budget.enableParallelism = true;
                budget.enableCaching = true;
                break;
        }
        
        return budget;
    }
};

// ============================================================================
// PERFORMANCE METRICS
// ============================================================================

struct PerformanceMetrics {
    size_t iterationsUsed;
    size_t tokensProcessed;
    size_t depthExplored;
    size_t branchesExplored;
    double timeUsedMs;
    size_t memoryUsedMB;
    double cacheHitRate;
    double efficiency;
    double quality;
    
    PerformanceMetrics()
        : iterationsUsed(0)
        , tokensProcessed(0)
        , depthExplored(0)
        , branchesExplored(0)
        , timeUsedMs(0.0)
        , memoryUsedMB(0)
        , cacheHitRate(0.0)
        , efficiency(0.0)
        , quality(0.0) {}
    
    double calculateEfficiency(const ResourceBudget& budget) {
        if (budget.maxIterations == 0 || budget.maxTimeMs == 0.0) {
            efficiency = 0.0;
            return 0.0;
        }
        
        double iterationRatio = static_cast<double>(iterationsUsed) / budget.maxIterations;
        double timeRatio = timeUsedMs / budget.maxTimeMs;
        double resourceUsage = (iterationRatio + timeRatio) / 2.0;
        
        efficiency = (resourceUsage > 0.0 && resourceUsage <= 1.0) 
                     ? quality / resourceUsage 
                     : 0.0;
        
        return efficiency;
    }
};

// ============================================================================
// THINKING CONTEXT
// ============================================================================

struct ThinkingContext {
    Level currentLevel;
    ResourceBudget budget;
    PerformanceMetrics metrics;
    size_t currentIteration;
    size_t currentDepth;
    std::vector<std::string> reasoningChain;
    std::vector<double> confidenceScores;
    std::unordered_map<std::string, double> contextVariables;
    std::string task;
    std::string currentGoal;
    bool shouldContinue;
    bool shouldBacktrack;
    
    ThinkingContext()
        : currentLevel(Level::MEDIUM)
        , currentIteration(0)
        , currentDepth(0)
        , shouldContinue(true)
        , shouldBacktrack(false) {}
    
    void reset() {
        currentIteration = 0;
        currentDepth = 0;
        reasoningChain.clear();
        confidenceScores.clear();
        contextVariables.clear();
        shouldContinue = true;
        shouldBacktrack = false;
        metrics = PerformanceMetrics();
    }
    
    void pushReasoning(const std::string& step, double confidence = 0.5) {
        reasoningChain.push_back(step);
        confidenceScores.push_back(confidence);
    }
    
    std::string getReasoningChain() const {
        std::string result;
        for (size_t i = 0; i < reasoningChain.size(); i++) {
            result += std::to_string(i + 1) + ". " + reasoningChain[i];
            if (i < confidenceScores.size()) {
                result += " (confidence: " + std::to_string(confidenceScores[i]) + ")";
            }
            result += "\n";
        }
        return result;
    }
};

// ============================================================================
// THINKING NODE (for tree-based reasoning)
// ============================================================================

struct ThinkingNode : public std::enable_shared_from_this<ThinkingNode> {
    std::string thought;
    double score;
    double confidence;
    size_t depth;
    std::vector<std::shared_ptr<ThinkingNode>> children;
    std::shared_ptr<ThinkingNode> parent;
    bool isExplored;
    bool isPruned;
    std::unordered_map<std::string, std::string> metadata;
    
    ThinkingNode(const std::string& t = "", double s = 0.0, double c = 0.0)
        : thought(t), score(s), confidence(c), depth(0)
        , isExplored(false), isPruned(false) {}
    
    void addChild(const std::shared_ptr<ThinkingNode>& child) {
        child->parent = shared_from_this();
        child->depth = depth + 1;
        children.push_back(child);
    }
    
    size_t getTotalNodes() const {
        size_t count = 1;
        for (const auto& child : children) {
            if (!child->isPruned) {
                count += child->getTotalNodes();
            }
        }
        return count;
    }
    
    void prune() {
        isPruned = true;
        for (auto& child : children) {
            child->prune();
        }
    }
    
    std::vector<std::shared_ptr<ThinkingNode>> getBestPath() {
        std::vector<std::shared_ptr<ThinkingNode>> path;
        auto current = shared_from_this();
        
        while (current) {
            if (!current->isPruned) {
                path.push_back(current);
            }
            
            // Find best child
            double bestScore = -1.0;
            std::shared_ptr<ThinkingNode> bestChild = nullptr;
            
            for (const auto& child : current->children) {
                if (!child->isPruned && child->score > bestScore) {
                    bestScore = child->score;
                    bestChild = child;
                }
            }
            
            current = bestChild;
        }
        
        return path;
    }
};

// ============================================================================
// ADAPTIVE THINKING CONTROLLER
// ============================================================================

class AdaptiveThinkingController {
private:
    Level currentLevel;
    Level minLevel;
    Level maxLevel;
    ThinkingContext context;
    std::unordered_map<std::string, PerformanceMetrics> historicalMetrics;
    bool enableAdaptation;
    double adaptationThreshold;
    std::mutex metricsMutex;
    
public:
    AdaptiveThinkingController(Level level = Level::MEDIUM)
        : currentLevel(level)
        , minLevel(Level::OFF)
        , maxLevel(Level::MAX)
        , enableAdaptation(true)
        , adaptationThreshold(0.1) {
        
        context.currentLevel = currentLevel;
        context.budget = ResourceBudget::fromLevel(currentLevel);
    }
    
    void setLevel(Level level) {
        std::lock_guard<std::mutex> lock(metricsMutex);
        currentLevel = level;
        context.currentLevel = level;
        context.budget = ResourceBudget::fromLevel(level);
    }
    
    void setLevelRange(Level min, Level max) {
        minLevel = min;
        maxLevel = max;
    }
    
    void setAdaptation(bool enable, double threshold = 0.1) {
        enableAdaptation = enable;
        adaptationThreshold = threshold;
    }
    
    Level getCurrentLevel() const {
        return currentLevel;
    }
    
    const ThinkingContext& getContext() const {
        return context;
    }
    
    const ResourceBudget& getBudget() const {
        return context.budget;
    }
    
    PerformanceMetrics getMetrics() const {
        return context.metrics;
    }
    
    void recordMetrics(const std::string& taskKey, const PerformanceMetrics& metrics) {
        std::lock_guard<std::mutex> lock(metricsMutex);
        historicalMetrics[taskKey] = metrics;
    }
    
    PerformanceMetrics getHistoricalMetrics(const std::string& taskKey) const {
        auto it = historicalMetrics.find(taskKey);
        if (it != historicalMetrics.end()) {
            return it->second;
        }
        return PerformanceMetrics();
    }
    
    void adaptLevel() {
        if (!enableAdaptation) return;
        
        std::lock_guard<std::mutex> lock(metricsMutex);
        double efficiency = context.metrics.efficiency;
        
        if (efficiency < adaptationThreshold) {
            // Performance poor - might need higher level
            if (currentLevel < maxLevel) {
                currentLevel = static_cast<Level>(static_cast<int>(currentLevel) + 1);
                context.budget = ResourceBudget::fromLevel(currentLevel);
            }
        } else if (efficiency > (1.0 - adaptationThreshold)) {
            // Performance excellent - could try lower level
            if (currentLevel > minLevel) {
                currentLevel = static_cast<Level>(static_cast<int>(currentLevel) - 1);
                context.budget = ResourceBudget::fromLevel(currentLevel);
            }
        }
    }
    
    void reset() {
        context.reset();
    }
    
    void pushReasoning(const std::string& step, double confidence = 0.5) {
        context.pushReasoning(step, confidence);
    }
    
    void setTask(const std::string& t) {
        context.task = t;
        context.currentGoal = t;
    }
    
    bool shouldContinue() const {
        if (context.currentIteration >= context.budget.maxIterations) return false;
        if (context.currentDepth >= context.budget.maxDepth) return false;
        
        // Check if we've found a good enough answer
        if (!context.confidenceScores.empty()) {
            double lastScore = context.confidenceScores.back();
            if (lastScore >= 0.95) return false;
        }
        
        return true;
    }
    
    std::string getReasoningChain() const {
        return context.getReasoningChain();
    }
};

// ============================================================================
// TASK COMPLEXITY ESTIMATOR
// ============================================================================

class TaskComplexityEstimator {
private:
    std::unordered_map<std::string, double> taskComplexityCache;
    
public:
    double estimateComplexity(const std::string& task) {
        // Check cache first
        auto it = taskComplexityCache.find(task);
        if (it != taskComplexityCache.end()) {
            return it->second;
        }
        
        double complexity = 0.5;  // Default medium complexity
        
        // Simple heuristics for complexity estimation
        if (task.find("analyze") != std::string::npos ||
            task.find("explain") != std::string::npos) {
            complexity = 0.7;
        } else if (task.find("implement") != std::string::npos ||
                   task.find("create") != std::string::npos) {
            complexity = 0.8;
        } else if (task.find("optimize") != std::string::npos ||
                   task.find("refactor") != std::string::npos) {
            complexity = 0.9;
        } else if (task.find("debug") != std::string::npos ||
                   task.find("fix") != std::string::npos) {
            complexity = 0.85;
        } else if (task.find("list") != std::string::npos ||
                   task.find("show") != std::string::npos) {
            complexity = 0.3;
        } else if (task.find("help") != std::string::npos ||
                   task.find("what") != std::string::npos) {
            complexity = 0.4;
        }
        
        // Cache the result
        taskComplexityCache[task] = complexity;
        
        return complexity;
    }
    
    Level getRecommendedLevel(double complexity, double importance = 0.5) {
        // complexity: 0.0 (simple) to 1.0 (complex)
        // importance: 0.0 (trivial) to 1.0 (critical)
        
        double combinedScore = (complexity * 0.6 + importance * 0.4);
        
        if (combinedScore < 0.1) return Level::OFF;
        if (combinedScore < 0.25) return Level::LOW;
        if (combinedScore < 0.5) return Level::MEDIUM;
        if (combinedScore < 0.75) return Level::HIGH;
        if (combinedScore < 0.9) return Level::EXTRA;
        return Level::MAX;
    }
    
    void clearCache() {
        taskComplexityCache.clear();
    }
};

// ============================================================================
// UTILITY FUNCTIONS
// ============================================================================

inline double calculateConfidenceInterval(const std::vector<double>& scores) {
    if (scores.empty()) return 0.0;
    
    double mean = 0.0;
    for (double s : scores) {
        mean += s;
    }
    mean /= scores.size();
    
    double variance = 0.0;
    for (double s : scores) {
        double diff = s - mean;
        variance += diff * diff;
    }
    variance /= scores.size();
    
    return std::sqrt(variance);
}

inline std::string getThinkingStatistics(const ThinkingContext& ctx) {
    std::string stats;
    stats += "Level: " + std::string(levelToString(ctx.currentLevel)) + "\n";
    stats += "Iterations: " + std::to_string(ctx.currentIteration) + 
             " / " + std::to_string(ctx.budget.maxIterations) + "\n";
    stats += "Depth: " + std::to_string(ctx.currentDepth) + 
             " / " + std::to_string(ctx.budget.maxDepth) + "\n";
    stats += "Time: " + std::to_string(ctx.metrics.timeUsedMs) + "ms / " + 
             std::to_string(ctx.budget.maxTimeMs) + "ms\n";
    stats += "Quality: " + std::to_string(ctx.metrics.quality) + "\n";
    stats += "Efficiency: " + std::to_string(ctx.metrics.efficiency) + "\n";
    stats += "Reasoning Steps: " + std::to_string(ctx.reasoningChain.size()) + "\n";
    
    if (!ctx.confidenceScores.empty()) {
        stats += "Confidence Interval: " + 
                 std::to_string(calculateConfidenceInterval(ctx.confidenceScores)) + "\n";
    }
    
    return stats;
}

// ============================================================================
// C INTERFACE FOR SOVEREIGN IDE INTEGRATION
// ============================================================================

extern "C" {
    // Simple C interface for integration with sovereign.c
    typedef struct {
        void* controller;
        int currentLevel;
        size_t maxIterations;
        size_t maxTokens;
        double maxTimeMs;
    } ThinkingController;
    
    ThinkingController* thinking_create(int level) {
        ThinkingController* tc = (ThinkingController*)malloc(sizeof(ThinkingController));
        if (!tc) return NULL;
        
        auto controller = new AdaptiveThinkingController(static_cast<Level>(level));
        tc->controller = controller;
        tc->currentLevel = level;
        tc->maxIterations = controller->getBudget().maxIterations;
        tc->maxTokens = controller->getBudget().maxTokens;
        tc->maxTimeMs = controller->getBudget().maxTimeMs;
        
        return tc;
    }
    
    void thinking_destroy(ThinkingController* tc) {
        if (!tc) return;
        delete static_cast<AdaptiveThinkingController*>(tc->controller);
        free(tc);
    }
    
    void thinking_set_level(ThinkingController* tc, int level) {
        if (!tc || level < 0 || level > 5) return;
        auto controller = static_cast<AdaptiveThinkingController*>(tc->controller);
        controller->setLevel(static_cast<Level>(level));
        tc->currentLevel = level;
        tc->maxIterations = controller->getBudget().maxIterations;
        tc->maxTokens = controller->getBudget().maxTokens;
        tc->maxTimeMs = controller->getBudget().maxTimeMs;
    }
    
    int thinking_should_continue(ThinkingController* tc) {
        if (!tc) return 0;
        auto controller = static_cast<AdaptiveThinkingController*>(tc->controller);
        return controller->shouldContinue() ? 1 : 0;
    }
    
    void thinking_push_reasoning(ThinkingController* tc, const char* step, double confidence) {
        if (!tc || !step) return;
        auto controller = static_cast<AdaptiveThinkingController*>(tc->controller);
        controller->pushReasoning(std::string(step), confidence);
    }
    
    double thinking_estimate_complexity(const char* task) {
        if (!task) return 0.5;
        static TaskComplexityEstimator estimator;
        return estimator.estimateComplexity(std::string(task));
    }
    
    int thinking_get_recommended_level(const char* task, double importance) {
        if (!task) return 2; // MEDIUM
        static TaskComplexityEstimator estimator;
        double complexity = estimator.estimateComplexity(std::string(task));
        Level level = estimator.getRecommendedLevel(complexity, importance);
        return static_cast<int>(level);
    }
}

// ============================================================================
// DEMO / TEST FUNCTION
// ============================================================================

inline void demonstrateThinkingEffort() {
    printf("=== Thinking Effort Selection System Demo ===\n\n");
    
    // Create controller with MEDIUM level
    AdaptiveThinkingController controller(Level::MEDIUM);
    
    TaskComplexityEstimator estimator;
    
    // Example tasks
    std::vector<std::string> tasks = {
        "What is 2 + 2?",                                    // Simple
        "List the files in this directory",                  // Simple
        "Explain how quicksort works",                        // Medium
        "Implement a binary search tree",                     // High
        "Optimize this algorithm for better performance",     // Extra
        "Design a distributed system for handling 1M requests" // Max
    };
    
    std::vector<double> importance = {0.1, 0.2, 0.5, 0.7, 0.9, 1.0};
    
    for (size_t i = 0; i < tasks.size(); i++) {
        double complexity = estimator.estimateComplexity(tasks[i]);
        Level recommendedLevel = estimator.getRecommendedLevel(complexity, importance[i]);
        
        printf("Task: %s\n", tasks[i].c_str());
        printf("  Complexity: %.2f\n", complexity);
        printf("  Importance: %.2f\n", importance[i]);
        printf("  Recommended Level: %s\n", levelToString(recommendedLevel));
        printf("  Budget: %zu iterations, %zu tokens, %.0f ms\n\n",
               ResourceBudget::fromLevel(recommendedLevel).maxIterations,
               ResourceBudget::fromLevel(recommendedLevel).maxTokens,
               ResourceBudget::fromLevel(recommendedLevel).maxTimeMs);
    }
    
    printf("=== Adaptive Thinking Demo ===\n\n");
    
    // Demonstrate adaptive thinking
    for (int level = 0; level <= 5; level++) {
        Level lvl = static_cast<Level>(level);
        controller.setLevel(lvl);
        
        printf("Level: %s\n", levelToString(lvl));
        printf("  Max Iterations: %zu\n", controller.getBudget().maxIterations);
        printf("  Max Tokens: %zu\n", controller.getBudget().maxTokens);
        printf("  Max Depth: %zu\n", controller.getBudget().maxDepth);
        printf("  Max Time: %.0f ms\n\n", controller.getBudget().maxTimeMs);
    }
}

} // namespace ThinkingEffort

#endif // THINKING_EFFORT_HPP