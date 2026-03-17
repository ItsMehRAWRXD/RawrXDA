#pragma once

#include <string>
#include <vector>
#include <filesystem>
#include <memory>
#include <mutex>
#include <chrono>
#include <map>
#include <functional>
#include <atomic>
#include <future>
#include <ctime>

namespace AutoModelLoader {

/**
 * @brief Log levels for structured logging
 */
enum class LogLevel {
    LOG_DEBUG,
    LOG_INFO,
    LOG_WARN,
    LOG_ERROR,
    LOG_FATAL
};

/**
 * @brief Model metadata structure
 */
struct ModelMetadata {
    std::string path;
    std::string name;
    std::string fullPath;
    std::string modelName;
    std::string architecture;
    size_t sizeBytes;
    std::string sha256Hash;
    std::chrono::system_clock::time_point lastVerified;
    bool isHealthy;
    int failureCount;
    std::string vendor; // ollama, local, huggingface, custom, etc.
    std::string modelType; // "ollama", "gguf", "custom", "github"
    bool isCustomModel = false;
    std::string customModelId; // For custom-built models
    std::time_t lastModified = 0;
    bool isAvailable = false;
};

/**
 * @brief Configuration structure for model loader
 */
struct LoaderConfig {
    bool autoLoadEnabled = true;
    std::string preferredModel;
    std::vector<std::string> searchPaths;
    int maxRetries = 3;
    int retryDelayMs = 1000;
    int discoveryTimeoutMs = 30000;
    int loadTimeoutMs = 60000;
    bool enableCaching = true;
    bool enableHealthChecks = true;
    bool enableMetrics = true;
    bool enableAsyncLoading = true;
    size_t maxCacheSize = 10;
    std::string logLevel = "INFO";
    std::string configFilePath = "./model_loader_config.json";
    
    // AI features
    bool enableAISelection = true;
    bool enableGitHubCopilot = true;
    bool enablePredictivePreloading = false;
    bool enableCustomModels = true;
    bool enableGitHubIntegration = true;
    std::string customModelsDirectory = "./custom_models";
    std::string githubToken = "";  // GitHub Personal Access Token
    std::string githubOrg = "";    // GitHub Organization/User
    
    // Circuit breaker settings
    int circuitBreakerThreshold = 5;
    int circuitBreakerTimeoutMs = 60000;
    
    // Feature flags
    bool enableFuzzing = false;
    bool enableDistributedTracing = false;
    bool enablePrometheusMetrics = false;
};

/**
 * @brief Performance metrics for monitoring
 */
struct PerformanceMetrics {
    std::atomic<uint64_t> totalDiscoveryCalls{0};
    std::atomic<uint64_t> totalLoadCalls{0};
    std::atomic<uint64_t> successfulLoads{0};
    std::atomic<uint64_t> failedLoads{0};
    std::atomic<uint64_t> cacheHits{0};
    std::atomic<uint64_t> cacheMisses{0};
    
    // Latency histograms (microseconds)
    std::vector<uint64_t> discoveryLatencies;
    std::vector<uint64_t> loadLatencies;
    std::vector<uint64_t> selectionLatencies;
    
    mutable std::mutex metricsLock;
    
    void recordDiscoveryLatency(uint64_t micros);
    void recordLoadLatency(uint64_t micros);
    void recordSelectionLatency(uint64_t micros);
    std::string generatePrometheusMetrics() const;
};

/**
 * @brief Circuit breaker for fault tolerance
 */
class CircuitBreaker {
public:
    enum class State {
        CLOSED,  // Normal operation
        OPEN,    // Circuit broken, reject calls
        HALF_OPEN // Testing if service recovered
    };
    
    CircuitBreaker(int threshold, int timeoutMs);
    bool allowRequest();
    void recordSuccess();
    void recordFailure();
    State getState() const { return m_state; }
    
private:
    std::atomic<State> m_state{State::CLOSED};
    std::atomic<int> m_failureCount{0};
    std::atomic<int> m_successCount{0};
    int m_threshold;
    int m_timeoutMs;
    std::chrono::system_clock::time_point m_lastFailureTime;
    mutable std::mutex m_mutex;
};

/**
 * @brief Usage pattern for predictive preloading
 */
struct UsagePattern {
    std::string modelName;
    std::string projectType;
    std::chrono::system_clock::time_point timestamp;
    int hourOfDay;
    int dayOfWeek;
    std::string userContext; // e.g., "debugging", "coding", "refactoring"
    int usageCount;
    double averageSessionDuration; // in minutes
};

/**
 * @brief Predictive preloading based on usage patterns
 */
class UsagePatternTracker {
public:
    static UsagePatternTracker& GetInstance();
    
    void recordUsage(const std::string& modelName, const std::string& projectType, 
                    const std::string& userContext);
    std::vector<std::string> predictNextModels(int count = 3);
    void learnFromHistory();
    bool shouldPreload(const std::string& modelName) const;
    void savePatterns(const std::string& filepath);
    void loadPatterns(const std::string& filepath);
    std::vector<UsagePattern> getPatterns() const;
    
private:
    UsagePatternTracker() = default;
    std::vector<UsagePattern> m_patterns;
    std::map<std::string, double> m_modelScores;
    mutable std::mutex m_patternMutex;
    
    double calculatePredictionScore(const UsagePattern& pattern) const;
    std::string getCurrentUserContext() const;
};

/**
 * @brief Model ensemble configuration
 */
struct EnsembleConfig {
    std::string name;
    std::vector<std::string> models;
    std::map<std::string, double> weights; // model -> weight
    std::string votingStrategy; // "weighted", "unanimous", "majority"
    bool enableFallback;
    int maxParallelLoads;
    int loadBalancingStrategy; // 0=round-robin, 1=least-loaded, 2=weighted
};

/**
 * @brief Multi-model ensemble support
 */
class ModelEnsemble {
public:
    ModelEnsemble(const EnsembleConfig& config);
    
    bool loadAllModels();
    bool loadModelsAsync(std::function<void(bool, const std::string&)> callback);
    std::vector<std::string> getLoadedModels() const;
    std::string selectModelForTask(const std::string& taskType);
    bool unloadModel(const std::string& modelName);
    bool reloadModel(const std::string& modelName);
    
    // Voting and aggregation
    std::string weightedVote(const std::map<std::string, std::string>& modelOutputs);
    bool isFullyLoaded() const;
    double getEnsembleHealth() const;
    
    EnsembleConfig getConfig() const { return m_config; }
    
private:
    EnsembleConfig m_config;
    std::map<std::string, bool> m_modelStatus; // model -> loaded
    std::map<std::string, int> m_modelLoadCount; // for load balancing
    mutable std::mutex m_ensembleMutex;
    
    std::string selectRoundRobin();
    std::string selectLeastLoaded();
    std::string selectWeighted();
};

/**
 * @brief A/B test variant configuration
 */
struct ABTestVariant {
    std::string variantId;
    std::string modelName;
    double trafficPercentage; // 0.0 - 1.0
    std::map<std::string, std::string> parameters;
};

/**
 * @brief A/B test metrics
 */
struct ABTestMetrics {
    std::string variantId;
    int requestCount;
    int successCount;
    int failureCount;
    double averageLatencyMs;
    double p50LatencyMs;
    double p99LatencyMs;
    std::chrono::system_clock::time_point startTime;
    std::chrono::system_clock::time_point endTime;
};

/**
 * @brief A/B Testing framework for model comparison
 */
class ABTestingFramework {
public:
    static ABTestingFramework& GetInstance();
    
    // Test configuration
    std::string createTest(const std::string& testName, 
                          const std::vector<ABTestVariant>& variants);
    bool startTest(const std::string& testId);
    bool stopTest(const std::string& testId);
    bool deleteTest(const std::string& testId);
    
    // Variant assignment
    std::string assignVariant(const std::string& testId, const std::string& userId);
    ABTestVariant getVariant(const std::string& testId, const std::string& variantId);
    
    // Metrics collection
    void recordRequest(const std::string& testId, const std::string& variantId, 
                      bool success, double latencyMs);
    ABTestMetrics getMetrics(const std::string& testId, const std::string& variantId);
    std::map<std::string, ABTestMetrics> getAllMetrics(const std::string& testId);
    
    // Statistical analysis
    bool hasStatisticalSignificance(const std::string& testId, 
                                   const std::string& variantA, 
                                   const std::string& variantB,
                                   double confidenceLevel = 0.95);
    std::string getWinningVariant(const std::string& testId);
    std::string generateReport(const std::string& testId);
    
    // Persistence
    void saveTests(const std::string& filepath);
    void loadTests(const std::string& filepath);
    
private:
    ABTestingFramework() = default;
    
    struct TestConfig {
        std::string testId;
        std::string testName;
        std::vector<ABTestVariant> variants;
        bool isActive;
        std::chrono::system_clock::time_point createdAt;
    };
    
    std::map<std::string, TestConfig> m_tests;
    std::map<std::string, std::map<std::string, ABTestMetrics>> m_metrics; // testId -> variantId -> metrics
    std::map<std::string, std::map<std::string, std::string>> m_assignments; // testId -> userId -> variantId
    mutable std::mutex m_testMutex;
    
    double calculateZScore(const ABTestMetrics& a, const ABTestMetrics& b);
    std::string hashUserId(const std::string& userId, const std::string& testId);
};

/**
 * @brief Inferred model capabilities
 */
struct InferredCapabilities {
    std::string modelType; // "text", "code", "multimodal", "embedding", "unknown"
    std::vector<std::string> languages; // programming languages or natural languages
    std::vector<std::string> tasks; // "completion", "chat", "analysis", "generation"
    double confidenceScore; // 0.0 - 1.0
    std::string inferenceMethod; // how capabilities were inferred
    std::map<std::string, double> capabilityScores; // capability -> score
};

/**
 * @brief Zero-shot learning for unknown model types
 */
class ZeroShotHandler {
public:
    static ZeroShotHandler& GetInstance();
    
    // Capability inference
    InferredCapabilities inferCapabilities(const std::string& modelPath);
    InferredCapabilities inferFromMetadata(const ModelMetadata& metadata);
    InferredCapabilities inferFromProbing(const std::string& modelPath);
    
    // Fallback strategies
    bool handleUnknownModel(const std::string& modelPath);
    std::vector<std::string> suggestSimilarModels(const std::string& modelPath);
    std::string selectFallbackModel(const std::string& failedModel);
    
    // Learning from feedback
    void recordSuccess(const std::string& modelPath, const std::string& taskType);
    void recordFailure(const std::string& modelPath, const std::string& taskType, 
                      const std::string& errorReason);
    void updateInferredCapabilities(const std::string& modelPath, 
                                   const InferredCapabilities& capabilities);
    
    // Knowledge base
    void saveKnowledgeBase(const std::string& filepath);
    void loadKnowledgeBase(const std::string& filepath);
    std::map<std::string, InferredCapabilities> getKnowledgeBase() const;
    
private:
    ZeroShotHandler() = default;
    
    std::map<std::string, InferredCapabilities> m_knowledgeBase;
    std::map<std::string, std::vector<std::pair<std::string, bool>>> m_executionHistory; // model -> (task, success)
    mutable std::mutex m_knowledgeMutex;
    
    double calculateSimilarity(const std::string& modelA, const std::string& modelB);
    std::vector<std::string> extractFeaturesFromPath(const std::string& modelPath);
    std::vector<std::string> extractFeaturesFromMetadata(const ModelMetadata& metadata);
    double scoreFallbackCandidate(const std::string& candidate, const std::string& failed);
};

/**
 * @brief Enterprise-grade automatic model loading system for RawrXD IDEs
 * 
 * Production-ready features:
 * - Structured logging with latency tracking
 * - External configuration management
 * - Comprehensive error handling with retry logic
 * - Model metadata caching and health checks
 * - Thread-safe operations with async support
 * - Prometheus-compatible metrics export
 * - Circuit breaker pattern for fault tolerance
 * - SHA256 model verification
 * - Distributed tracing support (OpenTelemetry compatible)
 */
class AutoModelLoader {
public:
    static AutoModelLoader& GetInstance();
    
    // Model discovery and selection
    std::vector<std::string> discoverAvailableModels();
    std::string selectOptimalModel(const std::vector<std::string>& availableModels);
    
    // Automatic loading with retry logic
    bool autoLoadModel();
    bool loadModel(const std::string& modelPath);
    bool loadModelAsync(const std::string& modelPath, std::function<void(bool)> callback);
    
    // Status and information
    bool isModelLoaded() const { return m_modelLoaded; }
    std::string getLoadedModel() const { return m_loadedModelPath; }
    std::string getStatus() const;
    std::string getDetailedStatus() const;
    
    // Configuration
    void setAutoLoadEnabled(bool enabled) { m_config.autoLoadEnabled = enabled; }
    bool isAutoLoadEnabled() const { return m_config.autoLoadEnabled; }
    void setPreferredModel(const std::string& model) { m_config.preferredModel = model; }
    bool loadConfiguration(const std::string& configPath);
    bool saveConfiguration(const std::string& configPath);
    LoaderConfig& getConfig() { return m_config; }
    
    // Health and metrics
    bool performHealthCheck(const std::string& modelPath);
    bool validateModel(const std::string& modelPath);
    std::string computeModelHash(const std::string& modelPath);
    PerformanceMetrics& getMetrics() { return m_metrics; }
    std::string exportMetrics() const;
    
    // Cache management
    void clearCache();
    void preloadModels(const std::vector<std::string>& modelPaths);
    std::vector<ModelMetadata> getCachedModels() const;
    
    AutoModelLoader();
    ~AutoModelLoader() = default;
    
    // AI-powered selection
    std::string selectOptimalModelAI(const std::vector<std::string>& availableModels, 
                                    const std::string& projectType);
    std::string detectProjectType();
    
    // Expose helper methods for advanced features
    ModelMetadata getModelMetadata(const std::string& modelPath);
    size_t getModelSize(const std::string& modelPath);
    std::string getModelArchitecture(const std::string& modelPath);
    void log(LogLevel level, const std::string& message, 
             const std::map<std::string, std::string>& context = {});
    
    // GitHub Copilot integration
    bool isGitHubCopilotAvailable() const;
    std::vector<std::string> getGitHubCopilotRecommendations();
    
    // Predictive preloading
    void enablePredictivePreloading(bool enable);
    std::vector<std::string> getPredictedModels(int count = 3);
    void recordModelUsage(const std::string& modelName, const std::string& context);
    
    // Multi-model ensemble
    bool createEnsemble(const std::string& name, const EnsembleConfig& config);
    std::shared_ptr<ModelEnsemble> getEnsemble(const std::string& name);
    bool loadEnsemble(const std::string& name);
    std::vector<std::string> listEnsembles() const;
    
    // A/B testing
    std::string createABTest(const std::string& testName, 
                            const std::vector<ABTestVariant>& variants);
    bool startABTest(const std::string& testId);
    std::string getABTestReport(const std::string& testId);
    
    // Zero-shot learning
    bool handleUnknownModelType(const std::string& modelPath);
    InferredCapabilities inferModelCapabilities(const std::string& modelPath);
    std::string suggestFallbackModel(const std::string& failedModel);
    
    // Custom model integration
    bool registerCustomModel(const std::string& modelName, const std::string& modelPath);
    bool unregisterCustomModel(const std::string& modelName);
    std::vector<std::string> listCustomModels() const;
    bool isCustomModel(const std::string& modelPath) const;
    std::string getCustomModelPath(const std::string& modelName) const;
    bool loadCustomModel(const std::string& modelName);
    bool syncCustomModelsRegistry();
    
    // GitHub integration (real scaffolding)
    bool authenticateGitHub(const std::string& token);
    bool isGitHubAuthenticated() const;
    std::vector<std::string> fetchGitHubModelRepos(const std::string& org);
    bool cloneModelFromGitHub(const std::string& repoUrl, const std::string& localPath);
    bool pushCustomModelToGitHub(const std::string& modelPath, const std::string& repoName);
    bool syncWithGitHubRegistry();
    std::string getGitHubModelMetadata(const std::string& repoUrl);
    bool downloadGitHubModel(const std::string& repoUrl);
    std::vector<std::string> listGitHubModels() const;
    
    // Delete copy constructor and assignment
    AutoModelLoader(const AutoModelLoader&) = delete;
    AutoModelLoader& operator=(const AutoModelLoader&) = delete;
    
private:
    // Model discovery helpers
    std::vector<std::string> scanModelDirectories();
    std::vector<std::string> scanOllamaModels();
    std::string resolveOllamaModelPath(const std::string& modelName);
    std::vector<std::string> scanCustomModels();
    std::vector<std::string> scanGitHubModels();
    
    // Model selection logic with enhanced algorithms
    std::string selectModelByCapability(const std::vector<std::string>& models);
    std::string selectModelBySize(const std::vector<std::string>& models);
    std::string selectModelByName(const std::vector<std::string>& models);
    std::string selectModelByHealth(const std::vector<std::string>& models);
    
    // Model metadata helpers
    std::string extractVendor(const std::string& modelPath);
    LogLevel parseLogLevel(const std::string& level);
    std::string logLevelToString(LogLevel level) const;
    
    // Error handling with retry
    template<typename Func>
    bool retryOperation(Func operation, const std::string& operationName);
    
    // Cache operations
    void updateCache(const ModelMetadata& metadata);
    ModelMetadata* getCachedMetadata(const std::string& modelPath);
    void evictOldestCacheEntry();
    
    // Thread safety
    mutable std::mutex m_instanceMutex;
    mutable std::mutex m_cacheMutex;
    mutable std::mutex m_configMutex;
    
    // State
    std::atomic<bool> m_modelLoaded{false};
    std::string m_loadedModelPath;
    LoaderConfig m_config;
    
    // Cache and metrics
    std::map<std::string, ModelMetadata> m_modelCache;
    std::vector<ModelMetadata> m_discoveredModels;
    PerformanceMetrics m_metrics;
    
    // Circuit breaker for fault tolerance
    std::unique_ptr<CircuitBreaker> m_circuitBreaker;
    
    // Advanced features
    std::map<std::string, std::shared_ptr<ModelEnsemble>> m_ensembles;
    mutable std::mutex m_ensembleMutex;
    
    // Custom model registry
    std::map<std::string, std::string> m_customModelRegistry; // name -> path
    mutable std::mutex m_customModelMutex;
    
    // GitHub integration state
    std::string m_githubToken;
    bool m_githubAuthenticated = false;
    std::map<std::string, std::string> m_githubModelCache; // repoUrl -> localPath
    mutable std::mutex m_githubMutex;
    
    // Singleton instance
    static std::unique_ptr<AutoModelLoader> s_instance;
    static std::mutex s_singletonMutex;
};

/**
 * @brief CLI integration for automatic model loading
 */
class CLIAutoLoader {
public:
    static void initialize();
    static void initializeWithConfig(const std::string& configPath);
    static void autoLoadOnStartup();
    static bool isAutoLoadEnabled();
    static std::string getStatus();
    static void shutdown();
};

/**
 * @brief Qt IDE integration for automatic model loading
 */
class QtIDEAutoLoader {
public:
    static void initialize();
    static void initializeWithConfig(const std::string& configPath);
    static void autoLoadOnStartup();
    static bool isAutoLoadEnabled();
    static std::string getStatus();
    static void shutdown();
};

} // namespace AutoModelLoader