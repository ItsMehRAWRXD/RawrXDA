// model_interface.h - Unified Interface for All Model Types (Local + Cloud)
#ifndef MODEL_INTERFACE_H
#define MODEL_INTERFACE_H


#include <memory>
#include <functional>
#include <vector>
#include "cpu_inference_engine.h"

class UniversalModelRouter;

class CloudApiClient;
struct ModelConfig;  // Defined in universal_model_router.h

// Generation options
struct GenerationOptions {
    int max_tokens = 2048;
    double temperature = 0.7;
    double top_p = 0.9;
    int top_k = 50;
    double frequency_penalty = 0.0;
    double presence_penalty = 0.0;
    bool stream = false;
    std::map<std::string, std::string> additional_params;
};

// Generation result
struct GenerationResult {
    std::string content;
    std::string model_name;
    std::string backend;
    int tokens_used = 0;
    double latency_ms = 0.0;
    bool success = false;
    std::string error;
    void* metadata;
};

// Unified Model Interface
class ModelInterface {

public:
    explicit ModelInterface(void* parent = nullptr);
    ~ModelInterface();

    // Initialization
    void initialize(const std::string& config_file_path);
    void initializeWithRouter(std::shared_ptr<UniversalModelRouter> router);
    bool isInitialized() const;
    
    // =========== SYNCHRONOUS INTERFACE ===========
    // Generate completion (blocking)
    GenerationResult generate(const std::string& prompt,
                             const std::string& model_name,
                             const GenerationOptions& options = GenerationOptions());
    
    // =========== ASYNCHRONOUS INTERFACE ===========
    // Generate completion (non-blocking)
    void generateAsync(const std::string& prompt,
                      const std::string& model_name,
                      std::function<void(const GenerationResult&)> callback,
                      const GenerationOptions& options = GenerationOptions());
    
    // =========== STREAMING INTERFACE ===========
    // Stream completion in real-time
    void generateStream(const std::string& prompt,
                       const std::string& model_name,
                       std::function<void(const std::string&)> on_chunk,
                       std::function<void(const std::string&)> on_error = nullptr,
                       const GenerationOptions& options = GenerationOptions());
    
    // =========== BATCH OPERATIONS ===========
    // Generate for multiple prompts
    std::vector<GenerationResult> generateBatch(const std::vector<std::string>& prompts,
                                           const std::string& model_name,
                                           const GenerationOptions& options = GenerationOptions());
    
    void generateBatchAsync(const std::vector<std::string>& prompts,
                           const std::string& model_name,
                           std::function<void(const std::vector<GenerationResult>&)> callback,
                           const GenerationOptions& options = GenerationOptions());
    
    // =========== MODEL MANAGEMENT ===========
    // Get available models
    std::vector<std::string> getAvailableModels() const;
    std::vector<std::string> getLocalModels() const;
    std::vector<std::string> getCloudModels() const;
    
    // Model information
    std::string getModelDescription(const std::string& model_name) const;
    void* getModelInfo(const std::string& model_name) const;
    bool modelExists(const std::string& model_name) const;
    
    // Register custom model
    void registerModel(const std::string& model_name, const ModelConfig& config);
    void unregisterModel(const std::string& model_name);
    
    // =========== SMART ROUTING ===========
    // Automatic model selection based on task
    std::string selectBestModel(const std::string& task_type,
                           const std::string& language = "cpp",
                           bool prefer_local = false);
    
    // Cost-aware routing
    std::string selectCostOptimalModel(const std::string& prompt,
                                  double max_cost_usd = 1.0);
    
    // Performance-aware routing
    std::string selectFastestModel(const std::string& model_type = "general");
    
    // =========== CONFIGURATION ===========
    // Load/save configurations
    bool loadConfig(const std::string& config_file_path);
    bool saveConfig(const std::string& config_file_path) const;
    
    // Set default model
    void setDefaultModel(const std::string& model_name);
    std::string getDefaultModel() const;
    
    // =========== STATISTICS & MONITORING ===========
    // Usage statistics
    void* getUsageStatistics() const;
    void* getModelStats(const std::string& model_name) const;
    
    // Performance metrics
    double getAverageLatency(const std::string& model_name = "") const;
    int getSuccessRate(const std::string& model_name = "") const;
    
    // Cost tracking
    double getTotalCost() const;
    double getCostByModel(const std::string& model_name) const;
    void* getCostBreakdown() const;
    
    // =========== ERROR HANDLING ===========
    // Set error callbacks
    void setErrorCallback(std::function<void(const std::string&)> callback);
    
    // Retry configuration
    void setRetryPolicy(int max_retries, int retry_delay_ms);
    
    // =========== UTILITY ===========
    // Count tokens (for cost estimation)
    int estimateTokenCount(const std::string& text) const;
    
    // Format model list
    std::string formatModelList() const;
    void* getModelListAsJson() const;


    void initialized();
    void modelListUpdated(const std::vector<std::string>& models);
    void generationCompleted(const GenerationResult& result);
    void generationFailed(const std::string& error);
    void streamChunkReceived(const std::string& chunk);
    void streamingCompleted();
    void streamingFailed(const std::string& error);
    void batchCompleted(const std::vector<GenerationResult>& results);
    void batchFailed(const std::string& error);
    void statisticsUpdated();

private:
    void onRouterInitialized();
    void onModelRegistered(const std::string& model_name);

private:
    // Internal generation methods
    GenerationResult generateInternal(const std::string& prompt,
                                     const std::string& model_name,
                                     const GenerationOptions& options);
    
    void generateStreamInternal(const std::string& prompt,
                               const std::string& model_name,
                               std::function<void(const std::string&)> on_chunk,
                               std::function<void(const std::string&)> on_error,
                               const GenerationOptions& options);
    
    // Helper methods
    bool isLocalModel(const std::string& model_name) const;
    bool isCloudModel(const std::string& model_name) const;
    ModelConfig getModelConfigOrThrow(const std::string& model_name) const;
    
    // Member variables
    std::shared_ptr<UniversalModelRouter> router;
    std::shared_ptr<RawrXD::CPUInferenceEngine> local_engine;
    std::shared_ptr<CloudApiClient> cloud_client;
    std::string default_model;
    bool initialized_flag;
    
    // Statistics tracking
    struct ModelStats {
        int call_count = 0;
        int success_count = 0;
        int failure_count = 0;
        double total_latency_ms = 0.0;
        double total_cost = 0.0;
        int total_tokens = 0;
    };
    
    std::map<std::string, ModelStats> stats_map;
    
    // Configuration
    int max_retries = 3;
    int retry_delay_ms = 1000;
    std::function<void(const std::string&)> error_callback;
};

#endif // MODEL_INTERFACE_H

