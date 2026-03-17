#pragma once

#include <vector>
#include <string>
#include <map>
#include <memory>
#include <nlohmann/json.hpp>

namespace Json {
using Value = nlohmann::json;
}

// Interpretability visualization types and structures

enum class VisualizationType {
    AttentionHeatmap,
    TokenAttribution,
    LayerActivations,
    EmbeddingProjection,
    GradientFlow,
    ActivationDifference
};

struct AttentionPattern {
    std::string headName;
    std::vector<std::vector<float>> weights;
    float entropy = 0.0f;
    std::vector<int> focusedTokens;
    std::vector<float> concentration;
};

struct TokenAttribution {
    std::string token;
    int tokenId = 0;
    float importance = 0.0f;
    std::map<std::string, float> layerContributions;
    std::vector<float> gradients;
    std::string role;
};

struct LayerActivationProfile {
    int layerIndex = 0;
    std::string layerName;
    std::vector<float> activationMagnitudes;
    float sparseity = 0.0f;
    std::vector<int> topActivatedNeurons;
    float averageEntropy = 0.0f;
    std::vector<float> dimensionalityReduction;
};

struct InterpretabilityReport {
    std::string modelName;
    std::string inputPrompt;
    std::vector<std::string> tokens;
    std::vector<AttentionPattern> attentionPatterns;
    std::vector<TokenAttribution> tokenAttributions;
    std::vector<LayerActivationProfile> layerProfiles;
    float totalLikelihood = 0.0f;
    double inferenceTimeMs = 0.0;
    int totalTokensProcessed = 0;
    std::map<std::string, float> performanceMetrics;
};

class InterpretabilityAnalyzer {
public:
    InterpretabilityAnalyzer();
    ~InterpretabilityAnalyzer();

    // Core analysis
    InterpretabilityReport analyzeInference(const std::vector<std::string>& tokens,
                                           const std::vector<std::vector<float>>& rawAttentionWeights,
                                           const std::vector<std::vector<float>>& layerActivations,
                                           const std::vector<float>& logits);

    std::vector<AttentionPattern> analyzeAttention(const std::vector<std::vector<float>>& attentionWeights,
                                                   const std::vector<std::string>& tokens,
                                                   int numHeads);

    std::vector<TokenAttribution> computeTokenAttribution(
        const std::vector<std::string>& tokens,
        const std::vector<std::vector<float>>& gradients,
        const std::vector<std::vector<float>>& layerActivations);

    std::vector<LayerActivationProfile> analyzeActivations(
        const std::vector<std::vector<float>>& activations);

    // Visualization generation
    Json::Value generateAttentionVisualization(const AttentionPattern& pattern);
    Json::Value generateAttributionVisualization(const std::vector<TokenAttribution>& attributions);
    Json::Value generateActivationVisualization(const LayerActivationProfile& profile);
    Json::Value generateFullReport(const InterpretabilityReport& report);

    // Comparison
    Json::Value compareAttentionPatterns(const AttentionPattern& pattern1,
                                        const AttentionPattern& pattern2);

    // Interactive analysis
    std::vector<TokenAttribution> explainToken(int tokenIdx,
                                              const InterpretabilityReport& report);
    std::vector<AttentionPattern> getHeadAttention(int headIdx,
                                                   const InterpretabilityReport& report);

    // Performance metrics
    void recordInferenceMetrics(const std::string& modelName, double latencyMs, int tokensProcessed);
    Json::Value getPerformanceStats();

private:
    float computeAttentionEntropy(const std::vector<float>& attentionWeights);
    float computeConcentration(const std::vector<float>& attentionWeights);
    std::vector<float> normalizeWeights(const std::vector<float>& weights);
    std::vector<float> computeGradients(const std::vector<float>& activations,
                                       const std::vector<float>& nextLayerGradients);
    float computeSparsity(const std::vector<float>& activations);
    std::vector<float> performPCA(const std::vector<std::vector<float>>& data, int components);

    std::map<std::string, std::vector<double>> performanceHistory;
    int maxHistorySize = 1000;
};
