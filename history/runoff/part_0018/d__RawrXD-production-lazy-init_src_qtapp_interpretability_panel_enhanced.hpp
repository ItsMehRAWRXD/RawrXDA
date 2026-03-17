#pragma once

#include <QWidget>
#include <QJsonObject>
#include <QJsonArray>
#include <vector>
#include <map>
#include <memory>
#include <chrono>
#include <cmath>

/**
 * @file interpretability_panel_enhanced.hpp
 * @brief Production-grade interpretability panel for model behavior analysis
 * 
 * Comprehensive visualization component for:
 * - Attention weight analysis (multi-head, layer-wise)
 * - Gradient flow tracking (vanishing/exploding detection)
 * - Activation distribution analysis (sparsity, dead neurons)
 * - Feature importance and attribution methods (GradCAM, Integrated Gradients)
 * - Layer contribution analysis
 * - Real-time performance monitoring
 * 
 * Features:
 * - Structured JSON logging
 * - Complete error handling
 * - Performance metrics collection
 * - Export capabilities (JSON, CSV, PNG)
 * - Kubernetes health probe support
 * - Prometheus metrics integration
 */

class InterpretabilityPanelEnhanced : public QWidget {
    Q_OBJECT

public:
    explicit InterpretabilityPanelEnhanced(QWidget* parent = nullptr);
    ~InterpretabilityPanelEnhanced();

    // ========== VISUALIZATION TYPES ==========
    enum class VisualizationType {
        AttentionHeatmap = 0,           // Multi-head attention weights
        FeatureImportance = 1,          // Input feature attribution
        GradientFlow = 2,               // Layer-wise gradient norms
        ActivationDistribution = 3,     // Neuron activation statistics
        AttentionHeadComparison = 4,    // Cross-head attention patterns
        GradCAM = 5,                    // Class activation mapping
        LayerContribution = 6,          // Layer-wise output contribution
        EmbeddingSpace = 7,             // 2D/3D embedding visualization
        IntegratedGradients = 8,        // Input attribution via gradients
        SaliencyMap = 9,                // Input gradient magnitude
        TokenLogits = 10,               // Logit distribution per token
        LayerNormStats = 11,            // Layer normalization statistics
        AttentionFlow = 12,             // Attention head information flow
        GradientVariance = 13           // Gradient stability across batches
    };

    // ========== DATA STRUCTURES ==========
    
    struct AttentionHead {
        int layer_idx;
        int head_idx;
        std::vector<std::vector<float>> weights;  // seq_len x seq_len
        float mean_attn_weight = 0.0f;
        float max_attn_weight = 0.0f;
        float entropy = 0.0f;  // Shannon entropy of attention distribution
        std::chrono::system_clock::time_point timestamp;
    };

    struct GradientFlowMetrics {
        int layer_idx;
        float norm = 0.0f;
        float variance = 0.0f;
        float min_value = 0.0f;
        float max_value = 0.0f;
        float dead_neuron_ratio = 0.0f;  // Percentage of near-zero gradients
        bool is_vanishing = false;
        bool is_exploding = false;
        std::chrono::system_clock::time_point timestamp;
    };

    struct ActivationStats {
        int layer_idx;
        float mean = 0.0f;
        float variance = 0.0f;
        float min_val = 0.0f;
        float max_val = 0.0f;
        float sparsity = 0.0f;  // % of activations near zero
        float dead_neuron_count = 0.0f;
        std::vector<float> distribution;  // Histogram bins
        std::chrono::system_clock::time_point timestamp;
    };

    struct FeatureAttribution {
        QString feature_name;
        float attribution_score = 0.0f;
        float positive_contribution = 0.0f;
        float negative_contribution = 0.0f;
        int rank = 0;
    };

    struct LayerAttribution {
        int layer_idx;
        float contribution = 0.0f;
        float cumulative_importance = 0.0f;
        std::vector<float> neuron_importances;
        std::chrono::system_clock::time_point timestamp;
    };

    struct GradCAMMap {
        int layer_idx;
        int target_class;
        std::vector<float> activation_map;
        float mean_activation = 0.0f;
        float max_activation = 0.0f;
        std::chrono::system_clock::time_point timestamp;
    };

    struct ModelDiagnostics {
        bool has_vanishing_gradients = false;
        bool has_exploding_gradients = false;
        bool has_dead_neurons = false;
        bool has_saturation = false;
        float average_sparsity = 0.0f;
        float attention_entropy_mean = 0.0f;
        int problematic_layers = 0;
        std::vector<int> critical_layer_indices;
        std::chrono::system_clock::time_point timestamp;
    };

    // ========== VISUALIZATION UPDATES ==========
    
    /**
     * Update attention weights for a specific head
     * @param attention_head Complete attention data
     */
    void updateAttentionHeads(const std::vector<AttentionHead>& attention_heads);
    
    /**
     * Update gradient flow metrics across layers
     * @param gradient_data Vector of per-layer gradient metrics
     */
    void updateGradientFlow(const std::vector<GradientFlowMetrics>& gradient_data);
    
    /**
     * Update activation statistics for layers
     * @param activation_stats Vector of per-layer activation statistics
     */
    void updateActivationStats(const std::vector<ActivationStats>& activation_stats);
    
    /**
     * Update feature importance rankings
     * @param attributions Vector of feature attributions
     */
    void updateFeatureImportance(const std::vector<FeatureAttribution>& attributions);
    
    /**
     * Update layer contribution to final output
     * @param layer_attributions Vector of layer contributions
     */
    void updateLayerAttribution(const std::vector<LayerAttribution>& layer_attributions);
    
    /**
     * Update GradCAM heatmap for specific layer
     * @param gradcam_map GradCAM activation map
     */
    void updateGradCAM(const GradCAMMap& gradcam_map);
    
    /**
     * Update integrated gradients attribution
     * @param input_name Name of input being analyzed
     * @param attributions Per-element attribution values
     */
    void updateIntegratedGradients(const QString& input_name, const std::vector<float>& attributions);
    
    /**
     * Update input saliency map (gradient magnitude)
     * @param saliency_data Input gradient magnitudes
     */
    void updateSaliencyMap(const std::vector<float>& saliency_data);
    
    /**
     * Update token logit distribution
     * @param token_idx Token position
     * @param logits Class probabilities
     */
    void updateTokenLogits(int token_idx, const std::vector<float>& logits);

    // ========== ANALYSIS & DIAGNOSTICS ==========
    
    /**
     * Perform comprehensive model diagnostics
     * @return Diagnostics report with detected issues
     */
    ModelDiagnostics runDiagnostics();
    
    /**
     * Detect vanishing/exploding gradients
     * @return Vector of problematic layer indices
     */
    std::vector<int> detectGradientProblems() const;
    
    /**
     * Get sparsity report across layers
     * @return Map of layer index to sparsity percentage
     */
    std::map<int, float> getSparsityReport() const;
    
    /**
     * Get attention entropy across heads and layers
     * @return Map of (layer, head) -> entropy
     */
    std::map<std::pair<int, int>, float> getAttentionEntropy() const;
    
    /**
     * Get top-K most critical layers
     * @param k Number of layers to return
     * @return Sorted vector of layer indices by importance
     */
    std::vector<int> getCriticalLayers(int k = 5) const;
    
    /**
     * Get top-K most important features
     * @param k Number of features to return
     * @return Sorted vector of top features by attribution
     */
    std::vector<FeatureAttribution> getTopFeatures(int k = 10) const;

    // ========== CONFIGURATION ==========
    
    /**
     * Set visualization type and trigger update
     * @param viz_type Type of visualization to display
     */
    void setVisualizationType(VisualizationType viz_type);
    
    /**
     * Get current visualization type
     */
    VisualizationType getCurrentVisualizationType() const;
    
    /**
     * Set selected layer for detail view
     * @param layer_index Layer index to focus on
     */
    void setSelectedLayer(int layer_index);
    
    /**
     * Set selected attention head
     * @param head_index Head index to visualize
     */
    void setSelectedAttentionHead(int head_index);
    
    /**
     * Set attention focus position (for sequence analysis)
     * @param position Token position
     */
    void setAttentionFocusPosition(int position);
    
    /**
     * Set layer range for visualization
     * @param min_layer Minimum layer index
     * @param max_layer Maximum layer index
     */
    void setLayerRange(int min_layer, int max_layer);
    
    /**
     * Enable/disable gradient tracking
     * @param enabled Track gradient flow
     */
    void setGradientTrackingEnabled(bool enabled);
    
    /**
     * Set thresholds for anomaly detection
     * @param vanishing_threshold Gradient norm threshold for vanishing
     * @param exploding_threshold Gradient norm threshold for exploding
     * @param sparsity_threshold Sparsity % threshold
     */
    void setAnomalyThresholds(float vanishing_threshold, 
                             float exploding_threshold, 
                             float sparsity_threshold);

    // ========== DATA EXPORT ==========
    
    /**
     * Export current visualization as JSON
     * @return JSON object containing all visualization data
     */
    QJsonObject exportAsJSON() const;
    
    /**
     * Export data as JSON file
     * @param file_path Path to save JSON file
     * @return Success status
     */
    bool exportAsJSON(const QString& file_path);
    
    /**
     * Export data as CSV format
     * @param file_path Path to save CSV
     * @param viz_type Which visualization type to export
     * @return Success status
     */
    bool exportAsCSV(const QString& file_path, VisualizationType viz_type) const;
    
    /**
     * Export data as CSV file
     * @param file_path Path to save CSV
     * @return Success status
     */
    bool exportAsCSV(const QString& file_path);
    
    /**
     * Export visualization as image (PNG)
     * @param file_path Path to save PNG
     * @return Success status
     */
    bool exportAsPNG(const QString& file_path) const;
    
    /**
     * Load interpretability data from JSON
     * @param data JSON object containing serialized data
     * @return Success status
     */
    bool loadFromJSON(const QJsonObject& data);
    
    /**
     * Clear all stored visualizations
     */
    void clearVisualizations();

    // ========== OBSERVABILITY & MONITORING ==========
    
    /**
     * Get health status for Kubernetes probes
     * @return JSON object with health metrics
     */
    QJsonObject getHealthStatus() const;
    
    /**
     * Get Prometheus metrics
     * @return Text formatted for Prometheus scraping
     */
    QString getPrometheusMetrics() const;
    
    /**
     * Get performance statistics
     * @return JSON object with timing and memory info
     */
    QJsonObject getPerformanceStats() const;

    // ========== SIGNALS ==========

signals:
    void visualizationUpdated(int viz_type);
    void layerSelectionChanged(int layer_index);
    void attentionHeadSelectionChanged(int head_index);
    void diagnosticsCompleted(const QJsonObject& diagnostics);
    void anomalyDetected(const QString& anomaly_description);
    void exportCompleted(bool success, const QString& file_path);
    void exportRequested(const QString& format);

public slots:
    void onModelLoaded(const QString& modelPath);
    void onModelUnloaded();

private slots:
    void onRefreshDisplay();

private:
    // ========== HELPER METHODS ==========
    
    void setupUI();
    void updateDisplay();
    void createCharts();
    
    /**
     * Analyze attention weights for patterns
     */
    void analyzeAttentionPatterns();
    
    /**
     * Detect gradient flow issues
     */
    void analyzeGradientFlow();
    
    /**
     * Analyze neuron activation patterns
     */
    void analyzeActivationPatterns();
    
    /**
     * Detect anomalies across all metrics
     */
    void detectAnomalies();
    
    /**
     * Calculate entropy of probability distribution
     */
    static float calculateEntropy(const std::vector<float>& distribution);
    
    /**
     * Convert visualization to JSON format
     */
    QJsonObject visualizationToJSON() const;
    
    /**
     * Log event with structured format
     */
    void logEvent(const QString& event_type, const QJsonObject& event_data) const;

    // ========== MEMBER VARIABLES ==========
    
    VisualizationType m_current_visualization;
    int m_selected_layer = 0;
    int m_selected_attention_head = 0;
    int m_attention_focus_position = 0;
    int m_min_layer = 0;
    int m_max_layer = 100;
    
    bool m_gradient_tracking_enabled = true;
    float m_vanishing_threshold = 1e-7f;
    float m_exploding_threshold = 10.0f;
    float m_sparsity_threshold = 0.5f;
    
    // Data storage
    std::map<int, std::map<int, AttentionHead>> m_attention_heads;  // layer -> head -> data
    std::map<int, GradientFlowMetrics> m_gradient_flow_data;       // layer -> data
    std::map<int, ActivationStats> m_activation_stats;             // layer -> data
    std::vector<FeatureAttribution> m_feature_attributions;
    std::vector<LayerAttribution> m_layer_attributions;
    std::map<int, GradCAMMap> m_gradcam_data;                      // layer -> data
    std::map<QString, std::vector<float>> m_integrated_gradients;   // input_name -> values
    std::vector<float> m_saliency_map;
    std::map<int, std::vector<float>> m_token_logits;              // token_idx -> logits
    
    // Diagnostics cache
    ModelDiagnostics m_cached_diagnostics;
    std::chrono::system_clock::time_point m_last_diagnostics_time;
    
    // Performance metrics
    struct PerformanceMetrics {
        std::chrono::duration<double> last_update_duration;
        std::chrono::duration<double> last_diagnostics_duration;
        int total_updates = 0;
        int total_exports = 0;
        size_t current_memory_usage = 0;
    } m_perf_metrics;
    
    // Qt UI components
    class QChartView* m_chart_view = nullptr;
    class QChart* m_chart = nullptr;
    class QTabWidget* m_tab_widget = nullptr;
    class QComboBox* m_viz_type_combo = nullptr;
    class QSlider* m_layer_slider = nullptr;
    class QComboBox* m_attention_head_combo = nullptr;
    class QLabel* m_stats_label = nullptr;
    class QLabel* m_problems_label = nullptr;
    class QLabel* m_diagnostics_label = nullptr;
};
