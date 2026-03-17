#pragma once
#include <QObject>
#include <string>
#include <vector>
#include <map>
#include <QVariant>

class LargeModelOptimizer : public QObject {
    Q_OBJECT

public:
    explicit LargeModelOptimizer(QObject* parent = nullptr);

    // Large model analysis
    struct ModelAnalysis {
        std::string model_path;
        size_t total_size_bytes;
        size_t parameter_count;
        std::map<std::string, size_t> layer_sizes;
        std::vector<std::string> critical_layers;
        std::vector<std::string> optional_layers;
        double compression_ratio;
        size_t estimated_memory_4bit;
        size_t estimated_memory_3bit;
        size_t estimated_memory_2bit;
        bool supports_streaming;
        bool supports_quantization;
        std::string recommended_strategy;
    };
    
    ModelAnalysis analyzeLargeModel(const std::string& model_path);
    
    // Memory optimization strategies
    struct OptimizationPlan {
        std::string original_model_path;
        std::string optimized_model_path;
        size_t original_size;
        size_t optimized_size;
        double memory_reduction;
        std::vector<std::string> applied_techniques;
        std::map<std::string, QVariant> parameters;
        bool requires_streaming;
        bool requires_quantization;
        size_t minimum_memory_required;
        std::string recommended_strategy;
    };
    
    OptimizationPlan createOptimizationPlan(const std::string& model_path, 
                                           size_t available_memory);
    
    // Large model recommendations
    struct ModelRecommendation {
        std::string model_id;
        std::string model_name;
        size_t parameter_count;
        size_t memory_requirement_4bit;
        size_t memory_requirement_3bit;
        size_t memory_requirement_2bit;
        bool can_run_full;
        bool can_run_4bit;
        bool can_run_3bit;
        bool can_run_2bit;
        std::string recommended_precision;
        double expected_quality_loss;
        std::string streaming_strategy;
    };
    
    ModelRecommendation recommendModelConfiguration(const std::string& model_path, 
                                                   size_t available_memory);

signals:
    void analysisProgress(const QString& model_id, double progress);
    void optimizationRecommended(const QString& technique, double benefit);
    void modelConfigurationReady(const ModelRecommendation& recommendation);

private:
    // Analysis methods
    size_t estimateParameterCount(const std::string& model_path);
    std::map<std::string, size_t> analyzeLayerDistribution(const std::string& model_path);
    std::vector<std::string> identifyCriticalLayers(const std::map<std::string, size_t>& layer_sizes);
    std::vector<std::string> identifyOptionalLayers(const std::map<std::string, size_t>& layer_sizes);
    
    // Optimization calculations
    size_t calculate4BitMemory(size_t original_size);
    size_t calculate3BitMemory(size_t original_size);
    size_t calculate2BitMemory(size_t original_size);
    double estimateQualityLoss(int quantization_bits);
    
    // Strategy recommendations
    std::string recommendStreamingStrategy(size_t model_size, size_t available_memory);
    std::string recommendQuantizationLevel(size_t model_size, size_t available_memory);
    bool canRunWithStreaming(size_t model_size, size_t available_memory);
    bool canRunWithQuantization(size_t model_size, size_t available_memory, int bits);
};
